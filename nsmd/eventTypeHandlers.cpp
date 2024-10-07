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

#include "eventTypeHandlers.hpp"

#include "device-capability-discovery.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include "deviceManager.hpp"
#include "nsmDevice.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <memory>

namespace nsm
{

EventType0Handler::EventType0Handler()
{
    handlers.emplace(NSM_REDISCOVERY_EVENT,
                     [&](eid_t eid, NsmType /*type*/, NsmEventId /*eventId*/,
                         const nsm_msg* event, size_t eventLen) {
        return rediscovery(eid, event, eventLen);
    });
};

void EventType0Handler::rediscovery(uint8_t eid, const nsm_msg* event,
                                    size_t eventLen)
{
    // Member to hold reference to DeviceManager and sensor manager
    DeviceManager& deviceManager = DeviceManager::getInstance();
    SensorManager& sensorManager = SensorManager::getInstance();
    // update sensors for capabilities refresh
    // Get UUID from EID
    auto uuidOptional = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuidOptional)
    {
        uuid_t uuid = *uuidOptional;
        // findNSMDevice instance for that eid
        lg2::info("rediscovery event : UUID found: {UUID}", "UUID", uuid);
        auto nsmDevice = sensorManager.getNsmDevice(uuid);
        if (nsmDevice)
        {
            lg2::info(
                "Rediscovery event : The NSM device has been discovered for , uuid={UUID}",
                "UUID", uuid);
            deviceManager.updateNsmDevice(nsmDevice, eid).detach();
        }
        else
        {
            lg2::error(
                "Rediscovery event : The NSM device has not been discovered for , uuid={UUID}",
                "UUID", uuid);
        }
    }
    else
    {
        lg2::error("Rediscovery event : No UUID found for EID {EID}", "EID",
                   eid);
    }

    std::string messageId = "Rediscovery";

    auto createLog = [&messageId](std::map<std::string, std::string>& addData,
                                  Level& level) {
        static constexpr auto logObjPath = "/xyz/openbmc_project/logging";
        static constexpr auto logInterface =
            "xyz.openbmc_project.Logging.Create";
        auto& bus = utils::DBusHandler::getBus();

        try
        {
            auto service = utils::DBusHandler().getService(logObjPath,
                                                           logInterface);
            auto severity = sdbusplus::xyz::openbmc_project::Logging::server::
                convertForMessage(level);
            auto method = bus.new_method_call(service.c_str(), logObjPath,
                                              logInterface, "Create");
            method.append(messageId, severity, addData);
            bus.call_noreply(method);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "Failed to create D-Bus log entry for message registry, {ERROR}.",
                "ERROR", e);
        }
    };

    uint8_t eventClass = 0;
    uint16_t eventState = 0;
    auto rc = decode_nsm_rediscovery_event(event, eventLen, &eventClass,
                                           &eventState);
    if (rc != NSM_SUCCESS)
    {
        lg2::error("Failed to decode rediscovery event {RC}.", "RC", rc);
        return;
    }

    std::map<std::string, std::string> addData;
    addData["EID"] = std::to_string(eid);
    addData["CLASS"] = std::to_string(eventClass);
    addData["STATE"] = std::to_string(eventState);
    Level level = Level::Informational;
    createLog(addData, level);
}

EventType1Handler::EventType1Handler()
{
    enableDelegation(NSM_THRESHOLD_EVENT);
    enableDelegation(NSM_FABRIC_MANAGER_STATE_EVENT);
}

EventType3Handler::EventType3Handler()
{
    enableDelegation(NSM_XID_EVENT);
    enableDelegation(NSM_RESET_REQUIRED_EVENT);
}

} // namespace nsm
