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
#include "features/discovery/telemetryDiscovery.hpp"
#include "features/feature.hpp"
#include "features/policy/policyStore.hpp"

#include <sdbusplus/bus.hpp>

#include <coroutine>
#include <functional>
#include <queue>

namespace nsm::features
{

class YieldBudget;

/** @brief Creates device features from discovered devices and the policy
 *
 *  The first discovered device arms the policy wait. Devices are queued
 *  until the policy is ready, then resolved one at a time, in arrival order:
 *  defaults < NSM_Telemetry < matching NSM_Policy entries.
 */
class DeviceCoordinator
{
  public:
    /** @param[in] onDeviceCreated - called after each device's features are
     *  created
     */
    DeviceCoordinator(SensorManager& manager, sdbusplus::bus_t& bus,
                      const common::Event& event,
                      std::function<void()> onDeviceCreated);

    DeviceCoordinator(const DeviceCoordinator&) = delete;
    DeviceCoordinator& operator=(const DeviceCoordinator&) = delete;

  private:
    void onDevice(DiscoveredDevice device);
    void start();
    requester::Coroutine processQueue();
    requester::Coroutine resolveAndCreate(DiscoveredDevice device,
                                          YieldBudget& budget);

    SensorManager& manager;
    const common::Event& event;
    std::function<void()> onDeviceCreated;

    std::queue<DiscoveredDevice> pending;
    std::coroutine_handle<> taskHandle;

    PolicyStore policy;
    TelemetryDiscovery discovery;
};

} // namespace nsm::features
