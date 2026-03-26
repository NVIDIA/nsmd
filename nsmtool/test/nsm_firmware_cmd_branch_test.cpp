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

// Include the .cpp directly so that its anonymous-namespace `commands`
// vector and all class definitions are in this translation unit.
#include "firmware-utils.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#include "../nsm_firmware_cmd.cpp"

#include <gtest/gtest.h>

// Firmware commands (after registerCommand) in order:
//   [0]  GetRotInformation
//   [1]  IrreversibleConfig
//   [2]  QueryCodeAuthKeyPerm (registered as QueryFWCodeAuthKey)
//   [3]  UpdateCodeAuthKeyPerm
//   [4]  QueryFirmwareSecurityVersion
//   [5]  UpdateMinSecurityVersion
//   [6]  SetRoTProperty
//   [7]  DotCAKInstall
//   [8]  DotCAKBypass
//   [9]  ImageCopyControl
//   [10] DotLock
//   [11] DotCAKRotate
//   [12] DotUnlockChallenge
//   [13] DotUnlock
//   [14] DotGetInfo
//   [15] DotGetStatus
//   [16] DotDisable
//   [17] DotOverride
//   [18] DotRecovery

namespace nsmtool::firmware
{

// ---- Helpers ----------------------------------------------------------------

static void setupFirmwareCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* fwSub = app.get_subcommand("firmware");
    if (!fwSub)
        return;
    auto* leafSub = fwSub->get_subcommand(cmdName);
    if (!leafSub)
        return;

    std::vector<std::string> args = {"-m", "1"};
    args.insert(args.end(), extraArgs.begin(), extraArgs.end());
    std::reverse(args.begin(), args.end());
    try
    {
        leafSub->parse(args);
    }
    catch (...)
    {}
}

static std::string createTempBinFile(const std::string& path, size_t size)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    std::vector<uint8_t> zeros(size, 0);
    f.write(reinterpret_cast<const char*>(zeros.data()),
            static_cast<std::streamsize>(size));
    f.close();
    return path;
}

static const std::string kEcdsaKeyFile = "/tmp/fwb_test_ecdsa_96b.bin";
static const std::string kLmsKeyFile = "/tmp/fwb_test_lms_48b.bin";
static const std::string kSignatureFile = "/tmp/fwb_test_sig_1840b.bin";
static const std::string kDotBlobFile = "/tmp/fwb_test_dotblob_1024b.bin";
static const std::string kChallengeFile = "/tmp/fwb_test_challenge_32b.bin";
static const std::string kOutputFile = "/tmp/fwb_test_output.bin";

class FwBranchTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        createTempBinFile(kEcdsaKeyFile, 96);
        createTempBinFile(kLmsKeyFile, 48);
        createTempBinFile(kSignatureFile, 1840);
        createTempBinFile(kDotBlobFile, 1024);
        createTempBinFile(kChallengeFile, 32);
    }

    void TearDown() override
    {
        std::remove(kEcdsaKeyFile.c_str());
        std::remove(kLmsKeyFile.c_str());
        std::remove(kSignatureFile.c_str());
        std::remove(kDotBlobFile.c_str());
        std::remove(kChallengeFile.c_str());
        std::remove(kOutputFile.c_str());
    }
};

// ===========================================================================
// [0] GetRotInformation — mapEnumToString branches
// ===========================================================================

// Success with 2 firmware slots (exercises loop body and slot_info branches)
TEST_F(FwBranchTest, GetRotInformation_TwoSlots)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    // Allocate enough for header + full query response + 2 slot entries
    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 2;
    fw_info.fq_resp_hdr.background_copy_policy = 0; // Disabled
    fw_info.fq_resp_hdr.active_slot = 0;
    fw_info.fq_resp_hdr.active_keyset = 0;
    fw_info.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_AUTOMATIC_FAILOVER;

    nsm_firmware_slot_info slots[2] = {};
    // Slot 0: exercise known enum values
    slots[0].slot_id = 0;
    strncpy(reinterpret_cast<char*>(slots[0].firmware_version_string), "v1.0",
            sizeof(slots[0].firmware_version_string));
    slots[0].build_type = 0;          // Development
    slots[0].signing_type = 1;        // Production
    slots[0].write_protect_state = 1; // Enabled
    slots[0].firmware_state = 1;      // Activated

    // Slot 1: exercise "Not Defined" branches (unknown values)
    slots[1].slot_id = 1;
    strncpy(reinterpret_cast<char*>(slots[1].firmware_version_string), "v2.0",
            sizeof(slots[1].firmware_version_string));
    slots[1].build_type = 99;          // Not Defined
    slots[1].signing_type = 99;        // Not Defined
    slots[1].write_protect_state = 99; // Not Defined
    slots[1].firmware_state = 99;      // Not Defined

    fw_info.slot_info = slots;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// bgCopyPolicy=1 (Enabled), globalFailoverPolicy values
