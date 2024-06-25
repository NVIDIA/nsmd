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

#include "nsmChassis/nsmPowerControl.hpp"
#include "nsmClearPowerCapIface.hpp"
#include "nsmCommon/nsmCommon.hpp"
#include "nsmCpuOperatingConfigInterface.hpp"
#include "nsmEccModeIface.hpp"
#include "nsmInterface.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmMigModeIface.hpp"
#include "nsmPowerCapIface.hpp"
#include "nsmSensor.hpp"

#include <stdint.h>

#include <com/nvidia/Edpp/server.hpp>
#include <com/nvidia/MigMode/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/MemoryMetrics/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PersistentMemory/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/Memory/MemoryECC/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>
#include <xyz/openbmc_project/State/ProcessorPerformance/server.hpp>

#include <cstdint>

namespace nsm
{
class NsmPowerControl;
using AcceleratorIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Accelerator>;
using accelaratorType = sdbusplus::xyz::openbmc_project::Inventory::Item::
    server::Accelerator::AcceleratorType;

class NsmAcceleratorIntf : public NsmObject
{
  public:
    NsmAcceleratorIntf(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath);

  private:
    std::unique_ptr<AcceleratorIntf> acceleratorIntf = nullptr;
};

using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

class NsmProcessorAssociation : public NsmObject
{
  public:
    NsmProcessorAssociation(
        sdbusplus::bus::bus& bus, const std::string& name,
        const std::string& type, const std::string& inventoryObjPath,
        const std::vector<utils::Association>& associations);

  private:
    std::unique_ptr<AssociationDefinitionsIntf> associationDef = nullptr;
};

using UuidIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Common::server::UUID>;
class NsmUuidIntf : public NsmObject
{
  public:
    NsmUuidIntf(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
                std::string& inventoryObjPath, uuid_t uuid);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::unique_ptr<UuidIntf> uuidIntf = nullptr;
};

using AssetIntfProcessor = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset>;

template <typename IntfType>
class NsmAssetIntfProcessor : public NsmInterfaceProvider<IntfType>
{
  public:
    NsmAssetIntfProcessor(const std::string& name, const std::string& type,
                          const std::shared_ptr<AssetIntfProcessor> assetIntf) :
        NsmInterfaceProvider<IntfType>(name, type, assetIntf)
    {}
};

using LocationIntfProcessor = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Location>;
using LocationTypes = sdbusplus::xyz::openbmc_project::Inventory::Decorator::
    server::Location::LocationTypes;
class NsmLocationIntfProcessor : public NsmObject
{
  public:
    NsmLocationIntfProcessor(sdbusplus::bus::bus& bus, std::string& name,
                             std::string& type, std::string& inventoryObjPath,
                             std::string& locationType);

  private:
    std::unique_ptr<LocationIntfProcessor> locationIntf;
};

using LocationCodeIntfProcessor =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Decorator::server::LocationCode>;
class NsmLocationCodeIntfProcessor : public NsmObject
{
  public:
    NsmLocationCodeIntfProcessor(sdbusplus::bus::bus& bus, std::string& name,
                                 std::string& type,
                                 std::string& inventoryObjPath,
                                 std::string& locationCode);

  private:
    std::unique_ptr<LocationCodeIntfProcessor> locationCodeIntf;
};

using MigModeIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::MigMode>;

class NsmMigMode : public NsmSensor
{
  public:
    NsmMigMode(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
               std::string& inventoryObjPath,
               std::shared_ptr<NsmDevice> device);
    NsmMigMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield8_t flags);

    std::unique_ptr<NsmMigModeIntf> migModeIntf = nullptr;
};

using EccModeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Memory::server::MemoryECC>;

class NsmEccMode : public NsmObject
{
  public:
    NsmEccMode(std::string& name, std::string& type,
               std::shared_ptr<NsmEccModeIntf> eccIntf);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    void updateReading(bitfield8_t flags);
    std::shared_ptr<NsmEccModeIntf> eccModeIntf = nullptr;
};

