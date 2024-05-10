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

#include "nsmChassisAssembly.hpp"

#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

namespace nsm
{

void nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";

    auto chassisName = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_ChassisAssembly")
    {
        auto assemblyObject =
            std::make_shared<NsmChassisAssembly<AssemblyIntf>>(chassisName,
                                                               name);
        addSensor(device, assemblyObject);
    }
    else if (type == "NSM_Area")
    {
        auto physicalContext =
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "PhysicalContext", interface.c_str());
        auto chassisArea =
            std::make_shared<NsmChassisAssembly<AreaIntf>>(chassisName, name);
        chassisArea->pdi().physicalContext(
            AreaIntf::convertPhysicalContextTypeFromString(physicalContext));
        addSensor(device, chassisArea);
    }
    else if (type == "NSM_Asset")
    {
        auto vendor = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Vendor", interface.c_str());
        auto assetsName = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto assetObject = NsmChassisAssembly<AssetIntf>(chassisName, name);
        assetObject.pdi().manufacturer(vendor);
        assetObject.pdi().name(assetsName);
        // create sensor
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, BOARD_PART_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, SERIAL_NUMBER));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, MARKETING_NAME));
        addSensor(manager, device,
                  std::make_shared<NsmInventoryProperty<AssetIntf>>(
                      assetObject, BUILD_DATE));
    }
    else if (type == "NSM_Health")
    {
        auto health = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject =
            std::make_shared<NsmChassisAssembly<HealthIntf>>(chassisName, name);
        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        addSensor(device, healthObject);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = utils::DBusHandler().getDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto locationObject =
            std::make_shared<NsmChassisAssembly<LocationIntf>>(chassisName,
                                                               name);
        locationObject->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        addSensor(device, locationObject);
    }
}
std::vector<std::string> chassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.Area",
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.Location"};
REGISTER_NSM_CREATION_FUNCTION(nsmChassisAssemblyCreateSensors,
                               chassisAssemblyInterfaces)

} // namespace nsm
