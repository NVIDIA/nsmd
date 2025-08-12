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

#include "nsmLogInfo.hpp"

#include "diagnostics.h"

#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>

#define NSM_LOG_INFO_START_RECORD 0x00
#define NSM_LOG_INFO_END_RECORD 0x00
#define NSM_LOG_INFO_BUFFER_SIZE 65535

namespace nsm
{

NsmLogInfoObject::NsmLogInfoObject(sdbusplus::bus::bus& bus,
                                   const std::string& name,
                                   const std::string& inventoryPath,
                                   const std::string& type,
                                   const uuid_t& uuid) :
    NsmObject(name, type), LogInfoIntf(bus, (inventoryPath + name).c_str()),
    uuid(uuid)
{
    buffer.reserve(NSM_LOG_INFO_BUFFER_SIZE);
    lg2::debug("Created NsmLogInfoObject: {NAME}", "NAME", name.c_str());
}

void NsmLogInfoObject::finish(AsyncOperationStatusType status, uint8_t rc)
{
    statusInterface->status(status);
    valueInterface->value(rc);
    close(fd);
}

void NsmLogInfoObject::getLogInfoAsyncHandler(uint32_t recordHandle)
{
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    encode_get_network_device_log_info_req(0, recordHandle, requestMsg);
    getLogInfoAsyncHandler(request).detach();
}

requester::Coroutine
    NsmLogInfoObject::getLogInfoAsyncHandler(std::shared_ptr<Request> request)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await device->postPatchIO(eid, *request, responseMsg,
                                           responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmLogInfoObject: getRequest postPatchIO: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        finish(AsyncOperationStatusType::InternalFailure, rc);
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t bufferSize = 0;
    uint32_t nextHandle = 0;
    nsm_device_log_info_breakdown logInfo;

    buffer.resize(NSM_LOG_INFO_BUFFER_SIZE);
    rc = decode_get_network_device_log_info_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &nextHandle, &logInfo,
        buffer.data(), &bufferSize);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmLogInfoObject: decode_get_network_device_log_info_resp: "
            "eid={EID} rc={RC} cc={CC} len={LEN} reasonCode={REASON_CODE}",
            "EID", eid, "RC", rc, "CC", cc, "LEN", responseLen, "REASON_CODE",
            reasonCode);
        finish(AsyncOperationStatusType::InternalFailure, rc);
        // coverity[missing_return]
        co_return rc;
    }
    buffer.resize(bufferSize);

    // update data on interface
    lostEvents(static_cast<uint64_t>(logInfo.lost_events));
    entryPrefix(static_cast<uint64_t>(logInfo.entry_prefix));
    entrySuffix(static_cast<uint64_t>(logInfo.entry_suffix));
    length(static_cast<uint64_t>(logInfo.length));
    timeStamp(static_cast<uint64_t>(((uint64_t)logInfo.time_high << 32) |
                                    logInfo.time_low));

    switch (logInfo.synced_time)
    {
        case SYNCED_TIME_TYPE_BOOT:
            timeSynced(TimeSyncFrom::Boot);
            break;
        case SYNCED_TIME_TYPE_SYNCED:
            timeSynced(TimeSyncFrom::Synced);
            break;
        default:
            lg2::error("NsmLogInfoObject: unknown value for time synced");
            finish(AsyncOperationStatusType::InternalFailure, NSM_SW_ERROR);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
    }

    try
    {
        utils::appendBufferToFd(fd, buffer);
    }
    catch (const std::exception& e)
    {
        lg2::error("NsmLogInfoObject: appendBufferToFd failed: {ERR}", "ERR",
                   e.what());
        finish(AsyncOperationStatusType::WriteFailure, NSM_SW_ERROR);
        co_return NSM_SW_ERROR;
    }

    if (nextHandle == NSM_LOG_INFO_END_RECORD)
    {
        finish(AsyncOperationStatusType::Success, NSM_SW_SUCCESS);
    }
    else
    {
        getLogInfoAsyncHandler(nextHandle);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

sdbusplus::message::object_path
    NsmLogInfoObject::getLogInfo(sdbusplus::message::unix_fd fd)
{
    if (statusInterface != nullptr &&
        statusInterface->status() == AsyncOperationStatusType::InProgress)
    {
        lg2::debug("NsmLogInfoObject: getLogInfo already in progress");
        throw Common::Error::Unavailable();
    }

    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::debug(
            "NsmLogInfoObject: getLogInfo: cannot start async operation, objectPath is empty");
        throw Common::Error::Unavailable();
    }

    this->statusInterface = statusInterface;
    this->valueInterface = valueInterface;
    this->fd = dup(fd);
    if (this->fd < 0)
    {
        lg2::error(
            "NsmLogInfoObject: Failed to duplicate file descriptor: {ERR}",
            "ERR", strerror(errno));
        finish(AsyncOperationStatusType::InternalFailure, NSM_SW_ERROR);
        throw Common::Error::InternalFailure();
    }
    getLogInfoAsyncHandler(NSM_LOG_INFO_START_RECORD);
    return objectPath;
}

} // namespace nsm
