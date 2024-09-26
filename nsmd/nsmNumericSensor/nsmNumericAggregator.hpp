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

    void logFalseValid(const uint8_t tag)
    {
        if (shouldLogDebug(tag))
        {
            lg2::debug(
                "NsmNumericAggregator: False Valid bit in Tag {TAG} for Aggregator {NAME} of type {TYPE}.",
                "TAG", tag, "NAME", getName(), "TYPE", getType());
        }
    }

    void clearTagBitMap(std::string funcName)
    {
        if (tag_map.isAnyBitSet)
        {
            lg2::error("handleResponseMsg: {FUNCNAME} Aggregator {NAME}"
                       " of type {TYPE} | Bits Invalid Code Cleared for"
                       " Tag(s) : [{TAGCLEAREDBITS}]",
                       "FUNCNAME", funcName, "NAME", getName(), "TYPE",
                       getType(), "TAGCLEAREDBITS", tag_map.getSetBits());
        }
        // Clear the bitMaps
        for (int i = 0; i < 8; i++)
        {
            tag_map.bitMap.fields[i].byte = 0;
        }
        tag_map.isAnyBitSet = false;
    }

    bool priority;

  protected:
    int updateSensorReading(uint8_t tag, double reading,
                            uint64_t timestamp = 0);
    int updateSensorNotWorking(uint8_t tag, bool valid);

  private:
    std::array<std::shared_ptr<NsmNumericSensorValueAggregate>,
               NSM_AGGREGATE_MAX_SAMPLE_TAG_VALUE>
        sensors{};
    utils::bitfield256_err_code tag_map;

    bool shouldLogDebug(const uint8_t tag)
    {
        tag_map.isAnyBitSet = true;
        if (tag == 0)
        {
            if (tag_map.bitMap.fields[0].byte & 1)
            {
                return false;
            }
            else
            {
                tag_map.bitMap.fields[0].byte |= 1;
                return true;
            }
        }
        return !tag_map.isBitSet(tag);
    }
};
} // namespace nsm
