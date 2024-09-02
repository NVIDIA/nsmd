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

#include "nsmDebugInfo.hpp"

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

NsmDebugInfoObject::NsmDebugInfoObject(sdbusplus::bus::bus& bus,
                                       const std::string& name,
                                       const std::string& inventoryPath,
                                       const std::string& type,
                                       const uuid_t& uuid) :
    NsmObject(name, type), DebugInfoIntf(bus, (inventoryPath + name).c_str()),
    uuid(uuid)
{
    lg2::debug("NsmDebugInfoObject: {NAME}", "NAME", name.c_str());

    objPath = inventoryPath + name;
    sdbusplus::message::unix_fd unixFd(0);
    fd(unixFd, true);
}

uint8_t NsmDebugInfoObject::startDebugInfoCmd()
{
    if (cmdInProgress)
    {
        return NSM_SW_ERROR;
    }
    cmdInProgress = true;
    status(OperationStatus::InProgress);

    return NSM_SW_SUCCESS;
}

void NsmDebugInfoObject::finishDebugInfoCmd(OperationStatus opStatus)
{
    status(opStatus);
    cmdInProgress = false;
}

requester::Coroutine NsmDebugInfoObject::getDebugInfoAsyncHandler(
    std::shared_ptr<Request> request)
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
        lg2::error("NsmDebugInfoObject: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        finishDebugInfoCmd(OperationStatus::InternalFailure);
        // coverity[missing_return]
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t segDataSize = 0;
    uint32_t nextHandle = 0;
    std::vector<uint8_t> segData(65535, 0);

    auto rc = decode_get_network_device_debug_info_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &segDataSize,
        segData.data(), &nextHandle);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmDebugInfoObject: decode_get_network_device_debug_info_resp: "
            "eid={EID} rc={RC} cc={CC} len={LEN}",
            "EID", eid, "RC", rc, "CC", cc, "LEN", responseLen);
        finishDebugInfoCmd(OperationStatus::InternalFailure);
        // coverity[missing_return]
        co_return rc;
    }

    int fileDesc = memfd_create("debug_info", 0);
    if (fileDesc == -1)
    {
        lg2::error("NsmDebugInfoObject: memfd_create: eid={EID} error={ERROR}",
                   "EID", eid, "ERROR", strerror(errno));
        finishDebugInfoCmd(OperationStatus::WriteFailure);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }
    const uint8_t* requestPtr = segData.data();
    while (segDataSize != 0)
    {
        ssize_t written = write(fileDesc, requestPtr, segDataSize);
        if (written < 0)
        {
            lg2::error("NsmDebugInfoObject: write: eid={EID} error={ERROR}",
                       "EID", eid, "ERROR", strerror(errno));
            close(fileDesc);
            finishDebugInfoCmd(OperationStatus::WriteFailure);
            // coverity[missing_return]
            co_return NSM_SW_ERROR;
        }
        requestPtr += written;
        segDataSize -= written;
    }
    (void)lseek(fileDesc, 0, SEEK_SET);
    sdbusplus::message::unix_fd unixFd(fileDesc);
    fd(unixFd, true);
    nextRecordHandle(static_cast<uint64_t>(nextHandle));
    finishDebugInfoCmd(OperationStatus::Success);
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void NsmDebugInfoObject::getDebugInfo(DebugInformationType debugInfoType,
                                      uint64_t recHandle)
{
    nsm_debug_information_type infoType;
    switch (debugInfoType)
    {
        case DebugInformationType::DeviceInformation:
            infoType = INFO_TYPE_DEVICE_INFO;
            break;
        case DebugInformationType::FWRuntimeData:
            infoType = INFO_TYPE_FW_RUNTIME_INFO;
            break;
        case DebugInformationType::FWSavedInfo:
            infoType = INFO_TYPE_FW_SAVED_INFO;
            break;
        case DebugInformationType::DeviceDump:
            infoType = INFO_TYPE_DEVICE_DUMP;
            break;
        default:
            lg2::error("NsmDebugInfoObject: unsupported debug info type: {TP}",
                       "TP", debugInfoType);
            throw Common::Error::InvalidArgument();
    }

    if (startDebugInfoCmd() != NSM_SW_SUCCESS)
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
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    recordHandle(recHandle);

    auto rc = encode_get_network_device_debug_info_req(
        0, infoType, static_cast<uint32_t>(recHandle), requestMsg);
    if (rc == NSM_SW_SUCCESS)
    {
        getDebugInfoAsyncHandler(request).detach();
        return;
    }
    lg2::error(
        "NsmDebugInfoObject: encode_get_network_device_debug_info_req: rc={RC}",
        "RC", rc);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        finishDebugInfoCmd(OperationStatus::InvalidArgument);
        throw Common::Error::InvalidArgument();
    }
    finishDebugInfoCmd(OperationStatus::InternalFailure);
    throw Common::Error::InternalFailure();
}

} // namespace nsm
