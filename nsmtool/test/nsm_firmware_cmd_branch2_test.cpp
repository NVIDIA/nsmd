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

static const std::string kEcdsaKeyFile = "/tmp/fwb2_test_ecdsa_96b.bin";
static const std::string kLmsKeyFile = "/tmp/fwb2_test_lms_48b.bin";
static const std::string kSignatureFile = "/tmp/fwb2_test_sig_1840b.bin";
static const std::string kDotBlobFile = "/tmp/fwb2_test_dotblob_1024b.bin";
static const std::string kChallengeFile = "/tmp/fwb2_test_challenge_32b.bin";
static const std::string kOutputFile = "/tmp/fwb2_test_output.bin";

class FwBranch2Test : public ::testing::Test
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
// [0] GetRotInformation — additional success-path branch coverage
// ===========================================================================

// Build type = Release (1)
TEST_F(FwBranch2Test, GetRotInformation_BuildTypeRelease)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    fw_info.fq_resp_hdr.background_copy_policy = 0;
    fw_info.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_NO_FAILOVER;

    nsm_firmware_slot_info slot = {};
    slot.build_type = 1;          // Release
    slot.signing_type = 0;        // Debug
    slot.write_protect_state = 0; // Disabled
    slot.firmware_state = 0;      // Unknown
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Signing type = External (2)
TEST_F(FwBranch2Test, GetRotInformation_SigningTypeExternal)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.signing_type = 2; // External
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Signing type = DOT (4)
TEST_F(FwBranch2Test, GetRotInformation_SigningTypeDOT)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.signing_type = 4; // DOT
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Write protect state = Enabled (1)
TEST_F(FwBranch2Test, GetRotInformation_WriteProtectEnabled)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.write_protect_state = 1; // Enabled
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Pending Activation (2)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStatePendingActivation)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 2; // Pending Activation
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Staged (3)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStateStaged)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 3; // Staged
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Write in progress (4)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStateWriteInProgress)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 4; // Write in progress
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Inactive (5)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStateInactive)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 5; // Inactive
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Failed authentication (6)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStateFailedAuth)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 6; // Failed authentication
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Pending image copy (7)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStatePendingCopy)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 7; // Pending image copy
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Image copy in progress (8)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStateCopyInProgress)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 8; // Image copy in progress
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Firmware state = Failed image copy (9)
TEST_F(FwBranch2Test, GetRotInformation_FirmwareStateFailedCopy)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    nsm_firmware_slot_info slot = {};
    slot.firmware_state = 9; // Failed image copy
    fw_info.slot_info = &slot;

    std::vector<uint8_t> buf(2048, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Error path for GetRotInformation
TEST_F(FwBranch2Test, GetRotInformation_DecodeError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// Multiple slots with varied data
TEST_F(FwBranch2Test, GetRotInformation_ThreeSlots)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 3;
    fw_info.fq_resp_hdr.background_copy_policy = 1; // Enabled
    fw_info.fq_resp_hdr.active_slot = 1;
    fw_info.fq_resp_hdr.active_keyset = 2;
    fw_info.fq_resp_hdr.global_failover_policy =
        NSM_ROT_GLOBAL_FAILOVER_POLICY_AUTOMATIC_FAILOVER;

    nsm_firmware_slot_info slots[3] = {};
    slots[0].firmware_state = 1;
    slots[0].build_type = 0;
    slots[0].signing_type = 1;
    slots[0].write_protect_state = 1;

    slots[1].firmware_state = 3;
    slots[1].build_type = 1;
    slots[1].signing_type = 2;
    slots[1].write_protect_state = 0;

    slots[2].firmware_state = 5;
    slots[2].build_type = 99;
    slots[2].signing_type = 4;
    slots[2].write_protect_state = 99;

    fw_info.slot_info = slots;

    std::vector<uint8_t> buf(4096, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [1] IrreversibleConfig — QUERY success path
// ===========================================================================

TEST_F(FwBranch2Test, IrreversibleConfig_Query_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});

    nsm_firmware_irreversible_config_request_0_resp cfg_resp{};
    cfg_resp.irreversible_config_state = 1;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_irreversible_config_request_0_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_irreversible_config_request_0_resp(
        0, NSM_SUCCESS, ERR_NULL, &cfg_resp, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// QUERY with state = 0
TEST_F(FwBranch2Test, IrreversibleConfig_Query_StateZero)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});

    nsm_firmware_irreversible_config_request_0_resp cfg_resp{};
    cfg_resp.irreversible_config_state = 0;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_irreversible_config_request_0_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_irreversible_config_request_0_resp(
        0, NSM_SUCCESS, ERR_NULL, &cfg_resp, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ENABLE success path
TEST_F(FwBranch2Test, IrreversibleConfig_Enable_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "2"});

    nsm_firmware_irreversible_config_request_2_resp cfg_resp{};
    cfg_resp.nonce = 0x123456789ABCDEF0;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_irreversible_config_request_2_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_irreversible_config_request_2_resp(
        0, NSM_SUCCESS, ERR_NULL, &cfg_resp, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [2] QueryCodeAuthKeyPerm — success with permission data
// ===========================================================================

TEST_F(FwBranch2Test, QueryCodeAuthKeyPerm_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});

    uint8_t bitmapLen = 4;
    std::vector<uint8_t> activeBitmap(bitmapLen, 0xFF);
    std::vector<uint8_t> pendingBitmap(bitmapLen, 0xAA);
    std::vector<uint8_t> efuseBitmap(bitmapLen, 0x55);
    std::vector<uint8_t> pendingEfuseBitmap(bitmapLen, 0x0F);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_query_resp) +
                             4 * bitmapLen);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_query_resp(
        0, NSM_SUCCESS, ERR_NULL, 1, 2, bitmapLen, activeBitmap.data(),
        pendingBitmap.data(), efuseBitmap.data(), pendingEfuseBitmap.data(),
        msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// Success with empty bitmaps
TEST_F(FwBranch2Test, QueryCodeAuthKeyPerm_EmptyBitmaps)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});

    uint8_t bitmapLen = 1;
    std::vector<uint8_t> bitmap(bitmapLen, 0x00);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_query_resp) +
                             4 * bitmapLen);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_query_resp(
        0, NSM_SUCCESS, ERR_NULL, 0, 0, bitmapLen, bitmap.data(), bitmap.data(),
        bitmap.data(), bitmap.data(), msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [3] UpdateCodeAuthKeyPerm — individual bit tests
// ===========================================================================

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit0Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 0); // Automatic
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit1Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 1); // Self-Contained
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit2Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 2); // Medium-specific reset
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit3Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 3); // System reboot
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit4Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 4); // DC power cycle
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit5Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 5); // AC power cycle
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit16Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 16); // Warm Reset
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit17Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 17); // Hot Reset
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_Bit18Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    uint32_t updateMethod = (1u << 18); // Function Level Reset
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_code_auth_key_perm_update_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL,
                                              updateMethod, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [4] QueryFirmwareSecurityVersion — success
