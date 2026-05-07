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

#include "nsmDiagSetFlowControlEvent.hpp"

#include "diagnostics.h"

#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmPreBootDiagStateClient.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmDiagSetFlowControlEvent::NsmDiagSetFlowControlEvent(
    const std::string& name, const std::string& type) : NsmEvent(name, type)
{}

int NsmDiagSetFlowControlEvent::handle(eid_t eid, NsmType /*type*/,
                                       NsmEventId /*eventId*/,
                                       const nsm_msg* event, size_t eventLen)
{
    uint8_t eventClass{};
    uint16_t eventState{};
    uint8_t flowCtrlStatus{};

    auto nsmDevice =
        mctp::MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);

    auto rc = decode_nsm_diag_set_flow_control_event(
        event, eventLen, &eventClass, &eventState, &flowCtrlStatus);
    if (rc != NSM_SW_SUCCESS)
    {
        if (!nsmDevice || nsmDevice->shouldLog("PreBootDiag:decodeFlowCtrl",
                                               nsm_sw_codes(rc)))
        {
            lg2::error(
                "PreBootDiag: decode setDiagFlowCtrl failed rc={RC} EID={EID}",
                "RC", rc, "EID", eid);
        }
        return rc;
    }

    // Include the source EID so multi-CPU prebootdiag subscribers can
    // disambiguate which CPU emitted the event — matches the payload
    // shape used by the sibling Diag*Event handlers
    // (nsmDiagGetSystemConfigEvent.cpp, nsmDiagSetTestResultEvent.cpp).
    nlohmann::json payload;
    payload["Eid"] = static_cast<int>(eid);

    if (flowCtrlStatus == NSM_DIAG_FLOW_CTRL_IN_PROGRESS)
    {
        postStateUpdate(preBootDiagState::heartbeatReceived, payload.dump());
        lg2::info("PreBootDiag: HeartbeatReceived EID={EID}", "EID", eid);
    }
    else if (flowCtrlStatus == NSM_DIAG_FLOW_CTRL_EXECUTION_FINISHED)
    {
        postStateUpdate(preBootDiagState::sessionEnded, payload.dump());
        lg2::info("PreBootDiag: session complete EID={EID}", "EID", eid);
    }
    else
    {
        if (!nsmDevice ||
            nsmDevice->shouldLog("PreBootDiag:unknownFlowCtrl", bool(true)))
        {
            lg2::warning("PreBootDiag: unknown flowCtrlStatus={ST} EID={EID}",
                         "ST", flowCtrlStatus, "EID", eid);
        }
    }
    return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmDiagSetFlowControlEvent(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "PreBootDiag: SetFlowControl PDI matches no NsmDevice UUID={UUID} NAME={NAME}",
            "UUID", uuid, "NAME", name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmDiagSetFlowControlEvent>(name, type);
    nsmDevice->addDeviceEvent(event, NSM_TYPE_DIAGNOSTIC,
                              NSM_DIAG_SET_FLOW_CONTROL_EVENT);
    lg2::info(
        "PreBootDiag: Registered SetFlowControl event UUID={UUID} NAME={NAME}",
        "UUID", uuid, "NAME", name);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmDiagSetFlowControlEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_DiagSetFlowControl");

} // namespace nsm
