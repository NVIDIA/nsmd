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

#include "nsmGpuPresenceAndPowerStatus.hpp"

#include "libnsm/platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmGpuPresenceAndPowerStatus::NsmGpuPresenceAndPowerStatus(
    const NsmInterfaceProvider<OperationalStatusIntf>& provider,
    uint8_t gpuInstanceId) :
    NsmSensor(provider), NsmInterfaceContainer(provider),
    gpuInstanceId(gpuInstanceId)
{}

std::optional<Request>
    NsmGpuPresenceAndPowerStatus::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_gpu_presence_and_power_status_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc =
        encode_get_gpu_presence_and_power_status_req(instanceId, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_get_gpu_presence_and_power_status_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmGpuPresenceAndPowerStatus::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    uint8_t gpus_presence = 0;
    uint8_t gpus_power = 0;

    auto rc = decode_get_gpu_presence_and_power_status_resp(
        responseMsg, responseLen, &cc, &reasonCode, &gpus_presence,
        &gpus_power);
    if (rc)
    {
        lg2::error(
            "responseHandler: decode_get_gpu_presence_and_power_status_resp failed with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "REASONCODE", reasonCode, "CC", cc, "RC", rc);
        return rc;
    }

    if (cc == NSM_SUCCESS)
    {
        // "State": "Enabled" if presence=active, power=active
        // "State": "UnavailableOffline" if presence=active,
        // power=inactive "State": "Absent" if presence=inactive

        bool power = ((gpus_power >> (gpuInstanceId)) & 0x1) != 0;
        bool presence = ((gpus_presence >> (gpuInstanceId)) & 0x1) != 0;
        if (power && presence)
            pdi().state(OperationalStatusIntf::StateType::Enabled);
        else if (presence)
            pdi().state(OperationalStatusIntf::StateType::UnavailableOffline);
        else
            pdi().state(OperationalStatusIntf::StateType::Absent);
    }
    else
    {
        pdi().state(OperationalStatusIntf::StateType::Fault);
        lg2::error(
            "responseHandler: decode_get_gpu_presence_and_power_status_resp is not success CC. rc={RC}",
            "RC", rc);
        return rc;
    }

    return cc;
}

} // namespace nsm