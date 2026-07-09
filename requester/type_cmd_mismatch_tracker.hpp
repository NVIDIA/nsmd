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
#include <memory>
#include <unordered_map>
#include <vector>

namespace requester
{

using eid_t = uint8_t;

// Tracks NSM response type/command mismatches per EID.
// On the first mismatch for a given (EID, type, cmd) combination, a journal
// error is emitted immediately. Subsequent identical mismatches increment a
// counter without flooding the journal. All accumulated events are re-emitted
// on logMismatches(), which is called from NsmLogDumpIntf::logDump() to ensure
// the data survives journal rotation.
class TypeCmdMismatchTracker
{
  public:
    static void record(eid_t eid, uint8_t instanceId, uint8_t expectedType,
                       uint8_t expectedCmd, uint8_t gotType, uint8_t gotCmd,
                       const std::vector<uint8_t>& reqBytes,
                       const uint8_t* respBytes, size_t respLen);

    static void logMismatches();

  private:
    struct MismatchEvent
    {
        uint8_t instanceId;
        uint8_t expectedType;
        uint8_t expectedCmd;
        uint8_t gotType;
        uint8_t gotCmd;
        uint32_t count{1};
    };

    static const size_t MAX_MISMATCH_DEBUG_EVENTS_PER_EID = 10;

    static inline std::unordered_map<eid_t, std::deque<MismatchEvent>>
        instances;

    static void logEvent(eid_t eid, const MismatchEvent& event);
};

} // namespace requester
