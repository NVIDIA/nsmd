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

#include "nsmObject.hpp"

#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <memory>
#include <string>

namespace nsm
{

using GpuOperationalStatusIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::State::
                                    Decorator::server::OperationalStatus>;

/**
 * @brief Discovery-driven OperationalStatus for GPU (DeviceType 0)
 *
 * Sets State/Functional based on NsmDevice discovery state:
 *   - getEid() == 0  -> State=None,              Functional=false
 *   - isOnline()      -> State=Enabled,            Functional=true
 *   - else (offline)  -> State=UnavailableOffline,  Functional=false
 */
class NsmGpuOperationalStatus : public NsmObject
{
  public:
    NsmGpuOperationalStatus(sdbusplus::bus_t& bus, const std::string& name,
                            const std::string& type,
                            const std::string& inventoryObjPath);
    NsmGpuOperationalStatus() = delete;

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

    void handleOfflineState() override;

    void updateMetricOnSharedMemory() override;

  private:
    std::string inventoryObjPath;
    std::unique_ptr<GpuOperationalStatusIntf> operationalStatusIntf;
};

} // namespace nsm
