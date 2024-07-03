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

#include "platform-environmental.h"

#include "nsmInterface.hpp"

#include <com/nvidia/NVLink/NVLinkRefClock/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PCIeRefClock/server.hpp>

namespace nsm
{
using namespace sdbusplus::xyz::openbmc_project;
using namespace sdbusplus::com::nvidia;
using namespace sdbusplus::server;

using NVLinkRefClockIntf = object_t<NVLink::server::NVLinkRefClock>;
using PCIeRefClockIntf = object_t<Inventory::Decorator::server::PCIeRefClock>;
class NsmClockOutputEnableStateBase : public NsmSensor
{
  private:
    const clock_output_enable_state_index bufferIndex;
    NsmDeviceIdentification deviceType;
    uint8_t instanceNumber;
    const bool retimer;

  public:
    NsmClockOutputEnableStateBase(const NsmObject& provider,
                                  clock_output_enable_state_index bufferIndex,
                                  NsmDeviceIdentification deviceType,
                                  uint8_t instanceNumber, bool retimer = false);
    NsmClockOutputEnableStateBase() = delete;

    std::optional<Request> genRequestMsg(eid_t eid,
                                         uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  protected:
    virtual void handleResponse(const uint32_t& data) = 0;
    bool getPCIeClockBufferData(const nsm_pcie_clock_buffer_data& data) const;
    bool getNVHSClockBufferData(const nsm_nvhs_clock_buffer_data& data) const;
};

template <typename IntfType>
class NsmClockOutputEnableState :
    public NsmClockOutputEnableStateBase,
    public NsmInterfaceContainer<IntfType>
{
  protected:
    void handleResponse(const uint32_t& data) override;

  public:
    NsmClockOutputEnableState() = delete;
    NsmClockOutputEnableState(const NsmInterfaceProvider<IntfType>& provider,
                              clock_output_enable_state_index bufferIndex,
                              NsmDeviceIdentification deviceType,
                              uint8_t instanceNumber, bool retimer = false) :
        NsmClockOutputEnableStateBase(provider, bufferIndex, deviceType,
                                      instanceNumber, retimer),
        NsmInterfaceContainer<IntfType>(provider)
    {}
};

template <>
inline void NsmClockOutputEnableState<NVLinkRefClockIntf>::handleResponse(
    const uint32_t& data)
{
    pdi().nvLinkReferenceClockEnabled(getNVHSClockBufferData(
        reinterpret_cast<const nsm_nvhs_clock_buffer_data&>(data)));
}

template <>
inline void NsmClockOutputEnableState<PCIeRefClockIntf>::handleResponse(
    const uint32_t& data)
{
    pdi().pcIeReferenceClockEnabled(getPCIeClockBufferData(
        reinterpret_cast<const nsm_pcie_clock_buffer_data&>(data)));
}
} // namespace nsm
