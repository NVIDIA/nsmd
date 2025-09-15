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

#include "common/types.hpp"
#include "nsmd/nsmDevice.hpp"
#include "nsmd/socket_handler.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <initializer_list>
#include <vector>

namespace mctp
{

using Active = bool;
using DeviceType = uint8_t;
using DeviceRole = uint8_t;
using InstanceNumber = uint8_t;
using MctpMedium = std::string;
using MctpBinding = std::string;
using DiscoveredEIDs =
    std::map<eid_t, std::tuple<uuid_t, DeviceType, InstanceNumber, Active,
                               MctpMedium, MctpBinding>>;
using EidTable =
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>;
using RequesterHandler = requester::Handler<requester::Request>;
class MctpDiscovery
{
  public:
    MctpDiscovery() = delete;
    MctpDiscovery(const MctpDiscovery&) = delete;
    MctpDiscovery(MctpDiscovery&&) = delete;
    MctpDiscovery& operator=(const MctpDiscovery&) = delete;
    MctpDiscovery& operator=(MctpDiscovery&&) = delete;
    ~MctpDiscovery() = default;

    static MctpDiscovery& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "MctpDiscovery instance is not initialized yet");
        }
        return *instance;
    }

    static void
        initialize(sdbusplus::bus::bus& bus, mctp_socket::Handler& handler,
                   std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler,
                   EidTable& eidTable, nsm::NsmDeviceTable& nsmDevices,
                   sdbusplus::asio::object_server& objServer)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized MctpDiscovery");
        }
        static MctpDiscovery inst(bus, handler, nsmMsgHandler, eidTable,
                                  nsmDevices, objServer);
        instance = &inst;
    }

    std::shared_ptr<nsm::NsmDevice> getNsmDeviceFromStaticUUID(uuid_t uuid);
    std::shared_ptr<nsm::NsmDevice> getNsmDeviceFromEid(eid_t eid);
    std::shared_ptr<nsm::NsmDevice>
        getNsmDeviceByIdentification(uint8_t deviceType, uint8_t instanceNumber,
                                     uint8_t deviceRole);

  private:
    static MctpDiscovery* instance;
    /** @brief Constructs the MCTP Discovery object to handle discovery of
     *         MCTP enabled devices
     *
     *  @param[in] bus - reference to systemd bus
     *  @param[in] list - initializer list to the MctpDiscoveryHandlerIntf
     */
    explicit MctpDiscovery(
        sdbusplus::bus::bus& bus, mctp_socket::Handler& handler,
        std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler,
        EidTable& eidTable, nsm::NsmDeviceTable& nsmDevices,
        sdbusplus::asio::object_server& objServer);
    /** @brief reference to the systemd bus */
    sdbusplus::bus::bus& bus;
    mctp_socket::Handler& handler;
    std::shared_ptr<nsm::NsmMessageHandler> nsmMsgHandler;
    EidTable& eidTable;
    nsm::NsmDeviceTable& nsmDevices;
    sdbusplus::asio::object_server& objServer;
    /** @brief Used to watch for new MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointAddedSignal;

    /** @brief Used to watch for the removed MCTP endpoints */
    sdbusplus::bus::match_t mctpEndpointRemovedSignal;

    /** @brief map of Queue to store the pending property change signal from
     * mctp service */
    std::map<std::string, std::queue<sdbusplus::message::message>>
        mctpQueuedSignals;

    std::map<std::string, std::coroutine_handle<>> deviceStateChangeTaskHandles;
    requester::Coroutine deviceStateChangeTask(const std::string path);

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
    std::shared_ptr<nsm::NsmDevice>
        mapNsmDeviceUsingEid(eid_t eid, uuid_t mctpUuid, uint8_t deviceType,
                             uint8_t instanceNumber, bool active,
                             MctpMedium mctpMedium, MctpBinding mctpBinding);
    int mapMctpEIDForNsmDevice(std::shared_ptr<nsm::NsmDevice> nsmDevice);
    void handleMctpStateTransition(const std::string objPath,
                                   [[maybe_unused]] const bool state);
};

} // namespace mctp
