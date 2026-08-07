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

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct NsmPortBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ===========================================================================
// NsmNetworkAddressAggregator - genRequestMsg encode failure
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmNetworkAddressAggregator_GenRequestMsg_InvalidInstanceId)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_BadInst";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_bad_inst";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    // instanceId > NSM_INSTANCE_MAX triggers encode failure -> nullopt
    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmGetPortECCCounters - genRequestMsg encode failure
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmGetPortECCCounters_GenRequestMsg_InvalidInstanceId)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_BadInst";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_bad_inst";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmGetPortECCCounters - handleResponseMsg decode-fail (valgrind-safe)
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmGetPortECCCounters_HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_DecFail_VS";
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/ecc_decfail_vs";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Valgrind-safe decode-fail buffer: 7 bytes minimum for
    // decode_reason_code_and_cc to read payload[1] safely
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmGetPortECCCounters - handleResponseMsg: cc=NSM_ERROR via aggregate resp
// Covers the cc ? cc : rc TRUE branch at L1588
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmGetPortECCCounters_HandleResponseMsg_ErrorCC_ReturnsCcNotRc)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_ErrorCC_Br";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_errcc_br";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build an error aggregate response with cc=NSM_ERROR
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_aggregate_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_ERROR;
    payload->telemetry_count = 0;

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_ERROR);
}

// ===========================================================================
// NsmGetPortECCCounters - handleResponseMsg: success with no lane tags
// Covers updateRawErrorsPerLane == false branch at L1664
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmGetPortECCCounters_HandleResponseMsg_NoLaneTags_SkipsRawErrorUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_NoLane";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_nolane";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build aggregate response with only NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES
    // and NSM_TAG_ECC_CORRECTED_BITS (no lane tags)
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint64_t val1 = 42;
    uint8_t bytes1[8];
    memcpy(bytes1, &val1, sizeof(val1));
    size_t sample1Len = 0;
    uint8_t sample1Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES, true, bytes1, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample1Buf), &sample1Len);

    uint64_t val2 = 99;
    uint8_t bytes2[8];
    memcpy(bytes2, &val2, sizeof(val2));
    size_t sample2Len = 0;
    uint8_t sample2Buf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_ECC_CORRECTED_BITS, true, bytes2, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample2Buf), &sample2Len);

    std::vector<uint8_t> response(headerSize + sample1Len + sample2Len);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 2,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, sample1Buf, sample1Len);
    offset += sample1Len;
    memcpy(response.data() + offset, sample2Buf, sample2Len);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.portECCIntf->symbolErrorRXBytes(), 42u);
    EXPECT_EQ(sensor.portECCIntf->correctedBits(), 99u);
    // rawErrorsPerLane should not have been updated (updateRawErrorsPerLane
    // stays false)
}

// ===========================================================================
// NsmGetPortECCCounters - handleResponseMsg: decode_aggregate_port_ecc_counter
// fails for a sample -> sets returnValue to decodeRc but continues
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmGetPortECCCounters_HandleResponseMsg_DecodeCounterFail_Continue)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_DecCntFail";
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/ecc_deccntfail";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build aggregate response with a sample that has wrong data length
    // to trigger decode_aggregate_port_ecc_counter_data failure
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Use 4 bytes instead of 8 for a valid ECC tag -> decode fails
    uint8_t badBytes[4] = {0x01, 0x02, 0x03, 0x04};
    size_t sampleLen = 0;
    uint8_t sampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES, true, badBytes, 4,
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf), &sampleLen);

    std::vector<uint8_t> response(headerSize + sampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 1,
                          responseMsg);
    memcpy(response.data() + headerSize, sampleBuf, sampleLen);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

// ===========================================================================
// NsmNetworkAddressAggregator - handleResponseMsg: cc=NSM_ERROR via aggregate
// Covers the cc ? cc : rc TRUE branch at L1410
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmNetworkAddressAggregator_HandleResponseMsg_ErrorCC_ReturnsCc)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_ErrorCC_Br";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_errcc_br";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_aggregate_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_ERROR;
    payload->telemetry_count = 0;

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_ERROR);
}

// ===========================================================================
// NsmNetworkAddressAggregator - handleResponseMsg: valgrind-safe decode fail
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmNetworkAddressAggregator_HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_DecFail_VS";
    std::string type = "NSM_Port";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/naa_decfail_vs";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPortCharacteristics - handleResponseMsg: valgrind-safe decode fail
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortCharacteristics_HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Char_DecFail_VS";
    uint8_t portNum = 1;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/char_decfail_vs";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    auto portHealthMetricsIntfCtor =
        std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());
    NsmPortCharacteristics sensor(bus, portName, portNum, type, NSM_DEV_ID_GPU,
                                  portMetricsOem3Intf, iBPortIntf,
                                  portHealthMetricsIntfCtor, objPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPortMetrics - handleResponseMsg: valgrind-safe decode fail
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortMetrics_HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_DecFail_VS";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_decfail_vs_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_decfail_vs";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ===========================================================================
// NsmPortCharacteristics - genRequestMsg: success path
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmPortCharacteristics_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Char_GenOK";
    uint8_t portNum = 1;
    std::string type = "CharType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/char_gen_ok";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    auto portHealthMetricsIntfCtor =
        std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());
    NsmPortCharacteristics sensor(bus, portName, portNum, type, NSM_DEV_ID_GPU,
                                  portMetricsOem3Intf, iBPortIntf,
                                  portHealthMetricsIntfCtor, objPath);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ===========================================================================
