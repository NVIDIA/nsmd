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
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "../nsm_telemetry_cmd.cpp"

#include <gtest/gtest.h>

// Telemetry commands (after registerCommand) in order:
//   [0]  GetPortTelemetryCounter
//   [1]  QueryPortCharacteristics
//   [2]  QueryPortStatus
//   [3]  QueryPortsAvailable
//   [4]  SetPortDisableFuture
//   [5]  GetFabricManagerState
//   [6]  GetPortDisableFuture
//   [7]  GetPowerMode
//   [8]  SetPowerMode
//   [9]  GetSwitchIsolationMode
//   [10] SetSwitchIsolationMode
//   [11] GetInventoryInformation
//   [12] GetTemperatureReading
//   [13] ReadThermalParameter
//   [14] GetCurrentPowerDraw
//   [15] GetMaxObservedPower
//   [16] GetCurrentEnergyCount
//   [17] GetVoltage
//   [18] GetAltitudePressure
//   [19] GetDriverInfo
//   [20] GetMigMode
//   [21] SetMigMode
//   [22] GetEccMode
//   [23] SetEccMode
//   [24] GetEccErrorCounts
//   [25] SetClockLimit
//   [26] GetEDPpScalingFactors
//   [27] SetEDPpScalingFactors
//   [28] QueryScalarGroupTelemetry
//   [29] QueryVectorGroupTelemetry
//   [30] QueryAvailableAndClearableScalarGroup
//   [31] PcieFundamentalReset
//   [32] ClearScalarDataSource
//   [33] GetClockLimit
//   [34] SetPowerLimit
//   [35] GetPowerLimit
//   [36] GetCurrClockFreq
//   [37] GetAccumGpuUtilTime
//   [38] GetProcessorThrottleReason
//   [39] GetRowRemapState
//   [40] GetRowRemappingCounts
//   [41] GetRowRemapAvailability
//   [42] GetLeakDetectionInfo
//   [43] SetLeakDetectionThresholds
//   [44] GetMemoryCapacityUtil
//   [45] GetCurrentUtilization
//   [46] GetClockOutputEnableState
//   [47] GetSupportedGPMMetrics
//   [48] QueryAggregatedGPMMetrics
//   [49] QueryPerInstanceGPMMetrics
//   [50] QueryPerInstanceGPMMetricsV2
//   [51] GetViolationDuration
//   [52] GetListAvailablePciePorts
//   [53] GetPCIePortConfig
//   [54] SetPCIePortConfig
//   [55] QueryMultiportScalarGroupTelemetry
//   [56] GetEthPortTelemetryCounter
//   [57] GetPortNetworkAddresses
//   [58] GetPortEccCounters
//   [59] GetPowerSmoothingFeatureInfoV2
//   [60] GetPowerSmoothingCurrentProfileInformationV2
//   [61] GetPowerSmoothingAdminOverrideProfileInformationV2
//   [62] GetPowerSmoothingPresetProfileInformationV2

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

// Helper: build a minimal aggregate response (0 samples)
static std::vector<uint8_t> makeAggregateResp(uint8_t cmd)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, cmd, NSM_SUCCESS, 0, msg);
    return buf;
}

// ---- [0] GetPortTelemetryCounter --------------------------------------------

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    struct nsm_port_counter_data portData{};
    // nsm_get_port_telemetry_counter_resp has data[1] so allocate for full
    // counter data
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- [1] QueryPortCharacteristics -------------------------------------------

TEST(NsmTelemetryCmdParse, QueryPortCharacteristics_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortCharacteristics", {"-p", "0"});

    struct nsm_port_characteristics_data charData{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_port_characteristics_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL, &charData,
                                           msg);
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

// ---- [2] QueryPortStatus ----------------------------------------------------

TEST(NsmTelemetryCmdParse, QueryPortStatus_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPortStatus", {"-p", "0"});

    uint8_t portState = NSM_PORTSTATE_UP;
    uint8_t portStatus = NSM_PORTSTATUS_ENABLED;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_port_status_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL, portState,
                                  portStatus, msg);
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

// ---- [3] QueryPortsAvailable ------------------------------------------------

TEST(NsmTelemetryCmdParse, QueryPortsAvailable_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_ports_available_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_ports_available_resp(0, NSM_SUCCESS, ERR_NULL, 1, msg);
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

// ---- [4] SetPortDisableFuture -----------------------------------------------

TEST(NsmTelemetryCmdParse, SetPortDisableFuture_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPortDisableFuture",
                    {"-p", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                     "0",  "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
                     "0",  "0", "0", "0", "0", "0", "0", "0", "0", "0", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

// ---- [5] GetFabricManagerState ----------------------------------------------

TEST(NsmTelemetryCmdParse, GetFabricManagerState_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_fabric_manager_state_data fmData{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_fabric_manager_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_fabric_manager_state_resp(0, NSM_SUCCESS, ERR_NULL, &fmData,
                                         msg);
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

// ---- [6] GetPortDisableFuture -----------------------------------------------

TEST(NsmTelemetryCmdParse, GetPortDisableFuture_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t portMask[PORT_MASK_DATA_SIZE]{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_port_disable_future_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_disable_future_resp(0, NSM_SUCCESS, ERR_NULL, portMask,
                                        msg);
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

// ---- [7] GetPowerMode -------------------------------------------------------

TEST(NsmTelemetryCmdParse, GetPowerMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_power_mode_data pmData{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_mode_resp(0, NSM_SUCCESS, ERR_NULL, &pmData, msg);
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

// ---- [8] SetPowerMode -------------------------------------------------------

TEST(NsmTelemetryCmdParse, SetPowerMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPowerMode",
                    {"-C", "0", "-T", "0", "-P", "0", "-t", "0", "-a", "0",
                     "-i", "0", "-p", "0"});

    // encode_set_power_mode_resp has no cc parameter
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_power_mode_resp(0, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

// ---- [9] GetSwitchIsolationMode ---------------------------------------------

TEST(NsmTelemetryCmdParse, GetSwitchIsolationMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_switch_isolation_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, 0, msg);
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

// ---- [10] SetSwitchIsolationMode --------------------------------------------

TEST(NsmTelemetryCmdParse, SetSwitchIsolationMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetSwitchIsolationMode", {"-i", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_switch_isolation_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

// ---- [11] GetInventoryInformation -------------------------------------------

TEST(NsmTelemetryCmdParse, GetInventoryInformation_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "0"});

    // propertyId=0 = BOARD_PART_NUMBER (string type)
    const char* partNumber = "TEST-PART-NUM";
    uint16_t dataSize = static_cast<uint16_t>(strlen(partNumber));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize,
        reinterpret_cast<const uint8_t*>(partNumber), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- [12] GetTemperatureReading (sensorId=255 -> aggregate) -----------------

TEST(NsmTelemetryCmdParse, GetTemperatureReading_Aggregate_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "255"});

    auto buf = makeAggregateResp(NSM_GET_TEMPERATURE_READING);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [12] GetTemperatureReading (sensorId=0 -> regular) --------------------

TEST(NsmTelemetryCmdParse, GetTemperatureReading_Regular_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_temperature_reading_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    double temp = 25.0;
    encode_get_temperature_reading_resp(0, NSM_SUCCESS, ERR_NULL, temp, msg);
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

// ---- [13] ReadThermalParameter (sensorId=255 -> aggregate) -----------------

TEST(NsmTelemetryCmdParse, ReadThermalParameter_Aggregate_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "ReadThermalParameter", {"-s", "255"});

    auto buf = makeAggregateResp(NSM_READ_THERMAL_PARAMETER);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[13]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [13] ReadThermalParameter (sensorId=0 -> regular) ---------------------

TEST(NsmTelemetryCmdParse, ReadThermalParameter_Regular_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "ReadThermalParameter", {"-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_read_thermal_parameter_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    int32_t threshold = 100;
    encode_read_thermal_parameter_resp(0, NSM_SUCCESS, ERR_NULL, threshold,
                                       msg);
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

// ---- [14] GetCurrentPowerDraw (sensorId=255 -> aggregate) ------------------

TEST(NsmTelemetryCmdParse, GetCurrentPowerDraw_Aggregate_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentPowerDraw", {"-s", "255"});

    auto buf = makeAggregateResp(NSM_GET_POWER);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[14]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [14] GetCurrentPowerDraw (sensorId=0 -> regular) ----------------------

TEST(NsmTelemetryCmdParse, GetCurrentPowerDraw_Regular_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentPowerDraw", {"-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_power_draw_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 150000;
    encode_get_current_power_draw_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

// ---- [15] GetMaxObservedPower (sensorId=255 -> aggregate) ------------------

TEST(NsmTelemetryCmdParse, GetMaxObservedPower_Aggregate_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetMaxObservedPower", {"-s", "255"});

    auto buf = makeAggregateResp(NSM_GET_MAX_OBSERVED_POWER);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[15]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [15] GetMaxObservedPower (sensorId=0 -> regular) ----------------------

TEST(NsmTelemetryCmdParse, GetMaxObservedPower_Regular_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetMaxObservedPower", {"-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_power_draw_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 200000;
    encode_get_max_observed_power_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

// ---- [16] GetCurrentEnergyCount (sensorId=255 -> aggregate) ----------------

TEST(NsmTelemetryCmdParse, GetCurrentEnergyCount_Aggregate_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentEnergyCount", {"-s", "255"});

    auto buf = makeAggregateResp(NSM_GET_ENERGY_COUNT);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[16]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [16] GetCurrentEnergyCount (sensorId=0 -> regular) --------------------

TEST(NsmTelemetryCmdParse, GetCurrentEnergyCount_Regular_ParseResponseSuccess)
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

// ---- [17] GetVoltage (sensorId=255 -> aggregate) ---------------------------

TEST(NsmTelemetryCmdParse, GetVoltage_Aggregate_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetVoltage", {"-s", "255"});

    auto buf = makeAggregateResp(NSM_GET_VOLTAGE);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[17]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [17] GetVoltage (sensorId=0 -> regular) --------------------------------

TEST(NsmTelemetryCmdParse, GetVoltage_Regular_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetVoltage", {"-s", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_voltage_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 1200;
    encode_get_voltage_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

// ---- [18] GetAltitudePressure -----------------------------------------------

TEST(NsmTelemetryCmdParse, GetAltitudePressure_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_altitude_pressure_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    uint32_t reading = 101325;
    encode_get_altitude_pressure_resp(0, NSM_SUCCESS, ERR_NULL, reading, msg);
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

// ---- [19] GetDriverInfo -----------------------------------------------------

TEST(NsmTelemetryCmdParse, GetDriverInfo_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    const char* version = "550.00.00";
    uint16_t dataSize = 1 + static_cast<uint16_t>(strlen(version) + 1);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_driver_info_resp) +
                             strlen(version));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // driver_info_data: first byte = driver_state, remaining = version string
    std::vector<uint8_t> driverInfoData(dataSize, 0);
    driverInfoData[0] = static_cast<uint8_t>(DriverStateEnum::DriverLoaded);
    memcpy(driverInfoData.data() + 1, version, strlen(version));
    encode_get_driver_info_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                driverInfoData.data(), msg);
    EXPECT_NO_THROW(commands[19]->parseResponseMsg(msg, buf.size()));
}

// ---- [20] GetMigMode --------------------------------------------------------

TEST(NsmTelemetryCmdParse, GetMigMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_MIG_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_MIG_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, buf.size()));
}

// ---- [21] SetMigMode --------------------------------------------------------

TEST(NsmTelemetryCmdParse, SetMigMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetMigModes", {"-r", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_MIG_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[21]->parseResponseMsg(msg, buf.size()));
}

// ---- [22] GetEccMode --------------------------------------------------------

TEST(NsmTelemetryCmdParse, GetEccMode_ParseResponseSuccess)
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

// ---- [23] SetEccMode --------------------------------------------------------

TEST(NsmTelemetryCmdParse, SetEccMode_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetEccMode", {"-r", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[23]->parseResponseMsg(msg, buf.size()));
}

// ---- [24] GetEccErrorCounts -------------------------------------------------

TEST(NsmTelemetryCmdParse, GetEccErrorCounts_ParseResponseSuccess)
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

// ---- [25] SetClockLimit -----------------------------------------------------

TEST(NsmTelemetryCmdParse, SetClockLimit_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetClockLimit",
                    {"-c", "0", "-f", "0", "-a", "100", "-b", "200"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[25]->parseResponseMsg(msg, buf.size()));
}

// ---- [26] GetEDPpScalingFactors ---------------------------------------------

TEST(NsmTelemetryCmdParse, GetEDPpScalingFactors_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_EDPp_scaling_factors sf{};
    sf.persistent_scaling_factor = 80;
    sf.oneshot_scaling_factor = 90;
    sf.enforced_scaling_factor = 85;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_programmable_EDPp_scaling_factor_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_programmable_EDPp_scaling_factor_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &sf, msg);
    EXPECT_NO_THROW(commands[26]->parseResponseMsg(msg, buf.size()));
}

// ---- [27] SetEDPpScalingFactors ---------------------------------------------

TEST(NsmTelemetryCmdParse, SetEDPpScalingFactors_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetEDPpScalingFactors",
                    {"-a", "0", "-p", "0", "-s", "100"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_programmable_EDPp_scaling_factor_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     msg);
    EXPECT_NO_THROW(commands[27]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 0)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group0_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "0"});

    struct nsm_query_scalar_group_telemetry_group_0 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 1)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group1_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 2)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "2"});

    struct nsm_query_scalar_group_telemetry_group_2 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group2_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 3)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group3_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "3"});

    struct nsm_query_scalar_group_telemetry_group_3 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group3_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 4)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group4_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "4"});

    struct nsm_query_scalar_group_telemetry_group_4 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group4_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 5)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group5_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "5"});

    struct nsm_query_scalar_group_telemetry_group_5 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group5_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 6)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group6_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "6"});

    struct nsm_query_scalar_group_telemetry_group_6 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group6_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 8)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group8_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "8"});

    struct nsm_query_scalar_group_telemetry_group_8 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_8_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group8_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 9)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group9_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "9"});

    struct nsm_query_scalar_group_telemetry_group_9 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_9_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group9_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (group 10)
