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

#include "nsmPCIeLTSSMState.hpp"

#include "libnsm/pci-links.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmPCIeLTSSMState::NsmPCIeLTSSMState(
    const NsmInterfaceProvider<LTSSMStateIntf>& provider, uint8_t deviceIndex) :
    NsmSensor(provider), NsmInterfaceContainer(provider),
    deviceIndex(deviceIndex)
{}

std::optional<Request> NsmPCIeLTSSMState::genRequestMsg(eid_t eid,
                                                        uint8_t instanceId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, deviceIndex, GROUP_ID_6, requestPtr);
    if (rc)
    {
        lg2::error(
            "encode_query_scalar_group_telemetry_v1_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPCIeLTSSMState::handleResponseMsg(const struct nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_6 data = {};
    uint16_t size = 0;

    auto rc = decode_query_scalar_group_telemetry_v1_group6_resp(
        responseMsg, responseLen, &cc, &size, &reasonCode, &data);

    if (rc == NSM_SUCCESS && cc == NSM_SUCCESS)
    {
        // Current LTSSM state. The value is encoded as follows:
        // 0x0 – Detect
        // 0x1 – Polling
        // 0x2 – Configuration
        // 0x3 – Recovery
        // 0x4 – Recovery.EQ
        // 0x5 – L0
        // 0x6 – L0s
        // 0x7 – L1
        // 0x8 – L1_PLL_PD
        // 0x9 – L2
        // 0xA – L1 CPM
        // 0xB – L1.1
        // 0xC – L1.2
        // 0xD – Hot Reset
        // 0xE – Loopback
        // 0xF – Disabled
        // 0x10 – Link down
        // 0x11 – Link ready
        // 0x12 – Lanes in sleep
        // 0xFF – Illegal state

        LTSSMStateIntf::State state =
            data.ltssm_state == 0xFF ? LTSSMStateIntf::State::IllegalState
                                     : LTSSMStateIntf::State(data.ltssm_state);
        pdi().ltssmState(state);
    }
    else
    {
        pdi().ltssmState(LTSSMStateIntf::State::NA);
        lg2::error(
            "responseHandler: decode_query_scalar_group_telemetry_v1_group6_resp failed. rc={RC}, cc={CC}",
            "RC", rc, "CC", cc);
    }

    return rc;
}

} // namespace nsm
