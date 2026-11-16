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

#include "policyStore.hpp"

#include "libnsm/base.h"

#include "common/sleep.hpp"
#include "dBusAsyncUtils.hpp"
#include "features/overrides/overrideDecode.hpp"
#include "features/paramTemplates.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace nsm::features
{

namespace
{

void strictFail()
{
#ifdef NSM_STRICT_OVERRIDES
    std::abort();
#endif
}

/** @brief Index N of a Policy<N> interface, or nullopt for any other name */
std::optional<unsigned> entryIndex(std::string_view interface)
{
    std::string_view prefix = policyEntryPrefix;
    if (!interface.starts_with(prefix) || interface.size() == prefix.size())
    {
        return std::nullopt;
    }
    auto digits = interface.substr(prefix.size());
    unsigned index = 0;
    auto [end, ec] = std::from_chars(digits.data(),
                                     digits.data() + digits.size(), index);
    if (ec != std::errc{} || end != digits.data() + digits.size())
    {
        return std::nullopt;
    }
    return index;
}

} // namespace

PolicyStore::PolicyStore(sdbusplus::bus_t& bus, const common::Event& event,
                         std::function<void()> onReady) :
    event(event), onReady(std::move(onReady))
{
    lateMatch = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory"),
        [this](sdbusplus::message::message& msg) { interfacesAdded(msg); });
}

void PolicyStore::arm()
{
    if (armed)
    {
        return;
    }
    armed = true;
    lg2::info("Feature policy: waiting {MS} ms for NSM_Policy records", "MS",
              NSM_FEATURE_POLICY_WAIT_MS);
    requester::Coroutine::assign(taskHandle, [this]() { return run(); });
}

std::vector<Override> PolicyStore::overridesFor(std::string_view kind,
                                                std::string_view name) const
{
    std::vector<Override> out;
    auto it = byKind.find(kind);
    if (it == byKind.end())
    {
        return out;
    }
    for (const auto& entry : it->second)
    {
        if (entry.allDevices || entry.devices.contains(name))
        {
            out.insert(out.end(), entry.overrides.begin(),
                       entry.overrides.end());
        }
    }
    return out;
}

std::map<std::string, dbus::Value>
    PolicyStore::templateValuesFor(std::string_view kind,
                                   std::string_view name) const
{
    std::map<std::string, dbus::Value> out;
    auto it = byKind.find(kind);
    if (it == byKind.end())
    {
        return out;
    }
    for (const auto& entry : it->second)
    {
        if (entry.allDevices || entry.devices.contains(name))
        {
            for (const auto& [key, value] : entry.templateValues)
            {
                out.insert_or_assign(key, value);
            }
        }
    }
    return out;
}

requester::Coroutine PolicyStore::run()
{
    co_await common::Sleep{
        event, static_cast<uint64_t>(NSM_FEATURE_POLICY_WAIT_MS) * 1000,
        common::NonPriority};
    readStarted = true;
    co_await load();

    isReady = true;
    lg2::info("Feature policy ready: {KINDS} kinds", "KINDS", byKind.size());
    if (onReady)
    {
        onReady();
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine PolicyStore::load()
{
    GetSubTreeResponse subtree;
    try
    {
        subtree = utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/inventory", 0, {policyInterface});
    }
    catch (const std::exception& e)
    {
        lg2::info("No NSM_Policy records: {ERROR}", "ERROR", e);
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    // Duplicate-Kind resolution is by path, not mapper order.
    std::ranges::sort(subtree, {},
                      [](const auto& e) -> const auto& { return e.first; });
    for (const auto& [path, services] : subtree)
    {
        if (services.empty())
        {
            continue;
        }
        co_await loadRecord(path, services.front().first);
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine PolicyStore::loadRecord(std::string path,
                                             std::string service)
{
    dbus::PropertyMap base =
        co_await utils::coGetAllDbusProperty(service, path, policyInterface);
    auto kindIt = base.find("Kind");
    const auto* kind = kindIt == base.end()
                           ? nullptr
                           : std::get_if<std::string>(&kindIt->second);
    if (!kind || kind->empty())
    {
        lg2::error("NSM_Policy {PATH}: Kind is required, record ignored",
                   "PATH", path);
        strictFail();
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }
    if (auto [owner, inserted] = kindOwner.try_emplace(*kind, path); !inserted)
    {
        lg2::error(
            "NSM_Policy {PATH}: Kind {KIND} already defined by {OWNER}, record ignored",
            "PATH", path, "KIND", *kind, "OWNER", owner->second);
        strictFail();
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    MapperServiceMap serviceMap =
        co_await utils::coGetServiceMap(path, dbus::Interfaces{});
    std::map<unsigned, std::string> entryInterfaces;
    for (const auto& [owner, interfaces] : serviceMap)
    {
        if (owner != service)
        {
            continue;
        }
        for (const auto& interface : interfaces)
        {
            if (auto index = entryIndex(interface))
            {
                entryInterfaces.emplace(*index, interface);
            }
        }
    }

    auto& entries = byKind[*kind];
    for (const auto& [index, interface] : entryInterfaces)
    {
        const std::string source = std::format("{}:Policy{}", path, index);
        PolicyEntryMap props =
            co_await coGetAllAs<PolicyEntryMap>(service, path, interface);

        // EM does not publish an empty array, so "Devices": [] arrives as a
        // missing property; both apply the entry to every device of the Kind.
        Entry entry;
        auto devicesIt = props.find("Devices");
        if (devicesIt == props.end())
        {
            entry.allDevices = true;
        }
        else if (const auto* devices =
                     std::get_if<std::vector<std::string>>(&devicesIt->second))
        {
            entry.allDevices = devices->empty();
            entry.devices.insert(devices->begin(), devices->end());
        }
        else
        {
            lg2::error("{SOURCE}: Devices must be as, ignored", "SOURCE",
                       source);
            strictFail();
            continue;
        }
        for (const auto& [key, value] : props)
        {
            if (key == "Devices")
            {
                continue;
            }
            const auto* values = std::get_if<FeatureOverrides>(&value);
            if (!values)
            {
                lg2::error("{SOURCE}: {FEATURE} must be a{sv}, ignored",
                           "SOURCE", source, "FEATURE", key);
                strictFail();
                continue;
            }
            if (key == templateValuesKey)
            {
                for (const auto& [name, templateValue] : *values)
                {
                    if (isBuiltinVariable(name))
                    {
                        lg2::warning(
                            "{SOURCE}: TemplateValues {KEY} is a built-in variable, ignored",
                            "SOURCE", source, "KEY", name);
                        continue;
                    }
                    entry.templateValues.insert_or_assign(name, templateValue);
                }
                continue;
            }
            decodeFeatureOverrides(key, *values, source, entry.overrides);
        }
        lg2::debug(
            "{SOURCE}: {DEVICES} devices, {COUNT} overrides, {VALUES} template values",
            "SOURCE", source, "DEVICES", entry.devices.size(), "COUNT",
            entry.overrides.size(), "VALUES", entry.templateValues.size());
        entries.push_back(std::move(entry));
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

void PolicyStore::interfacesAdded(sdbusplus::message::message& msg)
{
    if (!readStarted)
    {
        // Picked up by load() if it arrives before the wait expires.
        return;
    }
    sdbusplus::object_path objPath;
    AddedInterfaces interfaces;
    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const std::exception&)
    {
        return;
    }
    if (interfaces.contains(policyInterface))
    {
        lg2::error(
            "NSM_Policy {PATH} added after the policy was read; not applied until nsmd restarts",
            "PATH", objPath.str);
    }
}

} // namespace nsm::features
