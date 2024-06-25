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

#include "nsmInterface.hpp"

#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;
using OperationalStatusIntf =
    object_t<State::Decorator::server::OperationalStatus>;

class NsmGpuPresenceAndPowerStatus :
    public NsmSensor,
    public NsmInterfaceContainer<OperationalStatusIntf>
{
  public:
    NsmGpuPresenceAndPowerStatus(
        const NsmInterfaceProvider<OperationalStatusIntf>& provider,
        uint8_t gpuInstanceId);
    NsmGpuPresenceAndPowerStatus() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    uint8_t gpuInstanceId;
    uint8_t gpusPresence;
    uint8_t gpusPower;
    enum class State
    {
        GetPresence,
        GetPowerStatus
    } state;
};

} // namespace nsm
