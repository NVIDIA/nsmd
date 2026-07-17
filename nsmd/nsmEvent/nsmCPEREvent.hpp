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

#include <coroutine>
#include <memory>
#include <queue>
#include <vector>

namespace nsm
{

inline constexpr uint16_t BYTES_PER_KB = 1024;
inline constexpr uint16_t MAX_CPER_RECORD_DATA_SIZE_KB = 16;
inline constexpr uint16_t MAX_EVENT_HANDLES = 255;
inline constexpr uint16_t PENDING_HANDLE_VALUE = 0;
inline constexpr uint16_t NO_MORE_HANDLES = 0xFFFF;
inline constexpr uint8_t DEFAULT_CPER_FORMAT = 0x00;
using eventHandles_t = std::queue<uint16_t>;
class NsmDevice;
class NsmEventLogRecordV2;

class NsmCPEREvent : public NsmEvent
{
  public:
    NsmCPEREvent(std::shared_ptr<NsmDevice> nsmDevice, const std::string& name,
                 const std::string& type);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) final;
    void logRecordOnRf(std::vector<uint8_t>& recordChunk, uint8_t returnCode,
                       uint16_t nextEventHandle, uint8_t eventVersion);

    void setEventLogRecordChunkCollector(
        std::shared_ptr<NsmEventLogRecordV2> collector);

  private:
    std::shared_ptr<NsmDevice> nsmDevice;
    std::coroutine_handle<> cperRecordLoggerHandle;
    std::shared_ptr<NsmEventLogRecordV2> eventLogRecordChunkCollector;
    eventHandles_t eventHandles;
    requester::Coroutine cperRecordLogger();
    std::vector<uint8_t> cperRecordData;
    uint8_t cperEventFormatVersion{NSM_EVENT_VERSION};
};

requester::Coroutine createNsmCPEREvent(std::shared_ptr<NsmDevice> nsmDevice,
                                        const std::string& name,
                                        const std::string& type);

} // namespace nsm
