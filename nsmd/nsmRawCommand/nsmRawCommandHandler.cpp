/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nsmRawCommandHandler.hpp"

#include "base.h"

#include "sensorManager.hpp"

#include <sys/mman.h> // for memfd_create
#include <sys/stat.h> // for fstat
#include <unistd.h>   // for write and lseek
namespace nsm
{

NsmRawCommandHandler* NsmRawCommandHandler::instance = nullptr;

NsmRawCommandHandler::NsmRawCommandHandler(sdbusplus::bus::bus& bus,
                                           const char* path) :
    NsmRawIntf(bus, path)
{}

void NsmRawCommandHandler::initialize(sdbusplus::bus::bus& bus,
                                      const char* path)
{
    static NsmRawCommandHandler instance(bus, path);
    NsmRawCommandHandler::instance = &instance;
}

NsmRawCommandHandler& NsmRawCommandHandler::getInstance()
{
    if (NsmRawCommandHandler::instance == nullptr)
    {
        throw std::runtime_error(
            "NsmRawCommandHandler instance is not initialized yet");
    }
    return *NsmRawCommandHandler::instance;
}

static Response copySuccessResponse(uint8_t cc, const nsm_msg* responseMsg,
                                    size_t responseLen)
{
    auto dataSize = responseLen - sizeof(nsm_msg_hdr) - 2;
    // completion code + data
    Response data(1 + dataSize, 0);
    data[0] = cc;
    memcpy(data.data() + 1, responseMsg->payload + 2, dataSize);
    return data;
}

static Response copyReasonCodeResponse(uint8_t cc, uint16_t reasonCode)
{
    // completion code + reason code
    Response data(3, 0);
    data[0] = cc;
    reasonCode = htole16(reasonCode);
    memcpy(data.data() + 1, &reasonCode, sizeof(uint16_t));
    return data;
}

requester::Coroutine NsmRawCommandHandler::doSendLongRunningRequest(
    uint8_t deviceType, uint8_t instanceId, bool isLongRunning,
    uint8_t messageType, uint8_t commandCode, int duplicateFdHandle,
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    std::shared_ptr<AsyncValueIntf> valueInterface)
{
    utils::CustomFD fd(duplicateFdHandle);
    uint8_t rc = NSM_SW_ERROR;
    std::shared_ptr<NsmDevice> device = nullptr;
    try
    {
        auto& manager = SensorManager::getInstance();
        device = manager.getNsmDevice(deviceType, instanceId);
        if (!device)
        {
            statusInterface->status(AsyncOperationStatusType::InvalidArgument);
            co_return rc;
        }

        // Acquire the semaphore before proceeding
        co_await device->getSemaphore().acquire(device->eid);
        // Create the long-running event handler
        auto longRunningHandler =
            std::make_shared<NsmRawLongRunningEventHandler>(
                "RawLongRunningHandler", "RawEvent", isLongRunning);
        longRunningHandler->isLongRunning = true;
        // Register the active handler in the device with messageType and
        // commandCode
        device->registerLongRunningHandler(messageType, commandCode,
                                           longRunningHandler);

        std::vector<uint8_t> data;
        utils::readFdToBuffer(fd, data);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
                        data.size());
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        encode_raw_cmd_req(0, messageType, commandCode, data.data(),
                           data.size(), requestMsg);

        auto eid = manager.getEid(device);
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                             responseLen);
        uint8_t cc;
        uint16_t reasonCode = 0;
        if (rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            cc = NSM_ERR_UNSUPPORTED_COMMAND_CODE;
            rc = NSM_SW_SUCCESS;
            reasonCode = 0;
            data = copyReasonCodeResponse(cc, reasonCode);
        }
        else if (rc != NSM_SW_SUCCESS)
        {
            throw std::runtime_error(std::format(
                "NsmRawCommandHandler::doSendLongRunningRequest: SendRecvNsmMsg failed, rc={}",
                rc));
        }
        else
        {
            rc = decode_reason_code_and_cc(responseMsg.get(), responseLen, &cc,
                                           &reasonCode);
            if (rc != NSM_SW_SUCCESS)
            {
                throw std::runtime_error(std::format(
                    "NsmRawCommandHandler::doSendLongRunningRequest: decode_common_resp failed, rc={}",
                    rc));
            }
            else if (cc == NSM_SUCCESS)
            {
                longRunningHandler->isLongRunning = false;
                data = copySuccessResponse(cc, responseMsg.get(), responseLen);
            }
            else
            {
                auto accepted = longRunningHandler->initAcceptInstanceId(
                    responseMsg->hdr.instance_id, cc, rc);
                if (!accepted)
                {
                    data = copyReasonCodeResponse(cc, reasonCode);
                }
                else
                {
                    rc = co_await longRunningHandler->timer;
                    if (rc != NSM_SW_SUCCESS)
                    {
                        lg2::error(
                            "NsmRawCommandHandler::doSendLongRunningRequest: LongRunning timer start failed, eid={EID}",
                            "EID", eid);
                    }
                    else if (longRunningHandler->timer.expired())
                    {
                        lg2::error(
                            "NsmRawCommandHandler::doSendLongRunningRequest:: LongRunning sensor timeout, eid={EID}",
                            "EID", eid);
                        rc = NSM_SW_ERROR;
                    }
                    else
                    {
                        data = std::move(longRunningHandler->data);
                        rc = longRunningHandler->rc;
                    }
                }
            }
        }
        utils::writeBufferToFd(fd, data);
        valueInterface->value(rc);
        statusInterface->status(AsyncOperationStatusType::Success);
    }
    catch (const std::runtime_error& e)
    {
        lg2::error(e.what());
        statusInterface->status(AsyncOperationStatusType::WriteFailure);
    }
    catch (const std::exception& e)
    {
        lg2::error(e.what());
        statusInterface->status(AsyncOperationStatusType::InternalFailure);
    }
    // degister handler and release semaphore
    device->clearLongRunningHandler();
    device->getSemaphore().release();
    // coverity[missing_return]
    co_return rc;
}