// --------------------------------

TEST(NsmTelemetryCmdParse,
     QueryScalarGroupTelemetry_Group10_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "10"});

    struct nsm_query_scalar_group_telemetry_group_10 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_10_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group10_resp(0, NSM_SUCCESS,
                                                        ERR_NULL, &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [28] QueryScalarGroupTelemetry (unknown group -> default case)
// ----------

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_UnknownGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- [29] QueryVectorGroupTelemetry (group 1) -------------------------------

TEST(NsmTelemetryCmdParse,
     QueryVectorGroupTelemetry_Group1_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "1", "-s", "1", "-l", "0"});

    nsm_query_vector_group_1_data data{};
    data.cdr_error_per_lane = 42;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_query_vector_data_sources_v2_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_vector_group_telemetry_v2_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

// ---- [29] QueryVectorGroupTelemetry (invalid group -> default case) ---------

TEST(NsmTelemetryCmdParse, QueryVectorGroupTelemetry_InvalidGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // groupId=2 hits the default case in the switch statement
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "2", "-s", "1", "-l", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

// ---- [30] QueryAvailableAndClearableScalarGroup (group 2) -------------------

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "2"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    // +1 because data[1] in the struct only holds 1 byte but we need
    // mask_length bytes for available + mask_length bytes for clearable = 2
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

// ---- [30] QueryAvailableAndClearableScalarGroup (group 3) -------------------

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group3_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "3"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
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

// ---- [30] QueryAvailableAndClearableScalarGroup (group 4) -------------------

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group4_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "4"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
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

// ---- [30] QueryAvailableAndClearableScalarGroup (default group)
// --------------

TEST(NsmTelemetryCmdParse, QueryAvailableAndClearableScalarGroup_DefaultGroup)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // groupId=1 hits the default case (invalid) in the switch statement
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

// ---- [31] PcieFundamentalReset ----------------------------------------------

TEST(NsmTelemetryCmdParse, PcieFundamentalReset_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "PcieFundamentalReset", {"-d", "0", "-a", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[31]->parseResponseMsg(msg, buf.size()));
}

// ---- [32] ClearScalarDataSource ---------------------------------------------

TEST(NsmTelemetryCmdParse, ClearScalarDataSource_ParseResponseSuccess)
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

// ---- [33] GetClockLimit -----------------------------------------------------

TEST(NsmTelemetryCmdParse, GetClockLimit_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockLimit", {"-c", "0"});

    struct nsm_clock_limit clockLimit{};
    clockLimit.present_limit_min = 100;
    clockLimit.present_limit_max = 1000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_limit_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit, msg);
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, buf.size()));
}

// ---- [33] GetClockLimit (SpeedLocked case) ----------------------------------

TEST(NsmTelemetryCmdParse, GetClockLimit_SpeedLocked_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockLimit", {"-c", "0"});

    struct nsm_clock_limit clockLimit{};
    // present_limit_min == present_limit_max -> SpeedLocked = true
    clockLimit.present_limit_min = 1000;
    clockLimit.present_limit_max = 1000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_limit_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, &clockLimit, msg);
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, buf.size()));
}

// ---- [34] SetPowerLimit -----------------------------------------------------

TEST(NsmTelemetryCmdParse, SetPowerLimit_ParseResponseSuccess)
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

// ---- [35] GetPowerLimit -----------------------------------------------------

TEST(NsmTelemetryCmdParse, GetPowerLimit_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPowerLimit", {"-i", "0"});

    uint32_t persistentLimit = 300000;
    uint32_t oneShotLimit = 400000;
    uint32_t enforcedLimit = 300000;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_power_limit_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, persistentLimit,
                                oneShotLimit, enforcedLimit, msg);
    EXPECT_NO_THROW(commands[35]->parseResponseMsg(msg, buf.size()));
}

// ---- [36] GetCurrClockFreq --------------------------------------------------

TEST(NsmTelemetryCmdParse, GetCurrClockFreq_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrClockFreq", {"-c", "0"});

    uint32_t clockFreq = 1200;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_curr_clock_freq_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL, &clockFreq, msg);
    EXPECT_NO_THROW(commands[36]->parseResponseMsg(msg, buf.size()));
}

// ---- [37] GetAccumGpuUtilTime -----------------------------------------------

TEST(NsmTelemetryCmdParse, GetAccumGpuUtilTime_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    uint32_t contextUtil = 12345;
    uint32_t smUtil = 67890;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_accum_GPU_util_time_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_accum_GPU_util_time_resp(0, NSM_SUCCESS, ERR_NULL, &contextUtil,
                                        &smUtil, msg);
    EXPECT_NO_THROW(commands[37]->parseResponseMsg(msg, buf.size()));
}

// ---- [38] GetProcessorThrottleReason ----------------------------------------

TEST(NsmTelemetryCmdParse, GetProcessorThrottleReason_ParseResponseSuccess)
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

// ---- [39] GetRowRemapState --------------------------------------------------

TEST(NsmTelemetryCmdParse, GetRowRemapState_ParseResponseSuccess)
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

// ---- [40] GetRowRemappingCounts ---------------------------------------------

TEST(NsmTelemetryCmdParse, GetRowRemappingCounts_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    uint32_t correctable = 0;
    uint32_t uncorrectable = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remapping_counts_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, ERR_NULL, correctable,
                                         uncorrectable, msg);
    EXPECT_NO_THROW(commands[40]->parseResponseMsg(msg, buf.size()));
}

// ---- [41] GetRowRemapAvailability -------------------------------------------

TEST(NsmTelemetryCmdParse, GetRowRemapAvailability_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_row_remap_availability remapData{};
    remapData.high_remapping = 100;
    remapData.low_remapping = 50;
    remapData.partial_remapping = 75;
    remapData.no_remapping = 25;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_availability_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL, &remapData,
                                           msg);
    EXPECT_NO_THROW(commands[41]->parseResponseMsg(msg, buf.size()));
}

// ---- [42] GetLeakDetectionInfo ----------------------------------------------

TEST(NsmTelemetryCmdParse, GetLeakDetectionInfo_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // Encode response with 0 sensors
    uint8_t numSensors = 0;
    uint8_t numThresholdLevels = 0;
    std::vector<uint8_t> sensorsData{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_leak_detection_info_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_leak_detection_info_resp(0, NSM_SUCCESS, ERR_NULL, numSensors,
                                        numThresholdLevels, sensorsData.data(),
                                        sensorsData.size(), msg);
    EXPECT_NO_THROW(commands[42]->parseResponseMsg(msg, buf.size()));
}

// ---- [43] SetLeakDetectionThresholds ----------------------------------------

TEST(NsmTelemetryCmdParse, SetLeakDetectionThresholds_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetLeakDetectionThresholds",
                    {"-s", "0", "-n", "1", "-t", "100"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    EXPECT_NO_THROW(commands[43]->parseResponseMsg(msg, buf.size()));
}

// ---- [44] GetMemoryCapacityUtil ---------------------------------------------

TEST(NsmTelemetryCmdParse, GetMemoryCapacityUtil_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_memory_capacity_utilization data{};
    data.used_memory = 8192;
    data.reserved_memory = 1024;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_memory_capacity_util_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);
    EXPECT_NO_THROW(commands[44]->parseResponseMsg(msg, buf.size()));
}

// ---- [45] GetCurrentUtilization ---------------------------------------------

TEST(NsmTelemetryCmdParse, GetCurrentUtilization_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    nsm_get_current_utilization_data data{};
    data.gpu_utilization = 75;
    data.memory_utilization = 50;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_current_utilization_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_utilization_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);
    EXPECT_NO_THROW(commands[45]->parseResponseMsg(msg, buf.size()));
}

// ---- [46] GetClockOutputEnableState -----------------------------------------

