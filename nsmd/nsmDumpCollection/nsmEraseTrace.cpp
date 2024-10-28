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

#include "nsmEraseTrace.hpp"

#include "diagnostics.h"

#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"
#include "utils.hpp"

#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

using std::filesystem::path;

namespace nsm
{

NsmEraseTraceObject::NsmEraseTraceObject(sdbusplus::bus::bus& bus,
                                         const std::string& name,
                                         const std::string& inventoryPath,
                                         const std::string& type,
                                         const uuid_t& uuid) :
    NsmObject(name, type),
    EraseIntf(bus, (inventoryPath + name).c_str()), uuid(uuid)
{
    lg2::debug("NsmEraseTraceObject: {NAME}", "NAME", name.c_str());
    objPath = inventoryPath + name;
    EraseIntf::eraseTraceStatus(std::make_tuple(
        EraseOperationStatus::Unavailable, EraseStatus::Unknown));
    EraseIntf::eraseDebugInfoStatus(std::make_tuple(
        EraseOperationStatus::Unavailable, EraseStatus::Unknown));
}

void NsmEraseTraceObject::eraseTrace()
{
    if (std::get<0>(eraseTraceStatus()) == EraseOperationStatus::InProgress)
    {
        throw Common::Error::Unavailable();
        return;
    }
    eraseTraceStatus(std::make_tuple(EraseOperationStatus::InProgress,
                                     EraseStatus::Unknown));
    eraseTraceOnDevice().detach();
}

void NsmEraseTraceObject::eraseDebugInfo(EraseInfoType infoType)
{
    if (std::get<0>(eraseDebugInfoStatus()) == EraseOperationStatus::InProgress)
    {
        throw Common::Error::Unavailable();
        return;
    }

    nsm_erase_information_type type;
    switch (infoType)
    {
        case EraseInfoType::FWSavedDumpInfo:
            type = INFO_TYPE_FW_SAVED_DUMP_INFO;
            break;
        default:
            lg2::error("NsmEraseDebugInfoObject: unsupported info type: {TP}",
                       "TP", infoType);
            eraseDebugInfoStatus(std::make_tuple(
                EraseOperationStatus::InternalFailure, EraseStatus::Unknown));
            return;
    }
    eraseDebugInfoStatus(std::make_tuple(EraseOperationStatus::InProgress,
                                         EraseStatus::Unknown));
    eraseDebugInfoOnDevice(type).detach();
}

requester::Coroutine NsmEraseTraceObject::eraseTraceOnDevice()
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::tuple<EraseOperationStatus, EraseStatus> result(eraseTraceStatus());
    auto& [operationStatus, eraseStatus] = result;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_erase_trace_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmEraseTraceObject: encode_erase_trace_req: rc={RC}", "RC",
                   rc);
        operationStatus = EraseOperationStatus::InternalFailure;
        eraseTraceStatus(result);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmEraseTraceObject: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        operationStatus = EraseOperationStatus::InternalFailure;
        eraseTraceStatus(result);
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t resStatus = 0;
    rc = decode_erase_trace_resp(responseMsg.get(), responseLen, &cc,
                                 &reasonCode, &resStatus);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmEraseTraceObject: decode_erase_trace_resp failed with rc = {RC}, cc = {CC} and reason_code = {REASON_CODE}",
            "RC", rc, "CC", cc, "REASON_CODE", reasonCode);
        operationStatus = EraseOperationStatus::InternalFailure;
        eraseTraceStatus(result);
        co_return rc;
    }

    operationStatus = EraseOperationStatus::Success;
    switch (resStatus)
    {
        case ERASE_TRACE_NO_DATA_ERASED:
            eraseStatus = EraseStatus::NoDataErased;
            break;
        case ERASE_TRACE_DATA_ERASED:
            eraseStatus = EraseStatus::DataErased;
            break;
        case ERASE_TRACE_DATA_ERASE_INPROGRESS:
            eraseStatus = EraseStatus::DataEraseInProgress;
            break;
        default:
            lg2::error(
                "NsmEraseTraceObject: unsupported response status type: {TP}",
                "TP", resStatus);
            eraseStatus = EraseStatus::Unknown;
            break;
    }
    eraseTraceStatus(result);
    co_return rc;
}

requester::Coroutine
    NsmEraseTraceObject::eraseDebugInfoOnDevice(uint8_t infoType)
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::tuple<EraseOperationStatus, EraseStatus> result(
        eraseDebugInfoStatus());
    auto& [operationStatus, eraseStatus] = result;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_erase_debug_info_req(0, infoType, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmEraseDebugInfoObject: encode_erase_debug_info_req: rc={RC}",
            "RC", rc);
        operationStatus = EraseOperationStatus::InternalFailure;
        eraseDebugInfoStatus(result);
        co_return rc;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await manager.SendRecvNsmMsg(eid, request, responseMsg,
                                         responseLen);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmEraseDebugInfoObject: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        operationStatus = EraseOperationStatus::InternalFailure;
        eraseDebugInfoStatus(result);
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t resStatus = 0;
    rc = decode_erase_debug_info_resp(responseMsg.get(), responseLen, &cc,
                                      &reasonCode, &resStatus);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmEraseDebugInfoObject: decode_erase_debug_info_resp failed with rc = {RC}, cc = {CC} and reason_code = {REASON_CODE}",
            "RC", rc, "CC", cc, "REASON_CODE", reasonCode);
        operationStatus = EraseOperationStatus::InternalFailure;
        eraseDebugInfoStatus(result);
        co_return rc;
    }

    operationStatus = EraseOperationStatus::Success;
    switch (resStatus)
    {
        case ERASE_TRACE_NO_DATA_ERASED:
            eraseStatus = EraseStatus::NoDataErased;
            break;
        case ERASE_TRACE_DATA_ERASED:
            eraseStatus = EraseStatus::DataErased;
            break;
        case ERASE_TRACE_DATA_ERASE_INPROGRESS:
            eraseStatus = EraseStatus::DataEraseInProgress;
            break;
        default:
            lg2::error(
                "NsmEraseDebugInfoObject: unsupported response status type: {TP}",
                "TP", resStatus);
            eraseStatus = EraseStatus::Unknown;
            break;
    }
    eraseDebugInfoStatus(result);
    co_return rc;
}

} // namespace nsm
