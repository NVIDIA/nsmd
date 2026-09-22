// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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

#include "nsmAstraRegistry.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmAstra.hpp"
#include "nsmNetworkAdapterRegistry.hpp"
#include "nsmObjectFactory.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace nsm
{

namespace
{

static std::string stringProperty(const dbus::PropertyMap& properties,
                                  const std::string& name)
{
    auto property = properties.find(name);
    if (property == properties.end())
    {
        return {};
    }
    const auto* value = std::get_if<std::string>(&property->second);
    return value == nullptr ? std::string{} : *value;
}

static std::vector<std::string>
    stringListProperty(const dbus::PropertyMap& properties,
                       const std::string& name)
{
    auto property = properties.find(name);
    if (property == properties.end())
    {
        return {};
    }
    const auto* value =
        std::get_if<std::vector<std::string>>(&property->second);
    return value == nullptr ? std::vector<std::string>{} : *value;
}

/**
 * @brief Read the record's NetworkAdapters entries.
 *
 * entity-manager publishes entry N as its own interface, NetworkAdaptersN.
 * An adapter listed twice is rejected.
 */
static requester::Coroutine
    coGetAdapterIds(const std::string& objPath, const std::string& interface,
                    std::set<NetworkAdapterId>& adapterIds)
{
    const std::string prefix = interface + ".NetworkAdapters";
    auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                          dbus::Interfaces{});
    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& entryInterface : interfaces)
        {
            if (!entryInterface.starts_with(prefix))
            {
                continue;
            }
            const std::string_view suffix =
                std::string_view(entryInterface).substr(prefix.size());
            size_t index = 0;
            const auto [end, ec] = std::from_chars(
                suffix.data(), suffix.data() + suffix.size(), index);
            if (ec != std::errc{} || end != suffix.data() + suffix.size())
            {
                continue;
            }

            auto properties = co_await utils::coGetAllDbusProperty(
                utils::entityManagerServiceStr, objPath, entryInterface);
            NetworkAdapterId id{stringProperty(properties, "Chassis"),
                                stringProperty(properties, "Name")};
            if (id.chassis.empty() || id.name.empty())
            {
                lg2::error("createAstra: NetworkAdapters entry {INDEX} of "
                           "{OBJPATH} needs a Chassis and a Name",
                           "INDEX", index, "OBJPATH", objPath);
                co_return NSM_SW_ERROR_DATA;
            }
            if (!adapterIds.insert(id).second)
            {
                lg2::error("createAstra: NetworkAdapters of {OBJPATH} lists "
                           "{NAME} on {CHASSIS} more than once",
                           "OBJPATH", objPath, "NAME", id.name, "CHASSIS",
                           id.chassis);
                co_return NSM_SW_ERROR_DATA;
            }
        }
    }
    co_return NSM_SW_SUCCESS;
}

} // namespace

AstraRegistry& AstraRegistry::instance()
{
    static AstraRegistry registry;
    return registry;
}

std::optional<std::string> AstraRegistry::claimChassis(
    const std::string& chassis,
    const std::function<std::shared_ptr<NsmAstra>()>& makeAstra)
{
    if (auto existing = astras.find(chassis); existing != astras.end())
    {
        return existing->second->group();
    }
    astras.emplace(chassis, makeAstra());
    return std::nullopt;
}

requester::Coroutine createAstra([[maybe_unused]] SensorManager& manager,
                                 const std::string& interface,
                                 const std::string& objPath)
{
    auto properties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath, interface);
    const auto groupName = stringProperty(properties, "Name");
    if (groupName.empty())
    {
        lg2::error("createAstra: Name missing for {OBJPATH}", "OBJPATH",
                   objPath);
        co_return NSM_ERR_INVALID_DATA;
    }
    // A list of objects is published only as separate interfaces, so a
    // property here is a list of bare Names, which cannot tell adapters apart.
    if (properties.count("NetworkAdapters"))
    {
        lg2::error("createAstra: NetworkAdapters of {OBJPATH} must list a "
                   "Chassis and a Name for each adapter",
                   "OBJPATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }
    std::set<NetworkAdapterId> adapterIds;
    uint8_t rc = co_await coGetAdapterIds(objPath, interface, adapterIds);
    if (rc != NSM_SW_SUCCESS)
    {
        co_return NSM_ERR_INVALID_DATA;
    }
    if (adapterIds.empty())
    {
        lg2::warning("createAstra: {OBJPATH} lists no NetworkAdapters, so "
                     "SetAstraMode will always be unavailable",
                     "OBJPATH", objPath);
    }

    const std::string parentChassis =
        sdbusplus::message::object_path(objPath).parent_path().str;
    const std::string path = parentChassis + "/Oem/Nvidia/Astra";
    const auto owningGroup =
        AstraRegistry::instance().claimChassis(parentChassis, [&] {
        return std::make_shared<NsmAstra>(
            utils::DBusHandler::getBus(), path, parentChassis, groupName,
            adapterIds, stringListProperty(properties, "PCIeTopologies"));
    });
    if (owningGroup == groupName)
    {
        // At startup, scanInventory and InterfacesAdded may both queue it.
        co_return NSM_SUCCESS;
    }
    if (owningGroup)
    {
        lg2::error("createAstra: {CHASSIS} already has an Astra group, "
                   "ignoring {GROUP}",
                   "CHASSIS", parentChassis, "GROUP", groupName);
        co_return NSM_ERR_INVALID_DATA;
    }
    lg2::info("createAstra: Astra {GROUP} created under {CHASSIS}", "GROUP",
              groupName, "CHASSIS", parentChassis);

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createAstra,
                               "xyz.openbmc_project.Configuration.NSM_Astra")

} // namespace nsm
