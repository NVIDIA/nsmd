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

// Create a temporary binary file filled with zeros of the given size.
// Returns the path to the created file.
static std::string createTempBinFile(const std::string& path, size_t size)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    std::vector<uint8_t> zeros(size, 0);
    f.write(reinterpret_cast<const char*>(zeros.data()),
            static_cast<std::streamsize>(size));
    f.close();
    return path;
}

// ---- File paths for temp key/signature files --------------------------------
static const std::string kEcdsaKeyFile = "/tmp/fw_test_ecdsa_96b.bin";
static const std::string kLmsKeyFile = "/tmp/fw_test_lms_48b.bin";
static const std::string kSignatureFile = "/tmp/fw_test_sig_1840b.bin";
static const std::string kDotBlobFile = "/tmp/fw_test_dotblob_1024b.bin";
static const std::string kChallengeFile = "/tmp/fw_test_challenge_32b.bin";
static const std::string kOutputFile = "/tmp/fw_test_output.bin";
static const std::string kEmptyFile = "/tmp/fw_test_empty_0b.bin";

// ---- Fixture that creates temp files ----------------------------------------

class NsmFirmwareCmdParseTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        createTempBinFile(kEcdsaKeyFile, 96);
        createTempBinFile(kLmsKeyFile, 48);
        createTempBinFile(kSignatureFile, 1840);
        createTempBinFile(kDotBlobFile, 1024);
        createTempBinFile(kChallengeFile, 32);
        createTempBinFile(kEmptyFile, 0);
    }

    void TearDown() override
    {
        std::remove(kEcdsaKeyFile.c_str());
        std::remove(kLmsKeyFile.c_str());
        std::remove(kSignatureFile.c_str());
        std::remove(kDotBlobFile.c_str());
        std::remove(kChallengeFile.c_str());
        std::remove(kOutputFile.c_str());
        std::remove(kEmptyFile.c_str());
    }
};

// ---- [0] GetRotInformation --------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, GetRotInformation_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});
    EXPECT_NO_THROW(commands[0]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, GetRotInformation_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});
    // Buffer too small → decoder fails → error path in parseResponseMsg
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, GetRotInformation_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    // Allocate a generous buffer for the response (0 firmware slots)
    std::vector<uint8_t> buf(512, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    nsm_firmware_erot_state_info_resp fw_info = {};
    fw_info.fq_resp_hdr.firmware_slot_count = 0;
    fw_info.slot_info = nullptr;
    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- [1] IrreversibleConfig -------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Query_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});
    EXPECT_NO_THROW(commands[1]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Disable_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "1"});
    EXPECT_NO_THROW(commands[1]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Enable_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "2"});
    EXPECT_NO_THROW(commands[1]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Query_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_irreversible_config_request_0_resp cfg{};
    cfg.irreversible_config_state = 1;
    encode_nsm_firmware_irreversible_config_request_0_resp(0, NSM_SUCCESS,
                                                           ERR_NULL, &cfg, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Disable_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "1"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_1_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_irreversible_config_request_1_resp(0, NSM_SUCCESS,
                                                           ERR_NULL, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Enable_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "2"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_2_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_irreversible_config_request_2_resp cfg{};
    cfg.nonce = 0xDEADBEEFCAFEBABEULL;
    encode_nsm_firmware_irreversible_config_request_2_resp(0, NSM_SUCCESS,
                                                           ERR_NULL, &cfg, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Unknown_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_request_0_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ---- [2] QueryCodeAuthKeyPerm -----------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, QueryCodeAuthKeyPerm_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});
    EXPECT_NO_THROW(commands[2]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, QueryCodeAuthKeyPerm_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});

    // 0-length bitmaps for simplicity
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t emptyBitmap[1] = {0};
    encode_nsm_code_auth_key_perm_query_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, 0,
                                             emptyBitmap, emptyBitmap,
                                             emptyBitmap, emptyBitmap, msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, QueryCodeAuthKeyPerm_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ---- [3] UpdateCodeAuthKeyPerm ----------------------------------------------

TEST_F(NsmFirmwareCmdParseTest,
       UpdateCodeAuthKeyPerm_RequestType0_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "UpdateCodeAuthKeyPerm",
        {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "12345"});
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest,
       UpdateCodeAuthKeyPerm_RequestType0WithKeys_CreateRequestMsg)
{
    // requestType=0 with non-empty keys string → early return error
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "12345",
                     "-k", "1,2,3"});
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest,
       UpdateCodeAuthKeyPerm_RequestType1_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "1", "-c", "1", "-i", "0", "-d", "0", "-n", "12345",
                     "-k", "0,1,2"});
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, UpdateCodeAuthKeyPerm_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Set all update method bits to cover all if-branches in parseResponseMsg
    uint32_t allBits = 0x000700FF;
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL, allBits,
                                              msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ---- [4] QueryFirmwareSecurityVersion ---------------------------------------

