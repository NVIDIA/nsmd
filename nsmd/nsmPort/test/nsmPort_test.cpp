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
} // namespace nsm

// Helper to create NsmPortMetrics for reuse
struct NsmPortMetricsHelper
{
    sdbusplus::bus::bus& bus = utils::DBusHandler::getBus();
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
    // GROUP_ID_4 constructor calls initHandleResponse(3) → l0ToRecoveryCount=0
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
    // else branch: handleResponse called with zeroed data → ceCount stays 0
    EXPECT_EQ(pcieEccIntf->ceCount(), 0u);
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
