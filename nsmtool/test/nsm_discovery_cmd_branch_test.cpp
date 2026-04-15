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

/**
 * Branch coverage tests for nsm_discovery_cmd.cpp
 *
 * Targets uncovered branches by exercising:
 *  - Success paths in parseResponseMsg for all command classes
 *  - QueryDeviceIdentification all device type switch cases
 *  - GetHistogramFormat all bucket_data_type switch cases
 *  - GetHistogramData all bucket_data_type switch cases
 *  - GetDeviceCapabilitiesV2 timestampGeneration switch cases
 *  - GetGpioState GPIO value parsing with HIGH/LOW bits
 *  - createRequestMsg for all command classes
 */

// Include the .cpp directly so that its anonymous-namespace `commands`
// vector and all class definitions are in this translation unit.
#include "base.h"
#include "device-capability-discovery.h"

#include "../nsm_discovery_cmd.cpp"

#include <gtest/gtest.h>

namespace nsmtool::discovery
{

// ---- Helpers ----------------------------------------------------------------

static void setupDiscoveryCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* discSub = app.get_subcommand("discovery");
    if (!discSub)
        return;
    auto* leafSub = discSub->get_subcommand(cmdName);
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

// Build a short (error) response: cc=NSM_ERROR
static std::vector<uint8_t> makeErrorResp()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* resp =
        reinterpret_cast<nsm_common_resp*>(buf.data() + sizeof(nsm_msg_hdr));
    resp->completion_code = NSM_ERROR;
    return buf;
}

// Discovery commands (after registerCommand) in order:
//   [0]  Ping
//   [1]  GetSupportedMessageTypes
//   [2]  GetSupportedCommandCodes
//   [3]  QueryDeviceIdentification
//   [4]  GetHistogramFormat
//   [5]  GetHistogramData
//   [6]  GetDeviceCapabilitiesV2
//   [7]  GetGpioState

// ---- Ping success path ------------------------------------------------------

TEST(NsmDiscoveryBranch, Ping_Success)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "Ping");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_ping_resp(0, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, Ping_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "Ping");

    auto [rc, requestMsg] = commands[0]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(requestMsg.size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

// ---- GetSupportedMessageTypes success path ----------------------------------

TEST(NsmDiscoveryBranch, GetSupportedMessageTypes_Success)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetSupportedMessageTypes");

    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0x03; // bits 0 and 1 set
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_supported_nvidia_message_types_resp),
        0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_nvidia_message_types_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   types, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetSupportedMessageTypes_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetSupportedMessageTypes");

    auto [rc, requestMsg] = commands[1]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- GetSupportedCommandCodes success path ----------------------------------

TEST(NsmDiscoveryBranch, GetSupportedCommandCodes_Success)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetSupportedCommandCodes", {"-t", "0"});

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0x0F;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetSupportedCommandCodes_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetSupportedCommandCodes", {"-t", "0"});

    auto [rc, requestMsg] = commands[2]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- QueryDeviceIdentification all device type switch cases -----------------

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_GPU)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_GPU, 0, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_Switch)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_SWITCH, 1, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_PCIeBridge)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_PCIE_BRIDGE, 2, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_Baseboard)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_BASEBOARD, 3, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_ERoT)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_EROT, 4, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_MCTPBridge)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_MCTP_BRIDGE, 5, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_CPU)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_CPU, 6, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_Unknown)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_DEV_ID_UNKNOWN, 7, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_InvalidDefault)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    // Use a value not in the enum to hit the default case
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL, 0xFE, 8,
                                            msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, QueryDeviceIdentification_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "QueryDeviceIdentification");

    auto [rc, requestMsg] = commands[3]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- GetHistogramFormat all bucket_data_type switch cases
// --------------------

// Helper to build histogram format response with given data type and buckets
static std::vector<uint8_t> makeHistogramFormatResp(uint8_t dataType,
                                                    uint16_t numBuckets)
{
    nsm_histogram_format_metadata meta{};
    meta.num_of_buckets = numBuckets;
    meta.min_sampling_time = 100;
    meta.accumulation_cycle = 1;
    meta.increment_duration = 50;
    meta.bucket_unit_of_measure = 0;
    meta.bucket_data_type = dataType;

    // Determine per-bucket byte size
    size_t elemSize = 1;
    switch (dataType)
    {
        case NvU8:
        case NvS8:
            elemSize = 1;
            break;
        case NvU16:
        case NvS16:
            elemSize = 2;
            break;
        case NvU32:
        case NvS32:
        case NvS24_8:
            elemSize = 4;
            break;
        case NvU64:
        case NvS64:
            elemSize = 8;
            break;
    }

    uint32_t bucketOffsetsSize =
        static_cast<uint32_t>((numBuckets + 1) * elemSize);
    std::vector<uint8_t> bucketOffsets(bucketOffsetsSize, 0);

    size_t respSize = sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_histogram_format_resp) - 1 +
                      bucketOffsetsSize;
    std::vector<uint8_t> buf(respSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_histogram_format_resp(0, NSM_SUCCESS, ERR_NULL, &meta,
                                     bucketOffsets.data(), bucketOffsetsSize,
                                     msg);
    return buf;
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvU8)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvU8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvS8)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvS8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvU16)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvU16, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvS16)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvS16, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvU32)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvU32, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvS32)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvS32, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvU64)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvU64, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvS64)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvS64, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_NvS24_8)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramFormatResp(NvS24_8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_DefaultUnknown)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    // Use invalid data type 0xFF to hit the default case
    auto buf = makeHistogramFormatResp(NvU8, 3);
    // Manually patch the bucket_data_type in the encoded response
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* resp = reinterpret_cast<nsm_get_histogram_format_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    resp->metadata.bucket_data_type = 0xFF;
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto [rc, requestMsg] = commands[4]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmDiscoveryBranch, GetHistogramFormat_ErrorPath)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramFormat", {"-i", "0", "-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- GetHistogramData all bucket_data_type switch cases ---------------------

