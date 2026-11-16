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

#include "nsmChassisPCIeDevice.hpp"

#include "nsmPriorityMapping.h"

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmAsioInterface/nsmAsioPCIeDeviceInterface.hpp"
#include "sensorManager.hpp"
#if defined(ENABLE_PCIE_AER_ERROR)
#include "nsmAERError.hpp"
#endif
#include "nsmPort/nsmRetimerPort.hpp"
#if defined(ENABLE_CLOCK_OUTPUT_STATE)
#include "nsmClockOutputEnableState.hpp"
#endif
#include "nsmCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeFunction.hpp"
#if defined(ENABLE_PCIE_LTSSM_STATE)
#include "nsmPCIeLTSSMState.hpp"
#endif
#include "features/associations.hpp"
#include "nsmChassisFeatures.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine
    NsmChassisPCIeDevice<IntfType>::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        // For UuidIntf, we need to get the device UUID from the device manager.
        mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(nsmDevice, mctpDiscovery, deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);
            co_return NSM_SUCCESS;
        }
        co_return NSM_ERROR;
    }
    // For other interfaces, we don't need to update anything.
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

void createChassisPCIeDeviceAsset(std::shared_ptr<NsmDevice> device,
                                  std::string& name,
                                  const std::string& chassisName)
{
    auto assetObject = NsmChassisPCIeDevice<NsmAssetIntf>(chassisName, name);
    std::string manufacturer = MANUFACTURER_NVIDIA;

    assetObject.invoke(pdiMethod(manufacturer), manufacturer);
    // create sensor
    auto partNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, DEVICE_PART_NUMBER);
    auto serialNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, SERIAL_NUMBER);
    const auto modelProperty = getModelInventoryProperty(
        device->getDeviceType(), device->getDeviceRole());
    auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, modelProperty);
    device->addStaticSensor(partNumber);
    device->addStaticSensor(serialNumber);
    device->addStaticSensor(model);
}

void createChassisPCIeDeviceHealth(std::shared_ptr<NsmDevice> device,
                                   std::string& name,
                                   const std::string& chassisName)
{
    std::string health = HEALTH_TYPE_OK;
    auto healthObject =
        std::make_shared<NsmChassisPCIeDevice<HealthIntf>>(chassisName, name);
    healthObject->invoke(pdiMethod(health),
                         HealthIntf::convertHealthTypeFromString(health));
    device->addStaticSensor(healthObject);
}

void createChassisPCIeDevicePCIeDevice(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& chassisName,
    dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string deviceType = PCIE_DEVICE_TYPE_SINGLE_FUNCTION;
    if (allCurrentIfaceProperties.count("DeviceType"))
    {
        deviceType =
            std::get<std::string>(allCurrentIfaceProperties.at("DeviceType"));
    }
    std::vector<uint64_t> functionIds = {0};
    if (allCurrentIfaceProperties.count("Functions"))
    {
        functionIds = std::get<std::vector<uint64_t>>(
            allCurrentIfaceProperties.at("Functions"));
    }

    const std::string inventoryObjPath = chassisInventoryBasePath /
                                         chassisName / "PCIeDevices" / name;

    auto pcieDeviceIntf = NsmAsioPCIeDeviceInterface::createSinglePortDevice(
        SensorManager::getInstance().getObjServer(), inventoryObjPath,
        deviceType, functionIds);

    if (!pcieDeviceIntf)
    {
        lg2::error("Failed to create PCIeDevice interface for {NAME} at {PATH}",
                   "NAME", name, "PATH", inventoryObjPath);
        return;
    }

    device->addSensor(
        std::make_shared<NsmPCIeDeviceLinkSpeedAsio>(
            name, "NSM_ChassisPCIeDevice", pcieDeviceIntf, 0, false),
        PCIE_LINK_SPEED_PCIE_DEVICE_PRIORITY);

    for (auto& id : functionIds)
    {
        auto function = std::make_shared<NsmPCIeDeviceFunctionAsio>(
            name, "NSM_ChassisPCIeDevice", pcieDeviceIntf, 0, id);
        device->addStaticSensor(function);
    }

    if (device->getDeviceType() == NSM_DEV_ID_GPU)
    {
#if defined(ENABLE_PCIE_AER_ERROR)
        auto aerErrorIntf = std::make_shared<NsmAERErrorStatusIntf>(
            utils::DBusHandler::getBus(), inventoryObjPath.c_str(), 0, device);
        auto aerErrorSensor = std::make_shared<NsmPCIeAERErrorStatus>(
            name, "PCIeAerErrorStatus", aerErrorIntf, 0);
        aerErrorIntf->linkAerStatusSensor(aerErrorSensor);
        device->addSensor(aerErrorSensor, AER_ERR_SENSOR_PRIORITY);
#endif
    }
}

