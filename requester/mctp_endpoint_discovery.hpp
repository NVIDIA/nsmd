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

#include "config.h"

#include "common/types.hpp"
#include "nsmd/nsmDevice.hpp"
#include "nsmd/socket_handler.hpp"
#include "requester/mctp_endpoint_prober.hpp"
#include "requester/retry_backoff_utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <vector>
// # define ENABLE_ASSOCIATION_DISCOVERY
namespace mctp
{

using Active = bool;
using DeviceType = uint8_t;
using DeviceRole = uint8_t;
using InstanceNumber = uint8_t;
using MctpMedium = std::string;
using MctpBinding = std::string;
using ConfiguredPath = std::string;
using DiscoveredEIDs =
    std::map<eid_t,
             std::tuple<uuid_t, DeviceType, InstanceNumber, Active, MctpMedium,
                        MctpBinding, ConfiguredPath, std::optional<eid_t>>>;
using EidTable =
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>;
using RequesterHandler = requester::Handler<requester::Request>;
class MctpDiscoveryTestAccess; // forward declaration for test access

class MctpDiscovery
{
    friend class MctpDiscoveryTestAccess;

  public:
    MctpDiscovery() = delete;
    MctpDiscovery(const MctpDiscovery&) = delete;
    MctpDiscovery(MctpDiscovery&&) = delete;
    MctpDiscovery& operator=(const MctpDiscovery&) = delete;
    MctpDiscovery& operator=(MctpDiscovery&&) = delete;
    virtual ~MctpDiscovery() = default;

    static MctpDiscovery& getInstance();

    static void
        initialize(sdbusplus::bus_t& bus, mctp_socket::Handler& handler,
                   std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler,
                   EidTable& eidTable, nsm::NsmDeviceTable& nsmDevices,
                   sdbusplus::asio::object_server& objServer);

    virtual std::shared_ptr<nsm::NsmDevice>
        getNsmDeviceFromStaticUUID(uuid_t uuid);
    virtual std::shared_ptr<nsm::NsmDevice> getNsmDeviceFromEid(eid_t eid);
    virtual std::shared_ptr<nsm::NsmDevice>
        getNsmDeviceByIdentification(uint8_t deviceType, uint8_t instanceNumber,
                                     uint8_t deviceRole);
    virtual nsm::DiscoveryEvents& discoveryEvents(eid_t eid);
    virtual requester::Coroutine dumpPingInfoTask(eid_t eid);

    static void logProberSummaries();

  protected:
    /** @brief Constructs the MCTP Discovery object.
     *  Does NOT perform D-Bus discovery — call init() after construction.
     */
    explicit MctpDiscovery(
        sdbusplus::bus_t& bus, mctp_socket::Handler& handler,
        std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler,
        EidTable& eidTable, nsm::NsmDeviceTable& nsmDevices,
        sdbusplus::asio::object_server& objServer);

    /** @brief Performs D-Bus signal registration and initial endpoint
     *         discovery. Called by initialize() after construction.
     *         Virtual so tests can override to skip D-Bus operations.
     */
    virtual void init();

  private:
    /** @brief reference to the systemd bus */
    sdbusplus::bus_t& bus;
    mctp_socket::Handler& handler;
    std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler;
    EidTable& eidTable;
    nsm::NsmDeviceTable& nsmDevices;
    sdbusplus::asio::object_server& objServer;
    // Dedicated prober to handle ping/query with backoff
    requester::MctpEndpointProber prober;
    /** @brief Used to watch for new MCTP endpoints */
    std::optional<sdbusplus::bus::match_t> mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    std::optional<sdbusplus::bus::match_t> mctpEndpointRemovedSignal;

    /** @brief map of Queue to store the pending property change signal from
     * mctp service */
    std::map<std::string, std::queue<sdbusplus::message::message>>
        mctpQueuedSignals;

    std::map<std::string, std::coroutine_handle<>> deviceStateChangeTaskHandles;
    requester::Coroutine deviceStateChangeTask(const std::string path);

    requester::Coroutine readMctpProperties(const std::string& objPath,
                                            MctpInfos& mctpInfos);

    void discoverEndpoints(sdbusplus::message::message& msg);
    requester::Coroutine
        handleDiscoverEndpoints(sdbusplus::message::message& msg,
                                MctpInfos& mctpInfos);

    /**
     * @brief matcher rule for property changes of
     * xyz.openbmc_project.Object.Enable dbus object
     */
    std::map<std::string, sdbusplus::bus::match_t> enableMatches;

    /** @brief handler for mctpEndpointRemovedSignal */
    void cleanEndpoints(sdbusplus::message::message& msg);
    requester::Coroutine handleCleanEndpoints(sdbusplus::message::message& msg,
                                              MctpInfos& mctpInfos);

    requester::Coroutine
        handleRefreshEndpoints(sdbusplus::message::message& msg,
                               MctpInfos& mctpInfos);
    /**
     * @brief A callback for propertiesChanges signal enabled matches matcher
     * rule to invoke registered handlers (online/offline mctp endpoint)
     */
    void refreshEndpoints(sdbusplus::message::message& msg);

