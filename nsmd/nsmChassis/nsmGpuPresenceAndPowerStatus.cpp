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

#include "device-configuration.h"

#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmGpuPresenceAndPowerStatus::NsmGpuPresenceAndPowerStatus(
    const NsmInterfaceProvider<OperationalStatusIntf>& provider,
    uint8_t gpuInstanceId) :
    NsmSensor(provider), NsmInterfaceContainer(provider),
    gpuInstanceId(gpuInstanceId)
{}

requester::Coroutine
    NsmGpuPresenceAndPowerStatus::update(SensorManager& manager, eid_t eid)
{
    uint8_t rc = NSM_SW_SUCCESS;
    for (int state = (int)State::GetPresence;
         state <= (int)State::GetPowerStatus && rc == NSM_SW_SUCCESS; state++)
    {
        NsmGpuPresenceAndPowerStatus::state = (State)state;
        rc = co_await NsmSensor::update(manager, eid);
    }

    if (rc == NSM_SW_SUCCESS)
    {
        // "State": "Enabled" if presence=active, power=active
        // "State": "UnavailableOffline" if presence=active,
        // power=inactive "State": "Absent" if presence=inactive

        bool power = ((gpusPower >> (gpuInstanceId)) & 0x1) != 0;
        bool presence = ((gpusPresence >> (gpuInstanceId)) & 0x1) != 0;

        for (auto pdi : interfaces)
        {
            if (power && presence)
                pdi->state(OperationalStatusIntf::StateType::Enabled);
            else if (presence)
                pdi->state(
                    OperationalStatusIntf::StateType::UnavailableOffline);
            else
                pdi->state(OperationalStatusIntf::StateType::Absent);
        }
    }
    else
    {
        for (auto pdi : interfaces)
        {
            pdi->state(OperationalStatusIntf::StateType::Fault);
        }
        lg2::error(
            "responseHandler: decode_get_gpu_power_status_resp is not success CC. rc={RC}",
            "RC", rc);
    }

    co_return rc;
}

std::optional<Request>
    NsmGpuPresenceAndPowerStatus::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    int rc = NSM_SW_ERROR;
    switch (state)
    {
        case State::GetPresence:
            rc = encode_get_fpga_diagnostics_settings_req(
                instanceId, GET_GPU_PRESENCE, requestPtr);
            break;
        case State::GetPowerStatus:
            rc = encode_get_fpga_diagnostics_settings_req(
                instanceId, GET_GPU_POWER_STATUS, requestPtr);
            break;
        default:
            lg2::error(
                "NsmGpuPresenceAndPowerStatus::genRequestMsg unsupported state. eid={EID} rc={RC}, state={STATE}",
                "EID", eid, "RC", rc, "STATE", int(state));
            break;
    }

    if (rc)
    {
        lg2::error(
            "NsmGpuPresenceAndPowerStatus::genRequestMsg failed. eid={EID} rc={RC}, state={STATE}",
            "EID", eid, "RC", rc, "STATE", int(state));
        return std::nullopt;
    }

    return request;
}

uint8_t NsmGpuPresenceAndPowerStatus::handleResponseMsg(
    const struct nsm_msg* responseMsg, size_t responseLen)
{
    uint8_t rc = NSM_SW_ERROR;
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;

    switch (state)
    {
        case State::GetPresence:
            rc = decode_get_gpu_presence_resp(responseMsg, responseLen, &cc,
                                              &reasonCode, &gpusPresence);
            break;
        case State::GetPowerStatus:
            rc = decode_get_gpu_power_status_resp(responseMsg, responseLen, &cc,
                                                  &reasonCode, &gpusPower);
            break;
        default:
            lg2::error(
                "NsmGpuPresenceAndPowerStatus::handleResponseMsg unsupported state. rc={RC}, state={STATE}",
                "RC", rc, "STATE", int(state));
            break;
    }

    if (cc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmGpuPresenceAndPowerStatus::handleResponseMsg  is not success CC. rc={RC}, cc={CC}, reasonCode={REASON_CODE}",
            "RC", rc, "CC", cc, "REASON_CODE", reasonCode);
    }
    return rc;
}

} // namespace nsm
