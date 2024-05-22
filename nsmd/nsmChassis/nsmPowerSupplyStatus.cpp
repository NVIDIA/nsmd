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

#include "libnsm/platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPowerSupplyStatus::NsmPowerSupplyStatus(
    const NsmInterfaceProvider<PowerStateIntf>& provider,
    uint8_t gpuInstanceId) :
    NsmSensor(provider), NsmInterfaceContainer(provider),
    gpuInstanceId(gpuInstanceId)
{}

std::optional<Request> NsmPowerSupplyStatus::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_power_supply_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_power_supply_status_req(instanceId, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_get_power_supply_status_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t
    NsmPowerSupplyStatus::handleResponseMsg(const struct nsm_msg* responseMsg,
                                            size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t status = 0;

    auto rc = decode_get_power_supply_status_resp(responseMsg, responseLen, &cc,
                                                  &reasonCode, &status);
    if (rc)
    {
        lg2::error(
            "responseHandler: decode_get_power_supply_status_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        for (auto pdi : interfaces)
        {
            pdi->currentPowerState(((status >> gpuInstanceId) & 0x01) != 0
                                       ? PowerStateIntf::PowerState::On
                                       : PowerStateIntf::PowerState::Off);
        }
    }
    else
    {
        for (auto pdi : interfaces)
        {
            pdi->currentPowerState(PowerStateIntf::PowerState::Unknown);
        }
        lg2::error(
            "responseHandler: decode_get_power_supply_status_resp is not success CC. rc={RC}",
            "RC", rc);
        return rc;
    }

    return cc;
}

} // namespace nsm