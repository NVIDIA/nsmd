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
#include "globals.hpp"
#include "nsmEventFeatures.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmRediscoveryEvent::NsmRediscoveryEvent(const std::string& name,
                                         const std::string& type,
                                         const NsmEventInfo info,
                                         std::weak_ptr<NsmDevice> device) :
    NsmEvent(name, type), info(info), device(device)
{
    // The GPU UUID label is resolved at emission time in handle(); the device
    // identity is read from the hardware asynchronously and is generally not
    // available yet while the event is being constructed from EM config.
    messageArgs = getUuidMessageArgs(info, {});

    eventData = {
        {"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
        {"REDFISH_MESSAGE_ARGS", messageArgs},
        {"REDFISH_MESSAGE_ID", info.messageId},
        {"namespace", info.loggingNamespace},
        {"xyz.openbmc_project.Logging.Entry.Resolution", info.resolution}};

    std::string errorId = nsm::getEventErrorId(info, "Rediscovery");
    if (!errorId.empty())
    {
        eventData["xyz.openbmc_project.Logging.Entry.EventId"] = errorId;
    }
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
        // Resolve the label now that the device identity is known. Written
        // back into eventData so the emitted payload is inspectable.
        eventData["REDFISH_MESSAGE_ARGS"] =
            getUuidMessageArgs(info, getEventDeviceUuid(device));

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

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createRediscoveryEventFeature([[maybe_unused]] SensorManager& manager,
                                  const DeviceContext& dev,
                                  const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("Rediscovery Event feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    NsmEventInfo info{};
    info.uuid = dev.staticUuid;

    auto name = utils::makeDBusNameValid(
        param<std::string>(params, "Name").value_or(""));
    const std::string type = "NSM_Event_Rediscovery";

    info.originOfCondition = (redfishChassisBasePath / dev.name).string();
    info.messageId = param<std::string>(params, "MessageId").value_or("");
    info.loggingNamespace = utils::makeDBusNameValid(
        param<std::string>(params, "LoggingNamespace").value_or(""));
    info.resolution = param<std::string>(params, "Resolution").value_or("");
    info.messageArgs = param<std::vector<std::string>>(params, "MessageArgs")
                           .value_or(std::vector<std::string>{});
    info.errorId = param<std::vector<std::string>>(params, "EventIds")
                       .value_or(std::vector<std::string>{});
    info.logging = param<bool>(params, "Logging").value_or(true);

    auto severityStr = param<std::string>(params, "Severity").value_or("");
    auto severityEnum = sdbusplus::common::xyz::openbmc_project::logging::
        Entry::convertStringToLevel("xyz.openbmc_project.Logging.Entry.Level." +
                                    severityStr);
    info.severity = severityEnum.value_or(Level::Critical);

    auto event = std::make_shared<NsmRediscoveryEvent>(name, type, info,
                                                       dev.device);

    lg2::info(
        "Created NSM Rediscovery Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", info.uuid, "NAME", name, "TYPE", type);

    dev.device->addDeviceEvent(event, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                               NSM_REDISCOVERY_EVENT);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
