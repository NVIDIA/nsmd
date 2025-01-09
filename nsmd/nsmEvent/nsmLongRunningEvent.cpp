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

#include "config.h"

#include "nsmLongRunningEvent.hpp"

namespace nsm
{

NsmLongRunningEvent::NsmLongRunningEvent(const std::string& name,
                                         const std::string& type,
                                         bool isLongRunning) :
    NsmEvent(name, type + "_LongRunningEvent"),
    isLongRunning(isLongRunning), timer(RESPONSE_TIME_OUT_LONG_RUNNING)
{}

bool NsmLongRunningEvent::initAcceptInstanceId(uint8_t instanceId, uint8_t cc,
                                               uint8_t rc)
{
    auto accepted = rc == NSM_SW_SUCCESS && cc == NSM_ACCEPTED;
    acceptInstanceId = accepted ? instanceId : 0xFF;
    return accepted;
}
bool NsmLongRunningEvent::validateEvent(eid_t eid, const nsm_msg* event,
                                        size_t eventLen)
{
    if (!timer.stop())
    {
        lg2::error(
            "NsmLongRunningEvent::validateEvent: LongRunning timer not stopped, eid={EID}",
            "EID", eid);
        return false;
    }

    uint8_t instanceId;
    auto rc = decode_long_running_event(event, eventLen, &instanceId, nullptr,
                                        nullptr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmLongRunningEvent::validateEvent: Failed to decode long running event, eid: {EID}, rc: {RC}",
            "EID", eid, "RC", rc);
        return false;
    }
    else if (timer.expired())
    {
        lg2::error(
            "NsmLongRunningEvent::validateEvent: LongRunning timer expired, eid: {EID}",
            "EID", eid);
        return false;
    }
    else if (acceptInstanceId == 0xFF || !isLongRunning)
    {
        lg2::error(
            "NsmLongRunningEvent::validateEvent: LongRunning not started or not accepted, eid: {EID}",
            "EID", eid);
        return false;
    }
    else if (acceptInstanceId != instanceId)
    {
        lg2::error(
            "NsmLongRunningEvent::validateEvent: Instance ID mismatch, eid: {EID}, acceptInstanceId: {ACCEPT_INSTANCE_ID}, instanceId: {INSTANCE_ID}",
            "EID", eid, "ACCEPT_INSTANCE_ID", acceptInstanceId, "INSTANCE_ID",
            instanceId);
        return false;
    }
    return true;
}

} // namespace nsm
