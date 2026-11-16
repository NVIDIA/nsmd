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

#include "nsmXIDEvent.hpp"

#include "platform-environmental.h"

#include "dBusAsyncUtils.hpp"
#include "globals.hpp"
#include "nsmEventFeatures.hpp"
#include "sensorManager.hpp"

#include <fmt/args.h>

#include <boost/algorithm/string/replace.hpp>
#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmXIDEvent::NsmXIDEvent(const std::string& name, const std::string& type,
                         const NsmEventInfo info,
                         std::weak_ptr<NsmDevice> device) :
    NsmEvent(name, type), info(info), device(device)
{}

int NsmXIDEvent::handle(eid_t eid, NsmType /*type*/, NsmEventId /*eventId*/,
                        const nsm_msg* event, size_t eventLen)
{
    lg2::info("Received XID Event from EID {EID}.", "EID", eid);

    uint8_t eventClass{};
    uint16_t eventState{};
    nsm_xid_event_payload payload{};
    char text[NSM_EVENT_DATA_MAX_LEN]{};
    size_t messageTextSize{};

    auto rc = decode_nsm_xid_event(event, eventLen, &eventClass, &eventState,
                                   &payload, text, &messageTextSize);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_xid_event failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());

        return rc;
    }

    std::string messageText(text, messageTextSize);
    boost::algorithm::replace_all(messageText, ",", ";");

    std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
        timePoint{std::chrono::nanoseconds(payload.timestamp)};

    std::time_t timestamp = std::chrono::system_clock::to_time_t(timePoint);
    constexpr size_t timeSize{100};
    char timeStr[timeSize];
    std::strftime(timeStr, timeSize, "%c", std::localtime(&timestamp));

    fmt::dynamic_format_arg_store<fmt::format_context> messageFmtArgs;
    messageFmtArgs.push_back(
        fmt::arg("SequenceNumber", payload.sequence_number));
    messageFmtArgs.push_back(fmt::arg("Flags", payload.flag));
    messageFmtArgs.push_back(fmt::arg("EventMessageReason", payload.reason));
    messageFmtArgs.push_back(fmt::arg("MessageTextString", messageText));
    messageFmtArgs.push_back(fmt::arg("Timestamp", timeStr));

    std::vector<std::string> formattedMessageArgs{};

    for (const auto& messageArg : info.messageArgs)
    {
        try
        {
            formattedMessageArgs.push_back(
                fmt::vformat(messageArg, messageFmtArgs));
        }
        catch (const std::exception& e)
        {
            formattedMessageArgs.push_back({});
            lg2::error(
                "XID Event : Error while formatting Message Arg {ARG}. Error={ERROR}, EID={SRC}, uuid={UUID}",
                "ARG", messageArg, "ERROR", e, "SRC", eid, "UUID", info.uuid);
        }
    }
    const auto deviceUuid = getEventDeviceUuid(device);

    if (!formattedMessageArgs.empty())
    {
        formattedMessageArgs[0] =
            replaceGpuMessageArgDeviceName(deviceUuid, formattedMessageArgs[0]);
    }

    std::string messageArgs{};

    if (!formattedMessageArgs.empty())
    {
        messageArgs += formattedMessageArgs[0];
    }

    for (size_t i{1}; i < formattedMessageArgs.size(); ++i)
    {
        messageArgs += ',';
        messageArgs += formattedMessageArgs[i];
    }

    eventData = {
        {"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
        {"REDFISH_MESSAGE_ARGS", messageArgs},
        {"REDFISH_MESSAGE_ID", info.messageId},
        {"namespace", info.loggingNamespace},
        {"xyz.openbmc_project.Logging.Entry.Resolution", info.resolution}};

    std::string xid = "XID " + std::to_string(payload.reason);
    std::string errorId = nsm::getEventErrorId(info, xid);
    if (!errorId.empty())
    {
        eventData["xyz.openbmc_project.Logging.Entry.EventId"] = errorId;
    }
    if (!info.impactedComponent.empty())
    {
        eventData["DEVICE_NAME"] =
            replaceGpuMessageArgDeviceName(deviceUuid, info.impactedComponent);
    }

    // Launch async logging without blocking the handle method
    auto loggingTask = logEventAsync("NsmXIDEvent", info.severity, eventData);
    loggingTask.detach(); // Fire-and-forget - runs independently

    return NSM_SW_SUCCESS;
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createXIDEventFeature([[maybe_unused]] SensorManager& manager,
                          const DeviceContext& dev,
                          const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("XID Event feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    NsmEventInfo info{};
    info.uuid = dev.staticUuid;

    auto name = utils::makeDBusNameValid(
        param<std::string>(params, "Name").value_or(""));
    const std::string type = "NSM_Event_XID";

    info.originOfCondition = (redfishChassisBasePath / dev.name).string();
    info.messageId = param<std::string>(params, "MessageId").value_or("");
    info.loggingNamespace = utils::makeDBusNameValid(
        param<std::string>(params, "LoggingNamespace").value_or(""));
    info.resolution = param<std::string>(params, "Resolution").value_or("");
    // MessageArgs fmt fields are filled per event
    info.messageArgs = param<std::vector<std::string>>(params, "MessageArgs")
                           .value_or(std::vector<std::string>{});
    info.errorId = param<std::vector<std::string>>(params, "EventIds")
                       .value_or(std::vector<std::string>{});
    if (auto impactedComponent = param<std::string>(params,
                                                    "ImpactedComponent"))
    {
        info.impactedComponent = *impactedComponent;
    }

    auto severityStr = param<std::string>(params, "Severity").value_or("");
    auto severityEnum = sdbusplus::common::xyz::openbmc_project::logging::
        Entry::convertStringToLevel("xyz.openbmc_project.Logging.Entry.Level." +
                                    severityStr);
    info.severity = severityEnum.value_or(Level::Critical);

    auto event = std::make_shared<NsmXIDEvent>(name, type, info, dev.device);

    lg2::info("Created NSM XID Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", info.uuid, "NAME", name, "TYPE", type);

    dev.device->addDeviceEvent(event, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                               NSM_XID_EVENT);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
