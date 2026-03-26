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
#include "device-configuration.h"

#include "../nsm_config_cmd.cpp"

#include <gtest/gtest.h>

// Config commands (after registerCommand) in order:
//   [0]  SetErrorInjectionModeV1
//   [1]  GetErrorInjectionModeV1
//   [2]  GetSupportedErrorInjectionTypesV1
//   [3]  SetErrorInjectionPayload
//   [4]  GetErrorInjectionPayload
//   [5]  ActivateErrorInjectionPayload
//   [6]  SetCurrentErrorInjectionTypesV1
//   [7]  GetCurrentErrorInjectionTypesV1
//   [8]  GetFpgaDiagnosticsSettings
//   [9]  EnableDisableGpuIstMode
//   [10] GetReconfigurationPermissionsV1
//   [11] SetReconfigurationPermissionsV1
//   [12] GetConfidentialComputeModeV1
//   [13] SetConfidentialComputeModeV1
//   [14] GetDevicemodeSettings
//   [15] SetDevicemodeSettings

namespace nsmtool::config
{

// ---- Helpers ----------------------------------------------------------------

static void setupConfigCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

// Parse args directly on the named sub-subcommand (leaf), which triggers the
// exec() callback. exec() tries nsmSendRecv which always fails in tests, but
// all option values are set before that. Any parse/exec exception is silently
// swallowed so the test can proceed to call parseResponseMsg directly.
static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* cfgSub = app.get_subcommand("config");
    if (!cfgSub)
        return;
    auto* leafSub = cfgSub->get_subcommand(cmdName);
    if (!leafSub)
        return;

