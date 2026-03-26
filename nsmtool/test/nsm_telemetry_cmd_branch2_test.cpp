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
 * Branch coverage tests for nsm_telemetry_cmd.cpp — Part 2
 *
 * Targets SUCCESS paths with real field values to cover the "if (rc == SUCCESS
 * && cc == SUCCESS)" branches and all the field-printing code inside
 * parseResponseMsg / printInventoryInfo / printPortTeleInfo / etc.
 *
 * Also covers:
 *  - QueryScalarGroupTelemetry groups 0-10 with populated data
 *  - QueryAvailableAndClearableScalarGroup groups 2-4,8,9 with all bits
 *  - QueryVectorGroupTelemetry group 1 with data
 *  - GetInventoryInformation all property-type branches
 *  - Port counter individual bit tests (each counter bit set individually)
 *  - GetClockLimit SpeedLocked vs not locked
 *  - Scalar group 1 clockMode / encodedSize lambda branches
 *  - Scalar group 7 portType lambda branches
 *  - SetPowerLimit / GetPowerLimit createRequestMsg DEVICE/MODULE/other
 *  - GetLeakDetectionInfo with sensors
 *  - GetSupportedGPMMetrics with bitmask
 *  - Remaining commands success paths
 */

#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "../nsm_telemetry_cmd.cpp"

#include <gtest/gtest.h>

namespace nsmtool::telemetry
{

// ---- Helpers ----------------------------------------------------------------

static void setupTelemetryCommands(CLI::App& app)
{
    commands.clear();
    registerCommand(app);
}

static void parseSubcmdArgs(CLI::App& app, const std::string& cmdName,
                            std::vector<std::string> extraArgs = {})
{
    auto* telSub = app.get_subcommand("telemetry");
    if (!telSub)
        return;
    auto* leafSub = telSub->get_subcommand(cmdName);
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

// ============================================================================
// QueryScalarGroupTelemetry success paths with populated data (groups 0-10)
// ============================================================================

TEST(NsmTelBranch2, ScalarGroup0_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "0"});

    struct nsm_query_scalar_group_telemetry_group_0 data{};
    data.pci_vendor_id = 0x10DE;
    data.pci_device_id = 0x2204;
    data.pci_subsystem_vendor_id = 0x10DE;
    data.pci_subsystem_device_id = 0x1234;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_WithData_SeparateClock)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.negotiated_link_speed = 4;
    data.negotiated_link_width = 16;
    data.target_link_speed = 5;
    data.max_link_speed = 5;
    data.max_link_width = 16;
    data.max_read_request_size_bytes = 0; // 128B
    data.max_payload_size_bytes = 1;      // 256B
    data.clock_mode = NSM_PCIE_CLOCK_MODE_SEPARATE;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_WithData_CommonClock)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.clock_mode = NSM_PCIE_CLOCK_MODE_COMMON;
    data.max_read_request_size_bytes = 5; // 4096B
    data.max_payload_size_bytes = 5;      // 4096B
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_UnknownClockMode)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.clock_mode = 99;                 // Unknown
    data.max_read_request_size_bytes = 6; // out of range -> 0
    data.max_payload_size_bytes = 6;      // out of range -> 0
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup2_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "2"});

    struct nsm_query_scalar_group_telemetry_group_2 data{};
    data.non_fatal_errors = 10;
    data.fatal_errors = 1;
    data.unsupported_request_count = 5;
    data.correctable_errors = 20;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group2_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup3_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "3"});

    struct nsm_query_scalar_group_telemetry_group_3 data{};
    data.L0ToRecoveryCount = 42;
    data.eieos_timeout = 3;
    data.training_seq_errors = 7;
    data.framing_errors = 2;
    data.link_down_count = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group3_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup4_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "4"});

    struct nsm_query_scalar_group_telemetry_group_4 data{};
    data.recv_err_cnt = 100;
    data.NAK_recv_cnt = 50;
    data.NAK_sent_cnt = 30;
    data.bad_TLP_cnt = 10;
    data.replay_rollover_cnt = 5;
    data.FC_timeout_err_cnt = 2;
    data.replay_cnt = 8;
    data.dllp_crc_errors = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group4_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup5_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "5"});

    struct nsm_query_scalar_group_telemetry_group_5 data{};
    data.PCIeTXDwords = 123456;
    data.PCIeRXDwords = 789012;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group5_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup6_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "6"});

    struct nsm_query_scalar_group_telemetry_group_6 data{};
    data.invalid_flit_counter = 77;
    data.ltssm_state = 3;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group6_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup7_Endpoint)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_ENDPOINT;
    data.pcie_bus_number = 5;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup7_RootPort)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_ROOT_PORT;
    data.pcie_bus_number = 10;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup7_Upstream)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_UPSTREAM;
    data.pcie_bus_number = 15;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup7_Downstream)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_DOWNSTREAM;
    data.pcie_bus_number = 20;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup7_UnknownPortType)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = 99; // Unknown
    data.pcie_bus_number = 25;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup8_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "8"});

    struct nsm_query_scalar_group_telemetry_group_8 data{};
    for (int i = 0; i < TOTAL_PCIE_LANE_COUNT; i++)
    {
        data.error_counts[i] = i * 10;
    }
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group8_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup9_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "9"});

    struct nsm_query_scalar_group_telemetry_group_9 data{};
    data.aer_uncorrectable_error_status = 0x00100000;
    data.aer_correctable_error_status = 0x00000040;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group9_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup10_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "10"});

    struct nsm_query_scalar_group_telemetry_group_10 data{};
    data.outbound_read_tlp_count = 1000;
    data.dwords_transferred_in_outbound_read_tlp_low = 0xDEADBEEF;
    data.dwords_transferred_in_outbound_read_tlp_high = 0x01;
    data.outbound_write_tlp_count = 2000;
    data.dwords_transferred_in_outbound_write_tlp_low = 0xCAFEBABE;
    data.dwords_transferred_in_outbound_write_tlp_high = 0x02;
    data.outbound_completion_tlp_count = 3000;
    data.dwords_transferred_in_outbound_completion = 4000;
    data.read_requests_dropped_tag_unavailable = 5;
    data.read_requests_dropped_credit_exhaustion = 3;
    data.read_requests_dropped_credit_not_posted = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group10_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// QueryScalarGroupTelemetry ERROR paths per group (rc != 0 || cc != 0)
