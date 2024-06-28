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

#include <xyz/openbmc_project/Inventory/Item/PCIeDevice/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/PCIeSlot/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::server;

using PCIeDeviceIntf = object_t<Inventory::Item::server::PCIeDevice>;
using PCIeSlotIntf = object_t<Inventory::Item::server::PCIeSlot>;
class NsmPCIeLinkSpeedBase : public NsmSensor
{
  public:
    NsmPCIeLinkSpeedBase(const NsmObject& provider, uint8_t deviceIndex);
    NsmPCIeLinkSpeedBase() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  protected:
    virtual void handleResponse(
        const nsm_query_scalar_group_telemetry_group_1& data) = 0;
    const uint8_t deviceIndex;

    static PCIeSlotIntf::Generations generation(uint32_t value);
    static PCIeDeviceIntf::PCIeTypes pcieType(uint32_t value);
    static uint32_t linkWidth(uint32_t value);
};

template <typename IntfType>
class NsmPCIeLinkSpeed :
    public NsmPCIeLinkSpeedBase,
    public NsmInterfaceContainer<IntfType>
{
  protected:
    void handleResponse(
        const nsm_query_scalar_group_telemetry_group_1& data) override;

  public:
    NsmPCIeLinkSpeed() = delete;
    NsmPCIeLinkSpeed(const NsmInterfaceProvider<IntfType>& provider,
                     uint8_t deviceIndex) :
        NsmPCIeLinkSpeedBase(provider, deviceIndex),
        NsmInterfaceContainer<IntfType>(provider)
    {}
};

template <>
inline void NsmPCIeLinkSpeed<PCIeDeviceIntf>::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    pdi().pcIeType(pcieType(data.negotiated_link_speed));
    pdi().generationInUse(generation(data.negotiated_link_speed));
    pdi().maxPCIeType(pcieType(data.max_link_speed));
    pdi().lanesInUse(linkWidth(data.negotiated_link_width));
    pdi().maxLanes(linkWidth(data.max_link_width));
}

template <>
inline void NsmPCIeLinkSpeed<PCIeSlotIntf>::handleResponse(
    const nsm_query_scalar_group_telemetry_group_1& data)
{
    pdi().generation(generation(data.negotiated_link_speed));
    pdi().lanes(linkWidth(data.negotiated_link_width));
}

} // namespace nsm
