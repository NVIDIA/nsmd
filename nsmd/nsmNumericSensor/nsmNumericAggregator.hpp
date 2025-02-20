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

#include "base.h"

#include "common/utils.hpp"
#include "nsmSensorAggregator.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace nsm
{
class NsmNumericSensorValueAggregate;

class NsmNumericAggregator : public NsmSensorAggregator
{
  public:
    NsmNumericAggregator(const std::string& name, const std::string& type,
                         bool priority) :
        NsmSensorAggregator(name, type),
        priority(priority){};
    virtual ~NsmNumericAggregator() = default;

    int addSensor(uint8_t tag,
                  std::shared_ptr<NsmNumericSensorValueAggregate> sensor);

    const NsmNumericSensorValueAggregate* getSensor(uint8_t tag)
    {
        return sensors[tag].get();
    };

    void logFalseValid(const uint8_t tag, bool valid);

    bool priority;
    // Override the update function
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  protected:
    int updateSensorReading(uint8_t tag, double reading,
                            uint64_t timestamp = 0);
    int updateSensorNotWorking(uint8_t tag, bool valid);

  private:
    std::array<std::shared_ptr<NsmNumericSensorValueAggregate>,
               NSM_AGGREGATE_MAX_SAMPLE_TAG_VALUE>
        sensors{};
    utils::Bitfield256 tag_map;

    bool shouldLogDebug(const uint8_t tag);
};
} // namespace nsm
