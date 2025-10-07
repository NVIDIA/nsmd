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

#pragma once

#include "progressCounterType.hpp"

#include <memory>

namespace nsm
{
class NsmDevice;
class DeviceCounterDumpObject;
enum class PollingType;

class ProgressCounters
{
  public:
    /**
     * @brief Constructor for ProgressCounters
     * @param dev Reference to the NsmDevice
     */
    ProgressCounters(NsmDevice& dev);

    /**
     * @brief Increment the counter for the given polling type.
     * @param pollingType Polling type
     * @param rc nsm_sw_codes, nsm_completion_codes and nsm_reason_codes
     * @param time Current time in microseconds
     */
    void increment(const PollingType& pollingType, const uint8_t rc,
                   const uint64_t& time);

    /**
     * @brief Increment the counter for the given counter type and analyse the
     * return code for detecting error and timeout
     * @param counterType Counter type
     * @param rc nsm_sw_codes, nsm_completion_codes and nsm_reason_codes
     * @param time Current time in microseconds
     */
    void increment(const ProgressCounterType& counterType, const uint8_t rc,
                   const uint64_t& time);

  private:
    /**
     * @brief Increment the counter for the given counter type
     * @param counterType Counter type
     * @param time Current time in microseconds
     */
    void increment(const ProgressCounterType& counterType,
                   const uint64_t& time);

    void updateCounters();
    void updateCountersIfNeeded();

    NsmDevice& dev;

    CountersArray counters{};
    std::shared_ptr<DeviceCounterDumpObject> deviceCounterDumpObject;
    uint32_t dumpIteration = 0;
    uint32_t totalCount = 0;
    uint64_t startTime = 0;
    uint64_t lastUpdateTime = 0;
};

} // namespace nsm
