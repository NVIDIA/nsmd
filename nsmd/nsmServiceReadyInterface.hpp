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
#include "nsmDevice.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/State/ServiceReady/common.hpp>
#include <xyz/openbmc_project/State/ServiceReady/server.hpp>

#include <map>
#include <memory>
#include <mutex>

namespace nsm
{
using ServiceReadyIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::server::ServiceReady>;

class NsmServiceReadyIntf
{
  public:
    // Singleton access method that simply returns the instance
    static NsmServiceReadyIntf& getInstance()
    {
        if (!instance)
        {
            throw std::runtime_error(
                "NsmServiceReadyIntf instance is not initialized yet");
        }
        return *instance;
    }

    // Initialization method to create and setup the singleton instance
    static void initialize(sdbusplus::bus::bus& bus, const char* path,
                           nsm::NsmDeviceTable& nsmDevices)
    {
        if (instance)
        {
            throw std::logic_error(
                "Initialize called on an already initialized NsmServiceReadyIntf");
        }
        static NsmServiceReadyIntf inst(bus, path, nsmDevices);
        instance = &inst;
    }

    NsmServiceReadyIntf(const NsmServiceReadyIntf&) = delete;
    NsmServiceReadyIntf& operator=(const NsmServiceReadyIntf&) = delete;
    NsmServiceReadyIntf(NsmServiceReadyIntf&&) = delete;
    NsmServiceReadyIntf& operator=(NsmServiceReadyIntf&&) = delete;

    void setStateEnabled()
    {
        serviceIntf->state(ServiceReadyIntf::States::Enabled);
    }

    void setStateStarting()
    {
        serviceIntf->state(ServiceReadyIntf::States::Starting);
    }

  private:
    // Private constructor to prevent direct instantiation
    NsmServiceReadyIntf(sdbusplus::bus::bus& bus, const char* path,
                        nsm::NsmDeviceTable& nsmDevices) :
        nsmDevices(nsmDevices)
    {
        serviceIntf = std::make_unique<ServiceReadyIntf>(bus, path);
        serviceIntf->state(ServiceReadyIntf::States::Starting);
        serviceIntf->serviceType(ServiceReadyIntf::ServiceTypes::NSM);
    }

    static inline NsmServiceReadyIntf* instance = nullptr;

    nsm::NsmDeviceTable nsmDevices;
    std::unique_ptr<ServiceReadyIntf> serviceIntf = nullptr;
};
} // namespace nsm