// ===========================================================================

TEST_F(FwBranch2Test, QueryFirmwareSecurityVersion_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_security_version_number_resp sec_info{};
    sec_info.active_component_security_version = htole16(10);
    sec_info.pending_component_security_version = htole16(11);
    sec_info.minimum_security_version = htole16(5);
    sec_info.pending_minimum_security_version = htole16(6);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_security_version_number_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_firmware_security_version_number_resp(
        0, NSM_SUCCESS, ERR_NULL, &sec_info, msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [5] UpdateMinSecurityVersion — individual bit tests
// ===========================================================================

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit0Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 0);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit1Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 1);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit2Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 2);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit3Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 3);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit4Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 4);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit5Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 5);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit16Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 16);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_Bit17Only)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = (1u << 17);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_NoBits)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});

    nsm_firmware_update_min_sec_ver_resp sec_info{};
    sec_info.update_methods = 0;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_update_min_sec_ver_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &sec_info,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [6] SetRoTProperty — success path
// ===========================================================================

TEST_F(FwBranch2Test, SetRoTProperty_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "0", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_set_rot_property_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// Success with different property types
TEST_F(FwBranch2Test, SetRoTProperty_SuccessProperty1)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "1", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_set_rot_property_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, SetRoTProperty_SuccessProperty2)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "2", "-a", "42", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_set_rot_property_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, SetRoTProperty_SuccessProperty3)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "3", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_firmware_set_rot_property_resp_command));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [7] DotCAKInstall — additional branches
// ===========================================================================