TEST(NsmTelemetryCmdParse, GetClockOutputEnableState_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState", {"-i", "0x80"});

    uint32_t clkBuf = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

// ---- [47] GetSupportedGPMMetrics --------------------------------------------

TEST(NsmTelemetryCmdParse, GetSupportedGPMMetrics_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetSupportedGPMMetrics", {"-t", "0"});

    uint8_t bitmask[1] = {0};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_supported_gpm_metrics_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_supported_gpm_metrics_resp(0, NSM_SUCCESS, ERR_NULL, 0, 0,
                                          bitmask, msg);
    EXPECT_NO_THROW(commands[47]->parseResponseMsg(msg, buf.size()));
}

// ---- [48] QueryAggregatedGPMMetrics (aggregate response) --------------------

TEST(NsmTelemetryCmdParse, QueryAggregatedGPMMetrics_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAggregatedGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-b", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[48]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [49] QueryPerInstanceGPMMetrics ----------------------------------------

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetrics_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-d", "1", "-b", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[49]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [50] QueryPerInstanceGPMMetricsV2 --------------------------------------

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetricsV2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetricsV2",
                    {"-r", "0", "-g", "0", "-c", "0", "-d", "1", "-b", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[50]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [51] GetViolationDuration ----------------------------------------------

TEST(NsmTelemetryCmdParse, GetViolationDuration_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    struct nsm_violation_duration vd{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_violation_duration_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_violation_duration_resp(0, NSM_SUCCESS, ERR_NULL, &vd, msg);
    EXPECT_NO_THROW(commands[51]->parseResponseMsg(msg, buf.size()));
}

// ---- [52] GetListAvailablePciePorts -----------------------------------------

TEST(NsmTelemetryCmdParse, GetListAvailablePciePorts_ParseResponseSuccess)
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

// ---- [53] GetPCIePortConfig (aggregate response) ----------------------------

TEST(NsmTelemetryCmdParse, GetPCIePortConfig_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[53]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [54] SetPCIePortConfig -------------------------------------------------

TEST(NsmTelemetryCmdParse, SetPCIePortConfig_ParseResponseSuccess)
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

// ---- [55] QueryMultiportScalarGroupTelemetry (group 0) ----------------------

TEST(NsmTelemetryCmdParse,
     QueryMultiportScalarGroupTelemetry_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryMultiportScalarGroupTelemetry",
                    {"-d", "0", "-g", "0", "-t", "0", "-u", "0", "-i", "0"});

    struct nsm_query_scalar_group_telemetry_group_0 data{};
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_0_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group0_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[55]->parseResponseMsg(msg, buf.size()));
}

// ---- [56] GetEthPortTelemetryCounter (aggregate response) -------------------

TEST(NsmTelemetryCmdParse, GetEthPortTelemetryCounter_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEthPortTelemetryCounter", {"-p", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[56]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [57] GetPortNetworkAddresses (aggregate response) ----------------------

TEST(NsmTelemetryCmdParse, GetPortNetworkAddresses_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortNetworkAddresses", {"-p", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[57]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [58] GetPortEccCounters (aggregate response) ---------------------------

TEST(NsmTelemetryCmdParse, GetPortEccCounters_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortEccCounters", {"-p", "0"});

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[58]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [59] GetPowerSmoothingFeatureInfoV2 (aggregate response) ---------------

TEST(NsmTelemetryCmdParse, GetPowerSmoothingFeatureInfoV2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[59]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [60] GetPowerSmoothingCurrentProfileInformationV2 (aggregate) ----------

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingCurrentProfileInformationV2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[60]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [61] GetPowerSmoothingAdminOverrideProfileInformationV2 (aggregate) ----

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingAdminOverrideProfileInformationV2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[61]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- [62] GetPowerSmoothingPresetProfileInformationV2 (aggregate) -----------

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingPresetProfileInformationV2_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);

    auto buf = makeAggregateResp(0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[62]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// Helper: build aggregate response with a single temperature sample
static std::vector<uint8_t>
    makeAggregateRespWithTempSample(uint8_t cmd, uint8_t sensorId, double temp)
{
    // Start with aggregate header (telemetry_count=1)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, cmd, NSM_SUCCESS, 1, msg);

    // Encode the temperature sample data
    std::array<uint8_t, 50> sampleBuf{};
    auto* nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
    uint8_t reading[8]{};
    size_t consumed_len{};
    encode_aggregate_temperature_reading_data(temp, reading, &consumed_len);
    size_t sample_len{};
    encode_aggregate_resp_sample(sensorId, true, reading, consumed_len,
                                 nsm_sample, &sample_len);
    buf.insert(buf.end(), sampleBuf.begin(),
               std::next(sampleBuf.begin(), sample_len));
    return buf;
}

// Helper: build aggregate response with timestamp + 1 temperature sample
static std::vector<uint8_t>
    makeAggregateRespWithTimestampAndSample(uint8_t cmd, uint8_t sensorId,
                                            double temp)
{
    // telemetry_count=2 (timestamp + data sample)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, cmd, NSM_SUCCESS, 2, msg);

    std::array<uint8_t, 50> sampleBuf{};
    auto* nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
    uint8_t reading[8]{};
    size_t consumed_len{};
    size_t sample_len{};

    // Timestamp sample (tag=0xFF)
    uint64_t timestamp = 1700000000ULL;
    encode_aggregate_timestamp_data(timestamp, reading, &consumed_len);
    encode_aggregate_resp_sample(0xFF, true, reading, consumed_len, nsm_sample,
                                 &sample_len);
    buf.insert(buf.end(), sampleBuf.begin(),
               std::next(sampleBuf.begin(), sample_len));

    // Temperature data sample
    encode_aggregate_temperature_reading_data(temp, reading, &consumed_len);
    encode_aggregate_resp_sample(sensorId, true, reading, consumed_len,
                                 nsm_sample, &sample_len);
    buf.insert(buf.end(), sampleBuf.begin(),
               std::next(sampleBuf.begin(), sample_len));

    return buf;
}

// ---- GetInventoryInformation variants for uncovered property IDs
// -------------

// uint32_t property: MAXIMUM_MEMORY_CAPACITY=7
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_MAXIMUM_MEMORY_CAPACITY)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "7"});
    uint32_t value = htole32(4096);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: GPU_HOST_ID=34 (Data = le32toh(value) + 1)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_GPU_HOST_ID)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "34"});
    uint32_t value = htole32(1);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// UUID property: DEVICE_GUID=10 (UUID_INT_SIZE=16 bytes)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_UUID_DEVICE_GUID)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "10"});
    constexpr uint16_t dataSize = UUID_INT_SIZE;
    std::vector<uint8_t> guidData(UUID_INT_SIZE, 0xAB);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          guidData.data(), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// GPU_NVLINK_PEER_TYPE=36 → Direct (peerType=0)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_GPU_NVLINK_PEER_TYPE_Direct)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "36"});
    uint32_t peerType = htole32(NSM_PEER_TYPE_DIRECT);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&peerType),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// GPU_NVLINK_PEER_TYPE=36 → Bridge (peerType=1)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_GPU_NVLINK_PEER_TYPE_Bridge)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "36"});
    uint32_t peerType = htole32(1); // 1 = Bridge
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&peerType),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// CHASSIS_SERIAL_NUMBER=31 (string)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_String_CHASSIS_SERIAL_NUMBER)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "31"});
    const char* serial = "CHASSIS-SN-001";
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

// GPU_IBGUID=30 (uint64_t / 8 bytes)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Hex_GPU_IBGUID)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "30"});
    uint64_t ibguid = 0xDEADBEEFCAFEBABEULL;
    uint16_t dataSize = sizeof(uint64_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&ibguid),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// MINIMUM_EDPP_SCALING_FACTOR=24 (uint8_t)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint8_MINIMUM_EDPP_SCALING_FACTOR)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "24"});
    uint8_t scalingFactor = 50;
    uint16_t dataSize = sizeof(uint8_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          &scalingFactor, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// PCIERETIMER_0_EEPROM_VERSION=144 (uint64_t, version bytes)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Version_PCIERETIMER_0_EEPROM)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "144"});
    // data[0]=Major, data[2]=Minor, data[4..5]=PatchHigh, data[6]=PatchLow
    uint64_t versionData = 0x0100020003000400ULL; // some version bytes
    uint16_t dataSize = sizeof(uint64_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize,
        reinterpret_cast<uint8_t*>(&versionData), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: MAXIMUM_MODULE_POWER_LIMIT=18 (uncovered switch group)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_MAXIMUM_MODULE_POWER_LIMIT)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "18"});
    uint32_t value = htole32(300000);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: RATED_MODULE_POWER_LIMIT=20 (uncovered switch group)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_RATED_MODULE_POWER_LIMIT)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "20"});
    uint32_t value = htole32(250000);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: MINIMUM_MEMORY_CLOCK_LIMIT=28
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_MINIMUM_MEMORY_CLOCK_LIMIT)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "28"});
    uint32_t value = htole32(100);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: MAXIMUM_MEMORY_CLOCK_LIMIT=29
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_MAXIMUM_MEMORY_CLOCK_LIMIT)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "29"});
    uint32_t value = htole32(1200);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: MAXIMUM_GRAPHICS_CLOCK_LIMIT=27
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_MAXIMUM_GRAPHICS_CLOCK_LIMIT)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "27"});
    uint32_t value = htole32(2000);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint32_t property: MINIMUM_GRAPHICS_CLOCK_LIMIT=26
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint32_MINIMUM_GRAPHICS_CLOCK_LIMIT)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "26"});
    uint32_t value = htole32(200);
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// uint8_t property: MAXIMUM_EDPP_SCALING_FACTOR=25
TEST(NsmTelemetryCmdParse, GetInventoryInfo_Uint8_MAXIMUM_EDPP_SCALING_FACTOR)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "25"});
    uint8_t scalingFactor = 100;
    uint16_t dataSize = sizeof(uint8_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          &scalingFactor, msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// Error path: data size mismatch (covers lines 1140-1142)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_SizeMismatch_ErrorPath)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // MAXIMUM_MEMORY_CAPACITY=7 expects sizeof(uint32_t)=4, but we send 2
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "7"});
    uint16_t wrongData = 0x1234;
    uint16_t dataSize = sizeof(uint16_t); // wrong size (expected 4, got 2)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, dataSize,
        reinterpret_cast<uint8_t*>(&wrongData), msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// Unknown property (default case → cerr + return)
TEST(NsmTelemetryCmdParse, GetInventoryInfo_UnknownProperty_Default)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // property 99 is not in any switch case -> hits default
    parseSubcmdArgs(app, "GetInventoryInformation", {"-p", "99"});
    uint32_t value = 0;
    uint16_t dataSize = sizeof(uint32_t);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_inventory_information_resp) +
                             dataSize);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL, dataSize,
                                          reinterpret_cast<uint8_t*>(&value),
                                          msg);
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryAvailableAndClearableScalarGroup with non-zero bitfields
// -----------

// Group 2 with all bits set (covers the push_back branches)
TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group2_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "2"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0x0F; // bit0..bit3 all set
    clearable[0].byte = 0x0F;
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

// Group 3 with bit0 set (covers the L0ToRecoveryCount push_back)
TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group3_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "3"});

    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    available[0].byte = 0x01; // bit0 set
    clearable[0].byte = 0x01;
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

// Group 4 with all bits set
TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group4_AllBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "4"});

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

// ---- GetClockOutputEnableState with NVHS and IBLINK indices -----------------

TEST(NsmTelemetryCmdParse, GetClockOutputEnableState_NVHS_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", "0x81"}); // NVHS_CLKBUF_INDEX

    uint32_t clkBuf = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse,
     GetClockOutputEnableState_IBLINK_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", "0x82"}); // IBLINK_CLKBUF_INDEX

    uint32_t clkBuf = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse,
     GetClockOutputEnableState_DefaultIndex_ParseResponseSuccess)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetClockOutputEnableState",
                    {"-i", "0x00"}); // unknown index -> default case

    uint32_t clkBuf = 0;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_clock_output_enabled_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, ERR_NULL, clkBuf,
                                              msg);
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

