/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Branch coverage for NsmPCIeECCGroup1-4 genRequestMsg / handleResponseMsg
 * in nsmd/nsmPort/nsmRetimerPort.cpp (used by nsmFpgaPort.cpp factory).
 *
 * Covers:
 * - NsmPcieGroup::genRequestMsg encode fail (instanceId=255)
 * - NsmPCIeECCGroup1 handleResponseMsg: success, errorCC, decode fail
 * - NsmPCIeECCGroup2 handleResponseMsg: success, errorCC, decode fail
 * - NsmPCIeECCGroup3 handleResponseMsg: success, errorCC, decode fail
 * - NsmPCIeECCGroup4 handleResponseMsg: success, errorCC, decode fail
 * - NsmPcieGroup::genRequestMsg multi-port encode fail
 * - cc ? cc : rc ternary in each group
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "pci-links.h"

#include "nsmRetimerPort.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

static const std::string invPath =
    "/xyz/openbmc_project/inventory/system/fpga/branch_port";

// ============================================================================
// NsmPcieGroup genRequestMsg encode-fail branches
// ============================================================================

TEST(NsmFpgaPortBranch, PcieGroup_GenRequestMsg_SinglePort_EncodeFail)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());

    NsmPCIeECCGroup1 group("test_g1", "NSM_PortInfo", invPath, portInfoIntf,
                           portWidthIntf, uint8_t(0));

    // instanceId=255 > NSM_INSTANCE_MAX(31) → encode fails → nullopt
    auto result = group.genRequestMsg(0, 255);
    EXPECT_FALSE(result.has_value());
}

TEST(NsmFpgaPortBranch, PcieGroup_GenRequestMsg_SinglePort_Success)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());

    NsmPCIeECCGroup1 group("test_g1_ok", "NSM_PortInfo", invPath, portInfoIntf,
                           portWidthIntf, uint8_t(0));

    auto result = group.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

TEST(NsmFpgaPortBranch, PcieGroup_GenRequestMsg_MultiPort_EncodeFail)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());
    auto clockModeIntf = std::make_shared<PCIeClockModeIntf>(testBus,
                                                             invPath.c_str());

    NsmPCIeECCGroup1 group("test_g1_multi", "NSM_PortInfo", invPath,
                           portInfoIntf, portWidthIntf, clockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(0));

    // instanceId=255 → encode fails → nullopt
    auto result = group.genRequestMsg(0, 255);
    EXPECT_FALSE(result.has_value());
}

TEST(NsmFpgaPortBranch, PcieGroup_GenRequestMsg_MultiPort_Success)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());
    auto clockModeIntf = std::make_shared<PCIeClockModeIntf>(testBus,
                                                             invPath.c_str());

    NsmPCIeECCGroup1 group("test_g1_multi_ok", "NSM_PortInfo", invPath,
                           portInfoIntf, portWidthIntf, clockModeIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(0));

    auto result = group.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmPCIeECCGroup1 handleResponseMsg branches
// ============================================================================

TEST(NsmFpgaPortBranch, Group1_HandleResponse_Success)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());

    NsmPCIeECCGroup1 group("g1_success", "NSM_PortInfo", invPath, portInfoIntf,
                           portWidthIntf, uint8_t(0));

    nsm_query_scalar_group_telemetry_group_1 data = {};
    data.negotiated_link_speed = 5; // 32 Gbps
    data.max_link_speed = 6;        // 64 Gbps
    data.target_link_speed = 4;     // 16 Gbps
    data.negotiated_link_width = 4; // x8
    data.max_link_width = 5;        // x16
    data.max_read_request_size_bytes = 3;
    data.max_payload_size_bytes = 2;
    data.clock_mode = 1;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_DOUBLE_EQ(portInfoIntf->currentSpeed(), 32.0);
}