TEST_F(FwBranch2Test, DotCAKInstall_SuccessWithVerbose)
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
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// Non-success completion code (rc == success but cc != success)
TEST_F(FwBranch2Test, DotCAKInstall_NonSuccessCC)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_install_resp(0, NSM_ERROR, 0x1234, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [8] DotCAKBypass — additional completion code checks
// ===========================================================================

TEST_F(FwBranch2Test, DotCAKBypass_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // payloadLength < sizeof(nsm_common_resp)
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(nullptr, 0));
}

// ===========================================================================
// [9] ImageCopyControl — success paths for QueryProgress
// ===========================================================================

TEST_F(FwBranch2Test, ImageCopyControl_QueryProgress_DefaultStatus)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_query_progress_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_query_progress_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_SUCCESS;
    resp->hdr.data_size =
        htole16(sizeof(nsm_firmware_image_copy_control_query_progress_resp));
    resp->image_copy_control_query.image_copy_status = 99; // Unknown (default)
    resp->image_copy_control_query.image_copy_progress = 0;
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Progress > UNSUPPORTED_PROGRESS_PERCENT (101) — triggers error
TEST_F(FwBranch2Test, ImageCopyControl_QueryProgress_InvalidProgress)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_query_progress_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_query_progress_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_SUCCESS;
    resp->hdr.data_size =
        htole16(sizeof(nsm_firmware_image_copy_control_query_progress_resp));
    resp->image_copy_control_query.image_copy_status =
        NSM_IMAGE_COPY_NOT_TRIGGERED;
    resp->image_copy_control_query.image_copy_progress = 200; // > 101
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Progress = 101 (UNSUPPORTED_PROGRESS_PERCENT) — boundary
TEST_F(FwBranch2Test, ImageCopyControl_QueryProgress_UnsupportedProgress)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_query_progress_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_query_progress_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_SUCCESS;
    resp->hdr.data_size =
        htole16(sizeof(nsm_firmware_image_copy_control_query_progress_resp));
    resp->image_copy_control_query.image_copy_status =
        NSM_IMAGE_COPY_IN_PROGRESS;
    resp->image_copy_control_query.image_copy_progress = 101; // Boundary
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// QueryProgress error with non-null reason code
TEST_F(FwBranch2Test, ImageCopyControl_QueryProgress_ErrorWithReasonCode)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_query_progress_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_query_progress_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_ERR_INVALID_DATA;
    resp->hdr.data_size = 0;
    // Set reason code in the reserved field
    resp->hdr.reserved = htole16(ERR_UPDATE_IN_PROGRESS);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// QueryProgress error with cc only (reason_code = ERR_NULL)
TEST_F(FwBranch2Test, ImageCopyControl_QueryProgress_ErrorCcOnly)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_query_progress_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_query_progress_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_ERR_INVALID_STATE_FOR_COMMAND;
    resp->hdr.data_size = 0;
    resp->hdr.reserved = 0; // ERR_NULL
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Initiate copy success
TEST_F(FwBranch2Test, ImageCopyControl_InitiateCopy_Success)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "1", "-n", "1", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_initiate_copy_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_SUCCESS;
    resp->hdr.data_size = 0;
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Initiate copy error with reason code
TEST_F(FwBranch2Test, ImageCopyControl_InitiateCopy_ErrorWithReasonCode)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "1", "-n", "1", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_initiate_copy_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_ERR_INVALID_DATA;
    resp->hdr.data_size = 0;
    resp->hdr.reserved = htole16(ERR_IMAGE_COPY_IN_PROGRESS);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Initiate copy error with cc only
