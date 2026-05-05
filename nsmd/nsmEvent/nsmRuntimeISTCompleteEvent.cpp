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

#include "nsmRuntimeISTCompleteEvent.hpp"

#include "diagnostics.h"

#include "dBusAsyncUtils.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <fmt/args.h>
#include <fmt/format.h>

#include <phosphor-logging/lg2.hpp>

#include <chrono>
#include <cstring>
#include <ctime>

namespace nsm
{

NsmRuntimeISTCompleteEvent::NsmRuntimeISTCompleteEvent(
    const std::string& name, const std::string& type,
    const NsmEventInfo& info) : NsmEvent(name, type), info(info)
{}

std::pair<Level, std::string>
    NsmRuntimeISTCompleteEvent::deriveRistSeverity(uint8_t result,
                                                   uint64_t status_code) const
{
    const uint8_t statusClass = (status_code >> NSM_RIST_STATUS_CLASS_SHIFT) &
                                NSM_RIST_STATUS_CLASS_MASK;
    if (result == NSM_RIST_RESULT_PASS &&
        statusClass == NSM_RIST_STATUS_CLASS_OK)
    {
        // Spec literal for the Pass row: no actionable resolution applies.
        // Phosphor's Level enum is syslog-based and has no OK/Ok variant;
        // map the spec's "Ok" to Informational (repo convention for benign
        // informational entries).
        return {Level::Informational, "None"};
    }
    if (result == NSM_RIST_RESULT_FAIL &&
        statusClass == NSM_RIST_STATUS_CLASS_WARNING)
    {
        return {Level::Warning, info.resolution};
    }
    if (result == NSM_RIST_RESULT_FAIL &&
        statusClass == NSM_RIST_STATUS_CLASS_CRITICAL)
    {
        return {Level::Critical, info.resolution};
    }
    lg2::warning(
        "Runtime IST Complete: undefined (Result, StatusClass) combination, Result={R}, Class={C}",
        "R", result, "C", statusClass);
    return {Level::Critical, info.resolution};
}

int NsmRuntimeISTCompleteEvent::handle(eid_t eid, NsmType /*type*/,
                                       NsmEventId /*eventId*/,
                                       const nsm_msg* event, size_t eventLen)
{
    lg2::info("Received Runtime IST Complete Event from EID {EID}.", "EID",
              eid);

    uint8_t eventClass{};
    uint16_t eventState{};
    nsm_runtime_ist_complete_event_payload payload{};

    auto rc = decode_nsm_runtime_ist_complete_event(
        event, eventLen, &eventClass, &eventState, &payload);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "decode_nsm_runtime_ist_complete_event failed. rc={RC}, EID={SRC}, Name={NAME}",
            "RC", rc, "SRC", eid, "NAME", getName());
        return rc;
    }

    // Strings are spec'd as null-terminated; bound by strnlen so a
    // non-conforming sender cannot cause an out-of-bounds read here.
    std::string gpuId(payload.gpu_identifier,
                      strnlen(payload.gpu_identifier, NSM_RIST_GPU_UUID_LEN));
    std::string appVersion(
        payload.app_version,
        strnlen(payload.app_version, NSM_RIST_APP_VERSION_LEN));

    std::string resultStr;
    if (payload.result == NSM_RIST_RESULT_PASS)
    {
        resultStr = "Pass";
    }
    else if (payload.result == NSM_RIST_RESULT_FAIL)
    {
        resultStr = "Fail";
    }
    else
    {
        lg2::warning(
            "Runtime IST Complete: unknown result byte {RAW}, EID={EID}, Name={NAME}",
            "RAW", payload.result, "EID", eid, "NAME", getName());
        resultStr = fmt::format("Unknown(0x{:02X})", payload.result);
    }
    const std::string timestampStr = utils::formatTimestamp(payload.timestamp);
    const double maxTempC = utils::nvS24_8ToCelsius(payload.max_temperature);
    const double avgTempC = utils::nvS24_8ToCelsius(payload.avg_temperature);

    fmt::dynamic_format_arg_store<fmt::format_context> messageFmtArgs;
    messageFmtArgs.push_back(fmt::arg("GpuId", gpuId));
    messageFmtArgs.push_back(fmt::arg("Timestamp", timestampStr));
    messageFmtArgs.push_back(fmt::arg("AppVersion", appVersion));
    messageFmtArgs.push_back(fmt::arg("Result", resultStr));
    // Brace-init forces a value copy before fmt::make_format_args captures
    // by reference, so we never bind a reference to the packed
    // payload.status_code member (rejected by ARM g++ /
    // -Werror=address-of-packed-member).
    messageFmtArgs.push_back(fmt::arg(
        "StatusCode", fmt::format("0x{:016X}", uint64_t{payload.status_code})));
    messageFmtArgs.push_back(fmt::arg("MaxTemperature", maxTempC));
    messageFmtArgs.push_back(fmt::arg("AvgTemperature", avgTempC));

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
                "Runtime IST Complete Event: Error while formatting Message Arg {ARG}. Error={ERROR}, EID={SRC}, uuid={UUID}",
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

    if (info.logging)
    {
        // Severity is derived from the wire payload per the RIST spec table
        // (Result + StatusCode[63:62]); Resolution is "None" for the Pass
        // row (spec literal) and the EM JSON Resolution config for failure
        // rows. Undefined combinations fall back to {Critical, info.resolution}
        // and are logged from inside deriveRistSeverity().
        auto [severity, resolution] = deriveRistSeverity(payload.result,
                                                         payload.status_code);

        std::map<std::string, std::string> eventData{
            {"REDFISH_ORIGIN_OF_CONDITION", info.originOfCondition},
            {"REDFISH_MESSAGE_ARGS", messageArgs},
            {"REDFISH_MESSAGE_ID", info.messageId},
            {"namespace", info.loggingNamespace},
            {"xyz.openbmc_project.Logging.Entry.Resolution", resolution}};

        // Optional EventId lookup. Try the result-specific key first
        // ("Runtime IST Complete Pass" / "...Fail" / "...Unknown(0xNN)") so
        // configs that want distinct correlators per outcome keep working,
        // then fall back to the bare "Runtime IST Complete" key so a single
        // pair in EventIds can serve all outcomes uniformly.
        std::string errorId =
            nsm::getEventErrorId(info, "Runtime IST Complete " + resultStr);
        if (errorId.empty())
        {
            errorId = nsm::getEventErrorId(info, "Runtime IST Complete");
        }
        if (!errorId.empty())
        {
            eventData["xyz.openbmc_project.Logging.Entry.EventId"] = errorId;
        }
        lg2::info("ErrorId: {ERROR_ID}", "ERROR_ID", errorId);
        if (!info.impactedComponent.empty())
        {
            eventData["DEVICE_NAME"] = info.impactedComponent;
        }

        // Fire-and-forget: detach the async logging task so the event handler
        // returns immediately without waiting for the phosphor log entry to
        // be created. The task owns its own lifetime after detach().
        auto loggingTask = logEventAsync("NsmRuntimeISTCompleteEvent", severity,
                                         eventData);
        loggingTask.detach();
    }

    return NSM_SW_SUCCESS;
}