TEST_F(NsmFirmwareCmdParseTest, QueryFirmwareSecurityVersion_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});
    EXPECT_NO_THROW(commands[4]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, QueryFirmwareSecurityVersion_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_security_version_number_resp sec{};
    sec.active_component_security_version = 1;
    sec.minimum_security_version = 1;
    encode_nsm_query_firmware_security_version_number_resp(0, NSM_SUCCESS,
                                                           ERR_NULL, &sec, msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- [5] UpdateMinSecurityVersion -------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, UpdateMinSecurityVersion_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion", {"-r", "0", "-n", "9999"});
    EXPECT_NO_THROW(commands[5]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, UpdateMinSecurityVersion_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion", {"-r", "0", "-n", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_update_min_sec_ver_resp resp{};
    // Set bits to exercise all if-branches for update methods
    resp.update_methods = 0x000600FF;
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &resp,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- [6] SetRoTProperty -----------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_RedundancyPolicy_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty", {"-p", "0", "-r", "1", "-l", "0"});
    EXPECT_NO_THROW(commands[6]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_InbandUpdatePolicy_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty", {"-p", "1", "-u", "1", "-l", "1"});
    EXPECT_NO_THROW(commands[6]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_ApSkuId_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty",
                    {"-p", "2", "-a", "0x1234", "-l", "0"});
    EXPECT_NO_THROW(commands[6]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty", {"-p", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_set_rot_property_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_firmware_set_rot_property_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ---- [7] DotCAKInstall ------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_EcdsaOnly_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_Hybrid_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--cak_lms_key", kLmsKeyFile,
                     "--lak_key_auth_scheme", "1", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_lms_key", kLmsKeyFile});
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_NullResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // Pass nullptr → early-return null-check in parseResponseMsg
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(nullptr, 0));
}

// ---- [8] DotCAKBypass -------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKBypass_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[8]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKBypass_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_bypass_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKBypass_ParseResponseCCError)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Non-success cc → different branch in parseResponseMsg
    encode_nsm_dot_cak_bypass_resp(0, NSM_ERR_INVALID_DATA, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKBypass_ParseResponseTooShort)
{
    CLI::App app;
    setupFirmwareCommands(app);
    std::vector<uint8_t> buf(2, 0); // too short
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ---- [9] ImageCopyControl ---------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_QueryProgress_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});
    EXPECT_NO_THROW(commands[9]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_InitiateCopy_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // Component count mismatch: -n 0 but no -c/-i/-d → returns early error
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "1", "-n", "1", "-c", "1", "-i", "0", "-d", "0"});
    EXPECT_NO_THROW(commands[9]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_QueryProgress_ParseResponse)
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
    resp->image_copy_control_query.image_copy_status = NSM_IMAGE_COPY_COMPLETE;
    resp->image_copy_control_query.image_copy_progress = 100;
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_QueryProgress_StatusVariants)
{
    // Test each status code for the switch statement coverage
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    for (uint8_t status = 0; status <= 7; ++status)
    {
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

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_InitiateCopy_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "1"});

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

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_Default_ParseResponse)
{
    // requestType=0 (default parse) but pass a response to trigger default
    CLI::App app;
    setupFirmwareCommands(app);
    // No -r → requestType stays at default (0), which is query progress
    // (default path in switch is unreachable via normal requestType values,
    // but error path in query progress is reachable with bad cc)
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
    resp->hdr.data_size =
        htole16(sizeof(nsm_firmware_image_copy_control_query_progress_resp));
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- [10] DotLock -----------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotLock_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    // Error response path
    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_lock_resp(0, NSM_ERR_INVALID_DATA, 0x0001, dotBlob, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(nullptr, 0));
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_ParseResponseWithOutputFile)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output", kOutputFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ---- [11] DotCAKRotate ------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});
    EXPECT_NO_THROW(commands[11]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_rotate_resp(0, NSM_ERR_INVALID_DATA, 0x0002, dotBlob,
                                   msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(nullptr, 0));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_ParseResponseWithOutputFile)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile, "--output", kOutputFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- [12] DotUnlockChallenge ------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotUnlockChallenge_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output", kOutputFile});
    EXPECT_NO_THROW(commands[12]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotUnlockChallenge_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output", kOutputFile});

    uint8_t challenge[32] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, ERR_NULL, challenge,
                                         msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotUnlockChallenge_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output", kOutputFile});

    uint8_t challenge[32] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_unlock_challenge_resp(0, NSM_ERR_INVALID_DATA, 0x0003,
                                         challenge, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotUnlockChallenge_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(nullptr, 0));
}

// ---- [13] DotUnlock ---------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotUnlock_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlock", {"--signature", kSignatureFile});
    EXPECT_NO_THROW(commands[13]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotUnlock_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlock", {"--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_unlock_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotUnlock_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlock", {"--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_unlock_resp(0, NSM_ERR_INVALID_DATA, 0x0004, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotUnlock_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(nullptr, 0));
}

// ---- [14] DotGetInfo --------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotGetInfo_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // DotGetInfo requires --mctp_eid; added via "-m 1" in parseSubcmdArgs
    parseSubcmdArgs(app, "DotGetInfo", {});
    EXPECT_NO_THROW(commands[14]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotGetInfo_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotGetInfo", {});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, ERR_NULL, 1, 0x02, 5, dotBlob,
                                 msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotGetInfo_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotGetInfo", {});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_info_resp(0, NSM_ERR_INVALID_DATA, 0x0005, 0, 0, 0,
                                 dotBlob, msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotGetInfo_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(nullptr, 0));
}

TEST_F(NsmFirmwareCmdParseTest, DotGetInfo_ParseResponseWithOutputFile)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotGetInfo", {"--output", kOutputFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, ERR_NULL, 2, 0x01, 3, dotBlob,
                                 msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ---- [15] DotGetStatus ------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotGetStatus_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[15]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotGetStatus_ParseResponseAllStatuses)
{
    // Status values 0-3 cover the switch cases in parseResponseMsg
    for (uint8_t status = 0; status <= 3; ++status)
    {
        CLI::App app;
        setupFirmwareCommands(app);

        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, status, msg);
        EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
    }
}

TEST_F(NsmFirmwareCmdParseTest, DotGetStatus_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_status_resp(0, NSM_ERR_INVALID_DATA, 0x0006, 0, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotGetStatus_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(nullptr, 0));
}

// ---- [16] DotDisable --------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotDisable_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotDisable_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob, msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotDisable_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_disable_resp(0, NSM_ERR_INVALID_DATA, 0x0007, dotBlob, msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotDisable_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(nullptr, 0));
}