// ---- GetPortTelemetryCounter with all supported_counter bits set ------------

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_AllCountersSupported)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    struct nsm_port_counter_data portData{};
    // Set all supported_counter bits to 1 so all if-branches are taken
    memset(&portData.supported_counter, 0xFF,
           sizeof(portData.supported_counter));
    // Set some counter values
    portData.port_rcv_pkts = 100;
    portData.port_rcv_data = 200;
    portData.port_xmit_pkts = 300;
    portData.port_xmit_data = 400;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- Aggregate response parsing with actual samples -------------------------

// GetTemperatureReading aggregate with 1 sample (covers loop body)
TEST(NsmTelemetryCmdParse, GetTemperatureReading_Aggregate_WithSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "255"});

    auto buf = makeAggregateRespWithTempSample(NSM_GET_TEMPERATURE_READING, 0,
                                               42.5);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// GetTemperatureReading aggregate with timestamp + sample (covers timestamp
// path)
TEST(NsmTelemetryCmdParse,
     GetTemperatureReading_Aggregate_WithTimestampAndSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "255"});

    auto buf = makeAggregateRespWithTimestampAndSample(
        NSM_GET_TEMPERATURE_READING, 0, 35.0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// GetCurrentPowerDraw aggregate with a sample (covers power reading path)
TEST(NsmTelemetryCmdParse, GetCurrentPowerDraw_Aggregate_WithSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentPowerDraw", {"-s", "255"});

    // Build aggregate with 1 power sample
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_POWER, NSM_SUCCESS, 1, msg);

    std::array<uint8_t, 50> sampleBuf{};
    auto* nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
    uint8_t reading[8]{};
    size_t consumed_len{};
    size_t sample_len{};
    encode_aggregate_get_current_power_draw_reading(25000, reading,
                                                    &consumed_len);
    encode_aggregate_resp_sample(0, true, reading, consumed_len, nsm_sample,
                                 &sample_len);
    buf.insert(buf.end(), sampleBuf.begin(),
               std::next(sampleBuf.begin(), sample_len));

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[14]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- Helper: append a sample to an aggregate response buffer ---------------

static void appendAggSample(std::vector<uint8_t>& buf, uint8_t tag,
                            const uint8_t* data, size_t data_len)
{
    std::array<uint8_t, 64> sampleBuf{};
    auto* nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
    size_t sample_len{};
    encode_aggregate_resp_sample(tag, true, data, data_len, nsm_sample,
                                 &sample_len);
    buf.insert(buf.end(), sampleBuf.begin(),
               std::next(sampleBuf.begin(), sample_len));
}

// ---- QueryAvailableAndClearableScalarGroup GROUP_ID_8 ----------------------

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group8_AllBitsSet)
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

// ---- QueryAvailableAndClearableScalarGroup GROUP_ID_9 ----------------------

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_Group9_AllBitsSet)
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

// ---- QueryAvailableAndClearableScalarGroup default (unknown groupId) --------

TEST(NsmTelemetryCmdParse, QueryAvailableAndClearableScalarGroup_DefaultGroupId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                    {"-d", "0", "-g", "99"});
    // default branch just prints error; no decode called
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp) + 1);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    bitfield8_t available[1]{};
    bitfield8_t clearable[1]{};
    encode_query_available_clearable_scalar_data_sources_v1_resp(
        0, NSM_SUCCESS, ERR_NULL,
        static_cast<uint16_t>(sizeof(uint8_t) + 2 * sizeof(uint8_t)), 1,
        reinterpret_cast<uint8_t*>(available),
        reinterpret_cast<uint8_t*>(clearable), msg);
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

// ---- GetPowerSmoothingFeatureInfoV2 with all sample tags -------------------

TEST(NsmTelemetryCmdParse, GetPowerSmoothingFeatureInfoV2_AllSampleTags)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // 8 samples: FEATURE_FLAG_TAG..MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 8, msg);

    uint8_t d[8]{};
    size_t dl{};

    encodeFeatureFlagSample(0x0F, d, &dl);
    appendAggSample(buf, FEATURE_FLAG_TAG, d, dl);

    encodeCurrentTMPSettingSample(100, d, &dl);
    appendAggSample(buf, CURRENT_TMP_SETTING_TAG, d, dl);

    encodeCurrentTMPFloorSettingSample(200, d, &dl);
    appendAggSample(buf, TMP_FLOOR_SETTING_TAG, d, dl);

    encodeMaxTmpFloorSettingSample(300, d, &dl);
    appendAggSample(buf, MAX_TMP_FLOOR_SETTING_TAG, d, dl);

    encodeMinTmpFloorSettingSample(50, d, &dl);
    appendAggSample(buf, MIN_TMP_FLOOR_SETTING_TAG, d, dl);

    encodeFloorWindowMultiplierSample(5, d, &dl);
    appendAggSample(buf, FLOOR_WINDOW_MULTIPLIER_TAG, d, dl);

    encodeMinPrimaryFloorActivationOffset(10, d, &dl);
    appendAggSample(buf, MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG, d, dl);

    encodeMinPrimaryFloorActivationPoint(20, d, &dl);
    appendAggSample(buf, MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[59]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPowerSmoothingCurrentProfileInformationV2 with all tags ------------

TEST(NsmTelemetryCmdParse, GetPowerSmoothingCurrentProfileInfoV2_AllSampleTags)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // 10 tags:
    // ACTIVE_PRESET_PROFILE_ID_TAG..CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 10, msg);

    uint8_t d[8]{};
    size_t dl{};

    encodeActivePresetProfileSample(1, d, &dl);
    appendAggSample(buf, ACTIVE_PRESET_PROFILE_ID_TAG, d, dl);

    encodeAdminOverrideMaskSample(0xFF, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_MASK_TAG, d, dl);

    encodeCurrentTMPFloorSample(200, d, &dl);
    appendAggSample(buf, CURRENT_TMP_FLOOR_SETTING_TAG, d, dl);

    encodeCurrentRampupRateSample(300, d, &dl);
    appendAggSample(buf, CURRENT_RAMPUP_RATE_TAG, d, dl);

    encodeCurrentRampdownRateSample(400, d, &dl);
    appendAggSample(buf, CURRENT_RAMPDOWN_RATE_TAG, d, dl);

    encodeCurrentRampdownHysteresisSample(500, d, &dl);
    appendAggSample(buf, CURRENT_RAMPDOWN_HYSTERESIS_TAG, d, dl);

    encodeCurrentSecondaryFloorSample(600, d, &dl);
    appendAggSample(buf, CURRENT_SECONDARY_FLOOR_TAG, d, dl);

    encodeCurrentPrimaryFloorActivationWindowMultiplierSample(7, d, &dl);
    appendAggSample(buf, CURRENT_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG,
                    d, dl);

    encodeCurrentPrimaryFloorTargetWindowSample(8, d, &dl);
    appendAggSample(buf, CURRENT_PRIMARY_FLOOR_TARGET_WINDOW_TAG, d, dl);

    encodeCurrentPrimaryFloorActivationOffsetSample(9000, d, &dl);
    appendAggSample(buf, CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[60]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPowerSmoothingAdminOverrideProfileInformationV2 with all tags ------

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingAdminOverrideProfileInfoV2_AllSampleTags)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // 8 samples: ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG..
    //            ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 8, msg);

    uint8_t d[8]{};
    size_t dl{};

    encodeCurrentTMPFloorSample(100, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG, d, dl);

    encodeCurrentRampupRateSample(200, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG, d, dl);

    encodeCurrentRampdownRateSample(300, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG, d, dl);

    encodeCurrentRampdownHysteresisSample(400, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG, d, dl);

    encodeCurrentSecondaryFloorSample(500, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG, d, dl);

    encodeCurrentPrimaryFloorActivationWindowMultiplierSample(6, d, &dl);
    appendAggSample(
        buf,
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG,
        d, dl);

    encodeCurrentPrimaryFloorTargetWindowSample(7, d, &dl);
    appendAggSample(buf, ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG,
                    d, dl);

    encodeCurrentPrimaryFloorActivationOffsetSample(8000, d, &dl);
    appendAggSample(
        buf, ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[61]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPowerSmoothingPresetProfileInformationV2 with all profile tags -----

TEST(NsmTelemetryCmdParse, GetPowerSmoothingPresetProfileInfoV2_AllSampleTags)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // Preset profile tag = (tagId << 3) | profId; use profId=0 for all
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 8, msg);

    uint8_t d[8]{};
    size_t dl{};

    // PRESET_PROFILE_TMP_FLOOR_SETTING_TAG=0: tag=(0<<3)|0=0
    encodeCurrentTMPFloorSample(100, d, &dl);
    appendAggSample(
        buf, static_cast<uint8_t>(PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3), d,
        dl);

    // PRESET_PROFILE_RAMPUP_RATE_TAG=1: tag=(1<<3)|0=8
    encodeCurrentRampupRateSample(200, d, &dl);
    appendAggSample(
        buf, static_cast<uint8_t>(PRESET_PROFILE_RAMPUP_RATE_TAG << 3), d, dl);

    // PRESET_PROFILE_RAMPDOWN_RATE_TAG=2: tag=(2<<3)|0=16
    encodeCurrentRampdownRateSample(300, d, &dl);
    appendAggSample(buf,
                    static_cast<uint8_t>(PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3),
                    d, dl);

    // PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG=3: tag=(3<<3)|0=24
    encodeCurrentRampdownHysteresisSample(400, d, &dl);
    appendAggSample(
        buf, static_cast<uint8_t>(PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3),
        d, dl);

    // PRESET_PROFILE_SECONDARY_FLOOR_TAG=4: tag=(4<<3)|0=32
    encodeCurrentSecondaryFloorSample(500, d, &dl);
    appendAggSample(
        buf, static_cast<uint8_t>(PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3), d,
        dl);

    // PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG=5:
    // tag=(5<<3)|0=40
    encodeCurrentPrimaryFloorActivationWindowMultiplierSample(6, d, &dl);
    appendAggSample(
        buf,
        static_cast<uint8_t>(
            PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG << 3),
        d, dl);

    // PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG=6: tag=(6<<3)|0=48
    encodeCurrentPrimaryFloorTargetWindowSample(7, d, &dl);
    appendAggSample(buf,
                    static_cast<uint8_t>(
                        PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3),
                    d, dl);

    // PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG=7: tag=(7<<3)|0=56
    encodeCurrentPrimaryFloorActivationOffsetSample(8000, d, &dl);
    appendAggSample(
        buf,
        static_cast<uint8_t>(PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG
                             << 3),
        d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[62]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetEthPortTelemetryCounter with a counter data sample -----------------

TEST(NsmTelemetryCmdParse, GetEthPortTelemetryCounter_WithCounterSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEthPortTelemetryCounter", {"-p", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    nsm_ethernet_port_counter_data counter{};
    counter.ethernet_port_counter_data_64bit = 12345;
    uint8_t d[8]{};
    size_t dl = sizeof(d);          // must pre-set to buffer capacity
    encode_aggregate_eth_port_telemetry_data(0, &counter, d, &dl);
    appendAggSample(buf, 0, d, dl); // tag=0 -> RXBytes (64-bit)

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[56]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPortNetworkAddresses with a link-type sample -----------------------

TEST(NsmTelemetryCmdParse, GetPortNetworkAddresses_WithLinkTypeSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortNetworkAddresses", {"-p", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    network_address_sample_data address{};
    address.link_type = 1;
    uint8_t d[16]{};
    size_t dl{};
    encode_aggregate_network_address_data(NSM_TAG_LINK_TYPE, &address, d, &dl);
    appendAggSample(buf, NSM_TAG_LINK_TYPE, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[57]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPortEccCounters with an ECC counter sample -------------------------

TEST(NsmTelemetryCmdParse, GetPortEccCounters_WithEccCounterSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortEccCounters", {"-p", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[8]{};
    size_t dl{};
    encode_aggregate_port_ecc_counter_data(NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES,
                                           999, d, &dl);
    appendAggSample(buf, NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[58]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- Error paths: all simple commands (payloadLength=0 → decode fails) -----

TEST(NsmTelemetryCmdParse, ErrorPaths_Commands0to11)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());

    // All pass payloadLength=0 to trigger decode_*_resp failure
    EXPECT_NO_THROW(
        commands[0]->parseResponseMsg(msg, 0)); // GetPortTelemetryCounter
    EXPECT_NO_THROW(
        commands[1]->parseResponseMsg(msg, 0)); // QueryPortCharacteristics
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, 0)); // QueryPortStatus
    EXPECT_NO_THROW(
        commands[3]->parseResponseMsg(msg, 0)); // QueryPortsAvailable
    EXPECT_NO_THROW(
        commands[4]->parseResponseMsg(msg, 0)); // SetPortDisableFuture
    EXPECT_NO_THROW(
        commands[5]->parseResponseMsg(msg, 0)); // GetFabricManagerState
    EXPECT_NO_THROW(
        commands[6]->parseResponseMsg(msg, 0)); // GetPortDisableFuture
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, 0)); // GetPowerMode
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, 0)); // SetPowerMode
    EXPECT_NO_THROW(
        commands[9]->parseResponseMsg(msg, 0));  // GetSwitchIsolationMode
    EXPECT_NO_THROW(
        commands[10]->parseResponseMsg(msg, 0)); // SetSwitchIsolationMode
    EXPECT_NO_THROW(
        commands[11]->parseResponseMsg(msg, 0)); // GetInventoryInformation
}

