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

#include "requester/handler.hpp"
#include "types.hpp"

namespace nsm
{

class SensorManager;
class NsmObject
{
  public:
    NsmObject() = delete;
    NsmObject(const std::string& name, const std::string& type) :
        name(name), type(type)
    {}
    NsmObject(const NsmObject& copy) : name(copy.name), type(copy.type) {}
    virtual ~NsmObject() = default;
    const std::string& getName() const
    {
        return name;
    }
    const std::string& getType() const
    {
        return type;
    }

    virtual requester::Coroutine update([[maybe_unused]] SensorManager& manager,
                                        [[maybe_unused]] eid_t eid)
    {
        co_return NSM_SW_SUCCESS;
    }

    virtual void handleOfflineState()
    {
        return;
    }

  private:
    const std::string name;
    const std::string type;
};
} // namespace nsm
