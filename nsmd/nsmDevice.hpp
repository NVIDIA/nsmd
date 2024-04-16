/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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

class NsmNumericAggregator;
class NsmDevice;
using NsmDeviceTable = std::vector<std::shared_ptr<NsmDevice>>;

class NsmDevice
{
  public:
    NsmDevice(uuid_t uuid) :
        uuid(uuid), eventMode(GLOBAL_EVENT_GENERATION_DISABLE)
    {}

    std::shared_ptr<sdbusplus::asio::dbus_interface> fruDeviceIntf;

    uuid_t uuid;
    std::unique_ptr<phosphor::Timer> pollingTimer;
    std::coroutine_handle<> doPollingTaskHandle;
    std::vector<std::shared_ptr<NsmObject>> deviceSensors;
    std::vector<std::shared_ptr<NsmSensor>> prioritySensors;
    std::deque<std::shared_ptr<NsmSensor>> roundRobinSensors;
    std::vector<std::shared_ptr<NsmObject>> staticSensors;
    std::vector<std::shared_ptr<NsmNumericAggregator>> sensorAggregators;

    std::shared_ptr<NsmNumericAggregator>
        findAggregatorByType(const std::string& type);

    void setEventMode(uint8_t mode);
    uint8_t getEventMode();

  private:
    std::vector<std::vector<bitfield8_t>> commands;
    uint8_t eventMode;
};

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid);

} // namespace nsm