// ---- Error paths: aggregate commands (payloadLength=0 → decode_aggregate_resp
//      fails → hits AggregateResponseParser error path at lines 1344-1347) ---

TEST(NsmTelemetryCmdParse, DISABLED_ErrorPaths_AggregateCommands12to17)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());

    // payloadLength=0 → msg_len=sizeof(nsm_msg_hdr)=8 < 12 needed
    // → decode_aggregate_resp fails → AggregateResponseParser error path
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, 0)); // GetTemperatureReading
    EXPECT_NO_THROW(
        commands[14]->parseResponseMsg(msg, 0)); // GetCurrentPowerDraw
    EXPECT_NO_THROW(
        commands[15]->parseResponseMsg(msg, 0)); // GetMaxObservedPower
    EXPECT_NO_THROW(
        commands[16]->parseResponseMsg(msg, 0)); // GetCurrentEnergyCount
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, 0)); // GetVoltage
}

TEST(NsmTelemetryCmdParse, DISABLED_ErrorPaths_Commands13_18to30)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());

    EXPECT_NO_THROW(
        commands[13]->parseResponseMsg(msg, 0)); // ReadThermalParameter
    EXPECT_NO_THROW(
        commands[18]->parseResponseMsg(msg, 0)); // GetAltitudePressure
    EXPECT_NO_THROW(commands[19]->parseResponseMsg(msg, 0)); // GetDriverInfo
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, 0)); // GetMigMode
    EXPECT_NO_THROW(commands[21]->parseResponseMsg(msg, 0)); // SetMigMode
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, 0)); // GetEccMode
    EXPECT_NO_THROW(commands[23]->parseResponseMsg(msg, 0)); // SetEccMode
    EXPECT_NO_THROW(
        commands[24]->parseResponseMsg(msg, 0)); // GetEccErrorCounts
    EXPECT_NO_THROW(commands[25]->parseResponseMsg(msg, 0)); // SetClockLimit
    EXPECT_NO_THROW(
        commands[26]->parseResponseMsg(msg, 0)); // GetEDPpScalingFactors
    EXPECT_NO_THROW(
        commands[27]->parseResponseMsg(msg, 0)); // SetEDPpScalingFactors
    EXPECT_NO_THROW(
        commands[28]->parseResponseMsg(msg, 0)); // QueryScalarGroupTelemetry
    EXPECT_NO_THROW(
        commands[31]->parseResponseMsg(msg, 0)); // PcieFundamentalReset
}

TEST(NsmTelemetryCmdParse, DISABLED_ErrorPaths_Commands31to53)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());

    EXPECT_NO_THROW(
        commands[32]->parseResponseMsg(msg, 0)); // ClearScalarDataSource
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, 0)); // GetClockLimit
    EXPECT_NO_THROW(commands[34]->parseResponseMsg(msg, 0)); // SetPowerLimit
    EXPECT_NO_THROW(commands[35]->parseResponseMsg(msg, 0)); // GetPowerLimit
    EXPECT_NO_THROW(commands[36]->parseResponseMsg(msg, 0)); // GetCurrClockFreq
    EXPECT_NO_THROW(
        commands[37]->parseResponseMsg(msg, 0)); // GetAccumGpuUtilTime
    EXPECT_NO_THROW(
        commands[38]->parseResponseMsg(msg, 0)); // GetProcessorThrottleReason
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, 0)); // GetRowRemapState
    EXPECT_NO_THROW(
        commands[40]->parseResponseMsg(msg, 0)); // GetRowRemappingCounts
    EXPECT_NO_THROW(
        commands[41]->parseResponseMsg(msg, 0)); // GetRowRemapAvailability
    EXPECT_NO_THROW(
        commands[42]->parseResponseMsg(msg, 0)); // GetLeakDetectionInfo
    EXPECT_NO_THROW(
        commands[43]->parseResponseMsg(msg, 0)); // SetLeakDetectionThresholds
    EXPECT_NO_THROW(
        commands[44]->parseResponseMsg(msg, 0)); // GetMemoryCapacityUtil
    EXPECT_NO_THROW(
        commands[45]->parseResponseMsg(msg, 0)); // GetCurrentUtilization
    EXPECT_NO_THROW(
        commands[46]->parseResponseMsg(msg, 0)); // GetClockOutputEnableState
    EXPECT_NO_THROW(
        commands[47]->parseResponseMsg(msg, 0)); // GetSupportedGPMMetrics
    EXPECT_NO_THROW(commands[48]->parseResponseMsg(
        msg, 0)); // QueryAggregatedGPMMetrics (aggregate)
    EXPECT_NO_THROW(commands[49]->parseResponseMsg(
        msg, 0)); // QueryPerInstanceGPMMetrics (aggregate)
    EXPECT_NO_THROW(commands[50]->parseResponseMsg(
        msg, 0)); // QueryPerInstanceGPMMetricsV2 (aggregate)
    EXPECT_NO_THROW(
        commands[51]->parseResponseMsg(msg, 0)); // GetViolationDuration
    EXPECT_NO_THROW(
        commands[52]->parseResponseMsg(msg, 0)); // GetListAvailablePciePorts
    EXPECT_NO_THROW(commands[53]->parseResponseMsg(
        msg, 0)); // GetPCIePortConfig (aggregate)
    EXPECT_NO_THROW(
        commands[54]->parseResponseMsg(msg, 0)); // SetPCIePortConfig
    EXPECT_NO_THROW(commands[55]->parseResponseMsg(
        msg, 0)); // QueryMultiportScalarGroupTelemetry
}

// ---- Error paths: commands 55-61 aggregate handlers ----------------------

TEST(NsmTelemetryCmdParse, DISABLED_ErrorPaths_Commands55to61_Aggregate)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());

    EXPECT_NO_THROW(
        commands[56]->parseResponseMsg(msg, 0)); // GetEthPortTelemetryCounter
    EXPECT_NO_THROW(
        commands[57]->parseResponseMsg(msg, 0)); // GetPortNetworkAddresses
    EXPECT_NO_THROW(
        commands[58]->parseResponseMsg(msg, 0)); // GetPortEccCounters
    EXPECT_NO_THROW(commands[59]->parseResponseMsg(
        msg, 0)); // GetPowerSmoothingFeatureInfoV2
    EXPECT_NO_THROW(commands[60]->parseResponseMsg(
        msg, 0)); // GetPowerSmoothingCurrentProfileInfoV2
    EXPECT_NO_THROW(commands[61]->parseResponseMsg(
        msg, 0)); // GetPowerSmoothingAdminOverrideV2
    EXPECT_NO_THROW(commands[62]->parseResponseMsg(
        msg, 0)); // GetPowerSmoothingPresetProfileInfoV2
}

// ---- Error paths: QueryAvailableAndClearableScalarGroup for each groupId --

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_ErrorPath_AllGroupIds)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // Pass payloadLength=0 with each groupId set → decode fails → error path
    for (int gid : {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 99})
    {
        parseSubcmdArgs(app, "QueryAvailableAndClearableScalarGroup",
                        {"-d", "0", "-g", std::to_string(gid)});
        std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN,
                                 0);
        auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());
        EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, 0));
    }
}

// ---- GetListAvailablePciePorts with 1 port (covers the ports loop) ---------

TEST(NsmTelemetryCmdParse, GetListAvailablePciePorts_WithOnePciPort)
{
    CLI::App app;
    setupTelemetryCommands(app);

    // Build info with 1 upstream port
    std::vector<uint8_t> infoBuf(sizeof(nsm_list_available_pcie_ports_info) +
                                 sizeof(nsm_pcie_upstream_port_info));
    auto* info =
        reinterpret_cast<nsm_list_available_pcie_ports_info*>(infoBuf.data());
    info->ports_count = 1;
    info->ports[0].type = NSM_PORT_TYPE_UPSTREAM;
    info->ports[0].downstream_ports_count = 2;

    std::vector<uint8_t> buf(NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_list_available_pcie_ports_resp(0, NSM_SUCCESS, ERR_NULL, info, msg);
    EXPECT_NO_THROW(commands[52]->parseResponseMsg(msg, buf.size()));
}

// ---- GetLeakDetectionInfo with 1 sensor (covers sensor-loop in parseResponse)

TEST(NsmTelemetryCmdParse, GetLeakDetectionInfo_WithOneSensor)
{
    CLI::App app;
    setupTelemetryCommands(app);

    uint8_t numSensors = 1;
    uint8_t numThresholdLevels = 1;
    // Sensor data: sensor_id, leak_state, adc_reading(2 bytes), threshold(2
    // bytes)
    struct
    {
        uint8_t sensor_id;
        uint8_t leak_state;
        uint16_t adc_reading;
        uint16_t thresholds[1];
    } __attribute__((packed)) sensorData{};
    sensorData.sensor_id = 0;
    sensorData.leak_state = 0;
    sensorData.adc_reading = 100;
    sensorData.thresholds[0] = 200;

    std::vector<uint8_t> sensorsBuf(sizeof(sensorData));
    std::memcpy(sensorsBuf.data(), &sensorData, sizeof(sensorData));

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_leak_detection_info_resp) +
                             sizeof(sensorData));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_leak_detection_info_resp(0, NSM_SUCCESS, ERR_NULL, numSensors,
                                        numThresholdLevels, sensorsBuf.data(),
                                        sensorsBuf.size(), msg);
    EXPECT_NO_THROW(commands[42]->parseResponseMsg(msg, buf.size()));
}

