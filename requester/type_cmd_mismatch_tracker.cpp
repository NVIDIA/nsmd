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

#include "type_cmd_mismatch_tracker.hpp"

#include "common/utils.hpp"

#include <phosphor-logging/lg2.hpp>

namespace requester
{

void TypeCmdMismatchTracker::logEvent(eid_t eid, const MismatchEvent& event)
{
    lg2::error("NSM type/command mismatch: EID={EID} instanceId={IID} "
               "expected type={ETYPE} cmd={ECMD}, got type={RTYPE} cmd={RCMD} "
               "count={COUNT}",
               "EID", eid, "IID", event.instanceId, "ETYPE", event.expectedType,
               "ECMD", event.expectedCmd, "RTYPE", event.gotType, "RCMD",
               event.gotCmd, "COUNT", event.count);
}

// Not thread-safe; must be called from the single-threaded event loop only.
void TypeCmdMismatchTracker::record(eid_t eid, uint8_t instanceId,
                                    uint8_t expectedType, uint8_t expectedCmd,
                                    uint8_t gotType, uint8_t gotCmd,
                                    const std::vector<uint8_t>& reqBytes,
                                    const uint8_t* respBytes, size_t respLen)
{
    auto& queue = instances[eid];

    // Check if the same mismatch (same type/cmd pair) is already tracked.
    for (auto& event : queue)
    {
        if (event.expectedType == expectedType &&
            event.expectedCmd == expectedCmd && event.gotType == gotType &&
            event.gotCmd == gotCmd)
        {
            event.count++;
            // Only the first occurrence is logged to journal with raw bytes;
            // subsequent identical mismatches are counted silently and
            // re-emitted on logMismatches().
            return;
        }
    }

    // New mismatch combination — log immediately with raw request and response
    // bytes so the full context is available for decoding against the NSM spec.
    std::vector<uint8_t> reqCopy(reqBytes);
    std::vector<uint8_t> respCopy(respBytes, respBytes + respLen);
    std::string reqHex = utils::requestMsgToHexString(reqCopy);
    std::string respHex = utils::requestMsgToHexString(respCopy);

    lg2::error("NSM type/command mismatch: EID={EID} instanceId={IID} "
               "expected type={ETYPE} cmd={ECMD}, got type={RTYPE} cmd={RCMD} "
               "request={REQ} response={RESP}",
               "EID", eid, "IID", instanceId, "ETYPE", expectedType, "ECMD",
               expectedCmd, "RTYPE", gotType, "RCMD", gotCmd, "REQ", reqHex,
               "RESP", respHex);

    if (queue.size() == MAX_MISMATCH_DEBUG_EVENTS_PER_EID)
    {
        queue.pop_front();
    }
    queue.push_back(
        {instanceId, expectedType, expectedCmd, gotType, gotCmd, 1});
}

void TypeCmdMismatchTracker::logMismatches()
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
