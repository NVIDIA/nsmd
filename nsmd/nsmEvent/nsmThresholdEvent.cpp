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

#include "nsmThresholdEvent.hpp"

#include "network-ports.h"

#include "dBusAsyncUtils.hpp"
#include "globals.hpp"
#include "nsmEventFeatures.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string_view>

namespace nsm
{

NsmThresholdEvent::NsmThresholdEvent(const std::string& name,
                                     const std::string& type,
                                     const NsmEventInfo info) :
    NsmEvent(name, type), info(info)
{
    if (info.messageId != "ResourceEvent.1.0.ResourceErrorsDetected")
    {
        throw std::invalid_argument(
            "MessageId for NsmThresholdEvent must be ResourceEvent.1.0.ResourceErrorsDetected.");
    }
    if (info.messageArgs.empty())
    {
        throw std::invalid_argument(
            "MessageArgs for NsmThresholdEvent cannot be empty. MessageId=" +
            info.messageId);
    }
};

int NsmThresholdEvent::handle(eid_t eid, NsmType /*type*/,
                              NsmEventId /*eventId*/, const nsm_msg* event,
                              size_t eventLen)
{
    uint16_t eventState{};
    nsm_health_event_payload payload;

    auto rc = decode_nsm_health_event(event, eventLen, &eventState, &payload);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_health_event failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());

        return rc;
    }

    std::string errors;
    auto appendError = [&errors](std::string_view error) {
        if (!errors.empty())
        {
            errors += "; ";
        }
        errors += error;
    };
    using std::literals::string_view_literals::operator""sv;
    if (payload.port_rcv_errors_threshold)
    {
        appendError("port_rcv_errors_threshold"sv);
    }
    if (payload.port_xmit_discard_threshold)
    {
        appendError("port_xmit_discard_threshold"sv);
    }
    if (payload.symbol_ber_threshold)
    {
        appendError("symbol_ber_threshold"sv);
    }
    if (payload.port_rcv_remote_physical_errors_threshold)
    {
        appendError("port_rcv_remote_physical_errors_threshold"sv);
    }
    if (payload.port_rcv_switch_relay_errors_threshold)
    {
        appendError("port_rcv_switch_relay_errors_threshold"sv);
    }
    if (payload.effective_ber_threshold)
    {
        appendError("effective_ber_threshold"sv);
    }
    if (payload.estimated_effective_ber_threshold)
    {
        appendError("estimated_effective_ber_threshold"sv);
    }

    SensorManager& manager = SensorManager::getInstance();
    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(info.uuid);
    auto messageArg = info.messageArgs[0];
    if (nsmDevice)
    {
        if (manager.deviceToPortMap.find(nsmDevice) !=
            manager.deviceToPortMap.end())
        {
            auto deviceToPortMap = manager.deviceToPortMap[nsmDevice];
            if (deviceToPortMap.find(payload.portNumber) !=
                deviceToPortMap.end())
            {
                messageArg = messageArg + " " +
                             deviceToPortMap[payload.portNumber];
            }
        }
    }

    std::map<std::string, std::string> eventData{
        {"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
        {"REDFISH_MESSAGE_ARGS", messageArg + ", " + errors + ""},
        {"REDFISH_MESSAGE_ID", info.messageId},
        {"namespace", info.loggingNamespace},
        {"xyz.openbmc_project.Logging.Entry.Resolution", info.resolution}};

    std::string errorId = nsm::getEventErrorId(info, "Threshold");
    if (!errorId.empty())
    {
        eventData["xyz.openbmc_project.Logging.Entry.EventId"] = errorId;
    }

    auto loggingTask = logEventAsync("NsmThresholdEvent", info.severity,
                                     eventData);
    loggingTask.detach(); // Fire-and-forget - runs independently

    return NSM_SW_SUCCESS;
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createThresholdEventFeature([[maybe_unused]] SensorManager& manager,
                                const DeviceContext& dev,
                                const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("Threshold Event feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    NsmEventInfo info{};
    info.uuid = dev.staticUuid;

    auto name = utils::makeDBusNameValid(
        param<std::string>(params, "Name").value_or(""));
    const std::string type = "NSM_Event_Threshold";

    info.originOfCondition = (redfishChassisBasePath / dev.name).string();
    info.messageId = param<std::string>(params, "MessageId").value_or("");
    info.loggingNamespace = utils::makeDBusNameValid(
        param<std::string>(params, "LoggingNamespace").value_or(""));
    info.resolution = param<std::string>(params, "Resolution").value_or("");
    info.messageArgs = param<std::vector<std::string>>(params, "MessageArgs")
                           .value_or(std::vector<std::string>{});
    info.errorId = param<std::vector<std::string>>(params, "EventIds")
                       .value_or(std::vector<std::string>{});

    auto severityStr = param<std::string>(params, "Severity").value_or("");
    auto severityEnum = sdbusplus::common::xyz::openbmc_project::logging::
        Entry::convertStringToLevel("xyz.openbmc_project.Logging.Entry.Level." +
                                    severityStr);
    info.severity = severityEnum.value_or(Level::Critical);

    auto event = std::make_shared<NsmThresholdEvent>(name, type, info);

    lg2::info(
        "Created NSM Threshold Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", info.uuid, "NAME", name, "TYPE", type);

    dev.device->addDeviceEvent(event, NSM_TYPE_NETWORK_PORT,
                               NSM_THRESHOLD_EVENT);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
