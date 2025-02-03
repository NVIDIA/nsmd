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

#include "base.h"
#include "device-capability-discovery.h"

#include "common/coroutineSemaphore.hpp"
#include "common/types.hpp"
#include "nsmEvent.hpp"
#include "nsmObject.hpp"
#include "nsmSensor.hpp"
#include "types.hpp"
#include "utils.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/timer.hpp>

#include <coroutine>
#include <deque>

namespace nsm
{

class SensorManager;
class NsmNumericAggregator;
class NsmDevice;
class NsmLongRunningEventHandler;
class NsmLongRunningEvent;
using NsmDeviceTable = std::vector<std::shared_ptr<NsmDevice>>;

struct ActiveLongRunningHandlerInfo
{
    uint8_t messageType;
    uint8_t commandCode;
    std::shared_ptr<NsmLongRunningEvent> sensorInstance;
};

class NsmDevice
{
  public:
    NsmDevice(uuid_t uuid) :
        uuid(uuid), isDeviceActive(false),
        longRunningEventHandler(registerLongRunningEventHandler()),
        messageTypesToCommandCodeMatrix(
            NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false)),
        eventMode(GLOBAL_EVENT_GENERATION_DISABLE)
    {}

    NsmDevice(uint8_t deviceType, uint8_t instanceNumber) :
        isDeviceActive(false),
        longRunningEventHandler(registerLongRunningEventHandler()),
        messageTypesToCommandCodeMatrix(
            NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false)),
        eventMode(GLOBAL_EVENT_GENERATION_DISABLE), deviceType(deviceType),
        instanceNumber(instanceNumber)
    {}

    std::unique_ptr<sdbusplus::asio::dbus_interface> fruDeviceIntf;
    std::unique_ptr<void, std::function<void(void*)>> nsmRawCmdIntf;

    eid_t eid = 0;
    uuid_t uuid;
    uuid_t deviceUuid;
    bool isDeviceActive;
    bool isDeviceReady = false;
    bool stopPolling = false;
    std::coroutine_handle<> doPollingTaskHandle;
    std::coroutine_handle<> doPollingTaskHandleLongRunning;
    std::vector<std::shared_ptr<NsmObject>> deviceSensors;
    std::vector<std::shared_ptr<NsmObject>> prioritySensors;
    std::deque<std::shared_ptr<NsmObject>> roundRobinSensors;
    std::vector<std::shared_ptr<NsmObject>> longRunningSensors;
    std::vector<std::shared_ptr<NsmObject>> capabilityRefreshSensors;
    std::vector<std::shared_ptr<NsmNumericAggregator>> sensorAggregators;
    std::vector<std::shared_ptr<NsmObject>> standByToDcRefreshSensors;

    EventDispatcher eventDispatcher;
    std::vector<std::shared_ptr<NsmEvent>> deviceEvents;
    NsmLongRunningEventHandler& longRunningEventHandler;

    std::shared_ptr<NsmNumericAggregator>
        findAggregatorByType(const std::string& type);

    void setEventMode(uint8_t mode);
    uint8_t getEventMode();
    std::vector<std::vector<bool>> messageTypesToCommandCodeMatrix;
    bool isCommandSupported(uint8_t messageType, uint8_t commandCode);
    /** @brief set the nsmDevice to online state */
    void setOnline();

    /** @brief set the nsmDevice to offline state */
    void setOffline();

    /**
     * @brief Adds device/static sensor to to NsmDevice.
     *
     * @param sensor[in] Pointer to device/static sensor
     * @return NsmObject& Sensor reference
     */
    NsmObject& addStaticSensor(std::shared_ptr<NsmObject> sensor);

    /**
     * @brief Adds dynamic sensor to NsmDevice. It read dbus property 'Priority'
     * for the provided interface
     *
     * @param sensor[in] Pointer to dynamic sensor
     * @param priority[in] Flag to add sensor as priority sensor
     * @param isLongRunning[in] Flag to add sensor as Long Running sensor
     */
    void addSensor(const std::shared_ptr<NsmObject>& sensor, bool priority,
                   bool isLongRunning = false);

    /** @brief getter of deviceType */
    uint8_t getDeviceType()
    {
        return deviceType;
    }

    /** @brief getter of instanceNumber */
    uint8_t getInstanceNumber()
    {
        return instanceNumber;
    }

    /** @brief Getter for the longRunningSemaphore */
    common::CoroutineSemaphore& getSemaphore()
    {
        return longRunningSemaphore;
    }

    inline PollingState getPollingState()
    {
        return devicePollingState;
    }

    inline void setPollingState(const PollingState s)
    {
        devicePollingState = s;
    }

  public:
    /**
     * @brief Registers a long-running handler for a specific message type and
     * command code.
     *
     * @param messageType The message type associated with the long-running
     * event.
     * @param commandCode The command code associated with the long-running
     * event.
     * @param handler The event handler to register.
     */
    void registerLongRunningHandler(
        uint8_t messageType, uint8_t commandCode,
        std::shared_ptr<NsmLongRunningEvent> sensorInstance);

    /**
     * @brief Clears the registered long-running handler.
     */
    void clearLongRunningHandler();

    /**
     * @brief Retrieves the active long-running handler, if any.
     *
     * @return std::optional<ActiveLongRunningHandlerInfo> containing the active
     * handler, or an empty optional if no handler is active.
     */
    std::optional<nsm::ActiveLongRunningHandlerInfo>
        getActiveLongRunningHandler() const;
    int invokeLongRunningHandler(eid_t eid, NsmType type, NsmEventId eventId,
                                 const nsm_msg* event, size_t eventLen);

  private:
    std::vector<std::vector<bitfield8_t>> commands;
    uint8_t eventMode;
    uint8_t deviceType = 0;
    uint8_t instanceNumber = 0;
    NsmLongRunningEventHandler& registerLongRunningEventHandler();
    common::CoroutineSemaphore
        longRunningSemaphore; // Semaphore for synchronizing long running
                              // commands
    std::optional<ActiveLongRunningHandlerInfo> longRunningHandler;
    PollingState devicePollingState;
};

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid);

std::shared_ptr<NsmDevice>
    findNsmDeviceByIdentification(NsmDeviceTable& nsmDevices,
                                  uint8_t deviceType, uint8_t instanceNumber);

int parseStaticUuid(uuid_t& uuid, uint8_t& deviceType, uint8_t& instanceNumber);

} // namespace nsm
