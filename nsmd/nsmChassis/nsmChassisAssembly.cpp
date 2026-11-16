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
#include "nsmChassisFeatures.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <format>
#include <tuple>
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

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createChassisAssemblyFeature([[maybe_unused]] SensorManager& manager,
                                 const DeviceContext& dev,
                                 const dbus::PropertyMap& params)
{
    // Holds both the base (AssemblyType) and attribute (Name, ...) keys the
    // helpers read; a copy because they take non-const maps.
    auto attributes = params;

    // Everything that can fail is checked before the index is taken, so a
    // failed assembly does not leave a gap or a partial object.
    for (const char* key :
         {"AssemblyType", "Name", "PhysicalContext", "LocationType"})
    {
        auto it = attributes.find(key);
        if (it != attributes.end() &&
            !std::holds_alternative<std::string>(it->second))
        {
            lg2::error("Chassis assembly of {NAME}: {KEY} must be a string",
                       "NAME", dev.name, "KEY", key);
            co_return NSM_SW_ERROR_DATA;
        }
    }
    const bool hasArea = attributes.contains("PhysicalContext");
    if (hasArea)
    {
        std::ignore = AreaIntf::convertPhysicalContextTypeFromString(
            std::get<std::string>(attributes.at("PhysicalContext")));
    }
    const bool hasLocation = attributes.contains("LocationType");
    if (hasLocation)
    {
        std::ignore = LocationIntf::convertLocationTypesFromString(
            std::get<std::string>(attributes.at("LocationType")));
    }

    const auto name = std::format("Assembly{}", dev.nextIndex("Assembly"));

    dev.device->addStaticSensor(
        std::make_shared<NsmChassisAssembly<AssemblyIntf>>(dev.name, name));
    createChassisAssemblyAsset(dev.device, dev.name, name, attributes,
                               attributes);
    createChassisAssemblyHealth(dev.device, dev.name, name);
    if (hasArea)
    {
        createChassisAssemblyArea(dev.device, dev.name, name, attributes);
    }
    if (hasLocation)
    {
        createChassisAssemblyLocation(dev.device, dev.name, name, attributes);
    }
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
