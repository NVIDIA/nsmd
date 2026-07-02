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

#include "common/sleep.hpp"
#include "common/types.hpp"
#include "common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmd/sensorManager.hpp"
#include "progressCounters.hpp"

#include <systemd/sd-bus.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdeventplus/event.hpp>

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace mctp
{
// Bounded application-level retry budget when coSetdeviceStateOnlineTask
// returns NSM_SW_ERROR_TIMEOUT for an EID. The transport layer already
// exhausts its retries before escalating; this protects against transient
// post-reboot unresponsiveness (e.g., late MCTP link-up, device in
// firmware-update mode) where the device is alive but answers a moment late.
// Mirrors the LinearBackoffConfig defaults used by MctpEndpointProber for
// NSM_ERR_NOT_READY retries (see requester/retry_backoff_utils.hpp).
constexpr uint8_t DiscoveryTimeoutMaxRetries = 3;
constexpr uint32_t DiscoveryTimeoutRetryDelayMs = 2000;

std::unique_ptr<MctpDiscovery> mctpDiscoveryInstance;

MctpDiscovery& MctpDiscovery::getInstance()
{
    if (!mctpDiscoveryInstance)
    {
        throw std::runtime_error(
            "MctpDiscovery instance is not initialized yet");
    }
    return *mctpDiscoveryInstance;
}

void MctpDiscovery::initialize(
    sdbusplus::bus_t& bus, mctp_socket::Handler& handler,
    std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler, EidTable& eidTable,
    nsm::NsmDeviceTable& nsmDevices, sdbusplus::asio::object_server& objServer)
{
    if (mctpDiscoveryInstance)
    {
        throw std::logic_error(
            "Initialize called on an already initialized MctpDiscovery");
    }
    mctpDiscoveryInstance = std::unique_ptr<MctpDiscovery>(new MctpDiscovery(
        bus, handler, nsmMsgHandler, eidTable, nsmDevices, objServer));
    mctpDiscoveryInstance->init();
}

void MctpDiscovery::logProberSummaries()
{
    if (mctpDiscoveryInstance)
    {
        mctpDiscoveryInstance->prober.logAllSummaries();
    }
}

const std::string emptyUUID = "00000000-0000-0000-0000-000000000000";

MctpDiscovery::MctpDiscovery(
    sdbusplus::bus_t& bus, mctp_socket::Handler& handler,
    std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler, EidTable& eidTable,
    nsm::NsmDeviceTable& nsmDevices,
    sdbusplus::asio::object_server& objServer) :
    bus(bus), handler(handler), nsmMsgHandler(nsmMsgHandler),
    eidTable(eidTable), nsmDevices(nsmDevices), objServer(objServer),
    prober(nsmMsgHandler, requester::retry::LinearBackoffConfig{},
           std::bind_front(&MctpDiscovery::SendRecvNsmMsg, this))
{}

void MctpDiscovery::init()
{
    // Phase 1.A — resolve the bus-owner BEFORE installing the runtime match
    // rules so the cached service name(s) can be substituted into the
    // arg0path + sender= filters (guideline § 2.1 Phase 1.A + § 2.3 item 11).
    // Resolve failures here fall through to the catch block below where the
    // legacy unfiltered subscription is installed as a safety net — bounded
    // retry on top is added by Commit 4 (N4).
    //
    // NOTE — Commit 2 (N2) deviation from work order § 5: nsmd's
    // utils::DBusHandler does NOT currently expose a coroutine wrapper for
    // mapper.GetSubTree. Adding one would touch common/utils.{hpp,cpp}
    // (forbidden by the work order's no-cross-cut rule + escalation row 2).
    // Instead we keep the sync getSubtree primitive but move the per-service
    // GetManagedObjects + endpoint dispatch work OFF the synchronous init()
    // path into the initEnumerateTask coroutine spawned at the end of this
    // function. The constructor + initialize() entry point therefore return
    // immediately; enumeration runs on the next event-loop tick.
    try
    {
        const dbus::Interfaces ifaceList{"xyz.openbmc_project.MCTP.Endpoint"};
        auto getSubTreeResponse = utils::DBusHandler().getSubtree(
            "/au/com/codeconstruct/mctp1", 0, ifaceList);
        std::set<std::string> mctpCtrlServices;
        for (const auto& [objPath, mapperServiceMap] : getSubTreeResponse)
        {
            for (const auto& [serviceName, interfaces] : mapperServiceMap)
            {
                mctpCtrlServices.emplace(serviceName);
            }
        }
        resolvedMctpServices = mctpCtrlServices;

        // arg0path narrows the match to the MCTP networks subtree so signals
        // for any other object under /au/com/codeconstruct/mctp1 (network /
        // interface children) do NOT wake the daemon. sender= further
        // narrows to the cached bus-owner so unrelated bus members publishing
        // under the same path namespace cannot trigger the callback.
        // Same shape as pldm's commit 8e693a40, but with the cached service
        // name from GetSubTree rather than a hardcoded constant.
        const std::string mctpNetworksPath =
            "/au/com/codeconstruct/mctp1/networks/";

        auto addedRule = sdbusplus::bus::match::rules::interfacesAddedAtPath(
            mctpNetworksPath);
        auto removedRule =
            sdbusplus::bus::match::rules::interfacesRemovedAtPath(
                mctpNetworksPath);
        if (resolvedMctpServices.size() == 1)
        {
            // Single bus-owner — add sender= narrowing. This is the common
            // case (au.com.codeconstruct.MCTP1 is the canonical owner today).
            const auto& service = *resolvedMctpServices.begin();
            addedRule += sdbusplus::bus::match::rules::sender(service);
            removedRule += sdbusplus::bus::match::rules::sender(service);
            lg2::info(
                "MctpDiscovery: subscribing with arg0path={PATH} sender={SVC}",
                "PATH", mctpNetworksPath, "SVC", service);
        }
        else
        {
            // Zero or >1 bus owners — skip sender= narrowing. With 0 we let
            // bounded retry from N4 cover the empty-resolve case; with >1 we
            // accept the slightly broader match rather than installing a
            // per-service rule explosion. arg0path narrowing still applies.
            lg2::info(
                "MctpDiscovery: subscribing with arg0path={PATH} (sender unset, services={N})",
                "PATH", mctpNetworksPath, "N",
                static_cast<int>(resolvedMctpServices.size()));
        }
        mctpEndpointAddedSignal.emplace(
            bus, addedRule,
            std::bind_front(&MctpDiscovery::discoverEndpoints, this));
        mctpEndpointRemovedSignal.emplace(
            bus, removedRule,
            std::bind_front(&MctpDiscovery::cleanEndpoints, this));
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "MctpDiscovery::init: bus-owner resolve / match install failed; "
            "falling back to unfiltered match (degraded fallback). {ERR}",
            "ERR", e);
        // Fall back to the legacy unfiltered subscription shape so we still
        // receive subsequent runtime signals even if the initial mapper
        // round-trip failed. Bounded retry from Commit 4 (N4) will layer
        // recovery on top of this path.
        try
        {
            mctpEndpointAddedSignal.emplace(
                bus,
                sdbusplus::bus::match::rules::interfacesAdded(
                    "/au/com/codeconstruct/mctp1"),
                std::bind_front(&MctpDiscovery::discoverEndpoints, this));
            mctpEndpointRemovedSignal.emplace(
                bus,
                sdbusplus::bus::match::rules::interfacesRemoved(
                    "/au/com/codeconstruct/mctp1"),
                std::bind_front(&MctpDiscovery::cleanEndpoints, this));
        }
        catch (const std::exception& fallbackErr)
        {
            lg2::error(
                "MctpDiscovery::init: fallback match install also failed; daemon will not receive runtime MCTP signals. {ERR}",
                "ERR", fallbackErr);
        }
        // No GetManagedObjects round can succeed without a resolved service
        // set. Spawn the coroutine anyway so Commit 4 (N4)'s bounded retry
        // layer can pick up here when added.
    }

    // Spawn the post-construction enumeration on the event loop and return
    // immediately. The MctpDiscovery instance is owned by the unique_ptr in
    // mctpDiscoveryInstance which outlives every coroutine here (the daemon
    // process lifetime exceeds discovery enumeration by design).
    requester::Coroutine::assign(initEnumerateTaskHandle,
                                 [this]() -> requester::Coroutine {
        // coverity[missing_return]
        co_return co_await initEnumerateTask();
    });
}