TEST_F(FwBranch2Test, ImageCopyControl_InitiateCopy_ErrorCcOnly)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "1", "-n", "1", "-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_image_copy_control_initiate_copy_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<
        nsm_firmware_image_copy_control_initiate_copy_resp_command*>(
        msg->payload);
    resp->hdr.completion_code = NSM_ERR_INVALID_REQUEST_TYPE;
    resp->hdr.data_size = 0;
    resp->hdr.reserved = 0;
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Default (unknown request type)
TEST_F(FwBranch2Test, ImageCopyControl_DefaultRequestType)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // Can't set request type > 1 via CLI (range check), so parse with valid
    // then we test the error path from decode
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    // Just exercise the decode error path
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Individual image copy status values
TEST_F(FwBranch2Test, ImageCopyControl_QueryProgress_AllStatusValues)
{
    for (uint8_t status = 0; status <= 8; ++status)
    {
        CLI::App app;
        setupFirmwareCommands(app);
        parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
                sizeof(
                    nsm_firmware_image_copy_control_query_progress_resp_command),
            0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        auto* resp = reinterpret_cast<
            nsm_firmware_image_copy_control_query_progress_resp_command*>(
            msg->payload);
        resp->hdr.completion_code = NSM_SUCCESS;
        resp->hdr.data_size = htole16(
            sizeof(nsm_firmware_image_copy_control_query_progress_resp));
        resp->image_copy_control_query.image_copy_status = status;
        resp->image_copy_control_query.image_copy_progress = 50;
        EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
    }
}

// ===========================================================================
// [10] DotLock — write failure branch (bad output path)
// ===========================================================================

TEST_F(FwBranch2Test, DotLock_SuccessWriteFailure)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output",
                     "/tmp/nonexistent_dir_fwb2/output.bin"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xCC);
    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [11] DotCAKRotate — write failure and success without output
// ===========================================================================

TEST_F(FwBranch2Test, DotCAKRotate_SuccessWriteFailure)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile, "--output",
                     "/tmp/nonexistent_dir_fwb2/output.bin"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_cak_rotate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xDD);
    encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(),
                                   msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [12] DotUnlockChallenge — write failure
// ===========================================================================

TEST_F(FwBranch2Test, DotUnlockChallenge_SuccessWriteFailure)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output",
                     "/tmp/nonexistent_dir_fwb2/output.bin"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_unlock_challenge_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> challenge(STATIC_CHALLENGE_SIZE, 0x42);
    encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, ERR_NULL,
                                         challenge.data(), msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [13] DotUnlock — additional
// ===========================================================================

TEST_F(FwBranch2Test, DotUnlock_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(nullptr, 0));
}

// ===========================================================================
// [14] DotGetInfo — write failure
// ===========================================================================

TEST_F(FwBranch2Test, DotGetInfo_SuccessWriteFailure)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotGetInfo",
                    {"--output", "/tmp/nonexistent_dir_fwb2/output.bin"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xEE);
    encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, ERR_NULL, 1, 0, 5,
                                 dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [15] DotGetStatus — success with masked status values
// ===========================================================================

TEST_F(FwBranch2Test, DotGetStatus_SuccessStatusBitsHigher)
{
    // Test with status bits above bit 1 set (masks to 0x03)
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, 0xFC, msg);
    // 0xFC & 0x03 = 0 -> Uninitialized
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, DotGetStatus_SuccessStatusMasked1)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, 0xFD, msg);
    // 0xFD & 0x03 = 1 -> Volatile
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, DotGetStatus_SuccessStatusMasked2)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, 0xFE, msg);
    // 0xFE & 0x03 = 2 -> Mutable Locked
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST_F(FwBranch2Test, DotGetStatus_SuccessStatusMasked3)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_get_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, 0xFF, msg);
    // 0xFF & 0x03 = 3 -> Mutable Disabled
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [16] DotDisable — write failure
// ===========================================================================

TEST_F(FwBranch2Test, DotDisable_SuccessWriteFailure)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output",
                     "/tmp/nonexistent_dir_fwb2/output.bin"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_dot_disable_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE, 0xFF);
    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob.data(), msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// [17] DotOverride — null pointer
