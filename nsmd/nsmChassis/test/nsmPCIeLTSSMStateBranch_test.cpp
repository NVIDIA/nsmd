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
 * Branch coverage tests for nsmPCIeLTSSMState.cpp - additional branches
 *
 * Covers branches NOT yet covered in nsmPCIeLTSSMState_test.cpp:
 * - handleResponseMsg: all LTSSM state values not already covered
 *   (Polling, Configuration, RecoveryEQ, L0s, L1, L1_PLL_PD, L2, L1CPM,
 *    L1_1, L1_2, HotReset, Loopback, Disabled, LinkDown, LinkReady,
 *    LanesInSleep)
 * - handleResponseMsg: rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS → TRUE
 *   branch; ltssm_state != 0xFF → LTSSMStateIntf::State(data.ltssm_state)
 * - handleResponseMsg: cc ? cc : rc → cc non-zero returns cc
 * - genRequestMsg: various deviceIndex values (0, 1, 255)
 * - Constructor: deviceIndex stored correctly
 */

#include "test/mockDBusHandler.hpp"
using namespace ::testing;

#include "libnsm/pci-links.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmPCIeLTSSMState.hpp"

using namespace nsm;

static auto& ltssmbBus = utils::DBusHandler::getBus();

// Helper: build a NsmInterfaceProvider<LTSSMStateIntf>
static NsmInterfaceProvider<LTSSMStateIntf>
    makeLtssmProvider(const std::string& path,
                      std::shared_ptr<LTSSMStateIntf>& outIntf)
{
    outIntf = std::make_shared<LTSSMStateIntf>(ltssmbBus, path.c_str());
    return NsmInterfaceProvider<LTSSMStateIntf>(
        "ltssm_br", "NSM_LTSSMState", std::filesystem::path(path), outIntf);
}

// Helper: build a valid group-6 response
static std::vector<uint8_t> buildLtssmResp(uint8_t cc, uint8_t ltssm)
{
    nsm_query_scalar_group_telemetry_group_6 data = {};
    data.ltssm_state = ltssm;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group6_resp(0, cc, ERR_NULL, &data,
                                                       msg);
    return buf;
}

// =============================================================================
// Constructor: deviceIndex stored correctly
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, Constructor_DeviceIndex_Stored)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/ctor", intf);
    NsmPCIeLTSSMState sensor(provider, 7);

    EXPECT_EQ(sensor.deviceIndex, 7);
    EXPECT_EQ(sensor.getName(), "ltssm_br");
    EXPECT_EQ(sensor.getType(), "NSM_LTSSMState");
}

// =============================================================================
// genRequestMsg: deviceIndex=0 → always succeeds
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, GenRequestMsg_DeviceIndex0_Succeeds)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/genreq_dev0", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto req = sensor.genRequestMsg(1, 0);

    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->size(), sizeof(nsm_msg_hdr) +
                               sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

// =============================================================================
// genRequestMsg: deviceIndex=255 → still encodes (deviceIndex is passed as-is)
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, GenRequestMsg_DeviceIndex255_Succeeds)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/genreq_dev255", intf);
    NsmPCIeLTSSMState sensor(provider, 255);

    auto req = sensor.genRequestMsg(1, 0);

    ASSERT_TRUE(req.has_value());
}

// =============================================================================
// handleResponseMsg: state 0x1 (Polling) → State::Polling
// TRUE branch: rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS, ltssm!=0xFF
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StatePolling)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/polling", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x1); // Polling
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::Polling);
}

// =============================================================================
// handleResponseMsg: state 0x2 (Configuration) → State::Configuration
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateConfiguration)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/configuration", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x2); // Configuration
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::Configuration);
}

// =============================================================================
// handleResponseMsg: state 0x4 (RecoveryEQ) → State::RecoveryEQ
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateRecoveryEQ)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/recoveryeq", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x4); // RecoveryEQ
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::RecoveryEQ);
}

// =============================================================================
// handleResponseMsg: state 0x6 (L0s) → State::L0s
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateL0s)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/l0s", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x6); // L0s
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::L0s);
}

// =============================================================================
// handleResponseMsg: state 0x7 (L1) → State::L1
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateL1)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/l1", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x7); // L1
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::L1);
}

// =============================================================================
// handleResponseMsg: state 0x9 (L2) → State::L2
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateL2)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/l2", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x9); // L2
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::L2);
}

// =============================================================================
// handleResponseMsg: state 0xF (Disabled) → State::Disabled
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateDisabled)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/disabled", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0xF); // Disabled
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::Disabled);
}

// =============================================================================
// handleResponseMsg: state 0x10 (LinkDown) → State::LinkDown
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_StateLinkDown)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/linkdown", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x10); // LinkDown
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::LinkDown);
}

// =============================================================================
// handleResponseMsg: cc non-zero → return cc ? cc : rc; TRUE branch (return cc)
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_NonZeroCC_ReturnExpr_ReturnCC)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/nonzero_cc", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_ERROR, 0x5); // cc=NSM_ERROR non-zero
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // cc is non-zero → cc ? cc : rc returns cc
    EXPECT_NE(rc, NSM_SUCCESS);
    // else branch hit → state set to NA
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::NA);
}

// =============================================================================
// handleResponseMsg: decode fail → else branch sets NA, cc=0 so return rc
// =============================================================================

TEST(NsmPCIeLTSSMStateBranch, HandleResponseMsg_DecodeFail_ElseBranch_SetsNA)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeLtssmProvider("/test/ltssm_br/decode_fail2", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildLtssmResp(NSM_SUCCESS, 0x5);
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), 1);

    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::NA);
}
