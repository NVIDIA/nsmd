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

#include "nsmEventInfo.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace nsm
{

/** @brief Handler for the Runtime IST Complete v1 event (Type 4 / EventID 1).
 *
 *  Decodes the wire payload, maps the spec's result enum (0=Fail, 1=Pass,
 *  anything else => Unknown) to a string for the Redfish message, and -- if
 *  enabled by the EM JSON Logging flag in NsmEventInfo -- emits a phosphor
 *  log entry. Pass and Fail follow the same code path.
 *
 *  Severity is derived from the wire payload per the RIST spec table
 *  (Result + StatusCode[63:62]) at handle() time; the EM JSON "Severity"
 *  property is not consulted (and intentionally not read by the factory)
 *  for this event. Resolution for the Pass row is the spec literal
 *  "None"; for failure rows it is taken from the EM JSON "Resolution"
 *  property.
 */
class NsmRuntimeISTCompleteEvent : public NsmEvent
{
  public:
    NsmRuntimeISTCompleteEvent(const std::string& name, const std::string& type,
                               const NsmEventInfo& info);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) final;

  private:
    /** @brief Derive {Severity, Resolution} per the RIST spec table from
     *         the payload's Result byte and the top 2 bits of StatusCode.
     *
     *  Spec table:
     *    Result=1, StatusCode[63:62]=0 -> Informational, "None"
     *    Result=0, StatusCode[63:62]=1 -> Warning,       info.resolution
     *    Result=0, StatusCode[63:62]=2 -> Critical,      info.resolution
     *
     *  The Pass row's "None" is a spec literal kept inline; the failure
     *  rows take the actionable text from the EM JSON Resolution config
     *  (info.resolution). Any other (result, statusClass) pair is undefined
     *  by the spec and falls back to {Critical, info.resolution} so the
     *  event is still surfaced loudly. handle() detects the undefined
     *  case separately to emit a diagnostic warning.
     */
    std::pair<Level, std::string>
        deriveRistSeverity(uint8_t result, uint64_t status_code) const;

    const NsmEventInfo info;
};

} // namespace nsm
