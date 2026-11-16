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

#include "features/creators.hpp"
#include "features/feature.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nsm::features
{

/** @brief Features of a ConnectX network adapter, in creation order */
inline FeatureList cxFeatures()
{
    return {};
}

/** @brief Features of a ConnectX SMA (MCTP bridge), in creation order */
inline FeatureList cxSmaFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

    // Chassis: Common.UUID and MCTP.UUID
    features.push_back({.id = "Chassis",
                        .create = createChassisFeature,
                        .params = {{"Name", "{device}"s}}});
    // Chassis attributes; SKU and Health are always created
    features.push_back(
        {.id = "ChassisAttributes",
         .create = createChassisAttributesFeature,
         .params = {
             {"Name", "{device}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s},
             {"LocationCode", optVar<std::string>("{LocationCode}")},
             {"LocationContext", optVar<std::string>("{LocationContext}")},
             {"FieldReplaceable", false},
             {"AssetInformationAvailable", true},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s},
             {"ResetMetricsSupported", true},
             {"ErrorInjectionSupported", true},
             {"DeviceDiagnosticsSupported", true}}});

    // ContainedBy: parent_chassis / all_chassis to the IO board the SMA sits
    // on (board/IO_Board_<n>), from the NSM_Telemetry ParentChassis variable
    features.push_back(
        {.id = "ChassisAssociations",
         .create = createChassisAssociationsFeature,
         .params = {{"Name", "{device}"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"{parentChassis}"}}}});

    features.push_back(
        {.id = "Temp_0",
         .create = createTempFeature,
         .params = {{"Name", "{device}_Temp_0"s},
                    {"SensorId", u64(17)},
                    {"PhysicalContext", "Board"s},
                    {"Description", "Internal SMA temperature values"s},
                    {"Aggregated", true},
                    {"Priority", true},
                    {"MaxValue", 125.0},
                    {"MinValue", -40.0},
                    {"ChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});

    // Leak detection; polled through the SMA, published under
    // board/<LeakDetectorChassis>
    features.push_back(
        {.id = "LeakDetection",
         .create = createLeakDetectionFeature,
         .params = {
             {"Name", "{LeakDetectorChassis}_LeakDetector"s},
             {"BoardName", var<std::string>("{LeakDetectorChassis}")},
             {"SensorIdMap", std::vector<uint64_t>{2}},
             {"SensorNameMap",
              std::vector<std::string>{"{LeakDetectorChassis}_LeakDetector_0"}},
             {"MinValue", 0.0},
             {"MaxValue", 3.3}}});

    // IRoT chassis with its slots; one array per slot field, element i is
    // slot i + 1
    features.push_back(
        {.id = "RoT",
         .create = createRoTFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ImageCopyEnabled", true},
             {"SlotChassisName",
              std::vector<std::string>{"{device}", "{device}"}},
             {"SlotFirmwareType", std::vector<std::string>{"AP", "AP"}},
             {"SlotType", std::vector<std::string>{"Active", "InActive"}},
             {"SlotComponentClassification", std::vector<uint64_t>{10, 10}},
             {"SlotComponentIdentifier", std::vector<uint64_t>{65282, 65282}},
             {"SlotComponentIndex", std::vector<uint64_t>{0, 0}},
             {"SlotSoftwareName",
              std::vector<std::string>{"{platformPrefix}FW_{deviceBase}",
                                       "{platformPrefix}FW_{deviceBase}"}}}});

    // IRoT objects on chassis/<platformPrefix>IRoT_<deviceBase>; the entity
    // decorators are owned by nsmd
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
                     std::vector<std::string>{"chassis/{device}"}},
                    {"InventoryChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{device}"}}}});
    features.push_back(
        {.id = "IRoTAsset",
         .create = createIRoTAssetFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"AssetName", "{device} IRoT"s}}});
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
                    {"LocationContext", "{LocationContext}/SMA_0"s}}});
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

    // Firmware component type: no nsmd consumer found, placeholder
    features.push_back({.id = "FWComponentType",
                        .create = createFWComponentTypeFeature,
                        .params = {{"Name", "AP"s}, {"ComponentType", "AP"s}}});

    return features;
}

} // namespace nsm::features
