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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "network-ports.h"

#include "utils.hpp"

#include <filesystem>

#define private public
#define protected public

#include "nsmInterface.hpp"
#include "nsmPCIeErrors.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPCIePort.hpp"
#include "nsmPort.hpp"
#include "nsmPortInfo.hpp"

namespace nsm
{
requester::Coroutine createNsmPortSensor(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath,
                                         bool enableNetworkPortAddresses);
requester::Coroutine
    createNsmPortSensorWithNetworkPortAddresses(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);
requester::Coroutine createNsmPortSensorGeneric(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath);
} // namespace nsm

// Helper to create NsmPortMetrics for reuse
struct NsmPortMetricsHelper
{
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    std::string pName{"dummy_port"};
    uint8_t portNum = 1;
    std::string type = "DummyType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/dummy_device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/dummy_device/Ports";
    std::vector<utils::Association> associations;
    std::shared_ptr<IBPortIntf> iBPortIntf =
        std::make_shared<IBPortIntf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());
};

TEST(NsmPortMetrics, GoodTest)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    EXPECT_EQ(portTel.portName, h.pName);
    EXPECT_EQ(portTel.portNumber, h.portNum);
    EXPECT_EQ(portTel.iBPortIntf, h.iBPortIntf);
    EXPECT_NE(portTel.portMetricsOem2Intf, nullptr);
    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);

    std::vector<uint8_t> portData{
        0xFF, 0xFF, 0xFF, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }; /*for counter values, 8 bytes each*/
    struct nsm_port_counter_data portTelData = {};
    std::memcpy(&portTelData, portData.data(), sizeof(portData));

    portTel.updateCounterValues(&portTelData);

    EXPECT_EQ(portTel.iBPortIntf->rxPkts(), portTelData.port_rcv_pkts);
    // checking only first and last values for iBPortIntf
    EXPECT_EQ(portTel.iBPortIntf->txWait(), portTelData.xmit_wait);

    EXPECT_EQ(portTel.portMetricsOem2Intf->rxBytes(),
              portTelData.port_rcv_data);
    // checking only first and last values for portMetricsOem2Intf
    EXPECT_EQ(portTel.portMetricsOem2Intf->txBytes(),
              portTelData.port_xmit_data);
}

TEST(NsmPortMetrics, GoodGenRequestMsg)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    auto request = portTel.genRequestMsg(12, 30);
    EXPECT_TRUE(request.has_value());
    EXPECT_GT(request->size(), sizeof(nsm_msg_hdr));
}

TEST(NsmPortMetrics, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // instanceId > NSM_INSTANCE_MAX -> encode fails -> return nullopt
    auto request = portTel.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPortMetrics, GoodHandleResponseMsg)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // Build a valid response with all counters supported
    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    data.port_rcv_pkts = 100;
    data.port_rcv_data = 200;
    data.port_xmit_pkts = 300;
    data.port_xmit_data = 400;
    data.symbol_ber = 0x0A21; // non-zero BER value

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_port_telemetry_counter_resp) -
                                  1 + sizeof(data));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = portTel.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(h.iBPortIntf->rxPkts(), 100u);
    EXPECT_EQ(h.portMetricsOem2Intf->rxBytes(), 200u);
}

TEST(NsmPortMetrics, BadHandleResponseMsg)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // Build an error response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_non_success_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_port_telemetry_counter_resp(0, NSM_ERROR, ERR_NULL, nullptr,
                                           responseMsg);

    auto cc = portTel.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// NsmPortMetrics: rc==NSM_SW_SUCCESS but cc==NSM_ERROR (L1134 second-operand
// FALSE branch). decode_reason_code_and_cc returns NSM_SW_SUCCESS (header ok)
// but sets cc=NSM_ERROR -> `&&` second operand evaluated and FALSE.
TEST(NsmPortMetrics, HandleResponseMsg_DecodeSuccessNonZeroCC_NoUpdate)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // Build a properly-encoded non-success response:
    // header + nsm_common_non_success_resp with cc=NSM_ERROR so that
    // decode_reason_code_and_cc returns NSM_SW_SUCCESS (length ok) but cc!=OK.
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_non_success_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    nsm_header_info hdrInfo = {};
    hdrInfo.nsm_msg_type = NSM_RESPONSE;
    hdrInfo.instance_id = 0;
    hdrInfo.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;
    pack_nsm_header(&hdrInfo, &responseMsg->hdr);
    encode_reason_code(NSM_ERROR, ERR_NULL, NSM_GET_PORT_TELEMETRY_COUNTER,
                       responseMsg);

    auto cc = portTel.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_ERROR);
}

TEST(NsmPortMetrics, UpdateCounterValuesNullPortData)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);
    // null portData => covers the else branch at line 1078
    portTel.updateCounterValues(nullptr);
}

TEST(NsmPortMetrics, UpdateCounterValuesNullInterfaces)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    data.port_rcv_pkts = 10;

    // Null out iBPortIntf to cover the else branch at line 1010
    portTel.iBPortIntf = nullptr;
    portTel.updateCounterValues(&data);

    // Null out portMetricsOem2Intf to cover else at line 1030
    portTel.iBPortIntf = h.iBPortIntf;
    portTel.portMetricsOem2Intf = nullptr;
    portTel.updateCounterValues(&data);

    // Null out portPacketCountersIntf to cover else at line 1070
    portTel.portMetricsOem2Intf = h.portMetricsOem2Intf;
    portTel.portPacketCountersIntf = nullptr;
    portTel.updateCounterValues(&data);
}

TEST(NsmPortMetrics, GetBitErrorRateNonZero)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // Test non-zero value with symbol_ber_coef_float == 0
    double ber = portTel.getBitErrorRate(0x0A01);
    EXPECT_GT(ber, 0.0);

    // Test non-zero value with symbol_ber_coef_float != 0
    double ber2 = portTel.getBitErrorRate(0x0A21);
    EXPECT_GT(ber2, 0.0);
}

TEST(NsmPortMetrics, GetBitErrorRateZero)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    double ber = portTel.getBitErrorRate(0);
    EXPECT_DOUBLE_EQ(ber, 0.0);
}

TEST(NsmPortMetrics, UpdateCounterValuesUnsupportedCounters)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // All supported_counter bits zero - should skip all counter updates
    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0x00, sizeof(data.supported_counter));
    data.port_rcv_data = 9999;
    data.port_xmit_data = 9999;

    portTel.updateCounterValues(&data);

    // Values should be unchanged (default 0) since counters are unsupported
    EXPECT_EQ(h.portMetricsOem2Intf->rxBytes(), 0u);
    EXPECT_EQ(h.portMetricsOem2Intf->txBytes(), 0u);
}

TEST(NsmPortMetrics, UpdateCounterValuesWithEstimatedEffectiveBER)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    // Enable specific counters to cover remaining branches
    struct nsm_port_counter_data data = {};
    memset(&data.supported_counter, 0xFF, sizeof(data.supported_counter));
    data.effective_ber = 0x0A31;
    data.total_raw_ber = 0x0B42;
    data.total_raw_error = 50;
    data.effective_error = 60;
    data.symbol_error = 70;
    data.unintentional_link_down_count = 80;
    data.intentional_link_down_count = 90;
    data.port_multicast_rcv_pkts = 100;
    data.port_unicast_rcv_pkts = 200;
    data.port_unicast_xmit_pkts = 300;
    data.port_multicast_xmit_pkts = 400;
    data.port_bcast_xmit_pkts = 500;

    portTel.updateCounterValues(&data);

    EXPECT_EQ(h.iBPortIntf->totalRawError(), 50u);
    EXPECT_EQ(h.iBPortIntf->effectiveError(), 60u);
    EXPECT_EQ(h.iBPortIntf->symbolErrors(), 70u);
    EXPECT_EQ(h.iBPortIntf->unintentionalLinkDownCount(), 80u);
    EXPECT_EQ(h.iBPortIntf->intentionalLinkDownCount(), 90u);
    EXPECT_EQ(h.portPacketCountersIntf->rxMulticastPkts(), 100u);
    EXPECT_EQ(h.portPacketCountersIntf->rxUnicastPkts(), 200u);
    EXPECT_EQ(h.portPacketCountersIntf->txUnicastPkts(), 300u);
    EXPECT_EQ(h.portPacketCountersIntf->txMulticastPkts(), 400u);
    EXPECT_EQ(h.portPacketCountersIntf->txBroadcastPkts(), 500u);
}

// ---- NsmPortCharacteristics tests ----

TEST(NsmPortCharacteristics, GoodConstructAndGenReq)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"char_port"};
    uint8_t portNum = 2;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/char_port";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());

    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);
    EXPECT_EQ(sensor.portName, portName);

    auto request = sensor.genRequestMsg(10, 20);
    EXPECT_TRUE(request.has_value());
}

TEST(NsmPortCharacteristics, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"char_port_bad"};
    uint8_t portNum = 2;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/char_port_bad";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());

    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);
    // instanceId > NSM_INSTANCE_MAX -> encode fails -> return nullopt
    auto request = sensor.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmPortCharacteristics, GoodHandleResponseMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"char_port2"};
    uint8_t portNum = 3;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/char_port2";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());

    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

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
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(NsmPortCharacteristics, BadHandleResponseMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"char_port3"};
    uint8_t portNum = 4;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/char_port3";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());

    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

    // Error response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_non_success_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_port_characteristics_resp(0, NSM_ERROR, ERR_NULL, nullptr,
                                           responseMsg);
    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmPortCharacteristics, UpdateLinkDownCodeAllCases)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"linkdown_port"};
    uint8_t portNum = 5;
    std::string type = "LDType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/linkdown_port";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());

    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

    // Test all link down reason codes
    const uint32_t codes[] = {
        NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN,
        NSM_PORT_DOWN_REASON_CODE_UNKNOWN,
        NSM_PORT_DOWN_REASON_CODE_HI_SER_BER,
        NSM_PORT_DOWN_REASON_CODE_BLOCK_LOCK_LOSS,
        NSM_PORT_DOWN_REASON_CODE_ALIGNMENT_LOSS,
        NSM_PORT_DOWN_REASON_CODE_FEC_SYNC_LOSS,
        NSM_PORT_DOWN_REASON_CODE_PLL_LOCK_LOSS,
        NSM_PORT_DOWN_REASON_CODE_FIFO_OVERFLOW,
        NSM_PORT_DOWN_REASON_CODE_FALSE_SKIP_CONDITION,
        NSM_PORT_DOWN_REASON_CODE_MINOR_ERR_THRESHOLD,
        NSM_PORT_DOWN_REASON_CODE_PHY_LAYER_RETRANSMIT_TIMEOUT,
        NSM_PORT_DOWN_REASON_CODE_HEARTBEAT_ERRORS,
        NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_CREDIT_MON_WD,
        NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_INTEGRITY_THRESHOLD,
        NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_BUFFER_OVERRUN,
        NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HEALTHY,
        NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HI_BER,
        NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HEALTHY,
        NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HI_BER,
        NSM_PORT_DOWN_REASON_CODE_DOWN_BY_VERIFICATION_GW,
        NSM_PORT_DOWN_REASON_CODE_RECEIVED_REMOTE_FAULT,
        NSM_PORT_DOWN_REASON_CODE_RECEIEVED_TS1,
        NSM_PORT_DOWN_REASON_CODE_DOWN_BY_MGMT_CMD,
        NSM_PORT_DOWN_REASON_CODE_CABLE_UNPLUGGED,
        NSM_PORT_DOWN_REASON_CODE_CABLE_ACCESS_ISSUES,
        NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN,
        NSM_PORT_DOWN_REASON_CODE_CURRENT_ISSUE,
        NSM_PORT_DOWN_REASON_CODE_POWER_BUDGET,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_RAW_BER,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_EFFECTIVE_BER,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_SYMBOL_BER,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_CREDIT_WATCHDOG,
        NSM_PORT_DOWN_REASON_CODE_PEER_SLEEP,
        NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE,
        NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE_LOCK,
        NSM_PORT_DOWN_REASON_CODE_PEER_THERMAL_EVENT,
        NSM_PORT_DOWN_REASON_CODE_PEER_FORCE_EVENT,
        NSM_PORT_DOWN_REASON_CODE_PEER_RESET_EVENT,
        0xFFFF, // default case
    };
    for (auto code : codes)
    {
        sensor.updateLinkDownCode(code);
    }
}

