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

/** @brief Features of a CPU, in creation order */
inline FeatureList cpuFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

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
                     std::vector<std::string>{"component/{device}"}},
                    {"RoTInventoryAssociationPaths",
                     std::vector<std::string>{"component/{device}"}},
                    {"InventoryChassisAssociationPaths",
                     std::vector<std::string>{"component/{device}"}}}});
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
        {.id = "IRoTChassisType",
         .create = createIRoTChassisTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ChassisType",
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"s}}});
    features.push_back(
        {.id = "IRoTLocationType",
         .create = createIRoTLocationTypeFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"LocationType",
              "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"s}}});
    features.push_back(
        {.id = "IRoTLocationCode",
         .create = createIRoTLocationCodeFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"LocationCode", "IRoT_CPU_0"s}}});
    features.push_back(
        {.id = "IRoTLocationContext",
         .create = createIRoTLocationContextFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"LocationContext",
              "{platformPrefix}Chassis_0/ProcessorModule_{instance}"s}}});
    features.push_back(
        {.id = "IRoTFieldReplaceable",
         .create = createIRoTFieldReplaceableFeature,
         .params = {{"Name", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"FieldReplaceable", false}}});

    // IRoT chassis with its slots; one array per slot field, element i is
    // slot i + 1
    features.push_back(
        {.id = "RoT",
         .create = createRoTFeature,
         .params = {
             {"Name", "{platformPrefix}IRoT_{deviceBase}"s},
             {"ImageCopyEnabled", true},
             {"SlotChassisName",
              std::vector<std::string>{
                  "CPU_FMC_{instance}", "CPU_FMC_{instance}",
                  "CPU_FWS_{instance}", "CPU_FWS_{instance}"}},
             {"SlotFirmwareType",
              std::vector<std::string>{"AP", "AP", "AP", "AP"}},
             {"SlotType", std::vector<std::string>{"Active", "InActive",
                                                   "Active", "InActive"}},
             {"SlotComponentClassification",
              std::vector<uint64_t>{10, 10, 10, 10}},
             {"SlotComponentIdentifier", std::vector<uint64_t>{2, 2, 513, 513}},
             {"SlotComponentIndex", std::vector<uint64_t>{0, 0, 0, 0}},
             {"SlotSoftwareName",
              std::vector<std::string>{"{platformPrefix}SBIOS_FMC_{instance}",
                                       "{platformPrefix}SBIOS_FMC_{instance}",
                                       "{platformPrefix}SBIOS_FW_{instance}",
                                       "{platformPrefix}SBIOS_FW_{instance}"}},
             {"SlotIsRoT", std::vector<uint64_t>{1, 1, 0, 0}},
             {"SlotReportSkuWithNsm", std::vector<uint64_t>{1, 0, 0, 0}},
             {"SlotUpdateSkuEnabled", std::vector<uint64_t>{1, 0, 0, 0}},
             {"SlotSkuInventoryPath",
              std::vector<std::string>{"component/{device}", "", "", ""}}}});

    features.push_back({.id = "EventSetting",
                        .create = createEventSettingFeature,
                        .params = {{"Name", "GlobalEventSetting"s},
                                   {"EventGenerationSetting", u64(2)}}});

    features.push_back({.id = "PreBootDiagEventConfig",
                        .create = createEventConfigFeature,
                        .params = {{"Name", "PreBootDiagEventSetting"s},
                                   {"MessageType", u64(4)},
                                   {"SubscribedEventIDs",
                                    std::vector<uint64_t>{0, 1, 2, 3}}}});

#ifdef ENABLE_DEBUG_TOKEN
    features.push_back(
        {.id = "DebugToken",
         .create = createDebugTokenFeature,
         .params = {{"ChassisName", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"DebugTokenType", std::vector<std::string>{"Unified"}},
                    {"DebugTokenDeviceType", "CPU"s}}});
#endif

#ifdef ENABLE_DOT
    // DOT, on chassis/<platformPrefix>IRoT_<deviceBase>; BlobPathName is the
    // DOTBlob PathName
    features.push_back(
        {.id = "DOT",
         .create = createDotFeature,
         .params = {{"ChassisName", "{platformPrefix}IRoT_{deviceBase}"s},
                    {"BlobPathName", "{deviceBase}"s}}});
    features.push_back({.id = "DOTBlob",
                        .create = createDotBlobFeature,
                        .params = {{"PathName", "{deviceBase}"s}}});
#endif

    return features;
}

} // namespace nsm::features
