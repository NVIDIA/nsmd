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

#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "../nsm_telemetry_cmd.cpp"

#include <gtest/gtest.h>

namespace nsmtool::telemetry
{

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

TEST(NsmTelBranch3, QueryPortCharacteristics_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortCharacteristics", {"-p", "3"});
    struct nsm_port_characteristics_data d{};
    d.nv_port_line_rate_mbps = 50000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_port_characteristics_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL, &d, msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, QueryPortStatus_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortStatus", {"-p", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_port_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL, NSM_PORTSTATE_UP,
                                  NSM_PORTSTATUS_ENABLED, msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, QueryPortsAvailable_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortsAvailable");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_ports_available_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_ports_available_resp(0, NSM_SUCCESS, ERR_NULL, 8, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, SetPortDisableFuture_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // PORT_MASK_DATA_SIZE = 32, pass all 32 bytes to avoid buffer overread
    parseSubcmdArgs(app, "SetPortDisableFuture",
                    {"-p", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                     "0",  "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                     "0",  "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetFabricManagerState_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetFabricManagerState");
    struct nsm_fabric_manager_state_data d{};
    d.fm_state = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_fabric_manager_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_fabric_manager_state_resp(0, NSM_SUCCESS, ERR_NULL, &d, msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetPortDisableFuture_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortDisableFuture");
    bitfield8_t portMask[PORT_MASK_DATA_SIZE]{};
    portMask[0].byte = 0xAB;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_port_disable_future_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, portMask,
                                        msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetPowerMode_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPowerMode");
    struct nsm_power_mode_data d{};
    d.l1_hw_mode_control = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &d, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetSwitchIsolationMode_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetSwitchIsolationMode");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_switch_isolation_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, 1, msg);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, SetSwitchIsolationMode_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetSwitchIsolationMode", {"-i", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* r =
        reinterpret_cast<nsm_common_resp*>(buf.data() + sizeof(nsm_msg_hdr));
    r->completion_code = NSM_SUCCESS;
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetTemperatureReading_RegularSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_temperature_reading_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    double reading = 72.5;
    encode_get_temperature_reading_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, ReadThermalParameter_RegularSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "ReadThermalParameter", {"-s", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_read_thermal_parameter_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    int32_t threshold = 95;
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, threshold,
                                       msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetCurrentPowerDraw_RegularSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentPowerDraw", {"-s", "0", "-a", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_power_draw_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 250000;
    encode_get_current_power_draw_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetMaxObservedPower_RegularSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetMaxObservedPower", {"-s", "0", "-a", "1"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_max_observed_power_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 350000;
    encode_get_max_observed_power_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetCurrentEnergyCount_RegularSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentEnergyCount", {"-s", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_energy_count_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint64_t reading = 123456789;
    encode_get_current_energy_count_resp(0, NSM_SUCCESS, ERR_NULL, reading,
                                         msg);
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetVoltage_RegularSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetVoltage", {"-s", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_voltage_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 12100;
    encode_get_voltage_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetAltitudePressure_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetAltitudePressure");
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_altitude_pressure_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 101325;
    encode_get_altitude_pressure_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetEccMode_EnabledPending)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEccMode");
    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetEccMode_DisabledNoPending)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEccMode");
    bitfield8_t flags{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetEccErrorCounts_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEccErrorCounts");
    struct nsm_ECC_error_counts ec{};
    ec.flags.bits.bit0 = 1;
    ec.sram_corrected = 10;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_error_counts_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL, &ec, msg);
    EXPECT_NO_THROW(commands[24]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetEDPpScalingFactors_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEDPpScalingFactors");
    struct nsm_EDPp_scaling_factors sf{};
    sf.persistent_scaling_factor = 80;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_programmable_EDPp_scaling_factor_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_programmable_EDPp_scaling_factor_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &sf, msg);
    EXPECT_NO_THROW(commands[26]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetCurrClockFreq_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrClockFreq", {"-c", "0"});
    uint32_t clockFreq = 1410;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_curr_clock_freq_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL, &clockFreq, msg);
    EXPECT_NO_THROW(commands[36]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetAccumGpuUtilTime_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetAccumGpuUtilTime");
    uint32_t ctxUtil = 5000, smUtil = 3000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_accum_GPU_util_time_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_accum_GPU_util_time_resp(0, NSM_SUCCESS, ERR_NULL, &ctxUtil,
                                        &smUtil, msg);
    EXPECT_NO_THROW(commands[37]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetProcessorThrottleReason_AllBits)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetProcessorThrottleReason");
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
TEST(NsmTelBranch3, GetProcessorThrottleReason_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetProcessorThrottleReason");
    bitfield32_t flags{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetRowRemapState_BothBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetRowRemapState");
    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    flags.bits.bit1 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetRowRemapState_NoBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetRowRemapState");
    bitfield8_t flags{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetRowRemappingCounts_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetRowRemappingCounts");
    uint32_t ce = 42, ue = 3;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remapping_counts_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, ERR_NULL, ce, ue, msg);
    EXPECT_NO_THROW(commands[40]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetRowRemapAvailability_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetRowRemapAvailability");
    struct nsm_row_remap_availability d{};
    d.no_remapping = 10;
    d.max_remapping = 50;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_availability_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL, &d, msg);
    EXPECT_NO_THROW(commands[41]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetMemoryCapacityUtil_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetMemoryCapacityUtil");
    struct nsm_memory_capacity_utilization d{};
    d.used_memory = 40960;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_memory_capacity_util_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, ERR_NULL, &d, msg);
    EXPECT_NO_THROW(commands[44]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetCurrentUtilization_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentUtilization");
    nsm_get_current_utilization_data d{};
    d.gpu_utilization = 85;
    d.memory_utilization = 60;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_utilization_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_utilization_resp(0, NSM_SUCCESS, ERR_NULL, &d, msg);
    EXPECT_NO_THROW(commands[45]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, ClockOutputEnableState_NVHS)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", std::to_string(NVHS_CLKBUF_INDEX)});
    uint32_t clkBuf = 0xFFFFFFFF;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, ClockOutputEnableState_IBLink)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", std::to_string(IBLINK_CLKBUF_INDEX)});
    uint32_t clkBuf = 0x07;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, ClockOutputEnableState_DefaultIndex)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState", {"-i", "99"});
    uint32_t clkBuf = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, GetSupportedGPMMetrics_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetSupportedGPMMetrics", {"-t", "0"});
    std::vector<uint8_t> bitmask(4, 0);
    bitmask[0] = 0x07;
    bitmask[1] = 0x10;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_supported_gpm_metrics_resp) +
                             bitmask.size());
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_gpm_metrics_resp(0, NSM_SUCCESS, ERR_NULL, 4, 8,
                                          bitmask.data(), msg);
    EXPECT_NO_THROW(commands[47]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, ScalarGroup_InvalidGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "99"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, VectorGroup_InvalidGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "99", "-s", "1", "-l", "0"});
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, AvailClearable_InvalidGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "99"});
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, AvailClearable_Group2_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "2"});
    bitfield8_t av[1]{}, cl[1]{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(av), reinterpret_cast<uint8_t*>(cl), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, AvailClearable_Group4_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "4"});
    bitfield8_t av[1]{}, cl[1]{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(av), reinterpret_cast<uint8_t*>(cl), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}
TEST(NsmTelBranch3, AvailClearable_Group9_NoBits)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "9"});
    bitfield8_t av[1]{}, cl[1]{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(av), reinterpret_cast<uint8_t*>(cl), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::telemetry