requester::Coroutine MctpDiscovery::retryResolveBusOwner()
{
    const auto backoff = getMapperRetryBackoff();
    auto event = sdeventplus::Event::get_default();
    const dbus::Interfaces ifaceList{"xyz.openbmc_project.MCTP.Endpoint"};
    for (size_t attempt = 0; attempt < backoff.size(); ++attempt)
    {
        try
        {
            auto resp = utils::DBusHandler().getSubtree(
                "/au/com/codeconstruct/mctp1", 0, ifaceList);
            if (!resp.empty())
            {
                std::set<std::string> newServices;
                for (const auto& [objPath, mapperServiceMap] : resp)
                {
                    for (const auto& [serviceName, ifaces] : mapperServiceMap)
                    {
                        newServices.emplace(serviceName);
                    }
                }
                if (!newServices.empty())
                {
                    resolvedMctpServices = newServices;
                    lg2::info(
                        "retryResolveBusOwner: resolved {N} service(s) on attempt {ATTEMPT}",
                        "N", static_cast<int>(newServices.size()), "ATTEMPT",
                        static_cast<int>(attempt + 1));
                    co_return NSM_SW_SUCCESS;
                }
            }
            lg2::error(
                "retryResolveBusOwner: empty mapper response attempt={ATTEMPT}",
                "ATTEMPT", static_cast<int>(attempt + 1));
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "retryResolveBusOwner: attempt {ATTEMPT} threw, sleeping then retrying. {ERR}",
                "ATTEMPT", static_cast<int>(attempt + 1), "ERR", e);
        }
        co_await common::Sleep(
            event, static_cast<uint64_t>(backoff[attempt].count()) * 1000,
            common::NonPriority);
    }
    lg2::error(
        "retryResolveBusOwner: exhausted {N} attempts; resolvedMctpServices stays empty",
        "N", static_cast<int>(backoff.size()));
    co_return NSM_SW_ERROR;
}

