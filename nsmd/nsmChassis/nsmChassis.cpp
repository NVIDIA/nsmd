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

#include "../../common/utils.hpp"
#include "nsmCommon.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmDevice.hpp"
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmProcessor/nsmOemResetStatistics.hpp"
#include "nsmWriteProtectedJumper.hpp"
#include "requester/mctp_endpoint_discovery.hpp"

#include <unordered_map>

namespace nsm
{

static void createAsset(std::shared_ptr<NsmDevice> device,
                        const std::string& name,
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
    // create sensor
    auto partNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        *chassisAsset, FRU_PART_NUMBER);
    auto serialNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        *chassisAsset, SERIAL_NUMBER);
    auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
        *chassisAsset, MARKETING_NAME);
    device->addStaticSensor(partNumber);
    device->addStaticSensor(serialNumber);
    device->addStaticSensor(model);
}

static void createFPGAAsset(std::shared_ptr<NsmDevice> device,
                            const std::string& name,
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
    device->getDeviceSensors().emplace_back(chassisAsset);
}

static void createDimension(std::shared_ptr<NsmDevice> device,
                            const std::string& name)
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

static void
    createChassisType(std::shared_ptr<NsmDevice> device,
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

static void createHealth(std::shared_ptr<NsmDevice> device,
                         const std::string& name)
{
    std::string health = HEALTH_TYPE_OK;
    auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
    chassisHealth->invoke(pdiMethod(health),
                          HealthIntf::convertHealthTypeFromString(health));
    device->addStaticSensor(chassisHealth);
}

static void createLocation(std::shared_ptr<NsmDevice> device,
                           const std::string& name,
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

static void
    createLocationCode(std::shared_ptr<NsmDevice> device,
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

static void createPowerLimit(std::shared_ptr<NsmDevice> device,
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

static void createPrettyName(std::shared_ptr<NsmDevice> device,
                             const std::string& name,
                             const dbus::PropertyMap& allCurrentIfaceProperties)
{
    std::string prettyName;
    if (allCurrentIfaceProperties.count("Name"))
    {
        prettyName =
            std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }

    auto chassisPrettyName = std::make_shared<NsmChassis<ItemIntf>>(name);
    chassisPrettyName->invoke(pdiMethod(prettyName), prettyName);
    device->addStaticSensor(chassisPrettyName);
}

static void createWriteProtect(std::shared_ptr<NsmDevice> device,
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

static void createResetMetrics(std::shared_ptr<NsmDevice> device,
                               const std::string& name,
                               sdbusplus::bus::bus& bus)
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

static void createErrorInjectionPayload(
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

static void createDeviceDiagnostics(std::shared_ptr<NsmDevice> device,
                                    const std::string& name, const uuid_t& uuid,
                                    sdbusplus::bus::bus& bus)
{
    device->addStaticSensor(std::make_shared<NsmDebugInfoObject>(
        bus, name, chassisInventoryBasePath.string() + "/",
        "NSM_DeviceDiagnostics", uuid, DebugDumpType::Diagnostics));
}

static void
    createChassisAttributes(std::shared_ptr<NsmDevice> device,
                            SensorManager& manager, sdbusplus::bus::bus& bus,
                            const std::string& name, const uuid_t& uuid,
                            const dbus::PropertyMap& allCurrentIfaceProperties,
                            const dbus::PropertyMap& allBaseIfaceProperties)
{
    // Handle Asset (always NVIDIA manufacturer if AssetInformationAvailable)
    if (allCurrentIfaceProperties.count("AssetInformationAvailable") &&
        std::get<bool>(
            allCurrentIfaceProperties.at("AssetInformationAvailable")))
    {
        createAsset(device, name, allCurrentIfaceProperties);
    }

    // Handle Location and LocationCode from ChassisAttributes
    if (allCurrentIfaceProperties.count("LocationType"))
    {
        createLocation(device, name, allCurrentIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("LocationCode"))
    {
        createLocationCode(device, name, allCurrentIfaceProperties);
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
    if (allCurrentIfaceProperties.count("ErrorInjectionSupported") &&
        std::get<bool>(allCurrentIfaceProperties.at("ErrorInjectionSupported")))
    {
        createErrorInjectionPayload(manager, device, name,
                                    allBaseIfaceProperties);
    }
    if (allCurrentIfaceProperties.count("DeviceDiagnosticsSupported") &&
        std::get<bool>(
            allCurrentIfaceProperties.at("DeviceDiagnosticsSupported")))
    {
        createDeviceDiagnostics(device, name, uuid, bus);
    }
}

static void
    createFPGAAttributes(std::shared_ptr<NsmDevice> device,
                         const std::string& name,
                         const dbus::PropertyMap& allCurrentIfaceProperties)
{
    createFPGAAsset(device, name, allCurrentIfaceProperties);

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

static void
    createOperationalStatus(std::shared_ptr<NsmDevice> device,
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

static void createPowerState(std::shared_ptr<NsmDevice> device,
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

requester::Coroutine
    nsmChassisCreateSensors(SensorManager& manager,
                            [[maybe_unused]] const std::string& interface,
                            const std::string& objPath)
{
    std::string baseInterface = "xyz.openbmc_project.Configuration.NSM_Chassis";
    auto& bus = utils::DBusHandler::getBus();
    std::string name{};
    std::string type{};
    uuid_t uuid{};

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

    if (type == "NSM_Chassis_Attributes")
    {
        createChassisAttributes(device, manager, bus, name, uuid,
                                allCurrentIfaceProperties,
                                allBaseIfaceProperties);
    }
    else if (type == "NSM_FPGA_Attributes")
    {
        createFPGAAttributes(device, name, allCurrentIfaceProperties);
    }
    else if (type == "NSM_OperationalStatus")
    {
        createOperationalStatus(device, name, allCurrentIfaceProperties,
                                allBaseIfaceProperties);
    }
    else if (type == "NSM_PowerState")
    {
        createPowerState(device, name, allCurrentIfaceProperties,
                         allBaseIfaceProperties);
    }
    else if (type == "NSM_Chassis")
    {
        auto chassisUuid = std::make_shared<NsmChassis<UuidIntf>>(name);
        uuid_t deviceUuid;
        if (allCurrentIfaceProperties.count("DEVICE_UUID"))
        {
            deviceUuid =
                std::get<uuid_t>(allCurrentIfaceProperties.at("DEVICE_UUID"));
        }

        chassisUuid->invoke(pdiMethod(uuid), deviceUuid);
        device->addStaticSensor(chassisUuid);
        auto mctpUuid = std::make_shared<NsmChassis<MctpUuidIntf>>(name);
        mctpUuid->invoke(pdiMethod(uuid), uuid);
        device->addStaticSensor(mctpUuid);

        std::vector<utils::Association> associations{};
        co_await utils::coGetAssociations(
            objPath, baseInterface + ".Associations", associations);
        if (!associations.empty())
        {
            auto associationsObject =
                std::make_shared<NsmChassis<AssociationDefinitionsInft>>(name);
            associationsObject->invoke(pdiMethod(associations),
                                       utils::getAssociations(associations));
            device->addStaticSensor(associationsObject);
        }

        // NOTE:
        // - gb200nvl does NOT support PCIeReferenceClockCount.
        //
        // Behavior of NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT macro:
        //   - If 'nvidia-fpga-pcie-reference-clock-count' is enabled:
        //       #define NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT true
        //   - If 'nvidia-fpga-pcie-reference-clock-count' is disabled:
        //       #define NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT false
        //
        // Please check this macro before using any logic
        // related to PCIe reference clock count to avoid unsupported cases on
        // gb200nvl.
        //
        // The following code block logs the macro status and controls
        // whether to create the PCIeRefClock sensor.

#ifdef NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT
        lg2::info("PCIeReferenceClockCount is supported. "
                  "NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT is enabled.");
        uint64_t deviceType{};
        if (allBaseIfaceProperties.count("DeviceType"))
        {
            deviceType =
                std::get<uint64_t>(allBaseIfaceProperties.at("DeviceType"));
        }
        if (deviceType == NSM_DEV_ID_BASEBOARD)
        {
            auto pCIeRefClock =
                std::make_shared<NsmChassis<PCIeRefClockIntf>>(name);
            device->addStaticSensor(pCIeRefClock);
        }
#else
        lg2::info("PCIeReferenceClockCount is not supported. "
                  "NVIDIA_FPGA_PCIE_REFERENCE_CLOCK_COUNT is disabled.");
#endif

        co_return NSM_SUCCESS;
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> chassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_Chassis.ChassisAttributes",
    "xyz.openbmc_project.Configuration.NSM_Chassis.OperationalStatus",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerState"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisCreateSensors, chassisInterfaces)

} // namespace nsm
