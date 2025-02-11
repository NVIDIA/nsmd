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

#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmInterface.hpp"
#include "nsmLongRunningSensor.hpp"
#include "nsmSensor.hpp"

#include <telemetry_mrd_producer.hpp>
#include <xyz/openbmc_project/Inventory/Item/Cpu/OperatingConfig/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/MemoryMetrics/server.hpp>
namespace nsm
{

using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

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
    virtual void
        updateReading(std::optional<uint32_t> maximumMemoryCapacity) = 0;
};

class NsmTotalMemory : public NsmMemoryCapacity
{
  public:
    NsmTotalMemory(const std::string& name, const std::string& type);
    NsmTotalMemory() = default;
    const std::optional<uint32_t> getReading();

  private:
    void updateReading(std::optional<uint32_t> maximumMemoryCapacity) override;
    std::optional<uint32_t> totalMemoryCapacity = std::nullopt;
};

using DimmMemoryMetricsIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Dimm::server::MemoryMetrics>;

class NsmMemoryCapacityUtil :
    public NsmLongRunningSensor,
    public NsmInterfaceContainer<DimmMemoryMetricsIntf>
{
  public:
    NsmMemoryCapacityUtil(
        const NsmInterfaceProvider<DimmMemoryMetricsIntf>& provider,
        std::shared_ptr<NsmTotalMemory> totalMemory, bool isLongRunning,
        std::shared_ptr<NsmDevice> device);
    NsmMemoryCapacityUtil() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    bool equals(const NsmSensor& other) const override;

  private:
    std::shared_ptr<NsmTotalMemory> totalMemory;
    void updateReading(const struct nsm_memory_capacity_utilization& data);
    void updateMetricOnSharedMemory();
    requester::Coroutine update(SensorManager& manager, eid_t eid);
};

using CpuOperatingConfigIntf =
    object_t<Inventory::Item::Cpu::server::OperatingConfig>;
class NsmMinGraphicsClockLimit : public NsmObject
{
  public:
    NsmMinGraphicsClockLimit(
        std::string& name, std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
        std::string& inventoryObjPath);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

    void updateMetricOnSharedMemory() override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmMaxGraphicsClockLimit : public NsmObject
{
  public:
    NsmMaxGraphicsClockLimit(
        std::string& name, std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
        std::string& inventoryObjPath);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

    void updateMetricOnSharedMemory() override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
    std::string inventoryObjPath;
};

} // namespace nsm
