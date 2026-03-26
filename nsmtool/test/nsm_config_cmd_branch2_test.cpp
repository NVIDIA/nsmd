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

#include "device-configuration.h"

#include "../nsm_config_cmd.cpp"

#include <gtest/gtest.h>

namespace nsmtool::config
{

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

// Config command indices:
// [0] SetErrorInjectionModeV1
// [1] GetErrorInjectionModeV1
// [2] GetSupportedErrorInjectionTypesV1
// [3] SetErrorInjectionPayload
// [4] GetErrorInjectionPayload
// [5] ActivateErrorInjectionPayload
// [6] SetCurrentErrorInjectionTypesV1
// [7] GetCurrentErrorInjectionTypesV1
// [8] GetFpgaDiagnosticsSettings
// [9] EnableDisableGpuIstMode
// [10] GetReconfigurationPermissionsV1
// [11] SetReconfigurationPermissionsV1
// [12] GetConfidentialComputeModeV1
// [13] SetConfidentialComputeModeV1
// [14] GetDevicemodeSettings
// [15] SetDevicemodeSettings
// [16] GetDeviceModeSettingsV2
// [17] SetDeviceModeSettingsV2

// [0] SetErrorInjectionModeV1 - success
TEST(NsmConfigBranch2, SetErrorInjectionModeV1_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionModeV1", {"-M", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_error_injection_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// [0] SetErrorInjectionModeV1 - error
TEST(NsmConfigBranch2, SetErrorInjectionModeV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionModeV1", {"-M", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// [1] GetErrorInjectionModeV1 - error
TEST(NsmConfigBranch2, GetErrorInjectionModeV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetErrorInjectionModeV1");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// [2] GetSupportedErrorInjectionTypesV1 - error
TEST(NsmConfigBranch2, GetSupportedErrorInjectionTypesV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetSupportedErrorInjectionTypesV1");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// [3] SetErrorInjectionPayload - success
TEST(NsmConfigBranch2, SetErrorInjectionPayload_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionPayload",
                    {"-t", "0", "-s", "0", "-d", "0", "0", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_error_injection_payload_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// [3] SetErrorInjectionPayload - error
TEST(NsmConfigBranch2, SetErrorInjectionPayload_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetErrorInjectionPayload",
                    {"-t", "0", "-s", "0", "-d", "0", "0", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// [5] ActivateErrorInjectionPayload - error
TEST(NsmConfigBranch2, ActivateErrorInjectionPayload_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "ActivateErrorInjectionPayload");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// [6] SetCurrentErrorInjectionTypesV1 - success
TEST(NsmConfigBranch2, SetCurrentErrorInjectionTypesV1_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetCurrentErrorInjectionTypesV1",
                    {"-d", "0", "0", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_current_error_injection_types_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// [6] SetCurrentErrorInjectionTypesV1 - error
TEST(NsmConfigBranch2, SetCurrentErrorInjectionTypesV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetCurrentErrorInjectionTypesV1",
                    {"-d", "0", "0", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// [7] GetCurrentErrorInjectionTypesV1 - error
TEST(NsmConfigBranch2, GetCurrentErrorInjectionTypesV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetCurrentErrorInjectionTypesV1");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// [9] EnableDisableGpuIstMode - success
TEST(NsmConfigBranch2, EnableDisableGpuIstMode_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "EnableDisableGpuIstMode", {"-d", "0", "-V", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_enable_disable_gpu_ist_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// [9] EnableDisableGpuIstMode - error
TEST(NsmConfigBranch2, EnableDisableGpuIstMode_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "EnableDisableGpuIstMode", {"-d", "0", "-V", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// [9] EnableDisableGpuIstMode - invalid device index (createRequestMsg)
TEST(NsmConfigBranch2, EnableDisableGpuIstMode_InvalidDeviceIndex)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "EnableDisableGpuIstMode", {"-d", "15", "-V", "1"});
    auto [rc, reqMsg] = commands[9]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// [13] SetConfidentialComputeModeV1 - success
TEST(NsmConfigBranch2, SetConfidentialComputeModeV1_Success)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetConfidentialComputeModeV1", {"-r", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_confidential_compute_mode_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

// [13] SetConfidentialComputeModeV1 - error
TEST(NsmConfigBranch2, SetConfidentialComputeModeV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetConfidentialComputeModeV1", {"-r", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

// [14] GetDevicemodeSettings - error
TEST(NsmConfigBranch2, GetDevicemodeSettings_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetDevicemodeSettings", {"-r", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// [15] SetDevicemodeSettings - error
TEST(NsmConfigBranch2, SetDevicemodeSettings_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetDevicemodeSettings", {"-i", "0", "-M", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// [4] GetErrorInjectionPayload - error
TEST(NsmConfigBranch2, GetErrorInjectionPayload_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetErrorInjectionPayload", {"-t", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// [10] GetReconfigurationPermissionsV1 - error
TEST(NsmConfigBranch2, GetReconfigurationPermissionsV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetReconfigurationPermissionsV1", {"-i", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// [11] SetReconfigurationPermissionsV1 - error
TEST(NsmConfigBranch2, SetReconfigurationPermissionsV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "SetReconfigurationPermissionsV1",
                    {"-i", "0", "-r", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// [12] GetConfidentialComputeModeV1 - error
TEST(NsmConfigBranch2, GetConfidentialComputeModeV1_Error)
{
    CLI::App app;
    setupConfigCommands(app);
    parseSubcmdArgs(app, "GetConfidentialComputeModeV1");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_common_non_success_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r = reinterpret_cast<nsm_common_non_success_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_ERROR;
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::config
