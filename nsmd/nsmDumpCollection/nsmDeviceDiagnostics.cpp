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

#include "nsmDeviceDiagnostics.hpp"

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

NsmDeviceDiagnostics::NsmDeviceDiagnostics(sdbusplus::bus::bus& bus,
                                           const std::string& name,
                                           const std::string& inventoryPath,
                                           const std::string& type,
                                           const uuid_t& uuid) :
    NsmObject(name, type),
    DiagnosticsIntf(bus, (inventoryPath + "Diagnostics/Dump/" + name).c_str()),
    uuid(uuid)
{
    lg2::debug("NsmDeviceDiagnostics: {NAME}", "NAME", name.c_str());
    fdName = name + "_device_diagnostics";
    sdbusplus::message::unix_fd unixFd(0);
    fd(unixFd, true);
    supportedDumpType(DumpType::Diagnostics);
}

void NsmDeviceDiagnostics::getDebugInfo(
    DiagnosticsInformationType debugInfoType, uint64_t recHandle)
{
    if (debugInfoType != DiagnosticsInformationType::DeviceDump)
    {
        throw Common::Error::InvalidArgument();
    }

    if (startDiagnosticsCmd() != NSM_SW_SUCCESS)
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
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    recordHandle(recHandle);

    auto rc = encode_get_device_diagnostics_req(
        0, static_cast<uint8_t>(recHandle), requestMsg);

    if (rc == NSM_SW_SUCCESS)
    {
        getDiagnosticsAsyncHandler(request).detach();
        return;
    }

    lg2::error(
        "NsmDeviceDiagnosticsObject: encode_get_device_diagnostics_req: rc={RC}",
        "RC", rc);
    if (rc == NSM_ERR_INVALID_DATA)
    {
        finishDiagnosticsCmd(GetDiagnosticsStatus::InvalidArgument);
        throw Common::Error::InvalidArgument();
    }
    finishDiagnosticsCmd(GetDiagnosticsStatus::InternalFailure);
    throw Common::Error::InternalFailure();
}

requester::Coroutine NsmDeviceDiagnostics::getDiagnosticsAsyncHandler(
    std::shared_ptr<Request> request)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::vector<uint8_t> segData(65535, 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto sendRc = co_await manager.SendRecvNsmMsg(eid, *request, responseMsg,
                                                  responseLen);
    if (sendRc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmDeviceDiagnostics: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", sendRc);
        finishDiagnosticsCmd(GetDiagnosticsStatus::InternalFailure);
        co_return sendRc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t segDataSize = 0;
    uint8_t nextRecHandle = 0;

    auto rc = decode_get_device_diagnostics_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, segData.data(),
        &segDataSize, &nextRecHandle);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("NsmDeviceDiagnostics : decode_get_device_diagnostics_resp: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        finishDiagnosticsCmd(GetDiagnosticsStatus::InternalFailure);
        co_return rc;
    }

    int fileDesc = memfd_create(fdName.c_str(), 0);
    if (fileDesc == -1)
    {
        lg2::error(
            "NsmDeviceDiagnostics: memfd_create: eid={EID} error={ERROR}",
            "EID", eid, "ERROR", strerror(errno));
        finishDiagnosticsCmd(GetDiagnosticsStatus::WriteFailure);
        co_return NSM_SW_ERROR;
    }

    const uint8_t* requestPtr = segData.data();
    while (segDataSize != 0)
    {
        ssize_t written = write(fileDesc, requestPtr, segDataSize);
        if (written < 0)
        {
            lg2::error("NsmDeviceDiagnostics: write: eid={EID} error={ERROR}",
                       "EID", eid, "ERROR", strerror(errno));
            close(fileDesc);
            finishDiagnosticsCmd(GetDiagnosticsStatus::WriteFailure);
            co_return NSM_SW_ERROR;
        }
        requestPtr += written;
        segDataSize -= written;
    }
    (void)lseek(fileDesc, 0, SEEK_SET);
    sdbusplus::message::unix_fd unixFd(fileDesc);
    fd(unixFd, true);
    nextRecordHandle(static_cast<uint64_t>(nextRecHandle));
    finishDiagnosticsCmd(GetDiagnosticsStatus::Success);
    co_return NSM_SW_SUCCESS;
}

void NsmDeviceDiagnostics::finishDiagnosticsCmd(GetDiagnosticsStatus opStatus)
{
    status(opStatus);
    cmdInProgress = false;
}

uint8_t NsmDeviceDiagnostics::startDiagnosticsCmd()
{
    if (cmdInProgress)
    {
        return NSM_SW_ERROR;
    }
    cmdInProgress = true;
    status(GetDiagnosticsStatus::InProgress);
    return NSM_SW_SUCCESS;
}
} // namespace nsm