int NsmRawLongRunningEventHandler::handle(eid_t eid, NsmType, NsmEventId,
                                          const nsm_msg* event, size_t eventLen)
{
    rc = validateEvent(eid, event, eventLen);
    if (rc == NSM_SW_SUCCESS)
    {
        auto cc = ((nsm_long_running_resp*)((nsm_event*)event->payload)->data)
                      ->completion_code;
        data = copySuccessResponse(cc, event, eventLen);
    }
    if (!timer.stop())
    {
        lg2::error(
            "NsmRawLongRunningEventHandler::handle: LongRunning timer not stopped, eid={EID}",
            "EID", eid);
    }
    return rc;
}

requester::Coroutine NsmRawCommandHandler::doSendRequest(
    uint8_t deviceType, uint8_t instanceId, uint8_t messageType,
    uint8_t commandCode, int duplicateFdHandle,
    std::shared_ptr<AsyncStatusIntf> statusInterface,
    std::shared_ptr<AsyncValueIntf> valueInterface)
{
    utils::CustomFD fd(duplicateFdHandle);
    uint8_t rc = NSM_SW_ERROR;
    try
    {
        std::vector<uint8_t> data;
        utils::readFdToBuffer(fd, data);
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
                        data.size());
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());
        encode_raw_cmd_req(0, messageType, commandCode, data.data(),
                           data.size(), requestMsg);

        auto& manager = SensorManager::getInstance();
        auto device = manager.getNsmDevice(deviceType, instanceId);
        if (!device)
        {
            throw std::invalid_argument(
                std::format("Device {}:{} not found", deviceType, instanceId));
        }
        auto eid = manager.getEid(device);
        std::shared_ptr<const nsm_msg> responseMsg;
        size_t responseLen = 0;
        rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                             responseLen);

        uint8_t cc;
        uint16_t reasonCode;
        if (rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            cc = NSM_ERR_UNSUPPORTED_COMMAND_CODE;
            rc = NSM_SW_SUCCESS;
            reasonCode = 0;
        }
        else if (rc != NSM_SW_SUCCESS)
        {
            throw std::runtime_error(
                std::format("SendRecvNsmMsg failed, rc={}", rc));
        }
        else
        {
            rc = decode_reason_code_and_cc(responseMsg.get(), responseLen, &cc,
                                           &reasonCode);
            if (rc != NSM_SW_SUCCESS)
            {
                throw std::runtime_error(
                    std::format("decode_common_resp failed, rc={}", rc));
            }
        }

        if (cc == NSM_SUCCESS)
        {
            data = copySuccessResponse(cc, responseMsg.get(), responseLen);
        }
        else
        {
            data = copyReasonCodeResponse(cc, reasonCode);
        }
        utils::writeBufferToFd(fd, data);
        valueInterface->value(rc);
        statusInterface->status(AsyncOperationStatusType::Success);
    }
    catch (const std::invalid_argument& e)
    {
        lg2::error(e.what());
        statusInterface->status(AsyncOperationStatusType::InvalidArgument);
    }
    catch (const std::runtime_error& e)
    {
        lg2::error(e.what());
        statusInterface->status(AsyncOperationStatusType::WriteFailure);
    }
    catch (const std::exception& e)
    {
        lg2::error(e.what());
        statusInterface->status(AsyncOperationStatusType::InternalFailure);
    }
    // coverity[missing_return]
    co_return rc;
}

sdbusplus::message::object_path NsmRawCommandHandler::sendRequest(
    uint8_t deviceType, uint8_t instanceId, bool isLongRunning,
    uint8_t messageType, uint8_t commandCode, sdbusplus::message::unix_fd fd)
{
    if (deviceType > NSM_DEV_ID_EROT || messageType > NSM_TYPE_FIRMWARE)
    {
        throw sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument();
    }

    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        throw sdbusplus::error::xyz::openbmc_project::common::Unavailable{};
    }

    if (isLongRunning)
    {
        doSendLongRunningRequest(deviceType, instanceId, isLongRunning,
                                 messageType, commandCode, dup(fd),
                                 statusInterface, valueInterface)
            .detach();
    }
    else
    {
        doSendRequest(deviceType, instanceId, messageType, commandCode, dup(fd),
                      statusInterface, valueInterface)
            .detach();
    }

    return objectPath;
}

} // namespace nsm
