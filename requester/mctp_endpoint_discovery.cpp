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

#include "config.h"

#include "mctp_endpoint_discovery.hpp"

#include "common/types.hpp"
#include "common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmd/sensorManager.hpp"
#include "progressCounters.hpp"

#include <systemd/sd-bus.h>

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
MctpDiscovery* MctpDiscovery::instance = nullptr;
const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";

MctpDiscovery::MctpDiscovery(
    sdbusplus::bus::bus& bus, mctp_socket::Handler& handler,
    std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler, EidTable& eidTable,
    nsm::NsmDeviceTable& nsmDevices,
    sdbusplus::asio::object_server& objServer) :
    bus(bus), handler(handler), nsmMsgHandler(nsmMsgHandler),
    eidTable(eidTable), nsmDevices(nsmDevices), objServer(objServer),
    mctpEndpointAddedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesAdded(
            "/au/com/codeconstruct/mctp1"),
        std::bind_front(&MctpDiscovery::discoverEndpoints, this)),
    mctpEndpointRemovedSignal(
        bus,
        sdbusplus::bus::match::rules::interfacesRemoved(
            "/au/com/codeconstruct/mctp1"),
        std::bind_front(&MctpDiscovery::cleanEndpoints, this))
{
    dbus::ObjectValueTree objects;
    std::set<dbus::Service> mctpCtrlServices;
    MctpInfos mctpInfos;

    try
    {
        const dbus::Interfaces ifaceList{"xyz.openbmc_project.MCTP.Endpoint"};
        auto getSubTreeResponse = utils::DBusHandler().getSubtree(
            "/au/com/codeconstruct/mctp1", 0, ifaceList);
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
        discoverNsmDevice(mctpInfos);
        return;
    }

    for (const auto& service : mctpCtrlServices)
    {
        dbus::ObjectValueTree objects{};
        try
        {
            auto method = bus.new_method_call(
                service.c_str(), "/au/com/codeconstruct/mctp1",
                "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
            auto reply = bus.call(method);
            reply.read(objects);
            for (const auto& [objectPath, interfaces] : objects)
            {
                populateMctpInfo(interfaces, objectPath.str, mctpInfos);

                // watch PropertiesChanged signal from
                // au.com.codeconstruct.MCTP.Endpoint1 PDI
                if (enableMatches.find(objectPath.str) == enableMatches.end())
                {
                    enableMatches.emplace(
                        objectPath.str,
                        sdbusplus::bus::match_t(
                            bus,
                            sdbusplus::bus::match::rules::propertiesChanged(
                                objectPath.str,
                                "au.com.codeconstruct.MCTP.Endpoint1"),
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

    discoverNsmDevice(mctpInfos);
}

void MctpDiscovery::populateMctpInfo(const dbus::InterfaceMap& interfaces,
                                     const std::string& objPath,
                                     MctpInfos& mctpInfos)
{
    uuid_t uuid{};
    int type = 0;
    int protocol = 0;
    std::vector<uint8_t> address{};
    std::string bindingType;
    Active active = false;
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

            if (intfName == codeConstructEndpointIntfName)
            {
                auto connectivity =
                    std::get<std::string>(properties.at("Connectivity"));
                active = (connectivity == "Available");
            }
        }

        if (uuid.empty())
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
                properties.contains("NetworkId"))
            {
                auto eid = std::get<uint8_t>(properties.at("EID"));
                if constexpr (FILTER_MCTP_EID)
                {
                    // MCTP EID 0 is a special Null EID as per MCTP DMTF
                    // specification doc
                    if (eid == MCTP_EID_TO_FILTER)
                    {
                        return;
                    }
                }
                auto mctpTypes = std::get<std::vector<uint8_t>>(
                    properties.at("SupportedMessageTypes"));

                std::string mediumType{};

                auto hasMediumType = properties.find("MediumType");
                if (hasMediumType != properties.end())
                {
                    mediumType = std::get<std::string>(hasMediumType->second);
                }

                auto networkId = std::get<uint32_t>(properties.at("NetworkId"));
                if (std::find(mctpTypes.begin(), mctpTypes.end(),
                              mctpTypeVDM) != mctpTypes.end())
                {
                    handler.registerMctpEndpoint(eid, type, protocol, address);
                    cachedMctpInfoByPath[objPath] =
                        std::make_tuple(eid, uuid, mediumType, networkId,
                                        bindingType, active, objPath);
                    mctpInfos.emplace_back(cachedMctpInfoByPath[objPath]);
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Error while getting properties.", "ERROR", e);
    }
}

void MctpDiscovery::handleMctpEndpoints(const MctpInfos& mctpInfos)
{
    discoverNsmDevice(mctpInfos);
}

requester::Coroutine
    MctpDiscovery::deviceStateChangeTask(const std::string objPath)
{
    while (!mctpQueuedSignals[objPath].empty())
    {
        MctpInfos mctpInfos{};
        sdbusplus::message::message& msg = mctpQueuedSignals[objPath].front();
        lg2::info(
            "deviceStateChangeTask mctpQueuedSignals for PATH={OBJ_PATH} size= {SIZE}",
            "OBJ_PATH", objPath, "SIZE", mctpQueuedSignals[objPath].size());
        auto member = msg.get_member();
        sd_bus_message_rewind(msg.get(), true);
        if (strcmp(member, "PropertiesChanged") == 0)
        {
            co_await handleRefreshEndpoints(msg, mctpInfos);
        }
        else if (strcmp(member, "InterfacesAdded") == 0)
        {
            co_await handleDiscoverEndpoints(msg, mctpInfos);
        }
        else if (strcmp(member, "InterfacesRemoved") == 0)
        {
            co_await handleCleanEndpoints(msg, mctpInfos);
        }
        else
        {
            lg2::error(
                "deviceStateChangeTask: unknown member={MEMBER} for PATH={OBJ_PATH}",
                "MEMBER", member, "OBJ_PATH", objPath);
        }
        discoverNsmDevice(mctpInfos);
        mctpQueuedSignals[objPath].pop();
        lg2::info(
            "deviceStateChangeTask: mctpQueuedSignals for PATH={OBJ_PATH} size= {SIZE}",
            "OBJ_PATH", objPath, "SIZE", mctpQueuedSignals[objPath].size());
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    MctpDiscovery::handleDiscoverEndpoints(sdbusplus::message::message& msg,
                                           MctpInfos& mctpInfos)
{
    constexpr std::string_view mctpEndpointIntfName{
        "xyz.openbmc_project.MCTP.Endpoint"};

    sdbusplus::message::object_path objPath;
    dbus::InterfaceMap interfaces;
    msg.read(objPath, interfaces);
    populateMctpInfo(interfaces, objPath.str, mctpInfos);

    if (!mctpInfos.empty())
    {
        auto eid = std::get<0>(mctpInfos[0]);
        discoveryEvents(eid).increment(
            nsm::DiscoveryEventType::InterfaceAddedSignal);
    }

    // watch PropertiesChanged signal from au.com.codeconstruct.MCTP.Endpoint1
    // PDI
    if (enableMatches.find(objPath.str) == enableMatches.end())
    {
        enableMatches.emplace(
            objPath.str,
            sdbusplus::bus::match_t(
                bus,
                sdbusplus::bus::match::rules::propertiesChanged(
                    objPath.str, "au.com.codeconstruct.MCTP.Endpoint1"),
                std::bind_front(&MctpDiscovery::refreshEndpoints, this)));
    }
    co_return NSM_SW_SUCCESS;
}

void MctpDiscovery::discoverEndpoints(sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    dbus::InterfaceMap interfaces;
    msg.read(objPath, interfaces);
    sd_bus_message_rewind(msg.get(), true);

    if (interfaces.find(std::string(mctpEndpointIntfName)) != interfaces.end())
    {
        lg2::info(
            "MctpDiscovery: Recieved InterfacesAdded signal for objPath={OBJ_PATH}",
            "OBJ_PATH", objPath.str);
        mctpQueuedSignals[objPath.str].emplace(msg);
        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath.str],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath.str);
        });
    }
}

requester::Coroutine
    MctpDiscovery::handleRefreshEndpoints(sdbusplus::message::message& msg,
                                          MctpInfos& mctpInfos)
{
    std::string interface;
    dbus::PropertyMap properties;
    dbus::PropertyMap allProperties;
    std::string sender = msg.get_sender();
    std::string objPath = msg.get_path();

    msg.read(interface, properties);
    auto prop = properties.find("Connectivity");
    if (prop != properties.end())
    {
        auto connectivity = std::get<std::string>(prop->second);
        lg2::info(
            "Processing au.com.codeconstruct.MCTP.Endpoint1 propertiesChanged signal for "
            "Connectivity=={CONN} at PATH={OBJ_PATH} from sender={SENDER}",
            "CONN", connectivity, "OBJ_PATH", objPath, "SENDER", sender);
        handleMctpStateTransition(objPath, (connectivity == "Available"));
        try
        {
            dbus::Interfaces interfaces{"xyz.openbmc_project.MCTP.Endpoint"};
            auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                                  interfaces);
            if (mapperResponse.size() == 0)
            {
                lg2::error(
                    "handleRefreshEndpoints: coGetServiceMap failed for PATH={OBJ_PATH}",
                    "OBJ_PATH", objPath);
                co_return NSM_SW_ERROR;
            }
            std::string service = mapperResponse.begin()->first;
            lg2::info("service of PATH={OBJ_PATH} is {SERVICE}", "OBJ_PATH",
                      objPath, "SERVICE", service);
            allProperties = co_await utils::coGetAllDbusProperty(service,
                                                                 objPath);
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "handleRefreshEndpoints: failed to get MctpInfo from PATH={OBJ_PATH},{ERROR}",
                "OBJ_PATH", objPath, "ERROR", e);
            co_return NSM_SW_ERROR;
        }

        uint8_t eid{};
        uint32_t networkId{};
        std::string mediumType{};
        std::string uuid{};
        std::string bindingType{};
        std::vector<uint8_t> mctpTypes{};

        if (allProperties.contains("EID"))
        {
            eid = std::get<uint8_t>(allProperties.at("EID"));
        }
        if constexpr (FILTER_MCTP_EID)
        {
            // MCTP EID 0 is a special Null EID as per MCTP DMTF
            // specification doc
            if (eid == MCTP_EID_TO_FILTER)
            {
                co_return NSM_SW_ERROR;
            }
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
        if (allProperties.contains("SupportedMessageTypes"))
        {
            mctpTypes = std::get<std::vector<uint8_t>>(
                allProperties.at("SupportedMessageTypes"));
        }

        MctpInfo mctpInfo =
            std::make_tuple(eid, uuid, mediumType, networkId, bindingType,
                            (connectivity == "Available"), objPath);
        cachedMctpInfoByPath[objPath] = mctpInfo;
        if (connectivity == "Available")
        {
            if (std::find(mctpTypes.begin(), mctpTypes.end(), mctpTypeVDM) !=
                mctpTypes.end())
            {
                mctpInfos.push_back(mctpInfo);
            }
            else
            {
                lg2::info(
                    "handleRefreshEndpoints: mctpTypeVDM command not supported for PATH={OBJ_PATH}",
                    "OBJ_PATH", objPath);
            }
        }
        else
        {
            mctpInfos.push_back(mctpInfo);
        }
        if (!mctpInfos.empty())
        {
            auto eid = std::get<0>(mctpInfos[0]);
            discoveryEvents(eid).setValue(
                nsm::DiscoveryEventType::ConnectivityAvailable,
                (connectivity == "Available") ? 1 : 0);
        }
    }
    co_return NSM_SW_SUCCESS;
}

void MctpDiscovery::refreshEndpoints(sdbusplus::message::message& msg)
{
    std::string interface;
    dbus::PropertyMap properties;
    dbus::PropertyMap allProperties;
    std::string objPath = msg.get_path();
    std::string sender = msg.get_sender();
    msg.read(interface, properties);
    // move back read cursor to beginning of message before puting on the queue
    sd_bus_message_rewind(msg.get(), true);

    auto prop = properties.find("Connectivity");
    if (prop != properties.end())
    {
        auto connectivity = std::get<std::string>(prop->second);
        lg2::info(
            "Received au.com.codeconstruct.MCTP.Endpoint1 propertiesChanged signal for "
            "Connectivity=={CONN} at PATH={OBJ_PATH} from sender={SENDER}",
            "CONN", connectivity, "OBJ_PATH", objPath, "SENDER", sender);

        mctpQueuedSignals[objPath].emplace(msg);

        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath);
        });
    }
}

