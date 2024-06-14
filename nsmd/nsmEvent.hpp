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

#include "libnsm/base.h"

#include "common/types.hpp"
#include "eventHandler.hpp"

#include <memory>
#include <unordered_map>

using NsmEventId = uint8_t;

namespace nsm
{

class NsmEvent
{
  public:
    NsmEvent(const std::string& name, const std::string& type) :
        name(name), type(type)
    {}

    virtual int handle(eid_t eid, NsmType type, NsmEventId eventId,
                       const nsm_msg* event, size_t eventLen) = 0;

    const std::string& getName() const
    {
        return name;
    }

    const std::string& getType() const
    {
        return type;
    }

  private:
    const std::string name;
    const std::string type;
};

int logEvent(const std::string& messageId, Level level,
             const std::map<std::string, std::string>& data);

class EventDispatcher
{
  public:
    int addEvent(NsmType type, NsmEventId eventId, std::shared_ptr<NsmEvent>);

    int handle(eid_t eid, NsmType type, NsmEventId eventId,
               const nsm_msg* event, size_t eventLen) const;

  private:
    std::unordered_map<
        NsmType, std::unordered_map<NsmEventId, std::shared_ptr<NsmEvent>>>
        eventsMap{};
};

class DelegatingEventHandler : public EventHandler
{
  protected:
    int enableDelegation(NsmEventId eventId);

  private:
    void delegate(eid_t eid, NsmType type, NsmEventId eventId,
                  const nsm_msg* event, size_t eventLen);
};

} // namespace nsm
