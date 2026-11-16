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

/** @brief Features of a BMC HPM SMA (MCTP bridge), in creation order.
 *
 * The processor-module top chassis (Chassis_0) is owned by entity-manager;
 * this SMA only polls the chassis leak detector and publishes it under that
 * chassis. One detector per SMA instance: ProcessorModule_SMA_0 ->
 * Chassis_0_LeakDetector_0, ProcessorModule_SMA_1 -> Chassis_0_LeakDetector_1.
 */
inline FeatureList bmcHpmSmaFeatures()
{
    using namespace std::string_literals;

    FeatureList features;

    // Leak detection; polled through the SMA, published under
    // chassis/<LeakDetectorChassis> (Chassis_0). LeakDetectorChassis is a
    // strict device variable set in the SMA's NSM_Telemetry.
    features.push_back(
        {.id = "LeakDetection",
         .create = createLeakDetectionFeature,
         .params = {{"Name", "{LeakDetectorChassis}_LeakDetector_{instance}"s},
                    {"ChassisName", var<std::string>("{LeakDetectorChassis}")},
                    {"SensorIdMap", std::vector<uint64_t>{0}},
                    {"SensorNameMap",
                     std::vector<std::string>{
                         "{LeakDetectorChassis}_LeakDetector_{instance}"}},
                    {"MinValue", 0.0},
                    {"MaxValue", 3.3}}});

    return features;
}

} // namespace nsm::features