requester::Coroutine
    createNsmRuntimeISTCompleteEvent(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    NsmEventInfo info{};

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    // info.uuid is the MCTP UUID used for NsmDevice lookup -- distinct from
    // the wire payload's gpu_identifier (Device UUID).
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

    // The EM JSON "Resolution" property carries the actionable text used
    // for the failure rows of the RIST spec table (Warning / Critical).
    // The Pass row's resolution is the spec literal "None" hardcoded in
    // deriveRistSeverity() and does not consult info.resolution.
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

    // NOTE: The EM JSON "Severity" property is intentionally not read here.
    // For NSM_Event_Runtime_IST_Complete the per-log Severity is derived
    // from the wire payload (see deriveRistSeverity in handle()), and
    // info.severity is left default-initialised since no code path consults
    // it for this event.
    if (allCurrentIfaceProperties.count("EventIds"))
    {
        info.errorId = std::get<std::vector<std::string>>(
            allCurrentIfaceProperties.at("EventIds"));
    }
    if (allCurrentIfaceProperties.count("ImpactedComponent"))
    {
        info.impactedComponent = std::get<std::string>(
            allCurrentIfaceProperties.at("ImpactedComponent"));
    }

    // Master on/off switch for phosphor logging. Defaults to true if the
    // property is absent so existing exemplars without a Logging key keep
    // logging by default. Pass and Fail are gated identically by this flag.
    info.logging = utils::DBusHandler().tryGetDbusProperty<bool>(
        objPath.c_str(), "Logging", interface.c_str(), true);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(info.uuid);
    if (!nsmDevice)
    {
        lg2::error(
            "The UUID of Runtime IST Complete Event PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", info.uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto event = std::make_shared<NsmRuntimeISTCompleteEvent>(name, type, info);

    lg2::info(
        "Created NSM Runtime IST Complete Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", info.uuid, "NAME", name, "TYPE", type);

    nsmDevice->addDeviceEvent(event, NSM_TYPE_DIAGNOSTIC,
                              NSM_RUNTIME_IST_COMPLETE_EVENT);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmRuntimeISTCompleteEvent,
    "xyz.openbmc_project.Configuration.NSM_Event_Runtime_IST_Complete");

} // namespace nsm
