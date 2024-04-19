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
#include "types.hpp"

struct nsm_msg;

namespace nsm
{
class NsmSensor : public NsmObject
{
  public:
    NsmSensor() = delete;
    NsmSensor(const std::string& name, const std::string& type) :
        NsmObject(name, type)
    {}
    NsmSensor(const NsmObject& copy) : NsmObject(copy)
    {}

    virtual std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) = 0;

    virtual uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                                      size_t responseLen) = 0;

    virtual requester::Coroutine update(SensorManager& manager,
                                        eid_t eid) override;
};

} // namespace nsm
