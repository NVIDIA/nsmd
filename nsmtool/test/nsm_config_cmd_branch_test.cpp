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
//   [16] GetDeviceModeSettingsV2
//   [17] SetDeviceModeSettingsV2

namespace nsmtool::config
{

// ---- Helpers ----------------------------------------------------------------

static void setupConfigCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* cfgSub = app.get_subcommand("config");
    if (!cfgSub)
        return;
    auto* leafSub = cfgSub->get_subcommand(cmdName);
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

// ===========================================================================
// GetFpgaDiagnosticsSettings — additional switch cases not in parse_test
// ===========================================================================

// GET_PCIE_FUNDAMENTAL_RESET_STATE (dataId=1) — hits "default" since no
// specific decode function exists for this in the switch, covers the default
// branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_PcieFundamentalReset)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_GPU_DEGRADE_MODE_SETTINGS (dataId=3) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_GpuDegradeMode)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "3"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_BOARD_POWER_SUPPLY_STATUS (dataId=6) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_BoardPowerSupply)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "6"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_POWER_BRAKE_STATE (dataId=7) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_PowerBrakeState)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "7"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_THERMAL_ALERT_STATE (dataId=8) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_ThermalAlert)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "8"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_NVSW_FLASH_PRESENT_SETTINGS (dataId=9) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_NvSwFlashPresent)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "9"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_NVSW_FUSE_SRC_SETTINGS (dataId=10) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_NvSwFuseSrc)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "10"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// GET_RETIMER_LTSSM_DUMP_MODE_SETTINGS (dataId=11) — default branch
TEST(ConfigBranch, GetFpgaDiagnosticsSettings_RetimerLtssm)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetFpgaDiagnosticsSettings", {"--dataId", "11"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// GetReconfigurationPermissionsV1 — all valid settingIndex values
// ===========================================================================

TEST(ConfigBranch, GetReconfigurationPermissionsV1_AllSettings)
{
    // Test each valid settingIndex from the settingsDictionary
    int validSettings[] = {
        RP_IN_SYSTEM_TEST,
        RP_FUSING_MODE,
        RP_CONFIDENTIAL_COMPUTE,
        RP_BAR0_FIREWALL,
        RP_CONFIDENTIAL_COMPUTE_DEV_MODE,
        RP_TOTAL_GPU_POWER_CURRENT_LIMIT,
        RP_TOTAL_GPU_POWER_RATED_LIMIT,
        RP_TOTAL_GPU_POWER_MAX_LIMIT,
        RP_TOTAL_GPU_POWER_MIN_LIMIT,
        RP_CLOCK_LIMIT,
        RP_NVLINK_DISABLE,
        RP_ECC_ENABLE,
        RP_PCIE_VF_CONFIGURATION,
        RP_ROW_REMAPPING_ALLOWED,
        RP_ROW_REMAPPING_FEATURE,
        RP_HBM_FREQUENCY_CHANGE,
        RP_HULK_LICENSE_UPDATE,
        RP_FORCE_TEST_COUPLING,
        RP_BAR0_TYPE_CONFIG,
        RP_EDPP_SCALING_FACTOR,
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1,
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2,
        RP_EGM_MODE,
        RP_INFOROM_RECREATE_ALLOW_INB,
    };

    for (auto setting : validSettings)
    {
        CLI::App app;
        setupConfigCommands(app);
        parseSubcmdArgs(app, "GetReconfigurationPermissionsV1",
                        {"--settingId", std::to_string(setting)});

        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_reconfiguration_permissions_v1 data{};
        data.host_oneshot = 1;
        data.host_persistent = 1;
        data.host_flr_persistent = 1;
        data.DOE_oneshot = 1;
        data.DOE_persistent = 1;
        data.DOE_flr_persistent = 1;
        encode_get_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
        EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
    }
}

// ===========================================================================
// Config commands continued (post-registerCommand, per the registration
// order in nsm_config_cmd.cpp::registerCommand):
//   [18] SetDeviceConfig
//   [19] GetDeviceConfig
//   [20] GetLLDPMode
//   [21] SetLLDPMode
//   [22] GetPowerCappingMode
//   [23] SetPowerCappingMode
//   [24] GetLTXMode
//   [25] SetLTXMode
//   [26] GetUPhyMode
//   [27] SetUPhyMode

// ===========================================================================
// GetDeviceModeSettingsV2 — currentModeLength/pendingModeLength branches
// ===========================================================================

TEST(ConfigBranch, GetDeviceModeSettingsV2_Success_Uint32)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDeviceModeSettingsV2",
                    {"--deviceModeIndex", "11"});

    // Build response with currentModeLength == sizeof(uint32_t)
    // and pendingModeLength == sizeof(uint32_t)
    uint32_t currentVal = htole32(42);
    uint32_t pendingVal = htole32(99);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp) +
                             sizeof(uint32_t) + sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, reinterpret_cast<const uint8_t*>(&currentVal),
        sizeof(uint32_t), reinterpret_cast<const uint8_t*>(&pendingVal),
        sizeof(uint32_t), msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, GetDeviceModeSettingsV2_Success_NonUint32)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDeviceModeSettingsV2",
                    {"--deviceModeIndex", "11"});

    // Use a 2-byte current mode and 3-byte pending mode (not sizeof(uint32_t))
    uint8_t currentData[] = {0x01, 0x02};
    uint8_t pendingData[] = {0x03, 0x04, 0x05};

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp) +
                             sizeof(currentData) + sizeof(pendingData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, currentData, sizeof(currentData), pendingData,
        sizeof(pendingData), msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, GetDeviceModeSettingsV2_Success_ZeroLength)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDeviceModeSettingsV2",
                    {"--deviceModeIndex", "11"});

    // Zero-length current and pending mode data
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, nullptr,
                                            0, nullptr, 0, msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, GetDeviceModeSettingsV2_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDeviceModeSettingsV2",
                    {"--deviceModeIndex", "11"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// SetDeviceModeSettingsV2
// ===========================================================================

TEST(ConfigBranch, SetDeviceModeSettingsV2_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetDeviceModeSettingsV2",
                    {"--deviceModeIndex", "11", "--value", "42"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, SetDeviceModeSettingsV2_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetDeviceModeSettingsV2",
                    {"--deviceModeIndex", "11", "--value", "42"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// Error injection mask bit branches — all 6 bits individually
// ===========================================================================

TEST(ConfigBranch, GetSupportedErrorInjectionTypes_IndividualBits)
{
    for (int bit = 0; bit < 6; ++bit)
    {
        CLI::App app;
        setupConfigCommands(app);

        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_error_injection_types_mask_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_error_injection_types_mask data{};
        data.mask[0] = (1u << bit);
        encode_get_supported_error_injection_types_v1_resp(
            0, NSM_SUCCESS, ERR_NULL, &data, msg);
        EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
    }
}

TEST(ConfigBranch, GetCurrentErrorInjectionTypes_IndividualBits)
{
    for (int bit = 0; bit < 6; ++bit)
    {
        CLI::App app;
        setupConfigCommands(app);

        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_error_injection_types_mask_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        nsm_error_injection_types_mask data{};
        data.mask[0] = (1u << bit);
        encode_get_current_error_injection_types_v1_resp(0, NSM_SUCCESS,
                                                         ERR_NULL, &data, msg);
        EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
    }
}

// ===========================================================================
// GetErrorInjectionModeV1 — mode/flags variations
// ===========================================================================

TEST(ConfigBranch, GetErrorInjectionModeV1_ModeEnabled)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_mode_v1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_error_injection_mode_v1 data{};
    data.mode = 1;
    data.flags.byte = 0; // persistent=0
    encode_get_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                            msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, GetErrorInjectionModeV1_ModeDisabled)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_mode_v1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_error_injection_mode_v1 data{};
    data.mode = 0;
    data.flags.byte = 1; // persistent=1
    encode_get_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                            msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// GetErrorInjectionPayload — success with no data (data.size() < 2 for device)
// ===========================================================================

TEST(ConfigBranch, GetErrorInjectionPayload_DeviceErrorsSmallPayload)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "4", "-s", "0"});

    // Payload with only 1 byte (< 2 for device errors) goes to else branch
    uint8_t rawData[] = {0x42};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_error_injection_payload_resp) - 1 +
                             sizeof(rawData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL,
                                            EI_DEVICE_ERRORS, 0, rawData,
                                            sizeof(rawData), msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// ActivateErrorInjectionPayload — with error injection type/subtype values
// ===========================================================================

TEST(ConfigBranch, ActivateErrorInjectionPayload_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "ActivateErrorInjectionPayload",
                    {"-t", "0", "-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_activate_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// SetReconfigurationPermissionsV1 — all configuration options
// ===========================================================================

TEST(ConfigBranch, SetReconfigurationPermissionsV1_AllConfigs)
{
    // Test each configuration: 0=RP_ONESHOOT_HOT_RESET, 1=RP_PERSISTENT,
    // 2=RP_ONESHOT_FLR
    for (int config = 0; config <= 2; ++config)
    {
        CLI::App app;
        setupConfigCommands(app);
        parseSubcmdArgs(app, "SetReconfigurationPermissionsV1",
                        {"--settingId", "0", "--configuration",
                         std::to_string(config), "--value", "3"});

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_set_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       msg);
        EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
    }
}

// ===========================================================================
// GetDevicemodeSettings — with device mode value
// ===========================================================================

TEST(ConfigBranch, GetDevicemodeSettings_ModeEnabled)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDevicemodeSettings", {"--mode_index", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_setting_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
                                         static_cast<uint8_t>(1), msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// SetDevicemodeSettings — with reason code
// ===========================================================================

TEST(ConfigBranch, SetDevicemodeSettings_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetDevicemodeSettings",
                    {"--index", "0", "--mode", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// GetConfidentialComputeModeV1 — different current/pending mode values
// ===========================================================================

TEST(ConfigBranch, GetConfidentialComputeModeV1_DifferentModes)
{
    CLI::App app;
    setupConfigCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_confidential_compute_mode_v1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_confidential_compute_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 0 /*current=disabled*/,
                                                 1 /*pending=enabled*/, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ===========================================================================
// EnableDisableGpuIstMode — ALL_GPUS_DEVICE_INDEX (10)
// ===========================================================================

TEST(ConfigBranch, EnableDisableGpuIstMode_AllGpus)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "EnableDisableGpuIstMode",
                    {"--deviceIndex", "10", "--value", "1"});

    auto [rc, reqMsg] = commands[9]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// GetLTXMode / SetLTXMode — LTX Mode Enable/Disable feature
// (NSM Type 5 idx DEVICE_MODE_LTX). Mirrors the GetDeviceModeSettingsV2 /
// SetDeviceModeSettingsV2 coverage above, which targets the same underlying
// Get/Set Device Mode Settings v2 command.
// ===========================================================================

TEST(ConfigBranch, GetLTXMode_ValidResponse_PrintsCurrentAndPendingMode)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetLTXMode");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp) +
                             LTX_MODE_DATA_SIZE * 2);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t currentMode = NSM_LTX_MODE_ENABLED;
    uint8_t pendingMode = NSM_LTX_MODE_DISABLED;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, LTX_MODE_DATA_SIZE,
        &pendingMode, LTX_MODE_DATA_SIZE, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(commands[24]->parseResponseMsg(msg, buf.size()));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("CurrentMode"), std::string::npos);
    EXPECT_NE(output.find("PendingMode"), std::string::npos);
    EXPECT_NE(output.find("Enabled"), std::string::npos);
    EXPECT_NE(output.find("Disabled"), std::string::npos);
}

// Primary regression test for the review correction: a missing/short
// CurrentMode must be reported as an error, not printed as a partial
// success. Matches NsmSwitchLTXMode::handleResponseMsg's hard-error rule.
TEST(ConfigBranch, GetLTXMode_CurrentLengthZero_ReturnsError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetLTXMode");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, nullptr, 0, nullptr, 0, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW(commands[24]->parseResponseMsg(msg, buf.size()));
    std::string stdoutput = testing::internal::GetCapturedStdout();
    std::string stderrOutput = testing::internal::GetCapturedStderr();
    // No partial-success JSON on stdout ...
    EXPECT_TRUE(stdoutput.empty());
    // ... and an explicit error is reported instead.
    EXPECT_NE(stderrOutput.find("LTX mode payload invalid"), std::string::npos);
}

TEST(ConfigBranch, SetLTXMode_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetLTXMode", {"-M", "Enabled"});

    auto [rc, reqMsg] = commands[25]->createRequestMsg();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[25]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, SetLTXMode_InvalidMode_ReturnsError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetLTXMode", {"-M", "NotAMode"});

    auto [rc, reqMsg] = commands[25]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(reqMsg.empty());
}

// ===========================================================================
// GetUPhyMode / SetUPhyMode — LTX Mode Enable/Disable feature
// (NSM Type 5 idx DEVICE_MODE_UPHY). Parallel to GetLTXMode / SetLTXMode.
// ===========================================================================

TEST(ConfigBranch, GetUPhyMode_ValidResponse_PrintsCurrentAndPendingMode)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetUPhyMode");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp) +
                             UPHY_MODE_DATA_SIZE * 2);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t currentMode = NSM_UPHY_MODE_ENABLED;
    uint8_t pendingMode = NSM_UPHY_MODE_DISABLED;
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, &currentMode, UPHY_MODE_DATA_SIZE,
        &pendingMode, UPHY_MODE_DATA_SIZE, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    testing::internal::CaptureStdout();
    EXPECT_NO_THROW(commands[26]->parseResponseMsg(msg, buf.size()));
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("CurrentMode"), std::string::npos);
    EXPECT_NE(output.find("PendingMode"), std::string::npos);
    EXPECT_NE(output.find("Enabled"), std::string::npos);
    EXPECT_NE(output.find("Disabled"), std::string::npos);
}

// Primary regression test for the review correction: same tightened
// payload-length validation as GetLTXMode, for UPhy mode.
TEST(ConfigBranch, GetUPhyMode_CurrentLengthZero_ReturnsError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetUPhyMode");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_mode_settings_v2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, nullptr, 0, nullptr, 0, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    EXPECT_NO_THROW(commands[26]->parseResponseMsg(msg, buf.size()));
    std::string stdoutput = testing::internal::GetCapturedStdout();
    std::string stderrOutput = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(stdoutput.empty());
    EXPECT_NE(stderrOutput.find("UPhy mode payload invalid"),
              std::string::npos);
}

TEST(ConfigBranch, SetUPhyMode_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetUPhyMode", {"-M", "Enabled"});

    auto [rc, reqMsg] = commands[27]->createRequestMsg();
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[27]->parseResponseMsg(msg, buf.size()));
}

TEST(ConfigBranch, SetUPhyMode_InvalidMode_ReturnsError)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetUPhyMode", {"-M", "NotAMode"});

    auto [rc, reqMsg] = commands[27]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(reqMsg.empty());
}

} // namespace nsmtool::config
