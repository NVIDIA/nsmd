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

#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <cstdint>

namespace nsm
{

requester::Coroutine
    nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";

    auto chassisName = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_ChassisAssembly")
    {
        auto assemblyObject =
            std::make_shared<NsmChassisAssembly<AssemblyIntf>>(chassisName,
                                                               name);
        device->addStaticSensor(assemblyObject);
    }
    else if (type == "NSM_Area")
    {
        auto physicalContext = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "PhysicalContext", interface.c_str());
        auto chassisArea =
            std::make_shared<NsmChassisAssembly<AreaIntf>>(chassisName, name);
        chassisArea->pdi().physicalContext(
            AreaIntf::convertPhysicalContextTypeFromString(physicalContext));
        device->addStaticSensor(chassisArea);
    }
    else if (type == "NSM_Asset")
    {
        auto vendor = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Vendor", interface.c_str());
        auto assetsName = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        // default part number for asset is Board part number
        auto deviceAssembly = co_await utils::coGetDbusProperty<bool>(
            objPath.c_str(), "DeviceAssembly", baseInterface.c_str());
        auto partNumberId = deviceAssembly ? DEVICE_PART_NUMBER
                                           : BOARD_PART_NUMBER;

        auto assetObject = NsmChassisAssembly<AssetIntf>(chassisName, name);
        assetObject.pdi().manufacturer(vendor);
        assetObject.pdi().name(assetsName);
        //  create sensor
        auto partNumber = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, partNumberId);
        auto serialNumber = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, SERIAL_NUMBER);
        auto model = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, MARKETING_NAME);
        auto buildDate = std::make_shared<NsmInventoryProperty<AssetIntf>>(
            assetObject, BUILD_DATE);
        device->addStaticSensor(partNumber);
        device->addStaticSensor(serialNumber);
        device->addStaticSensor(model);
        device->addStaticSensor(buildDate);
    }
    else if (type == "NSM_Health")
    {
        auto health = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());
        auto healthObject =
            std::make_shared<NsmChassisAssembly<HealthIntf>>(chassisName, name);
        healthObject->pdi().health(
            HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_Location")
    {
        auto locationType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());
        auto locationObject =
            std::make_shared<NsmChassisAssembly<LocationIntf>>(chassisName,
                                                               name);
        locationObject->pdi().locationType(
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(locationObject);
    }
    co_return NSM_SUCCESS;
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