    /** @brief Process the D-Bus MCTP endpoint info and prepare data to be used
     *         for NSM discovery.
     *
     *  @param[in] interfaces - MCTP D-Bus information
     *  @param[out] mctpInfos - MCTP info for NSM discovery
     */
    void populateMctpInfo(const dbus::InterfaceMap& interfaces,
                          const std::string& objPath, MctpInfos& mctpInfos);

    static constexpr uint8_t mctpTypeVDM = 0x7e;

    /** @brief MCTP endpoint interface name */
    const std::string mctpEndpointIntfName{"xyz.openbmc_project.MCTP.Endpoint"};

    const std::string mctpBindingIntfName{"xyz.openbmc_project.MCTP.Binding"};
    const std::string associationIntfName{
        "xyz.openbmc_project.Association.Definitions"};

    /** @brief UUID interface name */
    static constexpr std::string_view uuidEndpointIntfName{
        "xyz.openbmc_project.Common.UUID"};

    /** @brief Unix Socket interface name */
    static constexpr std::string_view unixSocketIntfName{
        "xyz.openbmc_project.Common.UnixSocket"};

    static constexpr std::string_view codeConstructEndpointIntfName{
        "au.com.codeconstruct.MCTP.Endpoint1"};

    std::queue<MctpInfos> queuedMctpInfos;
    std::map<std::string, MctpInfo> cachedMctpInfoByPath;
    std::map<eid_t, std::queue<MctpInfo>> perEidQueuedMctpInfos;
    std::map<eid_t, std::coroutine_handle<>> perEidDiscoverNsmDeviceTaskHandle;
    // Per-EID counter of consecutive discovery attempts ended by transport
    // timeout. Used by discoverNsmDeviceTask to bound re-queue retries when
    // ping/QueryDeviceIdentification time out (e.g., the device is briefly
    // unresponsive after BMC soft reboot or while in firmware-update mode).
    // Reset on success or on terminal (non-timeout) failure.
    std::map<eid_t, uint8_t> perEidDiscoveryTimeoutRetries;
    std::map<eid_t, std::shared_ptr<nsm::DiscoveryEvents>>
        perEidDiscoveryEvents;
    std::map<uint8_t, std::map<uint16_t, std::shared_ptr<nsm::NsmDevice>>>
        deviceMap;
    DiscoveredEIDs discoveredEIDs;

    /** @brief Helper function to invoke registered handlers
     *
     *  @param[in] mctpInfos - information of discovered MCTP endpoints
     */
    void handleMctpEndpoints(const MctpInfos& mctpInfos);

    requester::Coroutine
        SendRecvNsmMsg(eid_t eid, Request& request,
                       std::shared_ptr<const nsm_msg>& responseMsg,
                       size_t* responseLen);

    // Discovery methods
    bool insertIntoEidTableifNotExist(
        uuid_t uuid, const std::tuple<eid_t, MctpMedium, MctpBinding>& value);
    void discoverNsmDevice(const MctpInfos& mctpInfos);
    requester::Coroutine discoverNsmDeviceTask(eid_t eid);
    requester::Coroutine coSetdeviceStateOnlineTask(const MctpInfos& mctpInfos);
    requester::Coroutine
        coSetdeviceStateOfflineTask(const MctpInfos& mctpInfos);

    // Discovery helper methods
    requester::Coroutine ping(eid_t eid);
    requester::Coroutine
        getQueryDeviceIdentification(eid_t eid, uint8_t& deviceIdentification,
                                     uint8_t& deviceInstance);
    void discoverAndUpdateNsmDeviceTask(
        std::shared_ptr<nsm::NsmDevice> nsmDevice);
    requester::Coroutine
        updateNsmDeviceTask(std::shared_ptr<nsm::NsmDevice> nsmDevice);

    // NsmDevice Creation methods
    std::shared_ptr<nsm::NsmDevice>
        findOrCreateNsmDevice(uint8_t deviceType, uint8_t deviceRole,
                              uint8_t instanceNumber, std::string remapPropName,
                              std::vector<std::string>& remapPropValues);
    template <typename T>
    bool containsValue(
        const T& value,
        const std::variant<std::vector<uint8_t>, std::vector<uuid_t>>&
            remapPropValues) const;
    std::shared_ptr<nsm::NsmDevice> mapNsmDeviceUsingEid(
        eid_t eid, uuid_t mctpUuid, uint8_t deviceType, uint8_t instanceNumber,
        std::string associatedPath, bool active, MctpMedium mctpMedium,
        MctpBinding mctpBinding, std::optional<eid_t> localEid);
    int mapMctpEIDForNsmDevice(std::shared_ptr<nsm::NsmDevice> nsmDevice);
    void handleMctpStateTransition(const std::string objPath);
    requester::Coroutine findConfiguredAssociations(
        [[maybe_unused]] const std::string& objPath,
        [[maybe_unused]] ConfiguredPath& configuredPath);
};

} // namespace mctp
