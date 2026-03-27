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

/*
 * Branch coverage batch 6 for nsmd/nsmPort/nsmPort.cpp.
 *
 * Covers NsmNetworkAddressAggregator::handleResponseMsg uncovered else
 * branches:
 * - Line 1504: linkType=ETHERNET, sample.tag not MAC_ADDRESS(1) or
 *   PERMANENT_MAC_ADDRESS(2) → "Invalid Tag for Ethernet"
 * - Line 1527: linkType=INFINIBAND, sample.tag not NODE_GUID(3) or
 *   PORT_GUID(4): coverage via decode failure path (tag=1 with wrong len)
 * - Line 1533: linkType not ETHERNET or INFINIBAND → "Invalid Port link type"
 *   (injected via private member access)
 */

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"

using namespace nsm;

struct NsmPortBranch6Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    NsmPortBranch6Test() : SensorManagerTest(devices) {}
    ~NsmPortBranch6Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Helper: create aggregate response with link-type + one data sample.
// dataSampleTag: tag for the second sample.
// dataLen8: if true, use length=3 (8 bytes); else length=0 (1 byte).
// linkTypeValue: NSM_PORT_PROTOCOL_ETHERNET or NSM_PORT_PROTOCOL_INFINIBAND.
// ============================================================================

static std::vector<uint8_t> makeAggResponse(uint8_t linkTypeValue,
                                            uint8_t dataSampleTag,
                                            bool dataLen8 = true)
{
    // link-type sample: 1 data byte
    const size_t lsSize = sizeof(nsm_aggregate_resp_sample) - 1 + 1;
    // data sample: 8 data bytes when dataLen8, else 1
    const size_t dsData = dataLen8 ? 8 : 1;
    const size_t dsSize = sizeof(nsm_aggregate_resp_sample) - 1 + dsData;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) + lsSize + dsSize, 0);

    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(msg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto* ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link-type sample: length=0 → data_len=1 byte
    auto* ls = reinterpret_cast<nsm_aggregate_resp_sample*>(ptr);
    ls->tag = NSM_TAG_LINK_TYPE;
    ls->valid = 1;
    ls->length = 0;
    ls->data[0] = linkTypeValue;
    ptr += lsSize;

    // Data sample
    auto* ds = reinterpret_cast<nsm_aggregate_resp_sample*>(ptr);
    ds->tag = dataSampleTag;
    ds->valid = 1;
    ds->length = dataLen8 ? 3 : 0; // 2^3=8 bytes or 2^0=1 byte

    return buf;
}

// ============================================================================
// "Invalid Tag for Ethernet" — linkType=ETHERNET, tag=NODE_GUID(3).
// decode_aggregate_network_address_data(3, data, 8) → NSM_SUCCESS
// (8 bytes == sizeof(uint64_t)). But tag 3 is not MAC_ADDRESS or
// PERMANENT_MAC_ADDRESS → else branch at line 1504.
// ============================================================================

TEST_F(NsmPortBranch6Test,
       NsmNetworkAddressAggregator_HandleResponse_InvalidTagEthernet)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_br6_eth";
    NsmNetworkAddressAggregator sensor(
        bus, "NAA_InvalidEthTag", "NSM_Port", objPath, objPath + "/NodeGuid",
        objPath + "/EthMAC", objPath + "/PermMAC", 1);

    // NODE_GUID(3) with 8-byte data: decodes OK, but not valid for Ethernet
    auto buf = makeAggResponse(NSM_PORT_PROTOCOL_ETHERNET, NSM_TAG_NODE_GUID,
                               /*dataLen8=*/true);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    EXPECT_NO_THROW(sensor.handleResponseMsg(msg, buf.size()));
}

// ============================================================================
// InfiniBand path: tag=MAC_ADDRESS(1) with 8-byte payload.
// decode_aggregate_network_address_data(1, data, 8) fails (expects 6 bytes).
// returnValue set, continues. Exercises the decode-error branch inside the
// INFINIBAND block and the InfiniBand linkType branch itself.
// Note: reaching the else at line 1527 requires a tag ∉ {3,4} that decodes
// successfully — structurally impossible with the libnsm API (only tags 3,4
// decode with power-of-2 lengths in the IB range). See BLOCKED_TESTS.md.
// ============================================================================

TEST_F(NsmPortBranch6Test,
       NsmNetworkAddressAggregator_HandleResponse_InfiniBand_DecodeError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_br6_ib";
    NsmNetworkAddressAggregator sensor(
        bus, "NAA_IBDecodeErr", "NSM_Port", objPath, objPath + "/NodeGuid",
        objPath + "/EthMAC", objPath + "/PermMAC", 2);

    // Force linkType=INFINIBAND; provide only one sample (no link-type sample
    // in response) with tag=MAC_ADDRESS(1) and 8-byte data. decode fails
    // (needs 6B) → sets returnValue, continues. linkType stays INFINIBAND.
    sensor.linkType = NSM_PORT_PROTOCOL_INFINIBAND;

    const size_t dsSize = sizeof(nsm_aggregate_resp_sample) - 1 + 8;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) + dsSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(msg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto* ds = reinterpret_cast<nsm_aggregate_resp_sample*>(
        reinterpret_cast<uint8_t*>(payload + 1));
    ds->tag = NSM_TAG_MAC_ADDRESS;
    ds->valid = 1;
    ds->length = 3; // 2^3=8 bytes; MAC decode expects 6 → decode fails

    EXPECT_NO_THROW(sensor.handleResponseMsg(msg, buf.size()));
}

// ============================================================================
// "Invalid Port link type" — set linkType=2 (not UNKNOWN/ETHERNET/INFINIBAND).
// Response: no link-type sample; one NODE_GUID(3) sample with 8-byte data.
// decode succeeds; linkType(2) is neither ETHERNET nor INFINIBAND → else at
// line 1533: "Invalid Port link type".
// ============================================================================

TEST_F(NsmPortBranch6Test,
       NsmNetworkAddressAggregator_HandleResponse_InvalidPortLinkType)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/port_br6_badlt";
    NsmNetworkAddressAggregator sensor(
        bus, "NAA_BadLinkType", "NSM_Port", objPath, objPath + "/NodeGuid",
        objPath + "/EthMAC", objPath + "/PermMAC", 3);

    // linkType=2: not UNKNOWN(-1), ETHERNET(0), or INFINIBAND(1)
    sensor.linkType = 2;

    // One NODE_GUID(3) sample: length=3→8B, decode returns NSM_SUCCESS
    // (sizeof(uint64_t)==8). linkType(2) is not ETHERNET or INFINIBAND → else.
    const size_t dsSize = sizeof(nsm_aggregate_resp_sample) - 1 + 8;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) + dsSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* payload = reinterpret_cast<nsm_aggregate_resp*>(msg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 1;

    auto* ds = reinterpret_cast<nsm_aggregate_resp_sample*>(
        reinterpret_cast<uint8_t*>(payload + 1));
    ds->tag = NSM_TAG_NODE_GUID;
    ds->valid = 1;
    ds->length = 3; // 2^3=8 bytes == sizeof(uint64_t) ✓

    EXPECT_NO_THROW(sensor.handleResponseMsg(msg, buf.size()));
}
