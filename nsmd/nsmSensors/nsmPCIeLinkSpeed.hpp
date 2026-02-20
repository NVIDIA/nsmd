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

#include "libnsm/pci-links.h"

#include "nsmInterface.hpp"
#include "nsmPortInfo.hpp"
#ifdef NVIDIA_SHMEM
#include "sharedMemCommon.hpp"
#endif
#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeSlot/server.hpp>
#include <xyz/openbmc_project/PCIe/PCIeECC/server.hpp>

namespace nsm
{
using sdbusplus::server::object_t;
#ifdef NVIDIA_SHMEM
using namespace nsm_shmem_utils;
#endif

using PCIeDeviceIntf = object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PCIeDevice>;
using PCIeSlotIntf = object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::PCIeSlot>;
using PCIeEccIntf =
    object_t<sdbusplus::xyz::openbmc_project::PCIe::server::PCIeECC>;

class NsmPCIeLinkSpeedBase : public NsmSensor
{
  public:
    NsmPCIeLinkSpeedBase(const NsmObject& provider,
                         uint8_t deviceIndexOrUpstreamPortCount,
                         bool isMultiPciePortEnabled);
    NsmPCIeLinkSpeedBase() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    requester::Coroutine update(std::shared_ptr<NsmDevice> nsmDevice) override;

  protected:
    std::optional<Request> genSinglePCIeRequestMsg(eid_t eid,
                                                   uint8_t instanceId);
    std::optional<Request>
        genMultiPCIeRequestMsg(eid_t eid, uint8_t instanceId,
                               uint8_t multiPortType, uint8_t multiPortIndex,
                               uint8_t multiPortUpstreamPortNumber);

    virtual void handleResponse(
        const nsm_query_scalar_group_telemetry_group_1& data) = 0;
    uint8_t deviceIndex;
    bool isMultiPciePortEnabled = false;
    uint8_t upstreamPortCount;

    static PCIeSlotIntf::Generations generation(uint32_t value);
    static PCIeDeviceIntf::PCIeTypes pcieType(uint32_t value);
    static uint32_t linkWidth(uint32_t value);
};

template <typename IntfType>
class NsmPCIeLinkSpeed :
    public NsmPCIeLinkSpeedBase,
    public NsmInterfaceContainer<IntfType>
{
  private:
    void updateMetricOnSharedMemory() override;

  protected:
    void handleResponse(
        const nsm_query_scalar_group_telemetry_group_1& data) override;

  public:
    NsmPCIeLinkSpeed() = delete;
    NsmPCIeLinkSpeed(const NsmInterfaceProvider<IntfType>& provider,
                     uint8_t deviceIndexOrUpstreamPortCount,
                     bool isMultiPciePortEnabled) :
        NsmPCIeLinkSpeedBase(provider, deviceIndexOrUpstreamPortCount,
                             isMultiPciePortEnabled),
        NsmInterfaceContainer<IntfType>(provider)
    {
        nsm_query_scalar_group_telemetry_group_1 data{
            .negotiated_link_speed = 1, // Gen1
            .negotiated_link_width = 0,
            .target_link_speed = 0,
            .max_link_speed = 1, // Gen1
            .max_link_width = 0,
            .max_read_request_size_bytes = 0,
            .max_payload_size_bytes = 0,
            .clock_mode = 0,
        };
        handleResponse(data);
        updateMetricOnSharedMemory();
    }
    NsmPCIeLinkSpeed(const NsmInterfaceProvider<IntfType>& provider,
                     uint8_t multiPortType, uint8_t multiPortIndex,
                     uint8_t multiPortUpstreamPortNumber) :
        NsmPCIeLinkSpeedBase(provider, multiPortType, multiPortIndex,
                             multiPortUpstreamPortNumber),
        NsmInterfaceContainer<IntfType>(provider)
    {
        nsm_query_scalar_group_telemetry_group_1 data{
            .negotiated_link_speed = 1, // Gen1
            .negotiated_link_width = 0,
            .target_link_speed = 0,
            .max_link_speed = 1, // Gen1
            .max_link_width = 0,
            .max_read_request_size_bytes = 0,
            .max_payload_size_bytes = 0,
            .clock_mode = 0,
        };
        handleResponse(data);
        updateMetricOnSharedMemory();
    }
};

template <>
inline void NsmPCIeLinkSpeed<PCIeDeviceIntf>::updateMetricOnSharedMemory()
{}
template <>
inline void NsmPCIeLinkSpeed<PCIeDeviceIntf>::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    invoke(pdiMethod(pcIeType), pcieType(data.negotiated_link_speed));
    invoke(pdiMethod(generationInUse), generation(data.negotiated_link_speed));
    invoke(pdiMethod(maxPCIeType), pcieType(data.max_link_speed));
    invoke(pdiMethod(lanesInUse), linkWidth(data.negotiated_link_width));
    invoke(pdiMethod(maxLanes), linkWidth(data.max_link_width));
}