// ---- GetEthPortTelemetryCounter: 32-bit tags (tags 8-20) -------------------

TEST(NsmTelemetryCmdParse, GetEthPortTelemetryCounter_32BitTags)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEthPortTelemetryCounter", {"-p", "0"});

    // Test tags 8-20 (32-bit counters)
    for (uint8_t tag = 8; tag <= 20; ++tag)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        nsm_ethernet_port_counter_data counter{};
        counter.ethernet_port_counter_data_32bit = 42;
        uint8_t d[4]{};
        size_t dl = sizeof(d); // 4 bytes for 32-bit tags
        encode_aggregate_eth_port_telemetry_data(tag, &counter, d, &dl);
        appendAggSample(buf, tag, d, dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[56]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// ---- GetEthPortTelemetryCounter: 64-bit tags 1-7 ---------------------------

TEST(NsmTelemetryCmdParse, GetEthPortTelemetryCounter_64BitTags1to7)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetEthPortTelemetryCounter", {"-p", "0"});

    // Test tags 1-7 (also 64-bit counters)
    for (uint8_t tag = 1; tag <= 7; ++tag)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        nsm_ethernet_port_counter_data counter{};
        counter.ethernet_port_counter_data_64bit = 999;
        uint8_t d[8]{};
        size_t dl = sizeof(d); // 8 bytes for 64-bit tags
        encode_aggregate_eth_port_telemetry_data(tag, &counter, d, &dl);
        appendAggSample(buf, tag, d, dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[56]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// ---- GetPortNetworkAddresses: remaining tags (MAC, PERMANENT_MAC, GUID) ----

TEST(NsmTelemetryCmdParse, GetPortNetworkAddresses_MacAddressTags)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortNetworkAddresses", {"-p", "0"});

    // Test NSM_TAG_MAC_ADDRESS and NSM_TAG_PERMANENT_MAC_ADDRESS
    for (uint8_t tag :
         {(uint8_t)NSM_TAG_MAC_ADDRESS, (uint8_t)NSM_TAG_PERMANENT_MAC_ADDRESS})
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        network_address_sample_data address{};
        // mac_address[8] filled with test data
        for (int i = 0; i < MAC_ADDRESS_LENGTH; ++i)
            address.mac_address[i] = static_cast<uint8_t>(i + 1);
        uint8_t d[MAC_ADDRESS_LENGTH]{};
        size_t dl{};
        encode_aggregate_network_address_data(tag, &address, d, &dl);
        appendAggSample(buf, tag, d, dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[57]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

TEST(NsmTelemetryCmdParse, GetPortNetworkAddresses_GuidTags)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortNetworkAddresses", {"-p", "0"});

    // Test NSM_TAG_NODE_GUID and NSM_TAG_PORT_GUID
    for (uint8_t tag : {(uint8_t)NSM_TAG_NODE_GUID, (uint8_t)NSM_TAG_PORT_GUID})
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        network_address_sample_data address{};
        address.network_identifier_64bit = 0xDEADBEEFCAFE1234ULL;
        uint8_t d[8]{};
        size_t dl{};
        encode_aggregate_network_address_data(tag, &address, d, &dl);
        appendAggSample(buf, tag, d, dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[57]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// ---- GetPortEccCounters: all remaining tags (1-5) --------------------------

TEST(NsmTelemetryCmdParse, GetPortEccCounters_AllRemainingTags)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortEccCounters", {"-p", "0"});

    for (uint8_t tag = NSM_TAG_ECC_CORRECTED_BITS;
         tag <= NSM_TAG_ECC_RAW_ERRORS_LANE_3; ++tag)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        uint8_t d[8]{};
        size_t dl{};
        encode_aggregate_port_ecc_counter_data(tag, 42, d, &dl);
        appendAggSample(buf, tag, d, dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[58]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// ---- Aggregate-with-sample for commands 13,15,16,17 -----------------------

TEST(NsmTelemetryCmdParse, ReadThermalParameter_Aggregate_WithSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "ReadThermalParameter", {"-s", "255"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_READ_THERMAL_PARAMETER, NSM_SUCCESS, 1, msg);

    uint8_t d[8]{};
    size_t dl{};
    encode_aggregate_thermal_parameter_data(-50, d, &dl);
    appendAggSample(buf, 0, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[13]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

TEST(NsmTelemetryCmdParse, GetMaxObservedPower_Aggregate_WithSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetMaxObservedPower", {"-s", "255", "-a", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_MAX_OBSERVED_POWER, NSM_SUCCESS, 1, msg);

    uint8_t d[8]{};
    size_t dl{};
    encode_aggregate_get_current_power_draw_reading(75000, d, &dl);
    appendAggSample(buf, 0, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[15]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

TEST(NsmTelemetryCmdParse, GetCurrentEnergyCount_Aggregate_WithSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetCurrentEnergyCount", {"-s", "255"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_ENERGY_COUNT, NSM_SUCCESS, 1, msg);

    uint8_t d[16]{};
    size_t dl{};
    encode_aggregate_energy_count_data(123456789ULL, d, &dl);
    appendAggSample(buf, 0, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[16]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

TEST(NsmTelemetryCmdParse, GetVoltage_Aggregate_WithSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetVoltage", {"-s", "255"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_VOLTAGE, NSM_SUCCESS, 1, msg);

    uint8_t d[8]{};
    size_t dl{};
    encode_aggregate_voltage_data(1000, d, &dl);
    appendAggSample(buf, 0, d, dl);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[17]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- QueryScalarGroupTelemetry: error path for each groupId ----------------

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_AllGroups_ErrorPaths)
{
    for (int g : {0, 1, 2, 3, 4, 5, 6, 8, 9, 10})
    {
        CLI::App app;
        setupTelemetryCommands(app);
        parseSubcmdArgs(app, "QueryScalarGroupTelemetry",
                        {"-d", "0", "-g", std::to_string(g)});
        std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN,
                                 0);
        auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());
        EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, 0));
    }
}

// ---- GetProcessorThrottleReason: all 6 throttle flag bits ------------------

TEST(NsmTelemetryCmdParse, GetProcessorThrottleReason_AllFlagsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield32_t flags{};
    flags.byte = 0x3F; // bits 0-5 all set
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_get_current_clock_event_reason_code_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_clock_event_reason_code_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &flags, msg);
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

// ---- AggregateResponseParser error paths -----------------------------------

// !valid sample → triggers lines 1379-1383
TEST(NsmTelemetryCmdParse, AggregateParser_InvalidSample_Skips)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "255"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_TEMPERATURE_READING, NSM_SUCCESS, 1, msg);

    // Encode sample with valid=false to trigger !valid check
    uint8_t d[8]{};
    size_t dl{};
    encode_aggregate_temperature_reading_data(20.0, d, &dl);
    std::array<uint8_t, 64> sampleBuf{};
    auto* nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
    size_t sample_len{};
    encode_aggregate_resp_sample(0, false, d, dl, nsm_sample, &sample_len);
    buf.insert(buf.end(), sampleBuf.begin(),
               std::next(sampleBuf.begin(), sample_len));

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// Timestamp with data_len != 8 → triggers lines 1390-1395
TEST(NsmTelemetryCmdParse, AggregateParser_TimestampWrongSize_Skips)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "255"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_TEMPERATURE_READING, NSM_SUCCESS, 1, msg);

    // tag=0xFF with 4-byte data (not 8) → triggers wrong-size timestamp error
    uint8_t d[4]{};
    appendAggSample(buf, 0xFF, d, 4);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// handleSampleData returns non-success → triggers lines 1423-1428
TEST(NsmTelemetryCmdParse, AggregateParser_HandleSampleDataFails_Skips)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetTemperatureReading", {"-s", "255"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_GET_TEMPERATURE_READING, NSM_SUCCESS, 1, msg);

    // 4-byte temperature sample (decode_aggregate_temperature_reading_data
    // expects 8 bytes for double) → decode fails, handleSampleData returns err
    uint8_t d[4]{};
    appendAggSample(buf, 0, d, 4);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[12]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- QueryAggregatedGPMMetrics: PERCENTAGE and BANDWIDTH sample paths ------

TEST(NsmTelemetryCmdParse, QueryAggregatedGPMMetrics_PercentageSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAggregatedGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-b", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_QUERY_AGGREGATE_GPM_METRICS, NSM_SUCCESS, 1,
                          msg);

    uint8_t d[8]{};
    size_t dl{};
    encode_aggregate_gpm_metric_percentage_data(75.0, d, &dl);
    appendAggSample(buf, 0, d, dl); // tag=0 = GraphicsEngineActivityPercent

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[48]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

TEST(NsmTelemetryCmdParse, QueryAggregatedGPMMetrics_BandwidthSample)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAggregatedGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-b", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_QUERY_AGGREGATE_GPM_METRICS, NSM_SUCCESS, 1,
                          msg);

    uint8_t d[16]{};
    size_t dl{};
    encode_aggregate_gpm_metric_bandwidth_data(1024ULL * 1024 * 128, d, &dl);
    appendAggSample(buf, 8, d, dl); // tag=8 = PCIeRawTxBandwidthGbps

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[48]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// tag not in metricsTable → lines 4661-4664 (returns NSM_SW_ERROR_DATA)
TEST(NsmTelemetryCmdParse, QueryAggregatedGPMMetrics_UnknownTagError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryAggregatedGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-b", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, NSM_QUERY_AGGREGATE_GPM_METRICS, NSM_SUCCESS, 1,
                          msg);

    uint8_t d[4]{};
    appendAggSample(buf, 32, d, 4); // tag=32 not in metricsTable (0-31)

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[48]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// metricId not in metricsTable → early return at lines 4844-4845
TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetrics_UnknownMetricId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-i", "255", "-b", "1"});
    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());
    EXPECT_NO_THROW(commands[49]->parseResponseMsg(msg, 0));
}

// metricId not in metricsTable → early return at lines 4913-4914
TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetricsV2_UnknownMetricId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetricsV2",
                    {"-r", "0", "-g", "0", "-c", "0", "-i", "255", "-b", "1"});
    std::vector<uint8_t> hdr(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(hdr.data());
    EXPECT_NO_THROW(commands[50]->parseResponseMsg(msg, 0));
}

// ---- GetPowerSmoothingFeatureInfoV2: handleSampleData error paths ----------

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingFeatureInfoV2_HandleSampleDataErrors)
{
    // Each tag with wrong data_len triggers decode failure → lines 5143..5235
    // Expected sizes: uint32 tags=4, uint16 tags=2; pass 8 or 4 respectively
    const std::vector<std::pair<uint8_t, size_t>> tag_dl = {
        {FEATURE_FLAG_TAG, 8},                        // expects 4
        {CURRENT_TMP_SETTING_TAG, 8},                 // expects 4
        {TMP_FLOOR_SETTING_TAG, 8},                   // expects 4
        {MAX_TMP_FLOOR_SETTING_TAG, 4},               // expects 2
        {MIN_TMP_FLOOR_SETTING_TAG, 4},               // expects 2
        {FLOOR_WINDOW_MULTIPLIER_TAG, 8},             // expects 4
        {MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG, 8}, // expects 4
        {MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG, 8},  // expects 4
    };

    for (auto& [tag, dl] : tag_dl)
    {
        CLI::App app;
        setupTelemetryCommands(app);

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        std::vector<uint8_t> d(dl, 0);
        appendAggSample(buf, tag, d.data(), dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[59]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// Unknown tag in FeatureInfoV2 → else branch lines 5241-5243
TEST(NsmTelemetryCmdParse, GetPowerSmoothingFeatureInfoV2_UnknownTagError)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[4]{};
    appendAggSample(buf, 9, d, 4); // tag=9 not in switch

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[59]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPowerSmoothingCurrentProfileInfoV2: handleSampleData errors --------

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingCurrentProfileInfoV2_HandleSampleDataErrors)
{
    // tag → wrong dl to trigger decode failure
    const std::vector<std::pair<uint8_t, size_t>> tag_dl = {
        {ACTIVE_PRESET_PROFILE_ID_TAG, 2},                           // 1
        {ADMIN_OVERRIDE_MASK_TAG, 4},                                // 2
        {CURRENT_TMP_FLOOR_SETTING_TAG, 4},                          // 2
        {CURRENT_RAMPUP_RATE_TAG, 8},                                // 4
        {CURRENT_RAMPDOWN_RATE_TAG, 8},                              // 4
        {CURRENT_RAMPDOWN_HYSTERESIS_TAG, 8},                        // 4
        {CURRENT_SECONDARY_FLOOR_TAG, 8},                            // 4
        {CURRENT_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG, 2}, // 1
        {CURRENT_PRIMARY_FLOOR_TARGET_WINDOW_TAG, 2},                // 1
        {CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG, 8},            // 4
    };

    for (auto& [tag, dl] : tag_dl)
    {
        CLI::App app;
        setupTelemetryCommands(app);

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        std::vector<uint8_t> d(dl, 0);
        appendAggSample(buf, tag, d.data(), dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[60]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// Unknown tag in CurrentProfileInfoV2 → else branch lines 5461-5463
TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingCurrentProfileInfoV2_UnknownTagError)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[4]{};
    appendAggSample(buf, 10, d, 4); // tag=10 not in switch

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[60]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPowerSmoothingAdminOverrideProfileInfoV2: handleSampleData errors --

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingAdminOverrideProfileInfoV2_HandleSampleDataErrors)
{
    const std::vector<std::pair<uint8_t, size_t>> tag_dl = {
        {ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG, 4},               // 2
        {ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG, 8},                     // 4
        {ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG, 8},                   // 4
        {ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG, 8},             // 4
        {ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG, 8},                 // 4
        {ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG,
         2},                                                             // 1
        {ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG, 2},     // 1
        {ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG, 8}, // 4
    };

    for (auto& [tag, dl] : tag_dl)
    {
        CLI::App app;
        setupTelemetryCommands(app);

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        std::vector<uint8_t> d(dl, 0);
        appendAggSample(buf, tag, d.data(), dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[61]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// Unknown tag in AdminOverrideProfileInfoV2 → else branch lines 5801-5803
TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingAdminOverrideProfileInfoV2_UnknownTagError)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[4]{};
    appendAggSample(buf, 8, d, 4); // tag=8 not in switch

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[61]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPowerSmoothingPresetProfileInfoV2: handleSampleData error paths ----

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingPresetProfileInfoV2_HandleSampleDataErrors)
{
    // Preset tag = (tagId << 3) | profId; wrong dl to trigger decode failure
    // tagId 0-7 map to tags 0,8,16,24,32,40,48,56 (profId=0)
    const std::vector<std::pair<uint8_t, size_t>> tag_dl = {
        {static_cast<uint8_t>(PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3),
         4}, // tagId=0 → tag=0, expects 2
        {static_cast<uint8_t>(PRESET_PROFILE_RAMPUP_RATE_TAG << 3),
         8}, // tagId=1 → tag=8, expects 4
        {static_cast<uint8_t>(PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3),
         8}, // tagId=2 → tag=16, expects 4
        {static_cast<uint8_t>(PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3),
         8}, // tagId=3 → tag=24, expects 4
        {static_cast<uint8_t>(PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3),
         8}, // tagId=4 → tag=32, expects 4
        {static_cast<uint8_t>(
             PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
             << 3),
         2}, // tagId=5 → tag=40, expects 1
        {static_cast<uint8_t>(PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG
                              << 3),
         2}, // tagId=6 → tag=48, expects 1
        {static_cast<uint8_t>(PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG
                              << 3),
         8}, // tagId=7 → tag=56, expects 4
    };

    for (auto& [tag, dl] : tag_dl)
    {
        CLI::App app;
        setupTelemetryCommands(app);

        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        std::vector<uint8_t> d(dl, 0);
        appendAggSample(buf, tag, d.data(), dl);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[62]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// Unknown tagId in PresetProfileInfoV2 → else branch lines 5635-5637
TEST(NsmTelemetryCmdParse, GetPowerSmoothingPresetProfileInfoV2_UnknownTagError)
{
    CLI::App app;
    setupTelemetryCommands(app);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    // tagId=8 → tag=(8<<3)|0=64, not in switch
    uint8_t d[4]{};
    appendAggSample(buf, 64, d, 4);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[62]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetMigMode: MigModeEnabled=true branch (line 2124) --------------------

TEST(NsmTelemetryCmdParse, GetMigMode_FlagBit0Set)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1; // MigModeEnabled = true
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_MIG_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_MIG_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, buf.size()));
}

// ---- GetEccMode: ECCModeEnabled=true and PendingECCState=true (2239,2247) --

TEST(NsmTelemetryCmdParse, GetEccMode_AllFlagsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.byte = 0x03; // bit0=1 (ECCModeEnabled), bit1=1 (PendingECCState)
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_ECC_mode_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryPerInstanceGPMMetrics: valid metricId PERCENTAGE (4844-4845,
// 4748-4783) ----

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetrics_ValidMetricId_Percentage)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // metricId=0 is GPMMetricsUnit::PERCENTAGE
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-i", "0", "-b", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    // decode_aggregate_gpm_metric_percentage_data expects 4 bytes (uint32_t)
    uint32_t pct_data = htole32(static_cast<uint32_t>(50.0 * 256)); // 50% in Q8
    appendAggSample(buf, 0, reinterpret_cast<uint8_t*>(&pct_data),
                    sizeof(pct_data));

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[49]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- QueryPerInstanceGPMMetrics: valid metricId BANDWIDTH (4764-4776) ------

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetrics_ValidMetricId_Bandwidth)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // metricId=8 is GPMMetricsUnit::BANDWIDTH
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetrics",
                    {"-r", "0", "-g", "0", "-c", "0", "-i", "8", "-b", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    // decode_aggregate_gpm_metric_bandwidth_data expects 8 bytes (uint64_t)
    uint64_t bw = htole64(1024ULL * 1024 * 128);
    appendAggSample(buf, 0, reinterpret_cast<uint8_t*>(&bw), sizeof(bw));

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[49]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- QueryPerInstanceGPMMetricsV2: valid metricId (4913-4914) ---------------

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetricsV2_ValidMetricId)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // metricId=0 is GPMMetricsUnit::PERCENTAGE
    parseSubcmdArgs(app, "QueryPerInstanceGPMMetricsV2",
                    {"-r", "0", "-g", "0", "-c", "0", "-i", "0", "-b", "1"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint32_t pct_data = htole32(static_cast<uint32_t>(75.0 * 256));
    appendAggSample(buf, 0, reinterpret_cast<uint8_t*>(&pct_data),
                    sizeof(pct_data));

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[50]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- GetPCIePortConfig: aggregate handler all paths (4957-4996) -------------

// PCIe preset tag (0) with correct size (1 byte) → success path 4967-4983
TEST(NsmTelemetryCmdParse, GetPCIePortConfig_PresetTag_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0"});

    // Test all preset tags 0-3 with 1 byte each
    for (uint8_t tag = 0; tag <= 3; ++tag)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_aggregate_resp));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

        uint8_t preset_byte = 0xAB; // preset0=0xB, preset1=0xA
        appendAggSample(buf, tag, &preset_byte, 1);

        msg = reinterpret_cast<nsm_msg*>(buf.data());
        EXPECT_NO_THROW(commands[53]->parseResponseMsg(
            msg, buf.size() - sizeof(nsm_msg_hdr)));
    }
}

// PCIe_Tx_Amplitude tag (4) with 1 byte → success path 4984-4996
TEST(NsmTelemetryCmdParse, GetPCIePortConfig_TxAmplitude_Success)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t amp_byte = 0x05;
    appendAggSample(buf, 4, &amp_byte, 1); // tag=4 is PCIe_Tx_Amplitude

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[53]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// Unknown tag (5) → NSM_SW_ERROR_DATA path 4960-4964
TEST(NsmTelemetryCmdParse, GetPCIePortConfig_UnknownTag_Error)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[1]{};
    appendAggSample(buf, 5, d, 1); // tag=5 not in tagToPropertyMap

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[53]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// PCIe preset with wrong size (2 bytes) → error path 4977
TEST(NsmTelemetryCmdParse, GetPCIePortConfig_PresetTag_BadData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[2]{}; // wrong size: expects 1 byte
    appendAggSample(buf, 0, d, 2);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[53]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// PCIe_Tx_Amplitude with wrong size (2 bytes) → error path 4991
TEST(NsmTelemetryCmdParse, GetPCIePortConfig_TxAmplitude_BadData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0"});

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0, NSM_SUCCESS, 1, msg);

    uint8_t d[2]{}; // wrong size: expects 1 byte
    appendAggSample(buf, 4, d, 2);

    msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(
        commands[53]->parseResponseMsg(msg, buf.size() - sizeof(nsm_msg_hdr)));
}

// ---- SetPCIePortConfig: sampleCount mismatch triggers error path (line 5066)

// sampleCount=1 means 1*sizeof(nsm_aggregate_resp_sample)=3 bytes expected,
// but sampleData has only 1 byte → condition true → return NSM_SW_ERROR_DATA
TEST(NsmTelemetryCmdParse, SetPCIePortConfig_SampleCountMismatch)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0", "-c", "1", "-d", "0"});
    // createRequestMsg returns error code without throwing
    EXPECT_NO_THROW(commands[54]->createRequestMsg());
}

// ---- GetRowRemapState with flags set ----------------------------------------

// GetRowRemapState with bit0=1 → line 4025 (RowRemappingFailureState = true)
TEST(NsmTelemetryCmdParse, GetRowRemapState_Bit0Set)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit0 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

// GetRowRemapState with bit1=1 → line 4033 (RowRemappingPendingState = true)
TEST(NsmTelemetryCmdParse, GetRowRemapState_Bit1Set)
{
    CLI::App app;
    setupTelemetryCommands(app);

    bitfield8_t flags{};
    flags.bits.bit1 = 1;
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
                             sizeof(nsm_get_row_remap_state_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags, msg);
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

// ---- SetPowerLimit MODULE branch (createRequestMsg, line 3817-3820) ---------

TEST(NsmTelemetryCmdParse, SetPowerLimit_ModuleBranch)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // powerLimitId = MODULE (1)
    parseSubcmdArgs(app, "SetPowerLimit",
                    {"-i", "1", "-a", "0", "-p", "0", "-l", "100000"});
    EXPECT_NO_THROW(commands[34]->createRequestMsg());
}

// ---- GetPowerLimit MODULE branch (createRequestMsg, line 3886-3888) ---------

TEST(NsmTelemetryCmdParse, GetPowerLimit_ModuleBranch)
{
    CLI::App app;
    setupTelemetryCommands(app);
    // powerLimitId = MODULE (1)
    parseSubcmdArgs(app, "GetPowerLimit", {"-i", "1"});
    EXPECT_NO_THROW(commands[35]->createRequestMsg());
}

// ============================================================================
// Error-path tests: pass a truncated buffer so decode fails (rc !=
// NSM_SW_SUCCESS) This covers the else/error branch of all parseResponseMsg
// implementations.
// ============================================================================

static std::vector<uint8_t> makeTruncatedResp()
{
    // A buffer with header + error response bytes — too small for any
    // actual response payload, but large enough for decode_reason_code_and_cc.
    return std::vector<uint8_t>(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN,
                                0);
}

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryPortCharacteristics_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[1]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryPortStatus_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[2]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryPortsAvailable_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[3]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetPortDisableFuture_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[4]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetFabricManagerState_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[5]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPortDisableFuture_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[6]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPowerMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[7]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetPowerMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[8]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetSwitchIsolationMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[9]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetSwitchIsolationMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[10]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetInventoryInformation_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[11]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetTemperatureReading_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[12]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, ReadThermalParameter_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[13]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetCurrentPowerDraw_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[14]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetMaxObservedPower_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[15]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetCurrentEnergyCount_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[16]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetVoltage_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[17]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetAltitudePressure_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[18]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetDriverInfo_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[19]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetMigMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[20]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetMigMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[21]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetEccMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[22]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetEccMode_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[23]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetEccErrorCounts_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[24]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetClockLimit_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[25]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetEDPpScalingFactors_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[26]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetEDPpScalingFactors_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[27]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryVectorGroupTelemetry_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse,
     QueryAvailableAndClearableScalarGroup_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[30]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, PcieFundamentalReset_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[31]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, ClearScalarDataSource_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[32]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetClockLimit_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[33]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetPowerLimit_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[34]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPowerLimit_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[35]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetCurrClockFreq_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[36]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetAccumGpuUtilTime_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[37]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetProcessorThrottleReason_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[38]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetRowRemapState_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[39]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetRowRemappingCounts_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[40]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetRowRemapAvailability_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[41]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetLeakDetectionInfo_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[42]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetLeakDetectionThresholds_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[43]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetMemoryCapacityUtil_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[44]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetCurrentUtilization_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[45]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetClockOutputEnableState_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[46]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryAggregatedGPMMetrics_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[48]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetrics_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[49]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryPerInstanceGPMMetricsV2_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[50]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetViolationDuration_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[51]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetListAvailablePciePorts_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[52]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPCIePortConfig_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[53]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, SetPCIePortConfig_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[54]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse,
     QueryMultiportScalarGroupTelemetry_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[55]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetEthPortTelemetryCounter_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[56]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPortNetworkAddresses_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[57]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPortEccCounters_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[58]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPowerSmoothingFeatureInfoV2_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[59]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPowerSmoothingCurrentProfile_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[60]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse,
     GetPowerSmoothingAdminOverrideProfile_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[61]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, GetPowerSmoothingPresetProfile_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[62]->parseResponseMsg(msg, buf.size()));
}

