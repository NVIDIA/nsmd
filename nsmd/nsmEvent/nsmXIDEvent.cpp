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
#include "sensorManager.hpp"

#include <fmt/args.h>

#include <boost/algorithm/string/replace.hpp>
#include <phosphor-logging/lg2.hpp>

namespace nsm
{

NsmXIDEvent::NsmXIDEvent(const std::string& name, const std::string& type,
                         const NsmEventInfo info) :
    NsmEvent(name, type),
    info(info)
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

        return NSM_SW_ERROR;
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

    std::map<std::string, std::string> eventData{
        {"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
        {"REDFISH_MESSAGE_ARGS", messageArgs},
        {"REDFISH_MESSAGE_ID", info.messageId},
        {"namespace", info.loggingNamespace},
        {"xyz.openbmc_project.Logging.Entry.Resolution", info.resolution}};

    logEvent("NsmXIDEvent", info.severity, eventData);

    return NSM_SW_SUCCESS;
}

static requester::Coroutine createNsmXIDEvent(SensorManager& manager,
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
            "The UUID of XID Event PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", info.uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmXIDEvent>(name, type, info);

    lg2::info("Created NSM XID Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", info.uuid, "NAME", name, "TYPE", type);

    nsmDevice->deviceEvents.push_back(event);
    nsmDevice->eventDispatcher.addEvent(NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                        NSM_XID_EVENT, event);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmXIDEvent, "xyz.openbmc_project.Configuration.NSM_Event_XID");

} // namespace nsm
