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

#include "dBusAsyncUtils.hpp"
#include "instance_id.hpp"
#include "nsmDevice.hpp"
#include "nsmObject.hpp"
#include "nsmServiceReadyInterface.hpp"
#include "nsmd/nsmNumericSensor/nsmNumericSensorComposite.hpp"
#include "requester/handler.hpp"
#include "stateChangeLogger.hpp"
#include "types.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/timer.hpp>

#include <memory>
#include <queue>

namespace nsm
{
using RequesterHandler = requester::Handler<requester::Request>;
class NsmNumericSensorComposite;
class NsmPowerCap;
class NsmDefaultPowerCap;
class NsmMaxPowerCap;
class NsmMinPowerCap;
class LimitedSensorQueue;
/**
 * @brief Sensor manager abstraction class
 *
 */
class SensorManager
{
  public:
    SensorManager(NsmDeviceTable& nsmDevices, eid_t localEid) :
        nsmDevices(nsmDevices), localEid(localEid)
    {}
    virtual ~SensorManager() = default;

    virtual eid_t getEid(std::shared_ptr<NsmDevice> nsmDevice) = 0;
    virtual sdbusplus::asio::object_server& getObjServer() = 0;
    eid_t getLocalEid()
    {
        return localEid;
    }
    virtual std::shared_ptr<NsmDevice> getNsmDevice(uint8_t deviceType,
                                                    uint8_t instanceNumber,
                                                    uint8_t deviceRole) = 0;
    virtual std::shared_ptr<NsmDevice>
        getNsmDeviceFromStaticUUID(uuid_t uuid) = 0;
    // Static method to access the instance of the class
    static SensorManager& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "SensorManager instance not initialized yet");
        }
        return *instance;
    }
    std::unordered_map<std::string, std::shared_ptr<NsmObject>>
        objectPathToSensorMap;
    std::vector<std::shared_ptr<NsmPowerCap>> powerCapList;
    std::vector<std::shared_ptr<NsmDefaultPowerCap>> defaultPowerCapList;
    std::vector<std::shared_ptr<NsmMaxPowerCap>> maxPowerCapList;
    std::vector<std::shared_ptr<NsmMinPowerCap>> minPowerCapList;
    std::map<std::string, std::vector<std::shared_ptr<NsmDevice>>>
        processorModuleToDeviceMap;
    std::map<std::shared_ptr<NsmDevice>,
             std::unordered_map<uint8_t, std::string>>
        deviceToPortMap;

  protected:
    static std::unique_ptr<SensorManager> instance;
    NsmDeviceTable& nsmDevices;
    const eid_t localEid;
};

/**
 * @brief SensorManagerImpl
 *
 * This class manages the NSM sensors defined by EM configuration PDI.
 * SensorManagerImpl register callback function to create sensor when there is
 * new NSM device inventory added to D-Bus and provides function calls for other
 * classes to start/stop sensor monitoring.
 *
 */
class SensorManagerImpl : public SensorManager
{
  public:
    // Delete copy constructor and copy assignment operator
    SensorManagerImpl(const SensorManagerImpl&) = delete;
    SensorManagerImpl& operator=(const SensorManagerImpl&) = delete;

    // Static method to initialize the instance
    static void initialize(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid,
        mctp_socket::Manager& sockManager, bool verbose = false)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized "
                "SensorManager");
        }
        instance = std::make_unique<SensorManagerImpl>(
            bus, event, handler, instanceIdDb, objServer, eidTable, nsmDevices,
            localEid, sockManager, verbose);
    }

    sdbusplus::asio::object_server& getObjServer()
    {
        return objServer;
    }

    SensorManagerImpl(
        sdbusplus::bus::bus& bus, sdeventplus::Event& event,
        requester::Handler<requester::Request>& handler,
        nsm::InstanceIdDb& instanceIdDb,
        sdbusplus::asio::object_server& objServer,
        std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>&
            eidTable,
        NsmDeviceTable& nsmDevices, eid_t localEid,
        mctp_socket::Manager& sockManager, bool verbose);
    ~SensorManagerImpl();
    static void dumpReadinessLogs();

    static bool isEMReady();
    static void markEMReady();
    static bool isMCTPReady();
    static bool isNSMPollReady();

  private:
    // Regular methods as before
    void startPolling();
    void interfaceAddedHandler(sdbusplus::message::message& msg);
#ifdef NVIDIA_STANDBYTODC
    void gpioStatusPropertyChangedHandler(sdbusplus::message::message& msg);
#endif
    requester::Coroutine
        refreshCommandMatrix(std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine
        pollPrioritySensors(std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine
        pollNonPrioritySensors(std::shared_ptr<NsmDevice> nsmDevice,
                               uint64_t t0);

    requester::Coroutine deviceTask(std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine
        updateLongRunningSensor(std::shared_ptr<NsmDevice> nsmDevice,
                                std::shared_ptr<NsmObject> sensor,
                                std::shared_ptr<LimitedSensorQueue> sensors);

    std::shared_ptr<NsmDevice> getNsmDeviceFromStaticUUID(uuid_t uuid) override;
    std::shared_ptr<NsmDevice> getNsmDevice(uint8_t deviceType,
                                            uint8_t instanceNumber,
                                            uint8_t deviceRole) override;
    void scanInventory();
    eid_t getEid(std::shared_ptr<NsmDevice> nsmDevice) override;

    bool pollingIsRunning = false;
    static bool isReadyForReadinessCheck;
    static bool isMCTPReadyCheck;
    static bool isEMReadyCheck;
    static std::map<std::string, std::string> readynessFailureMap;
    std::unique_ptr<sdbusplus::bus::match_t> mctpReadinessSigMatch;
    void mctpReadinessSigHandler(sdbusplus::message::message& msg);

    std::unique_ptr<sdbusplus::bus::match_t> emReadinessSigMatch;

    void checkAllDevicesReady();

    sdbusplus::bus::bus& bus;
    sdeventplus::Event& event;
    requester::Handler<requester::Request>& handler;
    nsm::InstanceIdDb& instanceIdDb;
    sdbusplus::asio::object_server& objServer;
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>>& eidTable;

    std::unique_ptr<sdbusplus::bus::match_t> inventoryAddedSignal;
#ifdef NVIDIA_STANDBYTODC
    std::unique_ptr<sdbusplus::bus::match_t> gpioStatusPropertyChangedSignal;
#endif
    std::unique_ptr<sdeventplus::source::Defer> deferScanInventory;
    std::unique_ptr<sdeventplus::source::Defer> newSensorEvent;
    mctp_socket::Manager& sockManager;

    bool verbose;

    std::queue<std::pair<dbus::ObjectPath, dbus::Interface>>
        queuedAddedInterfaces;
    std::coroutine_handle<> interfaceAddedTaskHandle;
    requester::Coroutine interfaceAddedTask();
    std::map<eid_t, StateChangeLogger> stateChangeLoggers;
};
} // namespace nsm
