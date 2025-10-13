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

#include "nsmPriorityMapping.h"

#include "nsmDevice.hpp"
#include "nsmEvent/nsmGPIOStateChangeEvent.hpp"
#include "nsmGPIO/nsmGPIOState.hpp"
#include "sensorManager.hpp"

namespace nsm
{

inline void createNsmGPIOStateSensors(std::shared_ptr<NsmDevice> device,
                                      const std::string& name,
                                      const std::string& type,
                                      const std::string& parentChassisPath)
{
    auto objPath = "/xyz/openbmc_project/gpio/" + name;

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent_chassis", "gpio_state",
                                  parentChassisPath);
    auto gpioStateIntf = std::make_shared<GPIOStateIntf>(
        utils::DBusHandler::getBus(), objPath.c_str());

    auto gpioStateSensor = std::make_shared<NsmGPIOState>(
        utils::DBusHandler::getBus(), type, name, objPath, associationsList,
        gpioStateIntf);

    device->addStaticSensor(gpioStateSensor);

    // register gpio state changeevent
    auto gpioStateChangeEvent =
        std::make_shared<NsmGPIOStateChangeEvent>(name, type, gpioStateIntf);
    device->addDeviceEvent(gpioStateChangeEvent,
                           NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                           NSM_GPIO_STATE_CHANGE_EVENT);
    return;
}

} // namespace nsm
