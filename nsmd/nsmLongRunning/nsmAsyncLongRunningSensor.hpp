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

#include "nsmAsyncSensor.hpp"
#include "nsmLongRunningEvent.hpp"

namespace nsm
{

class NsmAsyncLongRunningSensor :
    public NsmAsyncSensor,
    public NsmLongRunningEvent
{
  public:
    explicit NsmAsyncLongRunningSensor(const std::string& name,
                                       const std::string& type,
                                       bool isLongRunning,
                                       std::shared_ptr<NsmDevice> device,
                                       uint8_t messageType,
                                       uint8_t commandCode);
    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) override;

  private:
    requester::Coroutine update(SensorManager& manager,
                                eid_t eid) override final;
    requester::Coroutine updateLongRunningSensor(SensorManager& manager,
                                                 eid_t eid);

    std::shared_ptr<NsmDevice> device = nullptr;
    uint8_t messageType;
    uint8_t commandCode;
};

} // namespace nsm