template <>
inline void NsmPCIeLinkSpeed<PCIeSlotIntf>::updateMetricOnSharedMemory()
{}
template <>
inline void NsmPCIeLinkSpeed<PCIeSlotIntf>::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    invoke(pdiMethod(generation), generation(data.negotiated_link_speed));
    invoke(pdiMethod(lanes), linkWidth(data.negotiated_link_width));
}

template <>
inline void NsmPCIeLinkSpeed<PCIeEccIntf>::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    invoke([](const auto& path, auto& pdi) {
        std::vector<uint8_t> data;
        SharedMemoryManager::cacheTALData(
            path, pdi.interface, "PCIeType", data,
            PCIeEccIntf::convertPCIeTypesToString(pdi.pcIeType()));
        SharedMemoryManager::cacheTALData(path, pdi.interface, "LanesInUse",
                                          data, pdi.lanesInUse());
        SharedMemoryManager::cacheTALData(path, pdi.interface, "MaxLanes", data,
                                          pdi.maxLanes());
    });
#endif
}
template <>
inline void NsmPCIeLinkSpeed<PCIeEccIntf>::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    invoke(pdiMethod(pcIeType),
           (PCIeEccIntf::PCIeTypes)pcieType(data.negotiated_link_speed));
    invoke(pdiMethod(lanesInUse), linkWidth(data.negotiated_link_width));
    invoke(pdiMethod(maxLanes), linkWidth(data.max_link_width));
}

template <>
inline void NsmPCIeLinkSpeed<NsmPortInfoIntf>::updateMetricOnSharedMemory()
{
#ifdef NVIDIA_SHMEM
    invoke([](const auto& path, auto& pdi) {
        std::vector<uint8_t> data;
        SharedMemoryManager::cacheTALData(path, pdi.PortInfoIntf::interface,
                                          "CurrentSpeedGbps", data,
                                          pdi.currentSpeed());
        SharedMemoryManager::cacheTALData(path, pdi.PortInfoIntf::interface,
                                          "MaxSpeedGbps", data, pdi.maxSpeed());
        SharedMemoryManager::cacheTALData(path, pdi.PortWidth::interface,
                                          "ActiveWidth", data,
                                          pdi.activeWidth());
        SharedMemoryManager::cacheTALData(path, pdi.PortWidth::interface,
                                          "Width", data, pdi.width());
    });
#endif
}
template <>
inline void NsmPCIeLinkSpeed<NsmPortInfoIntf>::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    auto convertToTransferRate = [](uint32_t generation) {
        switch (generation)
        {
            case NSM_PCIE_LINK_SPEED_CODE_GEN1:
                return NSM_PCIE_SPEED_GBPS_GEN1;
            case NSM_PCIE_LINK_SPEED_CODE_GEN2:
                return NSM_PCIE_SPEED_GBPS_GEN2;
            case NSM_PCIE_LINK_SPEED_CODE_GEN3:
                return NSM_PCIE_SPEED_GBPS_GEN3;
            case NSM_PCIE_LINK_SPEED_CODE_GEN4:
                return NSM_PCIE_SPEED_GBPS_GEN4;
            case NSM_PCIE_LINK_SPEED_CODE_GEN5:
                return NSM_PCIE_SPEED_GBPS_GEN5;
            case NSM_PCIE_LINK_SPEED_CODE_GEN6:
                return NSM_PCIE_SPEED_GBPS_GEN6;
            default:
            {
                lg2::debug(
                    "NsmPCIeLinkSpeed<NsmPortInfoIntf>::handleResponse: unknown generation {GENERATION}",
                    "GENERATION", generation);
                return 0.0;
            }
        }
    };
    invoke(pdiMethod(currentSpeed),
           convertToTransferRate(data.negotiated_link_speed));
    invoke(pdiMethod(maxSpeed), convertToTransferRate(data.max_link_speed));
    invoke(pdiMethod(activeWidth), linkWidth(data.negotiated_link_width));
    invoke(pdiMethod(width), linkWidth(data.max_link_width));
}

} // namespace nsm
