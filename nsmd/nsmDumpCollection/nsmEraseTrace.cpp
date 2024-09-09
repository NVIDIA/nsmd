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
}

std::tuple<uint64_t, EraseStatus> NsmEraseTraceObject::erase()
{
    SensorManager& manager = SensorManager::getInstance();
    auto device = manager.getNsmDevice(uuid);
    auto eid = manager.getEid(device);
    std::tuple<uint64_t, EraseStatus> result(NSM_ERROR, EraseStatus::Unknown);

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_req));
    auto requestMsg = reinterpret_cast<struct nsm_msg*>(request.data());

    auto rc = encode_erase_trace_req(0, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("NsmEraseTraceObject: encode_erase_trace_req: rc={RC}", "RC",
                   rc);
        return result;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc_ = manager.SendRecvNsmMsgSync(eid, request, responseMsg,
                                          responseLen);
    if (rc_ != NSM_SW_SUCCESS)
    {
        lg2::error("NsmEraseTraceObject: getRequest SendRecvNsmMsg: "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc_);
        return result;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t resStatus = 0;

    rc = decode_erase_trace_resp(responseMsg.get(), responseLen, &cc,
                                 &reasonCode, &resStatus);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        std::cerr << "Response message error: "
                  << "rc=" << rc << ", cc=" << (int)cc
                  << ", reasonCode=" << (int)reasonCode << "\n";
        return result;
    }

    std::get<0>(result) = NSM_SUCCESS;
    switch (resStatus)
    {
        case ERASE_TRACE_NO_DATA_ERASED:
            std::get<1>(result) = EraseStatus::NoDataErased;
            break;
        case ERASE_TRACE_DATA_ERASED:
            std::get<1>(result) = EraseStatus::DataErased;
            break;
        case ERASE_TRACE_DATA_ERASE_INPROGRESS:
            std::get<1>(result) = EraseStatus::DataEraseInProgress;
            break;
        default:
            lg2::error(
                "NsmEraseTraceObject: unsupported response status type: {TP}",
                "TP", resStatus);
            std::get<1>(result) = EraseStatus::Unknown;
            return result;
    }
    return result;
}

} // namespace nsm