TEST_F(NsmFirmwareCmdParseTest, DotDisable_ParseResponseWithOutputFile)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output", kOutputFile});

    uint8_t dotBlob[DOT_BLOB_SIZE] = {};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, dotBlob, msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ---- [17] DotOverride -------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotOverride_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotOverride",
        {"--vendor_signature_auth_scheme", "0", "--signature", kSignatureFile});
    EXPECT_NO_THROW(commands[17]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotOverride_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotOverride",
        {"--vendor_signature_auth_scheme", "0", "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_override_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotOverride_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotOverride",
        {"--vendor_signature_auth_scheme", "0", "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_override_resp(0, NSM_ERR_INVALID_DATA, 0x0008, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotOverride_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(nullptr, 0));
}

// ---- [18] DotRecovery -------------------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotRecovery_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotRecovery", {"--dot_blob", kDotBlobFile});
    EXPECT_NO_THROW(commands[18]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotRecovery_ParseResponseSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotRecovery", {"--dot_blob", kDotBlobFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_recovery_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotRecovery_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotRecovery", {"--dot_blob", kDotBlobFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_recovery_resp(0, NSM_ERR_INVALID_DATA, 0x0009, msg);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotRecovery_ParseResponseNullptr)
{
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(nullptr, 0));
}

// ---- Additional branch coverage tests ---------------------------------------

// validateFileSize: file that doesn't exist
TEST_F(NsmFirmwareCmdParseTest, ValidateFileSize_NonExistentFile)
{
    // Call validateFileSize indirectly via DotCAKInstall parse with bad file
    CLI::App app;
    setupFirmwareCommands(app);
    // Parsing with non-existent file triggers CLI::ExistingFile validator
    // (exception caught in parseSubcmdArgs)
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     "/nonexistent/file.bin", "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", "/nonexistent/file2.bin"});
    // createRequestMsg with empty file paths → early error return
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// readFileAsBytes: empty filename
TEST_F(NsmFirmwareCmdParseTest, DotOverride_EmptyFilePath)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // No --signature provided → signatureFile is empty → createRequestMsg
    // returns early from readFileAsBytes
    EXPECT_NO_THROW(commands[17]->createRequestMsg());
}

// DotCAKInstall hybrid scheme with missing LMS key
TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_HybridNoLmsKey_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // cak_key_auth_scheme=1 (hybrid) but no --cak_lms_key → error in
    // createRequestMsg
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// DotCAKInstall: lak hybrid with no lms key
TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_LakHybridNoLmsKey_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "1",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// DotCAKInstall: response with unexpected command code
TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_ParseResponseUnexpectedCmd)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    // Build response with wrong command code to trigger the "unexpected"
    // warning
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    // Overwrite command byte with wrong value
    auto* resp = reinterpret_cast<nsm_common_resp*>(msg->payload);
    resp->command = 0xFF; // Force wrong command code
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// DotCAKInstall payload too small
TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_ParseResponseTooSmall)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    // Buffer too small → payloadLength < sizeof(nsm_common_resp)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// DotLock: unlock_method=2 with static challenge file
TEST_F(NsmFirmwareCmdParseTest, DotLock_UnlockMethod2_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotLock",
        {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key", kEcdsaKeyFile,
         "--lak_key_auth_scheme", "0", "--lak_ecdsa_key", kEcdsaKeyFile,
         "--unlock_method", "2", "--static_challenge", kChallengeFile,
         "--lock_signature_auth_scheme", "0", "--signature", kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// DotLock: hybrid cak
TEST_F(NsmFirmwareCmdParseTest, DotLock_HybridCak_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "DotLock",
        {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key", kEcdsaKeyFile,
         "--cak_lms_key", kLmsKeyFile, "--lak_key_auth_scheme", "0",
         "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
         "--lock_signature_auth_scheme", "0", "--signature", kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// DotDisable: unlock_method=2 with static challenge
TEST_F(NsmFirmwareCmdParseTest, DotDisable_UnlockMethod2_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "2",
                     "--static_challenge", kChallengeFile,
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// DotCAKRotate: hybrid new_cak
TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_HybridNewCak_CreateRequestMsg)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "1", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--new_cak_lms_key", kLmsKeyFile,
                     "--lak_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[11]->createRequestMsg());
}

// UpdateCodeAuthKeyPerm: invalid key string (non-numeric)
TEST_F(NsmFirmwareCmdParseTest,
       UpdateCodeAuthKeyPerm_InvalidKeyStr_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "1", "-c", "1", "-i", "0", "-d", "0", "-n", "12345",
                     "-k", "abc,def"});
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

// ImageCopyControl: component arrays size mismatch
TEST_F(NsmFirmwareCmdParseTest,
       ImageCopyControl_ComponentMismatch_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // -c with 2 values but only 1 -i → size mismatch
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "0", "-c", "1", "-c", "2", "-i", "0"});
    EXPECT_NO_THROW(commands[9]->createRequestMsg());
}

// DotUnlockChallenge type=2 (Vendor_Unlock)
TEST_F(NsmFirmwareCmdParseTest, DotUnlockChallenge_VendorUnlock_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "2", "--output", kOutputFile});
    EXPECT_NO_THROW(commands[12]->createRequestMsg());
}

// QueryCodeAuthKeyPerm with non-zero bitmap length
TEST_F(NsmFirmwareCmdParseTest, QueryCodeAuthKeyPerm_BitmapLength_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});

    // permission_bitmap_length = 1
    uint8_t activeBitmap[1] = {0b10101010};
    uint8_t pendingBitmap[1] = {0b01010101};
    uint8_t efuseBitmap[1] = {0xFF};
    uint8_t pendingEfuseBitmap[1] = {0x01};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_resp) + 4, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_code_auth_key_perm_query_resp(
        0, NSM_SUCCESS, ERR_NULL, 0, 0, 1, activeBitmap, pendingBitmap,
        efuseBitmap, pendingEfuseBitmap, msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ============================================================
// Additional coverage tests – Phase 2
// ============================================================

// Exact-size buffer for cc != NSM_SUCCESS responses
// decode_reason_code_and_cc requires msg_len ==
//   sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp) exactly.
static constexpr size_t kErrBufSize = sizeof(nsm_msg_hdr) +
                                      sizeof(nsm_common_non_success_resp);

// Build a minimal error response buffer manually (no encoder needed).
static void buildErrResponse(std::vector<uint8_t>& buf, uint8_t cc,
                             uint16_t reason_code, uint8_t cmdCode)
{
    buf.assign(kErrBufSize, 0);
    auto* resp = reinterpret_cast<nsm_common_non_success_resp*>(
        reinterpret_cast<nsm_msg*>(buf.data())->payload);
    resp->command = cmdCode;
    resp->completion_code = cc;
    resp->reason_code = htole16(reason_code);
}

// ---- Direct utility function coverage
// ----------------------------------------

TEST_F(NsmFirmwareCmdParseTest, ValidateFileSize_CannotOpen)
{
    // Line 69: cannot open non-existent file
    auto result = validateFileSize("/nonexistent_fw_xyz.bin", 96, "test file");
    EXPECT_FALSE(result.empty());
}

TEST_F(NsmFirmwareCmdParseTest, ValidateFileSize_WrongSize)
{
    // Lines 78-80: file exists but has wrong size
    const std::string path = "/tmp/fw_wrong_size_test.bin";
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        std::vector<uint8_t> data(10, 0xAB);
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
    }
    auto result = validateFileSize(path, 96, "test file");
    EXPECT_FALSE(result.empty());
    std::remove(path.c_str());
}

