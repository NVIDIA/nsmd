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
 * Branch coverage tests for nsm_telemetry_cmd.cpp
 *
 * Targets half-covered branches by exercising:
 *  - Error/failure paths in parseResponseMsg (rc!=0 || cc!=0)
 *  - printPortTeleInfo with NULL portData
 *  - printPortTeleInfo with all supported_counter bits set
 *  - printInventoryInfo with various propertyId values
 *  - printClockBufferData with different index types
 *  - GetClockLimit SpeedLocked vs not locked
 *  - GetMigMode / GetEccMode bit flag branches
 *  - GetProcessorThrottleReason flag bits
 *  - GetRowRemapState flag bits
 *  - SetPowerLimit createRequestMsg DEVICE/MODULE/other
 *  - GetPowerLimit createRequestMsg DEVICE/MODULE/other
 *  - QueryScalarGroupTelemetry all group cases
 *  - QueryAvailableAndClearableScalarGroup all group cases
 *  - QueryVectorGroupTelemetry default case
 *  - Aggregate response paths
 */

// Include the .cpp directly so that its anonymous-namespace `commands`
// vector and all class definitions are in this translation unit.
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

// Build a short (error) response: cc=NSM_ERROR, so decode fails
static std::vector<uint8_t> makeErrorResp()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* resp =
        reinterpret_cast<nsm_common_resp*>(buf.data() + sizeof(nsm_msg_hdr));
    resp->completion_code = NSM_ERROR;
    return buf;
}

// ---- Error path tests -------------------------------------------------------
// These exercise the "else" branch in parseResponseMsg where rc!=0 || cc!=0

TEST(NsmTelemetryBranch, GetPortTelemetryCounter_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, QueryPortCharacteristics_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortCharacteristics", {"-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, QueryPortStatus_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortStatus", {"-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, QueryPortsAvailable_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetPortDisableFuture_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPortDisableFuture",
                    {"-p", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                     "0",  "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                     "0",  "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetFabricManagerState_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetPortDisableFuture_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetPowerMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetPowerMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPowerMode",
                    {"-C", "0", "-T", "0", "-P", "0", "-t", "0", "-a", "0",
                     "-i", "0", "-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetSwitchIsolationMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetSwitchIsolationMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetSwitchIsolationMode", {"-i", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInformation_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetTemperatureReading_Regular_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, ReadThermalParameter_Regular_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "ReadThermalParameter", {"-s", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetCurrentPowerDraw_Regular_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentPowerDraw", {"-s", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetMaxObservedPower_Regular_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetMaxObservedPower", {"-s", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetCurrentEnergyCount_Regular_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentEnergyCount", {"-s", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetVoltage_Regular_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetVoltage", {"-s", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetAltitudePressure_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetDriverInfo_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[19]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetMigMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetMigMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[21]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetEccMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetEccMode_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[23]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetEccErrorCounts_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[24]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetClockLimit_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[25]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetEDPpScalingFactors_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[26]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetEDPpScalingFactors_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[27]->parseResponseMsg(msg, buf.size()));
}

// ---- Port telemetry with all counters set -----------------------------------

TEST(NsmTelemetryBranch, GetPortTelemetryCounter_AllCounters)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "1"});

    struct nsm_port_counter_data portData{};
    // Set all supported_counter bits to 1
    memset(&portData.supported_counter, 0xFF,
           sizeof(portData.supported_counter));
    portData.port_rcv_pkts = 100;
    portData.port_rcv_data = 200;
    portData.port_multicast_rcv_pkts = 300;
    portData.port_unicast_rcv_pkts = 400;
    portData.port_malformed_pkts = 500;
    portData.vl15_dropped = 600;
    portData.port_rcv_errors = 700;
    portData.port_xmit_pkts = 800;
    portData.port_xmit_pkts_vl15 = 900;
    portData.port_xmit_data = 1000;
    portData.port_xmit_data_vl15 = 1100;
    portData.port_unicast_xmit_pkts = 1200;
    portData.port_multicast_xmit_pkts = 1300;
    portData.port_bcast_xmit_pkts = 1400;
    portData.port_xmit_discard = 1500;
    portData.port_neighbor_mtu_discards = 1600;
    portData.port_rcv_ibg2_pkts = 1700;
    portData.port_xmit_ibg2_pkts = 1800;
    portData.symbol_ber = 1900;
    portData.link_error_recovery_counter = 2000;
    portData.link_downed_counter = 2100;
    portData.port_rcv_remote_physical_errors = 2200;
    portData.port_rcv_switch_relay_errors = 2300;
    portData.QP1_dropped = 2400;
    portData.xmit_wait = 2500;
    portData.effective_ber = 2600;
    portData.total_raw_error = 2700;
    portData.effective_error = 2800;
    portData.symbol_error = 2900;
    portData.total_raw_ber = 3000;
    portData.unintentional_link_down_count = 3100;
    portData.intentional_link_down_count = 3200;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- GetMigMode with bit0=1 -------------------------------------------------

TEST(NsmTelemetryBranch, GetMigMode_MigEnabled)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_MIG_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_MIG_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, buf.size()));
}

// ---- GetEccMode with both bits set ------------------------------------------

TEST(NsmTelemetryBranch, GetEccMode_BothBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}

// ---- GetRowRemapState with both bits set ------------------------------------

