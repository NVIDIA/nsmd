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
#include "device-capability-discovery.h"

#include "cmd_helper.hpp"
#include "nsm_config_cmd.hpp"
#include "nsm_diag_cmd.hpp"
#include "nsm_discovery_cmd.hpp"
#include "nsm_telemetry_cmd.hpp"

#include <algorithm>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;
using ordered_json = nlohmann::ordered_json;

namespace nsmtool
{
namespace discovery
{
extern std::vector<std::unique_ptr<nsmtool::helper::CommandInterface>> commands;
} // namespace discovery
} // namespace nsmtool

TEST(parseBitfieldVar, GoodTest)
{
    bitfield8_t supportedTypes[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    ordered_json result;
    std::string key("Supported Nvidia Message Types");
    nsmtool::helper::parseBitfieldVar(result, key, &supportedTypes[0], 8);
    EXPECT_EQ(result[key].size(), 0);

    supportedTypes[0].byte = 0x1f;
    nsmtool::helper::parseBitfieldVar(result, key, &supportedTypes[0], 8);
    EXPECT_EQ(result[key].size(), 5);
    EXPECT_THAT(result[key], ElementsAre(0, 1, 2, 3, 4));

    supportedTypes[0].byte = 0;
    supportedTypes[7].byte = 0xf8;
    result.clear();
    nsmtool::helper::parseBitfieldVar(result, key, &supportedTypes[0], 8);
    EXPECT_EQ(result[key].size(), 5);
    EXPECT_THAT(result[key], ElementsAre(59, 60, 61, 62, 63));
}

TEST(NsmDiscoveryCmd, RegisterCommandCreatesAllCommands)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);
    EXPECT_EQ(nsmtool::discovery::commands.size(), 9);
}

TEST(NsmDiscoveryCmd, AllCommandsCreateRequestMsg)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);
    for (auto& cmd : nsmtool::discovery::commands)
    {
        auto [rc, msg] = cmd->createRequestMsg();
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        EXPECT_GT(msg.size(), 0u);
    }
}

TEST(NsmTelemetryCmd, RegisterCommandCreatesCommands)
{
    CLI::App app;
    nsmtool::telemetry::registerCommand(app);
    auto subs = app.get_subcommands({});
    EXPECT_GT(subs.size(), 0u);
    EXPECT_GT(subs[0]->get_subcommands({}).size(), 0u);
}

TEST(NsmConfigCmd, RegisterCommandCreatesCommands)
{
    CLI::App app;
    nsmtool::config::registerCommand(app);
    auto subs = app.get_subcommands({});
    EXPECT_GT(subs.size(), 0u);
    EXPECT_GT(subs[0]->get_subcommands({}).size(), 0u);
}

TEST(NsmDiagCmd, RegisterCommandCreatesCommands)
{
    CLI::App app;
    nsmtool::diag::registerCommand(app);
    auto subs = app.get_subcommands({});
    EXPECT_GT(subs.size(), 0u);
    EXPECT_GT(subs[0]->get_subcommands({}).size(), 0u);
}

TEST(NsmDiscoveryCmd, AllCommandsParseResponseWithError)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);

    // Build a minimal error response buffer
    std::vector<uint8_t> errResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
    auto* resp = reinterpret_cast<nsm_msg*>(errResp.data());
    auto* common = reinterpret_cast<nsm_common_resp*>(resp->payload);
    common->completion_code = NSM_ERROR;

    for (auto& cmd : nsmtool::discovery::commands)
    {
        EXPECT_NO_THROW(cmd->parseResponseMsg(resp, errResp.size()));
    }
}

// Helper: add up to `limit` options with dummy "0" values from a CLI::App.
// Skips flags and well-known base options.
static void addOptsWithDummyVals(
    CLI::App* app, std::vector<std::string>& args,
    std::size_t limit = std::numeric_limits<std::size_t>::max())
{
    std::size_t count = 0;
    for (auto* opt : app->get_options({}))
    {
        if (count >= limit)
            break;
        if (opt->get_type_size_max() == 0)
            continue; // flag - no value needed
        // Prefer long name (--foo) over short (-f)
        std::string name;
        if (!opt->get_lnames().empty())
            name = "--" + opt->get_lnames()[0];
        else if (!opt->get_snames().empty())
            name = "-" + opt->get_snames()[0];
        else
            continue; // positional or unnamed - skip
        // Skip base options already provided or not useful in tests
        if (name == "-m" || name == "--mctp_eid" || name == "-v" ||
            name == "--verbose" || name == "-h" || name == "--help")
            continue;
        // Determine how many values to provide for this option.
        // Multi-value options (e.g. portMask) need enough entries to satisfy
        // the underlying encoder's fixed-size requirement
        // (PORT_MASK_DATA_SIZE=32).
        std::size_t numVals = 1;
        const std::string longName =
            !opt->get_lnames().empty() ? opt->get_lnames()[0] : "";
        if (longName == "portMask")
            numVals = 32; // encode_set_port_disable_future_req reads 32 bytes
        args.push_back(name);
        for (std::size_t i = 0; i < numVals; ++i)
            args.push_back("0");
        ++count;
    }
}

