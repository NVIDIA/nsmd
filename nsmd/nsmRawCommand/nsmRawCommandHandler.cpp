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

requester::Coroutine NsmRawCommandHandler::doSendRequest(
    uint8_t deviceType, uint8_t instanceId, bool isLongRunning,
    uint8_t messageType, uint8_t commandCode, int duplicateFdHandle,
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
                                             responseLen, isLongRunning);

        uint8_t cc;
        uint16_t dataSize, reasonCode;
        if (rc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
        {
            cc = NSM_ERR_UNSUPPORTED_COMMAND_CODE;
            rc = NSM_SW_SUCCESS;
            dataSize = 0;
            reasonCode = 0;
        }
        else if (rc != NSM_SW_SUCCESS)
        {
            throw std::runtime_error(
                std::format("SendRecvNsmMsg failed, rc={}", rc));
        }
        else
        {
            rc = decode_common_resp(responseMsg.get(), responseLen, &cc,
                                    &dataSize, &reasonCode);
            if (rc != NSM_SW_SUCCESS)
            {
                throw std::runtime_error(
                    std::format("decode_common_resp failed, rc={}", rc));
            }
        }

        if (cc == NSM_SUCCESS)
        {
            // completion code + data
            data.resize(1 + dataSize);
            memcpy(data.data() + 1,
                   responseMsg->payload + sizeof(nsm_common_resp), dataSize);
        }
        else
        {
            // completion code + data
            data.resize(3);
            reasonCode = htole16(reasonCode);
            memcpy(data.data() + 1, &reasonCode, sizeof(uint16_t));
        }
        data[0] = cc;
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

    doSendRequest(deviceType, instanceId, isLongRunning, messageType,
                  commandCode, dup(fd), statusInterface, valueInterface)
        .detach();

    return objectPath;
}

} // namespace nsm
