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
#include "sharedMemCommon.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmGpuPresenceAndPowerStatus::NsmGpuPresenceAndPowerStatus(
    const NsmInterfaceProvider<OperationalStatusIntf>& provider,
    uint8_t gpuInstanceId) :
    NsmSensor(provider),
    NsmInterfaceContainer(provider), gpuInstanceId(gpuInstanceId),
    gpusPresence{}, gpusPower{}, state{}
{
    updateMetricOnSharedMemory();
}

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

        for (auto& [_, pdi] : interfaces)
        {
            if (power && presence)
            {
                pdi->state(OperationalStatusIntf::StateType::Enabled);
                pdi->functional(true);
            }
            else if (presence)
            {
                pdi->state(
                    OperationalStatusIntf::StateType::UnavailableOffline);
                pdi->functional(false);
            }
            else
            {
                pdi->state(OperationalStatusIntf::StateType::Absent);
                pdi->functional(false);
            }
        }
    }
    else
    {
        for (auto& [_, pdi] : interfaces)
        {
            pdi->state(OperationalStatusIntf::StateType::Fault);
            pdi->functional(false);
        }
    }
    updateMetricOnSharedMemory();

    // coverity[missing_return]
    co_return rc;
}

void NsmGpuPresenceAndPowerStatus::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    std::vector<uint8_t> data;
    for (auto& [path, pdi] : interfaces)
    {
        nsm_shmem_utils::updateSharedMemoryOnSuccess(
            path, pdi->interface, "State", data,
            OperationalStatusIntf::convertStateTypeToString(pdi->state()));
    }
#endif
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
            lg2::debug(
                "NsmGpuPresenceAndPowerStatus::genRequestMsg unsupported state. eid={EID} rc={RC}, state={STATE}",
                "EID", eid, "RC", rc, "STATE", int(state));
            break;
    }

    if (rc)
    {
        lg2::debug(
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
    std::string decodeMethodName = "";

    switch (state)
    {
        case State::GetPresence:
            rc = decode_get_gpu_presence_resp(responseMsg, responseLen, &cc,
                                              &reasonCode, &gpusPresence);
            decodeMethodName = "decode_get_gpu_presence_resp";
            break;
        case State::GetPowerStatus:
            rc = decode_get_gpu_power_status_resp(responseMsg, responseLen, &cc,
                                                  &reasonCode, &gpusPower);
            decodeMethodName = "decode_get_gpu_power_status_resp";
            break;
        default:
            lg2::debug(
                "NsmGpuPresenceAndPowerStatus::handleResponseMsg unsupported state. rc={RC}, state={STATE}",
                "RC", rc, "STATE", int(state));
            break;
    }

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        clearErrorBitMap(decodeMethodName);
    }
    else
    {
        logHandleResponseMsg(decodeMethodName, reasonCode, cc, rc);
    }
    return rc;
}

} // namespace nsm
