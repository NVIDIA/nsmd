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
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string_view>

namespace nsm
{

NsmThresholdEvent::NsmThresholdEvent(const std::string& name,
                                     const std::string& type,
                                     const NsmEventInfo info) :
    NsmEvent(name, type),
    info(info)
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

    logEvent(
        "NsmThresholdEvent", info.severity,
        {{"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
         {"REDFISH_MESSAGE_ARGS", info.messageArgs[0] + ", " + errors + ""},
         {"REDFISH_MESSAGE_ID", info.messageId},
         {"namespace", info.loggingNamespace},
         {"xyz.openbmc_project.Logging.Entry.Resolution", info.resolution}});

    return NSM_SW_SUCCESS;
}

requester::Coroutine createNsmThresholdEvent(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath)
{
    NsmEventInfo info{};

    info.uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto device = manager.getNsmDevice(info.uuid);

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

    auto event = std::make_shared<NsmThresholdEvent>(name, type, info);

    lg2::info(
        "Created NSM Threshold Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", info.uuid, "NAME", name, "TYPE", type);

    device->deviceEvents.emplace_back(event);
    device->eventDispatcher.addEvent(NSM_TYPE_NETWORK_PORT, NSM_THRESHOLD_EVENT,
                                     event);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmThresholdEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_Threshold");

} // namespace nsm
