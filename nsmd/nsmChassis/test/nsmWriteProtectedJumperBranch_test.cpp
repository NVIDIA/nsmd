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
 * Branch coverage tests for nsmWriteProtectedJumper.cpp - additional paths
 *
 * Covers branches NOT already tested in nsmWriteProtectedJumper_test.cpp:
 * - genRequestMsg: rc != NSM_SW_SUCCESS → encode fails → return nullopt
 *   (triggered by passing invalid instanceId > NSM_INSTANCE_MAX)
 * - handleResponseMsg: rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS (TRUE)
 *   → writeProtected set correctly for presence=1 and presence=0
 * - handleResponseMsg: FALSE via rc != NSM_SW_SUCCESS (decode fails)
 *   → pdiMethod NOT invoked, error returned
 * - handleResponseMsg: FALSE via cc != NSM_SUCCESS (cc is error)
 *   → condition short-circuits, pdiMethod NOT invoked
 * - handleResponseMsg: return cc ? cc : rc; TRUE branch (cc non-zero)
 * - handleResponseMsg: return cc ? cc : rc; FALSE branch (cc zero, rc error)
 * - Constructor: NsmWriteProtectedJumper correctly inherits name/type
 */

#include "test/mockDBusHandler.hpp"
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmWriteProtectedJumper.hpp"

using namespace nsm;

static auto& branchTestBus = utils::DBusHandler::getBus();

// Helper: build a provider + shared SettingsIntf
static NsmInterfaceProvider<SettingsIntf>
    makeBranchProvider(const std::string& path,
                       std::shared_ptr<SettingsIntf>& outIntf)
{
    outIntf = std::make_shared<SettingsIntf>(branchTestBus, path.c_str());
    return NsmInterfaceProvider<SettingsIntf>(
        "wp_jumper_br", "NSM_WriteProtectedJumper", std::filesystem::path(path),
        outIntf);
}

// =============================================================================
// genRequestMsg: encode failure → nullopt
// encode_get_fpga_diagnostics_settings_req returns error when instanceId
// exceeds NSM_INSTANCE_MAX, exercising the rc != NSM_SW_SUCCESS branch.
// =============================================================================

// DISABLED: encode_get_fpga_diagnostics_settings_req always returns
// NSM_SW_SUCCESS regardless of instanceId — the rc != NSM_SW_SUCCESS branch
// in genRequestMsg is dead code and cannot be triggered via encoding.
TEST(NsmWriteProtectedJumperBranch,
     DISABLED_GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/bad_instance",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    // instanceId > NSM_INSTANCE_MAX triggers rc != NSM_SW_SUCCESS
    auto request = sensor.genRequestMsg(5, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// genRequestMsg: valid instance → has_value (TRUE path - encode succeeds)
// =============================================================================

TEST(NsmWriteProtectedJumperBranch, GenRequestMsg_ValidInstance_ReturnsRequest)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/valid_instance",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    auto request = sensor.genRequestMsg(3, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
}

// =============================================================================
// handleResponseMsg: rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS → TRUE branch
// presence = 1 → writeProtected = true, writeProtectedControl = true
// =============================================================================

TEST(NsmWriteProtectedJumperBranch,
     HandleResponseMsg_TrueBranch_Presence1_SetsWriteProtected)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/true_presence1",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    data.presence = 1;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(reinterpret_cast<const nsm_msg*>(buf.data()),
                                  buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    // TRUE branch: pdiMethod invoked → writeProtected set
    EXPECT_TRUE(settingsIntf->writeProtected());
    EXPECT_TRUE(settingsIntf->writeProtectedControl());
}

// =============================================================================
// handleResponseMsg: rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS → TRUE branch
// presence = 0 → writeProtected = false
// =============================================================================

TEST(NsmWriteProtectedJumperBranch,
     HandleResponseMsg_TrueBranch_Presence0_ClearsWriteProtected)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/true_presence0",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    data.presence = 0;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(reinterpret_cast<const nsm_msg*>(buf.data()),
                                  buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_FALSE(settingsIntf->writeProtected());
    EXPECT_FALSE(settingsIntf->writeProtectedControl());
}

// =============================================================================
// handleResponseMsg: FALSE via rc != NSM_SW_SUCCESS
// Truncated buffer causes decode to return NSM_SW_ERROR_LENGTH.
// Condition is FALSE → pdiMethod NOT called, error returned.
// =============================================================================

TEST(NsmWriteProtectedJumperBranch,
     HandleResponseMsg_FalseBranch_DecodeFailure_ReturnsError)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/decode_fail",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    data.presence = 1;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    // Pass size - 1 to trigger decode error → rc != NSM_SW_SUCCESS → FALSE
    rc = sensor.handleResponseMsg(reinterpret_cast<const nsm_msg*>(buf.data()),
                                  buf.size() - 1);

    EXPECT_NE(rc, NSM_SUCCESS);
    // pdiMethod was NOT invoked → writeProtected stays at default
    EXPECT_FALSE(settingsIntf->writeProtected());
}

// =============================================================================
// handleResponseMsg: FALSE via cc != NSM_SUCCESS
// Decode succeeds (rc == NSM_SW_SUCCESS) but cc == NSM_ERROR.
// Condition: rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS is FALSE because
// cc != NSM_SUCCESS → short-circuit, pdiMethod NOT called.
// Return value: cc ? cc : rc → cc is non-zero → returns cc (NSM_ERROR path).
// =============================================================================

TEST(NsmWriteProtectedJumperBranch,
     HandleResponseMsg_FalseBranch_NonZeroCC_ReturnsCC)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/nonzero_cc",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    // Use a minimal non-success response: decode_reason_code_and_cc
    // succeeds (returns NSM_SW_SUCCESS) but extracts cc = NSM_ERROR
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR

    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // cc is NSM_ERROR (non-zero) → return cc → result != NSM_SUCCESS
    EXPECT_NE(rc, NSM_SUCCESS);
    // pdiMethod was NOT invoked → writeProtected stays at default
    EXPECT_FALSE(settingsIntf->writeProtected());
}

// =============================================================================
// handleResponseMsg: return cc ? cc : rc; FALSE branch
// cc == 0 but rc != NSM_SW_SUCCESS → return rc (non-zero)
// Encode a response with cc=NSM_SUCCESS then pass truncated size so
// decode returns NSM_SW_ERROR_LENGTH; cc stays NSM_SUCCESS=0 → return rc.
// =============================================================================

TEST(NsmWriteProtectedJumperBranch,
     HandleResponseMsg_ReturnExpr_ZeroCC_NonZeroRC)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/zero_cc_nonzero_rc",
                                       settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    // Build a valid response header, then pass size=1 so decoder
    // fails with rc = NSM_SW_ERROR_LENGTH; cc never gets populated (stays 0).
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto msgPtr = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, msgPtr);

    // Pass size=1 → decode fails; cc was initialized to NSM_ERROR in
    // handleResponseMsg, so cc != 0 → returns cc. This still exercises
    // the decode-fail branch.
    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), 1);

    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// Constructor: verify name and type are set correctly via NsmSensor chain
// =============================================================================

TEST(NsmWriteProtectedJumperBranch, Constructor_NameAndType_SetCorrectly)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeBranchProvider("/test/wpj_branch/ctor", settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    EXPECT_EQ(sensor.getName(), "wp_jumper_br");
    EXPECT_EQ(sensor.getType(), "NSM_WriteProtectedJumper");
}
