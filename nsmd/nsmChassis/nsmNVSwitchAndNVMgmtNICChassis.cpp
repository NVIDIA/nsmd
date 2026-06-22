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

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
#include "nsmDbusIfaceOverride/nsmResetIface.hpp"

#include <com/nvidia/Reset/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#endif

#include <array>
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
            auto& objServer = SensorManager::getInstance().getObjServer();
            if (nsmDeviceAssociationIntf)
            {
                objServer.remove_interface(nsmDeviceAssociationIntf);
                nsmDeviceAssociationIntf.reset();
            }

            nsmDeviceAssociationIntf = objServer.add_interface(
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
    std::string manufacturer = MANUFACTURER_NVIDIA;

    auto chassisAsset = NsmNVSwitchAndNicChassis<NsmAssetIntf>(name, baseType);
    chassisAsset.invoke(pdiMethod(manufacturer), manufacturer);

    auto partNumberSensor =
        std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            chassisAsset, DEVICE_PART_NUMBER);
    auto serialNumberSensor =
        std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(chassisAsset,
                                                             SERIAL_NUMBER);
    const auto modelProperty = getModelInventoryProperty(
        device->getDeviceType(), device->getDeviceRole());
    auto modelSensor = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        chassisAsset, modelProperty);
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

void createChassisSKU(std::shared_ptr<NsmDevice> device, std::string& name,
                      const std::string& baseType)
{
    auto chassisSKU =
        std::make_shared<NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>>(name,
                                                                   baseType);
    device->addStaticSensor(chassisSKU);
}

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
using NvidiaResetIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::Reset>;
using NvidiaResetTypes =
    sdbusplus::server::com::nvidia::Reset::NvidiaResetTypes;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;

/** @class NsmDeviceReset
 *
 *  One reset object under <chassis>/Reset/<Redfish-ResetType>. Owns its
 *  com.nvidia.Reset, Control.ResetAsync, and "reset_controls"/"chassis"
 *  association interfaces so the sensor manager manages their lifetimes — the
 *  same pattern as every other nsmd sensor object. bmcweb identifies the reset
 *  variant by this object's path leaf (the association endpoint's filename).
 */
class NsmDeviceReset : public NsmObject
{
  public:
    NsmDeviceReset(sdbusplus::bus_t& bus, const std::string& name,
                   const std::string& type, const std::string& resetObjPath,
                   std::shared_ptr<NsmDevice> device,
                   NvidiaResetTypes resetType, uint8_t resetTarget,
                   uint8_t trigger, const std::string& chassisPath) :
        NsmObject(name, type), objPath(resetObjPath)
    {
        lg2::info("NsmDeviceReset: create sensor:{NAME}", "NAME", name.c_str());

        // com.nvidia.Reset — advertises the generic NSM reset target. bmcweb
        // selects the object by its path leaf (via reset_controls), not by this
        // property, so a shared target across objects is fine.
        resetIntf = std::make_shared<NvidiaResetIntf>(bus, objPath.c_str());
        resetIntf->resetType(resetType);

        // Control.ResetAsync — issues NSM cmd 0x06 with these wire params.
        resetAsyncIntf = std::make_shared<NsmDeviceResetAsyncIntf>(
            bus, objPath.c_str(), device,
            NsmResetParams{resetTarget, trigger, /*portIndex=*/0});

        // Association: <chassis>/reset_controls → this object, so bmcweb
        // discovers it via getAssociationEndPoints(chassis +
        // "/reset_controls").
        assocIntf =
            std::make_shared<AssociationDefinitionsInft>(bus, objPath.c_str());
        assocIntf->associations({{"chassis", "reset_controls", chassisPath}});
    }

  private:
    std::shared_ptr<NvidiaResetIntf> resetIntf;
    std::shared_ptr<NsmDeviceResetAsyncIntf> resetAsyncIntf;
    std::shared_ptr<AssociationDefinitionsInft> assocIntf;
    std::string objPath;
};