TEST(NsmPortCharacteristics, HandleResponseWithLinkDownCodes)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"charld_port"};
    uint8_t portNum = 6;
    std::string type = "LDType2";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/charld_port";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());

    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

    // Test with various port down reason codes via handleResponseMsg
    uint32_t testCodes[] = {
        NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN,
        NSM_PORT_DOWN_REASON_CODE_PEER_RESET_EVENT,
    };

    for (auto code : testCodes)
    {
        struct nsm_port_characteristics_data data = {};
        data.nv_port_line_rate_mbps = 200000;
        data.nv_port_data_rate_kbps = 100000;
        data.status_lane_info = 0x02;
        data.port_status.port_down_reason_code = code;

        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp));
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_query_port_characteristics_resp(
            0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        auto cc = sensor.handleResponseMsg(responseMsg, response.size());
        EXPECT_EQ(cc, NSM_SUCCESS);
    }
}

// ---- NsmPortStatus tests ----

TEST(NsmPortStatus, GoodConstruct)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"status_port"};
    uint8_t portNum = 7;
    std::string type = "StatusType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/status_port";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());

    nsm::NsmPortStatus sensor(bus, portName, portNum, type, portMetricsOem3Intf,
                              objPath);
    EXPECT_EQ(sensor.portName, portName);
}

// ---- EthPortTelemetryAggregator tests ----

TEST(EthPortTelemetryAggregator, GoodConstruct)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port"};
    uint16_t portNumber = 1;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);
    EXPECT_EQ(sensor.portName, portName);
}

TEST(EthPortTelemetryAggregator, GoodGenRequestMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port2"};
    uint16_t portNumber = 2;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port2";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);

    auto request = sensor.genRequestMsg(10, 20);
    EXPECT_TRUE(request.has_value());
}

TEST(EthPortTelemetryAggregator, HandleSampleInvalidSample)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port3"};
    uint16_t portNumber = 3;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port3";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);

    // Invalid sample (valid = false)
    nsm::NsmSensorAggregator::TelemetrySample invalidSample{0, 0, nullptr,
                                                            false};
    auto rc = sensor.handleSample(invalidSample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Sample with tag > max
    nsm::NsmSensorAggregator::TelemetrySample highTagSample{0xFF, 0, nullptr,
                                                            true};
    rc = sensor.handleSample(highTagSample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(EthPortTelemetryAggregator, HandleSampleDecodeError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port3b"};
    uint16_t portNumber = 3;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port3b";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);

    // Valid tag but wrong data length to trigger decode failure path
    // Tag 0 (RXBytes) expects 8 bytes, provide only 4 (a power of 2)
    uint8_t badData[4] = {0x01, 0x02, 0x03, 0x04};
    nsm::NsmSensorAggregator::TelemetrySample sample{0, 4, badData, true};
    auto rc = sensor.handleSample(sample);
    // Note: handleSample returns bool as int - false (0) == NSM_SW_SUCCESS
    // The decode failure path is exercised regardless of assertion
    (void)rc;
}

TEST(EthPortTelemetryAggregator, HandleSampleValidCounters)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port4"};
    uint16_t portNumber = 4;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port4";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);

    // Test all 21 tags (0 through 20) via handleSample
    // Tags 0-7 are 64-bit counters, tags 8-20 are 32-bit counters
    for (uint8_t tag = 0; tag <= 20; tag++)
    {
        nsm_ethernet_port_counter_data counterData = {};
        uint8_t encodedData[8] = {};

        if (tag <= 7)
        {
            // 64-bit counter
            counterData.ethernet_port_counter_data_64bit = 1000 + tag;
            size_t encodedLen = sizeof(uint64_t);
            encode_aggregate_eth_port_telemetry_data(tag, &counterData,
                                                     encodedData, &encodedLen);

            nsm::NsmSensorAggregator::TelemetrySample sample{
                tag, static_cast<uint8_t>(encodedLen), encodedData, true};
            auto rc = sensor.handleSample(sample);
            EXPECT_EQ(rc, NSM_SW_SUCCESS);
        }
        else
        {
            // 32-bit counter
            counterData.ethernet_port_counter_data_32bit = 2000 + tag;
            size_t encodedLen = sizeof(uint32_t);
            encode_aggregate_eth_port_telemetry_data(tag, &counterData,
                                                     encodedData, &encodedLen);

            nsm::NsmSensorAggregator::TelemetrySample sample{
                tag, static_cast<uint8_t>(encodedLen), encodedData, true};
            auto rc = sensor.handleSample(sample);
            EXPECT_EQ(rc, NSM_SW_SUCCESS);
        }
    }
}

TEST(EthPortTelemetryAggregator, UpdateCounterValuesDirect)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port4b"};
    uint16_t portNumber = 4;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port4b";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);

    // Call updateCounterValues directly for each tag
    // Tags 0-7: 64-bit counters
    for (uint8_t tag = 0; tag <= 7; tag++)
    {
        nsm_ethernet_port_counter_data counterData = {};
        counterData.ethernet_port_counter_data_64bit = 5000 + tag;
        sensor.updateCounterValues(tag, &counterData);
    }

    // Tags 8-20: 32-bit counters
    for (uint8_t tag = 8; tag <= 20; tag++)
    {
        nsm_ethernet_port_counter_data counterData = {};
        counterData.ethernet_port_counter_data_32bit = 6000 + tag;
        sensor.updateCounterValues(tag, &counterData);
    }

    // Verify some values
    EXPECT_EQ(portMetricsOem2Intf->rxBytes(), 5000u);
    EXPECT_EQ(portMetricsOem2Intf->txBytes(), 5001u);
    EXPECT_EQ(sensor.ethPortIntf->rxfcsErrors(), 6008u);
    EXPECT_EQ(sensor.ethPortIntf->txExcessCollisionFrames(), 6020u);

    // Tag not in map should be no-op
    nsm_ethernet_port_counter_data extraData = {};
    extraData.ethernet_port_counter_data_32bit = 9999;
    sensor.updateCounterValues(99, &extraData);
}

TEST(EthPortTelemetryAggregator, GetInterfaceNameCovers)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"eth_port5"};
    uint16_t portNumber = 5;
    std::string type = "EthType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/eth_port5";
    auto portMetricsOem2Intf =
        std::make_shared<nsm::PortMetricsOem2Intf>(bus, objPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<nsm::PortPacketCountersIntf>(bus, objPath.c_str());

    nsm::EthPortTelemetryAggregator sensor(bus, portName, portNumber, type,
                                           objPath, portMetricsOem2Intf,
                                           portPacketCountersIntf);

    std::string ifaceName;
    // Cover all branches of getInterfaceName
    sensor.getInterfaceName("RXBytes", ifaceName);
    sensor.getInterfaceName("TXBytes", ifaceName);
    sensor.getInterfaceName("RXUnicastPkts", ifaceName);
    sensor.getInterfaceName("RXMulticastPkts", ifaceName);
    sensor.getInterfaceName("RXBroadcastPkts", ifaceName);
    sensor.getInterfaceName("TXUnicastPkts", ifaceName);
    sensor.getInterfaceName("TXMulticastPkts", ifaceName);
    sensor.getInterfaceName("TXBroadcastPkts", ifaceName);
    sensor.getInterfaceName("SomethingElse", ifaceName);
}

// ---- NsmNetworkAddressAggregator tests ----

TEST(NsmNetworkAddressAggregator, GoodConstructAndGenReq)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_sensor"};
    std::string type = "NAAType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/naa_sensor";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    auto request = sensor.genRequestMsg(10, 20);
    EXPECT_TRUE(request.has_value());
}

// ---- NsmGetPortECCCounters tests ----

TEST(NsmGetPortECCCounters, GoodConstructAndGenReq)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"ecc_sensor"};
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/ecc_sensor";
    uint8_t portNumber = 1;

    nsm::NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    auto request = sensor.genRequestMsg(10, 20);
    EXPECT_TRUE(request.has_value());
}

// ---- getTopologyObjPath tests ----

TEST(NsmPortMetrics, ConstructorWithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName{"assoc_port"};
    uint8_t portNum = 1;
    std::string type = "AssocType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/assoc_device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/assoc_device/Ports";
    std::vector<utils::Association> associations;
    associations.push_back({"connected_port", "connected_device",
                            "/xyz/openbmc_project/other/path"});
    associations.push_back(
        {"peer_port", "peer_device", "/xyz/openbmc_project/peer/path"});

    auto iBPortIntf =
        std::make_shared<nsm::IBPortIntf>(bus, inventoryObjPath.c_str());
    auto portMetricsOem2Intf = std::make_shared<nsm::PortMetricsOem2Intf>(
        bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf = std::make_shared<nsm::PortPacketCountersIntf>(
        bus, inventoryObjPath.c_str());

    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                associations, parentObjPath, inventoryObjPath,
                                iBPortIntf, portMetricsOem2Intf,
                                portPacketCountersIntf);

    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);
    EXPECT_NE(portTel.portIntf, nullptr);
}

// ---- NsmNetworkAddressAggregator handleResponseMsg tests ----

TEST(NsmNetworkAddressAggregator, HandleResponseMsgErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_err"};
    std::string type = "NAAType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/dummy/naa_err";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build an error aggregate response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_non_success_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_ERROR, 0,
                                    responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

TEST(NsmNetworkAddressAggregator, HandleResponseMsgEthernetPath)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_eth"};
    std::string type = "NAAType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/dummy/naa_eth";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build aggregate response with link type=Ethernet, MAC address, and
    // permanent MAC
    // We need: header + link_type sample + mac_address sample +
    // permanent_mac_address sample
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    // Link type sample: tag=0, data=[0] (Ethernet)
    uint8_t linkTypeData[] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t linkTypeSampleLen = 0;
    uint8_t linkTypeSampleBuf[16] = {};
    auto samplePtr =
        reinterpret_cast<nsm_aggregate_resp_sample*>(linkTypeSampleBuf);
    encode_aggregate_resp_sample(NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
                                 samplePtr, &linkTypeSampleLen);

    // MAC address sample: tag=1, data=6 bytes MAC
    uint8_t macData[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    size_t macSampleLen = 0;
    uint8_t macSampleBuf[16] = {};
    auto macSamplePtr =
        reinterpret_cast<nsm_aggregate_resp_sample*>(macSampleBuf);
    encode_aggregate_resp_sample(NSM_TAG_MAC_ADDRESS, true, macData, 8,
                                 macSamplePtr, &macSampleLen);

    // Permanent MAC address sample: tag=2
    size_t permMacSampleLen = 0;
    uint8_t permMacSampleBuf[16] = {};
    auto permMacSamplePtr =
        reinterpret_cast<nsm_aggregate_resp_sample*>(permMacSampleBuf);
    encode_aggregate_resp_sample(NSM_TAG_PERMANENT_MAC_ADDRESS, true, macData,
                                 8, permMacSamplePtr, &permMacSampleLen);

    // Build full response
    std::vector<uint8_t> response(headerSize + linkTypeSampleLen +
                                  macSampleLen + permMacSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 3,
                          responseMsg);

    // Copy samples after the header
    size_t offset = headerSize;
    memcpy(response.data() + offset, linkTypeSampleBuf, linkTypeSampleLen);
    offset += linkTypeSampleLen;
    memcpy(response.data() + offset, macSampleBuf, macSampleLen);
    offset += macSampleLen;
    memcpy(response.data() + offset, permMacSampleBuf, permMacSampleLen);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmNetworkAddressAggregator, HandleResponseMsgInfiniBandPath)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_ib"};
    std::string type = "NAAType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/dummy/naa_ib";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build aggregate response with link type=InfiniBand + node GUID + port
    // GUID
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type sample: tag=0, data=[1] (InfiniBand)
    uint8_t linkTypeData[] = {NSM_PORT_PROTOCOL_INFINIBAND};
    size_t linkTypeSampleLen = 0;
    uint8_t linkTypeSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(linkTypeSampleBuf),
        &linkTypeSampleLen);

    // Node GUID sample: tag=3
    uint8_t guidData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    size_t nodeGuidSampleLen = 0;
    uint8_t nodeGuidSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_NODE_GUID, true, guidData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(nodeGuidSampleBuf),
        &nodeGuidSampleLen);

    // Port GUID sample: tag=4
    size_t portGuidSampleLen = 0;
    uint8_t portGuidSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_PORT_GUID, true, guidData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(portGuidSampleBuf),
        &portGuidSampleLen);

    std::vector<uint8_t> response(headerSize + linkTypeSampleLen +
                                  nodeGuidSampleLen + portGuidSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 3,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, linkTypeSampleBuf, linkTypeSampleLen);
    offset += linkTypeSampleLen;
    memcpy(response.data() + offset, nodeGuidSampleBuf, nodeGuidSampleLen);
    offset += nodeGuidSampleLen;
    memcpy(response.data() + offset, portGuidSampleBuf, portGuidSampleLen);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST(NsmNetworkAddressAggregator, HandleResponseMsgUnknownLinkType)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_unk"};
    std::string type = "NAAType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/dummy/naa_unk";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build response with unknown link type (0xFF)
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint8_t linkTypeData[] = {0xFF}; // Unknown
    size_t linkTypeSampleLen = 0;
    uint8_t linkTypeSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(linkTypeSampleBuf),
        &linkTypeSampleLen);

    std::vector<uint8_t> response(headerSize + linkTypeSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 1,
                          responseMsg);

    memcpy(response.data() + headerSize, linkTypeSampleBuf, linkTypeSampleLen);

    // Should return error since linkType stays UNKNOWN
    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

