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

#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

namespace nsm
{
template <typename IntfType>
requester::Coroutine
    NsmNVSwitchAndNicChassis<IntfType>::update(SensorManager& manager,
                                               eid_t eid)
{
    DeviceManager& deviceManager = DeviceManager::getInstance();
    auto uuid = utils::getUUIDFromEID(deviceManager.getEidTable(), eid);
    if (uuid)
    {
        if constexpr (std::is_same_v<IntfType, UuidIntf>)
        {
            auto nsmDevice = manager.getNsmDevice(*uuid);
            if (nsmDevice)
            {
                this->invoke(pdiMethod(uuid), nsmDevice->deviceUuid);
            }
            nsmDeviceAssociationIntf =
                manager.getObjServer().add_unique_interface(
                    chassisInventoryBasePath / this->getName() /
                        "NsmDeviceAssociation",
                    "xyz.openbmc_project.Configuration.NsmDeviceAssociation");
            nsmDeviceAssociationIntf->register_property("UUID", *uuid);
            nsmDeviceAssociationIntf->initialize();
        }
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine createNsmChassis(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath,
                                      const std::string baseType)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration." + baseType;

    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", baseInterface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", baseInterface.c_str());
    auto device = manager.getNsmDevice(uuid);

    if (type == baseType)
    {
        lg2::debug("createNsmChassis: {NAME}, {TYPE}", "NAME", name.c_str(),
                   "TYPE", baseType.c_str());
        auto chassisUuid = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(
            name, baseType);
        auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
            objPath.c_str(), "UUID", interface.c_str());

        // initial value update
        chassisUuid->invoke(pdiMethod(uuid), uuid);

        // add sensor
        device->addStaticSensor(chassisUuid);
    }
    else if (type == "NSM_Chassis")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(
            name, baseType);
        auto chassisType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "ChassisType", interface.c_str());

        // initial value update
        chassis->invoke(pdiMethod(type),
                        ChassisIntf::convertChassisTypeFromString(chassisType));

        device->addStaticSensor(chassis);
    }
    else if (type == "NSM_Asset")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassisAsset = NsmNVSwitchAndNicChassis<NsmAssetIntf>(name,
                                                                   baseType);

        auto manufacturer = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Manufacturer", interface.c_str());

        // initial value update
        chassisAsset.invoke(pdiMethod(manufacturer), manufacturer);

        // create sensor
        auto partNumberSensor =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
                chassisAsset, DEVICE_PART_NUMBER);
        auto serialNumberSensor =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(chassisAsset,
                                                                 SERIAL_NUMBER);
        auto modelSensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            chassisAsset, MARKETING_NAME);
        device->addStaticSensor(partNumberSensor);
        device->addStaticSensor(serialNumberSensor);
        device->addStaticSensor(modelSensor);
    }
    else if (type == "NSM_Health")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassisHealth =
            std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(name,
                                                                   baseType);
        auto health = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Health", interface.c_str());

        // initial value update
        chassisHealth->invoke(pdiMethod(health),
                              HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        lg2::debug("createNsmChassis: {NAME}, {BTYPE}_{TYPE}", "NAME",
                   name.c_str(), "BTYPE", baseType.c_str(), "TYPE",
                   type.c_str());
        auto chassisLocation =
            std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(name,
                                                                     baseType);

        auto locationType = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "LocationType", interface.c_str());

        // initial value update
        chassisLocation->invoke(
            pdiMethod(locationType),
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(chassisLocation);
    }
    else if (type == "NSM_PrettyName")
    {
        auto prettyName = co_await utils::coGetDbusProperty<std::string>(
            objPath.c_str(), "Name", interface.c_str());
        auto chassisPrettyName =
            std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(name,
                                                                 baseType);
        chassisPrettyName->invoke(pdiMethod(prettyName), prettyName);
        device->addStaticSensor(chassisPrettyName);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine createNsmNVSwitchChassis(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath)
{
    co_await createNsmChassis(manager, interface, objPath,
                              "NSM_NVSwitch_Chassis");
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine createNsmNVLinkMgmtNicChassis(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath)
{
    co_await createNsmChassis(manager, interface, objPath,
                              "NSM_NVLinkMgmtNic_Chassis");
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> nvSwitchChassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.PrettyName",
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
