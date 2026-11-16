/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#pragma once

#include "common/globals.hpp"
#include "features/creators.hpp"
#include "features/feature.hpp"

#include <string>
#include <vector>

namespace nsm::features
{

/** @brief Features of an HMC HPM SMA (MCTP bridge), in creation order */
inline FeatureList hmcHpmSmaFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

    // Zone chassis (<platformPrefix>Chassis_0), served by one SMA only: EM
    // sets ZoneChassisName on that SMA alone
    features.push_back(
        {.id = "ZoneChassis",
         .create = createChassisFeature,
         .params = {{"Name", var<std::string>("{ZoneChassisName}")},
                    {"PCIeRefClockEnabled", true}}});
    features.push_back(
        {.id = "ZoneChassisAttributes",
         .create = createChassisAttributesFeature,
         .params = {
             {"Name", var<std::string>("{ZoneChassisName}")},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Zone"s},
             {"AssetInformationAvailable", false}}});
    // Total GPU power: the sum of the GPU Power_0 readings, on the
    // same SMA as the zone chassis; the GPUs add the total_power links
    features.push_back(
        {.id = "TotalGpuPower",
         .create = createNumericCompositeSensorFeature,
         .params = {{"Name", totalGpuPowerSensorPath.filename().string()},
                    {"SensorType", "power"s},
                    {"ChassisName", var<std::string>("{ZoneChassisName}")},
                    {"PhysicalContext", "GPU"s},
                    {"Implementation", "Synthesized"s}}});

    // Chassis of the processor module the SMA sits on
    features.push_back({.id = "ProcessorModuleChassis",
                        .create = createChassisFeature,
                        .params = {{"Name", "{parentChassisName}"s},
                                   {"PCIeRefClockEnabled", true}}});

    // Chassis attributes of the processor module the SMA sits on;
    // SKU and Health are always created
    features.push_back(
        {.id = "ProcessorModuleChassisAttributes",
         .create = createChassisAttributesFeature,
         .params = {
             {"Name", "{parentChassisName}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"s},
             {"AssetInformationAvailable", false},
             {"WriteProtectSupported", false}}});

    features.push_back({.id = "EventSetting",
                        .create = createEventSettingFeature,
                        .params = {{"Name", "GlobalEventSetting"s},
                                   {"EventGenerationSetting", u64(2)}}});

    features.push_back(
        {.id = "EventConfigGPIOState",
         .create = createEventConfigFeature,
         .params = {{"Name", "GPIOStateEventSetting"s},
                    {"MessageType", u64(0)},
                    {"SubscribedEventIDs", std::vector<uint64_t>{3}}}});

