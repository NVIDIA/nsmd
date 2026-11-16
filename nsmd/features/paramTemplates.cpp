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

#include "paramTemplates.hpp"

#include "features/overrides/overrideApply.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace nsm::features
{

namespace
{

constexpr std::array<std::string_view, 7> builtinNames = {
    "device",        "deviceBase",        "instance",      "instance+1",
    "parentChassis", "parentChassisName", "platformPrefix"};

struct Lookup
{
    bool known = false;               // a built-in or a device variable
    std::optional<dbus::Value> value; // nullopt: known but has no value
};

/** @param token "{name}" */
Lookup lookup(const DeviceContext& dev, std::string_view token)
{
    if (token.size() < 2 || token.front() != '{' || token.back() != '}')
    {
        return {};
    }
    auto name = token.substr(1, token.size() - 2);
    if (name == "device")
    {
        return {true, dbus::Value{dev.name}};
    }
    if (name == "deviceBase")
    {
        return {true, dbus::Value{deviceBaseName(dev)}};
    }
    if (name == "platformPrefix")
    {
        return {true, dbus::Value{dev.platformPrefix}};
    }
    if (name == "instance")
    {
        return {true, dbus::Value{static_cast<uint64_t>(dev.instance)}};
    }
    if (name == "instance+1")
    {
        return {true, dbus::Value{static_cast<uint64_t>(dev.instance) + 1}};
    }
    if (name == "parentChassis")
    {
        if (dev.parentChassis.empty())
        {
            return {true, std::nullopt};
        }
        return {true, dbus::Value{dev.parentChassis}};
    }
    if (name == "parentChassisName")
    {
        if (dev.parentChassis.empty())
        {
            return {true, std::nullopt};
        }
        return {true, dbus::Value{dev.parentChassis.substr(
                          dev.parentChassis.rfind('/') + 1)}};
    }
    auto it = dev.variables.find(std::string(name));
    if (it == dev.variables.end())
    {
        return {};
    }
    return {true, it->second};
}

std::optional<std::string> textOf(const dbus::Value& value)
{
    return std::visit(
        []([[maybe_unused]] const auto& v) -> std::optional<std::string> {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<V, std::string>)
        {
            return v;
        }
        else if constexpr (std::is_same_v<V, bool>)
        {
            return v ? "true" : "false";
        }
        else if constexpr (std::is_integral_v<V>)
        {
            return std::to_string(v);
        }
        else
        {
            return std::nullopt;
        }
    }, value);
}

bool resolveText(std::string& text, const DeviceContext& dev,
                 std::string& error)
{
    std::string out;
    size_t pos = 0;
    while (pos < text.size())
    {
        auto open = text.find('{', pos);
        auto close = open == std::string::npos ? std::string::npos
                                               : text.find('}', open + 1);
        if (close == std::string::npos)
        {
            out.append(text, pos);
            break;
        }
        out.append(text, pos, open - pos);
        std::string_view token(text.data() + open, close - open + 1);
        auto found = lookup(dev, token);
        if (!found.known)
        {
            out += '{';
            pos = open + 1;
            continue;
        }
        if (!found.value)
        {
            error = std::format("{} has no value", token);
            return false;
        }
        auto replacement = textOf(*found.value);
        if (!replacement)
        {
            error = std::format("{} value {} cannot be used in text", token,
                                valueToString(*found.value));
            return false;
        }
        out += *replacement;
        pos = close + 1;
    }
    text = std::move(out);
    return true;
}

} // namespace

bool isBuiltinVariable(std::string_view name)
{
    return std::ranges::find(builtinNames, name) != builtinNames.end();
}

std::optional<dbus::PropertyMap>
    resolveParams(const DeviceContext& dev, const ParamMap& params,
                  std::string& error, std::vector<std::string>& dropped)
{
    dbus::PropertyMap resolved;
    for (const auto& [key, param] : params)
    {
        if (!param.var.empty())
        {
            std::string reason;
            std::optional<dbus::Value> converted;
            auto found = lookup(dev, param.var);
            if (!found.value)
            {
                reason = std::format("{} has no value", param.var);
            }
            else
            {
                converted = normalize(param.value, *found.value);
                if (!converted)
                {
                    reason = std::format(
                        "{} value {} does not convert to the type of {}",
                        param.var, valueToString(*found.value), key);
                }
            }
            if (!converted)
            {
                if (!param.optional)
                {
                    error = std::format("{}, {} not set", reason, key);
                    return std::nullopt;
                }
                dropped.push_back(std::format("{}, {} not set", reason, key));
                continue;
            }
            resolved.emplace(key, std::move(*converted));
            continue;
        }

        dbus::Value value = param.value;
        if (auto* text = std::get_if<std::string>(&value))
        {
            if (!resolveText(*text, dev, error))
            {
                return std::nullopt;
            }
        }
        else if (auto* texts = std::get_if<std::vector<std::string>>(&value))
        {
            for (auto& text : *texts)
            {
                if (!resolveText(text, dev, error))
                {
                    return std::nullopt;
                }
            }
        }
        resolved.emplace(key, std::move(value));
    }
    return resolved;
}

} // namespace nsm::features
