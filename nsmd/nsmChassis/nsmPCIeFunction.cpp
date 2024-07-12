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

#include "nsmPCIeFunction.hpp"

#include "libnsm/pci-links.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeFunction::NsmPCIeFunction(
    const NsmInterfaceProvider<PCIeDeviceIntf>& provider, uint8_t deviceIndex,
    uint8_t functionId) :
    NsmSensor(provider), NsmInterfaceContainer(provider),
    deviceIndex(deviceIndex), functionId(functionId)
{}

std::optional<Request> NsmPCIeFunction::genRequestMsg(eid_t eid,
                                                      uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_0, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeFunction::handleResponseMsg(const struct nsm_msg* responseMsg,
                                           size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_0 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group0_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);

    auto error = rc != NSM_SUCCESS || cc != NSM_SUCCESS;
    if (error)
    {
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group0_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
    }
    auto hexFormat = [error](const uint32_t value) -> std::string {
        if (error)
            return "";
        std::string hexStr(6, '\0');
        sprintf(hexStr.data(), "0x%04X", (uint16_t)value);
        return hexStr;
    };

#define pcieFunction(X)                                                        \
    pdi().function##X##VendorId(hexFormat(data.pci_vendor_id));                \
    pdi().function##X##DeviceId(hexFormat(data.pci_device_id));                \
    pdi().function##X##ClassCode(hexFormat(0));                                \
    pdi().function##X##RevisionId(hexFormat(0));                               \
    pdi().function##X##FunctionType("Physical");                               \
    pdi().function##X##DeviceClass("Processor");                               \
    pdi().function##X##SubsystemVendorId(                                      \
        hexFormat(data.pci_subsystem_vendor_id));                              \
    pdi().function##X##SubsystemId(hexFormat(data.pci_subsystem_device_id));

    switch (functionId)
    {
        case 0:
            pcieFunction(0);
            break;
        case 1:
            pcieFunction(1);
            break;
        case 2:
            pcieFunction(2);
            break;
        case 3:
            pcieFunction(3);
            break;
        case 4:
            pcieFunction(4);
            break;
        case 5:
            pcieFunction(5);
            break;
        case 6:
            pcieFunction(6);
            break;
        case 7:
            pcieFunction(7);
            break;
    }

    return cc ? cc : rc;
}

} // namespace nsm
