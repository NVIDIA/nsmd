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
        auto chassisUuid = std::make_shared<NsmChassis<UuidIntf>>(name);
        chassisUuid->pdi().uuid(uuid);
        addSensor(device, chassisUuid);
    }
    else if (type == "NSM_Asset")
    {
        auto chassisAsset = NsmChassis<AssetIntf>(name);
        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        chassisAsset.pdi().manufacturer(manufacturer);
        // create sensor
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      chassisAsset, BOARD_PART_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      chassisAsset, SERIAL_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      chassisAsset, MARKETING_NAME));
    }
    else if (type == "NSM_ChassisType")
    {
        auto chassisType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "ChassisType", interface.c_str());
        auto chassis = std::make_shared<NsmChassis<ChassisIntf>>(name);
        chassis->pdi().type(
            ChassisIntf::convertChassisTypeFromString(chassisType));
        addSensor(device, chassis);
    }
    else if (type == "NSM_Dimension")
    {
        auto chassisDimension = NsmChassis<DimensionIntf>(name);
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<DimensionIntf>>(
                      chassisDimension, PRODUCT_LENGTH));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<DimensionIntf>>(
                      chassisDimension, PRODUCT_WIDTH));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<DimensionIntf>>(
                      chassisDimension, PRODUCT_HEIGHT));
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
        chassisHealth->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto chassisLocation = std::make_shared<NsmChassis<LocationIntf>>(name);
        chassisLocation->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        addSensor(device, chassisLocation);
    }
    else if (type == "NSM_LocationCode")
    {
        auto locationCode = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationCode", interface.c_str());
        auto chassisLocationCode =
            std::make_shared<NsmChassis<LocationCodeIntf>>(name);
        chassisLocationCode->pdi().locationCode(locationCode);
        addSensor(device, chassisLocationCode);
    }
    else if (type == "NSM_PowerLimit")
    {
        auto chassisPowerLimit = NsmChassis<PowerLimitIntf>(name);
        auto priority = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "Priority", interface.c_str());
        addSensor(device,
                  std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                      chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT),
                  priority);
        addSensor(device,
                  std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                      chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT),
                  priority);
    }
    else if (type == "NSM_OperationalStatus")
    {
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto inventoryObjPaths =
            utils::DBusHandler().getDbusProperty<dbus::Interfaces>(
                objPath.c_str(), "InventoryObjPaths", interface.c_str());
        auto gpuOperationalStatus = NsmInterfaceProvider<OperationalStatusIntf>(
            name, type, inventoryObjPaths);
        addSensor(device,
                  std::make_shared<NsmGpuPresenceAndPowerStatus>(
                      gpuOperationalStatus, instanceId),
                  objPath, interface);
    }
    else if (type == "NSM_PowerState")
    {
        auto instanceId = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "InstanceNumber", baseInterface.c_str());
        auto inventoryObjPaths =
            utils::DBusHandler().getDbusProperty<dbus::Interfaces>(
                objPath.c_str(), "InventoryObjPaths", interface.c_str());
        auto gpuPowerState =
            NsmInterfaceProvider<PowerStateIntf>(name, type, inventoryObjPaths);
        addSensor(
            device,
            std::make_shared<NsmPowerSupplyStatus>(gpuPowerState, instanceId),
            objPath, interface);
    }
}

std::vector<std::string> chassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Dimension",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Location",
    "xyz.openbmc_project.Configuration.NSM_Chassis.LocationCode",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerLimit",
    "xyz.openbmc_project.Configuration.NSM_Chassis.OperationalStatus",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerState"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisCreateSensors, chassisInterfaces)

} // namespace nsm