requester::Coroutine
    MctpDiscovery::retryGetManagedObjects(std::string service,
                                          dbus::ObjectValueTree& outObjects)
{
    const auto backoff = getMapperRetryBackoff();
    auto event = sdeventplus::Event::get_default();
    for (size_t attempt = 0; attempt < backoff.size(); ++attempt)
    {
        try
        {
            auto method = bus.new_method_call(
                service.c_str(), "/au/com/codeconstruct/mctp1",
                "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");
            auto reply = bus.call(method);
            reply.read(outObjects);
            co_return NSM_SW_SUCCESS;
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "retryGetManagedObjects: attempt {ATTEMPT} for service={SVC} threw, sleeping then retrying. {ERR}",
                "ATTEMPT", static_cast<int>(attempt + 1), "SVC", service, "ERR",
                e);
        }
        co_await common::Sleep(
            event, static_cast<uint64_t>(backoff[attempt].count()) * 1000,
            common::NonPriority);
    }
    lg2::error(
        "retryGetManagedObjects: exhausted {N} attempts for service={SVC}", "N",
        static_cast<int>(backoff.size()), "SVC", service);
    co_return NSM_SW_ERROR;
}

requester::Coroutine MctpDiscovery::initEnumerateTask()
{
    MctpInfos mctpInfos;
    bool anyServiceRoundSucceeded = false;

    // If init()'s initial sync resolve produced no services (mapper was
    // unhealthy at boot), retry with bounded backoff before giving up.
    // This closes the bug 5533307 nsm-side / 5922299 path — guideline
    // § 2.2 mandatory item 7.
    if (resolvedMctpServices.empty())
    {
        lg2::info(
            "initEnumerateTask: resolvedMctpServices empty after init() — invoking bounded retry");
        co_await retryResolveBusOwner();
    }

    // resolvedMctpServices was populated synchronously in init() above
    // (under the same try-block as the subscription install). If still
    // empty here, the bounded retry exhausted — the catch-block in init()
    // already invoked the degraded subscription-only fallback, and
    // mctpDiscoveryComplete stays false below.
    for (const auto& service : resolvedMctpServices)
    {
        dbus::ObjectValueTree objects{};
        auto rc = co_await retryGetManagedObjects(service, objects);
        if (rc != NSM_SW_SUCCESS)
        {
            // Bounded retry exhausted for this service — skip it.
            continue;
        }
        // A successful GetManagedObjects round — even with zero objects —
        // counts as a truthful enumeration per guideline § 2.2 mandatory
        // item 6. Mapper-failed states never reach this point.
        anyServiceRoundSucceeded = true;
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

    // Readiness gate (guideline § 2.2 mandatory item 6) — only flip the
    // flag once at least one service round succeeded. An all-failed round
    // (mapper unhealthy, resolvedMctpServices empty due to upstream
    // resolve failure) keeps mctpDiscoveryComplete=false so external
    // consumers do not treat nsmd as "0 endpoints, ready".
    if (anyServiceRoundSucceeded)
    {
        mctpDiscoveryComplete = true;
    }
    else
    {
        lg2::error(
            "initEnumerateTask: no service round succeeded; mctpDiscoveryComplete stays false");
    }

    // Suppress the legacy empty-list discoverNsmDevice publication when no
    // round succeeded — that is the "0 endpoints, ready" anti-pattern. When
    // at least one round succeeded (even with an empty endpoint set — the
    // legitimate "mapper healthy, no peers connected" state), publish.
    if (anyServiceRoundSucceeded || !mctpInfos.empty())
    {
        discoverNsmDevice(mctpInfos);
    }
    co_return NSM_SW_SUCCESS;
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
                std::optional<eid_t> localEid;
                if (properties.contains("LocalEID"))
                {
                    localEid = std::get<uint8_t>(properties.at("LocalEID"));
                }
                if (std::find(mctpTypes.begin(), mctpTypes.end(),
                              mctpTypeVDM) != mctpTypes.end())
                {
                    handler.registerMctpEndpoint(eid, type, protocol, address);
                    cachedMctpInfoByPath[objPath] =
                        std::make_tuple(eid, uuid, mediumType, networkId,
                                        bindingType, active, objPath, localEid);
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
        // Belt around the per-signal dispatch — any uncaught
        // sdbusplus::exception_t / std::bad_variant_access from inside the
        // handle*Endpoints coroutines previously escaped to the event loop
        // and triggered std::terminate (guideline § 2.2 mandatory item 5,
        // bug 5448615-adjacent). Log, drop the offending signal, continue
        // pumping the queue.
        try
        {
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
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "deviceStateChangeTask: handler threw on member={MEMBER} PATH={OBJ_PATH}, dropping signal. {ERR}",
                "MEMBER", member, "OBJ_PATH", objPath, "ERR", e);
        }
        discoverNsmDevice(mctpInfos);
        mctpQueuedSignals[objPath].pop();
        lg2::info(
            "deviceStateChangeTask: mctpQueuedSignals for PATH={OBJ_PATH} size= {SIZE}",
            "OBJ_PATH", objPath, "SIZE", mctpQueuedSignals[objPath].size());
    }
    co_return NSM_SW_SUCCESS;
}

