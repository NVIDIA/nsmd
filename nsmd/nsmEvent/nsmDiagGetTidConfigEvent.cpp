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

#include "nsmDiagGetTidConfigEvent.hpp"

#include "diagnostics.h"

#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmPreBootDiagStateClient.hpp"
#include "nsmProcessor/nsmPreBootDiag.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmDiagGetTidConfigEvent::NsmDiagGetTidConfigEvent(const std::string& name,
                                                   const std::string& type) :
    NsmEvent(name, type)
{}

int NsmDiagGetTidConfigEvent::handle(eid_t eid, NsmType /*type*/,
                                     NsmEventId /*eventId*/,
                                     const nsm_msg* event, size_t eventLen)
{
    uint8_t eventClass{};
    uint16_t eventState{};
    uint8_t tid{};

    auto nsmDevice =
        mctp::MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);

    auto rc = decode_nsm_diag_get_tid_config_event(event, eventLen, &eventClass,
                                                   &eventState, &tid);
    if (rc != NSM_SW_SUCCESS)
    {
        if (!nsmDevice || nsmDevice->shouldLog("PreBootDiag:decodeGetTidCfg",
                                               nsm_sw_codes(rc)))
        {
            lg2::error(
                "PreBootDiag: decode getDiagTidConfig failed rc={RC} EID={EID}",
                "RC", rc, "EID", eid);
        }
        return rc;
    }

    if (!nsmDevice)
    {
        lg2::warning("PreBootDiag: getDiagTidConfig no NsmDevice for EID={EID}",
                     "EID", eid);
        return NSM_SW_ERROR;
    }

    NsmPreBootDiag::getOrCreate(nsmDevice);

    nlohmann::json payload;
    payload["Tid"] = static_cast<int>(tid);
    payload["Eid"] = static_cast<int>(eid);
    postStateUpdate(preBootDiagState::tidConfigRequested, payload.dump());
    lg2::info("PreBootDiag: TIDConfigRequested TID=0x{TID} EID={EID}", "TID",
              lg2::hex, tid, "EID", eid);
    return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmDiagGetTidConfigEvent(SensorManager& manager,
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
            "PreBootDiag: GetTidConfig PDI matches no NsmDevice UUID={UUID} NAME={NAME}",
            "UUID", uuid, "NAME", name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmDiagGetTidConfigEvent>(name, type);
    nsmDevice->addDeviceEvent(event, NSM_TYPE_DIAGNOSTIC,
                              NSM_DIAG_GET_TID_CONFIG_EVENT);
    lg2::info(
        "PreBootDiag: Registered GetTidConfig event UUID={UUID} NAME={NAME}",
        "UUID", uuid, "NAME", name);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmDiagGetTidConfigEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_DiagGetTidConfig");

} // namespace nsm
