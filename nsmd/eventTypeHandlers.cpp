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

#include "eventTypeHandlers.hpp"

#include "device-capability-discovery.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "network-ports.h"
#include "platform-environmental.h"

namespace nsm
{

EventType0Handler::EventType0Handler()
{
    enableDelegation(NSM_REDISCOVERY_EVENT);
    enableDelegation(NSM_LONG_RUNNING_EVENT);
    enableDelegation(NSM_GPIO_STATE_CHANGE_EVENT);
}

EventType1Handler::EventType1Handler()
{
    enableDelegation(NSM_THRESHOLD_EVENT);
    enableDelegation(NSM_FABRIC_MANAGER_STATE_EVENT);
}

EventType3Handler::EventType3Handler()
{
    enableDelegation(NSM_XID_EVENT);
    enableDelegation(NSM_RESET_REQUIRED_EVENT);
    enableDelegation(NSM_CPER_EVENT);
}

EventType4Handler::EventType4Handler()
{
    enableDelegation(NSM_RUNTIME_IST_COMPLETE_EVENT);
    enableDelegation(NSM_DIAG_GET_SYSTEM_CONFIG_EVENT);
    enableDelegation(NSM_DIAG_GET_TID_CONFIG_EVENT);
    enableDelegation(NSM_DIAG_SET_TEST_RESULT_EVENT);
    enableDelegation(NSM_DIAG_SET_FLOW_CONTROL_EVENT);
}

EventType5Handler::EventType5Handler()
{
    // Device Configuration Request v1 (signoff): Type 5, event ID 1, class 0
    enableDelegation(NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1);
}

} // namespace nsm