class NsmEccErrorCounts : public NsmSensor
{
  public:
    NsmEccErrorCounts(std::string& name, std::string& type,
                      std::shared_ptr<NsmEccModeIntf> eccIntf);
    NsmEccErrorCounts() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(struct nsm_ECC_error_counts);

    std::shared_ptr<NsmEccModeIntf> eccErrorCountIntf = nullptr;
};

using PciePortIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Port>;

class NsmPciePortIntf : public NsmObject
{
  public:
    NsmPciePortIntf(sdbusplus::bus::bus& bus, const std::string& name,
                    const std::string& type, std::string& inventoryObjPath);

  private:
    std::shared_ptr<PciePortIntf> pciePortIntf = nullptr;
};

class NsmPcieGroup : public NsmSensor
{
  public:
    NsmPcieGroup(const std::string& name, const std::string& type,
                 uint8_t deviceId, uint8_t groupId);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

  private:
    uint8_t deviceId;
    uint8_t groupId;
};

using PCieEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;

class NsmPciGroup2 : public NsmPcieGroup
{
  public:
    NsmPciGroup2(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf,
                 std::shared_ptr<PCieEccIntf> pCiePortIntf, uint8_t deviceId);
    NsmPciGroup2() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_2& data);
    std::shared_ptr<PCieEccIntf> pCiePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
};

class NsmPciGroup3 : public NsmPcieGroup
{
  public:
    NsmPciGroup3(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf,
                 std::shared_ptr<PCieEccIntf> pCiePortIntf, uint8_t deviceId);
    NsmPciGroup3() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_3& data);
    std::shared_ptr<PCieEccIntf> pCiePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
};

class NsmPciGroup4 : public NsmPcieGroup
{
  public:
    NsmPciGroup4(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf,
                 std::shared_ptr<PCieEccIntf> pCiePortIntf, uint8_t deviceId);
    NsmPciGroup4() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_4& data);
    std::shared_ptr<PCieEccIntf> pCiePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
};

using EDPpIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::Edpp>;

class EDPpLocal : public EDPpIntf
{
  public:
    EDPpLocal(sdbusplus::bus::bus& bus, const std::string& objPath) :
        EDPpIntf(bus, objPath.c_str(), action::emit_interface_added)
    {}

    void reset()
    {
        return;
    }
};

class NsmEDPpScalingFactor : public NsmSensor
{
  public:
    NsmEDPpScalingFactor(sdbusplus::bus::bus& bus, std::string& name,
                         std::string& type, std::string& inventoryObjPath);
    NsmEDPpScalingFactor() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(struct nsm_EDPp_scaling_factors scaling_factor);

    std::shared_ptr<EDPpLocal> eDPpIntf = nullptr;
};

using CpuOperatingConfigIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Cpu::server::OperatingConfig>;

class NsmClockLimitGraphics : public NsmSensor
{
  public:
    NsmClockLimitGraphics(
        const std::string& name, const std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf);
    NsmClockLimitGraphics() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(const struct nsm_clock_limit&);

    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
};

class NsmCurrClockFreq : public NsmSensor
{
  public:
    NsmCurrClockFreq(const std::string& name, const std::string& type,
                     std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf);
    NsmCurrClockFreq() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(const uint32_t& clockFreq);

    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
};

class NsmMinGraphicsClockLimit : public NsmObject
{
  public:
    NsmMinGraphicsClockLimit(
        std::string& name, std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
};

class NsmMaxGraphicsClockLimit : public NsmObject
{
  public:
    NsmMaxGraphicsClockLimit(
        std::string& name, std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
};

using ProcessorPerformanceIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::server::ProcessorPerformance>;

using ThrottleReasons = sdbusplus::xyz::openbmc_project::State::server::
    ProcessorPerformance::ThrottleReasons;
class NsmProcessorThrottleReason : public NsmSensor
{
  public:
    NsmProcessorThrottleReason(
        std::string& name, std::string& type,
        std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf);
    NsmProcessorThrottleReason() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield32_t flags);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
};

class NsmAccumGpuUtilTime : public NsmSensor
{
  public:
    NsmAccumGpuUtilTime(
        const std::string& name, const std::string& type,
        std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf);
    NsmAccumGpuUtilTime() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(const uint32_t& context_util_time,
                       const uint32_t& SM_util_time);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
};

class NsmPciGroup5 : public NsmPcieGroup
{
  public:
    NsmPciGroup5(const std::string& name, const std::string& type,
                 std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
                 uint8_t deviceId);
    NsmPciGroup5() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_5& data);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
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

using PersistentMemoryInterface = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::PersistentMemory>;

class NsmTotalCacheMemory : public NsmMemoryCapacity
{
  public:
    NsmTotalCacheMemory(
        const std::string& name, const std::string& type,
        std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface);
    NsmTotalCacheMemory() = default;