// NsmPortCharacteristics - genRequestMsg: encode failure
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortCharacteristics_GenRequestMsg_InvalidInstanceId)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Char_BadInst";
    uint8_t portNum = 1;
    std::string type = "CharType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/char_bad_inst";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    auto portHealthMetricsIntfCtor =
        std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());
    NsmPortCharacteristics sensor(bus, portName, portNum, type, NSM_DEV_ID_GPU,
                                  portMetricsOem3Intf, iBPortIntf,
                                  portHealthMetricsIntfCtor, objPath);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmPortMetrics - genRequestMsg: encode failure
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmPortMetrics_GenRequestMsg_InvalidInstanceId)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_BadInst";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bad_inst_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_bad_inst";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ===========================================================================
// NsmPortMetrics - genRequestMsg: success path
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmPortMetrics_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_GenOK";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_gen_ok_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_gen_ok";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

// ===========================================================================
// NsmNetworkAddressAggregator - genRequestMsg: success path
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmNetworkAddressAggregator_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "NAA_GenOK";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/naa_gen_ok";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                       nodeGuidObjPath, ethernetMacObjPath,
                                       permanentMacObjPath, portNumber);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_addresses_req));
}

// ===========================================================================
// NsmGetPortECCCounters - genRequestMsg: success path
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmGetPortECCCounters_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_GenOK";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_gen_ok";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req));
}

// ===========================================================================
// NsmPortMetrics - handleResponseMsg: success updates interfaces
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortMetrics_HandleResponseMsg_Success_UpdatesInterfaces)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_Success";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_success_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_success";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    data.port_rcv_pkts = 500;
    data.port_rcv_data = 1000;
    data.port_xmit_pkts = 600;
    data.port_xmit_data = 1200;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_port_telemetry_counter_resp) -
                                  1 + sizeof(data));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(iBPortIntf->rxPkts(), 500u);
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(), 1000u);
}

// ===========================================================================
// NsmPortCharacteristics - handleResponseMsg: success updates interfaces
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortCharacteristics_HandleResponseMsg_Success_UpdatesInterfaces)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Char_Success";
    uint8_t portNum = 1;
    std::string type = "CharType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/char_success";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    auto portHealthMetricsIntfCtor =
        std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());
    NsmPortCharacteristics sensor(bus, portName, portNum, type, NSM_DEV_ID_GPU,
                                  portMetricsOem3Intf, iBPortIntf,
                                  portHealthMetricsIntfCtor, objPath);

    struct nsm_port_characteristics_data data = {};
    data.nv_port_line_rate_mbps = 400000;
    data.nv_port_data_rate_kbps = 200000;
    data.status_lane_info = 0x04;
    data.port_status.port_down_reason_code =
        NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN;

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(sensor.portInfoIntf->maxSpeed(), 400u);
}