    // Base options provided by CommandInterface constructor: -m (mctp_eid)
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

// ---- SetErrorInjectionModeV1 ------------------------------------------------

TEST(NsmConfigCmdParse, SetErrorInjectionModeV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionModeV1", {"--mode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- GetErrorInjectionModeV1 ------------------------------------------------

TEST(NsmConfigCmdParse, GetErrorInjectionModeV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_mode_v1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_error_injection_mode_v1 data{};
    data.mode = 1;
    data.flags.byte = 1; // persistent=1
    encode_get_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                            msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ---- GetSupportedErrorInjectionTypesV1 --------------------------------------

TEST(NsmConfigCmdParse, GetSupportedErrorInjectionTypesV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_types_mask_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_error_injection_types_mask data{};
    data.mask[0] = 0x3F; // all 6 types enabled
    encode_get_supported_error_injection_types_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ---- SetErrorInjectionPayload -----------------------------------------------

TEST(NsmConfigCmdParse, SetErrorInjectionPayload_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    // -d is required with ->expected(-3); provide at least 3 values
    parseSubcmdArgs(app, "SetErrorInjectionPayload", {"-d", "1", "2", "3"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ---- GetErrorInjectionPayload -----------------------------------------------

TEST(NsmConfigCmdParse, GetErrorInjectionPayload_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    // Type 0 (EI_MEMORY_ERRORS), subtype 0 — no file I/O needed
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "0", "-s", "0"});

    uint8_t rawData[] = {0xAA, 0xBB};
    // Must use nsm_get_error_injection_payload_resp (not nsm_common_resp) —
    // the struct has offset(4) + error_injection_type(2) + fault_payload[1]
    // fields beyond the common header.
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_payload_resp) - 1 +
                             sizeof(rawData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL,
                                            0 /*EI_MEMORY_ERRORS*/, 0, rawData,
                                            sizeof(rawData), msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetErrorInjectionPayload_DeviceErrorParseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    // Type 4 = EI_DEVICE_ERRORS: parseResponseMsg strips leading 2 bytes
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "4", "-s", "0"});

    uint8_t rawData[] = {0x00, 0x00, 0x55}; // 2-byte subtype + 1 payload byte
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_payload_resp) - 1 +
                             sizeof(rawData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL,
                                            4 /*EI_DEVICE_ERRORS*/, 0, rawData,
                                            sizeof(rawData), msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- ActivateErrorInjectionPayload ------------------------------------------

TEST(NsmConfigCmdParse, ActivateErrorInjectionPayload_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_activate_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- SetCurrentErrorInjectionTypesV1 ----------------------------------------

TEST(NsmConfigCmdParse, SetCurrentErrorInjectionTypesV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetCurrentErrorInjectionTypesV1",
                    {"-d", "1", "2", "3"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_current_error_injection_types_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ---- GetCurrentErrorInjectionTypesV1 ----------------------------------------

TEST(NsmConfigCmdParse, GetCurrentErrorInjectionTypesV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_types_mask_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_error_injection_types_mask data{};
    data.mask[0] = 0x01;
    encode_get_current_error_injection_types_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// ---- GetFpgaDiagnosticsSettings – GET_WP_SETTINGS ---------------------------

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpSettings)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "0"}); // GET_WP_SETTINGS=0

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_fpga_diagnostics_settings_wp_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp wpData{};
    encode_get_fpga_diagnostics_settings_wp_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 &wpData, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// Covers the compound (data.gpu1_4 && data.gpu5_8 && data.gpu9_12 &&
// data.gpu13_16) TRUE-path branches at L801-802 of nsm_config_cmd.cpp.
// The default all-zeros test above only takes the gpu1_4=FALSE short-circuit.
// This test sets all four GPU SPI-Flash bits to 1 so evaluation continues
// through all sub-conditions and the full AND evaluates to TRUE.
TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpSettings_AllGpuBitsSet)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "0"}); // GET_WP_SETTINGS=0

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_fpga_diagnostics_settings_wp_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp wpData{};
    // Set all four GPU SPI Flash group bits so all && sub-conditions are TRUE
    wpData.gpu1_4 = 1;
    wpData.gpu5_8 = 1;
    wpData.gpu9_12 = 1;
    wpData.gpu13_16 = 1;
    encode_get_fpga_diagnostics_settings_wp_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 &wpData, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// Covers gpu1_4=TRUE but gpu5_8=FALSE: L801 FALSE branch of gpu5_8 check.
TEST(NsmConfigCmdParse,
     GetFpgaDiagnosticsSettings_WpSettings_Gpu14TrueGpu58False)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_fpga_diagnostics_settings_wp_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp wpData{};
    wpData.gpu1_4 = 1;
    wpData.gpu5_8 = 0; // FALSE → short-circuit
    encode_get_fpga_diagnostics_settings_wp_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 &wpData, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// Covers gpu1_4=TRUE, gpu5_8=TRUE, gpu9_12=FALSE: L802 FALSE branch.
TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpSettings_Gpu912False)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_fpga_diagnostics_settings_wp_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp wpData{};
    wpData.gpu1_4 = 1;
    wpData.gpu5_8 = 1;
    wpData.gpu9_12 = 0; // FALSE → short-circuit
    encode_get_fpga_diagnostics_settings_wp_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 &wpData, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// Covers gpu1_4=TRUE, gpu5_8=TRUE, gpu9_12=TRUE, gpu13_16=FALSE: last branch.
TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpSettings_Gpu1316False)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_fpga_diagnostics_settings_wp_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp wpData{};
    wpData.gpu1_4 = 1;
    wpData.gpu5_8 = 1;
    wpData.gpu9_12 = 1;
    wpData.gpu13_16 = 0; // FALSE → short-circuit
    encode_get_fpga_diagnostics_settings_wp_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 &wpData, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpJumper)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "2"}); // GET_WP_JUMPER_PRESENCE=2

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_fpga_diagnostics_settings_wp_jumper jumpData{};
    encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &jumpData, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_PowerSupplyStatus)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "5"}); // GET_POWER_SUPPLY_STATUS=5

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_supply_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_supply_status_resp(0, NSM_SUCCESS, ERR_NULL, 1, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_GpuPresence)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "12"}); // GET_GPU_PRESENCE=12

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_gpu_presence_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpu_presence_resp(0, NSM_SUCCESS, ERR_NULL, 0xFF, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_GpuPowerStatus)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "13"}); // GET_GPU_POWER_STATUS=13

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_gpu_power_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpu_power_status_resp(0, NSM_SUCCESS, ERR_NULL, 0xFF, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_GpuIstMode)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "4"}); // GET_GPU_IST_MODE_SETTINGS=4

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_gpu_ist_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpu_ist_mode_resp(0, NSM_SUCCESS, ERR_NULL, 1, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_InvalidDataId)
{
    CLI::App app;
    setupConfigCommands(app);
    // dataId 99 is not a valid fpga_diagnostics_settings_data_index
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "99"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // parseResponseMsg hits the default case and prints an error; no throw
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ---- EnableDisableGpuIstMode ------------------------------------------------

TEST(NsmConfigCmdParse, EnableDisableGpuIstMode_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "EnableDisableGpuIstMode",
                    {"--deviceIndex", "0", "--value", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_enable_disable_gpu_ist_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- GetReconfigurationPermissionsV1 ----------------------------------------

TEST(NsmConfigCmdParse,
     GetReconfigurationPermissionsV1_ValidSetting_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    // RP_IN_SYSTEM_TEST = 0 (first entry in settingsDictionary)
    parseSubcmdArgs(app, "GetReconfigurationPermissionsV1",
                    {"--settingId", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_reconfiguration_permissions_v1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_reconfiguration_permissions_v1 data{};
    encode_get_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   &data, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse,
     GetReconfigurationPermissionsV1_InvalidSetting_EarlyReturn)
{
    CLI::App app;
    setupConfigCommands(app);
    // Default settingIndex=(reconfiguration_permissions_v1_index)-1 is invalid
    // parseResponseMsg returns early without decoding — exercise that branch.
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ---- SetReconfigurationPermissionsV1 ----------------------------------------

TEST(NsmConfigCmdParse,
     SetReconfigurationPermissionsV1_ValidSetting_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    // settingId=0, configuration=0 (RP_ONESHOOT_HOT_RESET), value=3
    parseSubcmdArgs(
        app, "SetReconfigurationPermissionsV1",
        {"--settingId", "0", "--configuration", "0", "--value", "3"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse,
     SetReconfigurationPermissionsV1_InvalidSetting_EarlyReturn)
{
    CLI::App app;
    setupConfigCommands(app);
    // Default settingIndex invalid → early return branch
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- GetConfidentialComputeModeV1 -------------------------------------------

TEST(NsmConfigCmdParse, GetConfidentialComputeModeV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_confidential_compute_mode_v1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, 1 /*current*/, 0 /*pending*/, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ---- SetConfidentialComputeModeV1 -------------------------------------------

TEST(NsmConfigCmdParse, SetConfidentialComputeModeV1_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetConfidentialComputeModeV1", {"--mode", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_confidential_compute_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

// ---- GetDevicemodeSettings --------------------------------------------------

TEST(NsmConfigCmdParse, GetDevicemodeSettings_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDevicemodeSettings", {"--mode_index", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_setting_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                         static_cast<uint8_t>(0), msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ---- SetDevicemodeSettings --------------------------------------------------

TEST(NsmConfigCmdParse, SetDevicemodeSettings_ParseResponseSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetDevicemodeSettings",
                    {"--index", "0", "--mode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ---- Error path (rc != NSM_SW_SUCCESS) tests --------------------------------

TEST(NsmConfigCmdParse, SetErrorInjectionModeV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionModeV1", {"--mode", "0"});

    // Tiny buffer causes decode error → exercises the error-return branch
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetErrorInjectionModeV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetSupportedErrorInjectionTypesV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetErrorInjectionPayload_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "0", "-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpSettings_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_PowerSupplyStatus_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "5"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_GpuPresence_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "12"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_GpuPowerStatus_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "13"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_GpuIstMode_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "4"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse,
     GetReconfigurationPermissionsV1_ValidSetting_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetReconfigurationPermissionsV1",
                    {"--settingId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, SetErrorInjectionPayload_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionPayload", {"-d", "1", "2", "3"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, ActivateErrorInjectionPayload_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, SetCurrentErrorInjectionTypesV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetCurrentErrorInjectionTypesV1",
                    {"-d", "1", "2", "3"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetCurrentErrorInjectionTypesV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetFpgaDiagnosticsSettings_WpJumper_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings",
                    {"--dataId", "2"}); // GET_WP_JUMPER_PRESENCE=2

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, EnableDisableGpuIstMode_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "EnableDisableGpuIstMode",
                    {"--deviceIndex", "0", "--value", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, EnableDisableGpuIstMode_InvalidDeviceIndex)
{
    CLI::App app;
    setupConfigCommands(app);
    // deviceIndex=9 is invalid (must be < 8 or == ALL_GPUS_DEVICE_INDEX=10)
    parseSubcmdArgs(app, "EnableDisableGpuIstMode",
                    {"--deviceIndex", "9", "--value", "1"});

    auto [rc, msg] = commands[9]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS); // createRequestMsg returns NSM_SW_ERROR
}

TEST(NsmConfigCmdParse, SetReconfigurationPermissionsV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(
        app, "SetReconfigurationPermissionsV1",
        {"--settingId", "0", "--configuration", "0", "--value", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetConfidentialComputeModeV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, SetConfidentialComputeModeV1_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetConfidentialComputeModeV1", {"--mode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, GetDevicemodeSettings_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDevicemodeSettings", {"--mode_index", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmConfigCmdParse, SetDevicemodeSettings_ParseResponseError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetDevicemodeSettings",
                    {"--index", "0", "--mode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ---- SetErrorInjectionPayload: EI_DEVICE_ERRORS inserts subtype bytes -------
// Covers lines 278-280 in nsm_config_cmd.cpp: when errorInjectionType equals
// EI_DEVICE_ERRORS, createRequestMsg prepends 2 bytes of subtype to rawData.

TEST(NsmConfigCmdParse, SetErrorInjectionPayload_DeviceErrors)
{
    CLI::App app;
    setupConfigCommands(app);
    // type=4 (EI_DEVICE_ERRORS), subtype=0 (FATAL), one data byte
    parseSubcmdArgs(app, "SetErrorInjectionPayload",
                    {"-t", "4", "-s", "0", "-d", "0"});
    EXPECT_NO_THROW(commands[3]->createRequestMsg());
}

// ---- GetErrorInjectionPayload: EI_DEVICE_ERRORS success path ----------------
// Covers lines 386-404 (success path) with EI_DEVICE_ERRORS/SUBTYPE_FATAL.
// A properly-sized 6-byte fatal payload lets decode succeed so cc==NSM_SUCCESS,
// exercising the if-branch at line 395 (EI_DEVICE_ERRORS && data.size()>=2).

TEST(NsmConfigCmdParse, GetErrorInjectionPayload_FatalPayloadSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "4", "-s", "0"});

    // Build a valid EI_DEVICE_ERRORS/SUBTYPE_FATAL (0) payload:
    // 2 bytes subtype=0 + 4 bytes bitmap=0 (= sizeof
    // nsm_error_injection_fatal_payload)
    nsm_error_injection_fatal_payload fatalPayload{};
    fatalPayload.error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_FATAL;
    fatalPayload.payload_bitmap = 0;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_payload_resp) - 1 +
                             sizeof(fatalPayload));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_error_injection_payload_resp(
        0, NSM_SUCCESS, ERR_NULL, EI_DEVICE_ERRORS,
        EI_DEVICE_ERRORS_SUBTYPE_FATAL,
        reinterpret_cast<const uint8_t*>(&fatalPayload), sizeof(fatalPayload),
        msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- GetErrorInjectionPayload: EI_GPIO_SPOOFING success path (else branch) --
// Covers the else-branch at lines 400-402 (Fault payload without stripping).
// type=5 (EI_GPIO_SPOOFING) with count_of_gpio=0 (2-byte payload).

TEST(NsmConfigCmdParse, GetErrorInjectionPayload_GpioSpoofingSuccess)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "5", "-s", "0"});

    // GPIO spoofing with count_of_gpio=0: payload = {0x00, 0x00}
    uint8_t rawData[] = {0x00, 0x00}; // count_of_gpio=0
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_payload_resp) - 1 +
                             sizeof(rawData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL,
                                            EI_GPIO_SPOOFING, 0, rawData,
                                            sizeof(rawData), msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- SetCurrentErrorInjectionTypesV1: createRequestMsg with 8+ bytes --------
// Covers the FALSE branch of `i < 8` in the for-loop at L519 of
// nsm_config_cmd.cpp.  The existing parse test uses 3 bytes (rawData.size()<8)
// so the loop exits via `i < rawData.size()` being FALSE, never reaching
// `i < 8` == FALSE.  With 9 bytes the loop runs i=0..7 and exits when i=8,
// exercising the `i < 8` FALSE branch (and its short-circuit companion).
TEST(NsmConfigCmdParse,
     SetCurrentErrorInjectionTypesV1_CreateRequestMsg_NineBytes)
{
    CLI::App app;
    setupConfigCommands(app);
    // 9 bytes so the for-loop iterates all 8 slots and exits via i<8 being
    // FALSE (not via rawData.size() being reached).
    parseSubcmdArgs(app, "SetCurrentErrorInjectionTypesV1",
                    {"-d", "0", "1", "2", "3", "4", "5", "6", "7", "8"});
    EXPECT_NO_THROW(commands[6]->createRequestMsg());
}

} // namespace nsmtool::config
