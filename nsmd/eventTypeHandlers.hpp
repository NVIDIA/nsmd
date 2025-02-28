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

#include "eventHandler.hpp"
#include "nsmEvent.hpp"

namespace nsm
{

class EventType0Handler : public DelegatingEventHandler
{
  public:
    EventType0Handler();

    uint8_t nsmType() override
    {
        // event type0, device capability discovery
        return NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    }
};

class EventType1Handler : public DelegatingEventHandler
{
  public:
    EventType1Handler();

    uint8_t nsmType() override
    {
        return NSM_TYPE_NETWORK_PORT;
    }
};

class EventType3Handler : public DelegatingEventHandler
{
  public:
    EventType3Handler();

    uint8_t nsmType() override
    {
        return NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    }
};

} // namespace nsm