// Helper to build histogram data response with given data type and buckets
static std::vector<uint8_t> makeHistogramDataResp(uint8_t dataType,
                                                  uint16_t numBuckets)
{
    // Determine per-bucket byte size
    size_t elemSize = 1;
    switch (dataType)
    {
        case NvU8:
        case NvS8:
            elemSize = 1;
            break;
        case NvU16:
        case NvS16:
            elemSize = 2;
            break;
        case NvU32:
        case NvS32:
        case NvS24_8:
            elemSize = 4;
            break;
        case NvU64:
        case NvS64:
            elemSize = 8;
            break;
    }

    uint32_t bucketDataSize = static_cast<uint32_t>(numBuckets * elemSize);
    std::vector<uint8_t> bucketData(bucketDataSize, 0);

    size_t respSize = sizeof(nsm_msg_hdr) +
                      sizeof(nsm_get_histogram_data_resp) - 1 + bucketDataSize;
    std::vector<uint8_t> buf(respSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_histogram_data_resp(0, NSM_SUCCESS, ERR_NULL, dataType,
                                   numBuckets, bucketData.data(),
                                   bucketDataSize, msg);
    return buf;
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvU8)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvU8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvS8)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvS8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvU16)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvU16, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvS16)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvS16, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvU32)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvU32, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvS32)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvS32, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvU64)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvU64, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvS64)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvS64, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_NvS24_8)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvS24_8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_DefaultUnknown)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeHistogramDataResp(NvU8, 3);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Patch data type to invalid value
    auto* resp = reinterpret_cast<nsm_get_histogram_data_resp*>(
        buf.data() + sizeof(nsm_msg_hdr));
    resp->bucket_data_type = 0xFF;
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetHistogramData_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto [rc, requestMsg] = commands[5]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(NsmDiscoveryBranch, GetHistogramData_ErrorPath)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetHistogramData", {"-i", "0", "-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- GetDeviceCapabilitiesV2 timestamp switch cases -------------------------

TEST(NsmDiscoveryBranch, GetDeviceCapabilitiesV2_TimestampNone)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    // Buffer must be large enough for TLV payload: common_telemetry_resp +
    // aggregate_tag for uint8 (3 bytes) + aggregate_tag for uint32 (6 bytes)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_get_device_capabilities_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_NONE, 4096, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetDeviceCapabilitiesV2_TimestampEpoch)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_get_device_capabilities_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_EPOCH_TIME, 4096, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetDeviceCapabilitiesV2_TimestampMonotonic)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_get_device_capabilities_v2_resp(
        0, NSM_SUCCESS, ERR_NULL,
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_MONOTONIC_TIME, 4096, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetDeviceCapabilitiesV2_TimestampUnknown)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_get_device_capabilities_v2_resp(0, NSM_SUCCESS, ERR_NULL, 0xFE,
                                               4096, msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetDeviceCapabilitiesV2_ErrorPath)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetDeviceCapabilitiesV2_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetDeviceCapabilitiesV2");

    auto [rc, requestMsg] = commands[6]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- GetGpioState success paths ---------------------------------------------

TEST(NsmDiscoveryBranch, GetGpioState_SuccessMultipleGpios)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetGpioState", {"-o", "0", "-l", "16"});

    // 2 bytes = 16 GPIO bits, alternating HIGH/LOW pattern
    uint8_t gpioValues[2] = {0xAA, 0x55};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_gpio_state_resp) - 1 +
                                 sizeof(gpioValues),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpio_state_resp(0, NSM_SUCCESS, ERR_NULL, 0, 16, gpioValues,
                               sizeof(gpioValues), msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetGpioState_SuccessPartialByte)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetGpioState", {"-o", "4", "-l", "3"});

    // 1 byte, only 3 GPIOs valid out of 8 bits
    uint8_t gpioValues[1] = {0x07}; // bits 0,1,2 set = all HIGH
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_gpio_state_resp) - 1 +
                                 sizeof(gpioValues),
                             0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpio_state_resp(0, NSM_SUCCESS, ERR_NULL, 4, 3, gpioValues,
                               sizeof(gpioValues), msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetGpioState_ErrorPath)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetGpioState", {"-o", "0", "-l", "8"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryBranch, GetGpioState_CreateRequestMsg)
{
    CLI::App app;
    setupDiscoveryCommands(app);
    parseSubcmdArgs(app, "GetGpioState", {"-o", "0", "-l", "8"});

    auto [rc, requestMsg] = commands[7]->createRequestMsg();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---- registerCommand coverage -----------------------------------------------

TEST(NsmDiscoveryBranch, RegisterCommand_AllCommandsCreated)
{
    CLI::App app;
    setupDiscoveryCommands(app);

    // Verify all 9 commands are registered
    EXPECT_EQ(commands.size(), 9u);
}

} // namespace nsmtool::discovery