// ============================================================================

TEST(NsmTelBranch2, ScalarGroup0_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup2_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "2"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup3_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "3"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup4_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "4"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup5_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "5"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup6_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "6"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup7_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup8_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "8"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup9_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "9"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup10_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "10"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// QueryAvailableAndClearableScalarGroup with ALL bits set
// ============================================================================

TEST(NsmTelBranch2, AvailClearable_Group2_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "2"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0xFF; // all bits set
    clearable[0].byte = 0xFF;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(available),
        reinterpret_cast<uint8_t*>(clearable), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group3_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "3"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0xFF;
    clearable[0].byte = 0xFF;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(available),
        reinterpret_cast<uint8_t*>(clearable), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group4_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "4"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0xFF;
    clearable[0].byte = 0xFF;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(available),
        reinterpret_cast<uint8_t*>(clearable), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group8_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "8"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0xFF;
    clearable[0].byte = 0xFF;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(available),
        reinterpret_cast<uint8_t*>(clearable), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group9_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "9"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0xFF;
    clearable[0].byte = 0xFF;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(available),
        reinterpret_cast<uint8_t*>(clearable), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// QueryAvailableAndClearableScalarGroup ERROR paths per group
// ============================================================================

TEST(NsmTelBranch2, AvailClearable_Group2_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "2"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group3_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "3"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group4_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "4"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group8_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "8"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, AvailClearable_Group9_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "9"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// QueryVectorGroupTelemetry success and error paths
// ============================================================================

TEST(NsmTelBranch2, VectorGroup1_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "1", "-s", "3", "-l", "5"});

    nsm_query_vector_group_1_data data{};
    data.cdr_error_per_lane = 1234;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_vector_data_sources_v2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_vector_group_telemetry_v2_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, VectorGroup1_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "1", "-s", "1", "-l", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetInventoryInformation — all property type branches
// ============================================================================

TEST(NsmTelBranch2, Inventory_BoardPartNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(BOARD_PART_NUMBER)});

    const char* val = "GPU-A100-80GB";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_SerialNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(SERIAL_NUMBER)});

    const char* val = "SN123456";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MarketingName)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MARKETING_NAME)});

    const char* val = "NVIDIA A100";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_ProductLength)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(PRODUCT_LENGTH)});

    uint32_t val = htole32(267);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_ProductWidth)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(PRODUCT_WIDTH)});

    uint32_t val = htole32(111);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_ProductHeight)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(PRODUCT_HEIGHT)});

    uint32_t val = htole32(40);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_RatedDevicePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(RATED_DEVICE_POWER_LIMIT)});

    uint32_t val = htole32(300000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MinDevicePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MINIMUM_DEVICE_POWER_LIMIT)});

    uint32_t val = htole32(100000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MaxDevicePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_DEVICE_POWER_LIMIT)});

    uint32_t val = htole32(400000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MinModulePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MINIMUM_MODULE_POWER_LIMIT)});

    uint32_t val = htole32(150000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MaxModulePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_MODULE_POWER_LIMIT)});

    uint32_t val = htole32(500000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_RatedModulePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(RATED_MODULE_POWER_LIMIT)});

    uint32_t val = htole32(350000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_RatedGpuBasePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(RATED_GPU_BASE_POWER_LIMIT)});

    uint32_t val = htole32(250000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MinGpuBasePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MINIMUM_GPU_BASE_POWER_LIMIT)});

    uint32_t val = htole32(100000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MaxGpuBasePowerLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_GPU_BASE_POWER_LIMIT)});

    uint32_t val = htole32(400000);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_DefaultBoostClocks)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(DEFAULT_BOOST_CLOCKS)});

    uint32_t val = htole32(1800);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_DefaultBaseClocks)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(DEFAULT_BASE_CLOCKS)});

    uint32_t val = htole32(1200);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_TraySlotNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(TRAY_SLOT_NUMBER)});

    uint32_t val = htole32(3);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_TraySlotIndex)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(TRAY_SLOT_INDEX)});

    uint32_t val = htole32(7);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_GpuModuleId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(GPU_MODULE_ID)});

    uint32_t val = htole32(2);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MinMemoryClockLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MINIMUM_MEMORY_CLOCK_LIMIT)});

    uint32_t val = htole32(800);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MaxMemoryClockLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_MEMORY_CLOCK_LIMIT)});

    uint32_t val = htole32(1600);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MinGraphicsClockLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MINIMUM_GRAPHICS_CLOCK_LIMIT)});

    uint32_t val = htole32(210);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MaxGraphicsClockLimit)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_GRAPHICS_CLOCK_LIMIT)});

    uint32_t val = htole32(2100);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MaxEdppScalingFactor)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_EDPP_SCALING_FACTOR)});

    uint8_t val = 100;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint8_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          sizeof(uint8_t), &val, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_DevicePartNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(DEVICE_PART_NUMBER)});

    const char* val = "900-21001-0020";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_FruPartNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(FRU_PART_NUMBER)});

    const char* val = "699-21001-0200";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MemoryVendor)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MEMORY_VENDOR)});

    const char* val = "Samsung";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_MemoryPartNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MEMORY_PART_NUMBER)});

    const char* val = "K4A8G085WC-BCTD";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_BuildDate)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(BUILD_DATE)});

    const char* val = "2024-01-15";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_FirmwareVersion)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(FIRMWARE_VERSION)});

    const char* val = "96.00.7e.00.00";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_InfoRomVersion)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(INFO_ROM_VERSION)});

    const char* val = "G001.0000.00.03";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_FpgaFirmwareVersion)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(FPGA_FIRMWARE_VERSION)});

    const char* val = "1.2.3";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_AssetTag)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(ASSET_TAG)});

    const char* val = "ASSET-001";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_PcieRetimer1EepromVersion)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(PCIERETIMER_1_EEPROM_VERSION)});

    uint8_t data[8] = {2, 0, 5, 0, 0, 0, 10, 0};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) + 8);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, 8, data,
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, Inventory_DefaultPropertyId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // Use a property ID that doesn't match any case -> default
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "250"});

    const char* val = "test";
    uint16_t dataSize = static_cast<uint16_t>(strlen(val));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<const uint8_t*>(val),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// Port counter — individual counter bits
