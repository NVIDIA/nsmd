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

#include <com/nvidia/Edpp/server.hpp>
#include <com/nvidia/MigMode/server.hpp>
#include <xyz/openbmc_project/Common/UUID/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Accelerator/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Cpu/OperatingConfig/server.hpp>
#include <xyz/openbmc_project/Memory/MemoryECC/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/State/ProcessorPerformance/server.hpp>

namespace nsm
{

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

using UuidIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Common::server::UUID>;
class NsmUuidIntf : public NsmObject
{
  public:
    NsmUuidIntf(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
                std::string& inventoryObjPath, uuid_t uuid);

  private:
    std::unique_ptr<UuidIntf> uuidIntf = nullptr;
};

using MigModeIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::MigMode>;

class NsmMigMode : public NsmSensor
{
  public:
    NsmMigMode(sdbusplus::bus::bus& bus, std::string& name, std::string& type,
               std::string& inventoryObjPath);
    NsmMigMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield8_t flags);

    std::unique_ptr<MigModeIntf> migModeIntf = nullptr;
};

using EccModeIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Memory::server::MemoryECC>;

class NsmEccMode : public NsmSensor
{
  public:
    NsmEccMode(std::string& name, std::string& type,
               std::shared_ptr<EccModeIntf> eccIntf);
    NsmEccMode() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(bitfield8_t flags);

    std::shared_ptr<EccModeIntf> eccModeIntf = nullptr;
};

class NsmEccErrorCounts : public NsmSensor
{
  public:
    NsmEccErrorCounts(std::string& name, std::string& type,
                      std::shared_ptr<EccModeIntf> eccIntf);
    NsmEccErrorCounts() = default;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    void updateReading(struct nsm_ECC_error_counts);

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
    bool updateStaticProp;

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

using ProcessorPerformanceIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::State::server::ProcessorPerformance>;

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

} // namespace nsm