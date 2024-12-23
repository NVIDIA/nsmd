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

#include "nsmRediscoveryEvent.hpp"

#include "device-capability-discovery.h"

#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmRediscoveryEvent::NsmRediscoveryEvent(const std::string& name,
                                         const std::string& type,
                                         const NsmEventInfo info) :
    NsmEvent(name, type),
    info(info)
{
    if (!info.messageArgs.empty())
    {
        messageArgs += info.messageArgs[0];
    }

    for (size_t i{1}; i < info.messageArgs.size(); ++i)
    {
        messageArgs += ',';
        messageArgs += info.messageArgs[i];
    }

    eventData = {
        {"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
        {"REDFISH_MESSAGE_ARGS", messageArgs},
        {"REDFISH_MESSAGE_ID", info.messageId},
        {"namespace", info.loggingNamespace},
        {"xyz.openbmc_project.Logging.Entry.Resolution", info.resolution}};
};

int NsmRediscoveryEvent::handle(eid_t eid, NsmType /*type*/,
                                NsmEventId /*eventId*/, const nsm_msg* event,
                                size_t eventLen)
{
    lg2::info("Received Rediscovery Event from EID {EID}.", "EID", eid);

    uint8_t eventClass{};
    uint16_t eventState{};

    auto rc = decode_nsm_rediscovery_event(event, eventLen, &eventClass,
                                           &eventState);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_rediscovery_event failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());

        return NSM_SW_ERROR;
    }

    logEvent("NsmRediscoveryEvent", info.severity, eventData);
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

    return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmRediscoveryEvent(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    NsmEventInfo info{};

    info.uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    info.originOfCondition = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "OriginOfCondition", interface.c_str());

    info.messageId = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "MessageId", interface.c_str());

    info.loggingNamespace = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "LoggingNamespace", interface.c_str());
    info.loggingNamespace = utils::makeDBusNameValid(info.loggingNamespace);

    info.resolution = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Resolution", interface.c_str());

    info.messageArgs =
        co_await utils::coGetDbusProperty<std::vector<std::string>>(
            objPath.c_str(), "MessageArgs", interface.c_str());

    auto severityStr = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Severity", interface.c_str());

    auto severityEnum = sdbusplus::common::xyz::openbmc_project::logging::
        Entry::convertStringToLevel("xyz.openbmc_project.Logging.Entry.Level." +
                                    severityStr);

    info.severity = severityEnum.value_or(Level::Critical);

    auto nsmDevice = manager.getNsmDevice(info.uuid);

    if (!nsmDevice)
    {
        lg2::error(
            "The UUID of Rediscovery Event PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", info.uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmRediscoveryEvent>(name, type, info);

    lg2::info(
        "Created NSM Rediscovery Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", info.uuid, "NAME", name, "TYPE", type);

    nsmDevice->deviceEvents.push_back(event);
    nsmDevice->eventDispatcher.addEvent(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                        NSM_REDISCOVERY_EVENT, event);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmRediscoveryEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_Rediscovery");

} // namespace nsm
