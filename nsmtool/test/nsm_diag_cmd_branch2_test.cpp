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

#include "base.h"
#include "debug-token.h"
#include "debug-token/error.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include "../nsm_diag_cmd.cpp"

#include <gtest/gtest.h>

namespace nsmtool::diag
{

static void setupDiagCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* diagSub = app.get_subcommand("diag");
    if (!diagSub)
        return;
    auto* leafSub = diagSub->get_subcommand(cmdName);
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

TEST(NsmDiagBranch2, QueryTokenParams_AllDeviceTypes)
{
    uint8_t devTypes[] = {NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_EROT,
                          NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_GPU,
                          NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_NVSWITCH,
                          NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_CX7,
                          NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_MCU,
                          NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_CX8,
                          99};
    uint8_t statuses[] = {
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_OK,
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_TOKEN_ALREADY_APPLIED,
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_TOKEN_NOT_SUPPORTED,
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_NO_KEY_CONFIGURED,
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_INTERFACE_NOT_ALLOWED,
        99};
    uint8_t opcodes[] = {
        NSM_DEBUG_TOKEN_OPCODE_RMCS,      NSM_DEBUG_TOKEN_OPCODE_RMDT,
        NSM_DEBUG_TOKEN_OPCODE_CRCS,      NSM_DEBUG_TOKEN_OPCODE_CRDT,
        NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC, 99};
    for (size_t i = 0; i < sizeof(devTypes); i++)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "QueryTokenParameters", {"-o", "0"});
        nsm_debug_token_request req{};
        req.device_type = devTypes[i];
        req.status = statuses[i % sizeof(statuses)];
        req.token_opcode = opcodes[i % sizeof(opcodes)];
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_token_parameters_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, ERR_NULL, &req,
                                               msg);
        EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
    }
}

TEST(NsmDiagBranch2, QueryTokenParams_ErrorPath)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenParameters", {"-o", "0"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, QueryTokenStatus_AllBranches)
{
    struct TC
    {
        uint8_t status;
        uint8_t info;
        uint8_t type;
    };
    TC cases[] = {
        {NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ENDED,
         NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE, NSM_DEBUG_TOKEN_TYPE_FRC},
        {NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE,
         NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION,
         NSM_DEBUG_TOKEN_TYPE_CRCS},
        {NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
         NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED,
         NSM_DEBUG_TOKEN_TYPE_CRDT},
        {NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
         NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_END_REQUEST_NOT_ACCEPTED,
         NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE},
        {NSM_DEBUG_TOKEN_STATUS_CHALLENGE_PROVIDED,
         NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_QUERY_DISALLOWED,
         99},
        {NSM_DEBUG_TOKEN_STATUS_INSTALLATION_TIMEOUT,
         NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE, 0},
        {NSM_DEBUG_TOKEN_STATUS_TOKEN_TIMEOUT, 99, 0},
        {99, 0, 0},
    };
    for (const auto& tc : cases)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "QueryTokenStatus", {"-t", "0"});
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_token_status_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_query_token_status_resp(
            0, NSM_SUCCESS, ERR_NULL,
            static_cast<nsm_debug_token_status>(tc.status),
            static_cast<nsm_debug_token_status_additional_info>(tc.info),
            static_cast<nsm_debug_token_type>(tc.type), 3600, msg);
        EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
    }
}

TEST(NsmDiagBranch2, QueryTokenStatus_ErrorPath)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenStatus", {"-t", "0"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, ProvideToken_Success)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "ProvideToken", {"-t", "AABB"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r =
        reinterpret_cast<nsm_common_resp*>(buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_SUCCESS;
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, ProvideToken_ErrorPath)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "ProvideToken", {"-t", "AABB"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, DisableTokens_Success)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "DisableTokens");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r =
        reinterpret_cast<nsm_common_resp*>(buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_SUCCESS;
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, DisableTokens_ErrorPath)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "DisableTokens");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, QueryDeviceIds_Success)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIds");
    uint8_t devId[] = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_device_ids_resp) + sizeof(devId));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, devId,
                                     sizeof(devId), msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, QueryDeviceIds_ErrorPath)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIds");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, EnableDisableWriteProtected_SuccessAndError)
{
    for (bool success : {true, false})
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "EnableDisableWriteProtected",
                        {"-d", "128", "-V", "1"});
        if (success)
        {
            std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp));
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            auto* r = reinterpret_cast<nsm_common_resp*>(buf.data() +
                                                         sizeof(nsm_msg_hdr));
            r->completion_code = NSM_SUCCESS;
            EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
        }
        else
        {
            std::vector<uint8_t> buf(
                sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
            buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
        }
    }
}