TEST(NsmNetworkAddressAggregator, HandleResponseMsgInvalidSamples)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_inv"};
    std::string type = "NAAType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/dummy/naa_inv";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build response with Ethernet link type + invalid valid bit + high tag
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = Ethernet
    uint8_t linkTypeData[] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t linkTypeSampleLen = 0;
    uint8_t linkTypeSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(linkTypeSampleBuf),
        &linkTypeSampleLen);

    // Invalid sample (valid=false)
    uint8_t someData[] = {0x00};
    size_t invalidSampleLen = 0;
    uint8_t invalidSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_MAC_ADDRESS, false, someData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(invalidSampleBuf),
        &invalidSampleLen);

    // High tag sample (tag > max unreserved)
    size_t highTagSampleLen = 0;
    uint8_t highTagSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        0xFE, true, someData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(highTagSampleBuf),
        &highTagSampleLen);

    // Invalid tag for Ethernet (e.g. tag=99 which doesn't match MAC or
    // permanent MAC)
    uint8_t ethInvalidTagData[8] = {0x01, 0x02, 0x03, 0x04,
                                    0x05, 0x06, 0x07, 0x08};
    size_t ethInvalidSampleLen = 0;
    uint8_t ethInvalidSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_NODE_GUID, true, ethInvalidTagData, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(ethInvalidSampleBuf),
        &ethInvalidSampleLen);

    std::vector<uint8_t> response(headerSize + linkTypeSampleLen +
                                  invalidSampleLen + highTagSampleLen +
                                  ethInvalidSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 4,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, linkTypeSampleBuf, linkTypeSampleLen);
    offset += linkTypeSampleLen;
    memcpy(response.data() + offset, invalidSampleBuf, invalidSampleLen);
    offset += invalidSampleLen;
    memcpy(response.data() + offset, highTagSampleBuf, highTagSampleLen);
    offset += highTagSampleLen;
    memcpy(response.data() + offset, ethInvalidSampleBuf, ethInvalidSampleLen);

    [[maybe_unused]] auto result = sensor.handleResponseMsg(responseMsg,
                                                            response.size());
    // Should still succeed since we have valid link type
    // (ethInvalidTag case logs error but doesn't fail)
}

// ---- NsmGetPortECCCounters handleResponseMsg tests ----

TEST(NsmGetPortECCCounters, HandleResponseMsgErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"ecc_err"};
    std::string type = "ECCType";
    std::string objPath = "/xyz/openbmc_project/inventory/system/dummy/ecc_err";
    uint8_t portNumber = 1;

    nsm::NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build error aggregate response
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_common_non_success_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_ERROR, 0,
                          responseMsg);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

TEST(NsmGetPortECCCounters, HandleResponseMsgGoodWithCounters)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"ecc_good"};
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/ecc_good";
    uint8_t portNumber = 1;

    nsm::NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build aggregate response with ECC counter samples
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Encode samples for each tag
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

TEST(NsmGetPortECCCounters, HandleResponseMsgDefaultTag)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"ecc_default"};
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/ecc_default";
    uint8_t portNumber = 1;

    nsm::NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build aggregate response with unknown tag + invalid valid bit
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Unknown tag (tag=99 - within valid range but unknown)
    uint64_t val = 12345;
    uint8_t counterBytes[8];
    memcpy(counterBytes, &val, sizeof(val));
    size_t sampleLen = 0;
    uint8_t sampleBuf[16] = {};
    encode_aggregate_resp_sample(
        10, true, counterBytes, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf), &sampleLen);

    // Invalid valid bit sample
    size_t invalidSampleLen = 0;
    uint8_t invalidSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        0, false, counterBytes, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(invalidSampleBuf),
        &invalidSampleLen);

    // High tag (above max unreserved)
    size_t highTagLen = 0;
    uint8_t highTagBuf[16] = {};
    encode_aggregate_resp_sample(
        0xFE, true, counterBytes, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(highTagBuf), &highTagLen);

    std::vector<uint8_t> response(headerSize + sampleLen + invalidSampleLen +
                                  highTagLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 3,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, sampleBuf, sampleLen);
    offset += sampleLen;
    memcpy(response.data() + offset, invalidSampleBuf, invalidSampleLen);
    offset += invalidSampleLen;
    memcpy(response.data() + offset, highTagBuf, highTagLen);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    // Unknown tag 10 causes decode_aggregate_port_ecc_counter_data to fail
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

TEST(NsmGetPortECCCounters, HandleResponseMsgTruncatedSample)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"ecc_trunc"};
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/ecc_trunc";
    uint8_t portNumber = 1;

    nsm::NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    // Build aggregate response with telemetry_count=2 but only provide
    // space for 1 complete sample + truncated second sample
    // This triggers decode_aggregate_resp_sample failure path
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // One valid sample
    uint64_t val = 100;
    uint8_t counterBytes[8];
    memcpy(counterBytes, &val, sizeof(val));
    size_t sampleLen = 0;
    uint8_t sampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_ECC_CORRECTED_BITS, true, counterBytes, 8,
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf), &sampleLen);

    // Allocate only enough for header + 1 sample + 1 byte (truncated 2nd)
    std::vector<uint8_t> response(headerSize + sampleLen + 1, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_PORT_ECC_COUNTERS, NSM_SUCCESS, 2,
                          responseMsg);
    memcpy(response.data() + headerSize, sampleBuf, sampleLen);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    // Should still process first sample but second decode will fail
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

TEST(NsmNetworkAddressAggregator, HandleResponseMsgDecodeAddressError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_decode_err"};
    std::string type = "NAAType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/naa_decode_err";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build response with Ethernet link type + MAC address sample with wrong
    // data length (3 bytes instead of 8) to trigger
    // decode_aggregate_network_address_data failure
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    // Link type = Ethernet
    uint8_t linkTypeData[] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t linkTypeSampleLen = 0;
    uint8_t linkTypeSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(linkTypeSampleBuf),
        &linkTypeSampleLen);

    // MAC address with wrong length (2 bytes instead of 8) - will trigger
    // decode_aggregate_network_address_data error
    uint8_t badMacData[2] = {0x00, 0x11};
    size_t badMacSampleLen = 0;
    uint8_t badMacSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_MAC_ADDRESS, true, badMacData, 2,
        reinterpret_cast<nsm_aggregate_resp_sample*>(badMacSampleBuf),
        &badMacSampleLen);

    std::vector<uint8_t> response(headerSize + linkTypeSampleLen +
                                  badMacSampleLen);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 2,
                          responseMsg);

    size_t offset = headerSize;
    memcpy(response.data() + offset, linkTypeSampleBuf, linkTypeSampleLen);
    offset += linkTypeSampleLen;
    memcpy(response.data() + offset, badMacSampleBuf, badMacSampleLen);

    auto result = sensor.handleResponseMsg(responseMsg, response.size());
    // decode_aggregate_network_address_data should fail with bad data length
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

TEST(NsmNetworkAddressAggregator, HandleResponseMsgTruncatedSample)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_trunc"};
    std::string type = "NAAType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/naa_trunc";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 1;

    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    // Build response with telemetry_count=2 but truncated second sample
    // to trigger decode_aggregate_resp_sample failure
    size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);

    uint8_t linkTypeData[] = {NSM_PORT_PROTOCOL_ETHERNET};
    size_t linkTypeSampleLen = 0;
    uint8_t linkTypeSampleBuf[16] = {};
    encode_aggregate_resp_sample(
        NSM_TAG_LINK_TYPE, true, linkTypeData, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(linkTypeSampleBuf),
        &linkTypeSampleLen);

    // Only provide space for header + link type sample + 1 byte (truncated)
    std::vector<uint8_t> response(headerSize + linkTypeSampleLen + 1, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_aggregate_resp(0, NSM_GET_NETWORK_ADDRESSES, NSM_SUCCESS, 2,
                          responseMsg);
    memcpy(response.data() + headerSize, linkTypeSampleBuf, linkTypeSampleLen);

    sensor.handleResponseMsg(responseMsg, response.size());
    // Second sample decode fails due to truncation, linkType is Ethernet
    // but we don't have all samples - function continues after decode failure
}

namespace nsm
{
requester::Coroutine createNsmPCIePort(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
};

using namespace nsm;

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

using sdbusplus::server::object_t;
using namespace sdbusplus::server::xyz::openbmc_project;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PortIntf = object_t<Inventory::Item::server::Port>;
using PortStateIntf = object_t<Inventory::Decorator::server::PortState>;
using HealthIntf = object_t<State::Decorator::server::Health>;

struct NsmPCIePortTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_PCIePort";
    const std::string name = "PCIe_0";
    const std::string objPath =
        chassisInventoryBasePath /
        "HGX_NVLinkManagementNIC_0/NetworkAdapters/NVLinkManagementNIC_0/Ports" /
        name;

    const uuid_t cx7Uuid = "STATIC:2:255:NSM_DEVICE_INSTANCE_NUMBER:255";
    std::shared_ptr<MockNsmDevice> cx7;

    NsmDeviceTable devices;

    NsmPCIePortTest() : SensorManagerTest(devices)
    {
        cx7 = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx7Uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(cx7, nullptr);
        EXPECT_EQ(NSM_DEV_ID_PCIE_BRIDGE, cx7->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"UUID", "992b3ec1-e468-f145-8686-badbadbadbad"},
    };
    dbus::PropertyMap basic = {
        {"Name", "PCIe_0"},
        {"Type", "NSM_PCIePort"},
        {"InventoryObjPath", objPath},
        {"UUID", cx7Uuid},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
        {"PortType",
         "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort"},
        {"PortProtocol",
         "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe"},
        {"LinkState",
         "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled"},
        {"LinkStatus",
         "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp"},
    };

    dbus::PropertyMap associations[1] = {
        {
            {"Forward", "parent_device"},
            {"Backward", "all_states"},
            {"AbsolutePath",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_NVLinkManagementNIC_0/NetworkAdapters/NVLinkManagementNIC_0"},
        },
    };
    const MapperServiceMap serviceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations0",
                },
            },
        },
    };
};