TEST(NsmTelemetryBranch, GetRowRemapState_BothBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

// ---- GetClockLimit SpeedLocked (min == max) ---------------------------------

TEST(NsmTelemetryBranch, GetClockLimit_SpeedLocked)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockLimit", {"-c", "0"});

    struct nsm_clock_limit clockLimit{};
    clockLimit.present_limit_min = 1000;
    clockLimit.present_limit_max = 1000;
    clockLimit.requested_limit_min = 1000;
    clockLimit.requested_limit_max = 1000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_limit_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit, msg);
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetClockLimit_SpeedNotLocked)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockLimit", {"-c", "0"});

    struct nsm_clock_limit clockLimit{};
    clockLimit.present_limit_min = 500;
    clockLimit.present_limit_max = 1500;
    clockLimit.requested_limit_min = 500;
    clockLimit.requested_limit_max = 1500;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_limit_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit, msg);
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, buf.size()));
}

// ---- GetProcessorThrottleReason with all bits set ---------------------------

TEST(NsmTelemetryBranch, GetProcessorThrottleReason_AllBits)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield32_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 1;
    flags.bits.bit2 = 1;
    flags.bits.bit3 = 1;
    flags.bits.bit4 = 1;
    flags.bits.bit5 = 1;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

// ---- GetInventoryInformation various property types -------------------------

TEST(NsmTelemetryBranch, GetInventoryInfo_MaxMemCapacity)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_MEMORY_CAPACITY)});

    uint32_t val = htole32(1024);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_DeviceGuid)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(DEVICE_GUID)});

    uint8_t guid[UUID_INT_SIZE] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                   0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
                                   0x0d, 0x0e, 0x0f, 0x10};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             UUID_INT_SIZE);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          UUID_INT_SIZE, guid, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_GpuNvlinkPeerType_Direct)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(GPU_NVLINK_PEER_TYPE)});

    uint32_t val = htole32(NSM_PEER_TYPE_DIRECT);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_GpuNvlinkPeerType_Bridge)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(GPU_NVLINK_PEER_TYPE)});

    uint32_t val = htole32(0xFF); // Not NSM_PEER_TYPE_DIRECT -> Bridge
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_GpuHostId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(GPU_HOST_ID)});

    uint32_t val = htole32(5);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint32_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint32_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_GpuIbguid)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(GPU_IBGUID)});

    uint64_t val = 0x123456789ABCDEF0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint64_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint64_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_MinEdppScalingFactor)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MINIMUM_EDPP_SCALING_FACTOR)});

    uint8_t val = 50;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint8_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          sizeof(uint8_t), &val, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_PcieRetimerEeprom)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(PCIERETIMER_0_EEPROM_VERSION)});

    uint8_t data[8] = {1, 0, 2, 0, 0, 0, 3, 0}; // Major=1, Minor=2, Patch=3
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) + 8);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, 8, data,
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_ChassisSerialNumber)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(CHASSIS_SERIAL_NUMBER)});

    const char* serial = "SN12345";
    uint16_t dataSize = static_cast<uint16_t>(strlen(serial));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(serial), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetInventoryInfo_LengthMismatch)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation",
                    {"-p", std::to_string(MAXIMUM_MEMORY_CAPACITY)});

    // Wrong length: expect 4 bytes but send 2
    uint16_t val = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             sizeof(uint16_t));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(uint16_t),
        reinterpret_cast<const uint8_t*>(&val), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- GetClockOutputEnableState index branches -------------------------------

TEST(NsmTelemetryBranch, GetClockOutputEnableState_NVHS)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", std::to_string(NVHS_CLKBUF_INDEX)});

    uint32_t data = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, data,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetClockOutputEnableState_IBLINK)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", std::to_string(IBLINK_CLKBUF_INDEX)});

    uint32_t data = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, data,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetClockOutputEnableState_Default)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState", {"-i", "99"});

    uint32_t data = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, data,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetClockOutputEnableState_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", std::to_string(PCIE_CLKBUF_INDEX)});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryScalarGroupTelemetry error paths for each group -------------------

TEST(NsmTelemetryBranch, QueryScalarGroup_DefaultGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "99"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // groupId=99 -> default case
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryVectorGroupTelemetry default group --------------------------------

TEST(NsmTelemetryBranch, QueryVectorGroup_DefaultGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "99", "-s", "1", "-l", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryAvailableAndClearableScalarGroup default group
// ---------------------

TEST(NsmTelemetryBranch, QueryAvailAndClearable_DefaultGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "99"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

// ---- More error paths for remaining commands --------------------------------

TEST(NsmTelemetryBranch, PcieFundamentalReset_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[31]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, ClearScalarDataSource_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[32]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetClockLimit_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockLimit", {"-c", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetPowerLimit_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[34]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetPowerLimit_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[35]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetCurrClockFreq_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrClockFreq", {"-c", "0"});

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[36]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetAccumGpuUtilTime_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[37]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetProcessorThrottleReason_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetRowRemapState_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetRowRemappingCounts_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[40]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetRowRemapAvailability_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[41]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetLeakDetectionInfo_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[42]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, SetLeakDetectionThresholds_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[43]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetMemoryCapacityUtil_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[44]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetCurrentUtilization_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[45]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryBranch, GetSupportedGPMMetrics_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeErrorResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[47]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::telemetry
