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

#include "nsmFabricManagerStateEvent.hpp"

#include "network-ports.h"

#include "dBusAsyncUtils.hpp"
#include "sensorManager.hpp"

#include <fmt/args.h>

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmFabricManagerStateEvent::NsmFabricManagerStateEvent(
    const std::string& name, const std::string& type,
    std::shared_ptr<FabricManagerIntf> fabricMgrIntf,
    std::shared_ptr<OperaStatusIntf> opStateIntf) :
    NsmEvent(name, type),
    fabricManagerIntf(fabricMgrIntf), operationalStatusIntf(opStateIntf)
{
    lg2::debug("NsmFabricManagerStateEvent: Name {NAME}.", "NAME", name);
}

int NsmFabricManagerStateEvent::handle(eid_t eid, NsmType /*type*/,
                                       NsmEventId /*eventId*/,
                                       const nsm_msg* event, size_t eventLen)
{
    lg2::debug("Received Fabric Manager State Event from EID {EID}.", "EID",
               eid);

    uint8_t eventClass{};
    uint16_t eventState{};
    nsm_get_fabric_manager_state_event_payload payload = {};

    auto rc = decode_nsm_get_fabric_manager_state_event(
        event, eventLen, &eventClass, &eventState, &payload);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_get_fabric_manager_state_event failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());

        return NSM_SW_ERROR;
    }

    switch (payload.fm_state)
    {
        case NSM_FM_STATE_OFFLINE:
            fabricManagerIntf->fmState(FMState::Offline);
            operationalStatusIntf->state(OpState::Starting);
            break;
        case NSM_FM_STATE_STANDBY:
            fabricManagerIntf->fmState(FMState::Standby);
            operationalStatusIntf->state(OpState::StandbyOffline);
            break;
        case NSM_FM_STATE_CONFIGURED:
            fabricManagerIntf->fmState(FMState::Configured);
            operationalStatusIntf->state(OpState::Enabled);
            break;
        case NSM_FM_STATE_RESERVED_TIMEOUT:
            fabricManagerIntf->fmState(FMState::Timeout);
            operationalStatusIntf->state(OpState::UnavailableOffline);
            break;
        case NSM_FM_STATE_ERROR:
            fabricManagerIntf->fmState(FMState::Error);
            operationalStatusIntf->state(OpState::UnavailableOffline);
            break;
        default:
            fabricManagerIntf->fmState(FMState::Unknown);
            operationalStatusIntf->state(OpState::None);
            break;
    }

    switch (payload.report_status)
    {
        case NSM_FM_REPORT_STATUS_NOT_RECEIVED:
            fabricManagerIntf->reportStatus(FMReportStatus::NotReceived);
            break;
        case NSM_FM_REPORT_STATUS_RECEIVED:
            fabricManagerIntf->reportStatus(FMReportStatus::Received);
            break;
        case NSM_FM_REPORT_STATUS_TIMEOUT:
            fabricManagerIntf->reportStatus(FMReportStatus::Timeout);
            break;
        default:
            fabricManagerIntf->reportStatus(FMReportStatus::Unknown);
            break;
    }

    fabricManagerIntf->lastRestartTime(payload.last_restart_timestamp);
    fabricManagerIntf->lastRestartDuration(
        payload.duration_since_last_restart_sec);

    return NSM_SW_SUCCESS;
}

} // namespace nsm