TEST_F(FwBranchTest, GetRotInformation_BgCopyEnabled)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 0;
    fw_info.fq_resp_hdr.background_copy_policy = 1; // Enabled
    fw_info.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_NO_FAILOVER;
    fw_info.slot_info = nullptr;

    std::vector<uint8_t> buf(512, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// globalFailoverPolicy=Not Applicable
TEST_F(FwBranchTest, GetRotInformation_FailoverNotApplicable)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 0;
    fw_info.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_NOT_APPLICABLE;
    fw_info.slot_info = nullptr;

    std::vector<uint8_t> buf(512, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// globalFailoverPolicy=Not Defined (unknown value)
TEST_F(FwBranchTest, GetRotInformation_FailoverNotDefined)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 0;
    fw_info.fq_resp_hdr.global_failover_policy = 99; // Not Defined
    fw_info.fq_resp_hdr.background_copy_policy = 99; // Not Defined
    fw_info.slot_info = nullptr;

    std::vector<uint8_t> buf(512, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// All firmware_state enum values
TEST_F(FwBranchTest, GetRotInformation_AllFirmwareStates)
{
    // Test firmware states 0-9 (each in a separate slot)
    for (uint8_t state = 0; state <= 9; ++state)
    {
        CLI::App app;
        setupFirmwareCommands(app);
        parseSubcmdArgs(app, "GetRotInformation",
                        {"-c", "1", "-i", "0", "-d", "0"});

        nsm_firmware_erot_state_info_resp fw_info = {};
        fw_info.fq_resp_hdr.firmware_slot_count = 1;
        nsm_firmware_slot_info slot = {};
        slot.firmware_state = state;
        fw_info.slot_info = &slot;

        std::vector<uint8_t> buf(2048, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_query_get_erot_state_parameters_resp(
            0, NSM_SUCCESS, ERR_NULL, &fw_info, msg);
        EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
    }
}

// All signing_type enum values
TEST_F(FwBranchTest, GetRotInformation_AllSigningTypes)
{
    for (uint8_t sigType : {0, 1, 2, 4})
    {
        CLI::App app;
        setupFirmwareCommands(app);
        parseSubcmdArgs(app, "GetRotInformation",
                        {"-c", "1", "-i", "0", "-d", "0"});

        nsm_firmware_erot_state_info_resp fw_info = {};
        fw_info.fq_resp_hdr.firmware_slot_count = 1;
        nsm_firmware_slot_info slot = {};
        slot.signing_type = sigType;
        fw_info.slot_info = &slot;

        std::vector<uint8_t> buf(2048, 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_query_get_erot_state_parameters_resp(
            0, NSM_SUCCESS, ERR_NULL, &fw_info, msg);
        EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
    }
}

// ===========================================================================
// [3] UpdateCodeAuthKeyPerm — updateMethod bit branches
// ===========================================================================

TEST_F(FwBranchTest, UpdateCodeAuthKeyPerm_AllUpdateBits)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    // Set all relevant bits: bit0-5, bit16-18
    uint32_t updateMethod = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                            (1u << 4) | (1u << 5) | (1u << 16) | (1u << 17) |
                            (1u << 18);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, UpdateCodeAuthKeyPerm_NoBits)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL, 0, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, UpdateCodeAuthKeyPerm_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [5] UpdateMinSecurityVersion — updateMethod bit branches (same pattern)
// ===========================================================================

TEST_F(FwBranchTest, UpdateMinSecurityVersion_AllUpdateBits)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) |
                              (1u << 4) | (1u << 5) | (1u << 16) | (1u << 17);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, UpdateMinSecurityVersion_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [1] IrreversibleConfig — switch(requestType) branches
// ===========================================================================

// requestType=0 (QUERY) — error path
TEST_F(FwBranchTest, IrreversibleConfig_Query_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set completion_code to non-zero so decode returns early before
    // memcpy of irreversible_cfg_query (which would be out-of-bounds in
    // this small error-response buffer).
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// requestType=1 (DISABLE) — success
TEST_F(FwBranchTest, IrreversibleConfig_Disable_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_irreversible_config_request_1_resp(0, NSM_SUCCESS,
                                                           ERR_NULL, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// requestType=1 (DISABLE) — error
TEST_F(FwBranchTest, IrreversibleConfig_Disable_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// requestType=2 (ENABLE) — error
TEST_F(FwBranchTest, IrreversibleConfig_Enable_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "2"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// requestType=99 (default branch)
TEST_F(FwBranchTest, IrreversibleConfig_Default)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "99"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [2] QueryCodeAuthKeyPerm — success with bitmap data
// ===========================================================================

TEST_F(FwBranchTest, QueryCodeAuthKeyPerm_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set completion_code to non-zero so decode_reason_code_and_cc returns
    // early before reading permission_bitmap_length (which would be
    // out-of-bounds in this small error-response buffer).
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [4] QueryFirmwareSecurityVersion
// ===========================================================================

TEST_F(FwBranchTest, QueryFirmwareSecurityVersion_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [6] SetRoTProperty
// ===========================================================================

TEST_F(FwBranchTest, SetRoTProperty_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "0", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [7] DotCAKInstall — parseResponseMsg branches
// ===========================================================================

TEST_F(FwBranchTest, DotCAKInstall_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotCAKInstall_PayloadTooSmall)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[7]->parseResponseMsg(msg, 1)); // payloadLength < min
}

TEST_F(FwBranchTest, DotCAKInstall_DecodeError)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotCAKInstall_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<nsm_common_resp*>(msg->payload);
    resp->command = NSM_FW_DOT_CAK_INSTALL;
    resp->completion_code = NSM_SUCCESS;
    resp->reserved = 0;
    encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// DotCAKInstall with wrong command code in response
TEST_F(FwBranchTest, DotCAKInstall_WrongCommand)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    // Overwrite command code to trigger the warning branch
    auto* resp = reinterpret_cast<nsm_common_resp*>(msg->payload);
    resp->command = 0xFF;
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [8] DotCAKBypass — branches: payloadTooShort, decodeError, cc!=SUCCESS,
// success
// ===========================================================================

TEST_F(FwBranchTest, DotCAKBypass_PayloadTooShort)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, 1));
}

TEST_F(FwBranchTest, DotCAKBypass_DecodeError)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotCAKBypass_NonSuccessCC)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_bypass_resp(0, NSM_ERROR, 0x1234, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotCAKBypass_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_bypass_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [9] ImageCopyControl — switch(requestType) branches
// ===========================================================================

// ImageCopyControl tests are extensively covered in nsm_firmware_cmd_parse_test

// ===========================================================================
// [10] DotLock — branches: nullptr, error, success with/without outputFile
// ===========================================================================

TEST_F(FwBranchTest, DotLock_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotLock_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotLock_SuccessNoOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xAA);
    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotLock_SuccessWithOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output", kOutputFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xBB);
    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [11] DotCAKRotate — branches: nullptr, decode error, cc error, success
// ===========================================================================

TEST_F(FwBranchTest, DotCAKRotate_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotCAKRotate_DecodeError)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotCAKRotate_NonSuccessCC)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_cak_rotate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0);
    encode_nsm_dot_cak_rotate_resp(0, NSM_ERROR, 0x1234, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotCAKRotate_SuccessNoOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_cak_rotate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xAA);
    encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(),
                                   msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotCAKRotate_SuccessWithOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile, "--output", kOutputFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_cak_rotate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xBB);
    encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(),
                                   msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [12] DotUnlockChallenge — branches
// ===========================================================================

TEST_F(FwBranchTest, DotUnlockChallenge_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotUnlockChallenge_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output", kOutputFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotUnlockChallenge_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output", kOutputFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_unlock_challenge_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> challenge(STATIC_CHALLENGE_SIZE, 0x42);
    encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, ERR_NULL,
                                         challenge.data(), msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [13] DotUnlock — branches
// ===========================================================================

TEST_F(FwBranchTest, DotUnlock_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotUnlock_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlock", {"--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_unlock_resp(0, NSM_ERROR, 0x5678, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotUnlock_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlock", {"--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_unlock_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [14] DotGetInfo — branches
// ===========================================================================

TEST_F(FwBranchTest, DotGetInfo_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotGetInfo_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotGetInfo_SuccessNoOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xCC);
    encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, ERR_NULL, 1, 0, 5,
                                 dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotGetInfo_SuccessWithOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotGetInfo", {"--output", kOutputFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xDD);
    encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, ERR_NULL, 1, 0, 5,
                                 dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [15] DotGetStatus — branches: nullptr, error, success with status 0-3
// ===========================================================================

TEST_F(FwBranchTest, DotGetStatus_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotGetStatus_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotGetStatus_AllStatuses)
{
    // status & 0x03: 0=Uninitialized, 1=Volatile, 2=Mutable Locked,
    //                3=Mutable Disabled
    for (uint8_t status = 0; status <= 3; ++status)
    {
        CLI::App app;
        setupFirmwareCommands(app);

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_dot_get_status_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, status, msg);
        EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
    }
}

// ===========================================================================
// [16] DotDisable — branches
// ===========================================================================

TEST_F(FwBranchTest, DotDisable_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotDisable_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set cc to non-zero (error) so decode returns early before any OOB read.
    msg->payload[1] = 1;
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotDisable_SuccessNoOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_disable_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xEE);
    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotDisable_SuccessWithOutput)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output", kOutputFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_disable_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xFF);
    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [17] DotOverride — branches
// ===========================================================================

TEST_F(FwBranchTest, DotOverride_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotOverride_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotOverride",
        {"--vendor_signature_auth_scheme", "0", "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_override_resp(0, NSM_ERROR, 0x1111, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotOverride_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotOverride",
        {"--vendor_signature_auth_scheme", "0", "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_override_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [18] DotRecovery — branches
// ===========================================================================

TEST_F(FwBranchTest, DotRecovery_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(nullptr, 0));
}

TEST_F(FwBranchTest, DotRecovery_Error)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotRecovery", {"--dot_blob", kDotBlobFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_recovery_resp(0, NSM_ERROR, 0x2222, msg);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranchTest, DotRecovery_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotRecovery", {"--dot_blob", kDotBlobFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_recovery_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::firmware
