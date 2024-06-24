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

#include "nsmObject.hpp"
#include "sensorManager.hpp"

#include <sdbusplus/bus.hpp>

namespace nsm
{

class SensorManager;

template <typename T>
class InterfaceWrapper : public NsmObject
{
  public:
    InterfaceWrapper(sdbusplus::bus_t& bus, const char* path) :
        NsmObject("", ""), interface{std::make_shared<T>(bus, path)}
    {}

    std::shared_ptr<T> getInterface()
    {
        return interface;
    }

  private:
    std::shared_ptr<T> interface;
};

template <typename T>
std::shared_ptr<T>
    retrieveInterfaceFromSensorMap(const std::string& sensorObjectPath,
                                   SensorManager& manager,
                                   sdbusplus::bus_t& bus, const char* path)
{
    std::shared_ptr<T> dBusIntf{};

    auto interfaceWrapper =
        manager.objectPathToSensorMap.find(sensorObjectPath);

    if (interfaceWrapper == manager.objectPathToSensorMap.end())
    {
        auto interface = std::make_shared<InterfaceWrapper<T>>(bus, path);
        dBusIntf = interface->getInterface();

        manager.objectPathToSensorMap.insert({sensorObjectPath, interface});
    }
    else
    {
        dBusIntf = std::dynamic_pointer_cast<InterfaceWrapper<T>>(
                       interfaceWrapper->second)
                       ->getInterface();
    }

    return dBusIntf;
}

} // namespace nsm
