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

#include "telemetryDiscovery.hpp"

#include "libnsm/base.h"

#include "dBusAsyncUtils.hpp"
#include "features/overrides/overrideDecode.hpp"
#include "features/paramTemplates.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <algorithm>
#include <string_view>
#include <type_traits>
#include <utility>

namespace nsm::features
{

namespace
{

constexpr auto probeInterface = "xyz.openbmc_project.Configuration.Probe";

std::optional<std::string> getString(const dbus::PropertyMap& props,
                                     const std::string& key)
{
    auto it = props.find(key);
    if (it == props.end())
    {
        return std::nullopt;
    }
    if (const auto* value = std::get_if<std::string>(&it->second))
    {
        return *value;
    }
    return std::nullopt;
}

std::optional<uint64_t> getUnsigned(const dbus::PropertyMap& props,
                                    const std::string& key)
{
    auto it = props.find(key);
    if (it == props.end())
    {
        return std::nullopt;
    }
    return std::visit(
        []([[maybe_unused]] const auto& v) -> std::optional<uint64_t> {
        using V = std::decay_t<decltype(v)>;
        if constexpr (std::is_integral_v<V> && !std::is_same_v<V, bool>)
        {
            if (std::in_range<uint64_t>(v))
            {
                return static_cast<uint64_t>(v);
            }
        }
        return std::nullopt;
    }, it->second);
}

bool hasInterface(const MapperServiceMap& serviceMap,
                  std::string_view interface)
{
    return std::ranges::any_of(serviceMap, [interface](const auto& entry) {
        return std::ranges::find(entry.second, interface) != entry.second.end();
    });
}

} // namespace

TelemetryDiscovery::TelemetryDiscovery(
    sdbusplus::bus_t& bus, const common::Event& event,
    std::function<void(DiscoveredDevice)> onDevice) :
    onDevice(std::move(onDevice))
{
    deferScan = std::make_unique<sdeventplus::source::Defer>(
        event, [this](sdeventplus::source::EventBase&) { scan(); });
    addedMatch = std::make_unique<sdbusplus::bus::match_t>(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/inventory"),
        [this](sdbusplus::message::message& msg) { interfacesAdded(msg); });
}

void TelemetryDiscovery::forget(const std::string& telemetryPath)
{
    seen.erase(telemetryPath);
}

void TelemetryDiscovery::scan()
{
    try
    {
        auto subtree = utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/inventory", 0, {telemetryInterface});
        for (const auto& [objPath, _] : subtree)
        {
            enqueue(objPath);
        }
    }
    catch (const std::exception& e)
    {
        lg2::info("No NSM_Telemetry records at startup: {ERROR}", "ERROR", e);
    }
}

void TelemetryDiscovery::interfacesAdded(sdbusplus::message::message& msg)
{
    sdbusplus::object_path objPath;
    AddedInterfaces interfaces;
    try
    {
        msg.read(objPath, interfaces);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "TelemetryDiscovery: failed to read InterfacesAdded: {ERROR}",
            "ERROR", e);
        return;
    }
    if (interfaces.contains(telemetryInterface))
    {
        enqueue(objPath);
    }
}

void TelemetryDiscovery::enqueue(const std::string& telemetryPath)
{
    // The startup scan and InterfacesAdded can both report a record, and a
    // record changed after its device was created is not applied.
    if (!seen.insert(telemetryPath).second)
    {
        lg2::info("NSM_Telemetry {PATH} already handled, ignored", "PATH",
                  telemetryPath);
        return;
    }
    pending.push(telemetryPath);
    requester::Coroutine::assign(taskHandle,
                                 [this]() { return processQueue(); });
}