#ifdef ENABLE_ASSOCIATION_DISCOVERY

requester::Coroutine
    MctpDiscovery::readMctpProperties(const std::string& objPath,
                                      MctpInfos& mctpInfos)
{
    dbus::Interfaces interfaces{mctpEndpointIntfName};
    dbus::PropertyMap allProperties;
    try
    {
        auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                              interfaces);
        if (mapperResponse.size() == 0)
        {
            lg2::error(
                "readMctpProperties: coGetServiceMap failed for PATH={OBJ_PATH}",
                "OBJ_PATH", objPath);
            co_return NSM_SW_ERROR;
        }
        std::string service = mapperResponse.begin()->first;
        lg2::info("service of PATH={OBJ_PATH} is {SERVICE}", "OBJ_PATH",
                  objPath, "SERVICE", service);
        allProperties = co_await utils::coGetAllDbusProperty(service, objPath);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "readMctpProperties: failed to get MctpInfo from PATH={OBJ_PATH},{ERROR}",
            "OBJ_PATH", objPath, "ERROR", e);
        co_return NSM_SW_ERROR;
    }

    uint8_t eid{};
    std::string connectivity{};
    uint32_t networkId{};
    std::string mediumType{};
    std::string uuid{};
    std::string bindingType{};
    std::vector<uint8_t> mctpTypes{};

    if (allProperties.contains("EID"))
    {
        eid = std::get<uint8_t>(allProperties.at("EID"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: EID property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if constexpr (FILTER_MCTP_EID)
    {
        // MCTP EID 0 is a special Null EID as per MCTP DMTF
        // specification doc
        if (eid == MCTP_EID_TO_FILTER)
        {
            lg2::error(
                "readMctpProperties: EID {EID}==MCTP_EID_TO_FILTER for PATH={OBJ_PATH}",
                "EID", eid, "OBJ_PATH", objPath);
            co_return NSM_SW_ERROR;
        }
    }
    if (allProperties.contains("Connectivity"))
    {
        connectivity = std::get<std::string>(allProperties.at("Connectivity"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: Connectivity property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("NetworkId"))
    {
        networkId = std::get<uint32_t>(allProperties.at("NetworkId"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: NetworkId property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("MediumType"))
    {
        mediumType = std::get<std::string>(allProperties.at("MediumType"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: MediumType property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        // Not a mandatory property as per upstream guidelines
    }

    if (allProperties.contains("UUID"))

    {
        uuid = std::get<std::string>(allProperties.at("UUID"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: UUID property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("BindingType"))
    {
        bindingType = std::get<std::string>(allProperties.at("BindingType"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: BindingType property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        // Not a mandatory property as per upstream guidelines
    }
    if (allProperties.contains("SupportedMessageTypes"))
    {
        mctpTypes = std::get<std::vector<uint8_t>>(
            allProperties.at("SupportedMessageTypes"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: SupportedMessageTypes property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    std::optional<eid_t> localEid;
    if (allProperties.contains("LocalEID"))
    {
        localEid = std::get<uint8_t>(allProperties.at("LocalEID"));
    }

    MctpInfo mctpInfo =
        std::make_tuple(eid, uuid, mediumType, networkId, bindingType,
                        (connectivity == "Available"), objPath, localEid);
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
                "readMctpProperties: mctpTypeVDM command not supported for PATH={OBJ_PATH}",
                "OBJ_PATH", objPath);
        }
    }
    else
    {
        mctpInfos.push_back(mctpInfo);
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    MctpDiscovery::handleDiscoverEndpoints(sdbusplus::message::message& msg,
                                           MctpInfos& mctpInfos)
{
    sdbusplus::object_path objPath;
    dbus::InterfaceMap interfaces;
    msg.read(objPath, interfaces);
    if (interfaces.find(std::string(mctpEndpointIntfName)) != interfaces.end())
    {
        populateMctpInfo(interfaces, objPath.str, mctpInfos);
    }
    else
    {
        handleMctpStateTransition(objPath);
        co_await readMctpProperties(objPath.str, mctpInfos);
    }

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
    sdbusplus::object_path objPath;
    dbus::InterfaceMap interfaces;
    msg.read(objPath, interfaces);
    sd_bus_message_rewind(msg.get(), true);

    if (interfaces.find(std::string(mctpEndpointIntfName)) !=
            interfaces.end() ||
        interfaces.find(std::string(associationIntfName)) != interfaces.end())
    {
        std::string foundIntf = interfaces.find(std::string(
                                    mctpEndpointIntfName)) != interfaces.end()
                                    ? mctpEndpointIntfName
                                    : associationIntfName;

        lg2::info(
            "MctpDiscovery: Recieved InterfacesAdded signal for objPath={OBJ_PATH} and interface = {INTF}",
            "OBJ_PATH", objPath.str, "INTF", foundIntf);
        mctpQueuedSignals[objPath.str].emplace(msg);
        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath.str],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath.str);
        });
    }
}

#else

requester::Coroutine
    MctpDiscovery::readMctpProperties(const std::string& objPath,
                                      MctpInfos& mctpInfos)
{
    dbus::Interfaces interfaces{mctpEndpointIntfName};
    dbus::PropertyMap allProperties;
    try
    {
        auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                              interfaces);
        if (mapperResponse.size() == 0)
        {
            lg2::error(
                "readMctpProperties: coGetServiceMap failed for PATH={OBJ_PATH}",
                "OBJ_PATH", objPath);
            co_return NSM_SW_ERROR;
        }
        std::string service = mapperResponse.begin()->first;
        lg2::info("service of PATH={OBJ_PATH} is {SERVICE}", "OBJ_PATH",
                  objPath, "SERVICE", service);
        allProperties = co_await utils::coGetAllDbusProperty(service, objPath);
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "readMctpProperties: failed to get MctpInfo from PATH={OBJ_PATH},{ERROR}",
            "OBJ_PATH", objPath, "ERROR", e);
        co_return NSM_SW_ERROR;
    }

    uint8_t eid{};
    std::string connectivity{};
    uint32_t networkId{};
    std::string mediumType{};
    std::string uuid{};
    std::string bindingType{};
    std::vector<uint8_t> mctpTypes{};

    if (allProperties.contains("EID"))
    {
        eid = std::get<uint8_t>(allProperties.at("EID"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: EID property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if constexpr (FILTER_MCTP_EID)
    {
        // MCTP EID 0 is a special Null EID as per MCTP DMTF
        // specification doc
        if (eid == MCTP_EID_TO_FILTER)
        {
            lg2::error(
                "readMctpProperties: EID {EID}==MCTP_EID_TO_FILTER for PATH={OBJ_PATH}",
                "EID", eid, "OBJ_PATH", objPath);
            co_return NSM_SW_ERROR;
        }
    }
    if (allProperties.contains("Connectivity"))
    {
        connectivity = std::get<std::string>(allProperties.at("Connectivity"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: Connectivity property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("NetworkId"))
    {
        networkId = std::get<uint32_t>(allProperties.at("NetworkId"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: NetworkId property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("MediumType"))
    {
        mediumType = std::get<std::string>(allProperties.at("MediumType"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: MediumType property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("UUID"))

    {
        uuid = std::get<std::string>(allProperties.at("UUID"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: UUID property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    if (allProperties.contains("BindingType"))
    {
        bindingType = std::get<std::string>(allProperties.at("BindingType"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: BindingType property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }
    if (allProperties.contains("SupportedMessageTypes"))
    {
        mctpTypes = std::get<std::vector<uint8_t>>(
            allProperties.at("SupportedMessageTypes"));
    }
    else
    {
        lg2::error(
            "readMctpProperties: SupportedMessageTypes property not found for PATH={OBJ_PATH}",
            "OBJ_PATH", objPath);
        co_return NSM_ERR_INVALID_DATA;
    }

    std::optional<eid_t> localEid;
    if (allProperties.contains("LocalEID"))
    {
        localEid = std::get<uint8_t>(allProperties.at("LocalEID"));
    }

    MctpInfo mctpInfo =
        std::make_tuple(eid, uuid, mediumType, networkId, bindingType,
                        (connectivity == "Available"), objPath, localEid);

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
                "readMctpProperties: mctpTypeVDM command not supported for PATH={OBJ_PATH}",
                "OBJ_PATH", objPath);
        }
    }
    else
    {
        mctpInfos.push_back(mctpInfo);
    }
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine
    MctpDiscovery::handleDiscoverEndpoints(sdbusplus::message::message& msg,
                                           MctpInfos& mctpInfos)
{
    sdbusplus::object_path objPath;
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
    sdbusplus::object_path objPath;
    dbus::InterfaceMap interfaces;
    msg.read(objPath, interfaces);
    sd_bus_message_rewind(msg.get(), true);

    if (interfaces.find(std::string(mctpEndpointIntfName)) != interfaces.end())
    {
        lg2::info(
            "MctpDiscovery: Recieved InterfacesAdded signal for objPath={OBJ_PATH} and interface = {INTF}",
            "OBJ_PATH", objPath.str, "INTF", mctpEndpointIntfName);
        mctpQueuedSignals[objPath.str].emplace(msg);
        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath.str],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath.str);
        });
    }
}

#endif

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
        handleMctpStateTransition(objPath);

        co_await readMctpProperties(objPath, mctpInfos);
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
    sdbusplus::object_path objPath;
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

#ifdef ENABLE_ASSOCIATION_DISCOVERY
void MctpDiscovery::cleanEndpoints(
    [[maybe_unused]] sdbusplus::message::message& msg)
{
    sdbusplus::object_path objPath;
    std::vector<std::string> interfaces;
    msg.read(objPath, interfaces);
    sd_bus_message_rewind(msg.get(), true);
    if (std::find(interfaces.begin(), interfaces.end(),
                  std::string(mctpEndpointIntfName)) != interfaces.end() ||
        std::find(interfaces.begin(), interfaces.end(),
                  std::string(associationIntfName)) != interfaces.end())
    {
        std::string foundIntf = std::find(interfaces.begin(), interfaces.end(),
                                          std::string(mctpEndpointIntfName)) !=
                                        interfaces.end()
                                    ? mctpEndpointIntfName
                                    : associationIntfName;
        lg2::info(
            "MctpDiscovery: Recieved InterfacesRemoved signal for objPath={OBJ_PATH} intf = {INTF}",
            "OBJ_PATH", objPath.str, "INTF", foundIntf);
        mctpQueuedSignals[objPath.str].emplace(msg);
        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath.str],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath.str);
        });
    }
}

#else

void MctpDiscovery::cleanEndpoints(
    [[maybe_unused]] sdbusplus::message::message& msg)
{
    sdbusplus::object_path objPath;
    std::vector<std::string> interfaces;
    msg.read(objPath, interfaces);
    sd_bus_message_rewind(msg.get(), true);
    if (std::find(interfaces.begin(), interfaces.end(),
                  std::string(mctpEndpointIntfName)) != interfaces.end())
    {
        lg2::info(
            "MctpDiscovery: Recieved InterfacesRemoved signal for objPath={OBJ_PATH} intf = {INTF}",
            "OBJ_PATH", objPath.str, "INTF", mctpEndpointIntfName);
        mctpQueuedSignals[objPath.str].emplace(msg);
        requester::Coroutine::assign(deviceStateChangeTaskHandles[objPath.str],
                                     [&, objPath]() -> requester::Coroutine {
            // coverity[missing_return]
            co_return co_await deviceStateChangeTask(objPath.str);
        });
    }
}

#endif

requester::Coroutine
    MctpDiscovery::SendRecvNsmMsg(eid_t eid, Request& request,
                                  std::shared_ptr<const nsm_msg>& responseMsg,
                                  size_t* responseLen)
{
    // Belt around the transport co_await — sdbusplus / mctp transport
    // exceptions previously escaped to the spawned-coroutine caller
    // and triggered std::terminate (guideline § 2.2 mandatory item 5).
    try
    {
        auto rc = co_await nsmMsgHandler->SendRecvNsmMsg(
            eid, request, responseMsg, responseLen);
        if (rc)
        {
            lg2::error(
                "MctpDiscovery::SendRecvNsmMsg failed. eid={EID} rc={RC}",
                "EID", eid, "RC", utils::nsmSwCodeToString(rc));
        }
        co_return rc;
    }
    catch (const std::exception& e)
    {
        lg2::error("MctpDiscovery::SendRecvNsmMsg threw on eid={EID}. {ERR}",
                   "EID", eid, "ERR", e);
        co_return NSM_SW_ERROR;
    }
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
        // Belt around the per-iteration coSetdeviceState* dispatch — any
        // uncaught exception from the inner transport / NSM code previously
        // escaped to the coroutine caller and triggered std::terminate
        // (guideline § 2.2 mandatory item 5). On throw, log, pop the stale
        // snapshot, continue the loop — never abort the task.
        try
        {
            if (active)
            {
                auto rc = co_await coSetdeviceStateOnlineTask(mctpInfos);
                discoveryEvents(eid).setValue(
                    nsm::DiscoveryEventType::SetDeviceStateOnline, rc);
                if (rc == NSM_SW_ERROR_TIMEOUT &&
                    perEidDiscoveryTimeoutRetries[eid] <
                        DiscoveryTimeoutMaxRetries)
                {
                    ++perEidDiscoveryTimeoutRetries[eid];
                    lg2::info(
                        "discoverNsmDeviceTask: ping/QDI timeout, re-queueing eid={EID} attempt={ATTEMPT}/{MAX} delayMs={DELAY}",
                        "EID", eid, "ATTEMPT",
                        perEidDiscoveryTimeoutRetries[eid], "MAX",
                        DiscoveryTimeoutMaxRetries, "DELAY",
                        DiscoveryTimeoutRetryDelayMs);
                    auto event = sdeventplus::Event::get_default();
                    co_await common::Sleep(
                        event,
                        static_cast<uint64_t>(DiscoveryTimeoutRetryDelayMs) *
                            1000,
                        common::NonPriority);
                    // A newer transition for this EID may have been queued
                    // while we slept (e.g. InterfacesRemoved or
                    // Connectivity=Unavailable). The snapshot we just processed
                    // is still at the front (popped below), so size() > 1 means
                    // a fresher state is already pending. Re-queueing the stale
                    // active snapshot would re-probe the EID after that newer
                    // (possibly offline) transition runs, clobbering it. Only
                    // retry while this snapshot is still the latest known state
                    // for the EID.
                    if (perEidQueuedMctpInfos[eid].size() == 1)
                    {
                        perEidQueuedMctpInfos[eid].emplace(mctpInfo);
                    }
                    else
                    {
                        lg2::info(
                            "discoverNsmDeviceTask: newer transition queued during retry sleep, dropping stale retry eid={EID}",
                            "EID", eid);
                        perEidDiscoveryTimeoutRetries.erase(eid);
                    }
                }
                else
                {
                    if (rc == NSM_SW_ERROR_TIMEOUT)
                    {
                        lg2::error(
                            "discoverNsmDeviceTask: timeout retry budget exhausted, eid={EID} attempts={ATTEMPTS}",
                            "EID", eid, "ATTEMPTS",
                            perEidDiscoveryTimeoutRetries[eid]);
                    }
                    perEidDiscoveryTimeoutRetries.erase(eid);
                }
            }
            else
            {
                auto rc = co_await coSetdeviceStateOfflineTask(mctpInfos);
                discoveryEvents(eid).setValue(
                    nsm::DiscoveryEventType::SetDeviceStateOffline, rc);
                perEidDiscoveryTimeoutRetries.erase(eid);
            }
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "discoverNsmDeviceTask: inner coSetdeviceState* threw for eid={EID}, dropping snapshot. {ERR}",
                "EID", eid, "ERR", e);
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
               mctpObjPath, localEid] = mctpInfo;
        // Per-iteration belt — uncaught exceptions from ping / QDI /
        // updateNsmDevice / setOnline previously propagated to the calling
        // discoverNsmDeviceTask coroutine and onward to std::terminate
        // (guideline § 2.2 mandatory item 5). Log + continue keeps the
        // batch alive for remaining endpoints.
        try
        {
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
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "NSM getQueryDeviceIdentification failed, rc={RC} eid={EID}",
                    "RC", utils::nsmSwCodeToString(rc), "EID", eid);
                overallRC = rc;
                continue;
            }

            std::string configuredPath = "";
            rc = co_await findConfiguredAssociations(mctpObjPath,
                                                     configuredPath);

            // save the nsm device identification info (localEid from MCTP,
            // nullopt if not provided)
            discoveredEIDs[eid] =
                {
                    mctpUuid,       deviceType, instanceNumber,
                    true,           mctpMedium, mctpBinding,
                    configuredPath, localEid}; // std::optional - no
                                               // assumption when absent
            auto nsmDevice = mapNsmDeviceUsingEid(
                eid, mctpUuid, deviceType, instanceNumber, configuredPath, true,
                mctpMedium, mctpBinding, localEid);
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
                        eid) // check if there is no pending mctp rediscovery
                             // signal for same EID and nsmDevice is not changed
                             // with new EID during updateNsmDevice
                {
                    co_await nsmDevice->setOnline();
                    if (nsmDevice->getEid() ==
                        eid) // check if nsmDevice is not changed with new EID
                             // during setOnline
                    {
                        if (perEidQueuedMctpInfos[eid].size() == 1)
                        {
                            nsmDevice->finishDeviceDiscovery();
                        }
                        else
                        {
                            lg2::info(
                                "coSetdeviceStateOnlineTask : signal still in queue for eid= {EID}, marking device as discovery pending",
                                "EID", eid);
                            nsmDevice->initDeviceDiscovery();
                        }
                    }
                }
            }
            // update eid table [from UUID from MCTP dbus property]
            insertIntoEidTableifNotExist(
                mctpUuid, std::make_tuple(eid, mctpMedium, mctpBinding));
        }
        catch (const std::exception& e)
        {
            lg2::error(
                "coSetdeviceStateOnlineTask: inner step threw for eid={EID}, skipping endpoint. {ERR}",
                "EID", eid, "ERR", e);
            overallRC = NSM_SW_ERROR;
            continue;
        }
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
        // Per-iteration belt — uncaught exceptions from mapNsmDeviceUsingEid
        // and the nested setOffline coroutine previously escaped to the
        // caller and triggered std::terminate.
        try
        {
            if (discoveredEIDs.find(eid) != discoveredEIDs.end())
            {
                auto& value = discoveredEIDs[eid];
                std::get<3>(value) = false; // set EID is inactive
                auto& [uuid, mctpDeviceType, mctpDeviceInstanceNumber, active,
                       mctpMedium, mctpBinding, associatedPath,
                       localEid] = value;
                nsmDevice = mapNsmDeviceUsingEid(
                    eid, uuid, mctpDeviceType, mctpDeviceInstanceNumber,
                    associatedPath, false, mctpMedium, mctpBinding, localEid);
                discoveryEvents(eid).setValue(
                    nsm::DiscoveryEventType::OfflineMapNsmDeviceUsingEid,
                    nsmDevice ? 1 : 0);
            }

            if (nsmDevice)
            {
                co_await nsmDevice->setOffline();
                if (perEidQueuedMctpInfos[eid].size() == 1 &&
                    nsmDevice->getEid() ==
                        eid) // check if nsmDevice is not changed
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
        catch (const std::exception& e)
        {
            lg2::error(
                "coSetdeviceStateOfflineTask: inner step threw for eid={EID}, skipping endpoint. {ERR}",
                "EID", eid, "ERR", e);
            continue;
        }
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine MctpDiscovery::ping(eid_t eid)
{
    // coverity[missing_return]
    co_return co_await prober.ping(eid);
}

requester::Coroutine MctpDiscovery::getQueryDeviceIdentification(
    eid_t eid, uint8_t& deviceIdentification, uint8_t& deviceInstance)
{
    // coverity[missing_return]
    co_return co_await prober.getQueryDeviceIdentification(
        eid, deviceIdentification, deviceInstance);
}

requester::Coroutine MctpDiscovery::findConfiguredAssociations(
    [[maybe_unused]] const std::string& objPath,
    [[maybe_unused]] std::string& configuredPath)
{
#ifdef ENABLE_ASSOCIATION_DISCOVERY
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
#endif
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
               mctpMedium, mctpBinding, associatedPath, localEid] = value;
        if (mctpDeviceType == nsmDevice->getDeviceType() &&
            nsmDevice->getDeviceRemapProp() ==
                nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER)
        {
            if (containsValue(mctpDeviceInstanceNumber,
                              nsmDevice->getDeviceRemapValues()))
            {
                nsmDevice->updateDiscoveryIdentifiers(
                    eid, uuid, mctpDeviceInstanceNumber, associatedPath,
                    mctpMedium, mctpBinding, localEid);
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
                    mctpMedium, mctpBinding, localEid);
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
                    mctpMedium, mctpBinding, localEid);
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
                    mctpMedium, mctpBinding, localEid);
                ret = NSM_SW_SUCCESS;
            }
        }
    }
    return ret;
}

std::shared_ptr<nsm::NsmDevice> MctpDiscovery::mapNsmDeviceUsingEid(
    eid_t eid, uuid_t mctpUuid, uint8_t deviceType, uint8_t instanceNumber,
    std::string associatedPath, [[__maybe_unused__]] bool active,
    MctpMedium mctpMedium, MctpBinding mctpBinding,
    std::optional<eid_t> localEid)
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
                        mctpMedium, mctpBinding, localEid))
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
                        mctpMedium, mctpBinding, localEid))
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
                        mctpMedium, mctpBinding, localEid))
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
                        mctpMedium, mctpBinding, localEid))
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

void MctpDiscovery::handleMctpStateTransition(const std::string objPath)
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