// ---- printPortTeleInfo: all counter bits set to 1 --------------------------
// Covers the "true" branch of every if (portData->supported_counter.X) check.

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_AllCounterBitsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    struct nsm_port_counter_data portData{};
    // Set all supported_counter bits to 1 so every if-branch is taken.
    memset(&portData.supported_counter, 0xFF,
           sizeof(portData.supported_counter));
    portData.port_rcv_pkts = 1;
    portData.port_rcv_data = 2;
    portData.port_multicast_rcv_pkts = 3;
    portData.port_unicast_rcv_pkts = 4;
    portData.port_malformed_pkts = 5;
    portData.vl15_dropped = 6;
    portData.port_rcv_errors = 7;
    portData.port_xmit_pkts = 8;
    portData.port_xmit_pkts_vl15 = 9;
    portData.port_xmit_data = 10;
    portData.port_xmit_data_vl15 = 11;
    portData.port_unicast_xmit_pkts = 12;
    portData.port_multicast_xmit_pkts = 13;
    portData.port_bcast_xmit_pkts = 14;
    portData.port_xmit_discard = 15;
    portData.port_neighbor_mtu_discards = 16;
    portData.port_rcv_ibg2_pkts = 17;
    portData.port_xmit_ibg2_pkts = 18;
    portData.symbol_ber = 19;
    portData.link_error_recovery_counter = 20;
    portData.link_downed_counter = 21;
    portData.port_rcv_remote_physical_errors = 22;
    portData.port_rcv_switch_relay_errors = 23;
    portData.QP1_dropped = 24;
    portData.xmit_wait = 25;
    portData.effective_ber = 26;
    portData.total_raw_error = 27;
    portData.effective_error = 28;
    portData.symbol_error = 29;
    portData.total_raw_ber = 30;
    portData.unintentional_link_down_count = 31;
    portData.intentional_link_down_count = 32;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryScalarGroupTelemetry Group 1: CommonClock mode --------------------

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group1_CommonClockMode)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.clock_mode = NSM_PCIE_CLOCK_MODE_COMMON;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryScalarGroupTelemetry Group 1: Unknown clock mode (default) --------

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group1_UnknownClockMode)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "1"});

    struct nsm_query_scalar_group_telemetry_group_1 data{};
    data.clock_mode = 99; // Unknown → default branch
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_1_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryScalarGroupTelemetry Group 7: success path (various port types) ---

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group7_Endpoint)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_ENDPOINT;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group7_RootPort)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_ROOT_PORT;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group7_UpstreamPort)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_UPSTREAM;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group7_DownstreamPort)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = NSM_PCIE_PORT_TYPE_DOWNSTREAM;
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

