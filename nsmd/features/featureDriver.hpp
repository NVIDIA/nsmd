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

#include "common/event.hpp"
#include "features/feature.hpp"

#include <chrono>
#include <cstdint>

#ifndef NSM_FEATURE_YIELD_BUDGET_US
#define NSM_FEATURE_YIELD_BUDGET_US 5000
#endif

namespace nsm::features
{

/** @brief Limits how long feature creation runs without returning to the
 *  event loop.
 *
 *  Uses the real clock: sd_event_now() is frozen during a dispatch. A slice
 *  restarts whenever the loop has iterated since the last check, so time
 *  spent suspended in a D-Bus or NSM await is not counted.
 */
class YieldBudget
{
  public:
    explicit YieldBudget(const common::Event& event,
                         uint64_t budgetUsec = NSM_FEATURE_YIELD_BUDGET_US);

    bool exceeded();

    /** @brief Yield to the event loop and start a new slice */
    requester::Coroutine yield();

    requester::Coroutine yieldIfExceeded();

    uint64_t yields() const
    {
        return yieldCount;
    }

    /** @brief Longest uninterrupted slice since the previous call, in
     *  microseconds, measured at feature boundaries
     */
    uint64_t takeLongestSliceUsec();

  private:
    uint64_t iteration() const;
    void restart();

    const common::Event& event;
    std::chrono::microseconds budget;
    std::chrono::steady_clock::time_point sliceStart;
    uint64_t sliceIteration = 0;
    uint64_t yieldCount = 0;
    uint64_t longestSlice = 0;
};

/** @brief Create the enabled features of one device, in list order
 *
 *  Takes the context and list by value so they stay alive across yields.
 *  Always yields once at the end, between devices.
 */
requester::Coroutine createDeviceFeatures(SensorManager& manager,
                                          DeviceContext dev,
                                          FeatureList features,
                                          YieldBudget& budget);

} // namespace nsm::features
