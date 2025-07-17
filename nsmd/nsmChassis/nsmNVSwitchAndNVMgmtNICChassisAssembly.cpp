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

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"

#include <unordered_map>

namespace nsm
{

requester::Coroutine createNsmChassisAssembly(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath,
                                              const std::string baseType)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    dbus::PropertyMap allBaseIfaceProperties;
    auto rc = co_await utils::coGetCachedBaseProperties(objPath, baseInterface,
                                                        allBaseIfaceProperties);
    if (rc != NSM_SUCCESS)
    {
        co_return rc;
    }
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allBaseIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allBaseIfaceProperties.at("Name"));
    }
    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    std::string chassisName{};
    if (allBaseIfaceProperties.count("ChassisName"))
    {
        chassisName =
            std::get<std::string>(allBaseIfaceProperties.at("ChassisName"));
    }
    uuid_t uuid{};
    if (allBaseIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allBaseIfaceProperties.at("UUID"));
    }

    auto device = manager.getNsmDevice(uuid);

    if (type == baseType)
    {
        lg2::debug("createNsmChassis: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", baseType.c_str());
        auto assemblyObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
                chassisName, name, baseType);
        device->addStaticSensor(assemblyObject);

        // add revision
        auto revisionObject = NsmNVSwitchAndNicChassisAssembly<RevisionIntf>(
            chassisName, name, baseType);
        auto versionSensor =
            std::make_shared<NsmInventoryProperty<RevisionIntf>>(
                revisionObject, INFO_ROM_VERSION);
        device->addStaticSensor(versionSensor);
    }
    else if (type == "NSM_Area")
    {
        std::string physicalContext{};
        if (allCurrentIfaceProperties.count("PhysicalContext"))
        {
            physicalContext = std::get<std::string>(
                allCurrentIfaceProperties.at("PhysicalContext"));
        }

        auto assemblyArea =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<AreaIntf>>(
                chassisName, name, baseType);
        assemblyArea->invoke(
            pdiMethod(physicalContext),
            AreaIntf::convertPhysicalContextTypeFromString(physicalContext));
        device->addStaticSensor(assemblyArea);
    }
    else if (type == "NSM_Asset")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto assetObject = NsmNVSwitchAndNicChassisAssembly<NsmAssetIntf>(
            chassisName, name, baseType);

        std::string assemblyName{};
        if (allCurrentIfaceProperties.count("Name"))
        {
            assemblyName =
                std::get<std::string>(allCurrentIfaceProperties.at("Name"));
        }
        std::string vendor{};
        if (allCurrentIfaceProperties.count("Vendor"))
        {
            vendor =
                std::get<std::string>(allCurrentIfaceProperties.at("Vendor"));
        }

        // initial value update
        assetObject.invoke(pdiMethod(name), assemblyName);
        assetObject.invoke(pdiMethod(manufacturer), vendor);

        // create sensor
        auto partNumberSensor =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                assetObject, DEVICE_PART_NUMBER);
        auto serialNumberSensor =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(assetObject,
                                                                 SERIAL_NUMBER);
        auto modelSensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            assetObject, MARKETING_NAME);
        auto buildDateSensor =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(assetObject,
                                                                 BUILD_DATE);
        device->addStaticSensor(partNumberSensor);
        device->addStaticSensor(serialNumberSensor);
        device->addStaticSensor(modelSensor);
        device->addStaticSensor(buildDateSensor);
    }
    else if (type == "NSM_Health")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto healthObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
                chassisName, name, baseType);
        std::string health{};
        if (allCurrentIfaceProperties.count("Health"))
        {
            health =
                std::get<std::string>(allCurrentIfaceProperties.at("Health"));
        }

        healthObject->invoke(pdiMethod(health),
                             HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(healthObject);
    }
    else if (type == "NSM_Location")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto locationObject =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
                chassisName, name, baseType);

        std::string locationType{};
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            locationType = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationType"));
        }

        locationObject->invoke(
            pdiMethod(locationType),
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(locationObject);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath)
{
    co_await createNsmChassisAssembly(manager, interface, objPath,
                                      "NSM_NVSwitch_ChassisAssembly");
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createNsmNVLinkMgmtNicChassisAssembly(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath)
{
    co_await createNsmChassisAssembly(manager, interface, objPath,
                                      "NSM_NVLinkMgmtNic_ChassisAssembly");
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> nvSwitchChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Area",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly.Location"};

std::vector<std::string> nvLinkMgmtNicChassisAssemblyInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Area",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Health",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly.Location"};

REGISTER_NSM_CREATION_FUNCTION(createNsmNVSwitchChassisAssembly,
                               nvSwitchChassisAssemblyInterfaces)
REGISTER_NSM_CREATION_FUNCTION(createNsmNVLinkMgmtNicChassisAssembly,
                               nvLinkMgmtNicChassisAssemblyInterfaces)
} // namespace nsm