TEST_F(NsmFirmwareCmdParseTest, ReadFileBytes_NonExistent)
{
    // Lines 97-98: file cannot be opened
    auto result = readFileAsBytes("/nonexistent_fw_xyz.bin");
    EXPECT_TRUE(result.empty());
}

TEST_F(NsmFirmwareCmdParseTest, ReadFileBytes_EmptyFile)
{
    // Lines 107-108: file is empty
    const std::string path = "/tmp/fw_empty_test.bin";
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
    }
    auto result = readFileAsBytes(path);
    EXPECT_TRUE(result.empty());
    std::remove(path.c_str());
}

// ---- GetRotInformation parseResponseMsg error path (lines 173-177)
// -----------

TEST_F(NsmFirmwareCmdParseTest, GetRotInformation_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    // Use error-sized buffer so decode_reason_code_and_cc succeeds
    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_GET_EROT_STATE_INFORMATION);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- GetRotInformation slot info loop (lines 208-231, 233-234)
// ---------------

TEST_F(NsmFirmwareCmdParseTest, GetRotInformation_ParseResponseWithOneSlot)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "GetRotInformation",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf(2048, 0); // large enough for aggregate encoding
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    nsm_firmware_slot_info slot{};
    slot.slot_id = 1;
    slot.build_type = 0;
    slot.signing_type = 1;
    slot.write_protect_state = 0;
    slot.firmware_state = 2;
    slot.security_version_number = 3;
    slot.signing_key_index = 4;
    slot.version_comparison_stamp = 0x100;
    memcpy(slot.firmware_version_string, "v1.0.0", 6);

    nsm_firmware_erot_state_info_resp fw_info{};
    fw_info.fq_resp_hdr.firmware_slot_count = 1;
    fw_info.fq_resp_hdr.active_slot = 1;
    fw_info.fq_resp_hdr.active_keyset = 0;
    fw_info.slot_info = &slot;

    encode_nsm_query_get_erot_state_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &fw_info, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryCodeAuthKeyPerm parseResponseMsg error (lines 354-357)
// -------------

TEST_F(NsmFirmwareCmdParseTest, QueryCodeAuthKeyPerm_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFWCodeAuthKey",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_QUERY_CODE_AUTH_KEY_PERM);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ---- UpdateCodeAuthKeyPerm parseResponseMsg error (lines 504-507)
// ------------

TEST_F(NsmFirmwareCmdParseTest, UpdateCodeAuthKeyPerm_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_UPDATE_CODE_AUTH_KEY_PERM);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryFirmwareSecurityVersion error (lines 622-625) ---------------------

TEST_F(NsmFirmwareCmdParseTest,
       QueryFirmwareSecurityVersion_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- UpdateMinSecurityVersion error (lines 713-716) -------------------------

TEST_F(NsmFirmwareCmdParseTest, UpdateMinSecurityVersion_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion", {"-r", "0", "-n", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- SetRoTProperty parse_complete_callback (lines 933-935) -----------------

TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_ApSkuIdRequired_Callback)
{
    // property=2 without --ap-sku-id triggers throw in parse_complete_callback
    CLI::App app;
    setupFirmwareCommands(app);
    // Exception is caught by parseSubcmdArgs; command should still work
    parseSubcmdArgs(app, "SetRoTProperty", {"-p", "2"});
    // No assertion needed: just verify it doesn't crash
}

// ---- SetRoTProperty parseResponseMsg error (lines 989-992) ------------------

TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty", {"-p", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_SET_ROT_PROPERTY);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ---- IrreversibleConfig error paths for all 3 request types -----------------

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Query_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_IRREVERSABLE_CONFIGURATION);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Disable_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "1"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_IRREVERSABLE_CONFIGURATION);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Enable_ParseResponseCcError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "2"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_IRREVERSABLE_CONFIGURATION);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ---- DotCAKInstall verbose mode (lines 1172-1194) ---------------------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_VerboseMode_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"-v", "--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// ---- DotCAKInstall payload too small (lines 1222-1225) ----------------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_ParseResponseTooShortPayload)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});

    // Payload less than sizeof(nsm_common_resp) = 6 bytes → early return
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERR_INVALID_DATA;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// ---- DotCAKBypass cc != NSM_SUCCESS path (lines 1328-1342) ------------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKBypass_ParseResponseCcNonSuccess)
{
    CLI::App app;
    setupFirmwareCommands(app);

    // Exact error buffer size so decode_reason_code_and_cc succeeds
    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_NULL,
                     NSM_FW_DOT_CAK_BYPASS);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ---- ImageCopyControl: component count mismatch (lines 1421-1425) -----------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_ComponentCountMismatch_Create)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // -n 2 but only 1 -c/-i/-d → componentCount(2) != compCount(1)
    parseSubcmdArgs(app, "ImageCopyControl",
                    {"-r", "1", "-n", "2", "-c", "1", "-i", "0", "-d", "0"});
    EXPECT_NO_THROW(commands[9]->createRequestMsg());
}

