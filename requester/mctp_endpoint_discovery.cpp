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

#include "mctp_endpoint_discovery.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mctp
{
const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";

MctpDiscovery::MctpDiscovery(
    sdbusplus::bus::bus& bus, mctp_socket::Handler& handler,
    std::initializer_list<MctpDiscoveryHandlerIntf*> list) :
    bus(bus),
    handler(handler),
    mctpEndpointAddedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/xyz/openbmc_project/mctp"),
        std::bind_front(&MctpDiscovery::discoverEndpoints, this)),
    mctpEndpointRemovedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesRemoved(
            "/xyz/openbmc_project/mctp"),
        std::bind_front(&MctpDiscovery::cleanEndpoints, this)),
    handlers(list)
{
    dbus::ObjectValueTree objects;
    std::set<dbus::Service> mctpCtrlServices;
    MctpInfos mctpInfos;

    try
    {
        const dbus::Interfaces ifaceList{"xyz.openbmc_project.MCTP.Endpoint"};
        auto getSubTreeResponse = utils::DBusHandler().getSubtree(
            "/xyz/openbmc_project/mctp", 0, ifaceList);
        for (const auto& [objPath, mapperServiceMap] : getSubTreeResponse)
        {
            for (const auto& [serviceName, interfaces] : mapperServiceMap)
            {
                mctpCtrlServices.emplace(serviceName);
            }
        }
    }
    catch (const std::exception& e)
    {
        handleMctpEndpoints(mctpInfos);
        return;
    }

    for (const auto& service : mctpCtrlServices)
    {
        dbus::ObjectValueTree objects{};
        try
        {
            auto method = bus.new_method_call(
                service.c_str(), "/xyz/openbmc_project/mctp",
                "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
            auto reply = bus.call(method);
            reply.read(objects);
            for (const auto& [objectPath, interfaces] : objects)
            {
                populateMctpInfo(interfaces, mctpInfos);

                // watch PropertiesChanged signal from
                // xyz.openbmc_project.Object.Enable PDI
                if (enableMatches.find(objectPath.str) == enableMatches.end())
                {
                    enableMatches.emplace(
                        objectPath.str,
                        sdbusplus::bus::match_t(
                            bus,
                            sdbusplus::bus::match::rules::propertiesChanged(
                                objectPath.str,
                                "xyz.openbmc_project.Object.Enable"),
                            std::bind_front(&MctpDiscovery::refreshEndpoints,
                                            this)));
                }
            }
        }
        catch (const std::exception& e)
        {
            continue;
        }
    }

    handleMctpEndpoints(mctpInfos);
}

void MctpDiscovery::populateMctpInfo(const dbus::InterfaceMap& interfaces,
                                     MctpInfos& mctpInfos)
{
    uuid_t uuid{};
    int type = 0;
    int protocol = 0;
    std::vector<uint8_t> address{};
    std::string bindingType;
    try
    {
        for (const auto& [intfName, properties] : interfaces)
        {
            if (intfName == uuidEndpointIntfName)
            {
                uuid = std::get<std::string>(properties.at("UUID"));
            }

            if (intfName == unixSocketIntfName)
            {
                type = std::get<size_t>(properties.at("Type"));
                protocol = std::get<size_t>(properties.at("Protocol"));
                address =
                    std::get<std::vector<uint8_t>>(properties.at("Address"));
            }
        }

        if (uuid.empty() || address.empty() || !type)
        {
            return;
        }

        if (interfaces.contains(mctpBindingIntfName))
        {
            const auto& properties = interfaces.at(mctpBindingIntfName);
            if (properties.contains("BindingType"))
            {
                bindingType =
                    std::get<std::string>(properties.at("BindingType"));
            }
        }
        if (interfaces.contains(mctpEndpointIntfName))
        {
            const auto& properties = interfaces.at(mctpEndpointIntfName);
            if (properties.contains("EID") &&
                properties.contains("SupportedMessageTypes") &&
                properties.contains("MediumType"))
            {
                auto eid = std::get<size_t>(properties.at("EID"));
		// MCTP EID 0 is a special Null EID as per MCTP DMTF specification doc
                if (eid == MCTP_NULL_EID)
                {
                    return;
                }
                auto mctpTypes = std::get<std::vector<uint8_t>>(
                    properties.at("SupportedMessageTypes"));
                auto mediumType =
                    std::get<std::string>(properties.at("MediumType"));
                auto networkId = std::get<size_t>(properties.at("NetworkId"));
                if (std::find(mctpTypes.begin(), mctpTypes.end(),
                              mctpTypeVDM) != mctpTypes.end())
                {
                    handler.registerMctpEndpoint(eid, type, protocol, address);
                    mctpInfos.emplace_back(std::make_tuple(
                        eid, uuid, mediumType, networkId, bindingType));
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error while getting properties.", "ERROR", e);
    }
}

void MctpDiscovery::discoverEndpoints(sdbusplus::message::message& msg)
{
    constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};
    MctpInfos mctpInfos{};

    sdbusplus::message::object_path objPath;
    dbus::InterfaceMap interfaces;
    msg.read(objPath, interfaces);

    populateMctpInfo(interfaces, mctpInfos);

    // watch PropertiesChanged signal from xyz.openbmc_project.Object.Enable PDI
    if (enableMatches.find(objPath.str) == enableMatches.end())
    {
        enableMatches.emplace(
            objPath.str,
            sdbusplus::bus::match_t(
                bus,
                sdbusplus::bus::match::rules::propertiesChanged(
                    objPath.str, "xyz.openbmc_project.Object.Enable"),
                std::bind_front(&MctpDiscovery::refreshEndpoints, this)));
    }
    handleMctpEndpoints(mctpInfos);
}

void MctpDiscovery::handleMctpEndpoints(const MctpInfos& mctpInfos)
{
    for (MctpDiscoveryHandlerIntf* handler : handlers)
    {
        if (handler)
        {
            handler->handleMctpEndpoints(mctpInfos);
        }
    }
}

void MctpDiscovery::refreshEndpoints(sdbusplus::message::message& msg)
{
    std::string interface;
    dbus::PropertyMap properties;
    dbus::PropertyMap allProperties;
    std::string objPath = msg.get_path();
    std::string sender = msg.get_sender();

    msg.read(interface, properties);
    auto prop = properties.find("Enabled");
    if (prop != properties.end())
    {
        auto enabled = std::get<bool>(prop->second);
        lg2::info(
            "Received xyz.openbmc_poject.Object.Enabled propertiesChanged signal for "
            "Enabled=={ENABLED} at PATH={OBJ_PATH} from sender={SENDER}",
            "ENABLED", enabled, "OBJ_PATH", objPath, "SENDER", sender);
        try
        {
            std::string service = utils::DBusHandler().getService(
                objPath.c_str(), "xyz.openbmc_project.MCTP.Endpoint");

            lg2::info("service of PATH={OBJ_PATH} is {SERVICE}", "OBJ_PATH",
                      objPath, "SERVICE", service);
            auto method = bus.new_method_call(service.c_str(), objPath.c_str(),
                                              "org.freedesktop.DBus.Properties",
                                              "GetAll");
            method.append("");
            auto reply = bus.call(method);
            reply.read(allProperties);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "refreshEndpoints: failed to get MctpInfo from PATH={OBJ_PATH}, {ERROR}",
                "OBJ_PATH", objPath, "ERROR", e);
            return;
        }

        uint32_t eid{};
        uint32_t networkId{};
        std::string mediumType{};
        std::string uuid{};
        std::string bindingType{};

        if (allProperties.contains("EID"))
        {
            eid = std::get<uint32_t>(allProperties.at("EID"));
        }
        if (allProperties.contains("NetworkId"))
        {
            networkId = std::get<uint32_t>(allProperties.at("NetworkId"));
        }
        if (allProperties.contains("MediumType"))
        {
            mediumType = std::get<std::string>(allProperties.at("MediumType"));
        }
        if (allProperties.contains("UUID"))
        {
            uuid = std::get<std::string>(allProperties.at("UUID"));
        }
        if (allProperties.contains("BindingType"))
        {
            bindingType =
                std::get<std::string>(allProperties.at("BindingType"));
        }
        lg2::error("refreshEndpoints: EID={EID}, UUID={UUID}", "EID", eid,
                   "UUID", uuid);

        MctpInfo mctpInfo = std::make_tuple(eid, uuid, mediumType, networkId,
                                            bindingType);
        for (MctpDiscoveryHandlerIntf* handler : handlers)
        {
            if (enabled)
            {
                handler->onlineMctpEndpoint(mctpInfo);
            }
            else
            {
                handler->offlineMctpEndpoint(mctpInfo);
            }
        }
    }
}

void MctpDiscovery::cleanEndpoints(
    [[maybe_unused]] sdbusplus::message::message& msg)
{
    // place holder: implement the function once mctp-ctrl service support the
    // InterfacesRemoved signal
}

} // namespace mctp