    // Chassis of the SMA itself: Common.UUID and MCTP.UUID
    features.push_back({.id = "Chassis",
                        .create = createChassisFeature,
                        .params = {{"Name", "{device}"s}}});
    // ContainedBy
    features.push_back(
        {.id = "ChassisAssociations",
         .create = createChassisAssociationsFeature,
         .params = {{"Name", "{device}"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"{parentChassis}"}}}});

    // Chassis attributes of the SMA itself; SKU and Health are always created
    features.push_back(
        {.id = "ChassisAttributes",
         .create = createChassisAttributesFeature,
         .params = {
             {"Name", "{device}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s},
             {"AssetInformationAvailable", true},
             {"LocationCode", optVar<std::string>("{LocationCode}")},
             {"LocationContext", optVar<std::string>("{LocationContext}")},
             {"FieldReplaceable", false},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s},
             {"ResetMetricsSupported", true},
             {"GPIOStateSupported", true},
             {"ErrorInjectionSupported", true},
             {"DeviceDiagnosticsSupported", false}}});

    features.push_back(
        {.id = "Temp_0",
         .create = createTempFeature,
         .params = {{"Name", "{device}_Temp_0"s},
                    {"SensorId", u64(17)},
                    {"PhysicalContext", "GPUSubsystem"s},
                    {"Description", "Internal SMA temperature values"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});

    features.push_back(
        {.id = "External_SMA_0_Temp_0",
         .create = createTempFeature,
         .params = {{"Name", "{parentChassisName}_External_SMA_0_Temp_0"s},
                    {"SensorId", u64(16)},
                    {"PhysicalContext", "GPUSubsystem"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"{ProcessorModuleName}"}},
                    {"PrimaryTemperatureAssociationEnabled", true},
                    {"ThresholdsEnabled", true},
                    {"UpperCautionParameterId", u64(16)},
                    {"UpperCautionPeriodicUpdate", false}}});
    features.push_back(
        {.id = "GPU_VR_Temp_0",
         .create = createTempFeature,
         .params = {{"Name", "{parentChassisName}_GPU_VR_Temp_0"s},
                    {"SensorId", u64(18)},
                    {"PhysicalContext", "Board"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"{ProcessorModuleName}"}},
                    {"ThresholdsEnabled", true},
                    {"UpperFatal", 135.0},
                    {"UpperCritical", 130.0},
                    {"UpperCaution", 120.0}}});
    // Present on SMA 1 only; NSM_Policy disables it on the other SMAs
    features.push_back(
        {.id = "BusBarTemp_0",
         .create = createTempFeature,
         .params = {{"Name", "{parentChassisName}_BusBar_Temp_0"s},
                    {"SensorId", u64(19)},
                    {"PhysicalContext", "DCBus"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"{ProcessorModuleName}"}},
                    {"ThresholdsEnabled", true},
                    {"UpperFatal", 110.0},
                    {"UpperCritical", 107.0},
                    {"UpperCaution", 100.0}}});
    features.push_back(
        {.id = "CPU_0_Die_0_Temp_0",
         .create = createTempFeature,
         .params = {{"Name", "{parentChassisName}_CPU_0_Die_0_Temp_0"s},
                    {"SensorId", u64(216)},
                    {"PhysicalContext", "CPU"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"{ProcessorModuleName}"}},
                    {"ThresholdsEnabled", true},
                    {"UpperCautionParameterId", u64(216)},
                    {"UpperCautionPeriodicUpdate", false}}});
    features.push_back(
        {.id = "CPU_0_SocTemp_0",
         .create = createTempFeature,
         .params = {{"Name", "{parentChassisName}_CPU_0_SocTemp_0"s},
                    {"SensorId", u64(217)},
                    {"PhysicalContext", "CPU"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"{ProcessorModuleName}"}},
                    {"ThresholdsEnabled", true},
                    {"UpperCautionParameterId", u64(217)},
                    {"UpperCautionPeriodicUpdate", false}}});

    // IRoT chassis with its slots; one array per slot field, element i is
    // slot i + 1
    features.push_back(
        {.id = "RoT",
         .create = createRoTFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ImageCopyEnabled", true},
             {"SlotChassisName",
              std::vector<std::string>{"{deviceBase}", "{deviceBase}",
                                       "CPLD_{instance}"}},
             {"SlotFirmwareType", std::vector<std::string>{"AP", "AP", "AP"}},
             {"SlotType",
              std::vector<std::string>{"Active", "InActive", "Active"}},
             {"SlotComponentClassification", std::vector<uint64_t>{10, 10, 10}},
             {"SlotComponentIdentifier",
              std::vector<uint64_t>{65282, 65282, 65283}},
             {"SlotComponentIndex", std::vector<uint64_t>{0, 0, 0}},
             {"SlotSoftwareName",
              std::vector<std::string>{"{platformPrefix}FW_{deviceBase}",
                                       "{platformPrefix}FW_{deviceBase}",
                                       "{platformPrefix}FW_CPLD_{instance}"}},
             {"SlotIsRoT", std::vector<uint64_t>{1, 1, 0}},
             {"SlotReportSkuWithNsm", std::vector<uint64_t>{0, 0, 1}},
             {"SlotSkuInventoryPath",
              std::vector<std::string>{
                  "", "", "component/{platformPrefix}CPLD_{instance}"}}}});

    // IRoT responder objects, all on chassis/<platformPrefix>IRoT_<deviceBase>
    // UUID object; its update adds SPDMResponder once the device UUID is read
    features.push_back(
        {.id = "IRoTSPDMResponder",
         .create = createIRoTSPDMResponderFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s}}});
    features.push_back(
        {.id = "IRoTAssociations",
         .create = createIRoTAssociationsFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}},
                    {"RoTInventoryAssociationPaths",
                     std::vector<std::string>{
                         "chassis/{device}",
                         "component/{platformPrefix}CPLD_{instance}"}},
                    {"InventoryChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});
    features.push_back(
        {.id = "IRoTAsset",
         .create = createIRoTAssetFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"AssetName", "{platformPrefix}IRoT_{deviceBase} IRoT"s}}});
    features.push_back(
        {.id = "IRoTHealth",
         .create = createIRoTHealthFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s}}});
    features.push_back(
        {.id = "IRoTLocationType",
         .create = createIRoTLocationTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});
    features.push_back(
        {.id = "IRoTChassisType",
         .create = createIRoTChassisTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s}}});
    features.push_back(
        {.id = "IRoTLocationCode",
         .create = createIRoTLocationCodeFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"LocationCode", "IRoT_0"s}}});
    features.push_back(
        {.id = "IRoTLocationContext",
         .create = createIRoTLocationContextFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"LocationContext", "{LocationContext}/HPM_SMA_0"s}}});
    features.push_back(
        {.id = "IRoTFieldReplaceable",
         .create = createIRoTFieldReplaceableFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"FieldReplaceable", false}}});

#ifdef ENABLE_DEBUG_TOKEN
    features.push_back(
        {.id = "DebugToken",
         .create = createDebugTokenFeature,
         .params = {{"ChassisName", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"DebugTokenType", std::vector<std::string>{"Unified"}},
                    {"DebugTokenDeviceType", "SMA"s}}});
#endif

    return features;
}

} // namespace nsm::features
