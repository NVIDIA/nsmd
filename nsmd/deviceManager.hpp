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
#include "common/utils.hpp"
#include "globals.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "requester/handler.hpp"
#include "requester/mctp_endpoint_discovery.hpp"
#include "stateChangeLogger.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdeventplus/event.hpp>

#include <map>
#include <queue>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace nsm
{

using RequesterHandler = requester::Handler<requester::Request>;

using Active = bool;
using DeviceType = uint8_t;
using DeviceRole = uint8_t;
using InstanceNumber = uint8_t;
using DiscoveredEIDs =
    std::map<mctp_eid_t,
             std::tuple<uuid_t, DeviceType, InstanceNumber, Active>>;

/** @class DeviceManager
 *
 * The Manager class provides a handle function for MctpDiscovery class to
 * discover NSM devices from the enumerated MCTP endpoints and then exposes the
 * retrieved FRU data from discovered NSM devices to D-Bus FruDevice PDI.
 */
class DeviceManager : public mctp::MctpDiscoveryHandlerIntf
{
  public:
    // Singleton access method that simply returns the instance
    static DeviceManager& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "DeviceManager instance is not initialized yet");
        }
        return *instance;
    }

    // Initialization method to create and setup the singleton instance
    static void initialize(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized DeviceManager");
        }
        static DeviceManager inst(event, handler, instanceIdDb, objServer,
                                  eidTable, nsmDevices);
        instance = &inst;
    }

    DeviceManager(const DeviceManager&) = delete;
    DeviceManager& operator=(const DeviceManager&) = delete;
    DeviceManager(DeviceManager&&) = delete;
    DeviceManager& operator=(DeviceManager&&) = delete;

    void handleMctpEndpoints(const MctpInfos& mctpInfos)
    {
        discoverNsmDevice(mctpInfos);
    }

    requester::Coroutine onlineMctpEndpoint(const MctpInfo& mctpInfo) override;

    requester::Coroutine offlineMctpEndpoint(const MctpInfo& mctpInfo) override;

    requester::Coroutine SendRecvNsmMsg(eid_t eid, Request& request,
                                        const nsm_msg** responseMsg,
                                        size_t* responseLen);

    const std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
        getEidTable() const
    {
        return eidTable;
    }

    const NsmDeviceTable& getNsmDevices() const
    {
        return nsmDevices;
    }

    requester::Coroutine updateNsmDevice(std::shared_ptr<NsmDevice> nsmDevice,
                                         uint8_t eid);
    requester::Coroutine
        refreshCapabilitySensor(std::shared_ptr<NsmDevice> nsmDevice);

    requester::Coroutine
        updateFruDeviceIntf(std::shared_ptr<NsmDevice> nsmDevice, uint8_t eid);

    std::optional<mctp_eid_t> searchEID(uint8_t deviceType,
                                        uint8_t instanceNumber,
                                        uint8_t deviceRole);

    std::map<uint16_t, std::vector<uint64_t>> mapInstanceNumberToInstanceNumber;
    std::map<uint16_t, std::vector<std::string>> mapUuidToInstanceNumber;
    std::map<uint16_t, std::vector<uint64_t>> mapEidToInstanceNumber;
    std::map<uint16_t, std::map<uint8_t, StateChangeLogger>>
        mapInstanceNumberToLogger;
    std::map<uint16_t, std::map<std::string, StateChangeLogger>>
        mapUuidToLogger;
    std::map<uint16_t, std::map<uint8_t, StateChangeLogger>> mapEidToLogger;
    StateChangeLogger getInventoryLogger;

    requester::Coroutine
        refreshCommandMatrix(std::shared_ptr<NsmDevice> nsmDevice, uint8_t eid);

    std::shared_ptr<NsmDevice> getNsmDeviceFromStaticUUID(uuid_t uuid);
    std::shared_ptr<NsmDevice> getNsmDeviceFromEid(eid_t eid);
    std::shared_ptr<NsmDevice>
        getNsmDeviceByIdentification(uint8_t deviceType, uint8_t instanceNumber,
                                     uint8_t deviceRole);
    void handleMctpStateTransition(const std::string objPath,
                                   const bool state) override;

  private:
    DeviceManager(
        sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices) :
        event(event), handler(handler), instanceIdDb(instanceIdDb),
        objServer(objServer), eidTable(eidTable), nsmDevices(nsmDevices)
    {}

    void discoverNsmDevice(const MctpInfos& mctpInfos);
    bool insertIntoEidTableifNotExist(
        uuid_t uuid, const std::tuple<eid_t, MctpMedium, MctpBinding>& value);

    requester::Coroutine discoverNsmDeviceTask(eid_t eid);
    requester::Coroutine coSetdeviceStateOnlineTask(const MctpInfos& mctpInfos);
    requester::Coroutine
        coSetdeviceStateOfflineTask(const MctpInfos& mctpInfos);
    static DeviceManager* instance;

    requester::Coroutine ping(eid_t eid);
    requester::Coroutine
        getSupportedNvidiaMessageType(eid_t eid,
                                      std::vector<uint8_t>& supportedTypes);
    requester::Coroutine
        getSupportedCommandCodes(eid_t eid, uint8_t nvidia_message_type,
                                 std::vector<uint8_t>& supportedCommandCodes);
    requester::Coroutine getFRU(eid_t eid, nsm::InventoryProperties& properties,
                                const uint8_t& deviceType);
    requester::Coroutine
        getInventoryInformation(eid_t eid, uint8_t& propertyIdentifier,
                                InventoryProperties& properties);

    requester::Coroutine
        getQueryDeviceIdentification(eid_t eid, uint8_t& deviceIdentification,
                                     uint8_t& deviceInstance);

    uint8_t remapInstanceNumber(uint8_t instanceNumber, uint8_t deviceType,
                                uint8_t deviceRole, uuid_t& uuid,
                                mctp_eid_t eid);

    std::shared_ptr<NsmDevice>
        findOrCreateNsmDevice(uint8_t deviceType, uint8_t deviceRole,
                              uint8_t instanceNumber, std::string remapPropName,
                              std::string remapPropValue);
    void updateDeviceParameter(std::shared_ptr<NsmDevice> nsmDevice, eid_t eid,
                               uuid_t uuid, uint8_t deviceInstanceNumber);
    int mapMctpEIDForNsmDevice(std::shared_ptr<NsmDevice> nsmDevice);
    void discoverAndUpdateNsmDeviceTask(std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine
        updateNsmDeviceTask(std::shared_ptr<NsmDevice> nsmDevice);

    std::shared_ptr<NsmDevice> mapNsmDeviceUsingEid(eid_t eid, uuid_t mctpUuid,
                                                    uint8_t deviceType,
                                                    uint8_t instanceNumber,
                                                    bool active);

    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable;
    std::map<uint8_t, std::map<uint16_t, std::shared_ptr<NsmDevice>>> deviceMap;

    std::queue<MctpInfos> queuedMctpInfos;
    std::map<eid_t, std::queue<MctpInfo>> perEidQueuedMctpInfos;
    std::map<eid_t, std::coroutine_handle<>> perEidDiscoverNsmDeviceTaskHandle;
    std::coroutine_handle<> discoverNsmDeviceTaskHandle;
    std::coroutine_handle<> deviceOfflineTaskHandle;
    NsmDeviceTable& nsmDevices;
    DiscoveredEIDs discoveredEIDs;
};
} // namespace nsm