requester::Coroutine TelemetryDiscovery::processQueue()
{
    while (!pending.empty())
    {
        auto telemetryPath = std::move(pending.front());
        pending.pop();
        DiscoveredDevice device;
        uint8_t rc = co_await readDevice(telemetryPath, device);
        if (rc != NSM_SW_SUCCESS)
        {
            // A later InterfacesAdded for the record may retry it.
            seen.erase(telemetryPath);
            continue;
        }
        if (onDevice)
        {
            onDevice(std::move(device));
        }
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine TelemetryDiscovery::readDevice(std::string telemetryPath,
                                                    DiscoveredDevice& out)
{
    out.telemetryPath = telemetryPath;
    const std::string entityPath =
        sdbusplus::object_path(telemetryPath).parent_path().str;

    // Unfiltered: GetObject filtered on a missing interface is a mapper error,
    // and a record without overrides is normal.
    MapperServiceMap recordServices =
        co_await utils::coGetServiceMap(telemetryPath, dbus::Interfaces{});
    std::string service;
    for (const auto& [owner, interfaces] : recordServices)
    {
        if (std::ranges::find(interfaces, telemetryInterface) !=
            interfaces.end())
        {
            service = owner;
            break;
        }
    }
    if (service.empty())
    {
        lg2::error("NSM_Telemetry {PATH}: no service hosts the record", "PATH",
                   telemetryPath);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }

    // The record's own Name is "NSM_Telemetry"; the device name is the
    // entity's.
    dbus::PropertyMap probe = co_await utils::coGetAllDbusProperty(
        service, entityPath, probeInterface);
    // A failed read is logged by the helper and fails the device below.
    dbus::PropertyMap record = co_await utils::coGetAllDbusProperty(
        service, telemetryPath, telemetryInterface);

    auto name = getString(probe, "Name");
    auto kind = getString(record, "Kind");
    auto instance = getUnsigned(record, "InstanceNumber");
    if (!name || !kind || !instance)
    {
        lg2::error(
            "NSM_Telemetry {PATH}: entity Name, record Kind and InstanceNumber are required",
            "PATH", telemetryPath);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }
    if (*instance > 0xff)
    {
        lg2::error("NSM_Telemetry {NAME}: InstanceNumber {INST} out of range",
                   "NAME", *name, "INST", *instance);
        // coverity[missing_return]
        co_return NSM_SW_ERROR_DATA;
    }
    out.name = *name;
    out.kind = *kind;
    out.instance = static_cast<uint8_t>(*instance);

    if (record.contains("EID"))
    {
        auto eid = getUnsigned(record, "EID");
        if (!eid || *eid > 0xff)
        {
            lg2::error("NSM_Telemetry {NAME}: invalid EID", "NAME", out.name);
            // coverity[missing_return]
            co_return NSM_SW_ERROR_DATA;
        }
        out.eid = static_cast<eid_t>(*eid);
    }
    if (auto parent = getString(record, "ParentChassis"))
    {
        out.parentChassis = *parent;
    }
    if (auto prefix = getString(record, "PlatformPrefix"))
    {
        out.platformPrefix = *prefix;
    }

    for (auto& [key, value] : record)
    {
        if (key == "Name" || key == "Type" || key == "Kind" ||
            key == "InstanceNumber" || key == "EID" || key == "ParentChassis")
        {
            continue;
        }
        if (isBuiltinVariable(key))
        {
            lg2::warning(
                "NSM_Telemetry {NAME}: {KEY} is a built-in variable, ignored",
                "NAME", out.name, "KEY", key);
            continue;
        }
        out.variables.emplace(key, std::move(value));
    }

    if (hasInterface(recordServices, telemetryOverridesInterface))
    {
        FeaturesMap features = co_await coGetAllAs<FeaturesMap>(
            service, telemetryPath, telemetryOverridesInterface);
        out.telemetry = decodeFeatureOverrides(features, telemetryPath);
    }

    lg2::info(
        "NSM_Telemetry {PATH}: Name={NAME} Kind={KIND} variables={VARIABLES} overrides={COUNT}",
        "PATH", telemetryPath, "NAME", out.name, "KIND", out.kind, "VARIABLES",
        out.variables.size(), "COUNT", out.telemetry.size());
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm::features
