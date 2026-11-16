/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "common/coroutine.hpp"
#include "common/types.hpp"

#include <concepts>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nsm
{
class SensorManager;
class NsmDevice;
} // namespace nsm

namespace nsm::features
{

/** @brief Identity of one device, built from its EM NSM_Telemetry record and
 * parent entity */
struct DeviceContext
{
    std::string name;           // EM "Name"
    std::string kind;           // EM "Kind"
    uint8_t deviceType = 0;     // NSM_DEV_ID_*
    uint8_t deviceRole = 0;     // NSM_*_DEV_ROLE_*
    uint8_t instance = 0;       // EM "InstanceNumber"
    std::optional<eid_t> eid;   // EM "EID"
    uuid_t staticUuid;          // "STATIC:d:d:s:s", built by nsmd
    std::string configPath;     // EM NSM_Telemetry object path
    std::string chassisPath;    // chassisInventoryBasePath / name
    std::string parentChassis;  // NSM_Telemetry "ParentChassis", relative to
                                // inventorySystemBasePath (e.g.
                                // "board/HGX_ProcessorModule_0"), empty if
                                // absent
    std::string platformPrefix; // NSM_Telemetry "PlatformPrefix" (e.g.
                                // "HGX_"), empty if absent
    std::shared_ptr<NsmDevice> device;
    // NSM_Telemetry record properties as EM published them, except Name,
    // Type and the built-in variable names
    std::map<std::string, dbus::Value> variables;

    /** @brief Next index of a group of repeated sub-objects (e.g. "Assembly")
     *
     *  Counted per device over the creators that have run, so enabled
     *  features of a group are numbered 0, 1, ... in list order.
     */
    unsigned nextIndex(std::string_view group) const
    {
        return indexCounters[std::string(group)]++;
    }

    mutable std::map<std::string, unsigned> indexCounters;
};

/** @brief Device name without the platform prefix (NSM_Telemetry
 *  "PlatformPrefix"), e.g. "HGX_GPU_0" -> "GPU_0"; a name without the
 *  prefix is returned unchanged
 */
inline std::string deviceBaseName(const DeviceContext& dev)
{
    const std::string& prefix = dev.platformPrefix;
    if (!prefix.empty() && dev.name.starts_with(prefix))
    {
        return dev.name.substr(prefix.size());
    }
    return dev.name;
}

using CreatorFn = requester::Coroutine (*)(SensorManager& manager,
                                           const DeviceContext& dev,
                                           const dbus::PropertyMap& params);

/** @brief One list param: a value, or a device variable converted to the
 *  type of the value (see var())
 */
struct ParamValue
{
    dbus::Value value;
    std::string var; // "{Key}" for a var() / optVar() param, empty otherwise
    bool optional = false; // optVar(): left out instead of failing

    ParamValue() = default;

    template <typename T>
        requires(!std::same_as<std::decay_t<T>, ParamValue> &&
                 std::constructible_from<dbus::Value, T>)
    ParamValue(T&& v) : value(std::forward<T>(v))
    {}
};

using ParamMap = std::map<std::string, ParamValue>;

/** @brief Param filled from a device variable, e.g.
 *  {"Count", var<uint64_t>("{PortCount}")}; the feature fails if the device
 *  has no such variable or its value does not convert to T
 */
template <typename T>
ParamValue var(std::string token)
{
    ParamValue p{T{}};
    p.var = std::move(token);
    return p;
}

/** @brief Like var(), but a missing variable or a value that does not
 *  convert leaves the param out (logged as an error) and the feature runs
 */
template <typename T>
ParamValue optVar(std::string token)
{
    ParamValue p = var<T>(std::move(token));
    p.optional = true;
    return p;
}

struct Feature
{
    std::string_view id; // unique within the device list
    bool enabled = true;
    CreatorFn create = nullptr;
    ParamMap params{};            // defaults, always passed to the creator
    dbus::PropertyMap optional{}; // declared but unset: the value only gives
                                  // the type and is never passed to the creator
};

/** @brief Ordered feature list; order is creation order */
using FeatureList = std::vector<Feature>;

/** @brief Literal helpers so list values get EM's types
 *  (a bare 0 would not be stored as uint64_t)
 */
constexpr uint64_t u64(uint64_t value)
{
    return value;
}

constexpr int64_t i64(int64_t value)
{
    return value;
}

/** @brief Value of a creator param, or nullopt if the list does not set it */
template <typename T>
std::optional<T> param(const dbus::PropertyMap& params, const char* key)
{
    auto it = params.find(key);
    if (it == params.end())
    {
        return std::nullopt;
    }
    return std::get<T>(it->second);
}

/** @brief Array param, or an empty vector if the list does not set it */
template <typename T>
std::vector<T> arrayParam(const dbus::PropertyMap& params, const char* key)
{
    return param<std::vector<T>>(params, key).value_or(std::vector<T>{});
}

/** @brief One decoded EM override, independent of its D-Bus encoding */
struct Override
{
    enum class Action
    {
        Enable,
        Disable,
        Set
    };

    Action action;
    std::string feature;  // "Temp.TEMP_0"
    std::string property; // Set only
    dbus::Value value;    // Set only
};

} // namespace nsm::features
