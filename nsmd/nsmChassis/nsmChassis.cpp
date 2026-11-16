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

#include "nsmChassis.hpp"

#include "../../common/nsmPropertySupport.hpp"
#include "../../common/utils.hpp"
#include "nsmApSkuId.hpp"
#include "nsmCommon.hpp"
#if defined(ENABLE_DEBUG_INFO)
#include "nsmDebugInfo.hpp"
#endif
#include "features/associations.hpp"
#include "nsmChassisFeatures.hpp"
#include "nsmDevice.hpp"
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#include "nsmGPIO/nsmGPIOStateCommon.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmProcessor/nsmOemResetStatistics.hpp"
#include "nsmProcessor/nsmSoCPowerSmoothing.hpp"
#include "nsmWriteProtectedJumper.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <unordered_map>
#include <unordered_set>

namespace nsm
{

void createAsset(std::shared_ptr<NsmDevice> device, const std::string& name,
                 const dbus::PropertyMap& allCurrentIfaceProperties,
                 const std::unordered_set<nsm_inventory_property_identifiers>&
                     unsupported = {})
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    std::string manufacturer = MANUFACTURER_NVIDIA;
    if (allCurrentIfaceProperties.count("Manufacturer"))
    {
        manufacturer =
            std::get<std::string>(allCurrentIfaceProperties.at("Manufacturer"));
    }

    chassisAsset->invoke(pdiMethod(manufacturer), manufacturer);

    markAssetPropertiesNotSupported(*chassisAsset, unsupported);

    const auto modelProperty = getModelInventoryProperty(
        device->getDeviceType(), device->getDeviceRole());
    const std::vector<nsm_inventory_property_identifiers> assetSensorProps = {
        FRU_PART_NUMBER, SERIAL_NUMBER, modelProperty};

    bool anySensorAdded = false;
    for (const auto& nsmProp : assetSensorProps)
    {
        if (unsupported.count(nsmProp))
        {
            continue;
        }
        device->addStaticSensor(
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(*chassisAsset,
                                                                 nsmProp));
        anySensorAdded = true;
    }

    if (!anySensorAdded)
    {
        device->addStaticSensor(chassisAsset);
    }
}

void createSKU(std::shared_ptr<NsmDevice> device, const std::string& name,
               bool notSupported)
{
    auto chassisSKU = std::make_shared<NsmChassis<NsmApSkuIdIntf>>(name);
    if (notSupported)
    {
        // Tombstone the placeholder SKU so bmcweb omits it from Redfish,
        // rather than leaving it empty on chassis that have no SKU
        // (e.g. a logical Zone). See nvbug 6339053.
        chassisSKU->invoke(pdiMethod(sku), std::string(propertyNotSupported));
    }
    device->addStaticSensor(chassisSKU);
}

void createSKU(std::shared_ptr<NsmDevice> device, const std::string& name)
{
    createSKU(device, name, false);
}

void createFPGAAsset(std::shared_ptr<NsmDevice> device, const std::string& name,
                     const dbus::PropertyMap& allCurrentIfaceProperties)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    std::string manufacturer = MANUFACTURER_NVIDIA;
    if (allCurrentIfaceProperties.count("Manufacturer"))
    {
        manufacturer =
            std::get<std::string>(allCurrentIfaceProperties.at("Manufacturer"));
    }

    chassisAsset->invoke(pdiMethod(manufacturer), manufacturer);
    device->addDeviceSensors(chassisAsset);
}

void createDimension(std::shared_ptr<NsmDevice> device, const std::string& name)
{
    auto chassisDimension = std::make_shared<NsmChassis<DimensionIntf>>(name);
    auto depth = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        *chassisDimension, PRODUCT_LENGTH);
    auto width = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        *chassisDimension, PRODUCT_WIDTH);
    auto height = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
        *chassisDimension, PRODUCT_HEIGHT);
    device->addStaticSensor(depth);
    device->addStaticSensor(width);
    device->addStaticSensor(height);
}

void createChassisType(std::shared_ptr<NsmDevice> device,
                       const std::string& name,
                       const dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string chassisType;
    if (allCurrentIfaceProperties.count("ChassisType"))
    {
        chassisType =
            std::get<std::string>(allCurrentIfaceProperties.at("ChassisType"));
    }

    auto chassis = std::make_shared<NsmChassis<ChassisIntf>>(name);
    chassis->invoke(pdiMethod(type),
                    ChassisIntf::convertChassisTypeFromString(chassisType));
    device->addStaticSensor(chassis);
}

