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

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <unordered_map>

namespace nsm
{

void createChassisAssemblyArea(std::shared_ptr<NsmDevice> device,
                               const std::string& chassisName,
                               const std::string& name,
                               dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string physicalContext =
        std::get<std::string>(allCurrentIfaceProperties.at("PhysicalContext"));

    auto chassisArea =
        std::make_shared<NsmChassisAssembly<AreaIntf>>(chassisName, name);
    chassisArea->invoke(
        pdiMethod(physicalContext),
        AreaIntf::convertPhysicalContextTypeFromString(physicalContext));
    device->addStaticSensor(chassisArea);
}

void createChassisAssemblyAsset(std::shared_ptr<NsmDevice> device,
                                const std::string& chassisName,
                                const std::string& name,
                                dbus::PropertyMap& allCurrentIfaceProperties,
                                dbus::PropertyMap& allBaseIfaceProperties)
{
    std::string assetsName{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        assetsName =
            std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    // default part number for asset is Board part number
    std::string assemblyType{};
    if (allBaseIfaceProperties.count("AssemblyType"))
    {
        assemblyType =
            std::get<std::string>(allBaseIfaceProperties.at("AssemblyType"));
    }

    auto partNumberId = BOARD_PART_NUMBER;
    if (assemblyType == "Device")
    {
        partNumberId = DEVICE_PART_NUMBER;
    }
    else if (assemblyType == "Board")
    {
        partNumberId = BOARD_PART_NUMBER;
    }
    else if (assemblyType == "FRU")
    {
        partNumberId = FRU_PART_NUMBER;
    }
    else
    {
        lg2::debug(
            "NSM_Asset: Invalid AssemblyType={VAL} using default value BOARD_PART_NUMBER.",
            "VAL", assemblyType);
    }
    std::string vendor = MANUFACTURER_NVIDIA;

    auto assetObject = NsmChassisAssembly<NsmAssetIntf>(chassisName, name);
    assetObject.invoke(pdiMethod(manufacturer), vendor);
    assetObject.invoke(pdiMethod(name), assetsName);
    //  create sensor
    auto partNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, partNumberId);
    auto serialNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, SERIAL_NUMBER);
    const auto modelProperty = getModelInventoryProperty(
        device->getDeviceType(), device->getDeviceRole());
    auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, modelProperty);
    auto buildDate = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        assetObject, BUILD_DATE);
    device->addStaticSensor(partNumber);
    device->addStaticSensor(serialNumber);
    device->addStaticSensor(model);
    device->addStaticSensor(buildDate);
}

void createChassisAssemblyHealth(std::shared_ptr<NsmDevice> device,
                                 const std::string& chassisName,
                                 const std::string& name)
{
    std::string health = HEALTH_TYPE_OK;
    auto healthObject =
        std::make_shared<NsmChassisAssembly<HealthIntf>>(chassisName, name);
    healthObject->invoke(pdiMethod(health),
                         HealthIntf::convertHealthTypeFromString(health));
    device->addStaticSensor(healthObject);
}

void createChassisAssemblyLocation(std::shared_ptr<NsmDevice> device,
                                   const std::string& chassisName,
                                   const std::string& name,
                                   dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationType =
        std::get<std::string>(allCurrentIfaceProperties.at("LocationType"));
    auto locationObject =
        std::make_shared<NsmChassisAssembly<LocationIntf>>(chassisName, name);
    locationObject->invoke(
        pdiMethod(locationType),
        LocationIntf::convertLocationTypesFromString(locationType));
    device->addStaticSensor(locationObject);
}

requester::Coroutine
    nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath)
{
    std::string baseInterface =
        "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";
    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, baseInterface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    dbus::PropertyMap allCurrentIfaceProperties =
        co_await utils::coGetAllDbusProperty(utils::entityManagerServiceStr,
                                             objPath, interface);

    std::string chassisName{};
    std::string name{};
    std::string type{};
    uuid_t uuid{};

    if (allBaseIfaceProperties.count("ChassisName"))
    {
        chassisName =
            std::get<std::string>(allBaseIfaceProperties.at("ChassisName"));
    }
    if (allBaseIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
    }
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);

    if (type == "NSM_ChassisAssembly")
    {
        auto assemblyObject =
            std::make_shared<NsmChassisAssembly<AssemblyIntf>>(chassisName,
                                                               name);
        device->addStaticSensor(assemblyObject);
    }
    else if (type == "NSM_Chassis_Attributes")
    {
        createChassisAssemblyAsset(device, chassisName, name,
                                   allCurrentIfaceProperties,
                                   allBaseIfaceProperties);
        createChassisAssemblyHealth(device, chassisName, name);
        if (allCurrentIfaceProperties.count("PhysicalContext"))
        {
            createChassisAssemblyArea(device, chassisName, name,
                                      allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            createChassisAssemblyLocation(device, chassisName, name,
                                          allCurrentIfaceProperties);
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> chassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.ChassisAttributes"};
REGISTER_NSM_CREATION_FUNCTION(nsmChassisAssemblyCreateSensors,
                               chassisAssemblyInterfaces)

} // namespace nsm
