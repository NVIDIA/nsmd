/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
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

#include "device-configuration.h"

#include "asyncOperationManager.hpp"
#include "nsmDevice.hpp"
#include "nsmSensor.hpp"

#include <com/nvidia/Network/LLDP/Modes/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <memory>
#include <string>

namespace nsm
{

/** Per-NetworkAdapter `com.nvidia.Network.LLDP.Modes` D-Bus object.
 *
 *  Owns the cold-start GetDeviceModeSettings v2 read of NSM Type 5 Device
 *  Mode Index 24 (NVBug 6136040) and the PropertiesChanged → SetDeviceMode
 *  Settings v2 write flow. The DCBX prerequisite check (DCBXMode = Enabled
 *  requires TXMode == RXMode == All) is enforced inside this class BEFORE
 *  any MCTP I/O — see DCBX_PRECHECK in nsmLldpMode.cpp.
 *
 *  Object path: <NetworkAdapter>/Settings/Oem/Nvidia/LLDPModes
 *
 *  Association: ("network_adapter", "lldp_mode_settings", <NetworkAdapter
 *  path>) — lets bmcweb discover LLDP mode applicability via ObjectMapper.
 *
 *  Lifetime: created when entity-manager marks DeviceModesSupported idx 24
 *  on the parent NSM_NetworkAdapter; destroyed when the endpoint is lost.
 */
using LLDPModesIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::network::lldp::Modes,
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;

class NsmLldpMode : public NsmSensor
{
  public:
    using LLDPModeType =
        sdbusplus::server::com::nvidia::network::lldp::Modes::LLDPModeType;
    using DCBXModeType =
        sdbusplus::server::com::nvidia::network::lldp::Modes::DCBXModeType;

    NsmLldpMode(sdbusplus::bus_t& bus, const std::string& name,
                const std::string& type, const std::string& objectPath,
                const std::string& networkAdapterPath,
                std::shared_ptr<NsmDevice> device);

    /* NsmSensor — drives the cold-start GetDeviceModeSettings read. */
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

    /* Property setter, hooked via AsyncOperationManager. Runs the DCBX
     * precheck, encodes Type 5 SetDeviceModeSettings v2, sends, and on
     * CC=0 updates the local property and fires the transition callback. */
    requester::Coroutine setTxMode(const AsyncSetOperationValueType& value,
                                   AsyncOperationStatusType* status,
                                   std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine setRxMode(const AsyncSetOperationValueType& value,
                                   AsyncOperationStatusType* status,
                                   std::shared_ptr<NsmDevice> nsmDevice);
    requester::Coroutine setDcbxMode(const AsyncSetOperationValueType& value,
                                     AsyncOperationStatusType* status,
                                     std::shared_ptr<NsmDevice> nsmDevice);

    LLDPModeType txMode() const;
    LLDPModeType rxMode() const;
    DCBXModeType dcbxMode() const;

    const std::string& dbusObjectPath() const
    {
        return objectPath_;
    }

  private:
    static bool dcbxPrecheck(LLDPModeType tx, LLDPModeType rx,
                             DCBXModeType dcbx);

    requester::Coroutine applyBitfield(LLDPModeType tx, LLDPModeType rx,
                                       DCBXModeType dcbx,
                                       AsyncOperationStatusType* status,
                                       std::shared_ptr<NsmDevice> nsmDevice);

    std::shared_ptr<LLDPModesIntf> intf_;
    std::string objectPath_;
    std::shared_ptr<NsmDevice> device_;
    bool asyncPatchInProgress_{false};
};

} // namespace nsm
