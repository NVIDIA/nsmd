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
#include "config.h"

#include "counterProducer.hpp"

namespace nsm
{
class NsmDevice;

CountersTemplate class ProgressCountersBase
{
  private:
    DeviceCounterDumpObject<CounterDataType, Size, MemFdBytesSize> dumpObject;
    const size_t countThreshold;
    const size_t timeThreshold;

    uint32_t totalCount = 0;
    uint32_t dumpIteration = 0;
    uint64_t lastUpdateTime = 0;

  protected:
    ProgressCountersBase(const std::string& path,
                         const std::string& description,
                         const CountersHeaders& countersHeaders,
                         size_t countThreshold = 0, size_t timeThreshold = 0);

    virtual ~ProgressCountersBase() = default;

    /**
     * @brief Counters array
     * @note The array is zero-initialized
     */
    CountersArray<CounterDataType, Size> counters{};

    /**
     * @brief Update the counters if the count threshold is exceeded and write
     * the counters to the dump object
     * @return true if the counters were updated, false otherwise
     */
    bool flushIfNeeded();

    /**
     * @brief Initialize the counters
     * @note This method is called after the counters are created and before the
     * counters are incremented. This method is used to initialize the counters
     * to a default value.
     */
    virtual void initializeCounters() = 0;

    /**
     * @brief Update the counters and write the counters to the dump object
     * @param time Current time in microseconds, defaults to current time
     */
    void updateCounters(
        const uint64_t& time = utils::getCurrentSteadyClockTimestampUs());

    /**
     * @brief Increment the counter for the given counter type
     * @param counterType Counter type
     */
    void increment(const uint32_t& counterType);

  public:
    /**
     * @brief Update the counters if the time threshold is exceeded and write
     * the counters to the dump object
     * @param time Current time in microseconds
     * @return true if the counters were updated, false otherwise
     */
    bool flushIfNeeded(const uint64_t& time);
};

using PollingCountersBase =
    ProgressCountersBase<uint32_t, PollingCountersSize,
                         SENSOR_PROGRESS_COUNTERS_MEMFD_SIZE>;

enum class PollingType;
class ProgressCounters : public PollingCountersBase
{
  private:
    void initializeCounters() override final;

  public:
    ProgressCounters(eid_t eid);

    /**
     * @brief Increment the counter for the given polling type.
     * @param pollingType Polling type
     * @param rc nsm_sw_codes, nsm_completion_codes and nsm_reason_codes
     */
    void increment(const PollingType& pollingType, const uint8_t rc);

    /**
     * @brief Increment the counter for the given counter type and analyse the
     * return code for detecting error and timeout
     * @param counterType Counter type
     * @param rc nsm_sw_codes, nsm_completion_codes and nsm_reason_codes
     */
    void increment(const ProgressCounterType& counterType, const uint8_t rc);
};

using DiscoveryEventsBase =
    ProgressCountersBase<int8_t, DiscoveryEventsSize,
                         DISCOVERY_PROGRESS_COUNTERS_MEMFD_SIZE>;
class DiscoveryEvents : public DiscoveryEventsBase
{
  private:
    void initializeCounters() override final;

  public:
    DiscoveryEvents(eid_t eid);

    /**
     * @brief Increment the counter for the given counter type
     * @param counterType Counter type
     */
    void increment(const DiscoveryEventType& counterType);

    /**
     * @brief Set the value for the given counter type and write the value to
     * the dump object
     * @param counterType Counter type
     * @param value Value to set
     */
    void setValue(const DiscoveryEventType& counterType, const uint8_t value);
};

} // namespace nsm
