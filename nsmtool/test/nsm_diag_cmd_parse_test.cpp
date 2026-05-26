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
#include "debug-token.h"
#include "diagnostics.h"

#include "../nsm_diag_cmd.cpp"

#include <gtest/gtest.h>

// Diag commands (after registerCommand) in order:
//   [0]  QueryTokenParameters
//   [1]  ProvideToken
//   [2]  DisableTokens
//   [3]  QueryTokenStatus
//   [4]  QueryDeviceIds
//   [5]  EnableDisableWriteProtected
//   [6]  ResetNetworkDevice
//   [7]  GetNetworkDeviceDebugInfo
//   [8]  EraseTrace
//   [9]  GetNetworkDeviceLogInfo
//   [10] EraseDebugInfo
//   [11] QueryResetStatistics
//   [12] GetDeviceDiagnostics
//   [13] GetDeviceDebugParameters
//   [14] SetDeviceDebugParameters
//   [15] InstallToken
//   [16] EraseToken
//   [17] QueryToken

namespace nsmtool::diag
{

// ---- Helpers ----------------------------------------------------------------

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

// ---- QueryTokenParameters ---------------------------------------------------

TEST(NsmDiagCmdParse, QueryTokenParameters_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenParameters", {"--opcode", "0"});

    nsm_debug_token_request tokenReq{};
    // decode_nsm_query_token_parameters_resp validates token_request_size
    tokenReq.token_request_size = sizeof(nsm_debug_token_request);
    tokenReq.device_type = NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_GPU;
    tokenReq.status = NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_OK;
    tokenReq.token_opcode = NSM_DEBUG_TOKEN_OPCODE_RMCS;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_token_parameters_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, ERR_NULL, &tokenReq,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryTokenParameters_ErrorDeviceType)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenParameters", {"--opcode", "0"});

    nsm_debug_token_request tokenReq{};
    tokenReq.token_request_size = sizeof(nsm_debug_token_request);
    tokenReq.device_type = NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_NVSWITCH;
    tokenReq.status =
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_TOKEN_ALREADY_APPLIED;
    tokenReq.token_opcode = NSM_DEBUG_TOKEN_OPCODE_RMDT;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_token_parameters_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, ERR_NULL, &tokenReq,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryTokenParameters_AllDeviceTypes)
{
    const uint8_t deviceTypes[] = {
        NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_CX7, NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_MCU,
        NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_NIC,
        0xFF // default case
    };
    const uint8_t statuses[] = {
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_TOKEN_NOT_SUPPORTED,
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_NO_KEY_CONFIGURED,
        NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_INTERFACE_NOT_ALLOWED,
        0xFF // default
    };
    const uint8_t opcodes[] = {
        NSM_DEBUG_TOKEN_OPCODE_CRCS, NSM_DEBUG_TOKEN_OPCODE_CRDT,
        NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC,
        0xFF // default
    };

    for (size_t i = 0; i < 4; i++)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "QueryTokenParameters", {"--opcode", "0"});

        nsm_debug_token_request tokenReq{};
        tokenReq.token_request_size = sizeof(nsm_debug_token_request);
        tokenReq.device_type = deviceTypes[i];
        tokenReq.status = statuses[i];
        tokenReq.token_opcode = opcodes[i];

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_token_parameters_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, ERR_NULL,
                                               &tokenReq, msg);
        EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
    }
}

// ---- ProvideToken -----------------------------------------------------------