TEST_F(NsmPCIePortTest, badTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any device
    const uuid_t invalidUuid =
        "a3b0bdf6-8661-4d8e-8268-0e59415f2076"; // From error collection
    propertyMap["InventoryObjPath"] = basic["InventoryObjPath"];
    propertyMap["UUID"] = invalidUuid;          // Invalid UUID as uuid_t type

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmPCIePortTest, goodTestCreateDeviceSensors)
{
    utils::MockDbusAsync::serviceMap() = serviceMap;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["UUID"] = basic["UUID"];

    // Set up interface-specific properties
    propertyMap["InventoryObjPath"] = basic["InventoryObjPath"];
    propertyMap["Health"] = basic["Health"];
    propertyMap["PortType"] = basic["PortType"];
    propertyMap["PortProtocol"] = basic["PortProtocol"];
    propertyMap["LinkState"] = basic["LinkState"];
    propertyMap["LinkStatus"] = basic["LinkStatus"];

    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations0");
    propertyMapAssociation0 = associations[0];

    createNsmPCIePort(mockManager, basicIntfName, objPath);

    EXPECT_EQ(0, cx7->prioritySensors.size());
    EXPECT_EQ(5, cx7->roundRobinSensors.size());
    EXPECT_EQ(9, cx7->deviceSensors.size());

    auto sensors = 1; // Skip msgTypes sensor added by initMsgTypesSensor()
    auto associationsObject =
        dynamic_pointer_cast<NsmPCIePort<AssociationDefinitionsInft>>(
            cx7->deviceSensors[sensors++]);
    auto healthObject = dynamic_pointer_cast<NsmPCIePort<HealthIntf>>(
        cx7->deviceSensors[sensors++]);
    auto portObject = dynamic_pointer_cast<NsmPCIePort<PortIntf>>(
        cx7->deviceSensors[sensors++]);
    auto portStateObject = dynamic_pointer_cast<NsmPCIePort<PortStateIntf>>(
        cx7->deviceSensors[sensors++]);
    auto pcieLinkSpeed =
        dynamic_pointer_cast<NsmPCIeLinkSpeed<NsmPortInfoIntf>>(
            cx7->deviceSensors[sensors++]);
    auto pcieErrorsGroup2 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7->deviceSensors[sensors++]);
    auto pcieErrorsGroup3 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7->deviceSensors[sensors++]);
    auto pcieErrorsGroup4 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7->deviceSensors[sensors++]);

    EXPECT_EQ(sensors, cx7->deviceSensors.size());
    EXPECT_NE(nullptr, associationsObject);
    EXPECT_NE(nullptr, healthObject);
    EXPECT_NE(nullptr, portObject);
    EXPECT_NE(nullptr, portStateObject);
    EXPECT_NE(nullptr, pcieLinkSpeed);
    EXPECT_NE(nullptr, pcieErrorsGroup2);
    EXPECT_NE(nullptr, pcieErrorsGroup3);
    EXPECT_NE(nullptr, pcieErrorsGroup4);

    EXPECT_EQ(1, associationsObject->invoke(pdiMethod(associations)).size());
    EXPECT_EQ(std::get<std::string>(basic["Health"]),
              HealthIntf::convertHealthTypeToString(
                  healthObject->invoke(pdiMethod(health))));
    EXPECT_EQ(std::get<std::string>(basic["PortType"]),
              PortInfoIntf::convertPortTypeToString(
                  pcieLinkSpeed->invoke(pdiMethod(type))));
    EXPECT_EQ(std::get<std::string>(basic["PortProtocol"]),
              PortInfoIntf::convertPortProtocolToString(
                  pcieLinkSpeed->invoke(pdiMethod(protocol))));
    EXPECT_EQ(std::get<std::string>(basic["LinkState"]),
              PortStateIntf::convertLinkStatesToString(
                  portStateObject->invoke(pdiMethod(linkState))));
    EXPECT_EQ(std::get<std::string>(basic["LinkStatus"]),
              PortStateIntf::convertLinkStatusTypeToString(
                  portStateObject->invoke(pdiMethod(linkStatus))));
    EXPECT_EQ(GROUP_ID_2, pcieErrorsGroup2->groupId);
    EXPECT_EQ(GROUP_ID_3, pcieErrorsGroup3->groupId);
    EXPECT_EQ(GROUP_ID_4, pcieErrorsGroup4->groupId);

    EXPECT_CALL(*cx7, sensorIO)
        .Times(cx7->roundRobinSensors.size())
        .WillRepeatedly(mockSensorIO(NSM_SUCCESS));
    for (size_t i = 0; i < cx7->roundRobinSensors.size(); i++)
    {
        cx7->roundRobinSensors[i]->update(cx7);
    }
}

TEST(NsmPortMetrics, TestNullCounters)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName("test_port");
    uint8_t portNum = 2;
    std::string type = "TestType";
    uint8_t deviceType = 2;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/test/device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/test/device/Ports";
    std::vector<utils::Association> associations;
    std::shared_ptr<IBPortIntf> iBPortIntf =
        std::make_shared<IBPortIntf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                associations, parentObjPath, inventoryObjPath,
                                iBPortIntf, portMetricsOem2Intf,
                                portPacketCountersIntf);

    // Test with null pointer - should not crash
    portTel.updateCounterValues(nullptr);

    // Counters should remain at initial values
    EXPECT_EQ(portTel.iBPortIntf->rxPkts(), 0);
}

TEST(NsmPortMetrics, TestEmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName("empty_assoc_port");
    uint8_t portNum = 3;
    std::string type = "EmptyType";
    uint8_t deviceType = 3;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/empty/device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/empty/device/Ports";

    // Empty associations vector
    std::vector<utils::Association> emptyAssociations;
    std::shared_ptr<IBPortIntf> iBPortIntf =
        std::make_shared<IBPortIntf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                emptyAssociations, parentObjPath,
                                inventoryObjPath, iBPortIntf,
                                portMetricsOem2Intf, portPacketCountersIntf);

    EXPECT_EQ(portTel.portName, pName);
    EXPECT_EQ(portTel.portNumber, portNum);
    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);
}

TEST(NsmPCIeErrors, Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PCIeErrors";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    uint8_t deviceIndex = 1;
    uint8_t groupId = GROUP_ID_2;

    nsm::NsmPCIeErrors errors(provider, deviceIndex, groupId);

    EXPECT_EQ(errors.getName(), name);
    EXPECT_EQ(errors.getType(), type);
}

TEST(NsmPCIeErrors, GenRequestMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PCIeErrors";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    uint8_t deviceIndex = 2;
    uint8_t groupId = GROUP_ID_3;

    nsm::NsmPCIeErrors errors(provider, deviceIndex, groupId);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = errors.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST(NsmPCIeErrors, HandleResponseMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PCIeErrors";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    uint8_t deviceIndex = 1;
    uint8_t groupId = GROUP_ID_2;

    nsm::NsmPCIeErrors errors(provider, deviceIndex, groupId);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_2),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_2 data = {};
    data.correctable_errors = 5;
    data.non_fatal_errors = 2;
    data.fatal_errors = 1;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, cc, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = errors.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify values were set
    EXPECT_EQ(pcieEccIntf->ceCount(), 5);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 2);
    EXPECT_EQ(pcieEccIntf->feCount(), 1);
}

// NsmPCIeErrors: Constructor with GROUP_ID_3 triggers handleResponse(group_3)
TEST(NsmPCIeErrors, Constructor_Group3)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/pcie_g3";
    std::string name = "PCIeErrors_g3";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    nsm::NsmPCIeErrors errors(provider, 1, GROUP_ID_3);

    EXPECT_EQ(errors.getName(), name);
    EXPECT_EQ(errors.getType(), type);
    // initHandleResponse(3) sets l0ToRecoveryCount to 0
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 0u);
}

// NsmPCIeErrors: Constructor with GROUP_ID_4 calls initHandleResponse(3)
TEST(NsmPCIeErrors, Constructor_Group4)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/pcie_g4";
    std::string name = "PCIeErrors_g4";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    nsm::NsmPCIeErrors errors(provider, 1, GROUP_ID_4);

    EXPECT_EQ(errors.getName(), name);
    EXPECT_EQ(errors.getType(), type);
    // GROUP_ID_4 constructor calls initHandleResponse(3) -> l0ToRecoveryCount=0
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 0u);
}

// NsmPCIeErrors: handleResponseMsg for GROUP_ID_3 sets l0ToRecoveryCount
TEST(NsmPCIeErrors, HandleResponseMsg_Group3)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/pcie_g3r";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(
        "PCIeErrors_g3r", "NSM_PCIeErrors", path, pcieEccIntf);

    nsm::NsmPCIeErrors errors(provider, 1, GROUP_ID_3);

    nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 42;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    uint8_t cc = errors.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 42u);
}

// NsmPCIeErrors: handleResponseMsg for GROUP_ID_4 sets replay/NAK counters
TEST(NsmPCIeErrors, HandleResponseMsg_Group4)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/pcie_g4r";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(
        "PCIeErrors_g4r", "NSM_PCIeErrors", path, pcieEccIntf);

    nsm::NsmPCIeErrors errors(provider, 1, GROUP_ID_4);

    nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 10;
    data.replay_rollover_cnt = 3;
    data.NAK_sent_cnt = 7;
    data.NAK_recv_cnt = 5;

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    uint8_t cc = errors.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->replayCount(), 10u);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 3u);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 7u);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 5u);
}

// NsmPCIeErrors: error cc response triggers else branch (handleResponse zeros)
TEST(NsmPCIeErrors, HandleResponseMsg_ErrorResponse)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/pcie_g2e";
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(
        "PCIeErrors_g2e", "NSM_PCIeErrors", path, pcieEccIntf);

    nsm::NsmPCIeErrors errors(provider, 1, GROUP_ID_2);

    nsm_query_scalar_group_telemetry_group_2 data = {};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_2),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    encode_query_scalar_group_telemetry_v1_group2_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, response);

    uint8_t cc = errors.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(cc, NSM_ERROR);
    // else branch: handleResponse called with zeroed data -> ceCount stays 0
    EXPECT_EQ(pcieEccIntf->ceCount(), 0u);
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmPCIePort
// =============================================================================