void createHealth(std::shared_ptr<NsmDevice> device, const std::string& name)
{
    std::string health = HEALTH_TYPE_OK;
    auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
    chassisHealth->invoke(pdiMethod(health),
                          HealthIntf::convertHealthTypeFromString(health));
    device->addStaticSensor(chassisHealth);
}

void createLocation(std::shared_ptr<NsmDevice> device, const std::string& name,
                    const dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationType;
    if (allCurrentIfaceProperties.count("LocationType"))
    {
        locationType =
            std::get<std::string>(allCurrentIfaceProperties.at("LocationType"));
    }

    auto chassisLocation = std::make_shared<NsmChassis<LocationIntf>>(name);
    chassisLocation->invoke(
        pdiMethod(locationType),
        LocationIntf::convertLocationTypesFromString(locationType));
    device->addStaticSensor(chassisLocation);
}

void createLocationCode(std::shared_ptr<NsmDevice> device,
                        const std::string& name,
                        const dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationCode;
    if (allCurrentIfaceProperties.count("LocationCode"))
    {
        locationCode =
            std::get<std::string>(allCurrentIfaceProperties.at("LocationCode"));
    }

    auto chassisLocationCode =
        std::make_shared<NsmChassis<LocationCodeIntf>>(name);
    chassisLocationCode->invoke(pdiMethod(locationCode), locationCode);
    device->addStaticSensor(chassisLocationCode);
}

void createLocationContext(std::shared_ptr<NsmDevice> device,
                           const std::string& name,
                           const dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string locationContext;
    if (allCurrentIfaceProperties.count("LocationContext"))
    {
        locationContext = std::get<std::string>(
            allCurrentIfaceProperties.at("LocationContext"));
    }

    auto chassisLocationContext =
        std::make_shared<NsmChassis<LocationContextIntf>>(name);
    chassisLocationContext->invoke(pdiMethod(locationContext), locationContext);
    device->addStaticSensor(chassisLocationContext);
}

void createFieldReplaceable(std::shared_ptr<NsmDevice> device,
                            const std::string& name,
                            const dbus::PropertyMap& allCurrentIfaceProperties)
{
    bool fieldReplaceable;
    if (allCurrentIfaceProperties.count("FieldReplaceable"))
    {
        fieldReplaceable =
            std::get<bool>(allCurrentIfaceProperties.at("FieldReplaceable"));
    }

    auto chassisReplaceable =
        std::make_shared<NsmChassis<ReplaceableIntf>>(name);
    chassisReplaceable->invoke(pdiMethod(fieldReplaceable), fieldReplaceable);
    device->addStaticSensor(chassisReplaceable);
}

void createPowerLimit(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties)
{
    auto chassisPowerLimit = std::make_shared<NsmChassis<PowerLimitIntf>>(name);
    bool priority = false; // Default priority is false for sub types
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }

    device->addSensor(std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                          *chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT),
                      priority);
    device->addSensor(std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                          *chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT),
                      priority);
}

void createPrettyName(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string prettyName;
    if (allCurrentIfaceProperties.count("PrettyNameForChassis"))
    {
        prettyName = std::get<std::string>(
            allCurrentIfaceProperties.at("PrettyNameForChassis"));
    }

    auto chassisPrettyName = std::make_shared<NsmChassis<ItemIntf>>(name);
    chassisPrettyName->invoke(pdiMethod(prettyName), prettyName);
    device->addStaticSensor(chassisPrettyName);
}

void createWriteProtect(std::shared_ptr<NsmDevice> device,
                        const std::string& name,
                        const dbus::PropertyMap& allBaseIfaceProperties)
{
    NsmDeviceIdentification deviceType = NSM_DEV_ID_UNKNOWN;
    if (allBaseIfaceProperties.count("DeviceType"))
    {
        deviceType = (NsmDeviceIdentification)std::get<uint64_t>(
            allBaseIfaceProperties.at("DeviceType"));
    }

    if (deviceType != NSM_DEV_ID_BASEBOARD)
    {
        throw std::runtime_error(
            "Cannot use NSM_WriteProtect for different device than Baseboard");
    }
    auto settings = std::make_shared<NsmChassis<SettingsIntf>>(name);
    auto writeProtectJumper =
        std::make_shared<NsmWriteProtectedJumper>(*settings);
    device->addSensor(writeProtectJumper, false);
}