TEST(NsmDiagCmdParse, ProvideToken_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_provide_token_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ---- DisableTokens ----------------------------------------------------------

TEST(NsmDiagCmdParse, DisableTokens_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_disable_tokens_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryTokenStatus -------------------------------------------------------

TEST(NsmDiagCmdParse, QueryTokenStatus_ParseResponseSuccess_AllStatuses)
{
    const int statuses[] = {NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ENDED,
                            NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE,
                            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
                            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
                            NSM_DEBUG_TOKEN_STATUS_CHALLENGE_PROVIDED,
                            NSM_DEBUG_TOKEN_STATUS_INSTALLATION_TIMEOUT,
                            NSM_DEBUG_TOKEN_STATUS_TOKEN_TIMEOUT,
                            0xFF};
    const int addInfos[] = {
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_END_REQUEST_NOT_ACCEPTED,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_QUERY_DISALLOWED,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE,
        0xFF,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE};
    const int tokenTypes[] = {NSM_DEBUG_TOKEN_TYPE_FRC,
                              NSM_DEBUG_TOKEN_TYPE_CRCS,
                              NSM_DEBUG_TOKEN_TYPE_CRDT,
                              NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE,
                              0xFF,
                              NSM_DEBUG_TOKEN_TYPE_FRC,
                              NSM_DEBUG_TOKEN_TYPE_FRC,
                              NSM_DEBUG_TOKEN_TYPE_FRC};

    for (size_t i = 0; i < 8; i++)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "QueryTokenStatus", {"--type", "0"});

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_query_token_status_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_nsm_query_token_status_resp(
            0, NSM_SUCCESS, ERR_NULL,
            static_cast<nsm_debug_token_status>(statuses[i]),
            static_cast<nsm_debug_token_status_additional_info>(addInfos[i]),
            static_cast<nsm_debug_token_type>(tokenTypes[i]), 0, msg);
        EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
    }
}

// ---- QueryDeviceIds ---------------------------------------------------------

TEST(NsmDiagCmdParse, QueryDeviceIds_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);

    uint8_t deviceId[] = {0x01, 0x02, 0x03, 0x04};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(deviceId));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, deviceId,
                                     sizeof(deviceId), msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- EnableDisableWriteProtected --------------------------------------------

