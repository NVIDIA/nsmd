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
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmChassis/nsmPowerControl.hpp"
#include "nsmClearPowerCapIface.hpp"
#include "nsmCommon/nsmCommon.hpp"
#include "nsmCommon/sharedMemCommon.hpp"
#include "nsmInterface.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPowerCapIface.hpp"
#include "nsmPowerSmoothing.hpp"
#include "nsmPowerSmoothingCurrentProfileIface.hpp"
#include "nsmPowerSmoothingFeatureIntf.hpp"
#include "nsmResetEdppAsyncIface.hpp"
#include "nsmSensor.hpp"
#include "nsmSetEgmMode.hpp"
#include "nsmSetMigMode.hpp"
#include "nsmWorkloadPowerProfile.hpp"

#include <stdint.h>

#include <com/nvidia/Edpp/server.hpp>
#include <com/nvidia/EgmMode/server.hpp>
#include <com/nvidia/MigMode/server.hpp>
#include <com/nvidia/NVLink/NvLinkTotalCount/server.hpp>
#include <com/nvidia/ResetCounters/ResetCounterMetricsSupported/server.hpp>
#include <com/nvidia/SMUtilization/server.hpp>
#include <tal.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Control/Power/Persistency/server.hpp>
#ifdef ENABLE_SYSTEM_GUID
#include <com/nvidia/SysGUID/SysGUID/server.hpp>
#endif
#include <com/nvidia/CCMode/server.hpp>
#include <com/nvidia/ResetCounters/ResetCounterMetrics/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Location/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PowerLimit/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Revision/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Cpu/OperatingConfig/server.hpp>
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

#ifdef NVIDIA_RESET_METRICS
using resetMetricsSupported =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::ResetCounters::server::
                                    ResetCounterMetricsSupported>;
#endif

class NsmAcceleratorIntf : public NsmObject
{
  public:
    NsmAcceleratorIntf(sdbusplus::bus::bus& bus, std::string& name,
                       std::string& type, std::string& inventoryObjPath);

  private:
    std::unique_ptr<AcceleratorIntf> acceleratorIntf = nullptr;
};

#ifdef NVIDIA_RESET_METRICS
class NsmResetCountersSupportedIntf : public NsmObject
{
  public:
    NsmResetCountersSupportedIntf(sdbusplus::bus::bus& bus, std::string& name,
                                  std::string& type,
                                  std::string& inventoryObjPath);

  private:
    std::unique_ptr<resetMetricsSupported> resetMetricsSupportedIntf = nullptr;
};
#endif

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
    void updateMetricOnSharedMemory() override;

  private:
    std::unique_ptr<UuidIntf> uuidIntf = nullptr;
    std::string inventoryObjPath;
};

#ifdef ENABLE_SYSTEM_GUID
using SysGuidIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::SysGUID::server::SysGUID>;
class NsmSysGuidIntf : public NsmObject
{
  public:
    NsmSysGuidIntf(sdbusplus::bus::bus& bus, std::string& name,
                   std::string& type, std::string& inventoryObjPath);

    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::unique_ptr<SysGuidIntf> sysguidIntf = nullptr;
    std::string inventoryObjPath;

    // SysGUID is used externally to identify which GPU's
    // are in the same tray.
    // So sysGUID is static (each GPU in a chassis will
    // share the same ID)
    static uint8_t sysGUID[];
    static bool sysGuidGenerated;
};
#endif

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
               std::string& inventoryObjPath, std::shared_ptr<NsmDevice> device,
               bool isLongRunning);
    NsmMigMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(bitfield8_t flags);
    std::string inventoryObjPath;
    std::unique_ptr<MigModeIntf> migModeIntf = nullptr;
};

using EccModeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Memory::server::MemoryECC>;
class NsmEccMode : public NsmSensor
{
  public:
    NsmEccMode(std::string& name, std::string& type,
               std::shared_ptr<EccModeIntf> eccIntf,
               std::string& inventoryObjPath, bool isLongRunning);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(bitfield8_t flags);
    std::shared_ptr<EccModeIntf> eccModeIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmEccErrorCounts : public NsmSensor
{
  public:
    NsmEccErrorCounts(std::string& name, std::string& type,
                      std::shared_ptr<EccModeIntf> eccIntf,
                      std::string& inventoryObjPath);
    NsmEccErrorCounts() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(struct nsm_ECC_error_counts);
    std::string inventoryObjPath;

    std::shared_ptr<EccModeIntf> eccErrorCountIntf = nullptr;
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
                 std::shared_ptr<PCieEccIntf> pCiePortIntf, uint8_t deviceId,
                 std::string& inventoryObjPath);
    NsmPciGroup2() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_2& data);
    std::shared_ptr<PCieEccIntf> pCiePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmPciGroup3 : public NsmPcieGroup
{
  public:
    NsmPciGroup3(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf,
                 std::shared_ptr<PCieEccIntf> pCiePortIntf, uint8_t deviceId,
                 std::string& inventoryObjPath);
    NsmPciGroup3() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_3& data);
    std::shared_ptr<PCieEccIntf> pCiePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmPciGroup4 : public NsmPcieGroup
{
  public:
    NsmPciGroup4(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pCieECCIntf,
                 std::shared_ptr<PCieEccIntf> pCiePortIntf, uint8_t deviceId,
                 std::string& inventoryObjPath);

    NsmPciGroup4() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_4& data);
    std::shared_ptr<PCieEccIntf> pCiePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
    std::string inventoryObjPath;
};

using EDPpIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::Edpp>;

class EDPpLocal : public EDPpIntf
{
  public:
    EDPpLocal(sdbusplus::bus::bus& bus, const std::string& objPath) :
        EDPpIntf(bus, objPath.c_str(), action::emit_interface_added)
    {}

    void reset() {
    } // reset method defined in the pdi, so overloading it. It's not in use,
      // since we shifted to async methods for post operation
};

class NsmEDPpScalingFactor : public NsmSensor
{
  public:
    NsmEDPpScalingFactor(
        std::string& name, std::string& type, std::string& inventoryObjPath,
        std::shared_ptr<EDPpLocal> eDPpIntf,
        std::shared_ptr<NsmResetEdppAsyncIntf> resetEdppAsyncIntf);
    NsmEDPpScalingFactor() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

    requester::Coroutine
        patchSetPoint(const AsyncSetOperationValueType& value,
                      [[maybe_unused]] AsyncOperationStatusType* status,
                      std::shared_ptr<NsmDevice> device);

  private:
    void updateReading(struct nsm_EDPp_scaling_factors scaling_factor);
    bool persistence;
    std::shared_ptr<EDPpLocal> eDPpIntf = nullptr;
    std::shared_ptr<NsmResetEdppAsyncIntf> resetEdppAsyncIntf;
    std::string inventoryObjPath;
};