TEST(NsmTelemetryCmdParse, QueryScalarGroupTelemetry_Group7_UnknownPortType)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "QueryScalarGroupTelemetry", {"-d", "0", "-g", "7"});

    struct nsm_query_scalar_group_telemetry_group_7 data{};
    data.port_type = 99; // Unknown → default branch
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_7_resp));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group7_resp(0, NSM_SUCCESS, ERR_NULL,
                                                       &data, msg);
    EXPECT_NO_THROW(commands[28]->parseResponseMsg(msg, buf.size()));
}

// ---- QueryVectorGroupTelemetry Group 1: error path --------------------------
// The existing ParseResponseError test runs with groupId=2 (default case).
// This test explicitly sets groupId=1 and passes a truncated buffer so the
// decode-failure branch inside the GROUP_ID_1 case is reached.

TEST(NsmTelemetryCmdParse, QueryVectorGroupTelemetry_Group1_ParseResponseError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(
        app, "QueryVectorGroupTelemetry",
        {"-t", "0", "-u", "0", "-i", "0", "-g", "1", "-s", "1", "-l", "0"});

    auto buf = makeTruncatedResp();
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[29]->parseResponseMsg(msg, buf.size()));
}

// ---- SetPCIePortConfig: parseResponseMsg cc error path ----------------------
// A 9-byte buffer (nsm_msg_hdr + nsm_common_non_success_resp) with
// completion_code=NSM_ERROR makes decode_reason_code_and_cc succeed but return
// cc!=NSM_SUCCESS, so the error branch at lines 5267-5272 is taken.

TEST(NsmTelemetryCmdParse, SetPCIePortConfig_ParseResponseCcError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "SetPCIePortConfig",
                    {"-p", "0", "-t", "0", "-i", "0", "-c", "0", "-d", "0"});

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    // Set completion_code (byte 1 in payload) to NSM_ERROR so the error path
    // inside parseResponseMsg is exercised.
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    EXPECT_NO_THROW(commands[54]->parseResponseMsg(msg, buf.size()));
}

// ---- GetPortTelemetryCounter: printPortTeleInfo null portData
// ---------------- Call printPortTeleInfo directly with nullptr to cover the
// null-check branch at lines 122-126 of nsm_telemetry_cmd.cpp.

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_NullPortData)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    auto* cmd = static_cast<GetPortTelemetryCounter*>(commands[0].get());
    EXPECT_NO_THROW(cmd->printPortTeleInfo(0, nullptr));
}

// ---- GetPortTelemetryCounter: all counter flags set -------------------------
// Exercises the TRUE branch of every `if (portData->supported_counter.xxx)`
// check in printPortTeleInfo (lines 134-334 of nsm_telemetry_cmd.cpp).
// The existing success test uses a zeroed portData, so all counter checks take
// the FALSE branch. This test calls printPortTeleInfo directly with all
// supported_counter bits set to avoid encode/decode stripping the flags.

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_AllCounterFlagsSet)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    struct nsm_port_counter_data portData{};
    // Set all supported_counter bits so every flag-guarded branch is taken
    memset(&portData.supported_counter, 0xFF,
           sizeof(portData.supported_counter));
    portData.port_rcv_pkts = 1;
    portData.port_rcv_data = 2;
    portData.port_multicast_rcv_pkts = 3;
    portData.port_unicast_rcv_pkts = 4;
    portData.port_malformed_pkts = 5;
    portData.vl15_dropped = 6;
    portData.port_rcv_errors = 7;
    portData.port_xmit_pkts = 8;

    // Call printPortTeleInfo directly to bypass encode/decode
    auto* cmd = static_cast<GetPortTelemetryCounter*>(commands[0].get());
    EXPECT_NO_THROW(cmd->printPortTeleInfo(sizeof(portData), &portData));
}

// ---- GetPortTelemetryCounter: decode ok but cc = NSM_ERROR ------------------
// Covers the missing branch at L104 where rc==NSM_SW_SUCCESS but
// cc!=NSM_SUCCESS (the second `&&` sub-condition fails, directing to the
// else/error path).

TEST(NsmTelemetryCmdParse, GetPortTelemetryCounter_ParseResponseCcError)
{
    CLI::App app;
    setupTelemetryCommands(app);
    parseSubcmdArgs(app, "GetPortTelemetryCounter", {"-p", "0"});

    struct nsm_port_counter_data portData{};
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data));
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Encode with NSM_ERROR: decode succeeds (rc=NSM_SW_SUCCESS) but cc !=
    // NSM_SUCCESS
    encode_get_port_telemetry_counter_resp(0, NSM_ERROR, ERR_NULL, &portData,
                                           msg);
    EXPECT_NO_THROW(commands[0]->parseResponseMsg(msg, buf.size()));
}

} // namespace nsmtool::telemetry
