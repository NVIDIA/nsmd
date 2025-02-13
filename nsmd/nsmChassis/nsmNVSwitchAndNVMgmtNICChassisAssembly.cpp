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

#include "dBusAsyncUtils.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"

#include <utils.hpp>

namespace nsm
{

requester::Coroutine createNsmChassisAssembly(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath,
                                              const std::string baseType)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto chassisName = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "ChassisName", baseInterface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
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
        auto physicalContext = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "PhysicalContext", interface.c_str());
        auto assemblyArea =
            std::make_shared<NsmNVSwitchAndNicChassisAssembly<AreaIntf>>(
                chassisName, name, baseType);
        assemblyArea->pdi().physicalContext(
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

        auto assemblyName = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto vendor = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Vendor", interface.c_str());

        // initial value update
        assetObject.pdi().name(assemblyName);
        assetObject.pdi().manufacturer(vendor);

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
        auto health = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());

        healthObject->pdi().health(
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

        auto locationType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());

        locationObject->pdi().locationType(
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
