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

#include <string>
#include <vector>

namespace nsm::features
{

/** @brief Features of a VR BMC ERoT, in creation order */
inline FeatureList bmcErotFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

    // RoT chassis with its slots; one array per slot field, element i is
    // slot i + 1
    features.push_back(
        {.id = "RoT",
         .create = createRoTFeature,
         .params = {
             {"Name", "{deviceBase}"s},
             {"ImageCopyEnabled", true},
             {"ImageCopyPolicyEnabled", true},
             {"SlotChassisName",
              std::vector<std::string>{"{deviceBase}", "{deviceBase}",
                                       "{parentChassisName}",
                                       "{parentChassisName}"}},
             {"SlotFirmwareType",
              std::vector<std::string>{"EC", "EC", "AP", "AP"}},
             {"SlotType", std::vector<std::string>{"Active", "InActive",
                                                   "Active", "InActive"}},
             {"SlotComponentClassification",
              std::vector<uint64_t>{10, 10, 10, 10}},
             {"SlotComponentIdentifier",
              std::vector<uint64_t>{65280, 65280, 16, 16}},
             {"SlotComponentIndex", std::vector<uint64_t>{0, 0, 0, 0}},
             {"SlotSoftwareNameFromChassis", true},
             {"PlatformPrefix", ""s}}});

    // Firmware component types: no nsmd consumer found, placeholders
    features.push_back({.id = "FWComponentTypeAP",
                        .create = createFWComponentTypeFeature,
                        .params = {{"Name", "AP"s}, {"ComponentType", "AP"s}}});
    features.push_back({.id = "FWComponentTypeEC",
                        .create = createFWComponentTypeFeature,
                        .params = {{"Name", "EC"s}, {"ComponentType", "EC"s}}});

    // Interfaces published by EM on the entity today, all on
    // chassis/{deviceBase}
    // UUID object; its update adds SPDMResponder once the device UUID is read
    features.push_back({.id = "IRoTSPDMResponder",
                        .create = createIRoTSPDMResponderFeature,
                        .params = {{"Name", "{deviceBase}"s}}});
    features.push_back(
        {.id = "IRoTAssociations",
         .create = createIRoTAssociationsFeature,
         .params = {{"Name", "{deviceBase}"s},
                    {"ParentChassisAssociationPaths",
                     std::vector<std::string>{"{parentChassis}"}},
                    {"RoTInventoryAssociationPaths",
                     std::vector<std::string>{"{parentChassis}"}},
                    {"InventoryChassisAssociationPaths",
                     std::vector<std::string>{"chassis/{deviceBase}"}}}});
    features.push_back({.id = "IRoTAsset",
                        .create = createIRoTAssetFeature,
                        .params = {{"Name", "{deviceBase}"s}}});
    features.push_back({.id = "IRoTHealth",
                        .create = createIRoTHealthFeature,
                        .params = {{"Name", "{deviceBase}"s}}});
    // Decorator.SKU written by pldm (EC SKU)
    features.push_back({.id = "IRoTSKU",
                        .create = createIRoTSKUFeature,
                        .params = {{"Name", "{deviceBase}"s}}});
    features.push_back(
        {.id = "IRoTLocationType",
         .create = createIRoTLocationTypeFeature,
         .params = {
             {"Name", "{deviceBase}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});
    features.push_back(
        {.id = "IRoTLocationCode",
         .create = createIRoTLocationCodeFeature,
         .params = {{"Name", "{deviceBase}"s},
                    {"LocationCode", optVar<std::string>("{LocationCode}")}}});
    features.push_back(
        {.id = "IRoTLocationContext",
         .create = createIRoTLocationContextFeature,
         .params = {
             {"Name", "{deviceBase}"s},
             {"LocationContext", optVar<std::string>("{LocationContext}")}}});
    features.push_back(
        {.id = "IRoTFieldReplaceable",
         .create = createIRoTFieldReplaceableFeature,
         .params = {{"Name", "{deviceBase}"s}, {"FieldReplaceable", false}}});
    features.push_back(
        {.id = "IRoTChassisType",
         .create = createIRoTChassisTypeFeature,
         .params = {
             {"Name", "{deviceBase}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s}}});

#ifdef ENABLE_DEBUG_TOKEN
    features.push_back(
        {.id = "DebugToken",
         .create = createDebugTokenFeature,
         .params = {{"ChassisName", "{deviceBase}"s},
                    {"DebugTokenType", std::vector<std::string>{"Unified"}},
                    {"DebugTokenDeviceType", "ERoT"s}}});
#endif

    return features;
}

} // namespace nsm::features