TEST(NsmDiagCmdParse, EnableDisableWriteProtected_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EnableDisableWriteProtected",
                    {"--dataId", "128", "--value", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_enable_disable_wp_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- ResetNetworkDevice -----------------------------------------------------

TEST(NsmDiagCmdParse, ResetNetworkDevice_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "ResetNetworkDevice", {"--mode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_reset_network_device_resp(0, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ---- GetNetworkDeviceDebugInfo ----------------------------------------------

TEST(NsmDiagCmdParse, GetNetworkDeviceDebugInfo_ParseResponseNoData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceDebugInfo",
                    {"--debugInfoType", "0", "--recordHandle", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_network_device_debug_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_network_device_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, nullptr,
                                              0, 0, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, GetNetworkDeviceDebugInfo_ParseResponseWithData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceDebugInfo",
                    {"--debugInfoType", "0", "--recordHandle", "0"});

    uint8_t seg[] = {0xDE, 0xAD};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_network_device_debug_info_resp) +
                             sizeof(seg) - 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_network_device_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, seg,
                                              sizeof(seg), 42, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// ---- EraseTrace -------------------------------------------------------------

TEST(NsmDiagCmdParse, EraseTrace_NoDataErased)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_trace_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_trace_resp(0, NSM_SUCCESS, ERR_NULL,
                            ERASE_TRACE_NO_DATA_ERASED, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseTrace_DataErased)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_trace_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_trace_resp(0, NSM_SUCCESS, ERR_NULL, ERASE_TRACE_DATA_ERASED,
                            msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseTrace_InProgress)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_trace_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_trace_resp(0, NSM_SUCCESS, ERR_NULL,
                            ERASE_TRACE_DATA_ERASE_INPROGRESS, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseTrace_Unknown)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_trace_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_trace_resp(0, NSM_SUCCESS, ERR_NULL, 0xFF /*unknown*/, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ---- GetNetworkDeviceLogInfo ------------------------------------------------

TEST(NsmDiagCmdParse, GetNetworkDeviceLogInfo_ParseResponseNoData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceLogInfo", {"--recordHandle", "0"});

    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_network_device_log_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_network_device_log_info_resp(0, NSM_SUCCESS, ERR_NULL, 0,
                                            logInfo, nullptr, 0, msg);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, GetNetworkDeviceLogInfo_ParseResponseWithData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceLogInfo", {"--recordHandle", "0"});

    nsm_device_log_info_breakdown logInfo{};
    logInfo.synced_time = 1; // "Synced" branch
    uint8_t logData[] = {0xAA, 0xBB};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_network_device_log_info_resp) +
                             sizeof(logData) - 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_network_device_log_info_resp(
        0, NSM_SUCCESS, ERR_NULL, 10, logInfo, logData, sizeof(logData), msg);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// Note: synced_time is a 1-bit bitfield; the else branch in parseResponseMsg
// is unreachable, so no "unknown synced_time" test is needed.

// ---- EraseDebugInfo ---------------------------------------------------------

TEST(NsmDiagCmdParse, EraseDebugInfo_NoData)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseDebugInfo", {"--infoType", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_debug_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_debug_info_resp(0, NSM_SUCCESS, ERR_NULL,
                                 ERASE_TRACE_NO_DATA_ERASED, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseDebugInfo_DataErased)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseDebugInfo", {"--infoType", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_debug_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_debug_info_resp(0, NSM_SUCCESS, ERR_NULL,
                                 ERASE_TRACE_DATA_ERASED, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseDebugInfo_InProgress)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseDebugInfo", {"--infoType", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_debug_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_debug_info_resp(0, NSM_SUCCESS, ERR_NULL,
                                 ERASE_TRACE_DATA_ERASE_INPROGRESS, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// [10] EraseDebugInfo – default case (lines 1196-1198 in nsm_diag_cmd.cpp)
// result_status = 3 is not covered by ERASE_TRACE_* constants → hits default:
TEST(NsmDiagCmdParse, EraseDebugInfo_UnknownStatus)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseDebugInfo", {"--infoType", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_erase_debug_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_erase_debug_info_resp(0, NSM_SUCCESS, ERR_NULL, 3, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryResetStatistics ---------------------------------------------------

TEST(NsmDiagCmdParse, QueryResetStatistics_ParseResponseNoSamples)
{
    CLI::App app;
    setupDiagCommands(app);

    // Aggregate response with 0 samples
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_DEVICE_RESET_STATISTICS, NSM_SUCCESS, 0,
                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryResetStatistics_ParseResponseWithSamples)
{
    CLI::App app;
    setupDiagCommands(app);

    // Build an aggregate response with samples covering all tag paths:
    // tag 0  (uint16_t)  → sample_len = 4
    // tag 7  (uint8_t)   → sample_len = 3
    // tag 8  (32 bytes)  → sample_len = 34
    // tag 9  (unknown)   → sample_len = 4 (NSM_SW_ERROR_DATA path)
    constexpr size_t sample_bytes = 4 + 3 + 34 + 4;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
                             sample_bytes);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_DEVICE_RESET_STATISTICS, NSM_SUCCESS, 4,
                          msg);

    uint8_t* ptr = msg->payload + sizeof(nsm_aggregate_resp);
    size_t sample_len = 0;

    // tag 0: PF_FLR_ResetEntryCount (uint16_t)
    uint8_t count_data[2] = {0x05, 0x00};
    encode_aggregate_resp_sample(
        0, true, count_data, 2,
        reinterpret_cast<nsm_aggregate_resp_sample*>(ptr), &sample_len);
    ptr += sample_len;

    // tag 7: LastResetType (uint8_t)
    uint8_t enum_data[1] = {0x01};
    encode_aggregate_resp_sample(
        7, true, enum_data, 1,
        reinterpret_cast<nsm_aggregate_resp_sample*>(ptr), &sample_len);
    ptr += sample_len;

    // tag 8: BootReason (4 × uint64_t = 32 bytes)
    uint8_t boot_data[32] = {};
    encode_aggregate_resp_sample(
        8, true, boot_data, 32,
        reinterpret_cast<nsm_aggregate_resp_sample*>(ptr), &sample_len);
    ptr += sample_len;

    // tag 9: unknown tag → handleSampleData returns NSM_SW_ERROR_DATA
    uint8_t unk_data[2] = {0x00, 0x00};
    encode_aggregate_resp_sample(
        9, true, unk_data, 2, reinterpret_cast<nsm_aggregate_resp_sample*>(ptr),
        &sample_len);
    ptr += sample_len;

    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryResetStatistics_ParseResponseDecodeErrors)
{
    CLI::App app;
    setupDiagCommands(app);

    // Build samples with wrong data lengths to hit the decode-error paths:
    // tag 0 with data_len=4 → decode_reset_count_data fails (expects 2)
    // tag 7 with data_len=4 → decode_reset_enum_data fails (expects 1)
    // tag 8 with data_len=4 → decode_reset_count_256data fails (expects 32)
    // Each sample: data_len + sizeof(nsm_aggregate_resp_sample) - 1 = 4 + 2 = 6
    // bytes
    constexpr size_t sample_bytes = 6 + 6 + 6;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
                             sample_bytes);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_DEVICE_RESET_STATISTICS, NSM_SUCCESS, 3,
                          msg);

    uint8_t* ptr = msg->payload + sizeof(nsm_aggregate_resp);
    size_t sample_len = 0;
    uint8_t data4[4] = {};

    encode_aggregate_resp_sample(
        0, true, data4, 4, reinterpret_cast<nsm_aggregate_resp_sample*>(ptr),
        &sample_len);
    ptr += sample_len;

    encode_aggregate_resp_sample(
        7, true, data4, 4, reinterpret_cast<nsm_aggregate_resp_sample*>(ptr),
        &sample_len);
    ptr += sample_len;

    encode_aggregate_resp_sample(
        8, true, data4, 4, reinterpret_cast<nsm_aggregate_resp_sample*>(ptr),
        &sample_len);
    ptr += sample_len;

    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- GetDeviceDiagnostics ---------------------------------------------------

