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

#pragma once

#include "base.h"

#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>

namespace requester
{

using eid_t = uint8_t;

enum class MismatchReason : uint8_t
{
    TypeCmdRejected, // instanceId matched but type/cmd wrong
    NotFound,        // no outstanding request matched
};

// Tracks NSM response mismatches per EID with rate limiting.
// First occurrence of each unique (EID, reason, type/cmd) combination is
// logged immediately with raw bytes. Subsequent identical occurrences
// increment a counter silently. All events are re-emitted on logMismatches(),
// called from NsmLogDumpIntf::logDump() to survive journal rotation.
class ResponseMismatchTracker
{
  public:
    // instanceId matched but type/cmd wrong; both request and response bytes
    // are available for logging.
    static void recordTypeCmdRejected(eid_t eid, uint8_t instanceId,
                                      uint8_t expectedType, uint8_t expectedCmd,
                                      uint8_t gotType, uint8_t gotCmd,
                                      const std::vector<uint8_t>& reqBytes,
                                      const uint8_t* respBytes, size_t respLen);

    // No outstanding request matched; response bytes only.
    static void recordNotFound(eid_t eid, uint8_t tag, uint8_t instanceId,
                               uint8_t gotType, uint8_t gotCmd,
                               const uint8_t* respBytes, size_t respLen);

    static void logMismatches();

  private:
    struct MismatchEvent
    {
        MismatchReason reason;
        uint8_t instanceId;
        uint8_t expectedType; // 0 for NotFound
        uint8_t expectedCmd;  // 0 for NotFound
        uint8_t gotType;
        uint8_t gotCmd;
        uint32_t count{1};
    };

    static constexpr size_t MAX_EVENTS_PER_EID = 10;

    static inline std::unordered_map<eid_t, std::deque<MismatchEvent>>
        instances;

    // If a matching event exists, increments its count and returns true.
    // Otherwise evicts oldest if at capacity and returns false (caller must
    // push_back a new entry and emit the first-occurrence log).
    static bool dedupOrEvict(eid_t eid, MismatchReason reason,
                             uint8_t expectedType, uint8_t expectedCmd,
                             uint8_t gotType, uint8_t gotCmd);

    static void logEvent(eid_t eid, const MismatchEvent& event);
};

} // namespace requester
