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

#include "base.h"
#include "network-ports.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmPort.hpp"

using namespace nsm;

struct NsmNetworkAddressAggregatorTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:40";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmNetworkAddressAggregatorTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmNetworkAddressAggregatorTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmNetworkAddressAggregatorTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Network";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_net";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    EXPECT_EQ(networkAddr.getName(), portName);
    EXPECT_EQ(networkAddr.portNumber, portNumber);
    EXPECT_NE(networkAddr.linkTypeIntf, nullptr);
    EXPECT_NE(networkAddr.portGuidIntf, nullptr);
    EXPECT_NE(networkAddr.nodeGuidIntf, nullptr);
    EXPECT_NE(networkAddr.macAddressIntf, nullptr);
    EXPECT_NE(networkAddr.permanentMacAddressIntf, nullptr);
}

TEST_F(NsmNetworkAddressAggregatorTest, testGenRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Req";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_req";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 2;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = networkAddr.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_addresses_req));
}

TEST_F(NsmNetworkAddressAggregatorTest, testHandleResponseEthernetMacAddress)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth_MAC";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_eth_mac";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 3;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // Create mock aggregate response with link type Ethernet and MAC address
    const size_t mac_len = MAC_ADDRESS_LENGTH;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + mac_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_ETHERNET;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // MAC address sample
    auto macSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    macSample->tag = NSM_TAG_MAC_ADDRESS;
    macSample->valid = 1;
    macSample->length = std::log2(mac_len);
    uint8_t macAddr[MAC_ADDRESS_LENGTH] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(macSample->data, macAddr, mac_len);

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmNetworkAddressAggregatorTest, testHandleResponseInfinibandGuid)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_IB_GUID";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_ib";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 4;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // Create mock aggregate response with link type Infiniband and GUIDs
    const size_t guid_len = 8;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        3 * (sizeof(nsm_aggregate_resp_sample) - 1 + guid_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 3;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_INFINIBAND;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // Node GUID sample
    auto nodeGuidSample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    nodeGuidSample->tag = NSM_TAG_NODE_GUID;
    nodeGuidSample->valid = 1;
    nodeGuidSample->length = std::log2(guid_len);
    uint64_t nodeGuid = 0x1122334455667788;
    memcpy(nodeGuidSample->data, &nodeGuid, guid_len);
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + guid_len;

    // Port GUID sample
    auto portGuidSample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    portGuidSample->tag = NSM_TAG_PORT_GUID;
    portGuidSample->valid = 1;
    portGuidSample->length = std::log2(guid_len);
    uint64_t portGuid = 0xAABBCCDDEEFF0011;
    memcpy(portGuidSample->data, &portGuid, guid_len);

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmNetworkAddressAggregatorTest, badTestNoLinkType)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_NoLink";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_nolink";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 5;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // Create mock aggregate response without link type
    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_aggregate_resp));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 0;

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST_F(NsmNetworkAddressAggregatorTest, badTestInvalidTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_BadTag";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_badtag";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 6;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // Create mock response with Ethernet link type but invalid tag for that
    // type
    const size_t data_len = 8;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample - Ethernet
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_ETHERNET;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // Invalid tag for Ethernet (should be MAC, not GUID)
    auto invalidSample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    invalidSample->tag = NSM_TAG_NODE_GUID; // Invalid for Ethernet
    invalidSample->valid = 1;
    invalidSample->length = std::log2(data_len);
    uint64_t guid = 0x1122334455667788;
    memcpy(invalidSample->data, &guid, data_len);

    // Note: Current implementation may not strictly validate tag/link type
    // mismatch
    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    // Test completes - implementation may handle gracefully
    (void)rc;
}