TEST(NsmFpgaPortBranch, Group1_HandleResponse_ErrorCC)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());

    NsmPCIeECCGroup1 group("g1_errcc", "NSM_PortInfo", invPath, portInfoIntf,
                           portWidthIntf, uint8_t(0));

    nsm_query_scalar_group_telemetry_group_1 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmFpgaPortBranch, Group1_HandleResponse_DecodeFail)
{
    auto portInfoIntf = std::make_shared<PortInfoIntf>(testBus,
                                                       invPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(testBus,
                                                         invPath.c_str());

    NsmPCIeECCGroup1 group("g1_decode_fail", "NSM_PortInfo", invPath,
                           portInfoIntf, portWidthIntf, uint8_t(0));

    // Valgrind-safe minimal buffer
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup2 handleResponseMsg branches
// ============================================================================

TEST(NsmFpgaPortBranch, Group2_HandleResponse_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup2 group("g2_success", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 10;
    data.fatal_errors = 5;
    data.correctable_errors = 20;
    data.unsupported_request_count = 3;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 10u);
    EXPECT_EQ(pcieEccIntf->feCount(), 5u);
    EXPECT_EQ(pcieEccIntf->ceCount(), 20u);
    EXPECT_EQ(pcieEccIntf->unsupportedRequestCount(), 3u);
}

TEST(NsmFpgaPortBranch, Group2_HandleResponse_ErrorCC)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup2 group("g2_errcc", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    nsm_query_scalar_group_telemetry_group_2 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group2_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmFpgaPortBranch, Group2_HandleResponse_DecodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup2 group("g2_decode_fail", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup3 handleResponseMsg branches
// ============================================================================

TEST(NsmFpgaPortBranch, Group3_HandleResponse_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup3 group("g3_success", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 42;
    data.training_seq_errors = 7;
    data.framing_errors = 3;
    data.link_down_count = 1;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 42u);
    EXPECT_EQ(pcieEccIntf->trainingSequenceErrorCount(), 7u);
    EXPECT_EQ(pcieEccIntf->framingErrorCount(), 3u);
    EXPECT_EQ(pcieEccIntf->linkDownedCount(), 1u);
}

TEST(NsmFpgaPortBranch, Group3_HandleResponse_ErrorCC)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup3 group("g3_errcc", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    nsm_query_scalar_group_telemetry_group_3 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group3_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmFpgaPortBranch, Group3_HandleResponse_DecodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup3 group("g3_decode_fail", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup4 handleResponseMsg branches
// ============================================================================

TEST(NsmFpgaPortBranch, Group4_HandleResponse_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup4 group("g4_success", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 100;
    data.replay_rollover_cnt = 2;
    data.NAK_sent_cnt = 15;
    data.NAK_recv_cnt = 8;
    data.FC_timeout_err_cnt = 4;
    data.bad_TLP_cnt = 6;
    data.recv_err_cnt = 9;
    data.dllp_crc_errors = 11;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->replayCount(), 100u);
    EXPECT_EQ(pcieEccIntf->replayRolloverCount(), 2u);
    EXPECT_EQ(pcieEccIntf->nakSentCount(), 15u);
    EXPECT_EQ(pcieEccIntf->nakReceivedCount(), 8u);
    EXPECT_EQ(pcieEccIntf->fcTimeoutErrors(), 4u);
    EXPECT_EQ(pcieEccIntf->badTLPCount(), 6u);
    EXPECT_EQ(pcieEccIntf->receiverErrorCount(), 9u);
    EXPECT_EQ(pcieEccIntf->dllpcrcErrorCount(), 11u);
}

TEST(NsmFpgaPortBranch, Group4_HandleResponse_ErrorCC)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup4 group("g4_errcc", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    nsm_query_scalar_group_telemetry_group_4 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_query_scalar_group_telemetry_v1_group4_resp(0, NSM_ERROR, ERR_NULL,
                                                       &data, responseMsg);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(NsmFpgaPortBranch, Group4_HandleResponse_DecodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup4 group("g4_decode_fail", "NSM_PCIe", invPath, pcieEccIntf,
                           uint8_t(0));

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ============================================================================
// NsmPCIeECCGroup2 multi-port constructor + genRequestMsg branches
// ============================================================================

TEST(NsmFpgaPortBranch, Group2_MultiPort_GenRequestMsg_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup2 group("g2_multi", "NSM_PCIe", invPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto result = group.genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

TEST(NsmFpgaPortBranch, Group2_MultiPort_GenRequestMsg_EncodeFail)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup2 group("g2_multi_fail", "NSM_PCIe", invPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    auto result = group.genRequestMsg(0, 255);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// NsmPCIeECCGroup3 multi-port constructor + handleResponse
// ============================================================================

TEST(NsmFpgaPortBranch, Group3_MultiPort_HandleResponse_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup3 group("g3_multi", "NSM_PCIe", invPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 99;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->l0ToRecoveryCount(), 99u);
}

// ============================================================================
// NsmPCIeECCGroup4 multi-port constructor + handleResponse
// ============================================================================

TEST(NsmFpgaPortBranch, Group4_MultiPort_HandleResponse_Success)
{
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(testBus, invPath.c_str());

    NsmPCIeECCGroup4 group("g4_multi", "NSM_PCIe", invPath, pcieEccIntf,
                           NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));

    nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 50;
    data.replay_rollover_cnt = 1;
    data.NAK_sent_cnt = 7;
    data.NAK_recv_cnt = 3;
    data.FC_timeout_err_cnt = 2;
    data.bad_TLP_cnt = 1;
    data.recv_err_cnt = 4;
    data.dllp_crc_errors = 5;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(pcieEccIntf->replayCount(), 50u);
}