void createChassisPCIeDeviceMultiPortPCIeDevice(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& chassisName,
    dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string deviceType = PCIE_DEVICE_TYPE_SINGLE_FUNCTION;
    if (allCurrentIfaceProperties.count("DeviceType"))
    {
        deviceType =
            std::get<std::string>(allCurrentIfaceProperties.at("DeviceType"));
    }
    std::vector<uint64_t> functionIds = {0};
    if (allCurrentIfaceProperties.count("Functions"))
    {
        functionIds = std::get<std::vector<uint64_t>>(
            allCurrentIfaceProperties.at("Functions"));
    }
    uint64_t upstreamPortCount = 1;
    if (allCurrentIfaceProperties.count("UpstreamPortCount"))
    {
        upstreamPortCount = std::get<uint64_t>(
            allCurrentIfaceProperties.at("UpstreamPortCount"));
    }

    const std::string inventoryObjPath = chassisInventoryBasePath /
                                         chassisName / "PCIeDevices" / name;
    auto pcieDeviceIntf = std::make_shared<PCIeDeviceIntf>(
        utils::DBusHandler::getBus(), inventoryObjPath.c_str());
    pcieDeviceIntf->deviceType(
        PCIeDeviceIntf::convertDeviceTypesFromString(deviceType));

    auto pcieDeviceObject = NsmChassisPCIeDevice<PCIeDeviceIntf>(
        name, "NSM_ChassisPCIeDevice", inventoryObjPath, pcieDeviceIntf);
    device->addSensor(std::make_shared<NsmPCIeLinkSpeed<PCIeDeviceIntf>>(
                          pcieDeviceObject, upstreamPortCount, true),
                      PCIE_LINK_SPEED_PCIE_DEVICE_PRIORITY);

    for (auto& id : functionIds)
    {
        auto function = std::make_shared<NsmPCIeFunction>(pcieDeviceObject, id,
                                                          0, 0, 0);
        device->addStaticSensor(function);
    }

    // Add single sensor to query Group 7 and set Function{N}BusNumber for all
    // functions
    auto busNumberSensor = std::make_shared<NsmPCIeECCGroup7>(
        name, "PCIeBusNumber", pcieDeviceIntf, functionIds, 0, 0, 0);
    device->addStaticSensor(busNumberSensor);
}

void createChassisPCIeDeviceRetimerAERErrorStatus(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& chassisName)
{
    const std::string inventoyObjPath = chassisInventoryBasePath / chassisName /
                                        "PCIeDevices" / name;
    auto aerErrorIntf = std::make_shared<AERErrorStatusIntf>(
        utils::DBusHandler::getBus(), inventoyObjPath.c_str());
    auto aerErrorSensor = std::make_shared<NsmPCIeECCGroup9>(
        name, "PCIeAerErrorStatus", inventoyObjPath, aerErrorIntf, 0, 0, 0);
    device->addSensor(aerErrorSensor, AER_ERR_SENSOR_PRIORITY);
}

#if defined(ENABLE_PCIE_LTSSM_STATE)
void createChassisPCIeDeviceLTSSMState(
    std::shared_ptr<NsmDevice> device, std::string& name,
    dbus::PropertyMap& allCurrentIfaceProperties)
{
    uint64_t deviceIndex{};
    if (allCurrentIfaceProperties.count("DeviceIndex"))
    {
        deviceIndex =
            std::get<uint64_t>(allCurrentIfaceProperties.at("DeviceIndex"));
    }
    bool priority = false;
    std::string inventoryObjPath{};
    if (allCurrentIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("InventoryObjPath"));
    }

    auto ltssmStateObject = NsmChassisPCIeDevice<LTSSMStateIntf>(
        name, dbus::Interfaces{inventoryObjPath});
    device->addSensor(
        std::make_shared<NsmPCIeLTSSMState>(ltssmStateObject, deviceIndex),
        priority);
}
#endif

