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
#include "globals.hpp"
#include "nsmEventFeatures.hpp"
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

} // namespace nsm

namespace nsm::features
{

requester::Coroutine createRuntimeISTCompleteEventFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("Runtime IST Complete Event feature of {NAME}: no NsmDevice",
                   "NAME", dev.name);
        co_return NSM_ERROR;
    }

    NsmEventInfo info{};
    info.uuid = dev.staticUuid;

    auto name = utils::makeDBusNameValid(
        param<std::string>(params, "Name").value_or(""));
    const std::string type = "NSM_Event_Runtime_IST_Complete";

    info.originOfCondition = (redfishChassisBasePath / dev.name).string();
    info.messageId = param<std::string>(params, "MessageId").value_or("");
    info.loggingNamespace = utils::makeDBusNameValid(
        param<std::string>(params, "LoggingNamespace").value_or(""));
    // Failure rows only; the Pass row's resolution is the spec literal "None"
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
    // Severity is derived from the event payload, not configured
    info.logging = param<bool>(params, "Logging").value_or(true);

    auto event = std::make_shared<NsmRuntimeISTCompleteEvent>(name, type, info);

    lg2::info(
        "Created NSM Runtime IST Complete Event : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", info.uuid, "NAME", name, "TYPE", type);

    dev.device->addDeviceEvent(event, NSM_TYPE_DIAGNOSTIC,
                               NSM_RUNTIME_IST_COMPLETE_EVENT);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
