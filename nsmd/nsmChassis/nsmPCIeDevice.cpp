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

#include "nsmPCIeDevice.hpp"

#include "libnsm/pci-links.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeDevice::NsmPCIeDevice(
    const NsmInterfaceProvider<PCIeDeviceIntf>& provider, uint8_t deviceId) :
    NsmSensor(provider), NsmInterfaceContainer(provider), deviceId(deviceId)
{}

std::optional<Request> NsmPCIeDevice::genRequestMsg(eid_t eid,
                                                    uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, deviceId,
                                                         1, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeDevice::handleResponseMsg(const struct nsm_msg* responseMsg,
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
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group1_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        auto pcieType = [](uint32_t value) -> PCIeDeviceIntf::PCIeTypes {
            return value == 0 ? PCIeDeviceIntf::PCIeTypes::Unknown
                              : PCIeDeviceIntf::PCIeTypes(value - 1);
        };
        pdi().pcIeType(pcieType(data.negotiated_link_speed));
        pdi().maxPCIeType(pcieType(data.max_link_speed));
        pdi().lanesInUse(data.negotiated_link_width);
        pdi().maxLanes(data.max_link_width);
    }
    else
    {
        pdi().pcIeType(PCIeDeviceIntf::PCIeTypes::Unknown);
        pdi().maxPCIeType(PCIeDeviceIntf::PCIeTypes::Unknown);
        pdi().lanesInUse(0);
        pdi().maxLanes(0);

        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group1_resp is not success CC. rc={RC}",
            "RC", rc);
        return rc;
    }

    return cc;
}

} // namespace nsm