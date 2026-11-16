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

#include "overrideApply.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cstdlib>
#include <format>
#include <type_traits>
#include <utility>

namespace nsm::features
{

namespace
{

template <typename T>
constexpr bool isInteger = std::is_integral_v<T> && !std::is_same_v<T, bool>;

template <typename T>
struct IntegerVector : std::false_type
{};

template <typename T>
    requires isInteger<T>
struct IntegerVector<std::vector<T>> : std::true_type
{
    using element = T;
};

Feature* findFeature(FeatureList& features, const std::string& id)
{
    auto it = std::ranges::find_if(
        features, [&id](const Feature& f) { return f.id == id; });
    return it == features.end() ? nullptr : &*it;
}

const char* actionName(Override::Action action)
{
    switch (action)
    {
        case Override::Action::Enable:
            return "Enable";
        case Override::Action::Disable:
            return "Disable";
        case Override::Action::Set:
            return "Set";
    }
    return "Unknown";
}

void reject(ApplyStats& stats, const std::string& deviceName, const Override& o,
            const std::string& reason)
{
    ++stats.rejected;
    lg2::error(
        "Feature override rejected: {REASON}. Name={NAME} Action={ACTION} Feature={FEATURE} Property={PROPERTY} Value={VALUE}",
        "REASON", reason, "NAME", deviceName, "ACTION", actionName(o.action),
        "FEATURE", o.feature, "PROPERTY", o.property, "VALUE",
        o.action == Override::Action::Set ? valueToString(o.value)
                                          : std::string{});
#ifdef NSM_STRICT_OVERRIDES
    std::abort();
#endif
}

} // namespace

std::optional<dbus::Value> normalize(const dbus::Value& target,
                                     const dbus::Value& value)
{
    if (target.index() == value.index())
    {
        return value;
    }

    return std::visit(
        []([[maybe_unused]] const auto& t,
           [[maybe_unused]] const auto& v) -> std::optional<dbus::Value> {
        using T = std::decay_t<decltype(t)>;
        using V = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, double> && isInteger<V>)
        {
            return dbus::Value{static_cast<double>(v)};
        }
        else if constexpr (isInteger<T> && isInteger<V>)
        {
            if (!std::in_range<T>(v))
            {
                return std::nullopt;
            }
            return dbus::Value{static_cast<T>(v)};
        }
        else if constexpr (IntegerVector<T>::value && IntegerVector<V>::value)
        {
            using E = typename IntegerVector<T>::element;
            T converted;
            converted.reserve(v.size());
            for (const auto& element : v)
            {
                if (!std::in_range<E>(element))
                {
                    return std::nullopt;
                }
                converted.push_back(static_cast<E>(element));
            }
            return dbus::Value{std::move(converted)};
        }
        else
        {
            return std::nullopt;
        }
    },
        target, value);
}

bool applyOverride(Feature& feature, const std::string& key,
                   const dbus::Value& value, std::string& error)
{
    auto set = [&](const dbus::Value& declared) {
        auto normalized = normalize(declared, value);
        if (!normalized)
        {
            error = "type mismatch";
            return false;
        }
        feature.params.insert_or_assign(key,
                                        ParamValue{std::move(*normalized)});
        return true;
    };

    if (auto it = feature.params.find(key); it != feature.params.end())
    {
        return set(it->second.value);
    }
    if (auto it = feature.optional.find(key); it != feature.optional.end())
    {
        return set(it->second);
    }
    error = "unknown property";
    return false;
}

ApplyStats applyOverrides(FeatureList& features,
                          const std::vector<Override>& overrides,
                          const std::string& deviceName)
{
    ApplyStats stats;

    for (const auto& o : overrides)
    {
        if (o.action == Override::Action::Set)
        {
            continue;
        }
        auto* feature = findFeature(features, o.feature);
        if (!feature)
        {
            reject(stats, deviceName, o, "unknown feature");
            continue;
        }
        bool enable = o.action == Override::Action::Enable;
        if (feature->enabled == enable)
        {
            lg2::debug(
                "Feature override has no effect: {NAME} {ACTION} {FEATURE}",
                "NAME", deviceName, "ACTION", actionName(o.action), "FEATURE",
                o.feature);
            continue;
        }
        feature->enabled = enable;
        ++stats.applied;
    }

    for (const auto& o : overrides)
    {
        if (o.action != Override::Action::Set)
        {
            continue;
        }
        auto* feature = findFeature(features, o.feature);
        if (!feature)
        {
            reject(stats, deviceName, o, "unknown feature");
            continue;
        }
        std::string error;
        if (!applyOverride(*feature, o.property, o.value, error))
        {
            reject(stats, deviceName, o, error);
            continue;
        }
        ++stats.applied;
    }

    return stats;
}

std::string valueToString(const dbus::Value& value)
{
    return std::visit([]([[maybe_unused]] const auto& v) -> std::string {
        using V = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<V, bool>)
        {
            return v ? "true" : "false";
        }
        else if constexpr (isInteger<V> || std::is_same_v<V, double>)
        {
            return std::format("{}", v);
        }
        else if constexpr (std::is_same_v<V, std::string>)
        {
            return v;
        }
        else if constexpr (std::is_same_v<V, std::vector<std::string>> ||
                           IntegerVector<V>::value)
        {
            std::string out = "[";
            for (size_t i = 0; i < v.size(); ++i)
            {
                out += std::format("{}{}", i ? "," : "", v[i]);
            }
            return out + "]";
        }
        else
        {
            return "<unprintable>";
        }
    }, value);
}

} // namespace nsm::features
