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

#include <tal.hpp>

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
    const bool& isRefreshed() const
    {
        return refreshed;
    }
    void setRefreshed(bool r)
    {
        refreshed = r;
    }
    inline bool isStatic() const
    {
        return staticSensor;
    }
    inline void setStatic(bool s)
    {
        staticSensor = s;
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

    void updateSharedMemoryOnSuccess(
        std::string& inventoryObjPath, std::string& ifaceName,
        std::string& propName, std::vector<uint8_t>& smbusData,
        nv::sensor_aggregation::DbusVariantType propValue)
    {
#ifdef NVIDIA_SHMEM
        auto timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        tal::TelemetryAggregator::updateTelemetry(inventoryObjPath, ifaceName,
                                                  propName, smbusData,
                                                  timestamp, 0, propValue);
#endif
    }

  private:
    const std::string name;
    const std::string type;
    bool refreshed = false;
    bool staticSensor = false;
};
} // namespace nsm
