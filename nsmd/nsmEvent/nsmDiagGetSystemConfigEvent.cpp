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

#include "nsmDiagGetSystemConfigEvent.hpp"

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

NsmDiagGetSystemConfigEvent::NsmDiagGetSystemConfigEvent(
    const std::string& name, const std::string& type) : NsmEvent(name, type)
{}

int NsmDiagGetSystemConfigEvent::handle(eid_t eid, NsmType /*type*/,
                                        NsmEventId /*eventId*/,
                                        const nsm_msg* event, size_t eventLen)
{
    uint8_t eventClass{};
    uint16_t eventState{};
    uint8_t configType{};

    auto nsmDevice =
        mctp::MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);

    auto rc = decode_nsm_diag_get_system_config_event(
        event, eventLen, &eventClass, &eventState, &configType);
    if (rc != NSM_SW_SUCCESS)
    {
        if (!nsmDevice || nsmDevice->shouldLog("PreBootDiag:decodeGetSysCfg",
                                               nsm_sw_codes(rc)))
        {
            lg2::error(
                "PreBootDiag: decode getDiagSystemConfig failed rc={RC} EID={EID}",
                "RC", rc, "EID", eid);
        }
        return rc;
    }

    if (!nsmDevice)
    {
        lg2::warning(
            "PreBootDiag: getDiagSystemConfig no NsmDevice for EID={EID}",
            "EID", eid);
        return NSM_SW_ERROR;
    }

    // Ensure the per-device Config D-Bus endpoint exists so prebootdiag
    // has a target for its Config.Set callback.
    NsmPreBootDiag::getOrCreate(nsmDevice);

    // Cast to int to keep JSON serialization unambiguous: nlohmann's
    // handling of uint8_t has historically been fragile (sometimes treated
    // as a small integer, sometimes as a single-byte string). Forcing int
    // makes the wire form deterministic across nlohmann versions.
    nlohmann::json payload;
    payload["ConfigType"] = static_cast<int>(configType);
    payload["Eid"] = static_cast<int>(eid);
    postStateUpdate(preBootDiagState::systemConfigRequested, payload.dump());
    lg2::info("PreBootDiag: SystemConfigRequested configType={CT} EID={EID}",
              "CT", configType, "EID", eid);
    return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmDiagGetSystemConfigEvent(SensorManager& manager,
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
            "PreBootDiag: GetSystemConfig PDI matches no NsmDevice UUID={UUID} NAME={NAME}",
            "UUID", uuid, "NAME", name);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmDiagGetSystemConfigEvent>(name, type);
    nsmDevice->addDeviceEvent(event, NSM_TYPE_DIAGNOSTIC,
                              NSM_DIAG_GET_SYSTEM_CONFIG_EVENT);
    lg2::info(
        "PreBootDiag: Registered GetSystemConfig event UUID={UUID} NAME={NAME}",
        "UUID", uuid, "NAME", name);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmDiagGetSystemConfigEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_DiagGetSystemConfig");

} // namespace nsm
