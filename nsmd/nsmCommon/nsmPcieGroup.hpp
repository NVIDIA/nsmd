/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "libnsm/pci-links.h"

#include "common/types.hpp"
#include "nsmSensor.hpp"

#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>

#include <optional>
#include <vector>

namespace nsm
{
using PCieEccIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;

class NsmPcieGroup : public NsmSensor
{
  public:
    NsmPcieGroup(const std::string& name, const std::string& type,
                 uint8_t deviceId, uint8_t groupId);
    NsmPcieGroup(const std::string& name, const std::string& type,
                 uint8_t groupId, uint8_t multiPortType, uint8_t multiPortIndex,
                 uint8_t multiPortUpstreamPortNumber);
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

  protected:
    // Accessors for derived classes
    uint8_t getMultiPortType() const
    {
        return multiPortType;
    }
    uint8_t getMultiPortIndex() const
    {
        return multiPortIndex;
    }
    uint8_t getMultiPortUpstreamPortNumber() const
    {
        return multiPortUpstreamPortNumber;
    }
    uint8_t getGroupId() const
    {
        return groupId;
    }
    uint8_t getDeviceId() const
    {
        return deviceId;
    }
    bool isMultiPortEnabled() const
    {
        return isMultiPciePortEnabled;
    }

  private:
    std::optional<std::vector<uint8_t>>
        genSinglePortRequestMsg(eid_t eid, uint8_t instanceId);
    std::optional<std::vector<uint8_t>>
        genMultiPortRequestMsg(eid_t eid, uint8_t instanceId);

    bool isMultiPciePortEnabled = false;
    uint8_t multiPortType{};
    uint8_t multiPortIndex{};
    uint8_t multiPortUpstreamPortNumber{};
    uint8_t deviceId{};
    uint8_t groupId{};
};

class NsmPciGroup2 : public NsmPcieGroup
{
  public:
    NsmPciGroup2(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pcieECCIntf,
                 std::shared_ptr<PCieEccIntf> pciePortIntf, uint8_t deviceId,
                 std::string& inventoryObjPath);
    NsmPciGroup2() = default;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_2& data);
    std::shared_ptr<PCieEccIntf> pciePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmPciGroup3 : public NsmPcieGroup
{
  public:
    NsmPciGroup3(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pcieECCIntf,
                 std::shared_ptr<PCieEccIntf> pciePortIntf, uint8_t deviceId,
                 std::string& inventoryObjPath);
    NsmPciGroup3() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_3& data);
    std::shared_ptr<PCieEccIntf> pciePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
    std::string inventoryObjPath;
};

class NsmPciGroup4 : public NsmPcieGroup
{
  public:
    NsmPciGroup4(const std::string& name, const std::string& type,
                 std::shared_ptr<PCieEccIntf> pcieECCIntf,
                 std::shared_ptr<PCieEccIntf> pciePortIntf, uint8_t deviceId,
                 std::string& inventoryObjPath);

    NsmPciGroup4() = default;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;
    void updateMetricOnSharedMemory() override;

  private:
    void updateReading(
        const struct nsm_query_scalar_group_telemetry_group_4& data);
    std::shared_ptr<PCieEccIntf> pciePortIntf = nullptr;
    std::shared_ptr<PCieEccIntf> pCieEccIntf = nullptr;
    std::string inventoryObjPath;
};
} // namespace nsm