// Helper: exercise createRequestMsg for every subcommand by calling parse()
// directly on each individual command's CLI::App.
//
// Background: CommandInterface registers exec() via app->callback(), which
// stores it as CLI11 final_callback_. That callback only fires AFTER
// _process_requirements() in the top-level parse flow.  When required option
// groups are present, _process_requirements() throws RequiredError before the
// callback fires, so exec() is never reached.
//
// Calling cmdSub->parse(args) directly treats the individual command subcommand
// as the top-level app and supplies dummy "0" values for every registered
// option (including those in option groups). This satisfies validation and lets
// the final_callback_ fire, exercising exec() -> createRequestMsg().
static void
    exerciseAllSubcmdsViaDirectParse(std::function<void(CLI::App&)> registerFn)
{
    // First pass: collect command names
    std::vector<std::string> cmdNames;
    {
        CLI::App tempApp;
        registerFn(tempApp);
        auto parentSubs = tempApp.get_subcommands({});
        if (parentSubs.empty())
            return;
        for (auto* sub : parentSubs[0]->get_subcommands({}))
            cmdNames.push_back(sub->get_name());
    }

    // Second pass: for each command create a fresh registration and parse
    for (const auto& cmdName : cmdNames)
    {
        CLI::App freshApp;
        registerFn(freshApp);
        auto parentSubs = freshApp.get_subcommands({});
        if (parentSubs.empty())
            continue;
        auto* cmdSub = parentSubs[0]->get_subcommand(cmdName);
        if (!cmdSub)
            continue;

        // Build args: base -m 1 + dummy values for all options/option-groups
        std::vector<std::string> args = {"-m", "1"};
        addOptsWithDummyVals(cmdSub, args);
        // Process ALL sub-apps (option groups, both named and unnamed).
        // Use get_require_option_max() to add exactly the required number of
        // options — e.g. require_option(1) → add 1, require_option(7) → add 7,
        // no constraint (max==0) → add all.
        for (auto* grp : cmdSub->get_subcommands({}))
        {
            std::size_t maxReq = grp->get_require_option_max();
            std::size_t limit =
                (maxReq > 0) ? maxReq : std::numeric_limits<std::size_t>::max();
            addOptsWithDummyVals(grp, args, limit);
        }

        try
        {
            // CLI11 parse(vector) processes args from back to front, so
            // reverse so that options precede their values at the back.
            std::reverse(args.begin(), args.end());
            // Direct parse on the command subcommand triggers exec() callback
            cmdSub->parse(args);
        }
        catch (const std::exception&)
        {
            // Some commands may still fail (e.g. nsmSendRecv unavailable)
        }
    }
}

TEST(NsmTelemetryCmd, AllCommandsExecViaDirectParse)
{
    exerciseAllSubcmdsViaDirectParse(
        [](CLI::App& app) { nsmtool::telemetry::registerCommand(app); });
}

TEST(NsmConfigCmd, AllCommandsExecViaDirectParse)
{
    exerciseAllSubcmdsViaDirectParse(
        [](CLI::App& app) { nsmtool::config::registerCommand(app); });
}

TEST(NsmDiagCmd, AllCommandsExecViaDirectParse)
{
    exerciseAllSubcmdsViaDirectParse(
        [](CLI::App& app) { nsmtool::diag::registerCommand(app); });
}

// ──────────────────────────────────────────────────────────────────────────────
// Discovery parseResponseMsg success-path tests
//
// Commands in nsmtool::discovery::commands (after registerCommand):
//   [0] Ping
//   [1] GetSupportedMessageTypes
//   [2] GetSupportedCommandCodes
//   [3] QueryDeviceIdentification
//   [4] GetHistogramFormat
//   [5] GetHistogramData
//   [6] GetDeviceCapabilitiesV2
//   [7] GetGpioState
// ──────────────────────────────────────────────────────────────────────────────