TEST(NsmDiagCmdParse, GetDeviceDiagnostics_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetDeviceDiagnostics", {"--segmentId", "0"});

    uint8_t segData[] = "test data";
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_diagnostics_resp) +
                             sizeof(segData) - 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_diagnostics_resp(0, NSM_SUCCESS, ERR_NULL, segData,
                                       sizeof(segData) - 1, 1, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ---- GetDeviceDebugParameters -----------------------------------------------

TEST(NsmDiagCmdParse, GetDeviceDebugParameters_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetDeviceDebugParameters",
                    {"--configType", "0", "--portNumber", "0",
                     "--parameterIndex", "0", "--parameterSubId", "0"});

    uint8_t data[] = {0x11, 0x22};
    uint16_t dataSize = sizeof(data);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_device_debug_parameters_resp) +
                             sizeof(data) - 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_debug_parameters_resp(0, NSM_SUCCESS, ERR_NULL, &dataSize,
                                            data, msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

// ---- SetDeviceDebugParameters -----------------------------------------------

TEST(NsmDiagCmdParse, SetDeviceDebugParameters_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "SetDeviceDebugParameters",
                    {"--configType", "0", "--portNumber", "0",
                     "--parameterIndex", "0", "--parameterSubId", "0", "--data",
                     "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Build a minimal common response with NSM_SUCCESS
    auto* hdr = reinterpret_cast<nsm_msg_hdr*>(buf.data());
    hdr->nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;
    auto* resp = reinterpret_cast<nsm_common_resp*>(msg->payload);
    resp->completion_code = NSM_SUCCESS;
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ---- InstallToken -----------------------------------------------------------

TEST(NsmDiagCmdParse, InstallToken_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    // remaining=0 so the "if (remaining > 0)" branch is false
    // (fd=-1 so createChunkRequestMsg won't open file)

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_install_token_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ---- EraseToken -------------------------------------------------------------

TEST(NsmDiagCmdParse, EraseToken_ParseResponseSuccess)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseToken", {"--type", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_erase_token_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryToken -------------------------------------------------------------

TEST(NsmDiagCmdParse, QueryToken_ParseResponseNoPayload)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, nullptr, 0, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryToken_ParseResponseWithPayload)
{
    CLI::App app;
    setupDiagCommands(app);

    // Build a properly structured TLV payload using the encoder.
    // The TLV decoder requires a valid StructureHeader (32 bytes) before items.
    debug_token::tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);
    encoder.add(
        static_cast<uint16_t>(debug_token::types::Common::InstallationStatus),
        static_cast<uint8_t>(1));
    auto tlv = encoder.encode();

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             tlv.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, tlv.data(),
                                tlv.size(), msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- Error path tests -------------------------------------------------------

TEST(NsmDiagCmdParse, QueryTokenParameters_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenParameters", {"--opcode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, ProvideToken_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, DisableTokens_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryTokenStatus_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenStatus", {"--type", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryDeviceIds_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseTrace_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, GetDeviceDiagnostics_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetDeviceDiagnostics", {"--segmentId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseToken_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseToken", {"--type", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryToken_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// Error paths for commands not yet covered above

TEST(NsmDiagCmdParse, EnableDisableWriteProtected_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EnableDisableWriteProtected",
                    {"--dataId", "128", "--value", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, ResetNetworkDevice_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "ResetNetworkDevice", {"--mode", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, GetNetworkDeviceDebugInfo_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceDebugInfo",
                    {"--debugInfoType", "0", "--recordHandle", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, GetNetworkDeviceLogInfo_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetNetworkDeviceLogInfo", {"--recordHandle", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, EraseDebugInfo_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseDebugInfo", {"--infoType", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, QueryResetStatistics_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    // parseAggregateResponse adds sizeof(nsm_msg_hdr) to payloadLength, so
    // pass 1 byte to ensure msg_len (1+hdr=6) < hdr+aggregate_resp(9),
    // triggering NSM_SW_ERROR_LENGTH without reading beyond the buffer
    std::vector<uint8_t> buf(1, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, GetDeviceDebugParameters_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "GetDeviceDebugParameters",
                    {"--configType", "0", "--portNumber", "0",
                     "--parameterIndex", "0", "--parameterSubId", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, SetDeviceDebugParameters_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "SetDeviceDebugParameters",
                    {"--configType", "0", "--portNumber", "0",
                     "--parameterIndex", "0", "--parameterSubId", "0", "--data",
                     "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiagCmdParse, InstallToken_ParseResponseError)
{
    CLI::App app;
    setupDiagCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ---- InstallToken createRequestMsg error paths ------------------------------

TEST(NsmDiagCmdParse, InstallToken_CreateRequestMsg_BufferSizeZero)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "0", "--file", "/dev/null"});

    auto [rc, msg] = commands[15]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmDiagCmdParse, InstallToken_CreateRequestMsg_BufferSizeTooLarge)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "65536", "--file", "/dev/null"});

    auto [rc, msg] = commands[15]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmDiagCmdParse, InstallToken_CreateRequestMsg_FileNotFound)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "1024", "--file",
                     "/nonexistent/path/to/token_file.bin"});

    auto [rc, msg] = commands[15]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---- QueryTokenParameters: EROT device type (line 178) ----------------------

TEST(NsmDiagCmdParse, QueryTokenParameters_ErotDeviceType)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "QueryTokenParameters", {"--opcode", "0"});

    nsm_debug_token_request tokenReq{};
    tokenReq.token_request_size = sizeof(nsm_debug_token_request);
    tokenReq.device_type = NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_EROT;
    tokenReq.status = NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_OK;
    tokenReq.token_opcode = NSM_DEBUG_TOKEN_OPCODE_RMCS;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_token_parameters_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, ERR_NULL, &tokenReq,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- EraseDebugInfo success with all result_status values -------------------

TEST(NsmDiagCmdParse, EraseDebugInfo_ParseResponseAllStatuses)
{
    const uint8_t statuses[] = {
        static_cast<uint8_t>(ERASE_TRACE_NO_DATA_ERASED),
        static_cast<uint8_t>(ERASE_TRACE_DATA_ERASED),
        static_cast<uint8_t>(ERASE_TRACE_DATA_ERASE_INPROGRESS),
        0xFF, // default case
    };

    for (auto status : statuses)
    {
        CLI::App app;
        setupDiagCommands(app);
        parseSubcmdArgs(app, "EraseDebugInfo", {"--infoType", "0"});

        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp), 0);
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        auto* resp = reinterpret_cast<nsm_erase_trace_resp*>(msg->payload);
        resp->hdr.completion_code = NSM_SUCCESS;
        resp->hdr.data_size =
            htole16(sizeof(nsm_erase_trace_resp) - sizeof(nsm_common_resp));
        resp->result_status = status;
        EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
    }
}

// ---- InstallToken: create a temporary binary file helper --------------------

static std::string createDiagTokenFile(const std::string& path, size_t size)
{
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    std::vector<uint8_t> zeros(size, 0);
    f.write(reinterpret_cast<const char*>(zeros.data()),
            static_cast<std::streamsize>(size));
    return path;
}

// ---- InstallToken createRequestMsg: valid file (≥17 bytes) ------------------

TEST(NsmDiagCmdParse, InstallToken_CreateRequestMsg_ValidFile)
{
    // 32-byte file: 16-byte header + 16 bytes data → one chunk, remaining=0
    const std::string tmpPath = "/tmp/diag_token_valid_32.bin";
    createDiagTokenFile(tmpPath, 32);

    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "1024", "--file", tmpPath});

    auto [rc, reqMsg] = commands[15]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    ::unlink(tmpPath.c_str());
}

// ---- InstallToken createRequestMsg: file < 16-byte header -------------------

TEST(NsmDiagCmdParse, InstallToken_CreateRequestMsg_FileTooSmall)
{
    // 10-byte file: smaller than HEADER_SIZE (16) → NSM_SW_ERROR_DATA
    const std::string tmpPath = "/tmp/diag_token_small_10.bin";
    createDiagTokenFile(tmpPath, 10);

    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "1024", "--file", tmpPath});

    auto [rc, reqMsg] = commands[15]->createRequestMsg();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    ::unlink(tmpPath.c_str());
}

