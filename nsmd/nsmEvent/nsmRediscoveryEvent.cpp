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
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmRediscoveryEvent::NsmRediscoveryEvent(const std::string& name,
                                         const std::string& type,
                                         const NsmEventInfo info) :
    NsmEvent(name, type), info(info)
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

        return rc;
    }

    if (info.logging)
    {
        // Launch async logging without blocking the handle method
        auto loggingTask = logEventAsync("NsmRediscoveryEvent", info.severity,
                                         eventData);
        loggingTask.detach(); // Fire-and-forget - runs independently
    }

    // Member to hold reference to MctpDiscovery and sensor manager
    auto nsmDevice =
        mctp::MctpDiscovery::getInstance().getNsmDeviceFromEid(eid);
    if (nsmDevice)
    {
        lg2::info(
            "Rediscovery event : The NSM device has been discovered for , eid={EID}",
            "EID", eid);
        if (nsmDevice->isDiscoveryPending())
        {
            lg2::info(
                "Rediscovery event : Dropping the rediscovery process for eid={EID}, as mctp rediscovery is already in progress",
                "EID", eid);
            return NSM_SW_SUCCESS;
        }
        isRediscoveryRequired = true;
        requester::Coroutine::assign(
            rediscoveryTaskHandler,
            [&, nsmDevice, eid]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await handleRediscovery(nsmDevice);
        });
    }
    else
    {
        lg2::error(
            "Rediscovery event : The NSM device has not been discovered for , eid={EID}",
            "EID", eid);
    }

    return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmRediscoveryEvent::handleRediscovery(std::shared_ptr<NsmDevice> nsmDevice)
{
    while (isRediscoveryRequired)
    {
        isRediscoveryRequired = false;
        if (nsmDevice->gpuDriverSensor)
        {
            co_await nsmDevice->gpuDriverSensor->update(nsmDevice);
        }
        else
        {
            co_await nsmDevice->updateNsmDevice();
            co_await nsmDevice->refreshCapabilitySensor();
        }
    }
    co_return NSM_SW_SUCCESS;
}

static requester::Coroutine
    createNsmRediscoveryEvent([[maybe_unused]] SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    NsmEventInfo info{};

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    if (allCurrentIfaceProperties.count("UUID"))
    {
        info.uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    name = utils::makeDBusNameValid(name);

    auto type = interface.substr(interface.find_last_of('.') + 1);

    if (allCurrentIfaceProperties.count("OriginOfCondition"))
    {
        info.originOfCondition = std::get<std::string>(
            allCurrentIfaceProperties.at("OriginOfCondition"));
    }
    if (allCurrentIfaceProperties.count("MessageId"))
    {
        info.messageId =
            std::get<std::string>(allCurrentIfaceProperties.at("MessageId"));
    }
    if (allCurrentIfaceProperties.count("LoggingNamespace"))
    {
        info.loggingNamespace = std::get<std::string>(
            allCurrentIfaceProperties.at("LoggingNamespace"));
    }
    info.loggingNamespace = utils::makeDBusNameValid(info.loggingNamespace);

    if (allCurrentIfaceProperties.count("Resolution"))
    {
        info.resolution =
            std::get<std::string>(allCurrentIfaceProperties.at("Resolution"));
    }
    if (allCurrentIfaceProperties.count("MessageArgs"))
    {
        info.messageArgs = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("MessageArgs"));
    }
    info.logging = utils::DBusHandler().tryGetDbusProperty<bool>(
        objPath.c_str(), "Logging", interface.c_str(), true);

    std::string severityStr{};
    if (allCurrentIfaceProperties.count("Severity"))
    {
        severityStr =
            std::get<std::string>(allCurrentIfaceProperties.at("Severity"));
    }

    auto severityEnum = sdbusplus::common::xyz::openbmc_project::logging::
        Entry::convertStringToLevel("xyz.openbmc_project.Logging.Entry.Level." +
                                    severityStr);

    info.severity = severityEnum.value_or(Level::Critical);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(info.uuid);

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

    nsmDevice->addDeviceEvent(event, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                              NSM_REDISCOVERY_EVENT);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmRediscoveryEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_Rediscovery");

} // namespace nsm
