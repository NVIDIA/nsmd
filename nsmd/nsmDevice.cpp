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

#include "nsmDevice.hpp"

#include "nsmNumericSensor/nsmNumericAggregator.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

std::shared_ptr<NsmNumericAggregator>
    NsmDevice::findAggregatorByType([[maybe_unused]] const std::string& type)
{
    std::shared_ptr<NsmNumericAggregator> aggregator{};

    auto itr =
        std::find_if(sensorAggregators.begin(), sensorAggregators.end(),
                     [&](const auto& e) { return e->getType() == type; });
    if (itr != sensorAggregators.end())
    {
        aggregator = *itr;
    }

    return aggregator;
}

std::shared_ptr<NsmDevice> findNsmDeviceByUUID(NsmDeviceTable& nsmDevices,
                                               const uuid_t& uuid)
{
    std::shared_ptr<NsmDevice> ret{};

    for (auto nsmDevice : nsmDevices)
    {
        if ((nsmDevice->uuid).substr(0, UUID_LEN) == uuid.substr(0, UUID_LEN))
        {
            ret = nsmDevice;
            break;
        }
    }
    if (!ret)
    {
        lg2::error("Device not found for UUID: {UUID}", "UUID", uuid);
    }
    return ret;
}

void NsmDevice::setEventMode(uint8_t mode)
{
    if (mode > GLOBAL_EVENT_GENERATION_ENABLE_PUSH)
    {
        lg2::error("event generation setting: invalid value={SETTING} provided",
                   "SETTING", mode);
        return;
    }
    eventMode = mode;
}

uint8_t NsmDevice::getEventMode()
{
    return eventMode;
}

bool NsmDevice::isCommandSupported(uint8_t messageType, uint8_t commandCode)
{
    bool supported = false;
    if (messageTypesToCommandCodeMatrix[messageType][commandCode])
    {
        supported = true;
    }
    return supported;
}

void addSensor(std::shared_ptr<NsmDevice>& device,
               const std::shared_ptr<NsmObject>& sensor)
{
    if (!device)
        return;
    device->deviceSensors.emplace_back(sensor);
}

void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, bool priority)
{
    if (priority)
    {
        device->prioritySensors.emplace_back(sensor);
    }
    else
    {
        device->roundRobinSensors.emplace_back(sensor);
    }
}
void addSensor(std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor, const std::string& objPath,
               const std::string& interface)
{
    if (!device)
        return;
    auto priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());
    addSensor(device, sensor, priority);
}

void addSensor(SensorManager& manager, std::shared_ptr<NsmDevice>& device,
               std::shared_ptr<NsmSensor> sensor)
{
    if (!device)
        return;
    addSensor(device, sensor);
    sensor->update(manager, manager.getEid(device)).detach();
}

} // namespace nsm
