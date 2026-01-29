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
#include "nsmErrorInjection/nsmErrorInjection.hpp"
#include "nsmSetAsync/nsmSetErrorInjection.hpp"
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
    device->addDeviceSensors(setErrorInjection);
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
        if (type == ErrorInjectionCapabilityIntf::Type::FatalErrors ||
            type == ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors ||
            type ==
                ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors ||
            type == ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors ||
            type == ErrorInjectionCapabilityIntf::Type::GPIOSpoofingErrors)
        {
            // These error types are handled separately
            continue;
        }
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
        device->addDeviceSensors(setErrorInjectionEnabled);
    }
    return;
}

inline bool
    getErrorInjectionTypeAndSubtype(ErrorInjectionCapabilityIntf::Type type,
                                    uint16_t* errorInjectionType,
                                    uint16_t* errorInjectionSubtype)
{
    if (errorInjectionType == nullptr || errorInjectionSubtype == nullptr)
    {
        return false;
    }

    switch (type)
    {
        case ErrorInjectionCapabilityIntf::Type::MemoryErrors:
            *errorInjectionType = EI_MEMORY_ERRORS;
            *errorInjectionSubtype = 0;
            break;
        case ErrorInjectionCapabilityIntf::Type::PCIeErrors:
            *errorInjectionType = EI_PCI_ERRORS;
            *errorInjectionSubtype = 0;
            break;
        case ErrorInjectionCapabilityIntf::Type::NVLinkErrors:
            *errorInjectionType = EI_NVLINK_ERRORS;
            *errorInjectionSubtype = 0;
            break;
        case ErrorInjectionCapabilityIntf::Type::ThermalErrors:
            *errorInjectionType = EI_THERMAL_ERRORS;
            *errorInjectionSubtype = 0;
            break;
        case ErrorInjectionCapabilityIntf::Type::FatalErrors:
            *errorInjectionType = EI_DEVICE_ERRORS;
            *errorInjectionSubtype = EI_DEVICE_ERRORS_SUBTYPE_FATAL;
            break;
        case ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors:
            *errorInjectionType = EI_DEVICE_ERRORS;
            *errorInjectionSubtype = EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY;
            break;
        case ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors:
            *errorInjectionType = EI_DEVICE_ERRORS;
            *errorInjectionSubtype = EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION;
            break;
        case ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors:
            *errorInjectionType = EI_DEVICE_ERRORS;
            *errorInjectionSubtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
            break;
        case ErrorInjectionCapabilityIntf::Type::GPIOSpoofingErrors:
            *errorInjectionType = EI_GPIO_SPOOFING;
            *errorInjectionSubtype = 0;
            break;
        case ErrorInjectionCapabilityIntf::Type::Unknown:
        default:
            return false;
    }
    return true;
}

