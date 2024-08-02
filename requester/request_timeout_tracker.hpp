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

#include "request.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <queue>
#include <string>
#include <system_error>

namespace nsm
{

class DeviceRequestTimeOutTracker
{
  public:
    static std::unordered_map<eid_t,
                              std::shared_ptr<DeviceRequestTimeOutTracker>>
        instances;
    static DeviceRequestTimeOutTracker& getInstance(eid_t eid);

    bool isEmpty();
    bool isFull();
    void push(std::string nsm_request);
    void pop();
    std::string front();
    void emptyQueue();

    void handleTimeout(std::string nsm_request);
    void handleNoTimeout(std::string nsm_request);
    void logTimeOutFailure();

    static void logFailuresForAllEids();

  private:
    explicit DeviceRequestTimeOutTracker(eid_t eid);
    DeviceRequestTimeOutTracker(const DeviceRequestTimeOutTracker&) = delete;
    DeviceRequestTimeOutTracker&
        operator=(const DeviceRequestTimeOutTracker&) = delete;
    static const size_t MAXSIZE = 1;
    std::deque<std::string> messages;
    std::optional<std::string> firstTimeoutMessage;
    eid_t eid;
};

/** @class
 *  @brief Implementation of NSM TimeoutTracker
 */
class TimeOutTracker
{
  public:
    static TimeOutTracker& getInstance()
    {
        static TimeOutTracker instance;
        return instance;
    }

    DeviceRequestTimeOutTracker& getDeviceTimeOutTracker(eid_t eid)
    {
        return DeviceRequestTimeOutTracker::getInstance(eid);
    }

  private:
    TimeOutTracker() = default;
    TimeOutTracker(const TimeOutTracker&) = delete;
    TimeOutTracker& operator=(const TimeOutTracker&) = delete;
};

} // namespace nsm
