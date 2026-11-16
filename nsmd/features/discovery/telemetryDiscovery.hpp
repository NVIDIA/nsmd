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

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdeventplus/source/event.hpp>

#include <coroutine>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace nsm::features
{

/** @brief A device described by an NSM_Telemetry record */
struct DiscoveredDevice
{
    /** @brief EM object path of the NSM_Telemetry record */
    std::string telemetryPath;
    /** @brief Entity Name, from the parent's Configuration.Probe */
    std::string name;
    /** @brief Identity, from the record's Kind, InstanceNumber and EID */
    std::string kind;
    uint8_t instance = 0;
    std::optional<eid_t> eid;
    /** @brief The record's "ParentChassis" as EM gives it; empty if absent */
    std::string parentChassis;
    /** @brief The record's "PlatformPrefix"; empty if absent */
    std::string platformPrefix;
    /** @brief Device variables: NSM_Telemetry record properties as EM
     *  published them, except Name, Type, the identity keys and the built-in
     *  variable names */
    std::map<std::string, dbus::Value> variables;
    /** @brief Decoded NSM_Telemetry.FeatureOverrides; empty if absent */
    std::vector<Override> telemetry;
};

/** @brief Finds NSM_Telemetry records
 *
 *  Scans existing records at startup and handles new ones from
 *  InterfacesAdded. Each record path is reported once, in arrival order.
 */
class TelemetryDiscovery
{
  public:
    TelemetryDiscovery(sdbusplus::bus_t& bus, const common::Event& event,
                       std::function<void(DiscoveredDevice)> onDevice);

    TelemetryDiscovery(const TelemetryDiscovery&) = delete;
    TelemetryDiscovery& operator=(const TelemetryDiscovery&) = delete;

    /** @brief Allow a record to be reported again, e.g. after it failed */
    void forget(const std::string& telemetryPath);

  private:
    void scan();
    void interfacesAdded(sdbusplus::message::message& msg);
    void enqueue(const std::string& telemetryPath);
    requester::Coroutine processQueue();
    requester::Coroutine readDevice(std::string telemetryPath,
                                    DiscoveredDevice& out);

    std::function<void(DiscoveredDevice)> onDevice;

    std::unique_ptr<sdeventplus::source::Defer> deferScan;
    std::unique_ptr<sdbusplus::bus::match_t> addedMatch;

    std::queue<std::string> pending;
    std::set<std::string> seen;
    std::coroutine_handle<> taskHandle;
};

} // namespace nsm::features
