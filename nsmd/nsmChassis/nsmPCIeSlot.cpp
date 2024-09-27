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

#include "nsmPCIeSlot.hpp"

#include "libnsm/pci-links.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeSlot::NsmPCIeSlot(const NsmInterfaceProvider<PCIeSlotIntf>& provider,
                         uint8_t deviceIndex) :
    NsmSensor(provider),
    NsmInterfaceContainer(provider), deviceIndex(deviceIndex)
{}

std::optional<Request> NsmPCIeSlot::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, 1, requestPtr);
    if (rc)
    {
        lg2::debug(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeSlot::handleResponseMsg(const struct nsm_msg* responseMsg,
                                       size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_1 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group1_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);
    if (rc)
    {
        logHandleResponseMsg(
            "NsmPCIeSlot decode_query_scalar_group_telemetry_v1_group1_resp",
            reasonCode, cc, rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        auto slotType = [](uint32_t value) -> PCIeSlotIntf::SlotTypes {
            return value == 0 ? PCIeSlotIntf::SlotTypes::Unknown
                              : PCIeSlotIntf::SlotTypes(value - 1);
        };
        pdi().slotType(slotType(data.negotiated_link_speed));
        clearErrorBitMap(
            "NsmPCIeSlot decode_query_scalar_group_telemetry_v1_group1_resp");
    }
    else
    {
        pdi().slotType(PCIeSlotIntf::SlotTypes::Unknown);

        logHandleResponseMsg(
            "NsmPCIeSlot decode_query_scalar_group_telemetry_v1_group1_resp",
            reasonCode, cc, rc);
        return rc;
    }

    return cc;
}

} // namespace nsm
