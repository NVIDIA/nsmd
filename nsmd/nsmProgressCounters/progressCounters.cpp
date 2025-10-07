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

#include "progressCounters.hpp"

#include "counterProducer.hpp"
#include "nsmDevice.hpp"

namespace nsm
{

ProgressCounters::ProgressCounters(NsmDevice& dev) : dev(dev) {}

void ProgressCounters::increment(const PollingType& pollingType,
                                 const uint8_t rc, const uint64_t& time)
{
    increment(static_cast<ProgressCounterType>(pollingType), rc, time);
}

void ProgressCounters::increment(const ProgressCounterType& counterType,
                                 const uint8_t rc, const uint64_t& time)
{
    if (rc == NSM_SUCCESS)
    {
        increment(counterType, time);
    }
    else if (rc == NSM_SW_ERROR_TIMEOUT)
    {
        increment(ProgressCounterType::Timeout, time);
    }
    else
    {
        increment(ProgressCounterType::Error, time);
    }
}

void ProgressCounters::increment(const ProgressCounterType& counterType,
                                 const uint64_t& time)
{
#ifdef NVIDIA_PROGRESS_COUNTER
    if (totalCount == 0)
        startTime = time;

    counters[static_cast<uint32_t>(counterType)]++;
    ++totalCount;
    lastUpdateTime = time;

    updateCountersIfNeeded();
#else
    (void)counterType;
    (void)time;
#endif
}

void ProgressCounters::updateCounters()
{
    if (!deviceCounterDumpObject)
    {
        deviceCounterDumpObject =
            std::make_shared<DeviceCounterDumpObject>(dev.getEid());
    }
    deviceCounterDumpObject->updateCounters(dumpIteration++, lastUpdateTime,
                                            counters);
    totalCount = 0;
    startTime = 0;
    lastUpdateTime = 0;
    counters.fill(0);
}

void ProgressCounters::updateCountersIfNeeded()
{
    bool countExceeded =
        (totalCount > SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD);
    bool timeExceeded = ((lastUpdateTime - startTime) >=
                         SENSOR_PROGRESS_COUNTERS_DUMP_TIME_THRESHOLD);

    if (countExceeded || timeExceeded)
    {
        updateCounters();
    }
}

} // namespace nsm