// ===========================================================================

TEST_F(FwBranch2Test, DotOverride_NullPtr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(nullptr, 0));
}

// ===========================================================================
// [18] DotRecovery — additional
// ===========================================================================

TEST_F(FwBranch2Test, DotRecovery_NullPtr2)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(nullptr, 0));
}

// ===========================================================================
// createRequestMsg coverage for various commands
// ===========================================================================

TEST_F(FwBranch2Test, GetRotInformation_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[0]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, IrreversibleConfig_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});
    auto [rc, reqMsg] = commands[1]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, QueryCodeAuthKeyPerm_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[2]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_CreateRequest_Type0)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});
    auto [rc, reqMsg] = commands[3]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_CreateRequest_Type1WithKeys)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "UpdateCodeAuthKeyPerm",
        {"-r", "1", "-c", "1", "-i", "0", "-d", "0", "-n", "0", "-k", "0,1"});
    auto [rc, reqMsg] = commands[3]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_CreateRequest_Type0WithKeys)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "UpdateCodeAuthKeyPerm",
        {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0", "-k", "0"});
    auto [rc, reqMsg] = commands[3]->createRequestMsg();
    // requestType=0 with keys should error
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST_F(FwBranch2Test, UpdateCodeAuthKeyPerm_CreateRequest_Type1BadIndex)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "UpdateCodeAuthKeyPerm",
        {"-r", "1", "-c", "1", "-i", "0", "-d", "0", "-n", "0", "-k", "abc"});
    auto [rc, reqMsg] = commands[3]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST_F(FwBranch2Test, QueryFirmwareSecurityVersion_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[4]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, UpdateMinSecurityVersion_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion",
                    {"-r", "0", "-n", "0", "-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[5]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, SetRoTProperty_CreateRequest_Property0)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "SetRoTProperty",
        {"-p", "0", "-r", "1", "-l", "0", "-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[6]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, SetRoTProperty_CreateRequest_Property1)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "SetRoTProperty",
        {"-p", "1", "-u", "1", "-l", "0", "-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[6]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, SetRoTProperty_CreateRequest_Property2)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "SetRoTProperty",
        {"-p", "2", "-a", "100", "-l", "0", "-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[6]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, SetRoTProperty_CreateRequest_Property3)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "3", "-g", "1", "-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[6]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

TEST_F(FwBranch2Test, DotCAKBypass_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    auto [rc, reqMsg] = commands[8]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(reqMsg.empty());
}

// ===========================================================================
// readFileAsBytes / validateFileSize coverage
// ===========================================================================

TEST_F(FwBranch2Test, ReadFileAsBytes_EmptyFilename)
{
    auto data = readFileAsBytes("");
    EXPECT_TRUE(data.empty());
}

TEST_F(FwBranch2Test, ReadFileAsBytes_NonexistentFile)
{
    auto data = readFileAsBytes("/tmp/nonexistent_fwb2_file.bin");
    EXPECT_TRUE(data.empty());
}

TEST_F(FwBranch2Test, ReadFileAsBytes_EmptyFile)
{
    createTempBinFile("/tmp/fwb2_empty.bin", 0);
    auto data = readFileAsBytes("/tmp/fwb2_empty.bin");
    EXPECT_TRUE(data.empty());
    std::remove("/tmp/fwb2_empty.bin");
}

TEST_F(FwBranch2Test, ValidateFileSize_NonexistentFile)
{
    auto result = validateFileSize("/tmp/nonexistent_fwb2_validate.bin", 96,
                                   "test");
    EXPECT_FALSE(result.empty());
}

TEST_F(FwBranch2Test, ValidateFileSize_WrongSize)
{
    createTempBinFile("/tmp/fwb2_wrongsize.bin", 50);
    auto result = validateFileSize("/tmp/fwb2_wrongsize.bin", 96, "test");
    EXPECT_FALSE(result.empty());
    std::remove("/tmp/fwb2_wrongsize.bin");
}

TEST_F(FwBranch2Test, ValidateFileSize_CorrectSize)
{
    auto result = validateFileSize(kEcdsaKeyFile, 96, "test");
    EXPECT_TRUE(result.empty());
}

// ===========================================================================
// DotCAKInstall createRequestMsg branches
// ===========================================================================

TEST_F(FwBranch2Test, DotCAKInstall_CreateRequest_ECDSA)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    auto [rc, reqMsg] = commands[7]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(FwBranch2Test, DotCAKInstall_CreateRequest_Hybrid)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--cak_lms_key", kLmsKeyFile,
                     "--lak_key_auth_scheme", "1", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_lms_key", kLmsKeyFile});
    auto [rc, reqMsg] = commands[7]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(FwBranch2Test, DotCAKInstall_CreateRequest_MissingCakLms)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    auto [rc, reqMsg] = commands[7]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

TEST_F(FwBranch2Test, DotCAKInstall_CreateRequest_MissingLakLms)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "1",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    auto [rc, reqMsg] = commands[7]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

// ===========================================================================
// DotLock createRequestMsg branches
// ===========================================================================

TEST_F(FwBranch2Test, DotLock_CreateRequest_ECDSA)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    auto [rc, reqMsg] = commands[10]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(FwBranch2Test, DotLock_CreateRequest_StaticChallenge)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotLock",
        {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key", kEcdsaKeyFile,
         "--lak_key_auth_scheme", "0", "--lak_ecdsa_key", kEcdsaKeyFile,
         "--unlock_method", "2", "--static_challenge", kChallengeFile,
         "--lock_signature_auth_scheme", "0", "--signature", kSignatureFile});
    auto [rc, reqMsg] = commands[10]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(FwBranch2Test, DotLock_CreateRequest_MissingStaticChallenge)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "2",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    auto [rc, reqMsg] = commands[10]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

// ===========================================================================
// DotDisable createRequestMsg branches
// ===========================================================================

TEST_F(FwBranch2Test, DotDisable_CreateRequest_ECDSA)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    auto [rc, reqMsg] = commands[16]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(FwBranch2Test, DotDisable_CreateRequest_StaticChallenge)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "2",
                     "--static_challenge", kChallengeFile,
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    auto [rc, reqMsg] = commands[16]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(FwBranch2Test, DotDisable_CreateRequest_MissingStaticChallenge)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "2",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    auto [rc, reqMsg] = commands[16]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_ERROR);
}

// ===========================================================================
// DotCAKRotate createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotCAKRotate_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});
    auto [rc, reqMsg] = commands[11]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// DotUnlockChallenge createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotUnlockChallenge_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output", kOutputFile});
    auto [rc, reqMsg] = commands[12]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// DotUnlock createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotUnlock_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlock", {"--signature", kSignatureFile});
    auto [rc, reqMsg] = commands[13]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// DotGetInfo createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotGetInfo_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    auto [rc, reqMsg] = commands[14]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// DotGetStatus createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotGetStatus_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    auto [rc, reqMsg] = commands[15]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// DotOverride createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotOverride_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotOverride",
        {"--vendor_signature_auth_scheme", "0", "--signature", kSignatureFile});
    auto [rc, reqMsg] = commands[17]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// DotRecovery createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, DotRecovery_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotRecovery", {"--dot_blob", kDotBlobFile});
    auto [rc, reqMsg] = commands[18]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// ImageCopyControl createRequestMsg
// ===========================================================================

TEST_F(FwBranch2Test, ImageCopyControl_CreateRequest_MismatchArraySizes)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // classification count != identifier count
    parseSubcmdArgs(
        app, "ImageCopyControl",
        {"-r", "1", "-n", "1", "-c", "1", "-c", "2", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[9]->createRequestMsg();
    EXPECT_EQ(rc, -1);
}

TEST_F(FwBranch2Test, ImageCopyControl_CreateRequest_CountMismatch)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // n=2 but only 1 component
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "1", "-n", "2", "-c", "1", "-i", "0", "-d", "0"});
    auto [rc, reqMsg] = commands[9]->createRequestMsg();
    EXPECT_EQ(rc, -1);
}

} // namespace nsmtool::firmware