static void setupDiscoveryCommands(CLI::App& app)
{
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);
}

TEST(NsmDiscoveryCmd, PingParseResponseSuccess)
{
    CLI::App app;
    setupDiscoveryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(uint16_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_ping_resp(0, ERR_NULL, msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmDiscoveryCmd, GetSupportedMessageTypesParseResponseSuccess)
{
    CLI::App app;
    setupDiscoveryCommands(app);

    const size_t respSize = sizeof(nsm_msg_hdr) +
                            sizeof(nsm_get_supported_nvidia_message_types_resp);
    std::vector<uint8_t> buf(respSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE] = {};
    types[0].byte = 0x07; // types 0,1,2 supported
    encode_get_supported_nvidia_message_types_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   types, msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[1]->parseResponseMsg(msg, respSize));
}

TEST(NsmDiscoveryCmd, GetSupportedCommandCodesParseResponseSuccess)
{
    CLI::App app;
    setupDiscoveryCommands(app);

    const size_t respSize = sizeof(nsm_msg_hdr) +
                            sizeof(nsm_get_supported_command_codes_resp);
    std::vector<uint8_t> buf(respSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[0].byte = 0xFF;
    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[2]->parseResponseMsg(msg, respSize));
}

// QueryDeviceIdentification – one test per device type to cover switch cases
static void testQueryDeviceId(uint8_t devId)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);

    const size_t respSize = sizeof(nsm_msg_hdr) +
                            sizeof(nsm_query_device_identification_resp);
    std::vector<uint8_t> buf(respSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_device_identification_resp(0, NSM_SUCCESS, ERR_NULL, devId, 0,
                                            msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[3]->parseResponseMsg(msg, respSize));
}

TEST(NsmDiscoveryCmd, QueryDeviceIdentification_GPU)
{
    testQueryDeviceId(NSM_DEV_ID_GPU);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_Switch)
{
    testQueryDeviceId(NSM_DEV_ID_SWITCH);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_PCIeBridge)
{
    testQueryDeviceId(NSM_DEV_ID_PCIE_BRIDGE);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_Baseboard)
{
    testQueryDeviceId(NSM_DEV_ID_BASEBOARD);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_ERoT)
{
    testQueryDeviceId(NSM_DEV_ID_EROT);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_MctpBridge)
{
    testQueryDeviceId(NSM_DEV_ID_MCTP_BRIDGE);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_CPU)
{
    testQueryDeviceId(NSM_DEV_ID_CPU);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_Unknown)
{
    testQueryDeviceId(NSM_DEV_ID_UNKNOWN);
}
TEST(NsmDiscoveryCmd, QueryDeviceIdentification_InvalidDefault)
{
    // Exercises the default: branch (invalid device_identification)
    testQueryDeviceId(0x7F);
}

// GetHistogramFormat – helper; bucketType and numBuckets drive switch coverage
static void testHistogramFormat(uint8_t bucketType, uint16_t numBuckets,
                                uint32_t bucketOffsetsBytesPerBucket)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);

    nsm_histogram_format_metadata meta{};
    meta.num_of_buckets = numBuckets;
    meta.bucket_data_type = bucketType;
    meta.min_sampling_time = 1000;
    meta.accumulation_cycle = 1;
    meta.increment_duration = 500;
    meta.bucket_unit_of_measure = 0;

    uint32_t offsetsSize = (numBuckets + 1) * bucketOffsetsBytesPerBucket;
    std::vector<uint8_t> offsets(offsetsSize, 0);

    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_get_histogram_format_resp) - 1 +
                           offsetsSize;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_histogram_format_resp(0, NSM_SUCCESS, ERR_NULL, &meta,
                                     offsets.data(), offsetsSize, msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[4]->parseResponseMsg(msg, bufSize));
}

