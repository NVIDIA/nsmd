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
#include "nsmErrorInjection.hpp"
#include "nsmSetErrorInjection.hpp"
#include "sensorManager.hpp"

namespace nsm
{

inline void createNsmErrorInjectionSensors(SensorManager& manager,
                                           std::shared_ptr<NsmDevice> device,
                                           const path& objPath)
{
    auto setErrorInjection = std::make_shared<NsmSetErrorInjection>(manager,
                                                                    objPath);
    auto errorInjectionSensor =
        std::make_shared<NsmErrorInjection>(*setErrorInjection);
    device->deviceSensors.emplace_back(setErrorInjection);
    device->addSensor(errorInjectionSensor, ERROR_INJECTION_PRIORITY);

    auto& errorInjectionDispatcher =
        *AsyncOperationManager::getInstance()->getDispatcher(
            (objPath / "ErrorInjection"));
    errorInjectionDispatcher.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjection", "ErrorInjectionModeEnabled",
        AsyncSetOperationInfo{
            std::bind_front(&NsmSetErrorInjection::errorInjectionModeEnabled,
                            setErrorInjection.get()),
            errorInjectionSensor, device});

    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    for (int i = 0; i < (int)ErrorInjectionCapabilityIntf::Type::Unknown; i++)
    {
        auto type = ErrorInjectionCapabilityIntf::Type(i);
        auto name = ErrorInjectionCapabilityIntf::convertTypeToString(type);
        name = name.substr(name.find_last_of('.') + 1);
        auto path = objPath / "ErrorInjection" / name;
        auto interface = std::make_shared<ErrorInjectionCapabilityIntf>(
            utils::DBusHandler::getBus(), path.string().c_str());
        interface->type(type);
        interfaces.insert(std::make_pair(path, interface));
    }

    auto capabilitiesProvider =
        NsmInterfaceProvider<ErrorInjectionCapabilityIntf>(
            "ErrorInjectionCapability", "NSM_ErrorInjectionCapability",
            interfaces);
    auto errorInjectionSupported =
        std::make_shared<NsmErrorInjectionSupported>(capabilitiesProvider);
    auto errorInjectionEnabled =
        std::make_shared<NsmErrorInjectionEnabled>(capabilitiesProvider);

    device->addStaticSensor(errorInjectionSupported);
    device->addSensor(errorInjectionEnabled, ERROR_INJECTION_PRIORITY);

    for (const auto& [path, interface] : interfaces)
    {
        auto pathStr = path.string();
        auto name = pathStr.substr(pathStr.find_last_of('/') + 1);
        auto setErrorInjectionEnabled =
            std::make_shared<NsmSetErrorInjectionEnabled>(
                name, interface->type(), manager, interfaces);
        auto& asyncDispatcher =
            *AsyncOperationManager::getInstance()->getDispatcher(pathStr);
        asyncDispatcher.addAsyncSetOperation(
            "com.nvidia.ErrorInjection.ErrorInjectionCapability", "Enabled",
            AsyncSetOperationInfo{
                std::bind_front(&NsmSetErrorInjectionEnabled::enabled,
                                setErrorInjectionEnabled.get()),
                errorInjectionEnabled, device});
        device->deviceSensors.emplace_back(setErrorInjectionEnabled);
    }
}

} // namespace nsm