class NsmMaxEDPpLimit : public NsmObject
{
  public:
    NsmMaxEDPpLimit(std::string& name, std::string& type,
                    std::shared_ptr<EDPpLocal> eDPpIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<EDPpLocal> eDPpIntf = nullptr;
};

class NsmMinEDPpLimit : public NsmObject
{
  public:
    NsmMinEDPpLimit(std::string& name, std::string& type,
                    std::shared_ptr<EDPpLocal> eDPpIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<EDPpLocal> eDPpIntf = nullptr;
};

using CpuOperatingConfigIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Cpu::server::OperatingConfig>;
using SMUtilizationIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::SMUtilization>;

class NsmClockLimitGraphics : public NsmSensor
{
  public:
    NsmClockLimitGraphics(const std::string& name, const std::string& type,
                          std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
                          std::string& inventoryObjPath);
    NsmClockLimitGraphics() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(const struct nsm_clock_limit&);

    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmCurrClockFreq : public NsmSensor
{
  public:
    NsmCurrClockFreq(const std::string& name, const std::string& type,
                     std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
                     std::string& inventoryObjPath);
    NsmCurrClockFreq() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(const uint32_t& clockFreq);

    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmDefaultBaseClockSpeed : public NsmObject
{
  public:
    NsmDefaultBaseClockSpeed(
        std::string& name, std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
};

class NsmDefaultBoostClockSpeed : public NsmObject
{
  public:
    NsmDefaultBoostClockSpeed(
        std::string& name, std::string& type,
        std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf = nullptr;
};

class NsmCurrentUtilization : public NsmSensor
{
  public:
    NsmCurrentUtilization(const std::string& name, const std::string& type,
                          std::shared_ptr<CpuOperatingConfigIntf> cpuConfigIntf,
                          std::shared_ptr<SMUtilizationIntf> smUtilizationIntf,
                          std::string& inventoryObjPath, bool isLongRunning);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    std::shared_ptr<CpuOperatingConfigIntf> cpuOperatingConfigIntf{nullptr};
    std::shared_ptr<SMUtilizationIntf> smUtilizationIntf{nullptr};

    std::string inventoryObjPath;
    std::string smUtilizationIntfName;
    std::string smUtilizationPropertyName;
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
        std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
        std::string& inventoryObjPath);
    NsmProcessorThrottleReason() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(bitfield32_t flags);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
    std::string inventoryObjPath;
};

class NsmAccumGpuUtilTime : public NsmSensor
{
  public:
    NsmAccumGpuUtilTime(
        const std::string& name, const std::string& type,
        std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
        std::string& inventoryObjPath);
    NsmAccumGpuUtilTime() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(const uint32_t& context_util_time,
                       const uint32_t& SM_util_time);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
    std::string inventoryObjPath;
};

class NsmPciGroup5 : public NsmPcieGroup
{
  public:
    NsmPciGroup5(const std::string& name, const std::string& type,
                 std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
                 uint8_t deviceId, std::string& inventoryObjPath);
    NsmPciGroup5() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_5& data);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
    std::string inventoryObjPath;
};

using PersistentMemoryInterface = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::inventory::item::PersistentMemory>;

class NsmTotalMemorySize : public NsmObject
{
  public:
    NsmTotalMemorySize(
        std::string& name, std::string& type,
        std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;

  private:
    std::shared_ptr<PersistentMemoryInterface> persistentMemoryInterface;
};

using TotalNvLinkInterface = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::nv_link::NvLinkTotalCount>;
class NsmTotalNvLinks : public NsmSensor
{
  public:
    NsmTotalNvLinks(const std::string& name, const std::string& type,
                    std::shared_ptr<TotalNvLinkInterface> totalNvLinkInterface,
                    std::string& inventoryObjPath);
    NsmTotalNvLinks() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    void updateMetricOnSharedMemory() override;

  private:
    std::shared_ptr<TotalNvLinkInterface> totalNvLinkInterface;
    std::string inventoryObjPath;
};

using DimmMemoryMetricsIntf =
    sdbusplus::server::object_t<sdbusplus::xyz::openbmc_project::Inventory::
                                    Item::Dimm::server::MemoryMetrics>;

using PowerPersistencyIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::control::power::Persistency>;
class NsmPowerCap : public NsmSensor
{
  public:
    NsmPowerCap(std::string& name, std::string& type,
                std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                const std::vector<std::string>& parents,
                const std::shared_ptr<PowerPersistencyIntf> persistencyIntf,
                std::string& inventoryObjPath);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

    std::shared_ptr<NsmPowerCapIntf> getPowerCapIntf() const
    {
        return powerCapIntf;
    }

  private:
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    void updateReading(uint32_t value);
    std::vector<std::string> parents;
    std::shared_ptr<PowerPersistencyIntf> persistencyIntf;
    std::vector<std::shared_ptr<NsmPowerControl>> sensorCache;
    std::string inventoryObjPath;
};
using PowerLimitIface = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::PowerLimit>;
class NsmMaxPowerCap : public NsmObject
{
  public:
    NsmMaxPowerCap(std::string& name, std::string& type,
                   std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                   std::shared_ptr<PowerLimitIface> powerLimitIntf,
                   std::string& inventoryObjPath);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    void updateMetricOnSharedMemory() override;
    std::shared_ptr<NsmPowerCapIntf> getMaxPowerCapIntf() const
    {
        return powerCapIntf;
    }

  private:
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    std::shared_ptr<PowerLimitIface> powerLimitIntf = nullptr;
    void updateValue(uint32_t value);
    std::string inventoryObjPath;
};

class NsmMinPowerCap : public NsmObject
{
  public:
    NsmMinPowerCap(std::string& name, std::string& type,
                   std::shared_ptr<NsmPowerCapIntf> powerCapIntf,
                   std::shared_ptr<PowerLimitIface> powerLimitIntf,
                   std::string& inventoryObjPath);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    void updateMetricOnSharedMemory() override;
    std::shared_ptr<NsmPowerCapIntf> getMinPowerCapIntf() const
    {
        return powerCapIntf;
    }

  private:
    std::shared_ptr<NsmPowerCapIntf> powerCapIntf = nullptr;
    std::shared_ptr<PowerLimitIface> powerLimitIntf = nullptr;
    void updateValue(uint32_t value);
    std::string inventoryObjPath;
};
class NsmDefaultPowerCap : public NsmObject
{
  public:
    NsmDefaultPowerCap(
        std::string& name, std::string& type,
        std::shared_ptr<NsmClearPowerCapIntf> powerCapIntf,
        std::shared_ptr<NsmClearPowerCapAsyncIntf> clearPowerCapAsyncIntf);
    requester::Coroutine update(SensorManager& manager, eid_t eid) override;
    std::shared_ptr<NsmClearPowerCapIntf> getDefaultPowerCapIntf() const
    {
        return defaultPowerCapIntf;
    }
    std::shared_ptr<NsmClearPowerCapAsyncIntf> getClearPowerCapAsyncIntf() const
    {
        return clearPowerCapAsyncIntf;
    }

  private:
    std::shared_ptr<NsmClearPowerCapIntf> defaultPowerCapIntf = nullptr;
    std::shared_ptr<NsmClearPowerCapAsyncIntf> clearPowerCapAsyncIntf = nullptr;
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
    void updateMetricOnSharedMemory() override;

  private:
    std::unique_ptr<RevisionIntf> revisionIntf = nullptr;
    std::string inventoryObjPath;
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

class NsmProcessorThrottleDuration : public NsmSensor
{
  public:
    NsmProcessorThrottleDuration(
        std::string& name, std::string& type,
        std::shared_ptr<ProcessorPerformanceIntf> processorPerfIntf,
        std::string& inventoryObjPath, bool isLongRunning);
    NsmProcessorThrottleDuration() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(const nsm_violation_duration& data);
    std::shared_ptr<ProcessorPerformanceIntf> processorPerformanceIntf =
        nullptr;
    std::string inventoryObjPath;
};

using ConfidentialComputeIntf =
    sdbusplus::server::object_t<sdbusplus::server::com::nvidia::CCMode>;
class NsmConfidentialCompute : public NsmSensor
{
  public:
    NsmConfidentialCompute(
        const std::string& name, const std::string& type,
        std::shared_ptr<ConfidentialComputeIntf> confidentialComputeIntf,
        std::string& inventoryObjPath);
    NsmConfidentialCompute() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;
    requester::Coroutine
        patchCCMode(const AsyncSetOperationValueType& value,
                    [[maybe_unused]] AsyncOperationStatusType* status,
                    std::shared_ptr<NsmDevice> device);
    requester::Coroutine
        patchCCDevMode(const AsyncSetOperationValueType& value,
                       [[maybe_unused]] AsyncOperationStatusType* status,
                       std::shared_ptr<NsmDevice> device);

  private:
    void updateReading(uint8_t current_mode, uint8_t pending_mode);
    std::shared_ptr<ConfidentialComputeIntf> confidentialComputeIntf = nullptr;
    std::string inventoryObjPath;
};

using EgmModeIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::EgmMode>;

class NsmEgmMode : public NsmSensor
{
  public:
    NsmEgmMode(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
               std::string& inventoryObjPath, std::shared_ptr<NsmDevice> device,
               bool isLongRunning);
    NsmEgmMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(bitfield8_t flags);
    std::unique_ptr<EgmModeIntf> egmModeIntf = nullptr;
    std::string inventoryObjPath;
};

} // namespace nsm
