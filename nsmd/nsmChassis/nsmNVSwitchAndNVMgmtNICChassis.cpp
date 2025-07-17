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

#include "../../common/coroutine.hpp"
#include "../../common/utils.hpp"
#include "dBusAsyncUtils.hpp"
#include "deviceManager.hpp"
#include "nsmCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"

#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine
    NsmNVSwitchAndNicChassis<IntfType>::update(SensorManager& manager,
                                               eid_t eid)
{
    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        // For UuidIntf, we need to get the device UUID from the device manager.
        DeviceManager& deviceManager = DeviceManager::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(manager, eid, deviceManager,
                                         deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);
            nsmDeviceAssociationIntf =
                manager.getObjServer().add_unique_interface(
                    chassisInventoryBasePath / this->getName() /
                        "NsmDeviceAssociation",
                    "xyz.openbmc_project.Configuration.NsmDeviceAssociation");
            nsmDeviceAssociationIntf->register_property("UUID", deviceUuid);
            nsmDeviceAssociationIntf->initialize();
            co_return NSM_SUCCESS;
        }
        co_return NSM_ERROR;
    }
    // For other interfaces, we don't need to update anything.
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine createNsmChassis(SensorManager& manager,
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
        auto chassisUuid = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(
            name, baseType);
        uuid_t uuid{};
        if (allCurrentIfaceProperties.count("UUID"))
        {
            uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
        }

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
        std::string chassisType{};
        if (allCurrentIfaceProperties.count("ChassisType"))
        {
            chassisType = std::get<std::string>(
                allCurrentIfaceProperties.at("ChassisType"));
        }

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

        std::string manufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            manufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

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
        std::string health{};
        if (allCurrentIfaceProperties.count("Health"))
        {
            health =
                std::get<std::string>(allCurrentIfaceProperties.at("Health"));
        }

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

        std::string locationType{};
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            locationType = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationType"));
        }

        // initial value update
        chassisLocation->invoke(
            pdiMethod(locationType),
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(chassisLocation);
    }
    else if (type == "NSM_LocationCode")
    {
        std::string locationCode{};
        if (allCurrentIfaceProperties.count("LocationCode"))
        {
            locationCode = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationCode"));
        }

        auto chassisLocationCode =
            std::make_shared<NsmNVSwitchAndNicChassis<LocationCodeIntf>>(
                name, baseType);
        chassisLocationCode->invoke(pdiMethod(locationCode), locationCode);
        device->addStaticSensor(chassisLocationCode);
    }
    else if (type == "NSM_PrettyName")
    {
        std::string prettyName{};
        if (allCurrentIfaceProperties.count("Name"))
        {
            prettyName =
                std::get<std::string>(allCurrentIfaceProperties.at("Name"));
        }

        auto chassisPrettyName =
            std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(name,
                                                                 baseType);
        chassisPrettyName->invoke(pdiMethod(prettyName), prettyName);
        device->addStaticSensor(chassisPrettyName);
    }
    else if (type == "NSM_AssetTag")
    {
        auto assetTagIntf = NsmNVSwitchAndNicChassis<AssetTagIntf>(name,
                                                                   baseType);
        auto assetTag = std::make_shared<NsmInventoryProperty<AssetTagIntf>>(
            assetTagIntf, ASSET_TAG);
        device->addStaticSensor(assetTag);
    }
    else if (type == "NSM_ChassisVersion")
    {
        auto revisionObject = NsmNVSwitchAndNicChassis<RevisionIntf>(name,
                                                                     baseType);
        auto versionSensor =
            std::make_shared<NsmInventoryProperty<RevisionIntf>>(
                revisionObject, INFO_ROM_VERSION);
        device->addStaticSensor(versionSensor);
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
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.AssetTag",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.Location",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.LocationCode",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.ChassisVersion"};

REGISTER_NSM_CREATION_FUNCTION(createNsmNVSwitchChassis,
                               nvSwitchChassisInterfaces)
REGISTER_NSM_CREATION_FUNCTION(createNsmNVLinkMgmtNicChassis,
                               nvLinkMgmtNicChassisInterfaces)
} // namespace nsm