// ---- InstallToken parseResponseMsg: success with remaining > 0 --------------

TEST(NsmDiagCmdParse, InstallToken_ParseResponseSuccess_WithRemaining)
{
    // 50-byte file: 16-byte header + 34 data
    // bufferSize=32 → chunkSize=20 → remaining=34-20=14 after first chunk
    const std::string tmpPath = "/tmp/diag_token_remaining_50.bin";
    createDiagTokenFile(tmpPath, 50);

    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "32", "--file", tmpPath});

    // Call createRequestMsg to open file and read first chunk (remaining=14)
    auto [rc, reqMsg] = commands[15]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Build a minimal success response (cc=NSM_SUCCESS, no extra data)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<nsm_common_resp*>(msg->payload);
    resp->completion_code = NSM_SUCCESS;
    // parseResponseMsg with remaining>0 enqueues next chunk
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));

    ::unlink(tmpPath.c_str());
}

// ---- QueryToken: correctly sized buffer for payloadSize==0 path -------------

TEST(NsmDiagCmdParse, QueryToken_ParseResponseSuccess_EmptyPayload)
{
    CLI::App app;
    setupDiagCommands(app);

    // nsm_query_token_resp = nsm_common_resp(6) + tlv_payload[1](1) = 7 bytes
    // total buf = sizeof(nsm_msg_hdr)(5) + 7 = 12 bytes
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_resp),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t emptyPayload[1] = {0};
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, emptyPayload, 0, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryToken: ProcessingStatus TLV type ----------------------------------
// Covers the ProcessingStatus branch at lines 1689-1693 of nsm_diag_cmd.cpp.

