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

#include "nsmChassis.hpp"

#include "nsmDevice.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmSoftwareSettings.hpp"
#include "utils.hpp"

namespace nsm
{

void nsmChassisCreateSensors(SensorManager& manager,
                             const std::string& interface,
                             const std::string& objPath)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration.NSM_Chassis";

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_Chassis")
    {
        auto deviceType =
            (NsmDeviceIdentification)utils::DBusHandler()
                .getDbusProperty<uint64_t>(objPath.c_str(), "DeviceType",
                                           baseInterface.c_str());
        auto chassisUuid = std::make_shared<NsmChassis<UuidIntf>>(name);
        auto deviceUuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "DEVICE_UUID", interface.c_str());
        chassisUuid->pdi().uuid(deviceUuid);
        device->addStaticSensor(chassisUuid);
        if (deviceType == NSM_DEV_ID_BASEBOARD)
        {
            auto pCIeRefClock =
                std::make_shared<NsmChassis<PCIeRefClockIntf>>(name);
            device->addStaticSensor(pCIeRefClock);
        }
    }
    else if (type == "NSM_Asset")
    {
        auto chassisAsset = NsmChassis<AssetIntf>(name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        chassisAsset.pdi().manufacturer(manufacturer);
        auto eid = manager.getEid(device);
        // create sensor
        auto partNumber = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            chassisAsset, BOARD_PART_NUMBER);
        auto serialNumber = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            chassisAsset, SERIAL_NUMBER);
        auto model = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            chassisAsset, MARKETING_NAME);
        device->addStaticSensor(partNumber).update(manager, eid).detach();
        device->addStaticSensor(serialNumber).update(manager, eid).detach();
        device->addStaticSensor(model).update(manager, eid).detach();
    }
    else if (type == "NSM_ChassisType")
    {
        auto chassisType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "ChassisType", interface.c_str());
        auto chassis = std::make_shared<NsmChassis<ChassisIntf>>(name);
        chassis->pdi().type(
            ChassisIntf::convertChassisTypeFromString(chassisType));
        device->addStaticSensor(chassis);
    }
    else if (type == "NSM_Dimension")
    {
        auto chassisDimension = NsmChassis<DimensionIntf>(name);
        auto eid = manager.getEid(device);
        auto depth = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
            chassisDimension, PRODUCT_LENGTH);
        auto width = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
            chassisDimension, PRODUCT_WIDTH);
        auto height = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
            chassisDimension, PRODUCT_HEIGHT);
        device->addStaticSensor(depth).update(manager, eid).detach();
        device->addStaticSensor(width).update(manager, eid).detach();
        device->addStaticSensor(height).update(manager, eid).detach();
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
        chassisHealth->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto chassisLocation = std::make_shared<NsmChassis<LocationIntf>>(name);
        chassisLocation->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(chassisLocation);
    }
    else if (type == "NSM_LocationCode")
    {
        auto locationCode = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationCode", interface.c_str());
        auto chassisLocationCode =
            std::make_shared<NsmChassis<LocationCodeIntf>>(name);
        chassisLocationCode->pdi().locationCode(locationCode);
        device->addStaticSensor(chassisLocationCode);
    }
    else if (type == "NSM_PowerLimit")
    {
        auto chassisPowerLimit = NsmChassis<PowerLimitIntf>(name);
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        device->addSensor(
            std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT),
            priority);
        device->addSensor(
            std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT),
            priority);
    }
    else if (type == "NSM_OperationalStatus")
    {
        auto deviceType =
            (NsmDeviceIdentification)utils::DBusHandler()
                .getDbusProperty<uint64_t>(objPath.c_str(), "DeviceType",
                                           baseInterface.c_str());
        if (deviceType != NSM_DEV_ID_BASEBOARD)
        {
            throw std::runtime_error(
                "Cannot use NSM_OperationalStatus for different device than Baseboard");
        }
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto inventoryObjPaths =
            utils::DBusHandler().getDbusProperty<dbus::Interfaces>(
                objPath.c_str(), "InventoryObjPaths", interface.c_str());
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto gpuOperationalStatus = NsmInterfaceProvider<OperationalStatusIntf>(
            name, type, inventoryObjPaths);
        device->addSensor(std::make_shared<NsmGpuPresenceAndPowerStatus>(
                              gpuOperationalStatus, instanceId),
                          priority);
    }
    else if (type == "NSM_PowerState")
    {
        auto deviceType =
            (NsmDeviceIdentification)utils::DBusHandler()
                .getDbusProperty<uint64_t>(objPath.c_str(), "DeviceType",
                                           baseInterface.c_str());
        if (deviceType != NSM_DEV_ID_BASEBOARD)
        {
            throw std::runtime_error(
                "Cannot use NSM_PowerState for different device than Baseboard");
        }
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto inventoryObjPaths =
            utils::DBusHandler().getDbusProperty<dbus::Interfaces>(
                objPath.c_str(), "InventoryObjPaths", interface.c_str());
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        auto gpuPowerState =
            NsmInterfaceProvider<PowerStateIntf>(name, type, inventoryObjPaths);
        device->addSensor(
            std::make_shared<NsmPowerSupplyStatus>(gpuPowerState, instanceId),
            priority);
    }
    else if (type == "NSM_WriteProtect")
    {
        auto deviceType =
            (NsmDeviceIdentification)utils::DBusHandler()
                .getDbusProperty<uint64_t>(objPath.c_str(), "DeviceType",
                                           baseInterface.c_str());
        if (deviceType != NSM_DEV_ID_BASEBOARD)
        {
            throw std::runtime_error(
                "Cannot use NSM_WriteProtect for different device than Baseboard");
        }
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto settings = NsmChassis<SettingsIntf>(name);
        device->addStaticSensor(std::make_shared<NsmSoftwareSettings>(
            settings, deviceType, instanceId));
    }
}

std::vector<std::string> chassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_Chassis.ChassisType",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Dimension",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Location",
    "xyz.openbmc_project.Configuration.NSM_Chassis.LocationCode",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerLimit",
    "xyz.openbmc_project.Configuration.NSM_Chassis.OperationalStatus",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerState",
    "xyz.openbmc_project.Configuration.NSM_Chassis.WriteProtect"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisCreateSensors, chassisInterfaces)

} // namespace nsm
