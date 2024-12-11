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
#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "requester/request_timeout_tracker.hpp"
#include "sensorManager.hpp"

#include <com/nvidia/Common/LogDump/server.hpp>
#include <phosphor-logging/lg2.hpp>

namespace nsm
{
using LogDumpIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::Common::server::LogDump>;

class NsmLogDumpIntf : public LogDumpIntf
{
  public:
    NsmLogDumpIntf(sdbusplus::bus::bus& bus, const char* path) :
        LogDumpIntf(bus, path)
    {}

    void logDump() override
    {
        nsm::SensorManagerImpl::dumpReadinessLogs();
        nsm::DeviceRequestTimeOutTracker::logFailuresForAllEids();
    }
};

class NsmLogDumpTracker
{
  public:
    // Singleton access method that simply returns the instance
    static NsmLogDumpTracker& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "NsmLogDumpTracker instance is not initialized yet");
        }
        return *instance;
    }

    // Initialization method to create and setup the singleton instance
    static void initialize(sdbusplus::bus::bus& bus, const char* path)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized NsmLogDumpTracker");
        }
        static NsmLogDumpTracker inst(bus, path);
        instance = &inst;
    }

    NsmLogDumpTracker(const NsmLogDumpTracker&) = delete;
    NsmLogDumpTracker& operator=(const NsmLogDumpTracker&) = delete;
    NsmLogDumpTracker(NsmLogDumpTracker&&) = delete;
    NsmLogDumpTracker& operator=(NsmLogDumpTracker&&) = delete;

  private:
    // Private constructor to prevent direct instantiation
    NsmLogDumpTracker(sdbusplus::bus::bus& bus, const char* path)

    {
        dumpIntf = std::make_unique<NsmLogDumpIntf>(bus, path);
    }

    static inline NsmLogDumpTracker* instance = nullptr;

    nsm::NsmDeviceTable nsmDevices;
    std::unique_ptr<NsmLogDumpIntf> dumpIntf = nullptr;
};

} // namespace nsm
