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

#include <tal.hpp>

#include <chrono>
namespace nsm_shmem_utils
{
void updateSharedMemoryOnSuccess(
    [[maybe_unused]] const std::string& inventoryObjPath,
    [[maybe_unused]] const std::string& ifaceName,
    [[maybe_unused]] const std::string& propName,
    [[maybe_unused]] std::vector<uint8_t>& smbusData,
    [[maybe_unused]] nv::sensor_aggregation::DbusVariantType propValue)
{
#ifdef NVIDIA_SHMEM
    auto timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
    tal::TelemetryAggregator::updateTelemetry(inventoryObjPath, ifaceName,
                                              propName, smbusData, timestamp, 0,
                                              propValue);
#endif
}
} // namespace nsm_shmem_utils
