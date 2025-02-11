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

#include "nsmPowerSupplyStatus.hpp"

#include "device-configuration.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPowerSupplyStatus::NsmPowerSupplyStatus(
    const NsmInterfaceProvider<PowerStateIntf>& provider,
    uint8_t gpuInstanceId) :
    NsmGroupSensor(provider),
    NsmInterfaceContainer(provider), gpuInstanceId(gpuInstanceId)
{}

std::optional<Request> NsmPowerSupplyStatus::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_POWER_SUPPLY_STATUS, requestPtr);
    if (rc)
    {
        lg2::debug(
            "encode_get_fpga_diagnostics_settings_req(GET_POWER_SUPPLY_STATUS) failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPowerSupplyStatus::handleResponse(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t status = 0;

    auto rc = decode_get_power_supply_status_resp(responseMsg, responseLen, &cc,
                                                  &reasonCode, &status);
    if (rc)
    {
        lg2::debug(
            "responseHandler: decode_get_power_supply_status_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        invoke(pdiMethod(currentPowerState),
               ((status >> gpuInstanceId) & 0x01) != 0
                   ? PowerStateIntf::PowerState::On
                   : PowerStateIntf::PowerState::Off);
        clearErrorBitMap("decode_get_power_supply_status_resp");
    }
    else
    {
        invoke(pdiMethod(currentPowerState),
               PowerStateIntf::PowerState::Unknown);
        logHandleResponseMsg("decode_get_power_supply_status_resp", reasonCode,
                             cc, rc);
        return rc;
    }

    return cc;
}

} // namespace nsm