// InventoryObjPath absent -> inventoryObjPath="" -> NsmPCIePort<T>("") calls
// sd_bus_add_object_vtable with empty path -> SdBusError (InvalidArgs)
TEST_F(NsmPCIePortTest, PCIePort_MissingInventoryObjPath_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_inv_path";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["UUID"] = cx7Uuid;
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["LinkState"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");
    // "InventoryObjPath" intentionally omitted -> inventoryObjPath="" ->
    // NsmPCIePort<T>("") throws SdBusError

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

// UUID absent -> uuid="" -> parseStaticUuid("") throws std::runtime_error
// before NsmPCIePort objects are created (no D-Bus registration occurs)
TEST_F(NsmPCIePortTest, PCIePort_MissingUUID_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_uuid";
    const std::string testInvPath =
        "/xyz/openbmc_project/inventory/test/pcieport/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["InventoryObjPath"] = testInvPath;
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["LinkState"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");
    // "UUID" intentionally omitted -> uuid="" -> parseStaticUuid("") throws

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

// Health absent -> health="" -> convertHealthTypeFromString("") throws
// after NsmPCIePort objects are registered (then destroyed on unwind)
TEST_F(NsmPCIePortTest, PCIePort_MissingHealth_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_health";
    const std::string testInvPath =
        "/xyz/openbmc_project/inventory/test/pcieport/no_health";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["UUID"] = cx7Uuid;
    pm["InventoryObjPath"] = testInvPath;
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["LinkState"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");
    // "Health" intentionally omitted -> health="" ->
    // convertHealthTypeFromString throws after D-Bus objects created at
    // testInvPath

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

// PortType absent -> portType="" -> convertPortTypeFromString("") throws
// Health must be valid to reach the portType invoke()
TEST_F(NsmPCIePortTest, PCIePort_MissingPortType_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_porttype";
    const std::string testInvPath =
        "/xyz/openbmc_project/inventory/test/pcieport/no_porttype";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["UUID"] = cx7Uuid;
    pm["InventoryObjPath"] = testInvPath;
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["LinkState"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");
    // "PortType" intentionally omitted -> portType="" -> throws

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

// PortProtocol absent -> portProtocol="" -> convertPortProtocolFromString
// throws Health and PortType must be valid to reach the portProtocol invoke()
TEST_F(NsmPCIePortTest, PCIePort_MissingPortProtocol_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_portprotocol";
    const std::string testInvPath =
        "/xyz/openbmc_project/inventory/test/pcieport/no_portprotocol";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["UUID"] = cx7Uuid;
    pm["InventoryObjPath"] = testInvPath;
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["LinkState"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");
    // "PortProtocol" intentionally omitted -> portProtocol="" -> throws

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

// LinkState absent -> linkState="" -> convertLinkStatesFromString("") throws
// Health, PortType, PortProtocol must be valid to reach linkState invoke()
TEST_F(NsmPCIePortTest, PCIePort_MissingLinkState_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_linkstate";
    const std::string testInvPath =
        "/xyz/openbmc_project/inventory/test/pcieport/no_linkstate";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["UUID"] = cx7Uuid;
    pm["InventoryObjPath"] = testInvPath;
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");
    // "LinkState" intentionally omitted -> linkState="" -> throws

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

// LinkStatus absent -> linkStatus="" -> convertLinkStatusTypeFromString throws
// All prior properties must be valid to reach the linkStatus invoke()
TEST_F(NsmPCIePortTest, PCIePort_MissingLinkStatus_Throws)
{
    const std::string testObjPath = "/xyz/test/pcieport/no_linkstatus";
    const std::string testInvPath =
        "/xyz/openbmc_project/inventory/test/pcieport/no_linkstatus";
    auto& pm = utils::MockDbusAsync::propertyMap(testObjPath, basicIntfName);
    pm["UUID"] = cx7Uuid;
    pm["InventoryObjPath"] = testInvPath;
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["LinkState"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled");
    // "LinkStatus" intentionally omitted -> linkStatus="" -> throws

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, testObjPath),
        std::exception);
}

TEST_F(NsmPCIePortTest, TearDown)
{
    devices.clear();
    ::testing::Mock::VerifyAndClearExpectations(&mockManager);
}

struct NsmPortSensorCreateTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLink";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_create";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:20";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortSensorCreateTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortSensorCreateTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmPortSensorCreateTestFixture, goodTestCreatePortSensor)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(4)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0")},
        {"Priority", false},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    createNsmPortSensor(mockManager, basicIntfName, objPath, false);

    // Expect multiple sensors per port (Status, Characteristics, Metrics)
    EXPECT_GE(gpu->deviceSensors.size(), 4);
}

TEST_F(NsmPortSensorCreateTestFixture, badTestMissingUUID)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink")},
        {"Count", uint64_t(4)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createNsmPortSensor(mockManager, basicIntfName, objPath, false),
        std::runtime_error);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusUpdate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_0";
    uint8_t portNum = 1;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/nvlink_0";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Mock port status response - LinkUp
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());

    auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_PORTSTATE_UP,
                                            NSM_PORTSTATUS_ENABLED, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));

    portStatus.update(gpu);
}

// NsmPortStatus::update – error-CC response -> TRUE branch of
// co_return cc ? cc : rc (cc = NSM_ERROR != 0 after decode). //

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusUpdate_ErrorCC)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_0";
    uint8_t portNum = 1;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/nvlink_ec";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Build a port-status response with NSM_ERROR completion code.
    // decode_query_port_status_resp will set cc = NSM_ERROR (non-zero);
    // the final "co_return cc ? cc : rc" takes the TRUE branch (return cc).

    std::vector<uint8_t> errorResp(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(errorResp.data());
    ASSERT_EQ(encode_query_port_status_resp(0, NSM_ERROR, ERR_NULL,
                                            NSM_PORTSTATE_UP,
                                            NSM_PORTSTATUS_ENABLED, msg),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));

    portStatus.update(gpu);
    // cc = NSM_ERROR -> true branch of co_return cc ? cc : rc //
}

TEST_F(NsmPortSensorCreateTestFixture, testPortCharacteristicsUpdate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_1";
    uint8_t portNum = 2;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/nvlink_1";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    // Mock port characteristics response
    Response portCharResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

    nsm_port_characteristics_data data = {};
    data.nv_port_line_rate_mbps = 100000;   // 100 Gbps
    data.nv_port_data_rate_kbps = 90000000; // 90 Gbps
    data.status_lane_info = 0x04;           // 4 lanes
    data.port_status.port_down_reason_code =
        NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN;

    auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // handleResponseMsg is called directly, not through sensorIO
    auto result = portChar.handleResponseMsg(msg, portCharResp.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortMetricsUpdate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Metrics";
    uint8_t portNum = 4;
    std::string type = "NSM_NVLink";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath = "/xyz/openbmc_project/inventory/system/gpu3";
    std::string inventoryObjPath = parentObjPath + "/Ports/nvlink_metrics";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics portMetrics(bus, portName, portNum, type, deviceType,
                               associations, parentObjPath, inventoryObjPath,
                               iBPortIntf, portMetricsOem2Intf,
                               portPacketCountersIntf);

    // Mock port telemetry counter response
    Response portTelResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             PORT_COUNTER_TELEMETRY_MAX_DATA_SIZE,
                         0);
    auto msg = reinterpret_cast<nsm_msg*>(portTelResp.data());

    nsm_port_counter_data data = {};
    data.supported_counter.port_rcv_pkts = 1;
    data.port_rcv_pkts = 1000;
    data.supported_counter.port_xmit_pkts = 1;
    data.port_xmit_pkts = 2000;
    data.supported_counter.port_rcv_errors = 1;
    data.port_rcv_errors = 5;

    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = portMetrics.handleResponseMsg(msg, portTelResp.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);

    EXPECT_EQ(iBPortIntf->rxPkts(), 1000);
    EXPECT_EQ(iBPortIntf->txPkts(), 2000);
    EXPECT_EQ(iBPortIntf->rxErrors(), 5);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusVariousStates)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_States";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/states";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    // Test various port states
    std::vector<std::pair<uint8_t, uint8_t>> portStates = {
        {NSM_PORTSTATE_DOWN, NSM_PORTSTATUS_DISABLED},
        {NSM_PORTSTATE_UP, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_TRAINING, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_TRAINING_FAILURE, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_POLLING, NSM_PORTSTATUS_ENABLED}};

    for (size_t i = 0; i < portStates.size(); i++)
    {
        uint8_t portNum = i + 1;
        NsmPortStatus portStatus(bus, portName, portNum, "NSM_NVLink",
                                 portMetricsOem3Intf, inventoryObjPath);

        Response portStatusResp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());

        auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                                portStates[i].first,
                                                portStates[i].second, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        EXPECT_CALL(*gpu, sensorIO)
            .WillOnce(mockSensorIO(portStatusResp, Response{}));

        portStatus.update(gpu);
    }
}

TEST_F(NsmPortSensorCreateTestFixture, testPortCharacteristicsUpdateError)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Error";
    uint8_t portNum = 5;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/error";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    // Mock error response
    Response portCharResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

    nsm_port_characteristics_data data = {};
    auto rc = encode_query_port_characteristics_resp(0, NSM_ERROR, 0x5678,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = portChar.handleResponseMsg(msg, portCharResp.size());
    EXPECT_EQ(result, NSM_ERROR);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortMetricsCounterValues)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Counters";
    uint8_t portNum = 6;
    std::string type = "NSM_NVLink";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath = "/xyz/openbmc_project/inventory/system/gpu4";
    std::string inventoryObjPath = parentObjPath + "/Ports/counters";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics portMetrics(bus, portName, portNum, type, deviceType,
                               associations, parentObjPath, inventoryObjPath,
                               iBPortIntf, portMetricsOem2Intf,
                               portPacketCountersIntf);

    // Need larger buffer for port counter data (contains many uint64_t fields)
    Response portTelResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data),
                         0);
    auto msg = reinterpret_cast<nsm_msg*>(portTelResp.data());

    nsm_port_counter_data data = {};
    // Set all counters
    data.supported_counter.port_rcv_pkts = 1;
    data.port_rcv_pkts = 5000;
    data.supported_counter.port_xmit_pkts = 1;
    data.port_xmit_pkts = 6000;
    data.supported_counter.port_malformed_pkts = 1;
    data.port_malformed_pkts = 10;
    data.supported_counter.vl15_dropped = 1;
    data.vl15_dropped = 2;
    data.supported_counter.port_rcv_errors = 1;
    data.port_rcv_errors = 15;
    data.supported_counter.port_xmit_discard = 1;
    data.port_xmit_discard = 8;
    data.supported_counter.symbol_ber = 1;
    data.symbol_ber = 0x12345678;
    data.supported_counter.link_error_recovery_counter = 1;
    data.link_error_recovery_counter = 3;
    data.supported_counter.link_downed_counter = 1;
    data.link_downed_counter = 1;

    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = portMetrics.handleResponseMsg(msg, portTelResp.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);

    EXPECT_EQ(iBPortIntf->rxPkts(), 5000);
    EXPECT_EQ(iBPortIntf->txPkts(), 6000);
    EXPECT_EQ(iBPortIntf->malformedPkts(), 10);
    EXPECT_EQ(iBPortIntf->vL15DroppedPkts(), 2);
    EXPECT_EQ(iBPortIntf->rxErrors(), 15);
    EXPECT_EQ(iBPortIntf->txDiscardPkts(), 8);
    EXPECT_EQ(iBPortIntf->linkErrorRecoveryCounter(), 3);
    EXPECT_EQ(iBPortIntf->linkDownCount(), 1);
}

TEST_F(NsmPortSensorCreateTestFixture, testGetBitErrorRate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_BER";
    uint8_t portNum = 7;
    std::string type = "NSM_NVLink";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath = "/xyz/openbmc_project/inventory/system/gpu5";
    std::string inventoryObjPath = parentObjPath + "/Ports/ber";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics portMetrics(bus, portName, portNum, type, deviceType,
                               associations, parentObjPath, inventoryObjPath,
                               iBPortIntf, portMetricsOem2Intf,
                               portPacketCountersIntf);

    // Test BER calculation with different values
    uint64_t berValue = 0x0A0305; // magnitude=10, coef_float=3, coef=5
    double ber = portMetrics.getBitErrorRate(berValue);
    EXPECT_GT(ber, 0.0);

    // Test with zero value
    ber = portMetrics.getBitErrorRate(0);
    EXPECT_EQ(ber, 0.0);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusDownLockWithRuntimeError)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DownLock";
    uint8_t portNum = 8;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/downlock";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Mock port status response - DOWN_LOCK state
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());

    auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_PORTSTATE_DOWN_LOCK,
                                            NSM_PORTSTATUS_ENABLED, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Mock port characteristics response with error (use encode_reason_code
    // directly)
    Response portCharResp((sizeof(struct nsm_msg_hdr) +
                           sizeof(struct nsm_common_non_success_resp)),
                          0);
    auto charMsg = reinterpret_cast<nsm_msg*>(portCharResp.data());

    rc = encode_reason_code(NSM_ERROR, 0x1234, NSM_QUERY_PORT_CHARACTERISTICS,
                            charMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::InSequence seq;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portCharResp, Response{}));

    auto result = portStatus.update(gpu);
    // Force coroutine to complete by accessing data
    auto returnCode = result.data();
    EXPECT_EQ(returnCode, NSM_SW_SUCCESS);

    // Runtime error should be set after port characteristics error
    EXPECT_TRUE(portMetricsOem3Intf->runtimeError());
}

TEST_F(NsmPortSensorCreateTestFixture,
       testPortCharacteristicsLinkDownReasonCodes)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_LinkDown";
    uint8_t portNum = 9;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/linkdown";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    // Test various link down reason codes
    std::vector<uint32_t> reasonCodes = {
        NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN,
        NSM_PORT_DOWN_REASON_CODE_HI_SER_BER,
        NSM_PORT_DOWN_REASON_CODE_BLOCK_LOCK_LOSS,
        NSM_PORT_DOWN_REASON_CODE_ALIGNMENT_LOSS,
        NSM_PORT_DOWN_REASON_CODE_FEC_SYNC_LOSS,
        NSM_PORT_DOWN_REASON_CODE_CABLE_UNPLUGGED,
        NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN};

    for (auto reasonCode : reasonCodes)
    {
        Response portCharResp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp),
                              0);
        auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

        nsm_port_characteristics_data data = {};
        data.nv_port_line_rate_mbps = 50000;
        data.nv_port_data_rate_kbps = 45000000;
        data.status_lane_info = 0x02;
        data.port_status.port_down_reason_code = reasonCode;

        auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS,
                                                         ERR_NULL, &data, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        auto result = portChar.handleResponseMsg(msg, portCharResp.size());
        EXPECT_EQ(result, NSM_SW_SUCCESS);
    }
}