requester::Coroutine
    MctpDiscovery::handleCleanEndpoints(sdbusplus::message::message& msg,
                                        MctpInfos& mctpInfos)
{
    sdbusplus::message::object_path objPath;
    std::vector<std::string> interfaces;
    msg.read(objPath, interfaces);
    if (cachedMctpInfoByPath.find(objPath.str) != cachedMctpInfoByPath.end())
    {
        mctpInfos.push_back(cachedMctpInfoByPath[objPath.str]);
        std::get<5>(mctpInfos[0]) = false;
        eid_t eid = std::get<0>(mctpInfos[0]);
        discoveryEvents(eid).increment(
            nsm::DiscoveryEventType::InterfaceRemovedSignal);
    }
    else
    {
        lg2::error("MctpDiscovery: No MctpInfos cached for objPath={OBJ_PATH}",
                   "OBJ_PATH", objPath.str);
    }
    co_return NSM_SW_SUCCESS;
}

void MctpDiscovery::cleanEndpoints(
    [[maybe_unused]] sdbusplus::message::message& msg)
{
    sdbusplus::message::object_path objPath;
    std::vector<std::string> interfaces;
    msg.read(objPath, interfaces);
    sd_bus_message_rewind(msg.get(), true);
    if (std::find(interfaces.begin(), interfaces.end(),
                  std::string(mctpEndpointIntfName)) != interfaces.end())
    {
        lg2::info(
            "MctpDiscovery: Recieved InterfacesRemoved signal for objPath={OBJ_PATH}",
            "OBJ_PATH", objPath.str);
        mctpQueuedSignals[objPath.str].emplace(msg);
        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath.str],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath.str);
        });
    }
}

