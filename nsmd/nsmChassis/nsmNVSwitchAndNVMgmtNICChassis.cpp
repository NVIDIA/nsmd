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

#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"

#include "nsmChassis/nsmInventoryProperty.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

namespace nsm
{

void createNsmChassis(SensorManager& manager, const std::string& interface,
                      const std::string& objPath, const std::string baseType)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == baseType)
    {
        lg2::debug("createNsmChassis: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", baseType.c_str());
        auto chassisUuid = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(
            name, baseType);
        auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());

        // initial value update
        chassisUuid->pdi().uuid(uuid);

        // add sensor
        addSensor(device, chassisUuid);
    }
    else if (type == "NSM_Chassis")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(
            name, baseType);
        auto chassisType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "ChassisType", interface.c_str());

        // initial value update
        chassis->pdi().type(
            ChassisIntf::convertChassisTypeFromString(chassisType));

        addSensor(device, chassis);
    }
    else if (type == "NSM_Asset")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassisAsset = NsmNVSwitchAndNicChassis<AssetIntf>(name, baseType);

        auto manufacturer = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());
        auto model = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Model", interface.c_str());
        auto sku = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SKU", interface.c_str());
        auto serialNumber = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SerialNumber", interface.c_str());
        auto partNumber = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "PartNumber", interface.c_str());

        // initial value update
        chassisAsset.pdi().sku(sku);
        chassisAsset.pdi().model(model);
        chassisAsset.pdi().manufacturer(manufacturer);
        chassisAsset.pdi().serialNumber(serialNumber);
        chassisAsset.pdi().partNumber(partNumber);

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
    else if (type == "NSM_Health")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassisHealth =
            std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(name,
                                                                   baseType);
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());

        // initial value update
        chassisHealth->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassisLocation =
            std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(name,
                                                                     baseType);

        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());

        // initial value update
        chassisLocation->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        addSensor(device, chassisLocation);
    }
}

void createNsmNVSwitchChassis(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath)
{
    createNsmChassis(manager, interface, objPath, "NSM_NVSwitch_Chassis");
}

void createNsmNVLinkMgmtNicChassis(SensorManager& manager,
                                   const std::string& interface,
                                   const std::string& objPath)
{
    createNsmChassis(manager, interface, objPath, "NSM_NVLinkMgmtNic_Chassis");
}

std::vector<std::string> nvSwitchChassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Location"};

std::vector<std::string> nvLinkMgmtNicChassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Location"};

REGISTER_NSM_CREATION_FUNCTION(createNsmNVSwitchChassis,
                               nvSwitchChassisInterfaces)
REGISTER_NSM_CREATION_FUNCTION(createNsmNVLinkMgmtNicChassis,
                               nvLinkMgmtNicChassisInterfaces)
} // namespace nsm