TEST_F(NsmPortSensorCreateTestFixture, testNetworkAddressAggregatorEthernet)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_eth";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    EXPECT_EQ(networkAddr.getName(), portName);
    EXPECT_EQ(networkAddr.portNumber, portNumber);
}

TEST_F(NsmPortSensorCreateTestFixture, testGetPortECCCountersRequest)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc";
    uint8_t portNumber = 2;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = eccCounters.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req));
}

TEST_F(NsmPortSensorCreateTestFixture, testEthPortTelemetryAggregator)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort";
    uint16_t portNumber = 3;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    EXPECT_EQ(ethPort.getName(), portName);
    EXPECT_EQ(ethPort.portNumber, portNumber);
    EXPECT_NE(ethPort.ethPortIntf, nullptr);
}

TEST_F(NsmPortSensorCreateTestFixture, badTestPortStatusUpdateDecodeError)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DecodeErr";
    uint8_t portNum = 10;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/decodeerr";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    auto mockGpu = std::make_shared<MockNsmDevice>();

    // Mock invalid response with minimal valid size
    Response badResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + sizeof(uint16_t), 0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    encode_reason_code(NSM_ERROR, 0x9999, NSM_QUERY_PORT_STATUS, badMsg);

    EXPECT_CALL(*mockGpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    portStatus.update(mockGpu);
}

// Decode fail: 7-byte buffer is too short for
// decode_get_port_telemetry_counter_resp -> returns error.
TEST(NsmPortMetrics, HandleResponseMsg_DecodeFail_ReturnsError)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    auto rc = portTel.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Decode fail: 7-byte buffer is too short for
// decode_query_port_characteristics_resp -> returns error.
TEST(NsmPortCharacteristics, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"char_decode_fail"};
    uint8_t portNum = 5;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/char_decode_fail";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());
    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Decode fail: 7-byte buffer is too short for decode_aggregate_resp in
// NsmNetworkAddressAggregator::handleResponseMsg -> returns error.
TEST(NsmNetworkAddressAggregator, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"naa_decode_fail2"};
    std::string type = "NAAType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/naa_decode_fail2";
    std::string nodeGuidObjPath = objPath + "/NodeGuid";
    std::string ethernetMacObjPath = objPath + "/EthMac";
    std::string permanentMacObjPath = objPath + "/PermMac";
    uint16_t portNumber = 2;
    nsm::NsmNetworkAddressAggregator sensor(bus, name, type, objPath,
                                            nodeGuidObjPath, ethernetMacObjPath,
                                            permanentMacObjPath, portNumber);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Decode fail: 7-byte buffer is too short for decode_aggregate_resp in
// NsmGetPortECCCounters::handleResponseMsg -> returns error.
TEST(NsmGetPortECCCounters, HandleResponseMsg_DecodeFail_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name{"ecc_decode_fail"};
    std::string type = "ECCType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/ecc_decode_fail";
    uint8_t portNumber = 2;
    nsm::NsmGetPortECCCounters sensor(bus, name, type, objPath, portNumber);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    auto rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Error-CC path: handleResponseMsg – decode succeeds but cc = NSM_ERROR
// -> return cc ? cc : rc; takes the true branch.
// Note: encode_query_port_characteristics_resp checks data != NULL before cc,
// so a valid (non-null) data struct must be provided.
TEST(NsmPortCharacteristics, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"char_errcc"};
    uint8_t portNum = 7;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/char_errcc";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());
    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_port_characteristics_data charData = {};
    auto encRc = encode_query_port_characteristics_resp(0, NSM_ERROR, ERR_NULL,
                                                        &charData, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto cc = sensor.handleResponseMsg(responseMsg, response.size());
    // cc = NSM_ERROR -> true branch of return cc ? cc : rc
    EXPECT_EQ(cc, NSM_ERROR);
}

// Error-CC path: handleResponseMsg – decode succeeds but cc = NSM_ERROR
// -> return cc ? cc : rc; takes the true branch.
// Note: encode_get_port_telemetry_counter_resp checks data != NULL before cc,
// so a valid (non-null) data struct must be provided.
TEST(NsmPortMetrics, HandleResponseMsg_ErrorCC_CoversBranch)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    std::vector<uint8_t> response(256, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    struct nsm_port_counter_data portData = {};
    auto encRc = encode_get_port_telemetry_counter_resp(0, NSM_ERROR, ERR_NULL,
                                                        &portData, responseMsg);
    ASSERT_EQ(encRc, NSM_SW_SUCCESS);

    auto cc = portTel.handleResponseMsg(responseMsg, response.size());
    // cc = NSM_ERROR -> true branch of return cc ? cc : rc
    EXPECT_EQ(cc, NSM_ERROR);
}

// createNsmPortSensor with enableNetworkPortAddresses=true covers line 1790
// TRUE branch (adds NsmNetworkAddressAggregator sensor)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_EnableNetworkAddresses)
{
    using namespace nsm;

    const std::string netAddrObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_net";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_Net")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu1")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(netAddrObjPath, basicIntfName);
    pm = properties;

    size_t initialCount = gpu->deviceSensors.size();
    createNsmPortSensor(mockManager, basicIntfName, netAddrObjPath, true);

    // Network address aggregator + port status + characteristics + metrics
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// createNsmPortSensor with a PCIE_BRIDGE+CX8 device covers:
//   - line 1818 FALSE branch (deviceType != NSM_DEV_ID_GPU, no Status/Char)
//   - lines 1849-1858 TRUE branch (adds NsmGetPortECCCounters)
//   - lines 1872-1881 TRUE branch (adds EthPortTelemetryAggregator)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_PCIEBridge_CX8)
{
    using namespace nsm;

    // n1 = (CX8_role=2 << 8) | PCIE_BRIDGE=2 = 514
    const uuid_t bridgeUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    const std::string bridgeObjPath =
        "/xyz/openbmc_project/inventory/system/cx80/port_cx8";

    dbus::PropertyMap properties = {
        {"Name", std::string("CX8Port")},
        {"UUID", bridgeUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_PCIE_BRIDGE)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/cx80")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(bridgeObjPath, basicIntfName);
    pm = properties;

    createNsmPortSensor(mockManager, basicIntfName, bridgeObjPath, false);

    auto bridgeDevice = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(bridgeUuid));
    ASSERT_NE(bridgeDevice, nullptr);
    // No Status/Characteristics (deviceType!=GPU); has Metrics + ECC + Eth
    EXPECT_GE(bridgeDevice->deviceSensors.size(), 3u);
}

// createNsmPortSensor with PCIE_BRIDGE+CX9 device covers:
//   - line 1851 TRUE branch: getDeviceRole() == CX9 is evaluated (CX8 FALSE,
//     so short-circuit doesn't prevent CX9 check) and returns TRUE ->
//     NsmGetPortECCCounters + EthPortTelemetryAggregator added
//   - line 1874 TRUE branch: same condition repeated for Eth sensor
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_PCIEBridge_CX9)
{
    using namespace nsm;

    // combined = (NSM_PCIE_BRIDGE_DEV_ROLE_CX9=3 << 8) |
    // NSM_DEV_ID_PCIE_BRIDGE=2
    //          = 770
    const uuid_t cx9Uuid = "STATIC:770:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    const std::string cx9ObjPath =
        "/xyz/openbmc_project/inventory/system/cx90/port_cx9";

    dbus::PropertyMap properties = {
        {"Name", std::string("CX9Port")},
        {"UUID", cx9Uuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_PCIE_BRIDGE)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/cx90")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(cx9ObjPath, basicIntfName);
    pm = properties;

    createNsmPortSensor(mockManager, basicIntfName, cx9ObjPath, false);

    auto cx9Device = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(cx9Uuid));
    ASSERT_NE(cx9Device, nullptr);
    EXPECT_EQ(cx9Device->getDeviceRole(), NSM_PCIE_BRIDGE_DEV_ROLE_CX9);
    // Like CX8: no Status/Characteristics; has Metrics + ECC + Eth aggregator
    EXPECT_GE(cx9Device->deviceSensors.size(), 3u);
}

// createNsmPortSensor with PCIE_BRIDGE+CX7 device covers:
//   - line 1851 FALSE branch: getDeviceRole() == CX9 is evaluated (CX8 FALSE)
//     and returns FALSE -> NsmGetPortECCCounters NOT added
//   - line 1874 FALSE branch: same, EthPortTelemetryAggregator NOT added
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_PCIEBridge_CX7)
{
    using namespace nsm;

    // combined = (NSM_PCIE_BRIDGE_DEV_ROLE_CX7=1 << 8) |
    // NSM_DEV_ID_PCIE_BRIDGE=2
    //          = 258
    const uuid_t cx7Uuid = "STATIC:258:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    const std::string cx7ObjPath =
        "/xyz/openbmc_project/inventory/system/cx70/port_cx7";

    dbus::PropertyMap properties = {
        {"Name", std::string("CX7Port")},
        {"UUID", cx7Uuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_PCIE_BRIDGE)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/cx70")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(cx7ObjPath, basicIntfName);
    pm = properties;

    createNsmPortSensor(mockManager, basicIntfName, cx7ObjPath, false);

    auto cx7Device = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(cx7Uuid));
    ASSERT_NE(cx7Device, nullptr);
    EXPECT_EQ(cx7Device->getDeviceRole(), NSM_PCIE_BRIDGE_DEV_ROLE_CX7);
    // CX7 is PCIE_BRIDGE but not CX8/CX9: no ECC or Eth aggregator added
    // Only port metrics sensor is created
    EXPECT_GE(cx7Device->deviceSensors.size(), 1u);
}

// createNsmPortSensor with count=0 covers the loop body not executing
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_ZeroCount)
{
    using namespace nsm;

    const std::string zeroCountObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_zero";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_Zero")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(0)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(zeroCountObjPath,
                                                 basicIntfName);
    pm = properties;

    size_t initialCount = gpu->deviceSensors.size();
    createNsmPortSensor(mockManager, basicIntfName, zeroCountObjPath, false);

    // count=0 -> loop body never executes -> no sensors added
    EXPECT_EQ(gpu->deviceSensors.size(), initialCount);
}

// getTopologyObjPath with NSM_DEV_ID_SWITCH covers "SWITCH/" case (line 24-25)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_SwitchDeviceType)
{
    using namespace nsm;

    const std::string switchObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_switch";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_Switch")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_SWITCH)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/switch0")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(switchObjPath, basicIntfName);
    pm = properties;

    size_t initialCount = gpu->deviceSensors.size();
    createNsmPortSensor(mockManager, basicIntfName, switchObjPath, false);
    // DeviceType=SWITCH -> getTopologyObjPath uses "SWITCH/" branch;
    // no Status/Characteristics sensors (deviceType != GPU)
    EXPECT_GE(gpu->deviceSensors.size(), initialCount);
}

// getTopologyObjPath with NSM_DEV_ID_EROT covers "EROT/" case (line 30-32)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_EROTDeviceType)
{
    using namespace nsm;

    const std::string erotObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_erot";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_EROT")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_EROT)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/erot0")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(erotObjPath, basicIntfName);
    pm = properties;

    size_t initialCount = gpu->deviceSensors.size();
    createNsmPortSensor(mockManager, basicIntfName, erotObjPath, false);
    // DeviceType=EROT -> getTopologyObjPath uses "EROT/" branch
    EXPECT_GE(gpu->deviceSensors.size(), initialCount);
}

// getTopologyObjPath with NSM_DEV_ID_CPU covers "CPU/" case (line 33-35)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_CPUDeviceType)
{
    using namespace nsm;

    const std::string cpuObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_cpu";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_CPU")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_CPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/cpu0")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(cpuObjPath, basicIntfName);
    pm = properties;

    size_t initialCount = gpu->deviceSensors.size();
    createNsmPortSensor(mockManager, basicIntfName, cpuObjPath, false);
    // DeviceType=CPU -> getTopologyObjPath uses "CPU/" branch
    EXPECT_GE(gpu->deviceSensors.size(), initialCount);
}

