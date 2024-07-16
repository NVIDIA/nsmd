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
#include "pci-links.h"
#include "platform-environmental.h"
#include "nsmSensor.hpp"
#include <xyz/openbmc_project/Inventory/Item/Dimm/MemoryMetrics/server.hpp>
#include <telemetry_mrd_producer.hpp>
#include "nsmCommon/sharedMemCommon.hpp"
namespace nsm
{

class NsmMemoryCapacity : public NsmSensor
{
  public:
    NsmMemoryCapacity(const std::string& name, const std::string& type);
    NsmMemoryCapacity() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    virtual void updateReading(uint32_t* maximumMemoryCapacity) = 0;
};

class NsmTotalMemory : public NsmMemoryCapacity
{
  public:
    NsmTotalMemory(const std::string& name, const std::string& type);
    NsmTotalMemory() = default;
    const uint32_t* getReading();

  private:
    void updateReading(uint32_t* maximumMemoryCapacity) override;
    uint32_t* totalMemoryCapacity = nullptr;
};

using DimmMemoryMetricsIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Dimm::server::MemoryMetrics>;

class NsmMemoryCapacityUtil : public NsmSensor
{
  public:
    NsmMemoryCapacityUtil(sdbusplus::bus::bus& bus, const std::string& name,
                          const std::string& type,
                          std::string& inventoryObjPath, std::shared_ptr<NsmTotalMemory>totalMemory);
    NsmMemoryCapacityUtil() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<NsmTotalMemory> totalMemory;
    void updateReading(const struct nsm_memory_capacity_utilization& data);
    std::string inventoryObjPath;
    void updateMetricOnSharedMemory();
    std::unique_ptr<DimmMemoryMetricsIntf> dimmMemoryMetricsIntf = nullptr;
};

} // namespace nsm