void createResetMetrics(std::shared_ptr<NsmDevice> device,
                        const std::string& name, sdbusplus::bus_t& bus)
{
    auto objectPath = chassisInventoryBasePath.string() + "/" + name +
                      "/ResetStatistics";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(bus, objectPath.c_str());

    //  Add associations between chassis and reset_statistics
    std::vector<utils::Association> associations{
        {"parent", "reset_statistics",
         chassisInventoryBasePath.string() + "/" + name}};
    auto resetMetricsAssociationIntf =
        std::make_unique<AssociationDefinitionsIntf>(bus, objectPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    for (const auto& association : associations)
    {
        associationsList.emplace_back(association.forward, association.backward,
                                      association.absolutePath);
    }
    resetMetricsAssociationIntf->associations(associationsList);

    // Initialize all properties to their default values
    resetCountersIntf->lastResetType(LastResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(std::nan(""));
    resetCountersIntf->pfflrResetExitCount(std::nan(""));
    resetCountersIntf->conventionalResetEntryCount(std::nan(""));
    resetCountersIntf->conventionalResetExitCount(std::nan(""));
    resetCountersIntf->fundamentalResetEntryCount(std::nan(""));
    resetCountersIntf->fundamentalResetExitCount(std::nan(""));
    resetCountersIntf->iRoTResetExitCount(std::nan(""));
    resetCountersIntf->bootReason(
        std::vector<BootReasonTypes>{BootReasonTypes::PowerOn});

    auto resetStatisticsSensor = std::make_shared<ResetStatisticsAggregator>(
        "ResetMetrics", "NSM_ResetStatistics", objectPath, resetCountersIntf,
        std::move(resetMetricsAssociationIntf));

    device->addSensor(resetStatisticsSensor, false);
}

void createErrorInjectionPayload(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const std::string& name, const dbus::PropertyMap& allBaseIfaceProperties)
{
    NsmDeviceIdentification deviceType = NSM_DEV_ID_UNKNOWN;
    if (allBaseIfaceProperties.count("DeviceType"))
    {
        deviceType = (NsmDeviceIdentification)std::get<uint64_t>(
            allBaseIfaceProperties.at("DeviceType"));
    }

    if (deviceType == NSM_DEV_ID_MCTP_BRIDGE)
    {
        createNsmMCUErrorInjectionSensors(
            manager, device, path(chassisInventoryBasePath / name));
    }
}

#if defined(ENABLE_DEBUG_INFO)
void createDeviceDiagnostics(std::shared_ptr<NsmDevice> device,
                             const std::string& name, const uuid_t& uuid,
                             sdbusplus::bus_t& bus)
{
    device->addStaticSensor(std::make_shared<NsmDebugInfoObject>(
        bus, name, chassisInventoryBasePath.string() + "/",
        "NSM_DeviceDiagnostics", uuid, DebugDumpType::Diagnostics));
}
#endif

void createChassisAttributes(std::shared_ptr<NsmDevice> device,
                             SensorManager& manager, sdbusplus::bus_t& bus,
                             const std::string& name,
                             [[maybe_unused]] const uuid_t& uuid,
                             const dbus::PropertyMap& allCurrentIfaceProperties,
                             const dbus::PropertyMap& allBaseIfaceProperties)
{
    // Handle Asset (always NVIDIA manufacturer if AssetInformationAvailable)
    if (allCurrentIfaceProperties.count("AssetInformationAvailable") &&
        std::get<bool>(
            allCurrentIfaceProperties.at("AssetInformationAvailable")))
    {
        auto unsupported = getUnsupportedAssetProperties(
            device->getDeviceType(), device->getDeviceRole());
        createAsset(device, name, allCurrentIfaceProperties, unsupported);
    }
    // Baseboard-class chassis (a logical Zone or ProcessorModule) have no SKU
    // of their own; tombstone it so bmcweb omits it rather than the default
    // "NA". See nvbug 6339053.
    createSKU(device, name, isSkuUnsupported(device->getDeviceType()));
    // Handle Location and LocationCode from ChassisAttributes
    if (allCurrentIfaceProperties.count("LocationType"))
    {
        createLocation(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("LocationCode"))
    {
        createLocationCode(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("LocationContext"))
    {
        createLocationContext(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("FieldReplaceable"))
    {
        createFieldReplaceable(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("ChassisType"))
    {
        createChassisType(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("PrettyNameForChassis"))
    {
        createPrettyName(device, name, allCurrentIfaceProperties);
    }

    createHealth(device, name);

    // Optional features based on support flags
    if (allCurrentIfaceProperties.count("DimensionSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("DimensionSupported")))
    {
        createDimension(device, name);
    }
    if (allCurrentIfaceProperties.count("PowerLimitSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("PowerLimitSupported")))
    {
        createPowerLimit(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("WriteProtectSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("WriteProtectSupported")))
    {
        createWriteProtect(device, name, allBaseIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("ResetMetricsSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("ResetMetricsSupported")))
    {
        createResetMetrics(device, name, bus);
    }
    if (allCurrentIfaceProperties.count("GPIOStateSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("GPIOStateSupported")))
    {
        createNsmGPIOStateSensors(device, name, "NSM_GPIOState",
                                  chassisInventoryBasePath.string() + "/" +
                                      name);
    }
    if (allCurrentIfaceProperties.count("ErrorInjectionSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("ErrorInjectionSupported")))
    {
        createErrorInjectionPayload(manager, device, name,
                                    allBaseIfaceProperties);
    }
#if defined(ENABLE_DEBUG_INFO)
    if (allCurrentIfaceProperties.count("DeviceDiagnosticsSupported") &&
        std::get<bool>(
            allCurrentIfaceProperties.at("DeviceDiagnosticsSupported")))
    {
        createDeviceDiagnostics(device, name, uuid, bus);
    }
#endif
    if (allCurrentIfaceProperties.count("SoCPowerSmoothingSupported") &&
        std::get<bool>(
            allCurrentIfaceProperties.at("SoCPowerSmoothingSupported")))
    {
        if (device->getDeviceType() == NSM_DEV_ID_MCTP_BRIDGE &&
            (device->getDeviceRole() == NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA ||
             device->getDeviceRole() == NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA))
        {
            std::string type = "NSM_Chassis";
            std::string inventoryObjPath = chassisInventoryBasePath.string() +
                                           "/" + name;
            createSoCPowerSmoothing(device, bus, name, type, inventoryObjPath,
                                    SoCDeviceType::MCU);
        }
    }
}

void createFPGAAttributes(std::shared_ptr<NsmDevice> device,
                          const std::string& name,
                          const dbus::PropertyMap& allCurrentIfaceProperties)
{
    createFPGAAsset(device, name, allCurrentIfaceProperties);
    createSKU(device, name);

    // Handle Location and ChassisType from FPGAAttributes
    if (allCurrentIfaceProperties.count("LocationType"))
    {
        createLocation(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("ChassisType"))
    {
        createChassisType(device, name, allCurrentIfaceProperties);
    }

    createHealth(device, name);
}

void createOperationalStatus(std::shared_ptr<NsmDevice> device,
                             const std::string& name,
                             const dbus::PropertyMap& allCurrentIfaceProperties,
                             const dbus::PropertyMap& allBaseIfaceProperties)
{
    uint64_t deviceType = 0;
    if (allBaseIfaceProperties.count("DeviceType"))
    {
        deviceType =
            std::get<uint64_t>(allBaseIfaceProperties.at("DeviceType"));
    }
    if (deviceType != NSM_DEV_ID_BASEBOARD)
    {
        throw std::runtime_error(
            "Cannot use NSM_OperationalStatus for different device than Baseboard");
    }

    uint64_t instanceNumber = 0;
    if (allBaseIfaceProperties.count("InstanceNumber"))
    {
        instanceNumber =
            std::get<uint64_t>(allBaseIfaceProperties.at("InstanceNumber"));
    }

    dbus::Interfaces inventoryObjPaths;
    if (allCurrentIfaceProperties.count("InventoryObjPaths"))
    {
        inventoryObjPaths = std::get<dbus::Interfaces>(
            allCurrentIfaceProperties.at("InventoryObjPaths"));
    }

    bool priority = false; // Default priority is false for sub types
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }

    auto gpuOperationalStatus = NsmInterfaceProvider<OperationalStatusIntf>(
        name, "NSM_OperationalStatus", inventoryObjPaths);
    device->addSensor(std::make_shared<NsmGpuPresenceAndPowerStatus>(
                          gpuOperationalStatus, instanceNumber),
                      priority);
}

void createPowerState(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties,
                      const dbus::PropertyMap& allBaseIfaceProperties)
{
    uint64_t deviceType = 0;
    if (allBaseIfaceProperties.count("DeviceType"))
    {
        deviceType =
            std::get<uint64_t>(allBaseIfaceProperties.at("DeviceType"));
    }

    if (deviceType != NSM_DEV_ID_BASEBOARD)
    {
        throw std::runtime_error(
            "Cannot use NSM_PowerState for different device than Baseboard");
    }

    uint64_t instanceNumber = 0;
    if (allBaseIfaceProperties.count("InstanceNumber"))
    {
        instanceNumber =
            std::get<uint64_t>(allBaseIfaceProperties.at("InstanceNumber"));
    }

    dbus::Interfaces inventoryObjPaths;
    if (allCurrentIfaceProperties.count("InventoryObjPaths"))
    {
        inventoryObjPaths = std::get<dbus::Interfaces>(
            allCurrentIfaceProperties.at("InventoryObjPaths"));
    }

    bool priority = false; // Default priority is false for sub types
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }

    auto gpuPowerState = NsmInterfaceProvider<PowerStateIntf>(
        name, "NSM_PowerState", inventoryObjPaths);
    device->addSensor(
        std::make_shared<NsmPowerSupplyStatus>(gpuPowerState, instanceNumber),
        priority);
}

template <typename IntfType>
requester::Coroutine
    NsmChassis<IntfType>::update(std::shared_ptr<NsmDevice> nsmDevice)
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
            co_return NSM_SUCCESS;
        }
        co_return NSM_ERROR;
    }
    // For other interfaces, we don't need to update anything.
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createChassisAttributesFeature(SensorManager& manager,
                                   const DeviceContext& dev,
                                   const dbus::PropertyMap& params)
{
    auto attributes = params;
    const auto name = std::get<std::string>(attributes.at("Name"));
    attributes.erase("Name");
    // EM publishes "" for a LocationCode variable the topology does not set
    if (auto code = param<std::string>(attributes, "LocationCode");
        code && code->empty())
    {
        attributes.erase("LocationCode");
    }
    const dbus::PropertyMap base{
        {"DeviceType", static_cast<uint64_t>(dev.deviceType)}};
    createChassisAttributes(dev.device, manager, utils::DBusHandler::getBus(),
                            name, dev.staticUuid, attributes, base);
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createChassisFeature([[maybe_unused]] SensorManager& manager,
                         const DeviceContext& dev,
                         const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("Chassis feature of {NAME}: no NsmDevice", "NAME", dev.name);
        co_return NSM_ERROR;
    }
    const auto name = std::get<std::string>(params.at("Name"));

    // update() reads the device UUID over NSM
    auto chassisUuid = std::make_shared<NsmChassis<UuidIntf>>(name);
    chassisUuid->invoke(pdiMethod(uuid), dev.device->deviceUuid);
    dev.device->addStaticSensor(chassisUuid);

    auto mctpUuid = std::make_shared<NsmChassis<MctpUuidIntf>>(name);
    mctpUuid->invoke(pdiMethod(uuid), dev.device->getUuid());
    dev.device->addStaticSensor(mctpUuid);

#ifdef NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT
    // Legacy creates it for a DeviceType 3 (baseboard) record
    if (param<bool>(params, "PCIeRefClockEnabled").value_or(false))
    {
        dev.device->addStaticSensor(
            std::make_shared<NsmChassis<PCIeRefClockIntf>>(name));
    }
#endif
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createChassisAssociationsFeature([[maybe_unused]] SensorManager& manager,
                                     const DeviceContext& dev,
                                     const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("Chassis associations of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }
    const auto name = std::get<std::string>(params.at("Name"));
    std::vector<utils::Association> associations;
    // The parent's category differs per platform (board/ on VR, chassis/ on
    // HGXR)
    addAssociations(associations, params, "ParentChassisAssociationPaths",
                    "parent_chassis", "all_chassis");
    // The bridge chassis gets bridging_chassis back to this chassis
    addAssociation(associations, params, "BridgingChassisAssociationPath",
                   "bridged_chassis", "bridging_chassis");
    if (associations.empty())
    {
        co_return NSM_SUCCESS;
    }
    auto associationsObject =
        std::make_shared<NsmChassis<AssociationDefinitionsInft>>(name);
    associationsObject->invoke(pdiMethod(associations),
                               utils::getAssociations(associations));
    dev.device->addStaticSensor(associationsObject);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