// ============================================================================

TEST(NsmTelBranch2, PortCounter_NoCounterBits)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    struct nsm_port_counter_data portData{};
    // All bits 0 - no counters printed
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, PortCounter_NullPortData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    // Construct a valid response and parse it
    struct nsm_port_counter_data portData{};
    memset(&portData.supported_counter, 0xFF,
           sizeof(portData.supported_counter));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// SetPowerLimit — createRequestMsg DEVICE / MODULE / other
// ============================================================================

TEST(NsmTelBranch2, SetPowerLimit_Device)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPowerLimit",
                    {"-i", "0", "-a", "0", "-p", "0", "-l", "100000"});

    auto [rc, reqMsg] = commands[34]->createRequestMsg();
    EXPECT_EQ(rc, 0);
}

TEST(NsmTelBranch2, SetPowerLimit_Module)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPowerLimit",
                    {"-i", "1", "-a", "0", "-p", "0", "-l", "100000"});

    auto [rc, reqMsg] = commands[34]->createRequestMsg();
    EXPECT_EQ(rc, 0);
}

TEST(NsmTelBranch2, SetPowerLimit_Other)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPowerLimit",
                    {"-i", "5", "-a", "0", "-p", "0", "-l", "100000"});

    auto [rc, reqMsg] = commands[34]->createRequestMsg();
    EXPECT_EQ(rc, 0);
}