  private:
    void updateReading(uint32_t* maximumMemoryCapacity) override;
    std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface;
};

using DimmMemoryMetricsIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Dimm::server::MemoryMetrics>;

class NsmMemoryCapacityUtil : public NsmSensor
{
  public:
    NsmMemoryCapacityUtil(sdbusplus::bus::bus& bus, const std::string& name,
                          const std::string& type,
                          std::string& inventoryObjPath,
                          std::shared_ptr<NsmTotalMemory> totalMemory);
    NsmMemoryCapacityUtil() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::shared_ptr<NsmTotalMemory> totalMemory;
    void updateReading(const struct nsm_memory_capacity_utilization& data);
    std::unique_ptr<DimmMemoryMetricsIntf> dimmMemoryMetricsIntf = nullptr;
};

class NsmPowerCap : public NsmSensor
{
  public:
    NsmPowerCap(std::string& name, std::string& type,
                std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                const std::vector<std::string>& parents);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    std::shared_ptr<NsmPowerCapIntf> getPowerCapIntf() const
    {
        return powerCapIntf;
    }

  private:
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    void updateReading(uint32_t value);
    std::vector<std::string> parents;
    std::vector<std::shared_ptr<NsmPowerControl>> sensorCache;
};
using PowerLimitIface = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::PowerLimit>;
class NsmMaxPowerCap : public NsmObject
{
  public:
    NsmMaxPowerCap(std::string& name, std::string& type,
                   std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                   std::shared_ptr<PowerLimitIface> powerLimitIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    std::shared_ptr<NsmPowerCapIntf> getMaxPowerCapIntf() const
    {
        return powerCapIntf;
    }

  private:
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    std::shared_ptr<PowerLimitIface> powerLimitIntf = nullptr;
    void updateValue(uint32_t value);
};

class NsmMinPowerCap : public NsmObject
{
  public:
    NsmMinPowerCap(std::string& name, std::string& type,
                   std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                   std::shared_ptr<PowerLimitIface> powerLimitIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    std::shared_ptr<NsmPowerCapIntf> getMinPowerCapIntf() const
    {
        return powerCapIntf;
    }

  private:
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    std::shared_ptr<PowerLimitIface> powerLimitIntf = nullptr;
    void updateValue(uint32_t value);
};
class NsmDefaultPowerCap : public NsmObject
{
  public:
    NsmDefaultPowerCap(std::string& name, std::string& type,
                       std::shared_ptr<NsmClearPowerCapIntf> powerCapIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    std::shared_ptr<NsmClearPowerCapIntf> getDefaultPowerCapIntf() const
    {
        return clearPowerCapIntf;
    }

  private:
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf = nullptr;
    void updateValue(uint32_t value);
};
using RevisionIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::decorator::Revision>;
class NsmProcessorRevision : public NsmSensor
{
  public:
    NsmProcessorRevision(sdbusplus::bus::bus& bus, const std::string& name,
                         const std::string& type,
                         std::string& inventoryObjPath);
    NsmProcessorRevision() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::unique_ptr<RevisionIntf> revisionIntf = nullptr;
};

using GpuHealthIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::Decorator::server::Health>;
using GpuHealthType = sdbusplus::xyz::openbmc_project::State::Decorator::
    server::Health::HealthType;
class NsmGpuHealth : public NsmObject
{
  public:
    NsmGpuHealth(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
                 std::string& inventoryObjPath);

  private:
    std::shared_ptr<GpuHealthIntf> healthIntf;
};

} // namespace nsm