requester::Coroutine
    MctpDiscovery::SendRecvNsmMsg(eid_t eid, Request& request,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t* responseLen)
{
    auto rc = co_await nsmMsgHandler->SendRecvNsmMsg(eid, request, responseMsg,
                                                     responseLen);
    if (rc)
    {
        lg2::error("MctpDiscovery::SendRecvNsmMsg failed. eid={EID} rc={RC}",
                   "EID", eid, "RC", utils::nsmSwCodeToString(rc));
    }
    co_return rc;
}

bool MctpDiscovery::insertIntoEidTableifNotExist(
    uuid_t uuid, const std::tuple<eid_t, MctpMedium, MctpBinding>& value)
{
    auto range = eidTable.equal_range(uuid);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == value)
        {
            return false;
        }
    }
    eidTable.emplace(uuid, value);
    return true;
}

void MctpDiscovery::discoverNsmDevice(const MctpInfos& mctpInfos)
{
    for (auto mctpInfo : mctpInfos)
    {
        auto eid = std::get<0>(mctpInfo);
        perEidQueuedMctpInfos[eid].emplace(mctpInfo);
        requester::Coroutine::assign(perEidDiscoverNsmDeviceTaskHandle[eid],
                                     [&, eid]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await discoverNsmDeviceTask(eid);
        });
    }
}