// ---- ImageCopyControl reason_code path (lines 1486-1489) --------------------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_QueryProgress_ReasonCodeError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    // reason_code != ERR_NULL triggers reason code branch at line 1484
    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, ERR_PROPERTY_NOT_SUPPORTED,
                     NSM_IMAGE_COPY_QUERY_PROGRESS);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- ImageCopyControl: invalid progress percent (lines 1497-1499) -----------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_InvalidProgressPercent)
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
    resp->image_copy_control_query.image_copy_status = 0;
    resp->image_copy_control_query.image_copy_progress =
        102; // > UNSUPPORTED_PROGRESS_PERCENT (101)
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- ImageCopyControl: status default case (lines 1531-1537) ----------------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_StatusUnknownDefault)
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
    resp->image_copy_control_query.image_copy_status = 0xFF; // unknown
    resp->image_copy_control_query.image_copy_progress = 50;
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- ImageCopyControl InitiateCopy reason_code (lines 1555-1572) -----------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_InitiateCopy_ReasonCodeError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "1"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_STATE_FOR_COMMAND,
                     ERR_UPDATE_IN_PROGRESS,
                     NSM_IMAGE_COPY_INITIATE_IMAGE_COPY);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- ImageCopyControl getCompletionCodeDescription all cc values
// -------------

TEST_F(NsmFirmwareCmdParseTest,
       ImageCopyControl_CompCodeDescription_InvalidState)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_STATE_FOR_COMMAND, ERR_NULL,
                     NSM_IMAGE_COPY_QUERY_PROGRESS);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest,
       ImageCopyControl_CompCodeDescription_InvalidRequestType)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_REQUEST_TYPE, ERR_NULL,
                     NSM_IMAGE_COPY_QUERY_PROGRESS);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_CompCodeDescription_Unknown)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

    // Use an unknown cc value (0x7F)
    std::vector<uint8_t> buf;
    buildErrResponse(buf, 0x7F, ERR_NULL, NSM_IMAGE_COPY_QUERY_PROGRESS);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- ImageCopyControl getReasonCodeDescription all reason codes
// --------------

TEST_F(NsmFirmwareCmdParseTest, ImageCopyControl_ReasonCode_AllKnownValues)
{
    // Cover all case labels in getReasonCodeDescription (lines 1618-1636)
    static const uint16_t reasonCodes[] = {
        ERR_PROPERTY_NOT_SUPPORTED,
        ERR_LIFESPAN_VOLATILE_NOT_SUPPORTED,
        ERR_LIFESPAN_PERSISTENT_NOT_SUPPORTED,
        ERR_NO_BOOT_COMPLETE,
        ERR_UPDATE_IN_PROGRESS,
        ERR_IMAGE_COPY_IN_PROGRESS,
        ERR_IMAGE_COPY_COMPLETED,
        ERR_FLASH_WEAR_MITIGATION,
        ERR_INCOMPLETE_COMPONENT_SET,
        0xFFFF, // unknown - default case
    };

    for (auto rc : reasonCodes)
    {
        CLI::App app;
        setupFirmwareCommands(app);
        parseSubcmdArgs(app, "ImageCopyControl", {"-r", "0"});

        std::vector<uint8_t> buf;
        buildErrResponse(buf, NSM_ERR_INVALID_DATA, rc,
                         NSM_IMAGE_COPY_QUERY_PROGRESS);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
    }
}

// ---- DotLock createRequestMsg error paths (lines 1746-1848) -----------------

TEST_F(NsmFirmwareCmdParseTest, DotLock_NoCakEcdsaKey_CreateRequest)
{
    // cakEcdsaKeyFile empty → lines 1746-1748
    CLI::App app;
    setupFirmwareCommands(app);
    // No --cak_ecdsa_key provided → file path is empty
    // (parseSubcmdArgs silently fails required options)
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_HybridCakNoLmsKey_CreateRequest2)
{
    // cakKeyAuthScheme=1 but cakLmsKeyFile empty → lines 1756-1758
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_HybridLakNoLmsKey_CreateRequest)
{
    // lakKeyAuthScheme=1 but lakLmsKeyFile empty → lines 1784-1788
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "1",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotLock_NoSignatureFile_CreateRequest)
{
    // signatureFile empty → lines 1841-1844
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0"});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// ---- DotCAKRotate createRequestMsg error paths (lines 2007-2048) ------------

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_NoNewCakKey_CreateRequest)
{
    // newCakEcdsaKeyFile empty → lines 2007-2009
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0",
                     "--lak_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[11]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_HybridNoLmsKey_CreateRequest)
{
    // newCakKeyAuthScheme=1 but newCakLmsKeyFile empty → lines 2017-2019
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "1", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});
    EXPECT_NO_THROW(commands[11]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_NoSignatureFile_CreateRequest)
{
    // signatureFile empty → lines 2046-2048
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0"});
    EXPECT_NO_THROW(commands[11]->createRequestMsg());
}

// ---- DotCAKRotate parseResponseMsg error/cc paths (lines 2088-2131) ---------

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_ParseResponseDecodeFailure)
{
    // rc != NSM_SW_SUCCESS path (lines 2088-2093): pass tiny buffer
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 3, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, DotCAKRotate_ParseResponseCcError)
{
    // cc != NSM_SUCCESS path (lines 2125-2131)
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile});

    std::vector<uint8_t> buf;
    buildErrResponse(buf, NSM_ERR_INVALID_DATA, 0x0001, NSM_FW_DOT_CAK_ROTATE);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- DotUnlock createRequestMsg error (lines 2282-2284) ---------------------

TEST_F(NsmFirmwareCmdParseTest, DotUnlock_NoSignatureFile_CreateRequest)
{
    // signatureFile empty → lines 2282-2284
    CLI::App app;
    setupFirmwareCommands(app);
    // No --signature provided → signatureFile is empty
    EXPECT_NO_THROW(commands[13]->createRequestMsg());
}

// ---- DotGetStatus: status default case (lines 2551-2552) --------------------

