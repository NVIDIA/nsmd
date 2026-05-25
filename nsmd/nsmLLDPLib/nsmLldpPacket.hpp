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

#include "network-ports.h"

#include "nsmDevice.hpp"
#include "nsmSensor.hpp"

#include <com/nvidia/Network/LLDP/RawFrame/server.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Network/LLDP/TLVs/server.hpp>

#include <memory>
#include <string>
#include <vector>

namespace nsm
{

/** Per (NetworkAdapter, port, direction) LLDP packet buffer.
 *
 *  Co-hosts two D-Bus interfaces on a single object:
 *    - `com.nvidia.Network.LLDP.RawFrame`
 *    - `xyz.openbmc_project.Network.LLDP.TLVs`
 *
 *  Object path:
 *    /xyz/openbmc_project/inventory/system/<chassis>/<CX_NIC_N>/
 *      Ports/<Port_M>/LLDP/<RX|TX>
 *
 *  Polling: enqueued on the existing `roundRobinSensors` queue — no
 *  separate timer. Each tick issues NSM Type 1 cmd 0x16 GetLLDPPacket
 *  with this sensor's (port, direction) and updates the D-Bus properties
 *  per IEEE 802.1AB §8 TLV decode.
 *
 *  Lifetime: created when the parent NetworkAdapter's LLDP mode goes
 *  Off → non-Off; destroyed when it returns to fully-Off
 *  (discard-on-disable per SADD §3.2).
 */
using LldpRawFrameIntf = sdbusplus::server::object_t<
    sdbusplus::server::com::nvidia::network::lldp::RawFrame>;
using LldpTlvsIntf = sdbusplus::server::object_t<
    sdbusplus::server::xyz::openbmc_project::network::lldp::TLVs>;

class NsmLldpPacket : public NsmSensor
{
  public:
    /** @param bus          - sdbusplus bus
     *  @param name         - sensor name (used in logs and aggregator keys)
     *  @param type         - sensor type tag (per existing nsmd convention)
     *  @param objectPath   - D-Bus object path (must terminate in /RX or /TX)
     *  @param portNumber   - port index on the CX9 NIC (0-based)
     *  @param direction    - NSM_LLDP_DIRECTION_TX or NSM_LLDP_DIRECTION_RX
     */
    NsmLldpPacket(sdbusplus::bus::bus& bus, const std::string& name,
                  const std::string& type, const std::string& objectPath,
                  uint16_t portNumber, uint8_t direction);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override;

    const std::string& dbusObjectPath() const
    {
        return objectPath_;
    }

  private:
    /* Parse a raw LLDP PDU (IEEE 802.1AB §8) and populate the TLVs
     * interface properties. Resets TLV state before decode so omitted
     * optional TLVs are not carried over from the previous frame.
     * Returns false on a malformed frame; the caller clears TLVs again
     * and surfaces the raw bytes via RawFrame.Data per work order §4. */
    bool decodeAndPopulateTlvs(const uint8_t* frame, size_t frameLen);

    /* Reset the TLVs interface properties to their default-empty state. */
    void clearTlvs();

    std::shared_ptr<LldpRawFrameIntf> rawIntf_;
    std::shared_ptr<LldpTlvsIntf> tlvsIntf_;
    std::string objectPath_;
    uint16_t portNumber_;
    uint8_t direction_;
};

} // namespace nsm
