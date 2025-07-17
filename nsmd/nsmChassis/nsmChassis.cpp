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
#include "deviceManager.hpp"
#include "nsmCommon.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmDevice.hpp"
#include "nsmErrorInjectionCommon.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmProcessor/nsmOemResetStatistics.hpp"
#include "nsmWriteProtectedJumper.hpp"

#include <unordered_map>

namespace nsm
{

template <typename IntfType>
requester::Coroutine NsmChassis<IntfType>::update(SensorManager& manager,
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

    auto device = manager.getNsmDevice(uuid);

    if (type == "NSM_Chassis")
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
    }
    else if (type == "NSM_DeviceDiagnostics")
    {
        device->addStaticSensor(std::make_shared<NsmDebugInfoObject>(
            utils::DBusHandler::getBus(), name,
            chassisInventoryBasePath.string() + "/", type, uuid,
            DebugDumpType::Diagnostics));
    }
    else if (type == "NSM_FPGA_Asset")
    {
        auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
        std::string manufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            manufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

        chassisAsset->invoke(pdiMethod(manufacturer), manufacturer);
        device->deviceSensors.emplace_back(chassisAsset);
    }
    else if (type == "NSM_Asset")
    {
        auto chassisAsset = NsmChassis<NsmAssetIntf>(name);
        std::string manufacturer{};
        if (allCurrentIfaceProperties.count("Manufacturer"))
        {
            manufacturer = std::get<std::string>(
                allCurrentIfaceProperties.at("Manufacturer"));
        }

        chassisAsset.invoke(pdiMethod(manufacturer), manufacturer);
        // create sensor
        auto partNumber = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            chassisAsset, FRU_PART_NUMBER);
        auto serialNumber =
            std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(chassisAsset,
                                                                 SERIAL_NUMBER);
        auto model = std::make_shared<NsmInventoryProperty<NsmAssetIntf>>(
            chassisAsset, MARKETING_NAME);
        device->addStaticSensor(partNumber);
        device->addStaticSensor(serialNumber);
        device->addStaticSensor(model);
    }
    else if (type == "NSM_ChassisType")
    {
        std::string chassisType;
        if (allCurrentIfaceProperties.count("ChassisType"))
        {
            chassisType = std::get<std::string>(
                allCurrentIfaceProperties.at("ChassisType"));
        }

        auto chassis = std::make_shared<NsmChassis<ChassisIntf>>(name);
        chassis->invoke(pdiMethod(type),
                        ChassisIntf::convertChassisTypeFromString(chassisType));
        device->addStaticSensor(chassis);
    }
    else if (type == "NSM_Dimension")
    {
        auto chassisDimension = NsmChassis<DimensionIntf>(name);
        auto depth = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
            chassisDimension, PRODUCT_LENGTH);
        auto width = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
            chassisDimension, PRODUCT_WIDTH);
        auto height = std::make_shared<NsmInventoryProperty<DimensionIntf>>(
            chassisDimension, PRODUCT_HEIGHT);
        device->addStaticSensor(depth);
        device->addStaticSensor(width);
        device->addStaticSensor(height);
    }
    else if (type == "NSM_Health")
    {
        std::string health;
        if (allCurrentIfaceProperties.count("Health"))
        {
            health =
                std::get<std::string>(allCurrentIfaceProperties.at("Health"));
        }
        auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
        chassisHealth->invoke(pdiMethod(health),
                              HealthIntf::convertHealthTypeFromString(health));
        device->addStaticSensor(chassisHealth);
    }
    else if (type == "NSM_Location")
    {
        std::string locationType;
        if (allCurrentIfaceProperties.count("LocationType"))
        {
            locationType = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationType"));
        }

        auto chassisLocation = std::make_shared<NsmChassis<LocationIntf>>(name);
        chassisLocation->invoke(
            pdiMethod(locationType),
            LocationIntf::convertLocationTypesFromString(locationType));
        device->addStaticSensor(chassisLocation);
    }
    else if (type == "NSM_LocationCode")
    {
        std::string locationCode;
        if (allCurrentIfaceProperties.count("LocationCode"))
        {
            locationCode = std::get<std::string>(
                allCurrentIfaceProperties.at("LocationCode"));
        }

        auto chassisLocationCode =
            std::make_shared<NsmChassis<LocationCodeIntf>>(name);
        chassisLocationCode->invoke(pdiMethod(locationCode), locationCode);
        device->addStaticSensor(chassisLocationCode);
    }
    else if (type == "NSM_PowerLimit")
    {
        auto chassisPowerLimit = NsmChassis<PowerLimitIntf>(name);
        bool priority;
        if (allCurrentIfaceProperties.count("Priority"))
        {
            priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
        }

        device->addSensor(
            std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                chassisPowerLimit, MINIMUM_DEVICE_POWER_LIMIT),
            priority);
        device->addSensor(
            std::make_shared<NsmInventoryProperty<PowerLimitIntf>>(
                chassisPowerLimit, MAXIMUM_DEVICE_POWER_LIMIT),
            priority);
    }
    else if (type == "NSM_OperationalStatus")
    {
        uint64_t deviceType;
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

        uint64_t instanceNumber;
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

        bool priority;
        if (allCurrentIfaceProperties.count("Priority"))
        {
            priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
        }

        auto gpuOperationalStatus = NsmInterfaceProvider<OperationalStatusIntf>(
            name, type, inventoryObjPaths);
        device->addSensor(std::make_shared<NsmGpuPresenceAndPowerStatus>(
                              gpuOperationalStatus, instanceNumber),
                          priority);
    }
    else if (type == "NSM_PowerState")
    {
        uint64_t deviceType;
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

        uint64_t instanceNumber;
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

        bool priority;
        if (allCurrentIfaceProperties.count("Priority"))
        {
            priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
        }

        auto gpuPowerState =
            NsmInterfaceProvider<PowerStateIntf>(name, type, inventoryObjPaths);
        device->addSensor(std::make_shared<NsmPowerSupplyStatus>(
                              gpuPowerState, instanceNumber),
                          priority);
    }
    else if (type == "NSM_WriteProtect")
    {
        NsmDeviceIdentification deviceType;
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
        auto settings = NsmChassis<SettingsIntf>(name);
        auto writeProtectJumper =
            std::make_shared<NsmWriteProtectedJumper>(settings);
        device->addSensor(writeProtectJumper, false);
    }
    else if (type == "NSM_PrettyName")
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
    else if (type == "NSM_ResetMetrics")
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
            std::make_unique<AssociationDefinitionsIntf>(bus,
                                                         objectPath.c_str());
        std::vector<std::tuple<std::string, std::string, std::string>>
            associationsList;
        for (const auto& association : associations)
        {
            associationsList.emplace_back(association.forward,
                                          association.backward,
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

        auto resetStatisticsSensor =
            std::make_shared<ResetStatisticsAggregator>(
                "ResetMetrics", "NSM_ResetStatistics", objectPath,
                resetCountersIntf, std::move(resetMetricsAssociationIntf));

        device->deviceSensors.emplace_back(resetStatisticsSensor);
        device->addSensor(resetStatisticsSensor, false);
    }
    else if (type == "NSM_ChassisErrorInjectionPayload" &&
             device->getDeviceType() == NSM_DEV_ID_MCTP_BRIDGE)
    {
        createNsmMCUErrorInjectionSensors(
            manager, device, path(chassisInventoryBasePath / name));
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

std::vector<std::string> chassisInterfaces{
    "xyz.openbmc_project.Configuration.NSM_Chassis.Chassis",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Asset",
    "xyz.openbmc_project.Configuration.NSM_Chassis.ChassisType",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Dimension",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Health",
    "xyz.openbmc_project.Configuration.NSM_Chassis.Location",
    "xyz.openbmc_project.Configuration.NSM_Chassis.LocationCode",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerLimit",
    "xyz.openbmc_project.Configuration.NSM_Chassis.OperationalStatus",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PowerState",
    "xyz.openbmc_project.Configuration.NSM_Chassis.PrettyName",
    "xyz.openbmc_project.Configuration.NSM_Chassis.WriteProtect",
    "xyz.openbmc_project.Configuration.NSM_Chassis.ResetMetrics",
    "xyz.openbmc_project.Configuration.NSM_Chassis.ErrorInjectionPayload",
    "xyz.openbmc_project.Configuration.NSM_Chassis.DeviceDiagnostics"};

REGISTER_NSM_CREATION_FUNCTION(nsmChassisCreateSensors, chassisInterfaces)

} // namespace nsm
