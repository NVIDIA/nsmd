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

#include <coroutine>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#ifndef NSM_FEATURE_POLICY_WAIT_MS
#define NSM_FEATURE_POLICY_WAIT_MS 5000
#endif

namespace nsm::features
{

/** @brief Cache of the platform policy (NSM_Policy records)
 *
 *  arm() starts a one-shot wait; when it expires every NSM_Policy record is
 *  read once into an immutable cache and onReady is called. The policy file
 *  is optional: no records still makes the cache ready. Records added after
 *  that are logged and not applied.
 */
class PolicyStore
{
  public:
    PolicyStore(sdbusplus::bus_t& bus, const common::Event& event,
                std::function<void()> onReady);

    PolicyStore(const PolicyStore&) = delete;
    PolicyStore& operator=(const PolicyStore&) = delete;

    /** @brief Start the wait. Only the first call has an effect. */
    void arm();

    bool ready() const
    {
        return isReady;
    }

    /** @brief Overrides of the entries of this Kind that list the device, or
     *  list no devices, in entry index order
     */
    std::vector<Override> overridesFor(std::string_view kind,
                                       std::string_view name) const;

    /** @brief TemplateValues of the entries of this Kind that list the
     *  device, or list no devices; a later entry's value wins
     */
    std::map<std::string, dbus::Value>
        templateValuesFor(std::string_view kind, std::string_view name) const;

  private:
    struct Entry
    {
        bool allDevices = false;
        std::set<std::string, std::less<>> devices;
        std::vector<Override> overrides;
        std::map<std::string, dbus::Value> templateValues;
    };

    requester::Coroutine run();
    requester::Coroutine load();
    requester::Coroutine loadRecord(std::string path, std::string service);
    void interfacesAdded(sdbusplus::message::message& msg);

    const common::Event& event;
    std::function<void()> onReady;
    std::unique_ptr<sdbusplus::bus::match_t> lateMatch;

    bool armed = false;
    bool readStarted = false;
    bool isReady = false;
    std::coroutine_handle<> taskHandle;

    /** @brief Kind -> entries, in index order */
    std::map<std::string, std::vector<Entry>, std::less<>> byKind;
    /** @brief Kind -> path of the record that owns it */
    std::map<std::string, std::string> kindOwner;
};

} // namespace nsm::features