/** @brief Create all device reset D-Bus objects under <chassisPath>/Reset/.
 *
 *  Called from createNsmChassis when the chassis EM config entry has
 *  "DeviceResetSupported": true. The EM config is the source of truth — no
 *  device-role gating. Each reset type gets its own NsmDeviceReset object; a
 *  "reset_controls"/"chassis" association is registered on each so bmcweb can
 *  discover them via getAssociationEndPoints(chassisPath + "/reset_controls").
 */
void createDeviceResetObjects(sdbusplus::bus_t& bus,
                              std::shared_ptr<NsmDevice> device,
                              const std::string& chassisPath,
                              const std::string& type)
{
    struct DeviceResetEntry
    {
        // Object-path leaf (…/Reset/<name>). bmcweb surfaces this verbatim as
        // the Redfish ResetType, so it is the reset variant's Redfish name.
        const char* name;
        // com.nvidia.Reset.ResetType — the generic NSM reset target this object
        // represents. Several Redfish variants can share one target (they
        // differ only by trigger), so this is not unique across entries.
        NvidiaResetTypes resetType;
        uint8_t reset_target;
        uint8_t trigger;
    };

    // NSM spec parameters per reset type (NVBug 6129734 comment #4). The leaf
    // name is the Redfish ResetType; resetType is the generic NSM target. A
    // single target (e.g. DeviceFullReset) backs multiple Redfish variants that
    // differ only by trigger, so each gets its own object at a distinct path.
    static constexpr std::array<DeviceResetEntry, 5> deviceResets{{
        {"FullReset", NvidiaResetTypes::DeviceFullReset,
         NSM_RESET_TARGET_DEVICE, NSM_RESET_TRIGGER_PCIE_LINK_DISABLE},
        {"ForceDpuReset", NvidiaResetTypes::DeviceFullReset,
         NSM_RESET_TARGET_DEVICE, NSM_RESET_TRIGGER_IMMEDIATE},
        {"DpuReset", NvidiaResetTypes::NetworkGracefulReset,
         NSM_RESET_TARGET_NETWORK, NSM_RESET_TRIGGER_HOST_PERST},
        {"ArmReset", NvidiaResetTypes::ComputeGracefulReset,
         NSM_RESET_TARGET_COMPUTE, NSM_RESET_TRIGGER_IMMEDIATE},
        {"ArmShutdown", NvidiaResetTypes::ComputeGracefulShutDown,
         NSM_RESET_TARGET_COMPUTE_SHUTDOWN, NSM_RESET_TRIGGER_IMMEDIATE},
    }};

    for (const auto& entry : deviceResets)
    {
        const std::string objPath = chassisPath + "/Reset/" + entry.name;
        device->addDeviceSensors(std::make_shared<NsmDeviceReset>(
            bus, entry.name, type, objPath, device, entry.resetType,
            entry.reset_target, entry.trigger, chassisPath));
    }
}
#endif

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

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
        // "DeviceResetSupported": true on the chassis EM config entry triggers
        // creation of the reset objects under <chassis>/Reset/<Type>. The reset
        // controls belong to the chassis object, so they are created here.
        if (allCurrentIfaceProperties.count("DeviceResetSupported") &&
            std::get<bool>(
                allCurrentIfaceProperties.at("DeviceResetSupported")))
        {
            createDeviceResetObjects(
                utils::DBusHandler::getBus(), device,
                std::string(chassisInventoryBasePath) + "/" + name, baseType);
        }
#endif
    }
    else if (type == "NSM_Chassis_Attributes")
    {
        createChassisAsset(device, name, baseType);
        createChassisSKU(device, name, baseType);
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
            (device->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX8 ||
             device->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX9 ||
             device->getDeviceRole() ==
                 NSM_PCIE_BRIDGE_DEV_ROLE_CX_BLUEFIELD_NIC))
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
