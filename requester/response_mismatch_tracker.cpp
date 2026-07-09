/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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

#include "response_mismatch_tracker.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace requester
{

bool ResponseMismatchTracker::dedupOrEvict(eid_t eid, MismatchReason reason,
                                           uint8_t expectedType,
                                           uint8_t expectedCmd, uint8_t gotType,
                                           uint8_t gotCmd)
{
    auto& queue = instances[eid];
    for (auto& event : queue)
    {
        if (event.reason == reason && event.expectedType == expectedType &&
            event.expectedCmd == expectedCmd && event.gotType == gotType &&
            event.gotCmd == gotCmd)
        {
            event.count++;
            return true;
        }
    }
    if (queue.size() == MAX_EVENTS_PER_EID)
    {
        queue.pop_front();
    }
    return false;
}

// Not thread-safe; must be called from the single-threaded event loop only.
void ResponseMismatchTracker::recordTypeCmdRejected(
    eid_t eid, uint8_t instanceId, uint8_t expectedType, uint8_t expectedCmd,
    uint8_t gotType, uint8_t gotCmd, const std::vector<uint8_t>& reqBytes,
    const uint8_t* respBytes, size_t respLen)
{
    if (dedupOrEvict(eid, MismatchReason::TypeCmdRejected, expectedType,
                     expectedCmd, gotType, gotCmd))
    {
        return;
    }

    std::vector<uint8_t> reqCopy(reqBytes);
    std::vector<uint8_t> respCopy(respBytes, respBytes + respLen);
    std::string reqHex = utils::requestMsgToHexString(reqCopy);
    std::string respHex = utils::requestMsgToHexString(respCopy);

    lg2::error("NSM response mismatch [TYPE_CMD_REJECTED]: EID={EID} "
               "instanceId={IID} expected type={ETYPE} cmd={ECMD} "
               "got type={GTYPE} cmd={GCMD} request={REQ} response={RESP}",
               "EID", eid, "IID", instanceId, "ETYPE", expectedType, "ECMD",
               expectedCmd, "GTYPE", gotType, "GCMD", gotCmd, "REQ", reqHex,
               "RESP", respHex);

    instances[eid].push_back({MismatchReason::TypeCmdRejected, instanceId,
                              expectedType, expectedCmd, gotType, gotCmd, 1});
}

// Not thread-safe; must be called from the single-threaded event loop only.
void ResponseMismatchTracker::recordNotFound(eid_t eid, uint8_t tag,
                                             uint8_t instanceId,
                                             uint8_t gotType, uint8_t gotCmd,
                                             const uint8_t* respBytes,
                                             size_t respLen)
{
    if (dedupOrEvict(eid, MismatchReason::NotFound, 0, 0, gotType, gotCmd))
    {
        return;
    }

    std::vector<uint8_t> respCopy(respBytes, respBytes + respLen);
    std::string respHex = utils::requestMsgToHexString(respCopy);

    lg2::error("NSM response mismatch [NOT_FOUND]: EID={EID} tag={TAG} "
               "instanceId={IID} got type={GTYPE} cmd={GCMD} response={RESP}",
               "EID", eid, "TAG", tag, "IID", instanceId, "GTYPE", gotType,
               "GCMD", gotCmd, "RESP", respHex);

    instances[eid].push_back(
        {MismatchReason::NotFound, instanceId, 0, 0, gotType, gotCmd, 1});
}

void ResponseMismatchTracker::logEvent(eid_t eid, const MismatchEvent& ev)
{
    if (ev.reason == MismatchReason::TypeCmdRejected)
    {
        lg2::error("NSM response mismatch [TYPE_CMD_REJECTED]: EID={EID} "
                   "instanceId={IID} expected type={ETYPE} cmd={ECMD} "
                   "got type={GTYPE} cmd={GCMD} count={COUNT}",
                   "EID", eid, "IID", ev.instanceId, "ETYPE", ev.expectedType,
                   "ECMD", ev.expectedCmd, "GTYPE", ev.gotType, "GCMD",
                   ev.gotCmd, "COUNT", ev.count);
    }
    else
    {
        lg2::error("NSM response mismatch [NOT_FOUND]: EID={EID} "
                   "instanceId={IID} got type={GTYPE} cmd={GCMD} count={COUNT}",
                   "EID", eid, "IID", ev.instanceId, "GTYPE", ev.gotType,
                   "GCMD", ev.gotCmd, "COUNT", ev.count);
    }
}

void ResponseMismatchTracker::logMismatches()
{
    for (const auto& [eid, queue] : instances)
    {
        if (queue.empty())
        {
            continue;
        }
        lg2::error("******Start logMismatches: EID={EID}*****", "EID", eid);
        for (const auto& event : queue)
        {
            logEvent(eid, event);
        }
        lg2::error("******End logMismatches: EID={EID}*****", "EID", eid);
    }
}

} // namespace requester
