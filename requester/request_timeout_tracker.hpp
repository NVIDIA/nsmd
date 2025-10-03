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

namespace requester
{

class DeviceRequestTimeOutTracker : protected std::deque<RequestBase>
{
  public:
    static void pushWithTimeout(const RequestBase& request);
    static void pushWithoutTimeout(const RequestBase& request);
    static void logFailures();

  private:
    inline static std::unordered_map<
        eid_t, std::shared_ptr<DeviceRequestTimeOutTracker>>
        instances;
    static DeviceRequestTimeOutTracker& instance(eid_t eid);
    void push(const RequestBase& request);
    void logTimeOutFailure() const;
    explicit DeviceRequestTimeOutTracker(eid_t eid);
    DeviceRequestTimeOutTracker(const DeviceRequestTimeOutTracker&) = delete;
    DeviceRequestTimeOutTracker&
        operator=(const DeviceRequestTimeOutTracker&) = delete;
    static const size_t MAXSIZE = 1;
    std::optional<RequestBase> timeoutMessage;
    eid_t eid;
};

} // namespace requester
