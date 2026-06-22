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

#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <filesystem>

#define NSM_DIAGNOSTICS_START_RECORD 0x00
#define NSM_DIAGNOSTICS_END_RECORD 0xFF
#define NSM_DEBUG_INFO_START_RECORD 0x00
#define NSM_DEBUG_INFO_END_RECORD 0x00
#define NSM_DEBUG_INFO_BUFFER_SIZE 65535

namespace nsm
{

std::string getDumpPath(const std::string& name,
                        const std::string& inventoryPath,
                        DebugDumpType dumpType)
{
    using std::filesystem::path;
    if (dumpType == DebugDumpType::Diagnostics)
    {
        return path(inventoryPath) / "Diagnostics" / "Dump" / name;
    }
    return path(inventoryPath) / name;
}

NsmDebugInfoObject::NsmDebugInfoObject(sdbusplus::bus_t& bus,
                                       const std::string& name,
                                       const std::string& inventoryPath,
                                       const std::string& type,
                                       const uuid_t& uuid,
                                       DebugDumpType dumpType) :
    NsmObject(name, type),
    DebugInfoIntf(bus, getDumpPath(name, inventoryPath, dumpType).c_str()),
    uuid(uuid)
{
    buffer.reserve(NSM_DEBUG_INFO_BUFFER_SIZE);
    lg2::debug("Created NsmDebugInfoObject: {NAME}", "NAME", name.c_str());
    supportedDumpType(dumpType);
}

void NsmDebugInfoObject::finish(AsyncOperationStatusType status, uint8_t rc)
{
    statusInterface->status(status);
    valueInterface->value(rc);
    close(fd);
}

void NsmDebugInfoObject::getDebugInfoAsyncHandler(uint32_t recordHandle)
{
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());

    encode_get_network_device_debug_info_req(0, infoType, recordHandle,
                                             requestMsg);
    getDebugInfoAsyncHandler(request).detach();
}

requester::Coroutine NsmDebugInfoObject::getDebugInfoAsyncHandler(
    std::shared_ptr<Request> request)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await device->postPatchIO(eid, *request, responseMsg,
                                           responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmDebugInfoObject::getDebugInfoAsyncHandler postPatchIO: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        finish(AsyncOperationStatusType::InternalFailure, rc);
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t bufferSize = 0;
    uint32_t nextHandle = 0;

    buffer.resize(NSM_DEBUG_INFO_BUFFER_SIZE);
    rc = decode_get_network_device_debug_info_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &bufferSize,
        buffer.data(), &nextHandle);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmDebugInfoObject: decode_get_network_device_debug_info_resp: "
            "eid={EID} rc={RC} cc={CC} len={LEN}",
            "EID", eid, "RC", rc, "CC", cc, "LEN", responseLen);
        finish(AsyncOperationStatusType::InternalFailure, rc);
        // coverity[missing_return]
        co_return rc;
    }
    buffer.resize(bufferSize);

    try
    {
        utils::appendBufferToFd(fd, buffer);
    }
    catch (const std::exception& e)
    {
        lg2::error("NsmDebugInfoObject: appendBufferToFd failed: {ERR}", "ERR",
                   e.what());
        finish(AsyncOperationStatusType::WriteFailure, NSM_SW_ERROR);
        co_return NSM_SW_ERROR;
    }

    if (nextHandle == NSM_DEBUG_INFO_END_RECORD)
    {
        finish(AsyncOperationStatusType::Success, NSM_SW_SUCCESS);
    }
    else
    {
        getDebugInfoAsyncHandler(nextHandle);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

sdbusplus::object_path
    NsmDebugInfoObject::getDebugInfo(DebugInformationType debugInfoType,
                                     sdbusplus::message::unix_fd fd)
{
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

    if (statusInterface != nullptr &&
        statusInterface->status() == AsyncOperationStatusType::InProgress)
    {
        lg2::debug("NsmDebugInfoObject: getDebugInfo already in progress");
        throw Common::Error::Unavailable();
    }

    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::debug(
            "NsmDebugInfoObject: getDebugInfo: cannot start async operation, objectPath is empty");
        throw Common::Error::Unavailable();
    }

    this->statusInterface = statusInterface;
    this->valueInterface = valueInterface;
    this->fd = dup(fd);
    if (this->fd < 0)
    {
        lg2::error(
            "NsmDebugInfoObject: Failed to duplicate file descriptor: {ERR}",
            "ERR", strerror(errno));
        finish(AsyncOperationStatusType::InternalFailure, NSM_SW_ERROR);
        throw Common::Error::InternalFailure();
    }
    getDebugInfoAsyncHandler(NSM_DEBUG_INFO_START_RECORD);
    return objectPath;
}

