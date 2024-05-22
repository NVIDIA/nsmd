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

#include "nsmEvent.hpp"

namespace nsm
{

struct NsmXIDEventInfo
{
    std::string uuid;
    std::string originOfCondition;
    std::string messageId;
    Level severity;
    std::string loggingNamespace;
    std::string resolution;
    std::vector<std::string> messageArgs;
};

class NsmXIDEvent : public NsmEvent
{
  public:
    NsmXIDEvent(const std::string& name, const std::string& type,
                const NsmXIDEventInfo info);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) final;

  private:
    const NsmXIDEventInfo info;
};
} // namespace nsm