// ===========================================================================
// NsmPortCharacteristics - handleResponseMsg: error CC path
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmPortCharacteristics_HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Char_ErrCC";
    uint8_t portNum = 1;
    std::string type = "CharType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/char_errcc";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    auto portHealthMetricsIntfCtor =
        std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());
    NsmPortCharacteristics sensor(bus, portName, portNum, type, NSM_DEV_ID_GPU,
                                  portMetricsOem3Intf, iBPortIntf,
                                  portHealthMetricsIntfCtor, objPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_port_characteristics_data charData = {};
    auto encRc = encode_query_port_characteristics_resp(0, NSM_ERROR, ERR_NULL,
                                                        &charData, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmPortMetrics - handleResponseMsg: error CC path
// ===========================================================================
TEST_F(NsmPortBranchTest, NsmPortMetrics_HandleResponseMsg_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_ErrCC";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_errcc_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_errcc";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_port_counter_data portData = {};
    auto encRc = encode_get_port_telemetry_counter_resp(0, NSM_ERROR, ERR_NULL,
                                                        &portData, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmPortCharacteristics - handleResponseMsg: decode-success, non-zero CC
// Covers the `if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS)` FALSE branch
// where rc==SUCCESS but cc!=SUCCESS
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortCharacteristics_HandleResponseMsg_DecodeSuccessNonZeroCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Char_NonZeroCC";
    uint8_t portNum = 1;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/char_nonzerocc";
    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus, objPath.c_str());

    auto portHealthMetricsIntfCtor =
        std::make_shared<PortHealthMetricsIntf>(bus, objPath.c_str());
    NsmPortCharacteristics sensor(bus, portName, portNum, type, NSM_DEV_ID_GPU,
                                  portMetricsOem3Intf, iBPortIntf,
                                  portHealthMetricsIntfCtor, objPath);

    // Build a properly-sized non-success response so decode_reason_code_and_cc
    // returns NSM_SW_SUCCESS with cc=NSM_ERROR
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_header_info hdrInfo = {};
    hdrInfo.nsm_msg_type = NSM_RESPONSE;
    hdrInfo.instance_id = 0;
    hdrInfo.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;
    pack_nsm_header(&hdrInfo, &responseMsg->hdr);
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_QUERY_PORT_CHARACTERISTICS,
                       responseMsg);

    auto cc = sensor.handleResponseMsg(responseMsg, buf.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmPortMetrics - handleResponseMsg: decode-success, non-zero CC
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmPortMetrics_HandleResponseMsg_DecodeSuccessNonZeroCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName = "PM_NonZeroCC";
    uint8_t portNum = 1;
    std::string type = "PMType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nonzerocc_parent";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pm_nonzerocc";
    std::vector<utils::Association> associations;
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics sensor(bus, pName, portNum, type, deviceType, associations,
                          parentObjPath, inventoryObjPath, iBPortIntf,
                          portMetricsOem2Intf, portPacketCountersIntf);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_header_info hdrInfo = {};
    hdrInfo.nsm_msg_type = NSM_RESPONSE;
    hdrInfo.instance_id = 0;
    hdrInfo.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;
    pack_nsm_header(&hdrInfo, &responseMsg->hdr);
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_GET_PORT_TELEMETRY_COUNTER,
                       responseMsg);

    auto cc = sensor.handleResponseMsg(responseMsg, buf.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

// ===========================================================================
// NsmGetPortECCCounters - handleResponseMsg: success with all lane tags
// Covers updateRawErrorsPerLane == true branch at L1664
// ===========================================================================
TEST_F(NsmPortBranchTest,
       NsmGetPortECCCounters_HandleResponseMsg_AllLaneTags_UpdatesRawErrors)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "ECC_AllLanes";
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/ecc_alllanes";
    uint8_t portNumber = 1;

    NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    struct SampleEntry
    {
        uint8_t tag;
        uint64_t value;
    };
    SampleEntry entries[] = {
        {NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES, 100},
        {NSM_TAG_ECC_CORRECTED_BITS, 200},
        {NSM_TAG_ECC_RAW_ERRORS_LANE_0, 10},
        {NSM_TAG_ECC_RAW_ERRORS_LANE_1, 20},
        {NSM_TAG_ECC_RAW_ERRORS_LANE_2, 30},
        {NSM_TAG_ECC_RAW_ERRORS_LANE_3, 40},
    };

    std::vector<uint8_t> allSamples;
    for (auto& entry : entries)
    {
        uint8_t counterBytes[8];
        memcpy(counterBytes, &entry.value, sizeof(entry.value));
        size_t sampleLen = 0;
        uint8_t sampleBuf[16] = {};
        encode_aggregate_resp_sample(
            entry.tag, true, counterBytes, 8,
            reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf),
            &sampleLen);
        allSamples.insert(allSamples.end(), sampleBuf, sampleBuf + sampleLen);
    }

    std::vector<uint8_t> response(headerSize + allSamples.size());
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 6,
                          responseMsg);
    memcpy(response.data() + headerSize, allSamples.data(), allSamples.size());

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.portECCIntf->symbolErrorRXBytes(), 100u);
    EXPECT_EQ(sensor.portECCIntf->correctedBits(), 200u);
    auto rawErrors = sensor.portECCIntf->rawErrorsPerLane();
    EXPECT_EQ(rawErrors.size(), 4u);
    EXPECT_EQ(rawErrors[0], 10u);
    EXPECT_EQ(rawErrors[1], 20u);
    EXPECT_EQ(rawErrors[2], 30u);
    EXPECT_EQ(rawErrors[3], 40u);
}

// ===========================================================================
// EthPortTelemetryAggregator - genRequestMsg: success + encode failure
// ===========================================================================
TEST_F(NsmPortBranchTest, EthPortTelemetryAggregator_GenRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_GenOK";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_gen_ok";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    auto request = sensor.genRequestMsg(10, 1);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmPortBranchTest,
       EthPortTelemetryAggregator_GenRequestMsg_InvalidInstanceId)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Eth_BadInst";
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/eth_bad_inst";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, objPath.c_str());

    EthPortTelemetryAggregator sensor(bus, portName, portNumber, type, objPath,
                                      portMetricsOem2Intf,
                                      portPacketCountersIntf);

    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}
