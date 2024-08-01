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

#include "nsmObject.hpp"

namespace nsm
{
class NsmEventConfig : public NsmObject
{
  public:
    NsmEventConfig(const std::string& name, const std::string& type,
                   uint8_t messageType, std::vector<uint64_t>& srcEventIds,
                   std::vector<uint64_t>& ackEventIds);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void convertIdsToMask(std::vector<uint64_t>& eventIds,
                          std::vector<bitfield8_t>& bitfields);
    requester::Coroutine
        setCurrentEventSources(SensorManager& manager, eid_t eid,
                               uint8_t nvidiaMessageType,
                               std::vector<bitfield8_t>& eventIdMasks);
    requester::Coroutine
        configureEventAcknowledgement(SensorManager& manager, eid_t eid,
                                      uint8_t nvidiaMessageType,
                                      std::vector<bitfield8_t>& eventIdMasks);

    uint8_t messageType;
    std::vector<bitfield8_t> srcEventMask;
    std::vector<bitfield8_t> ackEventMask;
};

} // namespace nsm
