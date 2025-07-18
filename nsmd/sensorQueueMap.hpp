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

#include "nsmDevice.hpp"

#include <memory>
#include <unordered_map>

namespace nsm
{

class LimitedSensorQueue
{
  public:
    LimitedSensorQueue(SensorQueue& sensors) :
        sensors(sensors), countToUpdate(sensors.size())
    {}

    void next()
    {
        if (countToUpdate)
        {
            sensors.next();
            --countToUpdate;
        }
    }

    std::shared_ptr<NsmObject>& current()
    {
        return sensors.current();
    }

    bool hasSensorsToUpdate() const
    {
        return countToUpdate > 0 && sensors.size() > 0;
    }

  private:
    SensorQueue& sensors;
    size_t countToUpdate;
};

using SensorQueueUnorderedMap =
    std::unordered_map<PollingType, std::shared_ptr<LimitedSensorQueue>>;
class SensorQueueMap : public SensorQueueUnorderedMap
{
  public:
    SensorQueueMap(const SensorQueueUnorderedMap& sensors) :
        SensorQueueUnorderedMap(sensors)
    {
        if (sensors.empty())
        {
            throw std::runtime_error("SensorQueueMap: sensors is empty");
        }
    }

    bool hasSensorsToUpdate() const
    {
        for (auto& [_, sensorQueue] : *this)
        {
            if (sensorQueue->hasSensorsToUpdate())
            {
                return true;
            }
        }
        return false;
    };

    void nextSensorState(PollingType& pollingType) const
    {
        if (!contains(pollingType))
        {
            pollingType = begin()->first;
            return;
        }
        const auto startPollingType = pollingType;
        do
        {
            int currentMapIndex = std::distance(begin(), find(pollingType));
            int nextMapIndex = (currentMapIndex + 1) % size();
            pollingType = std::next(begin(), nextMapIndex)->first;
        } while (pollingType != startPollingType &&
                 !this->at(pollingType)->hasSensorsToUpdate());
    };
};

} // namespace nsm