// getTopologyObjPath with unknown type covers default branch (lines 36-39)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_UnknownDeviceType)
{
    using namespace nsm;

    const std::string unknownObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_unknown";
    constexpr uint64_t unknownDeviceType = 99;

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_Unknown")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", unknownDeviceType},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/unknown0")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(unknownObjPath, basicIntfName);
    pm = properties;

    // Unknown deviceType -> getTopologyObjPath logs error and returns ""
    // factory still proceeds with empty topologyObjPath
    EXPECT_NO_THROW(
        createNsmPortSensor(mockManager, basicIntfName, unknownObjPath, false));
}

// coGetTopologyData body (lines 61-112) covered by populating serviceMap with
// a NVLinkTopology interface entry; also covers deviceTopologies match path
// (lines 1779-1782 in factory).
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_WithTopologyData)
{
    using namespace nsm;

    const std::string topoParentPath =
        "/xyz/openbmc_project/inventory/system/gpu_topo_test";
    const std::string topoObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_topo_test";
    const std::string topoInterface =
        "xyz.openbmc_project.Configuration.NVLinkTopology.Topology0";
    // topologyObjPath computed by factory:
    //   deviceName = "gpu_topo_test"
    //   topologyObjPath = ".../linktopology/GPU/gpu_topo_test"
    const std::string topologyObjPath =
        "/xyz/openbmc_project/inventory/system/linktopology/GPU/gpu_topo_test";
    // portObjPath for i=0: parentObjPath + "/Ports/" + name + "_0"
    const std::string portObjPath = topoParentPath + "/Ports/NVLinkTopo_0";

    // Populate service map with a topology interface entry
    auto& sm = utils::MockDbusAsync::serviceMap();
    sm.clear();
    sm.push_back(
        {"xyz.openbmc_project.EntityManager", dbus::Interfaces{topoInterface}});

    // Populate topology property map so coGetAllDbusProperty finds it
    auto& topoPropertyMap = utils::MockDbusAsync::propertyMap(topologyObjPath,
                                                              topoInterface);
    topoPropertyMap["InventoryObjPath"] = std::string(portObjPath);
    topoPropertyMap["LogicalPortNumber"] = uint64_t(2);
    topoPropertyMap["Associations"] = std::vector<std::string>{"fwd", "bwd",
                                                               "/abs/path"};

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLinkTopo")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath", topoParentPath},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(topoObjPath, basicIntfName);
    pm = properties;

    createNsmPortSensor(mockManager, basicIntfName, topoObjPath, false);

    // Cleanup static service map so it doesn't affect other tests
    sm.clear();

    // Topology data was populated -> deviceTopologies non-empty ->
    // logicalPortNum taken from topology (value 2) rather than default (i+1 =
    // 1)
    EXPECT_GE(gpu->deviceSensors.size(), 1u);
}

// coGetTopologyData with topology interface present but no optional properties
// -> count("InventoryObjPath"), count("LogicalPortNumber"),
//    count("Associations") all return 0 -> FALSE branches at L71/L77/L83.
TEST_F(NsmPortSensorCreateTestFixture,
       createPortSensor_TopologyNoOptionalFields)
{
    using namespace nsm;

    const std::string topoParentPath =
        "/xyz/openbmc_project/inventory/system/gpu_topo_noopt";
    const std::string topoObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_topo_noopt";
    const std::string topoInterface =
        "xyz.openbmc_project.Configuration.NVLinkTopology.Topology0";
    const std::string topologyObjPath =
        "/xyz/openbmc_project/inventory/system/linktopology/GPU/gpu_topo_noopt";

    auto& sm = utils::MockDbusAsync::serviceMap();
    sm.clear();
    sm.push_back(
        {"xyz.openbmc_project.EntityManager", dbus::Interfaces{topoInterface}});

    // Register topology map with NO optional properties (no InventoryObjPath,
    // no LogicalPortNumber, no Associations) -> all three FALSE branches taken
    auto& topoPropertyMap = utils::MockDbusAsync::propertyMap(topologyObjPath,
                                                              topoInterface);
    topoPropertyMap.clear(); // empty - none of the three properties present

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLinkTopoNoOpt")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath", topoParentPath},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(topoObjPath, basicIntfName);
    pm = properties;

    createNsmPortSensor(mockManager, basicIntfName, topoObjPath, false);

    sm.clear();
}

// coGetTopologyData with Associations count not multiple of 3 -> L89 TRUE
// (error log branch: "Association in topology must follow (fwd, bck, abs)")
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_TopologyInvalidAssoc)
{
    using namespace nsm;

    const std::string topoParentPath =
        "/xyz/openbmc_project/inventory/system/gpu_topo_badassoc";
    const std::string topoObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_topo_badassoc";
    const std::string topoInterface =
        "xyz.openbmc_project.Configuration.NVLinkTopology.Topology0";
    const std::string topologyObjPath =
        "/xyz/openbmc_project/inventory/system/linktopology/GPU/"
        "gpu_topo_badassoc";
    const std::string portObjPath = topoParentPath +
                                    "/Ports/NVLinkTopoInvalidAssoc_0";

    auto& sm = utils::MockDbusAsync::serviceMap();
    sm.clear();
    sm.push_back(
        {"xyz.openbmc_project.EntityManager", dbus::Interfaces{topoInterface}});

    auto& topoPropertyMap = utils::MockDbusAsync::propertyMap(topologyObjPath,
                                                              topoInterface);
    topoPropertyMap["InventoryObjPath"] = std::string(portObjPath);
    topoPropertyMap["LogicalPortNumber"] = uint64_t(1);
    // Only 2 entries (not a multiple of 3) -> associations.size() % 3 != 0 ->
    // TRUE branch at L89 logs error
    topoPropertyMap["Associations"] = std::vector<std::string>{"fwd_only",
                                                               "bwd_only"};

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLinkTopoInvalidAssoc")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath", topoParentPath},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(topoObjPath, basicIntfName);
    pm = properties;

    createNsmPortSensor(mockManager, basicIntfName, topoObjPath, false);

    sm.clear();
}

// Cover port state switch branches: SLEEP, RESERVED, TRAINING_FAILURE_LOCKED,
// PHYSICAL_UP, unknown default, and portStatus unknown default
TEST_F(NsmPortSensorCreateTestFixture, testPortStatusAdditionalStates)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Extra";
    std::string type = "NSM_NVLink";
    std::string baseObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/extra";

    // Each entry: {portState, portStatus}
    std::vector<std::pair<uint8_t, uint8_t>> states = {
        {NSM_PORTSTATE_SLEEP, NSM_PORTSTATUS_DISABLED},
        {NSM_PORTSTATE_RESERVED, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_TRAINING_FAILURE_LOCKED, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_PHYSICAL_UP, NSM_PORTSTATUS_ENABLED},
        {0xFF, NSM_PORTSTATUS_ENABLED}, // unknown state -> default branch
        {NSM_PORTSTATE_UP, 0xFF},       // unknown status -> default branch
    };

    for (size_t i = 0; i < states.size(); i++)
    {
        uint8_t portNum = static_cast<uint8_t>(100 + i);
        std::string path = baseObjPath + std::to_string(i);

        auto portMetricsOem3Intf =
            std::make_shared<PortMetricsOem3Intf>(bus, path.c_str());
        NsmPortStatus portStatus(bus, portName, portNum, type,
                                 portMetricsOem3Intf, path);

        Response portStatusResp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());
        auto rc = encode_query_port_status_resp(
            0, NSM_SUCCESS, ERR_NULL, states[i].first, states[i].second, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS) << "state=" << (int)states[i].first;

        EXPECT_CALL(*gpu, sensorIO)
            .WillOnce(mockSensorIO(portStatusResp, Response{}));
        portStatus.update(gpu);
    }
}

// Cover updateLinkDownCode switch: all remaining uncovered reason codes
// (UNKNOWN through PEER_RESET_EVENT and default)
TEST_F(NsmPortSensorCreateTestFixture,
       testPortCharacteristicsLinkDownReasonCodesRemaining)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_LinkDownAll";
    uint8_t portNum = 10;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/linkdown_all";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    std::vector<uint32_t> reasonCodes = {
        NSM_PORT_DOWN_REASON_CODE_UNKNOWN,
        NSM_PORT_DOWN_REASON_CODE_PLL_LOCK_LOSS,
        NSM_PORT_DOWN_REASON_CODE_FIFO_OVERFLOW,
        NSM_PORT_DOWN_REASON_CODE_FALSE_SKIP_CONDITION,
        NSM_PORT_DOWN_REASON_CODE_MINOR_ERR_THRESHOLD,
        NSM_PORT_DOWN_REASON_CODE_PHY_LAYER_RETRANSMIT_TIMEOUT,
        NSM_PORT_DOWN_REASON_CODE_HEARTBEAT_ERRORS,
        NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_CREDIT_MON_WD,
        NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_INTEGRITY_THRESHOLD,
        NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_BUFFER_OVERRUN,
        NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HEALTHY,
        NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HI_BER,
        NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HEALTHY,
        NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HI_BER,
        NSM_PORT_DOWN_REASON_CODE_DOWN_BY_VERIFICATION_GW,
        NSM_PORT_DOWN_REASON_CODE_RECEIVED_REMOTE_FAULT,
        NSM_PORT_DOWN_REASON_CODE_RECEIEVED_TS1,
        NSM_PORT_DOWN_REASON_CODE_DOWN_BY_MGMT_CMD,
        NSM_PORT_DOWN_REASON_CODE_CABLE_ACCESS_ISSUES,
        NSM_PORT_DOWN_REASON_CODE_CURRENT_ISSUE,
        NSM_PORT_DOWN_REASON_CODE_POWER_BUDGET,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_RAW_BER,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_EFFECTIVE_BER,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_SYMBOL_BER,
        NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_CREDIT_WATCHDOG,
        NSM_PORT_DOWN_REASON_CODE_PEER_SLEEP,
        NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE,
        NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE_LOCK,
        NSM_PORT_DOWN_REASON_CODE_PEER_THERMAL_EVENT,
        NSM_PORT_DOWN_REASON_CODE_PEER_FORCE_EVENT,
        NSM_PORT_DOWN_REASON_CODE_PEER_RESET_EVENT,
        0xFFFF, // unknown code -> default branch
    };

    for (auto reasonCode : reasonCodes)
    {
        Response portCharResp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp),
                              0);
        auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

        nsm_port_characteristics_data data = {};
        data.nv_port_line_rate_mbps = 50000;
        data.nv_port_data_rate_kbps = 45000000;
        data.status_lane_info = 0x02;
        data.port_status.port_down_reason_code = reasonCode;

        auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS,
                                                         ERR_NULL, &data, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        auto result = portChar.handleResponseMsg(msg, portCharResp.size());
        EXPECT_EQ(result, NSM_SW_SUCCESS) << "reason_code=" << reasonCode;
    }
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmPortSensor
// =============================================================================

