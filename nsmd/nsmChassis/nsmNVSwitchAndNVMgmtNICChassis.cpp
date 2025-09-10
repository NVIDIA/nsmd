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
#include "nsmCommon.hpp"
#include "nsmDevice.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine NsmNVSwitchAndNicChassis<IntfType>::update(
    std::shared_ptr<NsmDevice> nsmDevice)
{
    if constexpr (std::is_same_v<IntfType, UuidIntf>)
    {
        // For UuidIntf, we need to get the device UUID from the device manager.
        mctp::MctpDiscovery& mctpDiscovery = mctp::MctpDiscovery::getInstance();
        uuid_t deviceUuid;
        auto rc = co_await getDeviceUUID(nsmDevice, mctpDiscovery, deviceUuid);
        if (rc == NSM_SW_SUCCESS && !deviceUuid.empty())
        {
            this->invoke(pdiMethod(uuid), deviceUuid);
            nsmDeviceAssociationIntf =
                SensorManager::getInstance().getObjServer().add_unique_interface(
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

void createChassisAsset(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType)
{
    std::string manufacturer = CHASSIS_ASSET_MANUFACTURER_NVIDIA;

    auto chassisAsset = NsmNVSwitchAndNicChassis<NsmAssetIntf>(name, baseType);
    chassisAsset.invoke(pdiMethod(manufacturer), manufacturer);

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

void createChassisType(std::shared_ptr<NsmDevice> device, std::string& name,
                       const std::string& baseType,
                       dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string chassisType =
        std::get<std::string>(allCurrentIfaceProperties.at("ChassisType"));

    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(name, baseType);
    chassis->invoke(pdiMethod(type),
                    ChassisIntf::convertChassisTypeFromString(chassisType));
    device->addStaticSensor(chassis);
}

void createChassisHealth(std::shared_ptr<NsmDevice> device, std::string& name,
                         const std::string& baseType)
{
    std::string health = HEALTH_TYPE_OK;

    auto chassisHealth =
        std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(name, baseType);
    chassisHealth->invoke(pdiMethod(health),
                          HealthIntf::convertHealthTypeFromString(health));
    device->addStaticSensor(chassisHealth);
}

void createLocationType(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType,
                        dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationType{};
    if (allCurrentIfaceProperties.count("LocationType"))
    {
        locationType =
            std::get<std::string>(allCurrentIfaceProperties.at("LocationType"));
    }

    auto chassisLocation =
        std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(name,
                                                                 baseType);
    chassisLocation->invoke(
        pdiMethod(locationType),
        LocationIntf::convertLocationTypesFromString(locationType));
    device->addStaticSensor(chassisLocation);
}

void createLocationCode(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType,
                        dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationCode{};
    locationCode =
        std::get<std::string>(allCurrentIfaceProperties.at("LocationCode"));

    auto chassisLocationCode =
        std::make_shared<NsmNVSwitchAndNicChassis<LocationCodeIntf>>(name,
                                                                     baseType);
    chassisLocationCode->invoke(pdiMethod(locationCode), locationCode);
    device->addStaticSensor(chassisLocationCode);
}

void createPrettyName(std::shared_ptr<NsmDevice> device, std::string& name,
                      const std::string& baseType,
                      dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string prettyName{};
    prettyName = std::get<std::string>(
        allCurrentIfaceProperties.at("PrettyNameForChassis"));

    auto chassisPrettyName =
        std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(name, baseType);
    chassisPrettyName->invoke(pdiMethod(prettyName), prettyName);
    device->addStaticSensor(chassisPrettyName);
}

void createAssetTag(std::shared_ptr<NsmDevice> device, std::string& name,
                    const std::string& baseType)
{
    auto assetTagIntf = NsmNVSwitchAndNicChassis<AssetTagIntf>(name, baseType);
    auto assetTag = std::make_shared<NsmInventoryProperty<AssetTagIntf>>(
        assetTagIntf, ASSET_TAG);
    device->addStaticSensor(assetTag);
}

void createChassisVersion(std::shared_ptr<NsmDevice> device, std::string& name,
                          const std::string& baseType)
{
    auto revisionObject = NsmNVSwitchAndNicChassis<RevisionIntf>(name,
                                                                 baseType);
    auto versionSensor = std::make_shared<NsmInventoryProperty<RevisionIntf>>(
        revisionObject, INFO_ROM_VERSION);
    device->addStaticSensor(versionSensor);
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

    auto device = manager.getNsmDeviceFromStaticUUID(uuid);

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
    else if (type == "NSM_NVSwitch_Chassis_Attributes")
    {
        createChassisAsset(device, name, baseType);
        createChassisHealth(device, name, baseType);
        if (allCurrentIfaceProperties.count("ChassisType"))
        {
            createChassisType(device, name, baseType,
                              allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            createLocationType(device, name, baseType,
                               allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("LocationCode"))
        {
            createLocationCode(device, name, baseType,
                               allCurrentIfaceProperties);
        }
        if (allCurrentIfaceProperties.count("PrettyNameForChassis"))
        {
            createPrettyName(device, name, baseType, allCurrentIfaceProperties);
        }
        if (device->getDeviceType() == NSM_DEV_ID_PCIE_BRIDGE &&
            device->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX8)
        {
            createAssetTag(device, name, baseType);
            createChassisVersion(device, name, baseType);
        }
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
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.ChassisAttributes"};

std::vector<std::string> nvLinkMgmtNicChassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis",
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.ChassisAttributes"};

REGISTER_NSM_CREATION_FUNCTION(createNsmNVSwitchChassis,
                               nvSwitchChassisInterfaces)
REGISTER_NSM_CREATION_FUNCTION(createNsmNVLinkMgmtNicChassis,
                               nvLinkMgmtNicChassisInterfaces)
} // namespace nsm