void NsmDebugInfoObject::getDiagnosticsAsyncHandler(uint32_t recordHandle)
{
    auto request = std::make_shared<Request>(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
    encode_get_device_diagnostics_req(0, static_cast<uint8_t>(recordHandle),
                                      requestMsg);
    getDiagnosticsAsyncHandler(request).detach();
}

requester::Coroutine NsmDebugInfoObject::getDiagnosticsAsyncHandler(
    std::shared_ptr<Request> request)
{
    auto device = SensorManager::getInstance().getNsmDeviceFromStaticUUID(uuid);
    auto eid = device->getEid();
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await device->postPatchIO(eid, *request, responseMsg,
                                           responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmDebugInfoObject: getRequest postPatchIO: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        finish(AsyncOperationStatusType::InternalFailure, rc);
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint16_t bufferSize = 0;
    uint8_t nextHandle = 0;

    buffer.resize(NSM_DEBUG_INFO_BUFFER_SIZE);
    rc = decode_get_device_diagnostics_resp(responseMsg.get(), responseLen, &cc,
                                            &reasonCode, buffer.data(),
                                            &bufferSize, &nextHandle);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("NsmDebugInfoObject: decode_get_device_diagnostics_resp: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        finish(AsyncOperationStatusType::InternalFailure, rc);
        // coverity[missing_return]
        co_return rc;
    }
    buffer.resize(bufferSize);

    try
    {
        utils::appendBufferToFd(fd, buffer);
    }
    catch (const std::exception& e)
    {
        lg2::error("NsmDebugInfoObject: appendBufferToFd failed: {ERR}", "ERR",
                   e.what());
        finish(AsyncOperationStatusType::WriteFailure, NSM_SW_ERROR);
        co_return NSM_SW_ERROR;
    }

    if (nextHandle == NSM_DIAGNOSTICS_END_RECORD)
    {
        finish(AsyncOperationStatusType::Success, NSM_SW_SUCCESS);
    }
    else
    {
        getDiagnosticsAsyncHandler(nextHandle);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

sdbusplus::object_path
    NsmDebugInfoObject::getDiagnostics(sdbusplus::message::unix_fd fd)
{
    if (statusInterface != nullptr &&
        statusInterface->status() == AsyncOperationStatusType::InProgress)
    {
        lg2::debug("NsmDebugInfoObject: getDiagnostics already in progress");
        throw Common::Error::Unavailable();
    }

    const auto [objectPath, statusInterface, valueInterface] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    if (objectPath.empty())
    {
        lg2::debug(
            "NsmDebugInfoObject: getDiagnostics: cannot start async operation, objectPath is empty");
        throw Common::Error::Unavailable();
    }

    this->statusInterface = statusInterface;
    this->valueInterface = valueInterface;
    this->fd = dup(fd);
    if (this->fd < 0)
    {
        lg2::error(
            "NsmDebugInfoObject: Failed to duplicate file descriptor: {ERR}",
            "ERR", strerror(errno));
        finish(AsyncOperationStatusType::InternalFailure, NSM_SW_ERROR);
        throw Common::Error::InternalFailure();
    }
    getDiagnosticsAsyncHandler(NSM_DIAGNOSTICS_START_RECORD);
    return objectPath;
}

} // namespace nsm
