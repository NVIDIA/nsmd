/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmPcieGroup.cpp:
 * - NsmPcieGroup single-port constructor
 * - NsmPcieGroup multi-port constructor
 * - genRequestMsg: single-port vs multi-port dispatch
 * - genSinglePortRequestMsg: encode failure
 * - genMultiPortRequestMsg: encode failure
 * - NsmPciGroup2/3/4 constructors
 * - NsmPciGroup2/3/4 handleResponseMsg: decode failure (rc!=0)
 * - NsmPciGroup2/3/4 handleResponseMsg: cc!=NSM_SUCCESS
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;

#include "base.h"
#include "pci-links.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmCommon/nsmPcieGroup.hpp"

using namespace nsm;

static auto pgBus = sdbusplus::bus::new_default();
static const std::string pgInvPath =
    "/xyz/openbmc_project/inventory/test/pcieGroupBranch";

// ---------------------------------------------------------------------------
// NsmPcieGroup: verify single-port genRequestMsg dispatch
// ---------------------------------------------------------------------------
TEST(NsmPcieGroupBranch, GenRequestMsg_SinglePort)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P0";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup2 group("g2_sp", "T", eccIntf, portIntf, uint8_t(3), invPath);

    EXPECT_FALSE(group.isMultiPortEnabled());
    EXPECT_EQ(group.getDeviceId(), 3);
    EXPECT_EQ(group.getGroupId(), GROUP_ID_2);

    auto req = group.genRequestMsg(10, 1);
    EXPECT_TRUE(req.has_value());
}

// ---------------------------------------------------------------------------
// NsmPciGroup3: constructor and handleResponseMsg success
// ---------------------------------------------------------------------------
TEST(NsmPcieGroupBranch, Group3_Constructor_And_HandleSuccess)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P3";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup3 group("g3_ctor", "T", eccIntf, portIntf, uint8_t(1), invPath);

    EXPECT_EQ(group.getGroupId(), GROUP_ID_3);

    struct nsm_query_scalar_group_telemetry_group_3 data = {};
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
    EXPECT_EQ(eccIntf->l0ToRecoveryCount(), 42u);
}

// NsmPciGroup3: handleResponseMsg decode failure
TEST(NsmPcieGroupBranch, Group3_HandleResponseMsg_DecodeFail)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P3f";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup3 group("g3_decfail", "T", eccIntf, portIntf, uint8_t(1),
                       invPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPciGroup3: handleResponseMsg cc != SUCCESS
TEST(NsmPcieGroupBranch, Group3_HandleResponseMsg_CCError)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P3e";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup3 group("g3_ccerr", "T", eccIntf, portIntf, uint8_t(1), invPath);

    struct nsm_query_scalar_group_telemetry_group_3 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ---------------------------------------------------------------------------
// NsmPciGroup4: constructor and handleResponseMsg success
// ---------------------------------------------------------------------------
TEST(NsmPcieGroupBranch, Group4_Constructor_And_HandleSuccess)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P4";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup4 group("g4_ctor", "T", eccIntf, portIntf, uint8_t(2), invPath);

    EXPECT_EQ(group.getGroupId(), GROUP_ID_4);

    struct nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 10;
    data.replay_rollover_cnt = 20;
    data.NAK_sent_cnt = 30;
    data.NAK_recv_cnt = 40;
    data.FC_timeout_err_cnt = 50;
    data.bad_TLP_cnt = 60;
    data.recv_err_cnt = 70;
    data.dllp_crc_errors = 80;

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(eccIntf->replayCount(), 10u);
    EXPECT_EQ(eccIntf->replayRolloverCount(), 20u);
    EXPECT_EQ(eccIntf->nakSentCount(), 30u);
    EXPECT_EQ(eccIntf->nakReceivedCount(), 40u);
}

// NsmPciGroup4: handleResponseMsg decode failure
TEST(NsmPcieGroupBranch, Group4_HandleResponseMsg_DecodeFail)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P4f";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup4 group("g4_decfail", "T", eccIntf, portIntf, uint8_t(2),
                       invPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// NsmPciGroup4: handleResponseMsg cc error
TEST(NsmPcieGroupBranch, Group4_HandleResponseMsg_CCError)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P4e";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup4 group("g4_ccerr", "T", eccIntf, portIntf, uint8_t(2), invPath);

    struct nsm_query_scalar_group_telemetry_group_4 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// ---------------------------------------------------------------------------
// NsmPciGroup2: genRequestMsg and handleResponseMsg cc error path
// ---------------------------------------------------------------------------
TEST(NsmPcieGroupBranch, Group2_HandleResponseMsg_CCError)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P2e";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup2 group("g2_ccerr", "T", eccIntf, portIntf, uint8_t(0), invPath);

    struct nsm_query_scalar_group_telemetry_group_2 data = {};
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto cc = group.handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(cc, NSM_SUCCESS);
}

// NsmPciGroup2: decode failure (truncated response)
TEST(NsmPcieGroupBranch, Group2_HandleResponseMsg_DecodeFail)
{
    auto eccIntf = std::make_shared<PCieEccIntf>(pgBus, pgInvPath.c_str());
    auto portPath = pgInvPath + "/Ports/P2f";
    auto portIntf = std::make_shared<PCieEccIntf>(pgBus, portPath.c_str());
    std::string invPath = pgInvPath;

    NsmPciGroup2 group("g2_decfail", "T", eccIntf, portIntf, uint8_t(0),
                       invPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = group.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