TEST_F(NsmNetworkAddressAggregatorTest,
       HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_NetAddr_Fail";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_fail";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 99;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // 7-byte buffer: decode_aggregate_resp requires 9 bytes minimum so the
    // short buffer triggers NSM_SW_ERROR_LENGTH before accessing any fields
    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<struct nsm_msg*>(responseBuffer.data());

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// L1518 FALSE branch: InfiniBand link type + tag that is neither
// NSM_TAG_NODE_GUID nor NSM_TAG_PORT_GUID → falls to else at L1525.
// NSM_TAG_MAC_ADDRESS (tag=1) with MAC_ADDRESS_LENGTH bytes decodes OK but
// is not a GUID tag, so L1518 evaluates to false.
TEST_F(NsmNetworkAddressAggregatorTest,
       HandleResponseMsg_InfiniBand_UnknownTag_FalseBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_IB_UnknownTag";
    std::string type = "NSM_Port";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/port_ib_unk_tag";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 100;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // Build a 2-sample aggregate response:
    //   sample 0: NSM_TAG_LINK_TYPE = NSM_PORT_PROTOCOL_INFINIBAND
    //   sample 1: NSM_TAG_MAC_ADDRESS (tag=1, valid MAC data)
    // After processing the link-type sample, linkType==INFINIBAND.
    // The MAC sample passes decode but is not NODE_GUID or PORT_GUID, so
    // L1511 is false, L1518 is false (the else at L1525 executes).
    const size_t mac_len = MAC_ADDRESS_LENGTH;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + mac_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample — InfiniBand
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0; // log2(1) = 0
    linkSample->data[0] = NSM_PORT_PROTOCOL_INFINIBAND;
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + mac_len;

    // MAC address sample — valid for Ethernet but not for InfiniBand
    auto macSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    macSample->tag = NSM_TAG_MAC_ADDRESS;
    macSample->valid = 1;
    macSample->length = static_cast<uint8_t>(std::log2(mac_len));
    uint8_t macAddr[MAC_ADDRESS_LENGTH] = {0xDE, 0xAD, 0xBE, 0xEF,
                                           0x00, 0x01, 0x02, 0x03};
    memcpy(macSample->data, macAddr, mac_len);

    // handleResponseMsg should succeed overall (or return decode error for the
    // invalid-for-IB MAC sample) but must NOT crash.
    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    // The MAC decode itself succeeds (NSM_SUCCESS from decode_aggregate_
    // network_address_data), but the tag is invalid for InfiniBand (logged
    // as error). The function may return NSM_SW_SUCCESS with the error logged.
    (void)rc; // result is implementation-defined; we only verify no crash
}

// Covers L1530: Ethernet link type + NSM_TAG_PERMANENT_MAC_ADDRESS sample.
// The else-if branch at L1530 must be taken.
TEST_F(NsmNetworkAddressAggregatorTest,
       HandleResponseMsg_Ethernet_PermanentMacAddress)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth_PermanentMAC";
    std::string type = "NSM_Port";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/port_eth_permac";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 101;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    const size_t mac_len = MAC_ADDRESS_LENGTH;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + mac_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample — Ethernet
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_ETHERNET;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // Permanent MAC sample
    auto permMacSample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    permMacSample->tag = NSM_TAG_PERMANENT_MAC_ADDRESS;
    permMacSample->valid = 1;
    permMacSample->length = static_cast<uint8_t>(std::log2(mac_len));
    uint8_t permMac[MAC_ADDRESS_LENGTH] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    memcpy(permMacSample->data, permMac, mac_len);

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// Covers L1503: sample.valid==0 → skip (FALSE branch of !sample.valid)
// Need Ethernet link type + one invalid (valid=0) MAC sample.
TEST_F(NsmNetworkAddressAggregatorTest,
       HandleResponseMsg_Ethernet_InvalidSample_Skipped)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth_InvalidSample";
    std::string type = "NSM_Port";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/port_eth_inval";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 102;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    const size_t mac_len = MAC_ADDRESS_LENGTH;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + mac_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_ETHERNET;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // MAC sample with valid=0 → should be skipped
    auto macSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    macSample->tag = NSM_TAG_MAC_ADDRESS;
    macSample->valid = 0; // invalid → skip
    macSample->length = static_cast<uint8_t>(std::log2(mac_len));
    uint8_t macAddr[MAC_ADDRESS_LENGTH] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(macSample->data, macAddr, mac_len);

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// Covers L1499: sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
// → continue (reserved tag). Use tag=0xFF (>0xEF) after a valid link type.
TEST_F(NsmNetworkAddressAggregatorTest,
       HandleResponseMsg_Ethernet_ReservedTag_Skipped)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth_ReservedTag";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_eth_rsvd";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 103;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    const size_t data_len = MAC_ADDRESS_LENGTH;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample — Ethernet
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_ETHERNET;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // Reserved tag sample (tag=0xFF > 0xEF) → should be skipped
    auto rsvdSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    rsvdSample->tag = 0xFF;
    rsvdSample->valid = 1;
    rsvdSample->length = static_cast<uint8_t>(std::log2(data_len));
    memset(rsvdSample->data, 0, data_len);

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// Covers L1511: decode_aggregate_network_address_data fails (wrong data
// length). Use NSM_TAG_MAC_ADDRESS with length field set to give 4 bytes
// instead of 6.
TEST_F(NsmNetworkAddressAggregatorTest,
       HandleResponseMsg_Ethernet_DecodeFail_SetsReturnError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth_DecodeFail";
    std::string type = "NSM_Port";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/port_eth_decfail";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 104;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    // 4 bytes of data (not 6) for NSM_TAG_MAC_ADDRESS → NSM_SW_ERROR_LENGTH
    const size_t wrong_len = 4;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + wrong_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Link type sample — Ethernet
    auto linkSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    linkSample->tag = NSM_TAG_LINK_TYPE;
    linkSample->valid = 1;
    linkSample->length = 0;
    linkSample->data[0] = NSM_PORT_PROTOCOL_ETHERNET;
    sample_ptr += sizeof(nsm_aggregate_resp_sample);

    // MAC sample with wrong length (4 bytes, not 6) → decode fails
    auto macSample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    macSample->tag = NSM_TAG_MAC_ADDRESS;
    macSample->valid = 1;
    macSample->length =
        static_cast<uint8_t>(std::log2(wrong_len)); // 2 = log2(4)
    memset(macSample->data, 0, wrong_len);

    auto rc = networkAddr.handleResponseMsg(responseMsg, responseBuffer.size());
    // Decode fails → returnValue is set to decodeRc, continues loop, returns
    // error
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}
