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

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <sstream>
#include <stdexcept>

using std::filesystem::path;

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
    lg2::debug("NsmLogInfoObject: {NAME}", "NAME", name.c_str());

    objPath = inventoryPath + name;
    sdbusplus::message::unix_fd unixFd(0);
    fd(unixFd, true);
}

uint8_t NsmLogInfoObject::startLogInfoCmd()
{
    if (cmdInProgress)
    {
        return NSM_SW_ERROR;
    }
    cmdInProgress = true;
    status(CmdOperationStatus::InProgress);

    return NSM_SW_SUCCESS;
}

void NsmLogInfoObject::finishLogInfoCmd(CmdOperationStatus opStatus)
{
    status(opStatus);
    cmdInProgress = false;
}

requester::Coroutine
    NsmLogInfoObject::getLogInfoAsyncHandler(std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmLogInfoObject: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        finishLogInfoCmd(CmdOperationStatus::InternalFailure);
        // coverity[missing_return]
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint32_t nextHandle = 0;
    uint16_t logInfoSize = 0;
    struct nsm_device_log_info_breakdown logInfo;
    std::vector<uint8_t> logData(65535, 0);

    auto rc = decode_get_network_device_log_info_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &nextHandle, &logInfo,
        logData.data(), &logInfoSize);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("NsmLogInfoObject: decode_get_network_device_log_info_resp: "
                   "eid={EID} rc={RC} cc={CC} len={LEN}",
                   "EID", eid, "RC", rc, "CC", cc, "LEN", responseLen);
        finishLogInfoCmd(CmdOperationStatus::InternalFailure);
        // coverity[missing_return]
        co_return rc;
    }

    int fileDesc = memfd_create("log_info", 0);
    if (fileDesc == -1)
    {
        lg2::error("NsmLogInfoObject: memfd_create: eid={EID} error={ERROR}",
                   "EID", eid, "ERROR", strerror(errno));
        finishLogInfoCmd(CmdOperationStatus::WriteFailure);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    const uint8_t* requestPtr = logData.data();
    while (logInfoSize != 0)
    {
        ssize_t written = write(fileDesc, requestPtr, logInfoSize);
        if (written < 0)
        {
            lg2::error("NsmLogInfoObject: write: eid={EID} error={ERROR}",
                       "EID", eid, "ERROR", strerror(errno));
            close(fileDesc);
            finishLogInfoCmd(CmdOperationStatus::WriteFailure);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        requestPtr += written;
        logInfoSize -= written;
    }
    (void)lseek(fileDesc, 0, SEEK_SET);
    sdbusplus::message::unix_fd unixFd(fileDesc);

    // update data on interface
    fd(unixFd, true);
    nextRecordHandle(static_cast<uint64_t>(nextHandle));
    lostEvents(static_cast<uint64_t>(logInfo.lost_events));
    entryPrefix(static_cast<uint64_t>(logInfo.entry_prefix));
    entrySuffix(static_cast<uint64_t>(logInfo.entry_suffix));
    length(static_cast<uint64_t>(logInfo.length));

    uint64_t timestamp64 = ((uint64_t)logInfo.time_high << 32) |
                           logInfo.time_low;
    timeStamp(timestamp64);

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
            finishLogInfoCmd(CmdOperationStatus::InternalFailure);
            throw Common::Error::InternalFailure();
    }
    finishLogInfoCmd(CmdOperationStatus::Success);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmLogInfoObject::getLogInfo(uint64_t recHandle)
{
    if (startLogInfoCmd() != NSM_SW_SUCCESS)
    {
        throw Common::Error::Unavailable();
    }

    if (fd() != 0)
    {
        close(fd());
        sdbusplus::message::unix_fd unixFd(0);
        fd(unixFd, true);
    }

    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    recordHandle(recHandle);

    auto rc = encode_get_network_device_log_info_req(
        0, static_cast<uint32_t>(recHandle), requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        getLogInfoAsyncHandler(request).detach();
        return;
    }
    lg2::error(
        "NsmLogInfoObject: encode_get_network_device_log_info_req: rc={RC}",
        "RC", rc);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        finishLogInfoCmd(CmdOperationStatus::InvalidArgument);
        throw Common::Error::InvalidArgument();
    }
    finishLogInfoCmd(CmdOperationStatus::InternalFailure);
    throw Common::Error::InternalFailure();
}

} // namespace nsm
