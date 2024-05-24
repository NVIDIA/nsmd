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

#include "nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

#include "nsmChassis/nsmInventoryProperty.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"

#include <utils.hpp>

namespace nsm
{

void createNsmChassisAssembly(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath,
                              const std::string baseType)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == baseType)
    {
        lg2::debug("createNsmChassis: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", baseType.c_str());
        auto assemblyObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
                chassisName, name, baseType);
        device->addStaticSensor(assemblyObject);
    }
    else if (type == "NSM_Asset")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto assetObject = NsmNVSwitchAndNicChassisAssembly<AssetIntf>(
            chassisName, name, baseType);

        auto assemblyName = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto model = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Model", interface.c_str());
        auto vendor = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Vendor", interface.c_str());
        auto sku = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SKU", interface.c_str());
        auto serialNumber = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "SerialNumber", interface.c_str());
        auto productionDate = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "ProductionDate", interface.c_str());

        // initial value update
        assetObject.pdi().name(assemblyName);
        assetObject.pdi().model(model);
        assetObject.pdi().manufacturer(vendor);
        assetObject.pdi().sku(sku);
        assetObject.pdi().serialNumber(serialNumber);
        assetObject.pdi().buildDate(productionDate);

        // create sensor
        auto eid = manager.getEid(device);
        auto partNumberSensor =
            std::make_shared<NsmInventoryProperty<AssetIntf>>(
                assetObject, DEVICE_PART_NUMBER);
        auto serialNumberSensor =
            std::make_shared<NsmInventoryProperty<AssetIntf>>(assetObject,
                                                              SERIAL_NUMBER);
        auto modelSensor = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, MARKETING_NAME);
        auto buildDateSensor =
            std::make_shared<NsmInventoryProperty<AssetIntf>>(assetObject,
                                                              BUILD_DATE);
        device->addStaticSensor(partNumberSensor).update(manager, eid).detach();
        device->addStaticSensor(serialNumberSensor)
            .update(manager, eid)
            .detach();
        device->addStaticSensor(modelSensor).update(manager, eid).detach();
        device->addStaticSensor(buildDateSensor).update(manager, eid).detach();
    }
    else if (type == "NSM_Health")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto healthObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
                chassisName, name, baseType);
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());

        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_Location")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{Type}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto locationObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
                chassisName, name, baseType);

        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());

        locationObject->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(locationObject);
    }
}

void createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath)
{
    createNsmChassisAssembly(manager, interface, objPath,
                             "NSM_NVSwitch_ChassisAssembly");
}

void createNsmNVLinkMgmtNicChassisAssembly(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath)
{
    createNsmChassisAssembly(manager, interface, objPath,
                             "NSM_NVLinkMgmtNic_ChassisAssembly");
}

std::vector<std::string> nvSwitchChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Location"};

std::vector<std::string> nvLinkMgmtNicChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Location"};

REGISTER_NSM_CREATION_FUNCTION(createNsmNVSwitchChassisAssembly,
                               nvSwitchChassisAssemblyInterfaces)
REGISTER_NSM_CREATION_FUNCTION(createNsmNVLinkMgmtNicChassisAssembly,
                               nvLinkMgmtNicChassisAssemblyInterfaces)
} // namespace nsm