// ============================================================================
// GetPowerLimit — createRequestMsg DEVICE / MODULE / other
// ============================================================================

TEST(NsmTelBranch2, GetPowerLimit_Device)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPowerLimit", {"-i", "0"});

    auto [rc, reqMsg] = commands[35]->createRequestMsg();
    EXPECT_EQ(rc, 0);
}

TEST(NsmTelBranch2, GetPowerLimit_Module)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPowerLimit", {"-i", "1"});

    auto [rc, reqMsg] = commands[35]->createRequestMsg();
    EXPECT_EQ(rc, 0);
}

TEST(NsmTelBranch2, GetPowerLimit_Other)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPowerLimit", {"-i", "5"});

    auto [rc, reqMsg] = commands[35]->createRequestMsg();
    EXPECT_EQ(rc, 0);
}

// ============================================================================
// Success paths for remaining commands
// ============================================================================

TEST(NsmTelBranch2, PcieFundamentalReset_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "PcieFundamentalReset", {"-d", "0", "-a", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[31]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ClearScalarDataSource_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "ClearScalarDataSource",
                    {"-d", "0", "-g", "2", "-i", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_clear_data_source_v1_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[32]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, SetPowerLimit_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPowerLimit",
                    {"-i", "0", "-a", "0", "-p", "0", "-l", "100000"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[34]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetPowerLimit_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPowerLimit", {"-i", "0"});

    uint32_t p = 300000, o = 400000, e = 300000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_limit_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, p, o, e, msg);
    EXPECT_NO_THROW(commands[35]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetCurrClockFreq_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrClockFreq", {"-c", "0"});

    uint32_t clockFreq = 1800;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_curr_clock_freq_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL, &clockFreq, msg);
    EXPECT_NO_THROW(commands[36]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetAccumGpuUtilTime_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    uint32_t ctx = 99999, sm = 88888;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_accum_GPU_util_time_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_accum_GPU_util_time_resp(0, NSM_SUCCESS, ERR_NULL, &ctx, &sm,
                                        msg);
    EXPECT_NO_THROW(commands[37]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetRowRemappingCounts_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    uint32_t corr = 10, uncorr = 2;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remapping_counts_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, ERR_NULL, corr, uncorr,
                                         msg);
    EXPECT_NO_THROW(commands[40]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetRowRemapAvailability_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_row_remap_availability data{};
    data.no_remapping = 0;
    data.low_remapping = 10;
    data.partial_remapping = 50;
    data.high_remapping = 80;
    data.max_remapping = 100;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_availability_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                           msg);
    EXPECT_NO_THROW(commands[41]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, SetLeakDetectionThresholds_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetLeakDetectionThresholds",
                    {"-s", "0", "-n", "1", "-t", "200"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[43]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetMemoryCapacityUtil_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_memory_capacity_utilization data{};
    data.used_memory = 32768;
    data.reserved_memory = 4096;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_memory_capacity_util_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);
    EXPECT_NO_THROW(commands[44]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetCurrentUtilization_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    nsm_get_current_utilization_data data{};
    data.gpu_utilization = 95;
    data.memory_utilization = 80;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_utilization_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_utilization_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);
    EXPECT_NO_THROW(commands[45]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetSupportedGPMMetrics_WithBitmask)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetSupportedGPMMetrics", {"-t", "0"});

    uint8_t bitmask[2] = {0xFF, 0x03}; // bits 0-9 set
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_supported_gpm_metrics_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_gpm_metrics_resp(0, NSM_SUCCESS, ERR_NULL, 2, 8,
                                          bitmask, msg);
    EXPECT_NO_THROW(commands[47]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetViolationDuration_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_violation_duration vd{};
    vd.supported_counter.byte = 0xFF;
    vd.hw_violation_duration = 100;
    vd.global_sw_violation_duration = 200;
    vd.power_violation_duration = 300;
    vd.thermal_violation_duration = 400;
    vd.counter4 = 500;
    vd.counter5 = 600;
    vd.counter6 = 700;
    vd.counter7 = 800;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_violation_duration_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_violation_duration_resp(0, NSM_SUCCESS, ERR_NULL, &vd, msg);
    EXPECT_NO_THROW(commands[51]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetListAvailablePciePorts_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_list_available_pcie_ports_info info{};
    info.ports_count = 0;
    std::vector<uint8_t> buf(NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_list_available_pcie_ports_resp(0, NSM_SUCCESS, ERR_NULL, &info, msg);
    EXPECT_NO_THROW(commands[52]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, SetPCIePortConfig_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0", "-c", "0", "-d", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_port_config_aggregate_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[54]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// Error paths for ViolationDuration, ListAvailablePciePorts, SetPCIePortConfig
// ============================================================================

TEST(NsmTelBranch2, GetViolationDuration_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[51]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetListAvailablePciePorts_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[52]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, SetPCIePortConfig_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0", "-c", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[54]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetEccErrorCounts success
// ============================================================================

TEST(NsmTelBranch2, GetEccErrorCounts_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_ECC_error_counts errorCounts{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_error_counts_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL, &errorCounts,
                                     msg);
    EXPECT_NO_THROW(commands[24]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetDriverInfo success with driver state and version
// ============================================================================

TEST(NsmTelBranch2, GetDriverInfo_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);

    const char* version = "550.54.15";
    uint16_t dataSize = 1 + static_cast<uint16_t>(strlen(version) + 1);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_driver_info_resp) +
                             strlen(version));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    std::vector<uint8_t> driverInfoData(dataSize, 0);
    driverInfoData[0] = static_cast<uint8_t>(DriverStateEnum::DriverLoaded);
    memcpy(driverInfoData.data() + 1, version, strlen(version));
    encode_get_driver_info_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                driverInfoData.data(), msg);
    EXPECT_NO_THROW(commands[19]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetMigMode — disabled (bit0=0)
// ============================================================================

TEST(NsmTelBranch2, GetMigMode_Disabled)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_MIG_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_MIG_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetEccMode — various bit combinations
// ============================================================================

TEST(NsmTelBranch2, GetEccMode_Bit0Only)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetEccMode_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetRowRemapState — individual bit combinations
// ============================================================================

TEST(NsmTelBranch2, GetRowRemapState_Bit0Only)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, GetRowRemapState_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetProcessorThrottleReason — individual bit tests
// ============================================================================

TEST(NsmTelBranch2, ThrottleReason_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield32_t flags{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ThrottleReason_Bit0Only)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield32_t flags{};
    flags.bits.bit0 = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ThrottleReason_Bit2Only)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield32_t flags{};
    flags.bits.bit2 = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ThrottleReason_Bit4Only)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield32_t flags{};
    flags.bits.bit4 = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// GetClockOutputEnableState — PCIE index with data
// ============================================================================

TEST(NsmTelBranch2, ClockOutputEnableState_PCIE)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", std::to_string(PCIE_CLKBUF_INDEX)});

    uint32_t data = 0xFFFFFFFF; // All bits set
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, data,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// Scalar group 1 — encodedSizeToBytes lambda all branches
// ============================================================================

TEST(NsmTelBranch2, ScalarGroup1_EncodedSize0)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.max_read_request_size_bytes = 0; // 128
    data.max_payload_size_bytes = 0;      // 128
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_EncodedSize1)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.max_read_request_size_bytes = 1; // 256
    data.max_payload_size_bytes = 1;      // 256
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_EncodedSize2)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.max_read_request_size_bytes = 2; // 512
    data.max_payload_size_bytes = 2;      // 512
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_EncodedSize3)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.max_read_request_size_bytes = 3; // 1024
    data.max_payload_size_bytes = 3;      // 1024
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelBranch2, ScalarGroup1_EncodedSize4)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.max_read_request_size_bytes = 4; // 2048
    data.max_payload_size_bytes = 4;      // 2048
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ============================================================================
// QueryMultiportScalarGroupTelemetry
// ============================================================================

TEST(NsmTelBranch2, MultiportScalarGroup0_WithData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryMultiportScalarGroupTelemetry",
                    {"-d", "0", "-g", "0", "-t", "0", "-u", "0", "-i", "0"});

    struct nsm_query_scalar_group_telemetry_group_0 data{};
    data.pci_vendor_id = 0x10DE;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[55]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::telemetry