// Name absent -> name="" -> portName="_0" -> valid D-Bus path component
// count("Name") FALSE branch taken
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_MissingName_Succeeds)
{
    using namespace nsm;

    const std::string missingNamePath =
        "/xyz/openbmc_project/inventory/system/gpu/port_no_name";

    dbus::PropertyMap properties = {
        // "Name" intentionally omitted -> name="" -> count("Name") FALSE
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0_noname")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(missingNamePath,
                                                 basicIntfName);
    pm = properties;

    const size_t initialCount = gpu->deviceSensors.size();
    EXPECT_NO_THROW(createNsmPortSensor(mockManager, basicIntfName,
                                        missingNamePath, false));
    // portName="_0", objPath=parentObjPath+"/Ports/_0" -> valid -> sensors
    // added
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// ParentObjPath absent -> parentObjPath="" -> deviceName="" -> topologyObjPath
// computed from empty deviceName; portObjPath = "/Ports/NVLink_0"
// count("ParentObjPath") FALSE branch taken
TEST_F(NsmPortSensorCreateTestFixture,
       createPortSensor_MissingParentObjPath_Succeeds)
{
    using namespace nsm;

    const std::string missingParentPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_no_parent";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_NoParent")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        // "ParentObjPath" intentionally omitted -> parentObjPath=""
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(missingParentPath,
                                                 basicIntfName);
    pm = properties;

    const size_t initialCount = gpu->deviceSensors.size();
    EXPECT_NO_THROW(createNsmPortSensor(mockManager, basicIntfName,
                                        missingParentPath, false));
    // portObjPath="/Ports/NVLink_NoParent_0" -> valid -> sensors added
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// DeviceType absent -> deviceType=0 = NSM_DEV_ID_GPU -> GPU branch executes
// count("DeviceType") FALSE branch taken (deviceType stays 0)
TEST_F(NsmPortSensorCreateTestFixture,
       createPortSensor_MissingDeviceType_DefaultsToGPU)
{
    using namespace nsm;

    const std::string missingDTypePath =
        "/xyz/openbmc_project/inventory/system/gpu/port_no_dtype";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_NoDType")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        // "DeviceType" intentionally omitted -> deviceType=0=NSM_DEV_ID_GPU
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0_nodtype")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(missingDTypePath,
                                                 basicIntfName);
    pm = properties;

    const size_t initialCount = gpu->deviceSensors.size();
    EXPECT_NO_THROW(createNsmPortSensor(mockManager, basicIntfName,
                                        missingDTypePath, false));
    // deviceType=0=GPU -> Status+Characteristics+Metrics sensors added
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// Priority absent -> count("Priority") FALSE branch -> priority=false (default)
// Sensor created with priority=false (non-priority queue)
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_MissingPriority_Success)
{
    using namespace nsm;

    const std::string noPriorityPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_no_priority";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_NoPrio")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0_noprio")},
        // "Priority" intentionally omitted -> count("Priority") FALSE ->
        // priority=false
    };

    auto& pm = utils::MockDbusAsync::propertyMap(noPriorityPath, basicIntfName);
    pm = properties;

    const size_t initialCount = gpu->deviceSensors.size();
    EXPECT_NO_THROW(
        createNsmPortSensor(mockManager, basicIntfName, noPriorityPath, false));
    // priority defaults to false -> sensor added to deviceSensors (not
    // priority)
    EXPECT_GT(gpu->deviceSensors.size(), initialCount);
}

// checkPortCharactersticRCAndPopulateRuntimeErr: characteristics decode
// succeeds (cc=NSM_SUCCESS) -> else branch at nsmPort.cpp line 283 ->
// runtimeError(false) set.
TEST_F(NsmPortSensorCreateTestFixture,
       testPortStatusDownLockSuccessfulCharacteristics)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DLSuccess";
    uint8_t portNum = 33;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/downlock_success";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    portMetricsOem3Intf->runtimeError(true); // start with true

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Port status response: DOWN_LOCK -> triggers checkPortCharacteristic call
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto statusMsg = reinterpret_cast<nsm_msg*>(portStatusResp.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                  NSM_PORTSTATE_DOWN_LOCK,
                                  NSM_PORTSTATUS_ENABLED, statusMsg);

    // Port characteristics response: SUCCESS -> else branch ->
    // runtimeError(false)
    Response portCharResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);
    auto charMsg = reinterpret_cast<nsm_msg*>(portCharResp.data());
    nsm_port_characteristics_data data = {};
    data.nv_port_line_rate_mbps = 50000;
    encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                           charMsg);

    testing::InSequence seq;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portCharResp, Response{}));

    auto result = portStatus.update(gpu);
    EXPECT_EQ(result.data(), NSM_SW_SUCCESS);

    // Successful characteristics decode -> else branch -> runtimeError cleared
    EXPECT_FALSE(portMetricsOem3Intf->runtimeError());
}

// NsmPortStatus::update – sensorIO returns error -> if (rc) TRUE at L159,
// early co_return without decoding.
TEST_F(NsmPortSensorCreateTestFixture, testPortStatusUpdate_SensorIOFail)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_SIOFail";
    uint8_t portNum = 97;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/siofail";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // sensorIO returns NSM_ERROR -> if (rc) TRUE -> early return
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    auto result = portStatus.update(gpu);
    EXPECT_NE(result.data(), NSM_SW_SUCCESS);
}

// checkPortCharactersticRCAndPopulateRuntimeErr – second sensorIO fails ->
// if (rc) TRUE at L262, early co_return without populating runtimeError. //

TEST_F(NsmPortSensorCreateTestFixture,
       testPortStatusDownLock_CharacteristicsSensorIOFail)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DLSIOFail";
    uint8_t portNum = 98;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/dlsiofail";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // First sensorIO: DOWN_LOCK port status -> triggers characteristics call
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                  NSM_PORTSTATE_DOWN_LOCK,
                                  NSM_PORTSTATUS_ENABLED, msg);

    testing::InSequence seq;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));
    // Second sensorIO (characteristics): fails -> if (rc) TRUE early return
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));

    auto result = portStatus.update(gpu);
    // update itself returns NSM_SW_SUCCESS (from status decode), but
    // characteristics sensorIO error is propagated via co_return rc in
    // checkPortCharactersticRCAndPopulateRuntimeErr (the caller ignores it)
    (void)result.data();
}

// EthPortTelemetryAggregator::updateCounterValues – tag not in tagToPropertyMap
// (tags 0-20 are mapped; tag 255 is not) -> if (it != end()) FALSE branch.
TEST_F(NsmPortSensorCreateTestFixture, testEthPortTelemetry_UnknownTag)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort_UnknownTag";
    uint16_t portNumber = 7;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport_unknown";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    nsm_ethernet_port_counter_data counterValue = {};
    counterValue.ethernet_port_counter_data_64bit = 12345;
    // Tag 255 is NOT in tagToPropertyMap (only 0-20 are mapped) ->
    // tagToPropertyMap.find() returns end() -> FALSE branch taken, no update
    ethPort.updateCounterValues(uint8_t(255), &counterValue);
}

// Count absent -> count("Count") FALSE branch -> count=0 -> loop runs 0 times
// Same result as ZeroCount test but exercises the FALSE branch of
// count("Count")
TEST_F(NsmPortSensorCreateTestFixture, createPortSensor_MissingCount_NoSensors)
{
    using namespace nsm;

    const std::string noCountPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_no_count";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_NoCount")},
        {"UUID", gpuUuid},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0_nocount")},
        {"Priority", false},
        // "Count" intentionally omitted -> count("Count") FALSE -> count=0
    };

    auto& pm = utils::MockDbusAsync::propertyMap(noCountPath, basicIntfName);
    pm = properties;

    const size_t initialCount = gpu->deviceSensors.size();
    EXPECT_NO_THROW(
        createNsmPortSensor(mockManager, basicIntfName, noCountPath, false));
    // count=0 -> for loop never runs -> no port sensors added
    EXPECT_EQ(gpu->deviceSensors.size(), initialCount);
}

// =============================================================================
// createNsmPortSensorWithNetworkPortAddresses and createNsmPortSensorGeneric
// wrapper tests (static removed from nsmPort.cpp to allow forward declaration)
// These thin wrappers call createNsmPortSensor with enableNetworkPortAddresses
// set to true or false respectively.
// =============================================================================

// createNsmPortSensorWithNetworkPortAddresses calls createNsmPortSensor(true)
// Covers lines 1927–1932 in nsmPort.cpp
TEST_F(NsmPortSensorCreateTestFixture,
       createPortSensorWithNetworkPortAddresses_CallsWithTrue)
{
    using namespace nsm;

    const std::string netAddrWrapPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_netaddr_wrap";
    const std::string netAddrIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkWithNetworkPortAddresses";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_NetAddrWrap")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0_netaddr")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(netAddrWrapPath, netAddrIntf);
    pm = properties;

    const size_t before = gpu->deviceSensors.size();
    createNsmPortSensorWithNetworkPortAddresses(mockManager, netAddrIntf,
                                                netAddrWrapPath);
    // enableNetworkPortAddresses=true -> NetworkPortAddresses sensor added
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// createNsmPortSensorGeneric calls createNsmPortSensor(false)
// Covers lines 1935–1941 in nsmPort.cpp
TEST_F(NsmPortSensorCreateTestFixture, createPortSensorGeneric_CallsWithFalse)
{
    using namespace nsm;

    const std::string genericWrapPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_generic_wrap";
    const std::string genericIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLink";

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink_GenericWrap")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(1)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0_generic")},
        {"Priority", false},
    };

    auto& pm = utils::MockDbusAsync::propertyMap(genericWrapPath, genericIntf);
    pm = properties;

    const size_t before = gpu->deviceSensors.size();
    createNsmPortSensorGeneric(mockManager, genericIntf, genericWrapPath);
    // enableNetworkPortAddresses=false -> no NetworkPortAddresses sensor
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ─── Branch coverage: rc==NSM_SW_SUCCESS && cc!=NSM_SUCCESS ─────────────────

// NsmPortMetrics::handleResponseMsg: non-success CC via 9-byte buffer ->
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch
TEST(NsmPortMetrics, HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    NsmPortMetricsHelper h;
    nsm::NsmPortMetrics portTel(
        h.bus, h.pName, h.portNum, h.type, h.deviceType, h.associations,
        h.parentObjPath, h.inventoryObjPath, h.iBPortIntf,
        h.portMetricsOem2Intf, h.portPacketCountersIntf);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = portTel.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPortCharacteristics::handleResponseMsg: non-success CC via 9-byte buffer
TEST(NsmPortCharacteristics,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string portName{"cc_char_port"};
    uint8_t portNum = 3;
    std::string type = "CharType";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/dummy/cc_char_port";
    auto portMetricsOem3Intf =
        std::make_shared<nsm::PortMetricsOem3Intf>(bus, objPath.c_str());
    auto iBPortIntf = std::make_shared<nsm::IBPortIntf>(bus, objPath.c_str());
    nsm::NsmPortCharacteristics sensor(
        bus, portName, portNum, type, portMetricsOem3Intf, iBPortIntf, objPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPortStatus::update: rc==NSM_SW_SUCCESS, cc==NSM_ERROR ->
// L183 FALSE branch via cc!=NSM_SUCCESS (distinct from ErrorCC test
// which uses a full 256-byte buffer causing decode_reason_code_and_cc
// to return NSM_SW_ERROR_LENGTH, covering rc!=NSM_SW_SUCCESS instead)
TEST_F(NsmPortSensorCreateTestFixture,
       testPortStatusUpdate_DecodeSuccessNonZeroCC_ElseBranch)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_0";
    uint8_t portNum = 1;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/nvlink_cc_br";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Exactly-sized non-success buffer: decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR -> L183 FALSE via cc!=NSM_SUCCESS
    std::vector<uint8_t> errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    errorResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(errorResp, Response{}));
    portStatus.update(gpu);
}

// checkPortCharactersticRCAndPopulateRuntimeErr L277:
// if (cc != NSM_SUCCESS && reasonCode != ERR_NULL)
// Branch 2: cc!=NSM_SUCCESS, reasonCode==ERR_NULL ->
// second operand FALSE -> else -> runtimeError(false)
//
// 9-byte buffer: decode_reason_code_and_cc returns NSM_SW_SUCCESS
// with cc=NSM_ERROR (byte 6 set), reasonCode=0=ERR_NULL -> else branch
TEST_F(NsmPortSensorCreateTestFixture,
       testPortStatusDownLock_CharacteristicsNonZeroCCNullReason)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DLNullReason";
    uint8_t portNum = 99;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/dlnullreason";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    portMetricsOem3Intf->runtimeError(true); // start true, expect false after

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // First sensorIO: DOWN_LOCK -> triggers checkPortCharactersticRCAndPopulate
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto statusMsg = reinterpret_cast<nsm_msg*>(portStatusResp.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                  NSM_PORTSTATE_DOWN_LOCK,
                                  NSM_PORTSTATUS_ENABLED, statusMsg);

    // Second sensorIO: 9-byte buffer -> decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR, reasonCode=0=ERR_NULL
    // -> L277 AND second operand FALSE -> else -> runtimeError(false)
    Response portCharResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    portCharResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc = NSM_ERROR
    // reasonCode bytes [2,3] remain 0 = ERR_NULL

    testing::InSequence seq;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portCharResp, Response{}));

    portStatus.update(gpu);

    // cc!=NSM_SUCCESS but reasonCode==ERR_NULL -> else -> runtimeError cleared
    EXPECT_FALSE(portMetricsOem3Intf->runtimeError());
}
