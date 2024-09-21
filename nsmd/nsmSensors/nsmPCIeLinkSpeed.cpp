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

#include "nsmPCIeLinkSpeed.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeLinkSpeedBase::NsmPCIeLinkSpeedBase(const NsmObject& provider,
                                           uint8_t deviceIndex) :
    NsmSensor(provider),
    deviceIndex(deviceIndex)
{}

std::optional<Request> NsmPCIeLinkSpeedBase::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_1, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmPCIeLinkSpeedBase::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_1 data{};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);

    if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
    {
        handleResponse(data);
        clearErrorBitMap("decode_query_scalar_group_telemetry_v1_group1_resp");
    }
    else
    {
        memset(&data, 0, sizeof(data));
        handleResponse(data);
        logHandleResponseMsg(
            "decode_query_scalar_group_telemetry_v1_group1_resp", reasonCode,
            cc, rc);
    }
    updateMetricOnSharedMemory();

    return cc ? cc : rc;
}

PCIeSlotIntf::Generations NsmPCIeLinkSpeedBase::generation(uint32_t value)
{
    return (value == 0) || (value > 6) ? PCIeSlotIntf::Generations::Unknown
                                       : PCIeSlotIntf::Generations(value - 1);
}
PCIeDeviceIntf::PCIeTypes NsmPCIeLinkSpeedBase::pcieType(uint32_t value)
{
    return (value == 0) || (value > 6) ? PCIeDeviceIntf::PCIeTypes::Unknown
                                       : PCIeDeviceIntf::PCIeTypes(value - 1);
};
uint32_t NsmPCIeLinkSpeedBase::linkWidth(uint32_t value)
{
    return (value > 0) ? (uint32_t)pow(2, value - 1) : 0;
}

} // namespace nsm