inline void createErrorInjectionSensorsForType(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const path& objPath, ErrorInjectionCapabilityIntf::Type type)
{
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    Interfaces<ErrorInjectionPayloadIntf> payloadInterfaces;

    uint16_t errorInjectionType;
    uint16_t errorInjectionSubtype;
    if (!getErrorInjectionTypeAndSubtype(type, &errorInjectionType,
                                         &errorInjectionSubtype))
    {
        lg2::error(
            "createErrorInjectionSensorsForType: Unsupported ErrorInjectionCapability type. Cannot map to type/subtype and create sensor. type={TYPE}",
            "TYPE", static_cast<int>(type));
        return;
    }

    auto name = ErrorInjectionCapabilityIntf::convertTypeToString(type);
    name = name.substr(name.find_last_of('.') + 1);
    auto path = objPath / "ErrorInjection" / name;

    auto payloadInterface = std::make_shared<ErrorInjectionPayloadIntf>(
        utils::DBusHandler::getBus(), path.string().c_str());
    payloadInterfaces.insert(std::make_pair(path, payloadInterface));
    auto interface = std::make_shared<ErrorInjectionCapabilityIntf>(
        utils::DBusHandler::getBus(), path.string().c_str());
    interface->type(type);
    interfaces.insert(std::make_pair(path, interface));

    auto mcuCapabilitiesProvider =
        NsmInterfaceProvider<ErrorInjectionCapabilityIntf>(
            "MCUErrorInjectionCapability", "NSM_MCUErrorInjectionCapability",
            interfaces);
    auto mcuErrorInjectionSupported =
        std::make_shared<NsmErrorInjectionSupported>(mcuCapabilitiesProvider);
    device->addStaticSensor(mcuErrorInjectionSupported);
    auto mcuErrorInjectionEnabled =
        std::make_shared<NsmErrorInjectionEnabled>(mcuCapabilitiesProvider);
    device->addSensor(mcuErrorInjectionEnabled, ERROR_INJECTION_PRIORITY);

    auto payloadProvider = NsmInterfaceProvider<ErrorInjectionPayloadIntf>(
        "ErrorInjectionPayload", "NSM_ErrorInjectionPayload",
        payloadInterfaces);
    auto errorInjectionPayload = std::make_shared<NsmErrorInjectionPayload>(
        payloadProvider, errorInjectionType, errorInjectionSubtype);
    device->addSensor(errorInjectionPayload, ERROR_INJECTION_PRIORITY);

    auto pathStr = path.string();
    auto errorType = pathStr.substr(pathStr.find_last_of('/') + 1);
    auto activateIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        utils::DBusHandler::getBus(), path.string().c_str(), errorInjectionType,
        errorInjectionSubtype, device);
    // Set Error Injection Enabled
    auto setErrorInjectionEnabled =
        std::make_shared<NsmSetErrorInjectionEnabled>(name, interface->type(),
                                                      manager, interfaces);
    auto& asyncDispatcherEnabled =
        *AsyncOperationManager::getInstance()->getDispatcher(pathStr);
    asyncDispatcherEnabled.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjectionCapability", "Enabled",
        AsyncSetOperationInfo{
            std::bind_front(&NsmSetErrorInjectionEnabled::enabled,
                            setErrorInjectionEnabled.get()),
            mcuErrorInjectionEnabled, device});
    device->addDeviceSensors(setErrorInjectionEnabled);
    // Set Error Injection Payload
    auto setErrorInjectionPayloadSensor =
        std::make_shared<NsmSetErrorInjectionPayload>(
            errorType, manager, payloadInterfaces, activateIntf,
            errorInjectionType, errorInjectionSubtype);
    auto& asyncDispatcherPayload =
        *AsyncOperationManager::getInstance()->getDispatcher(pathStr);
    asyncDispatcherPayload.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjectionPayload", "Payload",
        AsyncSetOperationInfo{
            std::bind_front(
                &NsmSetErrorInjectionPayload::setErrorInjectionPayload,
                setErrorInjectionPayloadSensor.get()),
            errorInjectionPayload, device});
    device->addSetSensor(setErrorInjectionPayloadSensor);
    return;
}

inline void createNsmMCUErrorInjectionSensors(SensorManager& manager,
                                              std::shared_ptr<NsmDevice> device,
                                              const path& objPath)
{
    auto setErrorInjection = std::make_shared<NsmSetErrorInjection>(manager,
                                                                    objPath);
    auto errorInjectionSensor =
        std::make_shared<NsmErrorInjection>(*setErrorInjection);
    device->addDeviceSensors(setErrorInjection);
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

    auto deviceRole = device->getDeviceRole();

    // Applicable for all device roles
    createErrorInjectionSensorsForType(
        manager, device, objPath,
        ErrorInjectionCapabilityIntf::Type::FatalErrors);
    createErrorInjectionSensorsForType(
        manager, device, objPath,
        ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors);
    createErrorInjectionSensorsForType(
        manager, device, objPath,
        ErrorInjectionCapabilityIntf::Type::GPIOSpoofingErrors);

    if (deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA ||
        deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA)
    {
        createErrorInjectionSensorsForType(
            manager, device, objPath,
            ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors);
    }
    if (deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA)
    {
        createErrorInjectionSensorsForType(
            manager, device, objPath,
            ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors);
    }
    return;
}

} // namespace nsm
