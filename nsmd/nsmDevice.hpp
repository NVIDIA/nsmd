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
using NsmDeviceTable = std::vector<std::shared_ptr<NsmDevice>>;

class NsmDevice
{
  public:
    NsmDevice(uuid_t uuid) :
        uuid(uuid), isDeviceActive(false),
        messageTypesToCommandCodeMatrix(
            NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false)),
        eventMode(GLOBAL_EVENT_GENERATION_DISABLE)

    {}

    NsmDevice(uint8_t deviceType, uint8_t instanceNumber) :
        isDeviceActive(false),
        messageTypesToCommandCodeMatrix(
            NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false)),
        eventMode(GLOBAL_EVENT_GENERATION_DISABLE), deviceType(deviceType),
        instanceNumber(instanceNumber)

    {}

    std::unique_ptr<sdbusplus::asio::dbus_interface> fruDeviceIntf;

    eid_t eid = 255;
    uuid_t uuid;
    uuid_t deviceUuid;
    bool isDeviceActive;
    bool isDeviceReady = false;
    std::unique_ptr<sdbusplus::Timer> pollingTimer;
    std::coroutine_handle<> doPollingTaskHandle;
    std::vector<std::shared_ptr<NsmObject>> deviceSensors;
    std::vector<std::shared_ptr<NsmSensor>> prioritySensors;
    std::deque<std::shared_ptr<NsmSensor>> roundRobinSensors;
    std::vector<std::shared_ptr<NsmObject>> capabilityRefreshSensors;
    std::vector<std::shared_ptr<NsmNumericAggregator>> sensorAggregators;
    std::vector<std::shared_ptr<NsmObject>> standByToDcRefreshSensors;

    EventDispatcher eventDispatcher;
    std::vector<std::shared_ptr<NsmEvent>> deviceEvents;

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
     */
    void addSensor(const std::shared_ptr<NsmSensor>& sensor, bool priority);

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

  private:
    std::vector<std::vector<bitfield8_t>> commands;
    uint8_t eventMode;
    uint8_t deviceType;
    uint8_t instanceNumber;
};

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid);

std::shared_ptr<NsmDevice>
    findNsmDeviceByIdentification(NsmDeviceTable& nsmDevices,
                                  uint8_t deviceType, uint8_t instanceNumber);

int parseStaticUuid(uuid_t& uuid, uint8_t& deviceType, uint8_t& instanceNumber);

} // namespace nsm
