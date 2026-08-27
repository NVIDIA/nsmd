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

#pragma once

#include "nsmEvent.hpp"

#include <memory>
#include <string>

namespace nsm
{

class NsmDevice;

struct NsmEventInfo
{
    /** Entity-manager static device-identification key (STATIC:d:d:s:s) used to
     *  resolve the owning NsmDevice. It is NOT a device identity and must never
     *  be reported to event consumers as a UUID. */
    std::string uuid;
    std::string originOfCondition;
    std::string messageId;
    Level severity;
    std::string loggingNamespace;
    std::string resolution;
    std::vector<std::string> errorId;
    std::string impactedComponent;
    std::vector<std::string> messageArgs;
    bool logging;
};

inline std::string getEventErrorId(const NsmEventInfo& info,
                                   const std::string& errorIdKey)
{
    if (info.errorId.size() % 2 == 0)
    {
        auto it = std::find(info.errorId.begin(), info.errorId.end(),
                            errorIdKey);
        if (it != info.errorId.end() && (it + 1) != info.errorId.end())
        {
            std::string errorIdStr = *(it + 1);
            return errorIdStr;
        }
        lg2::debug(
            "NSM_Event getEventErrorId : no errorId found for errorIdKey = {EID}, uuid = {UUID}",
            "EID", errorIdKey, "UUID", info.uuid);
        return "";
    }
    lg2::error(
        "NSM_Event getEventErrorId : Invalid ErrorId Map Size when getting errorIdKey = {EID}, uuid = {UUID}",
        "EID", errorIdKey, "UUID", info.uuid);
    return "";
}

#ifdef ENABLE_EVENT_GPU_UUID_LABEL
/**
 * @brief Builds the "GPU UUID <uuid>" event label.
 *
 * @param[in] deviceUuid NSM DEVICE_GUID of the device that raised the event,
 *                       i.e. NsmDevice::deviceUuid. This is the same RFC 4122
 *                       value nsmd publishes on the GPU chassis
 *                       xyz.openbmc_project.Common.UUID interface and that
 *                       Redfish reports as Chassis.UUID, so event consumers can
 *                       correlate MessageArgs with Links.OriginOfCondition.
 *
 * @note Do NOT pass NsmEventInfo::uuid here. That field carries the
 *       entity-manager static device-identification key (STATIC:d:d:s:s, see
 *       utils::parseStaticUuid) used to look the NsmDevice up; it is never a
 *       device identity.
 */
inline std::string getGpuUuidMessageArg(const std::string& deviceUuid)
{
    return "GPU UUID " + deviceUuid;
}
#endif // ENABLE_EVENT_GPU_UUID_LABEL

/**
 * @brief Replaces the leading "GPU_<n>" logical device name of a message
 *        argument with the device's real UUID label.
 *
 * @param[in] deviceUuid NSM DEVICE_GUID of the device that raised the event.
 *                       Empty when it is not resolved yet, in which case the
 *                       argument is returned unchanged.
 * @param[in] messageArg Message argument as declared in the EM configuration.
 */
inline std::string replaceGpuMessageArgDeviceName(
    [[maybe_unused]] const std::string& deviceUuid,
    const std::string& messageArg)
{
#ifdef ENABLE_EVENT_GPU_UUID_LABEL
    if (deviceUuid.empty() || !messageArg.starts_with("GPU_"))
    {
        return messageArg;
    }

    auto suffixPos = messageArg.find(' ');
    if (suffixPos == std::string::npos)
    {
        return getGpuUuidMessageArg(deviceUuid);
    }

    return getGpuUuidMessageArg(deviceUuid) + messageArg.substr(suffixPos);
#else
    return messageArg;
#endif
}

/**
 * @brief Joins the configured message arguments into the comma separated
 *        REDFISH_MESSAGE_ARGS payload.
 *
 * @param[in] info Event configuration.
 * @param[in] deviceUuid NSM DEVICE_GUID of the device that raised the event,
 *                       or empty when it is not resolved yet, in which case the
 *                       configured names are emitted unchanged. Deliberately
 *                       has no default: this argument decides the output, so a
 *                       caller must state whether it holds a device identity
 *                       and the unsubstituted form cannot be selected by
 *                       accident. (The events' constructor parameter is a
 *                       dependency rather than an output selector, and is
 *                       defaulted so a device-less event stays expressible.)
 */
inline std::string getUuidMessageArgs(const NsmEventInfo& info,
                                      const std::string& deviceUuid)
{
    std::string messageArgs{};
    if (!info.messageArgs.empty())
    {
        messageArgs += replaceGpuMessageArgDeviceName(deviceUuid,
                                                      info.messageArgs[0]);
    }

    for (size_t i{1}; i < info.messageArgs.size(); ++i)
    {
        messageArgs += ',';
        messageArgs += info.messageArgs[i];
    }

    return messageArgs;
}

/**
 * @brief Returns the NSM DEVICE_GUID of a device, or an empty string when the
 *        device has gone away or its identity has not been read yet. Resolved
 *        at event emission time, from handle().
 */
std::string getEventDeviceUuid(const std::weak_ptr<NsmDevice>& device);

}; // namespace nsm
