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

#include "featureDriver.hpp"

#include "common/yield.hpp"
#include "paramTemplates.hpp"

#include <phosphor-logging/lg2.hpp>

#ifdef NVIDIA_SHMEM
#include "nsmCommon/sharedMemCommon.hpp"
#endif

#include <systemd/sd-event.h>

#include <algorithm>
#include <utility>

namespace nsm::features
{

YieldBudget::YieldBudget(const common::Event& event, uint64_t budgetUsec) :
    event(event), budget(budgetUsec)
{
    restart();
}

uint64_t YieldBudget::iteration() const
{
    uint64_t value = 0;
    sd_event_get_iteration(static_cast<const sdeventplus::Event&>(event).get(),
                           &value);
    return value;
}

void YieldBudget::restart()
{
    sliceStart = std::chrono::steady_clock::now();
    sliceIteration = iteration();
}

bool YieldBudget::exceeded()
{
    if (iteration() != sliceIteration)
    {
        restart();
    }
    auto slice = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - sliceStart)
            .count());
    longestSlice = std::max(longestSlice, slice);
    return slice >= static_cast<uint64_t>(budget.count());
}

uint64_t YieldBudget::takeLongestSliceUsec()
{
    return std::exchange(longestSlice, 0);
}

requester::Coroutine YieldBudget::yield()
{
    // Braces: with -Dco_await= (coverage builds) "common::Yield(event);"
    // would declare a variable named event.
    co_await common::Yield{event};
    ++yieldCount;
    restart();
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine YieldBudget::yieldIfExceeded()
{
    if (exceeded())
    {
        co_await yield();
    }
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine createDeviceFeatures(SensorManager& manager,
                                          DeviceContext dev,
                                          FeatureList features,
                                          YieldBudget& budget)
{
    const auto start = std::chrono::steady_clock::now();
    const uint64_t yieldsBefore = budget.yields();
    budget.takeLongestSliceUsec();
    size_t created = 0;
    size_t failed = 0;

    for (const auto& feature : features)
    {
        if (!feature.enabled)
        {
            continue;
        }
        if (!feature.create)
        {
            lg2::error("Feature {FEATURE} of {NAME} has no creator", "FEATURE",
                       feature.id, "NAME", dev.name);
            ++failed;
            continue;
        }

        std::string error;
        std::vector<std::string> dropped;
        auto params = resolveParams(dev, feature.params, error, dropped);
        for (const auto& reason : dropped)
        {
            lg2::error("Feature {FEATURE} of {NAME}: {ERROR}", "FEATURE",
                       feature.id, "NAME", dev.name, "ERROR", reason);
        }
        if (!params)
        {
            lg2::error("Feature {FEATURE} of {NAME}: {ERROR}", "FEATURE",
                       feature.id, "NAME", dev.name, "ERROR", error);
            ++failed;
            continue;
        }

        // The coroutine promise swallows creator exceptions (rc stays 0), so
        // check exception() as well as rc.
        auto creation = feature.create(manager, dev, *params);
        uint8_t rc = co_await creation;
        if (creation.exception())
        {
            lg2::error("Feature {FEATURE} of {NAME} threw an exception",
                       "FEATURE", feature.id, "NAME", dev.name);
            ++failed;
        }
        else if (rc != NSM_SW_SUCCESS)
        {
            lg2::error("Feature {FEATURE} of {NAME} failed, rc={RC}", "FEATURE",
                       feature.id, "NAME", dev.name, "RC", rc);
            ++failed;
        }
        else
        {
            ++created;
        }

#ifdef NVIDIA_SHMEM
        // Same threshold as the EM-driven path: skips the many tiny features.
        constexpr size_t drainThreshold = 100;
        if (nsm_shmem_utils::SharedMemoryManager::pendingTelemetryCount() >
            drainThreshold)
        {
            nsm_shmem_utils::SharedMemoryManager::
                updateAggregateTelemetryOnTAL();
        }
#endif

        co_await budget.yieldIfExceeded();
    }

    co_await budget.yield();

    lg2::info(
        "Device {NAME}: {CREATED} features created, {FAILED} failed, {MS} ms, {YIELDS} yields, longest slice {SLICE} us",
        "NAME", dev.name, "CREATED", created, "FAILED", failed, "MS",
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count(),
        "YIELDS", budget.yields() - yieldsBefore, "SLICE",
        budget.takeLongestSliceUsec());
    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace nsm::features