requester::Coroutine MctpDiscovery::discoverNsmDeviceTask(eid_t eid)
{
    while (!perEidQueuedMctpInfos[eid].empty())
    {
        lg2::info("discoverNsmDeviceTask eid={EID}, size={SIZE}", "EID", eid,
                  "SIZE", perEidQueuedMctpInfos[eid].size());
        auto mctpInfo = perEidQueuedMctpInfos[eid].front();
        auto active = std::get<5>(mctpInfo);
        MctpInfos mctpInfos{mctpInfo};
        if (active)
        {
            auto rc = co_await coSetdeviceStateOnlineTask(mctpInfos);
            discoveryEvents(eid).setValue(
                nsm::DiscoveryEventType::SetDeviceStateOnline, rc);
        }
        else
        {
            auto rc = co_await coSetdeviceStateOfflineTask(mctpInfos);
            discoveryEvents(eid).setValue(
                nsm::DiscoveryEventType::SetDeviceStateOffline, rc);
        }
        perEidQueuedMctpInfos[eid].pop();
        lg2::info("discoverNsmDeviceTask eid={EID}, size={SIZE}", "EID", eid,
                  "SIZE", perEidQueuedMctpInfos[eid].size());
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    MctpDiscovery::coSetdeviceStateOnlineTask(const MctpInfos& mctpInfos)
{
    uint8_t overallRC = NSM_SW_SUCCESS;
    for (auto& mctpInfo : mctpInfos)
    {
        // try ping
        auto& [eid, mctpUuid, mctpMedium, networkdId, mctpBinding, active,
               mctpObjPath] = mctpInfo;
        auto rc = co_await ping(eid);
        discoveryEvents(eid).setValue(nsm::DiscoveryEventType::Ping, rc);
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("NSM ping failed, rc={RC} eid={EID}", "RC",
                       utils::nsmSwCodeToString(rc), "EID", eid);
            overallRC = rc;
            continue;
        }

        lg2::info("found NSM Endpoint, eid={EID} uuid={UUID}", "EID", eid,
                  "UUID", mctpUuid);

        // get device identification from device
        uint8_t deviceType = 0;
        uint8_t instanceNumber = 0;
        rc = co_await getQueryDeviceIdentification(eid, deviceType,
                                                   instanceNumber);
        discoveryEvents(eid).setValue(
            nsm::DiscoveryEventType::QueryDeviceIdentification, rc);
        if (rc != NSM_SUCCESS)
        {
            lg2::error(
                "NSM getQueryDeviceIdentification failed, rc={RC} eid={EID}",
                "RC", utils::nsmSwCodeToString(rc), "EID", eid);
            overallRC = rc;
            continue;
        }

        std::string configuredPath = "";
        rc = co_await findConfiguredAssociations(mctpObjPath, configuredPath);

        // save the nsm device identification info
        discoveredEIDs[eid] = {mctpUuid,   deviceType,  instanceNumber, true,
                               mctpMedium, mctpBinding, configuredPath};
        auto nsmDevice = mapNsmDeviceUsingEid(eid, mctpUuid, deviceType,
                                              instanceNumber, configuredPath,
                                              true, mctpMedium, mctpBinding);
        discoveryEvents(eid).setValue(
            nsm::DiscoveryEventType::OnlineMapNsmDeviceUsingEid,
            nsmDevice ? 1 : 0);
        if (nsmDevice)
        {
            lg2::info("initDeviceDiscovery for nsmDevice eid={EID}", "EID",
                      eid);
            nsmDevice->initDeviceDiscovery();
            auto rc = co_await nsmDevice->updateNsmDevice();
            if (rc == NSM_SW_SUCCESS &&
                perEidQueuedMctpInfos[eid].size() == 1 &&
                nsmDevice->getEid() ==
                    eid) // check if there is no pending mctp rediscovery signal
                         // for same EID and nsmDevice is not changed with new
                         // EID during updateNsmDevice
            {
                co_await nsmDevice->setOnline();
                if (nsmDevice->getEid() ==
                    eid) // check if nsmDevice is not changed with new EID
                         // during setOnline
                {
                    nsmDevice->finishDeviceDiscovery();
                }
            }
        }
        // update eid table [from UUID from MCTP dbus property]
        insertIntoEidTableifNotExist(
            mctpUuid, std::make_tuple(eid, mctpMedium, mctpBinding));
    }

    // coverity[missing_return]
    co_return overallRC;
}

requester::Coroutine
    MctpDiscovery::coSetdeviceStateOfflineTask(const MctpInfos& mctpInfos)
{
    for (auto& mctpInfo : mctpInfos)
    {
        std::shared_ptr<nsm::NsmDevice> nsmDevice{};
        const mctp_eid_t eid = std::get<0>(mctpInfo);
        if (discoveredEIDs.find(eid) != discoveredEIDs.end())
        {
            auto& value = discoveredEIDs[eid];
            std::get<3>(value) = false; // set EID is inactive
            auto& [uuid, mctpDeviceType, mctpDeviceInstanceNumber, active,
                   mctpMedium, mctpBinding, associatedPath] = value;
            nsmDevice = mapNsmDeviceUsingEid(
                eid, uuid, mctpDeviceType, mctpDeviceInstanceNumber,
                associatedPath, false, mctpMedium, mctpBinding);
            discoveryEvents(eid).setValue(
                nsm::DiscoveryEventType::OfflineMapNsmDeviceUsingEid,
                nsmDevice ? 1 : 0);
        }

        if (nsmDevice)
        {
            co_await nsmDevice->setOffline();
            if (perEidQueuedMctpInfos[eid].size() == 1 &&
                nsmDevice->getEid() == eid) // check if nsmDevice is not changed
                                            // with new EID during setOffline
            {
                nsmDevice->finishDeviceDiscovery();
            }
        }
        else
        {
            // coverity[missing_return]
            co_return NSM_SW_ERROR_NULL;
        }
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine MctpDiscovery::ping(eid_t eid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_ping_req(DEFAULT_INSTANCE_ID, requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error("ping failed. eid={EID} rc={RC}", "EID", eid, "RC",
                   utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> respMsg;
    size_t respLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, respMsg, &respLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_ping_resp(respMsg.get(), respLen, &cc, &reasonCode);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "ping decode failed. eid={EID} cc={CC} reasonCode={REASONCODE} and rc={RC}",
            "EID", eid, "CC", utils::nsmCompletionCodeToString(cc),
            "REASONCODE", utils::nsmReasonCodeToString(reasonCode), "RC",
            utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine MctpDiscovery::getQueryDeviceIdentification(
    eid_t eid, uint8_t& deviceIdentification, uint8_t& deviceInstance)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_device_identification_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_identification_req(DEFAULT_INSTANCE_ID,
                                                         requestMsg);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "encode_nsm_query_device_identification_req failed. eid={EID} rc={RC}",
            "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    rc = co_await SendRecvNsmMsg(eid, request, responseMsg, &responseLen);
    if (rc)
    {
        // coverity[missing_return]
        co_return rc;
    }

    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    rc = decode_query_device_identification_resp(
        responseMsg.get(), responseLen, &cc, &reasonCode, &deviceIdentification,
        &deviceInstance);
    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error(
            "decode_query_device_identification_resp failed. eid={EID} cc={CC} reasonCode={REASONCODE} rc={RC}",
            "EID", eid, "CC", utils::nsmCompletionCodeToString(cc),
            "REASONCODE", utils::nsmReasonCodeToString(reasonCode), "RC",
            utils::nsmSwCodeToString(rc));
        // coverity[missing_return]
        co_return NSM_SW_ERROR_COMMAND_FAIL;
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    MctpDiscovery::findConfiguredAssociations(const std::string& objPath,
                                              std::string& configuredPath)
{
    dbus::Interfaces interfaces{"xyz.openbmc_project.Association.Definitions"};
    try
    {
        auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                              interfaces);
        if (mapperResponse.empty())
        {
            lg2::error(
                "No service found for PATH={OBJ_PATH} and associations interface",
                "OBJ_PATH", objPath);
            co_return NSM_SW_ERROR_NULL;
        }
        for (const auto& [service, interfaces] : mapperResponse)
        {
            auto allProperties = co_await utils::coGetAllDbusProperty(service,
                                                                      objPath);
            if (allProperties.contains("Associations"))
            {
                auto associations = std::get<std::vector<
                    std::tuple<std::string, std::string, std::string>>>(
                    allProperties.at("Associations"));
                for (const auto& [forward, reverse, target] : associations)
                {
                    if (forward == "configured_by")
                    {
                        configuredPath = target;
                        break;
                    }
                }
                if (configuredPath.empty())
                {
                    lg2::info(
                        "No configured association found for PATH={OBJ_PATH}",
                        "OBJ_PATH", objPath);
                }

                else
                {
                    lg2::info(
                        "Configured association path for objectPath={OBJ_PATH} is {CONFIGURED_BY_PATH}",
                        "OBJ_PATH", objPath, "CONFIGURED_BY_PATH",
                        configuredPath);
                }
                break;
            }
        }
    }

    catch (const std::exception& e)
    {
        lg2::error("Error while finding configured associations.", "ERROR", e);
        co_return NSM_SW_ERROR;
    }
    co_return NSM_SW_SUCCESS;
}

void MctpDiscovery::discoverAndUpdateNsmDeviceTask(
    std::shared_ptr<nsm::NsmDevice> nsmDevice)
{
    requester::Coroutine::assign(nsmDevice->updateNsmDeviceTaskHandle,
                                 [&, nsmDevice]() -> requester::Coroutine {
        // coverity[missing_return]
        co_return co_await updateNsmDeviceTask(nsmDevice);
    });
}

requester::Coroutine MctpDiscovery::updateNsmDeviceTask(
    std::shared_ptr<nsm::NsmDevice> nsmDevice)
{
    auto tmpEid = nsmDevice->getEid();
    auto rc = co_await nsmDevice->updateNsmDevice();
    if (rc == NSM_SW_SUCCESS &&
        nsmDevice->getEid() ==
            tmpEid && // check if nsmDevice is not changed with new EID
        perEidQueuedMctpInfos[tmpEid].size() ==
            0 && // check if there is no pending mctp rediscovery signal for
                 // same EID
        nsmDevice
            ->isDiscoveryPending()) // check if nsmDevice is in discovery
                                    // pending state (to confirm device is not
                                    // in offline state after rediscovery)
    {
        co_await nsmDevice->setOnline();
        if (nsmDevice->getEid() == tmpEid) // check if nsmDevice is not changed
                                           // with new EID during setOnline
        {
            nsmDevice->finishDeviceDiscovery();
        }
    }
    co_return NSM_SW_SUCCESS;
}

std::shared_ptr<nsm::NsmDevice> MctpDiscovery::findOrCreateNsmDevice(
    uint8_t deviceType, uint8_t deviceRole, uint8_t instanceNumber,
    std::string remapPropName, std::vector<std::string>& remapPropValues)
{
    uint16_t staticInstanceAndRole = (deviceRole << 8) | instanceNumber;

    if (deviceMap.find(deviceType) != deviceMap.end())
    {
        if (deviceMap[deviceType].find(staticInstanceAndRole) !=
            deviceMap[deviceType].end())
        {
            return deviceMap[deviceType][staticInstanceAndRole];
        }
    }

    auto objServerShared = std::shared_ptr<sdbusplus::asio::object_server>(
        &objServer, [](auto*) {});
    auto nsmDevice = std::make_shared<nsm::NsmDevice>(
        objServerShared, nsmMsgHandler, deviceType, instanceNumber,
        remapPropName, remapPropValues, deviceRole);
    lg2::info(
        "Creating new NsmDevice for deviceType:{TYPE} instanceNumber:{INST} deviceRole:{ROLE} remapPropName:{REMAPPNAME} remapPropValue:{REMAPPVALUE}",
        "TYPE", deviceType, "INST", instanceNumber, "ROLE", deviceRole,
        "REMAPPNAME", remapPropName, "REMAPPVALUE", remapPropValues[0]);
    nsmDevices.emplace_back(nsmDevice);
    deviceMap[deviceType][staticInstanceAndRole] = nsmDevice;
    if (mapMctpEIDForNsmDevice(nsmDevice) == NSM_SW_SUCCESS)
    {
        nsmDevice->initDeviceDiscovery();
        discoverAndUpdateNsmDeviceTask(nsmDevice);
    }
    return deviceMap[deviceType][staticInstanceAndRole];
}

template <typename T>
bool MctpDiscovery::containsValue(
    const T& value,
    const std::variant<std::vector<uint8_t>, std::vector<uuid_t>>&
        remapPropValues) const
{
    if (const auto* vec = std::get_if<std::vector<T>>(&remapPropValues))
    {
        return std::find(vec->begin(), vec->end(), value) != vec->end();
    }
    return false;
}

int MctpDiscovery::mapMctpEIDForNsmDevice(
    std::shared_ptr<nsm::NsmDevice> nsmDevice)
{
    auto ret = NSM_SW_ERROR_DATA;
    for (auto& [eid, value] : discoveredEIDs)
    {
        auto& [uuid, mctpDeviceType, mctpDeviceInstanceNumber, active,
               mctpMedium, mctpBinding, associatedPath] = value;
        if (mctpDeviceType == nsmDevice->getDeviceType() &&
            nsmDevice->getDeviceRemapProp() ==
                nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER)
        {
            if (containsValue(mctpDeviceInstanceNumber,
                              nsmDevice->getDeviceRemapValues()))
            {
                nsmDevice->updateDiscoveryIdentifiers(
                    eid, uuid, mctpDeviceInstanceNumber, associatedPath,
                    mctpMedium, mctpBinding);
                ret = NSM_SW_SUCCESS;
            }
        }
        else if (mctpDeviceType == nsmDevice->getDeviceType() &&
                 nsmDevice->getDeviceRemapProp() ==
                     nsm::DeviceRemapProperty::MCTP_UUID)
        {
            if (containsValue(uuid, nsmDevice->getDeviceRemapValues()))
            {
                nsmDevice->updateDiscoveryIdentifiers(
                    eid, uuid, mctpDeviceInstanceNumber, associatedPath,
                    mctpMedium, mctpBinding);
                ret = NSM_SW_SUCCESS;
            }
        }
        else if (mctpDeviceType == nsmDevice->getDeviceType() &&
                 nsmDevice->getDeviceRemapProp() ==
                     nsm::DeviceRemapProperty::MCTP_EID)
        {
            if (containsValue(eid, nsmDevice->getDeviceRemapValues()))
            {
                nsmDevice->updateDiscoveryIdentifiers(
                    eid, uuid, mctpDeviceInstanceNumber, associatedPath,
                    mctpMedium, mctpBinding);
                ret = NSM_SW_SUCCESS;
            }
        }
        else if (mctpDeviceType == nsmDevice->getDeviceType() &&
                 nsmDevice->getDeviceRemapProp() ==
                     nsm::DeviceRemapProperty::MCTP_ASSOCIATION)
        {
            if (containsValue(associatedPath,
                              nsmDevice->getDeviceRemapValues()))
            {
                nsmDevice->updateDiscoveryIdentifiers(
                    eid, uuid, mctpDeviceInstanceNumber, associatedPath,
                    mctpMedium, mctpBinding);
                ret = NSM_SW_SUCCESS;
            }
        }
    }
    return ret;
}

std::shared_ptr<nsm::NsmDevice> MctpDiscovery::mapNsmDeviceUsingEid(
    eid_t eid, uuid_t mctpUuid, uint8_t deviceType, uint8_t instanceNumber,
    std::string associatedPath, [[__maybe_unused__]] bool active,
    MctpMedium mctpMedium, MctpBinding mctpBinding)
{
    std::shared_ptr<nsm::NsmDevice> ret{};

    if (deviceMap.find(deviceType) == deviceMap.end())
    {
        lg2::info("No NsmDevice found for Mctp Eid : {EID}", "EID", eid);
        return ret;
    }

    for (auto it : deviceMap[deviceType])
    {
        auto nsmDevice = it.second;
        if (nsmDevice->getDeviceRemapProp() ==
            nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER)
        {
            if (containsValue(instanceNumber,
                              nsmDevice->getDeviceRemapValues()))
            {
                if (nsmDevice->updateDiscoveryIdentifiers(
                        eid, mctpUuid, instanceNumber, associatedPath,
                        mctpMedium, mctpBinding))
                {
                    ret = nsmDevice;
                }
            }
        }
        else if (nsmDevice->getDeviceRemapProp() ==
                 nsm::DeviceRemapProperty::MCTP_UUID)
        {
            if (containsValue(mctpUuid, nsmDevice->getDeviceRemapValues()))
            {
                if (nsmDevice->updateDiscoveryIdentifiers(
                        eid, mctpUuid, instanceNumber, associatedPath,
                        mctpMedium, mctpBinding))
                {
                    ret = nsmDevice;
                }
            }
        }
        else if (nsmDevice->getDeviceRemapProp() ==
                 nsm::DeviceRemapProperty::MCTP_EID)
        {
            if (containsValue(eid, nsmDevice->getDeviceRemapValues()))
            {
                if (nsmDevice->updateDiscoveryIdentifiers(
                        eid, mctpUuid, instanceNumber, associatedPath,
                        mctpMedium, mctpBinding))
                {
                    ret = nsmDevice;
                }
            }
        }
        else if (nsmDevice->getDeviceRemapProp() ==
                 nsm::DeviceRemapProperty::MCTP_ASSOCIATION)
        {
            if (containsValue(associatedPath,
                              nsmDevice->getDeviceRemapValues()))
            {
                if (nsmDevice->updateDiscoveryIdentifiers(
                        eid, mctpUuid, instanceNumber, associatedPath,
                        mctpMedium, mctpBinding))
                {
                    ret = nsmDevice;
                }
            }
        }
    }

    if (!ret)
    {
        lg2::info("No NsmDevice found for Mctp Eid : {EID}", "EID", eid);
    }
    return ret;
}

std::shared_ptr<nsm::NsmDevice>
    MctpDiscovery::getNsmDeviceFromStaticUUID(uuid_t uuid)
{
    uint8_t deviceType = 0xff;
    uint8_t instanceNumber = 0xff;
    uint8_t deviceRole = NSM_DEV_ROLE_RESERVED;
    std::string remapPropName;
    std::vector<std::string> remapPropValues;
    if (utils::parseStaticUuid(uuid, deviceType, instanceNumber, deviceRole,
                               remapPropName, remapPropValues) < 0)
    {
        throw std::runtime_error(
            "MctpDiscovery::getNsmDevice: uuid in EM json is not in a valid format(STATIC:d:d:s:s), UUID=" +
            uuid);
    }

    return findOrCreateNsmDevice(deviceType, deviceRole, instanceNumber,
                                 remapPropName, remapPropValues);
}

std::shared_ptr<nsm::NsmDevice> MctpDiscovery::getNsmDeviceFromEid(eid_t eid)
{
    std::shared_ptr<nsm::NsmDevice> ret{};
    if (discoveredEIDs.find(eid) == discoveredEIDs.end())
    {
        return ret;
    }

    for (auto& nsmDevice : nsmDevices)
    {
        if (nsmDevice->getEid() == eid)
        {
            ret = nsmDevice;
            break;
        }
    }

    return ret;
}

std::shared_ptr<nsm::NsmDevice> MctpDiscovery::getNsmDeviceByIdentification(
    uint8_t deviceType, uint8_t instanceNumber, uint8_t deviceRole)
{
    std::shared_ptr<nsm::NsmDevice> ret{};
    uint16_t staticInstanceAndRole = (deviceRole << 8) | instanceNumber;
    if (deviceMap.find(deviceType) != deviceMap.end())
    {
        if (deviceMap[deviceType].find(staticInstanceAndRole) !=
            deviceMap[deviceType].end())
        {
            ret = deviceMap[deviceType][staticInstanceAndRole];
        }
    }
    return ret;
}

nsm::DiscoveryEvents& MctpDiscovery::discoveryEvents(eid_t eid)
{
    if (perEidDiscoveryEvents.find(eid) == perEidDiscoveryEvents.end())
    {
        perEidDiscoveryEvents[eid] =
            std::make_shared<nsm::DiscoveryEvents>(eid);
    }
    return *perEidDiscoveryEvents[eid];
}

void MctpDiscovery::handleMctpStateTransition(const std::string objPath,
                                              [[maybe_unused]] const bool state)
{
    eid_t eid = 0;
    size_t lastSlash = objPath.rfind('/');
    if (lastSlash == std::string::npos || lastSlash == objPath.length() - 1)
    {
        return;
    }

    std::string numberStr = objPath.substr(lastSlash + 1);
    try
    {
        eid = std::stoi(numberStr);
    }
    catch (const std::exception&)
    {
        lg2::info(
            "MctpDiscovery::handleMctpStateTransition Invalid eid parsed: {EID}",
            "EID", numberStr);
        return;
    }
    auto nsmDevice = getNsmDeviceFromEid(eid);
    if (nsmDevice)
    {
        nsmDevice->initDeviceDiscovery();
    }
    else
    {
        lg2::info(
            "MctpDiscovery::handleMctpStateTransition No NsmDevice found for Eid: {EID}",
            "EID", eid);
    }
}

requester::Coroutine MctpDiscovery::dumpPingInfoTask(eid_t eid)
{
    auto rc = co_await ping(eid);
    if (rc == NSM_SW_SUCCESS)
    {
        lg2::error("Ping succeeded for Eid: {EID}", "EID", eid);
    }
    else
    {
        lg2::error("Ping failed for Eid: {EID} rc={RC}", "EID", eid, "RC",
                   utils::nsmSwCodeToString(rc));
    }

    // coverity[missing_return]
    co_return rc;
}

} // namespace mctp