#if defined(ENABLE_CLOCK_OUTPUT_STATE)
void createChassisPCIeDeviceClockOutputEnableState(
    std::shared_ptr<NsmDevice> device, std::string& name,
    const std::string& chassisName,
    dbus::PropertyMap& allCurrentIfaceProperties)
{
    uint64_t instanceNumber{};
    if (allCurrentIfaceProperties.count("InstanceNumber"))
    {
        instanceNumber =
            std::get<uint64_t>(allCurrentIfaceProperties.at("InstanceNumber"));
    }

    auto deviceType = (NsmDeviceIdentification)device->getDeviceType();
    auto pcieRefClockObject =
        NsmChassisPCIeDevice<PCIeRefClockIntf>(chassisName, name);

    auto pcieRefClock =
        std::make_shared<NsmClockOutputEnableState<PCIeRefClockIntf>>(
            pcieRefClockObject, PCIE_CLKBUF_INDEX, deviceType, instanceNumber);
    device->addSensor(pcieRefClock, CLOCK_OUTPUT_ENABLE_STATE_PRIORITY);

    if (deviceType == NSM_DEV_ID_GPU)
    {
        auto nvLinkRefClockObject =
            NsmChassisPCIeDevice<NVLinkRefClockIntf>(chassisName, name);
        auto nvLinkRefClock =
            std::make_shared<NsmClockOutputEnableState<NVLinkRefClockIntf>>(
                nvLinkRefClockObject, NVHS_CLKBUF_INDEX, deviceType,
                instanceNumber);
        device->addSensor(nvLinkRefClock, CLOCK_OUTPUT_ENABLE_STATE_PRIORITY);
    }
}
#endif

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createChassisPCIeDeviceUUIDFeature([[maybe_unused]] SensorManager& manager,
                                       const DeviceContext& dev,
                                       const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    // Starts empty; update() reads the device UUID over MCTP.
    dev.device->addStaticSensor(
        std::make_shared<NsmChassisPCIeDevice<UuidIntf>>(dev.name, name));
    co_return NSM_SUCCESS;
}

requester::Coroutine createChassisPCIeDeviceAssociationsFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));

    std::vector<utils::Association> associations;
    addAssociations(associations, params, "ChassisAssociationPaths", "chassis",
                    "pciedevice");
    if (associations.empty())
    {
        co_return NSM_SUCCESS;
    }

    auto associationsObject =
        std::make_shared<NsmChassisPCIeDevice<AssociationDefinitionsIntf>>(
            dev.name, name);
    associationsObject->invoke(pdiMethod(associations),
                               utils::getAssociations(associations));
    dev.device->addStaticSensor(associationsObject);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createChassisPCIeDeviceAssetFeature([[maybe_unused]] SensorManager& manager,
                                        const DeviceContext& dev,
                                        const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    createChassisPCIeDeviceAsset(dev.device, name, dev.name);
    co_return NSM_SUCCESS;
}

requester::Coroutine createChassisPCIeDeviceHealthFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    createChassisPCIeDeviceHealth(dev.device, name, dev.name);
    co_return NSM_SUCCESS;
}

requester::Coroutine createChassisPCIeDevicePCIeDeviceFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    auto attributes = params;
    createChassisPCIeDevicePCIeDevice(dev.device, name, dev.name, attributes);
    co_return NSM_SUCCESS;
}

requester::Coroutine createChassisPCIeDeviceMultiPortPCIeDeviceFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    auto attributes = params;
    createChassisPCIeDeviceMultiPortPCIeDevice(dev.device, name, dev.name,
                                               attributes);
    co_return NSM_SUCCESS;
}

requester::Coroutine createChassisPCIeDeviceRetimerAERErrorStatusFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    createChassisPCIeDeviceRetimerAERErrorStatus(dev.device, name, dev.name);
    co_return NSM_SUCCESS;
}

#if defined(ENABLE_PCIE_LTSSM_STATE)
requester::Coroutine createChassisPCIeDeviceLTSSMStateFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    auto attributes = params;
    createChassisPCIeDeviceLTSSMState(dev.device, name, attributes);
    co_return NSM_SUCCESS;
}
#endif

#if defined(ENABLE_CLOCK_OUTPUT_STATE)
requester::Coroutine createChassisPCIeDeviceClockOutputEnableStateFeature(
    [[maybe_unused]] SensorManager& manager, const DeviceContext& dev,
    const dbus::PropertyMap& params)
{
    auto name = std::get<std::string>(params.at("Name"));
    auto attributes = params;
    attributes.try_emplace("InstanceNumber", uint64_t{dev.instance});
    createChassisPCIeDeviceClockOutputEnableState(dev.device, name, dev.name,
                                                  attributes);
    co_return NSM_SUCCESS;
}
#endif

} // namespace nsm::features