TEST(NsmDiagCmdParse, QueryToken_ProcessingStatus)
{
    CLI::App app;
    setupDiagCommands(app);

    debug_token::tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);
    encoder.add(
        static_cast<uint16_t>(debug_token::types::Common::ProcessingStatus),
        static_cast<uint8_t>(1));
    auto tlv = encoder.encode();

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             tlv.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, tlv.data(),
                                tlv.size(), msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryToken: TokenTypeSubtypeList with even count -----------------------
// Covers the else branch of lines 1706-1718 (valid pair processing).

TEST(NsmDiagCmdParse, QueryToken_TokenTypeSubtypeList_Even)
{
    CLI::App app;
    setupDiagCommands(app);

    debug_token::tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);
    std::vector<uint32_t> pairs = {1, 2, 3, 4}; // 4 elements = 2 pairs (even)
    encoder.add(
        static_cast<uint16_t>(debug_token::types::Common::TokenTypeSubtypeList),
        pairs);
    auto tlv = encoder.encode();

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             tlv.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, tlv.data(),
                                tlv.size(), msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryToken: TokenTypeSubtypeList with odd count
// ------------------------- Covers the error path at lines 1698-1705 (odd
// number of elements → cerr).

TEST(NsmDiagCmdParse, QueryToken_TokenTypeSubtypeList_Odd)
{
    CLI::App app;
    setupDiagCommands(app);

    debug_token::tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);
    std::vector<uint32_t> vals = {1, 2, 3}; // 3 elements = odd → error path
    encoder.add(
        static_cast<uint16_t>(debug_token::types::Common::TokenTypeSubtypeList),
        vals);
    auto tlv = encoder.encode();

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             tlv.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, tlv.data(),
                                tlv.size(), msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryToken: unknown TLV type -------------------------------------------