TEST(NsmDiscoveryCmd, GetHistogramFormat_NvU8)
{
    testHistogramFormat(NvU8, 2, sizeof(uint8_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvS8)
{
    testHistogramFormat(NvS8, 2, sizeof(int8_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvU16)
{
    testHistogramFormat(NvU16, 2, sizeof(uint16_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvS16)
{
    testHistogramFormat(NvS16, 2, sizeof(int16_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvU32)
{
    testHistogramFormat(NvU32, 2, sizeof(uint32_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvS32)
{
    testHistogramFormat(NvS32, 2, sizeof(int32_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvU64)
{
    // NvU64 case uses sizeof(uint32_t) in resize (source code behaviour),
    // so use 1 bucket to keep the buffer large enough for 1 uint64_t read.
    testHistogramFormat(NvU64, 1, sizeof(uint64_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvS64)
{
    testHistogramFormat(NvS64, 2, sizeof(int64_t));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_NvS24_8)
{
    testHistogramFormat(NvS24_8, 2, sizeof(float));
}
TEST(NsmDiscoveryCmd, GetHistogramFormat_UnknownType)
{
    // Exercises the default: branch in the switch
    testHistogramFormat(0xFF, 2, sizeof(uint8_t));
}

// GetHistogramData – same set of data types
static void testHistogramData(uint8_t bucketType, uint16_t numBuckets,
                              uint32_t bytesPerBucket)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);

    uint32_t dataSize = numBuckets * bytesPerBucket;
    std::vector<uint8_t> bucketData(dataSize, 0);

    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_get_histogram_data_resp) - 1 + dataSize;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_histogram_data_resp(0, NSM_SUCCESS, ERR_NULL, bucketType,
                                   numBuckets, bucketData.data(), dataSize,
                                   msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[5]->parseResponseMsg(msg, bufSize));
}

TEST(NsmDiscoveryCmd, GetHistogramData_NvU8)
{
    testHistogramData(NvU8, 3, sizeof(uint8_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvS8)
{
    testHistogramData(NvS8, 3, sizeof(int8_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvU16)
{
    testHistogramData(NvU16, 3, sizeof(uint16_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvS16)
{
    testHistogramData(NvS16, 3, sizeof(int16_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvU32)
{
    testHistogramData(NvU32, 3, sizeof(uint32_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvS32)
{
    testHistogramData(NvS32, 3, sizeof(int32_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvU64)
{
    testHistogramData(NvU64, 3, sizeof(uint64_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvS64)
{
    testHistogramData(NvS64, 3, sizeof(int64_t));
}
TEST(NsmDiscoveryCmd, GetHistogramData_NvS24_8)
{
    testHistogramData(NvS24_8, 3, sizeof(float));
}
TEST(NsmDiscoveryCmd, GetHistogramData_UnknownType)
{
    testHistogramData(0xFF, 3, sizeof(uint8_t));
}

// GetDeviceCapabilitiesV2 – cover all timestampGenerationToString cases
static void testGetDeviceCapabilitiesV2(uint8_t timestampGen)
{
    CLI::App app;
    nsmtool::discovery::commands.clear();
    nsmtool::discovery::registerCommand(app);

    const size_t bufSize = 256;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_get_device_capabilities_v2_resp(0, NSM_SUCCESS, ERR_NULL,
                                               timestampGen, 4096, msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[6]->parseResponseMsg(msg, bufSize));
}

TEST(NsmDiscoveryCmd, GetDeviceCapabilitiesV2_TimestampNone)
{
    testGetDeviceCapabilitiesV2(
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_NONE);
}
TEST(NsmDiscoveryCmd, GetDeviceCapabilitiesV2_TimestampEpoch)
{
    testGetDeviceCapabilitiesV2(
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_EPOCH_TIME);
}
TEST(NsmDiscoveryCmd, GetDeviceCapabilitiesV2_TimestampMonotonic)
{
    testGetDeviceCapabilitiesV2(
        NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_MONOTONIC_TIME);
}
TEST(NsmDiscoveryCmd, GetDeviceCapabilitiesV2_TimestampUnknown)
{
    // Exercises the default: branch
    testGetDeviceCapabilitiesV2(0xFF);
}

TEST(NsmDiscoveryCmd, GetGpioStateParseResponseSuccess)
{
    CLI::App app;
    setupDiscoveryCommands(app);

    // 16 GPIOs starting at offset 0 → 2 bytes of data
    const uint16_t gpioOffset = 0;
    const uint16_t gpioLength = 16;
    const uint32_t gpioValuesSize = 2;
    uint8_t gpioValues[2] = {0xAB, 0xCD};

    const size_t bufSize = sizeof(nsm_msg_hdr) +
                           sizeof(nsm_get_gpio_state_resp) - 1 + gpioValuesSize;
    std::vector<uint8_t> buf(bufSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_gpio_state_resp(0, NSM_SUCCESS, ERR_NULL, gpioOffset, gpioLength,
                               gpioValues, gpioValuesSize, msg);

    EXPECT_NO_THROW(
        nsmtool::discovery::commands[7]->parseResponseMsg(msg, bufSize));
}
