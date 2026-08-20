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

#include <memory>
#include <vector>

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

    // Sole owner of this device's mask; the batch property and every per-type
    // Enabled property route their read-modify-write through it.
    auto setErrorInjectionCapabilities =
        std::make_shared<NsmSetErrorInjectionCapabilities>(
            "ErrorInjectionCapabilities", manager, interfaces,
            errorInjectionEnabled);
    device->addDeviceSensors(setErrorInjectionCapabilities);

    // One Async.Set carries every capability the client changed, so a
    // multi-type PATCH becomes one device write instead of one per type.
    errorInjectionDispatcher.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjection",
        "ErrorInjectionCapabilitiesEnabled",
        AsyncSetOperationInfo{
            std::bind_front(
                &NsmSetErrorInjectionCapabilities::capabilitiesEnabled,
                setErrorInjectionCapabilities.get()),
            errorInjectionEnabled, device});

    for (const auto& [path, interface] : interfaces)
    {
        auto pathStr = path.string();
        auto name = pathStr.substr(pathStr.find_last_of('/') + 1);
        auto setErrorInjectionEnabled =
            std::make_shared<NsmSetErrorInjectionEnabled>(
                name, interface->type(), interfaces,
                setErrorInjectionCapabilities);
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

/**
 * @brief Per-device error-injection state shared by every capability type.
 *        A type owning a private single-entry container would be blind to its
 *        siblings during read-modify-write and zero their bits.
 */
struct ErrorInjectionCapabilityContext
{
    Interfaces<ErrorInjectionCapabilityIntf> interfaces;
    std::shared_ptr<NsmErrorInjectionEnabled> enabledSensor;
    std::shared_ptr<NsmSetErrorInjectionCapabilities> capabilities;
};

/**
 * @brief Creates the shared capability objects for @p types and registers the
 *        aggregate batch property, mirroring the standard path. Types with no
 *        injection type/subtype mapping are skipped.
 */
inline ErrorInjectionCapabilityContext createErrorInjectionCapabilityContext(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const path& objPath,
    const std::vector<ErrorInjectionCapabilityIntf::Type>& types)
{
    ErrorInjectionCapabilityContext context;

    for (auto type : types)
    {
        uint16_t errorInjectionType;
        uint16_t errorInjectionSubtype;
        if (!getErrorInjectionTypeAndSubtype(type, &errorInjectionType,
                                             &errorInjectionSubtype))
        {
            continue;
        }
        auto name = ErrorInjectionCapabilityIntf::convertTypeToString(type);
        name = name.substr(name.find_last_of('.') + 1);
        auto path = objPath / "ErrorInjection" / name;
        auto interface = std::make_shared<ErrorInjectionCapabilityIntf>(
            utils::DBusHandler::getBus(), path.string().c_str());
        interface->type(type);
        context.interfaces.insert(std::make_pair(path, interface));
    }

    if (context.interfaces.empty())
    {
        // NsmInterfaces rejects an empty container, and there is no mask to
        // own. Per-type registration then finds no interface and skips.
        lg2::error(
            "createErrorInjectionCapabilityContext: No mappable capability type, skipping registration. objPath={PATH}",
            "PATH", objPath.string());
        return context;
    }

    auto mcuCapabilitiesProvider =
        NsmInterfaceProvider<ErrorInjectionCapabilityIntf>(
            "MCUErrorInjectionCapability", "NSM_MCUErrorInjectionCapability",
            context.interfaces);
    device->addStaticSensor(
        std::make_shared<NsmErrorInjectionSupported>(mcuCapabilitiesProvider));
    context.enabledSensor =
        std::make_shared<NsmErrorInjectionEnabled>(mcuCapabilitiesProvider);
    device->addSensor(context.enabledSensor, ERROR_INJECTION_PRIORITY);

    context.capabilities = std::make_shared<NsmSetErrorInjectionCapabilities>(
        "ErrorInjectionCapabilities", manager, context.interfaces,
        context.enabledSensor);
    device->addDeviceSensors(context.capabilities);

    auto& errorInjectionDispatcher =
        *AsyncOperationManager::getInstance()->getDispatcher(
            (objPath / "ErrorInjection"));
    errorInjectionDispatcher.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjection",
        "ErrorInjectionCapabilitiesEnabled",
        AsyncSetOperationInfo{
            std::bind_front(
                &NsmSetErrorInjectionCapabilities::capabilitiesEnabled,
                context.capabilities.get()),
            context.enabledSensor, device});

    return context;
}

inline void createErrorInjectionSensorsForType(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const path& objPath, ErrorInjectionCapabilityIntf::Type type,
    const ErrorInjectionCapabilityContext& context)
{
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

    auto capability = context.interfaces.find(path);
    if (capability == context.interfaces.end())
    {
        lg2::error(
            "createErrorInjectionSensorsForType: Type absent from the shared capability container. type={TYPE}",
            "TYPE", static_cast<int>(type));
        return;
    }

    auto payloadInterface = std::make_shared<ErrorInjectionPayloadIntf>(
        utils::DBusHandler::getBus(), path.string().c_str());
    payloadInterfaces.insert(std::make_pair(path, payloadInterface));

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
    // Bound to the shared container and mask owner, so a single-type write
    // reads its siblings before composing the mask.
    auto setErrorInjectionEnabled =
        std::make_shared<NsmSetErrorInjectionEnabled>(
            name, capability->second->type(), context.interfaces,
            context.capabilities);
    auto& asyncDispatcherEnabled =
        *AsyncOperationManager::getInstance()->getDispatcher(pathStr);
    asyncDispatcherEnabled.addAsyncSetOperation(
        "com.nvidia.ErrorInjection.ErrorInjectionCapability", "Enabled",
        AsyncSetOperationInfo{
            std::bind_front(&NsmSetErrorInjectionEnabled::enabled,
                            setErrorInjectionEnabled.get()),
            context.enabledSensor, device});
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
    std::vector<ErrorInjectionCapabilityIntf::Type> types{
        ErrorInjectionCapabilityIntf::Type::FatalErrors,
        ErrorInjectionCapabilityIntf::Type::PortRecoveryErrors,
        ErrorInjectionCapabilityIntf::Type::GPIOSpoofingErrors};

    if (deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA ||
        deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA)
    {
        types.push_back(
            ErrorInjectionCapabilityIntf::Type::LeakDetectionErrors);
    }
    if (deviceRole == NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA)
    {
        types.push_back(
            ErrorInjectionCapabilityIntf::Type::USBBridgeEmulationErrors);
    }

    // One container and mask owner for every type this role exposes, so a
    // per-type write sees its siblings and the batch can address them all.
    auto context = createErrorInjectionCapabilityContext(manager, device,
                                                         objPath, types);
    for (auto type : types)
    {
        createErrorInjectionSensorsForType(manager, device, objPath, type,
                                           context);
    }
    return;
}

} // namespace nsm
