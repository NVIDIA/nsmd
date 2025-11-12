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

#include "config.h"

#include "sharedMemCommon.hpp"
#ifdef NVIDIA_SHMEM
#include "utils.hpp"

#include <tal.hpp>
namespace nsm_shmem_utils
{

// Static member definitions (required for static class members)
std::vector<tal::TelemetryData> SharedMemoryManager::telemetryData;

void SharedMemoryManager::cacheTALData(
    const std::string& inventoryObjPath, const std::string& ifaceName,
    const std::string& propName, std::vector<uint8_t>& smbusData,
    nv::sensor_aggregation::DbusVariantType propValue,
    const std::string& associatedEntityPath, uint8_t rc)
{
    telemetryData.push_back(
        tal::TelemetryData{inventoryObjPath, ifaceName, propName, smbusData, rc,
                           propValue, associatedEntityPath, 0});
}

void SharedMemoryManager::updateAggregateTelemetryOnTAL()
{
    auto timestamp = utils::getCurrentSteadyClockTimestamp();
    for (auto& data : telemetryData)
    {
        data.timestamp = timestamp;
    }

    tal::TelemetryAggregator::updateAggregateTelemetry(telemetryData);

    telemetryData.clear();
}

void SharedMemoryManager::initTALNamespace()
{
    if (tal::TelemetryAggregator::namespaceInit(tal::ProcessType::Producer,
                                                "nsmd"))
    {
        lg2::info("Initialized tal from nsmd.");
    }
    telemetryData.reserve(20);
}

} // namespace nsm_shmem_utils
#endif
