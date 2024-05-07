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
        uuid(uuid),
        messageTypesToCommandCodeMatrix(
            NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false)),
        eventMode(GLOBAL_EVENT_GENERATION_DISABLE)

    {}

    std::shared_ptr<sdbusplus::asio::dbus_interface> fruDeviceIntf;

    uuid_t uuid;
    bool isDeviceActive;
    std::unique_ptr<sdbusplus::Timer> pollingTimer;
    std::coroutine_handle<> doPollingTaskHandle;
    std::vector<std::shared_ptr<NsmObject>> deviceSensors;
    std::vector<std::shared_ptr<NsmSensor>> prioritySensors;
    std::deque<std::shared_ptr<NsmSensor>> roundRobinSensors;
    std::vector<std::shared_ptr<NsmObject>> capabilityRefreshSensors;
    std::vector<std::shared_ptr<NsmNumericAggregator>> sensorAggregators;
    std::vector<std::shared_ptr<NsmObject>> standByToDcRefreshSensors;

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

  private:
    std::vector<std::vector<bitfield8_t>> commands;
    uint8_t eventMode;
};

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid);

/**
 * @brief Adds device/static sensor to to NsmDevice.
 *
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to device/static sensor
 */
void addSensor(std::shared_ptr<NsmDevice>& device,
               const std::shared_ptr<NsmObject>& sensor);

/**
 * @brief Adds dynamic sensor to NsmDevice. It read dbus property 'Priority' for
 * the provided interface
 *
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to dynamic sensor
 * @param priority[in] Flag to add sensor as priority sensor
 */
void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, bool priority);

/**
 * @brief Adds dynamic sensor to NsmDevice. It read dbus property 'Priority' for
 * the provided interface
 *
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to dynamic sensor
 * @param objPath[in] Dbus object path
 * @param interface[in] Interface name for finding Priority property
 */
void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, const std::string& objPath,
               const std::string& interface);

/**
 * @brief Adds static sensor to NsmDevice. Function calls update
 * method of the sensor and detach its co_await call
 *
 * @param manager[in,out] Reference to sensor manager
 * @param device[in,out] Pointer to shared NsmDevice
 * @param sensor[in] Pointer to static sensor
 */
void addSensor(SensorManager& manager, std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor);

} // namespace nsm