TEST(NsmDiagBranch2, ResetNetworkDevice_SuccessAndError)
{
    for (bool success : {true, false})
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "ResetNetworkDevice", {"-M", "0"});
        if (success)
        {
            std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp));
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            auto* r = reinterpret_cast<nsm_common_resp*>(buf.data() +
                                                         sizeof(nsm_msg_hdr));
            r->completion_code = NSM_SUCCESS;
            EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
        }
        else
        {
            std::vector<uint8_t> buf(
                sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
            buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
        }
    }
}

TEST(NsmDiagBranch2, GetNetworkDeviceDebugInfo_WithAndWithoutData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceDebugInfo", {"-t", "0", "-r", "0"});
    uint8_t segData[] = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_network_device_debug_info_resp) +
                             sizeof(segData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_network_device_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, segData,
                                              sizeof(segData), 1, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, GetNetworkDeviceDebugInfo_NoData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceDebugInfo", {"-t", "0", "-r", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_network_device_debug_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_network_device_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, nullptr,
                                              0, 0, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, EraseTrace_AllStatus)
{
    uint8_t statuses[] = {ERASE_TRACE_NO_DATA_ERASED, ERASE_TRACE_DATA_ERASED,
                          ERASE_TRACE_DATA_ERASE_INPROGRESS, 99};
    for (auto s : statuses)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "EraseTrace");
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_erase_trace_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_erase_trace_resp(0, NSM_SUCCESS, ERR_NULL, s, msg);
        EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
    }
}

TEST(NsmDiagBranch2, GetNetworkDeviceLogInfo_AllSyncedTypes)
{
    for (uint8_t syncType : {0, 1, 2})
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "GetNetworkDeviceLogInfo", {"-r", "0"});
        nsm_device_log_info_breakdown logInfo{};
        logInfo.synced_time = syncType;
        logInfo.length = (syncType == 0) ? 2 : 0;
        uint8_t logData[] = {0xAA, 0xBB, 0xCC, 0xDD};
        size_t logSz = (syncType == 0) ? sizeof(logData) : 0;
        const uint8_t* logPtr = (syncType == 0) ? logData : nullptr;
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_network_device_log_info_resp) +
                                 logSz);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_get_network_device_log_info_resp(0, NSM_SUCCESS, ERR_NULL,
                                                (syncType == 0) ? 1u : 0u,
                                                logInfo, logPtr, logSz, msg);
        EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
    }
}

TEST(NsmDiagBranch2, EraseDebugInfo_AllStatus)
{
    uint8_t statuses[] = {ERASE_TRACE_NO_DATA_ERASED, ERASE_TRACE_DATA_ERASED,
                          ERASE_TRACE_DATA_ERASE_INPROGRESS, 99};
    for (auto s : statuses)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "EraseDebugInfo", {"-t", "0"});
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_erase_debug_info_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_erase_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, s, msg);
        EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
    }
}

TEST(NsmDiagBranch2, GetDeviceDiagnostics_Success)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetDeviceDiagnostics", {"-s", "0"});
    uint8_t segData[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_diagnostics_resp) +
                             sizeof(segData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_diagnostics_resp(0, NSM_SUCCESS, ERR_NULL, segData,
                                       sizeof(segData), 1, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, GetDeviceDebugParameters_Success)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetDeviceDebugParameters",
                    {"-t", "0", "-p", "0", "-i", "0", "-s", "0"});
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint16_t dataSize = sizeof(data);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_debug_parameters_resp) +
                             sizeof(data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_debug_parameters_resp(0, NSM_SUCCESS, ERR_NULL, &dataSize,
                                            data, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagBranch2, SetDeviceDebugParameters_SuccessAndError)
{
    for (bool success : {true, false})
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(
            app, "SetDeviceDebugParameters",
            {"-t", "0", "-p", "0", "-i", "0", "-s", "0", "-d", "1"});
        if (success)
        {
            std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp));
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            auto* r = reinterpret_cast<nsm_common_resp*>(buf.data() +
                                                         sizeof(nsm_msg_hdr));
            r->completion_code = NSM_SUCCESS;
            EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
        }
        else
        {
            std::vector<uint8_t> buf(
                sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
            buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
        }
    }
}

TEST(NsmDiagBranch2, EraseToken_SuccessAndError)
{
    for (bool success : {true, false})
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "EraseToken", {"-t", "0"});
        if (success)
        {
            std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_common_resp));
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            auto* r = reinterpret_cast<nsm_common_resp*>(buf.data() +
                                                         sizeof(nsm_msg_hdr));
            r->completion_code = NSM_SUCCESS;
            EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
        }
        else
        {
            std::vector<uint8_t> buf(
                sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
            buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
            auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
            EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
        }
    }
}

TEST(NsmDiagBranch2, QueryToken_ErrorPath)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryToken");
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::diag
