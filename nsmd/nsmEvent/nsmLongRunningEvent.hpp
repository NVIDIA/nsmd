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

#include "common/timer.hpp"
#include "nsmEvent.hpp"

namespace nsm
{
class NsmLongRunningEvent :
    public NsmEvent,
    public std::enable_shared_from_this<NsmLongRunningEvent>
{
  public:
    explicit NsmLongRunningEvent(const std::string& name,
                                 const std::string& type, bool isLongRunning);
    uint8_t acceptInstanceId = 0xFF;
    bool isLongRunning;
    common::TimerAwaiter timer;
    bool initAcceptInstanceId(uint8_t instanceId, uint8_t cc, uint8_t rc);

  protected:
    int validateEvent(eid_t eid, const nsm_msg* event, size_t eventLen);
};

} // namespace nsm