TEST_F(NsmFirmwareCmdParseTest, DotGetStatus_ParseResponseUnknownStatus)
{
    // Status value >= 4 → default case (lines 2551-2552)
    CLI::App app;
    setupFirmwareCommands(app);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, ERR_NULL, 4, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ---- DotDisable createRequestMsg error paths --------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotDisable_NoLakKey_CreateRequest)
{
    // lakEcdsaKeyFile empty → error path in createRequestMsg
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

TEST_F(NsmFirmwareCmdParseTest, DotDisable_HybridNoLmsKey_CreateRequest)
{
    // lakKeyAuthScheme=1 but lakLmsKeyFile empty
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "1", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// ---- DotOverride createRequestMsg error path --------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotOverride_NoSignatureFile_CreateRequest)
{
    // signatureFile empty → error path in createRequestMsg
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotOverride",
                    {"--vendor_signature_auth_scheme", "0"});
    EXPECT_NO_THROW(commands[17]->createRequestMsg());
}

// ---- DotRecovery createRequestMsg error path --------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotRecovery_NoDotBlob_CreateRequest)
{
    // dotBlobFile empty → error path in createRequestMsg
    CLI::App app;
    setupFirmwareCommands(app);
    EXPECT_NO_THROW(commands[18]->createRequestMsg());
}

// ---- DotLock parse error paths ----------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotLock_ParseResponseDecodeFailure)
{
    // rc != NSM_SW_SUCCESS path: buffer too small
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 3, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ---- DotDisable parse error paths -------------------------------------------

TEST_F(NsmFirmwareCmdParseTest, DotDisable_ParseResponseDecodeFailure)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 3, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ---- Error-path tests for commands missing truncated-buffer coverage --------

static std::vector<uint8_t> makeTruncatedFwResp()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERR_INVALID_DATA;
    return buf;
}

// [1] IrreversibleConfig – error paths for all three request types
TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Query_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "0"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Disable_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "1"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Enable_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "2"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// [3] UpdateCodeAuthKeyPerm – error path
TEST_F(NsmFirmwareCmdParseTest, UpdateCodeAuthKeyPerm_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// [4] QueryFirmwareSecurityVersion – error path
TEST_F(NsmFirmwareCmdParseTest, QueryFirmwareSecurityVersion_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "QueryFirmwareSecurityVersion",
                    {"-c", "1", "-i", "0", "-d", "0"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// [5] UpdateMinSecurityVersion – error path
TEST_F(NsmFirmwareCmdParseTest, UpdateMinSecurityVersion_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion", {"-r", "0", "-n", "0"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// [6] SetRoTProperty – error path
TEST_F(NsmFirmwareCmdParseTest, SetRoTProperty_ParseResponseError)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "SetRoTProperty", {"-p", "0"});
    auto buf = makeTruncatedFwResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// [5] UpdateMinSecurityVersion – bit16 (Warm Reset) branch coverage
// (existing test uses update_methods=0x000600FF which has bit17,bit18 set but
// bit16=0; this test sets bit16 to cover line 751)
TEST_F(NsmFirmwareCmdParseTest,
       UpdateMinSecurityVersion_ParseResponse_WarmReset)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion", {"-r", "0", "-n", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_update_min_sec_ver_resp resp{};
    // 0x000700FF: bits 0-7 + bit16 + bit17 + bit18 all set
    resp.update_methods = 0x000700FF;
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &resp,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// [1] IrreversibleConfig – default case (requestType > 2) covers lines 868-869
TEST_F(NsmFirmwareCmdParseTest, IrreversibleConfig_Default_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // requestType = 3 hits the default: branch in the switch statement
    parseSubcmdArgs(app, "IrreversibleConfig", {"-r", "3"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// [7] DotCAKInstall – payloadLength < sizeof(nsm_common_resp) = 6 (lines
// 1230-1233) Use a 5-byte buffer (= sizeof(nsm_msg_hdr)) so payloadLength=5 < 6
// triggers the early return before decoding.
TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_PayloadTooSmall_ParseResponse)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // Just need to register with any valid args so commands[7] is DotCAKInstall
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile});
    // 5 bytes = sizeof(nsm_msg_hdr); payload = 5 < sizeof(nsm_common_resp)=6
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// [3] UpdateCodeAuthKeyPerm – indicesToBitmap throws when bitmapSize > 8
// (lines 480-483). Pass --bitmap 9 so indicesToBitmap({}, 9) throws.
TEST_F(NsmFirmwareCmdParseTest,
       UpdateCodeAuthKeyPerm_BitmapTooLarge_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "1", "-c", "0", "-i", "0", "-d", "0", "-n", "0",
                     "--bitmap", "9"});
    // indicesToBitmap({}, 9) throws std::invalid_argument (size >
    // maxBitmapSize=8) catch block returns NSM_SW_ERROR_LENGTH → no exception
    // from createRequestMsg
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

// [7] DotCAKInstall – LAK ECDSA key file reads as empty (lines 1133-1135)
// CLI11 throws before storing the member when validateFileSize fails, so
// lakKeyEcdsaKeyFile stays "" → readFileAsBytes("") returns {} → lines
// 1133-1135
TEST_F(NsmFirmwareCmdParseTest, DotCAKInstall_LakKeyReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // kEmptyFile fails validateFileSize so lakKeyEcdsaKeyFile stays ""
    // readFileAsBytes("") → empty (filename.empty() check) → lines 1133-1135
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEmptyFile});
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// [7] DotCAKInstall – hybrid CAK LMS unreadable (lines 1120-1122)
// Use valid-sized file so validator passes, then delete it before createRequest
// so readFileAsBytes fails to open → cakLms.empty() → lines 1120-1122
TEST_F(NsmFirmwareCmdParseTest,
       DotCAKInstall_HybridCakLmsReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kCakLmsTmp = "/tmp/fw_test_cak_lms_48b_tmp.bin";
    createTempBinFile(kCakLmsTmp, 48); // valid 48-byte → validator passes
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--cak_lms_key", kCakLmsTmp,
                     "--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile});
    std::remove(kCakLmsTmp.c_str()); // delete so readFileAsBytes fails
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// [7] DotCAKInstall – hybrid LAK LMS unreadable (lines 1150-1152)
// Use valid-sized file so validator passes, then delete it before createRequest
// so readFileAsBytes fails to open → lakLms.empty() → lines 1150-1152
TEST_F(NsmFirmwareCmdParseTest,
       DotCAKInstall_HybridLakLmsReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kLakLmsTmp = "/tmp/fw_test_lak_lms_48b_tmp.bin";
    createTempBinFile(kLakLmsTmp, 48); // valid 48-byte → validator passes
    parseSubcmdArgs(app, "DotCAKInstall",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "1",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--lak_lms_key",
                     kLakLmsTmp});
    std::remove(kLakLmsTmp.c_str()); // delete so readFileAsBytes fails
    EXPECT_NO_THROW(commands[7]->createRequestMsg());
}

// ============================================================
// DotLock tests (commands[10])
// ============================================================

// [10] DotLock – hybrid CAK LMS file deleted before createRequest (lines
// 1770-1775)
TEST_F(NsmFirmwareCmdParseTest, DotLock_HybridCakLmsReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kCakLmsTmpDL = "/tmp/fw_test_dotlock_cak_lms.bin";
    createTempBinFile(kCakLmsTmpDL, 48);
    parseSubcmdArgs(
        app, "DotLock",
        {"--cak_key_auth_scheme", "1", "--cak_ecdsa_key", kEcdsaKeyFile,
         "--cak_lms_key", kCakLmsTmpDL, "--lak_key_auth_scheme", "0",
         "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
         "--lock_signature_auth_scheme", "0", "--signature", kSignatureFile});
    std::remove(kCakLmsTmpDL.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// [10] DotLock – LAK ECDSA key file deleted before createRequest (lines
// 1783-1787)
TEST_F(NsmFirmwareCmdParseTest, DotLock_LakEcdsaReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kLakEcdsaTmpDL =
        "/tmp/fw_test_dotlock_lak_ecdsa.bin";
    createTempBinFile(kLakEcdsaTmpDL, 96);
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kLakEcdsaTmpDL, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    std::remove(kLakEcdsaTmpDL.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// [10] DotLock – hybrid LAK LMS file deleted before createRequest (lines
// 1800-1804)
TEST_F(NsmFirmwareCmdParseTest, DotLock_HybridLakLmsReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kLakLmsTmpDL = "/tmp/fw_test_dotlock_lak_lms.bin";
    createTempBinFile(kLakLmsTmpDL, 48);
    parseSubcmdArgs(
        app, "DotLock",
        {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key", kEcdsaKeyFile,
         "--lak_key_auth_scheme", "1", "--lak_ecdsa_key", kEcdsaKeyFile,
         "--lak_lms_key", kLakLmsTmpDL, "--unlock_method", "0",
         "--lock_signature_auth_scheme", "0", "--signature", kSignatureFile});
    std::remove(kLakLmsTmpDL.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// [10] DotLock – static challenge file missing (unlock_method=2) (lines
// 1831-1835)
TEST_F(NsmFirmwareCmdParseTest, DotLock_StaticChallengeMissing_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // unlock_method=2 but no --static_challenge → staticChallengeFile=""
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "2",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// [10] DotLock – static challenge file deleted before createRequest (lines
// 1838-1843)
TEST_F(NsmFirmwareCmdParseTest, DotLock_StaticChallengeReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kChallengeTmpDL =
        "/tmp/fw_test_dotlock_challenge.bin";
    createTempBinFile(kChallengeTmpDL, 32);
    parseSubcmdArgs(
        app, "DotLock",
        {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key", kEcdsaKeyFile,
         "--lak_key_auth_scheme", "0", "--lak_ecdsa_key", kEcdsaKeyFile,
         "--unlock_method", "2", "--static_challenge", kChallengeTmpDL,
         "--lock_signature_auth_scheme", "0", "--signature", kSignatureFile});
    std::remove(kChallengeTmpDL.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[10]->createRequestMsg());
}

// [10] DotLock – parseResponseMsg success path + output file write fails (line
// 1927)
TEST_F(NsmFirmwareCmdParseTest, DotLock_ParseResponseSuccessOutputWriteFail)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // Set output to unwritable path so std::ofstream fails to open
    parseSubcmdArgs(app, "DotLock",
                    {"--cak_key_auth_scheme", "0", "--cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_key_auth_scheme", "0",
                     "--lak_ecdsa_key", kEcdsaKeyFile, "--unlock_method", "0",
                     "--lock_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output",
                     "/tmp/nonexistent_dir_zzz/fw_dotlock_out.bin"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> blob(DOT_BLOB_SIZE, 0);
    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, blob.data(), msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ============================================================
// DotCAKRotate tests (commands[11])
// ============================================================

// [11] DotCAKRotate – hybrid new CAK LMS file deleted before createRequest
// (line 2034)
TEST_F(NsmFirmwareCmdParseTest,
       DotCAKRotate_HybridNewCakLmsReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kNewCakLmsTmp =
        "/tmp/fw_test_dotcakrotate_lms.bin";
    createTempBinFile(kNewCakLmsTmp, 48);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "1", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--new_cak_lms_key", kNewCakLmsTmp,
                     "--lak_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    std::remove(kNewCakLmsTmp.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[11]->createRequestMsg());
}

// [11] DotCAKRotate – parseResponseMsg success + output file write fails (line
// 2124)
TEST_F(NsmFirmwareCmdParseTest,
       DotCAKRotate_ParseResponseSuccessOutputWriteFail)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotCAKRotate",
                    {"--new_cak_key_auth_scheme", "0", "--new_cak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_signature_auth_scheme", "0",
                     "--signature", kSignatureFile, "--output",
                     "/tmp/nonexistent_dir_zzz/fw_dotcakrotate_out.bin"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> blob(DOT_BLOB_SIZE, 0);
    encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, blob.data(), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ============================================================
// DotUnlockChallenge tests (commands[12])
// ============================================================

// [12] DotUnlockChallenge – parseResponseMsg success + output write fails (line
// 2250)
TEST_F(NsmFirmwareCmdParseTest,
       DotUnlockChallenge_ParseResponseSuccessOutputWriteFail)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotUnlockChallenge",
                    {"--unlock_type", "1", "--output",
                     "/tmp/nonexistent_dir_zzz/fw_challenge_out.bin"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> challenge(DOT_CHALLENGE_SIZE, 0);
    encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, ERR_NULL,
                                         challenge.data(), msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ============================================================
// DotGetInfo tests (commands[14])
// ============================================================

// [14] DotGetInfo – parseResponseMsg success + output write fails (line 2457)
TEST_F(NsmFirmwareCmdParseTest, DotGetInfo_ParseResponseSuccessOutputWriteFail)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // output option has no validator so any path works
    parseSubcmdArgs(
        app, "DotGetInfo",
        {"--output", "/tmp/nonexistent_dir_zzz/fw_dotgetinfo_out.bin"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> infoBlob(DOT_BLOB_SIZE, 0);
    encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0, 0,
                                 infoBlob.data(), msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ============================================================
// DotDisable tests (commands[16])
// ============================================================

// [16] DotDisable – hybrid LAK LMS missing (similar to lines 2660-2664)
TEST_F(NsmFirmwareCmdParseTest, DotDisable_HybridLakLmsMissing_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // lak_key_auth_scheme=1 but no --lak_lms_key → lakLmsKeyFile=""
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "1", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// [16] DotDisable – hybrid LAK LMS file deleted before createRequest (lines
// 2666-2671)
TEST_F(NsmFirmwareCmdParseTest, DotDisable_HybridLakLmsReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kLakLmsTmpDD =
        "/tmp/fw_test_dotdisable_lak_lms.bin";
    createTempBinFile(kLakLmsTmpDD, 48);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "1", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--lak_lms_key", kLakLmsTmpDD,
                     "--unlock_method", "0", "--disable_signature_auth_scheme",
                     "0", "--signature", kSignatureFile});
    std::remove(kLakLmsTmpDD.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// [16] DotDisable – static challenge missing when unlock_method=2 (lines
// 2690-2694)
TEST_F(NsmFirmwareCmdParseTest, DotDisable_StaticChallengeMissing_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    // unlock_method=2 but no --static_challenge → staticChallengeFile=""
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "2",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// [16] DotDisable – static challenge file deleted before createRequest (lines
// 2700-2702)
TEST_F(NsmFirmwareCmdParseTest,
       DotDisable_StaticChallengeReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kChallengeTmpDD =
        "/tmp/fw_test_dotdisable_challenge.bin";
    createTempBinFile(kChallengeTmpDD, 32);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "2",
                     "--static_challenge", kChallengeTmpDD,
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile});
    std::remove(kChallengeTmpDD.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// [16] DotDisable – signature file deleted before createRequest (lines
// 2709-2711)
TEST_F(NsmFirmwareCmdParseTest, DotDisable_SignatureReadFail_CreateRequest)
{
    CLI::App app;
    setupFirmwareCommands(app);
    static const std::string kSigTmpDD = "/tmp/fw_test_dotdisable_sig.bin";
    createTempBinFile(kSigTmpDD, 1840);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSigTmpDD});
    std::remove(kSigTmpDD.c_str()); // delete → readFileAsBytes fails
    EXPECT_NO_THROW(commands[16]->createRequestMsg());
}

// [16] DotDisable – parseResponseMsg success + output file write fails (line
// 2788)
TEST_F(NsmFirmwareCmdParseTest, DotDisable_ParseResponseSuccessOutputWriteFail)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "DotDisable",
                    {"--lak_key_auth_scheme", "0", "--lak_ecdsa_key",
                     kEcdsaKeyFile, "--unlock_method", "0",
                     "--disable_signature_auth_scheme", "0", "--signature",
                     kSignatureFile, "--output",
                     "/tmp/nonexistent_dir_zzz/fw_dotdisable_out.bin"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> blob(DOT_BLOB_SIZE, 0);
    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, ERR_NULL, blob.data(), msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ---- UpdateCodeAuthKeyPerm: zero update methods (lines 516-548) -------------
// The existing ParseResponse test uses allBits=0x000700FF so all bit-branches
// take the TRUE path. This test uses updateMethod=0 to cover the FALSE branch
// of each `if (updateMethodBits.bits.bitX)` check.

TEST_F(NsmFirmwareCmdParseTest, UpdateCodeAuthKeyPerm_ParseResponse_NoBits)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateCodeAuthKeyPerm",
                    {"-r", "0", "-c", "1", "-i", "0", "-d", "0", "-n", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t noBits = 0;
    encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, ERR_NULL, noBits,
                                              msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ---- UpdateCodeAuthKeyPerm: invalid key index string (line 469) -------------
// requestType=1 with a non-numeric key string triggers the stoul() exception,
// covering the catch block at L469 of nsm_firmware_cmd.cpp.

TEST_F(NsmFirmwareCmdParseTest,
       UpdateCodeAuthKeyPerm_CreateRequest_InvalidKeyString)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(
        app, "UpdateCodeAuthKeyPerm",
        {"-r", "1", "-c", "1", "-i", "0", "-d", "0", "-n", "0", "-k", "abc"});
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

// ---- UpdateMinSecurityVersion: update_methods=0 (all FALSE branches) --------
// Covers the FALSE branches (bit not set) of every if(updateMethodBits.bits.X)
// check at L725-L757 in nsm_firmware_cmd.cpp.  The existing success test uses
// 0x000700FF which sets all relevant bits, so only the TRUE paths were taken.
// This test sets update_methods=0 so every bit check is FALSE, exercising the
// skip-push_back path for all 9 branches.
TEST_F(NsmFirmwareCmdParseTest,
       UpdateMinSecurityVersion_ParseResponse_AllBitsZero)
{
    CLI::App app;
    setupFirmwareCommands(app);
    parseSubcmdArgs(app, "UpdateMinSecurityVersion", {"-r", "0", "-n", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_update_min_sec_ver_resp_command),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_firmware_update_min_sec_ver_resp resp{};
    resp.update_methods =
        0; // all bits clear → every if-branch takes FALSE path
    encode_nsm_firmware_update_sec_ver_resp(0, NSM_SUCCESS, ERR_NULL, &resp,
                                            msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::firmware