// Covers the else branch at lines 1720-1725 (unknown type → raw hex output).

TEST(NsmDiagCmdParse, QueryToken_UnknownType)
{
    CLI::App app;
    setupDiagCommands(app);

    debug_token::tlv_encoder::Structure encoder;
    encoder.setVersion(1, 0);
    std::vector<uint8_t> rawData = {0xDE, 0xAD, 0xBE, 0xEF};
    encoder.add(static_cast<uint16_t>(0x9999), rawData); // unknown type
    auto tlv = encoder.encode();

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             tlv.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL, tlv.data(),
                                tlv.size(), msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- InstallToken: parseResponseMsg error path (cc=NSM_ERROR) ---------------

TEST(NsmDiagCmdParse, InstallToken_ParseResponseError_CcError)
{
    const std::string tmpPath = "/tmp/diag_token_cc_err_50.bin";
    createDiagTokenFile(tmpPath, 50);

    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "InstallToken",
                    {"--bufferSize", "32", "--file", tmpPath});

    auto [rc, reqMsg] = commands[15]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // 9-byte buffer with cc=NSM_ERROR triggers error block (line 1515-1522)
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));

    ::unlink(tmpPath.c_str());
}

// ---- EraseToken: parseResponseMsg error path (cc=NSM_ERROR) -----------------

TEST(NsmDiagCmdParse, EraseToken_ParseResponseError_CcError)
{
    CLI::App app;
    setupDiagCommands(app);
    parseSubcmdArgs(app, "EraseToken", {"--type", "1"});

    // 9-byte buffer with cc=NSM_ERROR triggers error block (line 1591-1598)
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::diag
