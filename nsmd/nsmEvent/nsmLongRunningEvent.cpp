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

#include "nsmObject.hpp"

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
int NsmLongRunningEvent::validateEvent(eid_t eid, const nsm_msg* event,
                                       size_t eventLen)
{
    uint8_t instanceId;
    auto rc = decode_long_running_event(event, eventLen, &instanceId, nullptr,
                                        nullptr);

    if (shouldLog("decode_long_running_event", nsm_sw_codes(rc)))
    {
        LG2_ERROR("decode_long_running_event failure, eid: {EID}, rc: {RC}",
                  "EID", eid, "RC", rc);
    }
    if (shouldLog("timer.expired()", timer.expired()))
    {
        LG2_ERROR("LongRunning timer expired, eid: {EID}", "EID", eid);
    }
    if (shouldLog("acceptInstanceId == 0xFF || !isLongRunning",
                  acceptInstanceId == 0xFF || !isLongRunning))
    {
        LG2_ERROR("LongRunning not started or not accepted, eid: {EID}", "EID",
                  eid);
    }
    if (shouldLog("acceptInstanceId != instanceId",
                  acceptInstanceId != instanceId))
    {
        LG2_ERROR(
            "Instance ID mismatch, eid: {EID}, acceptInstanceId={AID}, instanceId={ID}",
            "EID", eid, "AID", acceptInstanceId, "ID", instanceId);
    }

    if (timer.expired() || (acceptInstanceId == 0xFF || !isLongRunning))
    {
        rc = NSM_SW_ERROR_COMMAND_FAIL;
    }
    else if (acceptInstanceId != instanceId)
    {
        rc = NSM_SW_ERROR_DATA;
    }
    return rc;
}

} // namespace nsm
