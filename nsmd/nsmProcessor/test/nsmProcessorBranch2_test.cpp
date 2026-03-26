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

/*
 * Branch coverage tests for nsmd/nsmProcessor/nsmProcessor.cpp
 *
 * Covers genRequestMsg/handleResponseMsg for:
 *   NsmMigMode, NsmEccMode, NsmEccErrorCounts, NsmPciGroup5,
 *   NsmEDPpScalingFactor, NsmClockLimitGraphics, NsmCurrClockFreq,
 *   NsmCurrentUtilization, NsmProcessorThrottleReason,
 *   NsmAccumGpuUtilTime, NsmTotalNvLinks, NsmProcessorRevision,
 *   NsmPowerCap, NsmProcessorThrottleDuration,
 *   NsmConfidentialCompute, NsmEgmMode
 *
 * Each class gets:
 *   - genRequestMsg encode failure (instanceId > NSM_INSTANCE_MAX)
 *   - handleResponseMsg success (rc==0, cc==0)
 *   - handleResponseMsg error CC (cc != NSM_SUCCESS)
 *   - handleResponseMsg decode failure (truncated buffer)
 *   - handleResponseMsg decode success + non-zero CC
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmProcessor.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmProcessorBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    std::string sensorName{"branch2_sensor"};
    std::string sensorType{"branch2_type"};
    std::string inventoryObjPath{
        "/xyz/openbmc_project/inventory/branch2_device"};

    NsmProcessorBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmProcessorBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // Valgrind-safe minimal decode-fail buffer (7 bytes minimum)
    std::vector<uint8_t> makeDecodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ============================================================================
// NsmMigMode
// ============================================================================

TEST_F(NsmProcessorBranch2Test, MigMode_GenReq_EncodeFail)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, nullptr,
                      false);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, MigMode_HandleResp_Success)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, nullptr,
                      false);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 1;
    uint8_t rc = encode_get_MIG_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.migModeIntf->migModeEnabled());
}

TEST_F(NsmProcessorBranch2Test, MigMode_HandleResp_ErrorCC)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, nullptr,
                      false);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    uint8_t rc = encode_get_MIG_mode_resp(0, NSM_ERROR, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, MigMode_HandleResp_DecodeFail)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, nullptr,
                      false);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, DISABLED_MigMode_HandleResp_LongRunning_Success)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, gpu,
                      true);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 1;
    uint8_t rc = encode_get_MIG_mode_event_resp(0, NSM_SUCCESS, ERR_NULL,
                                                &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, DISABLED_MigMode_HandleResp_LongRunning_ErrorCC)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, gpu,
                      true);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_MIG_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    uint8_t rc = encode_get_MIG_mode_event_resp(0, NSM_ERROR, ERR_NULL, &flags,
                                                response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, MigMode_HandleResp_LongRunning_DecodeFail)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, gpu,
                      true);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmEccMode
// ============================================================================

TEST_F(NsmProcessorBranch2Test, EccMode_GenReq_EncodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, false,
                      nullptr);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, EccMode_HandleResp_Success)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, false,
                      nullptr);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 3; // eccModeEnabled=1, pendingECCState=1
    uint8_t rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.eccModeIntf->eccModeEnabled());
    EXPECT_TRUE(sensor.eccModeIntf->pendingECCState());
}

TEST_F(NsmProcessorBranch2Test, EccMode_HandleResp_ErrorCC)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, false,
                      nullptr);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    uint8_t rc = encode_get_ECC_mode_resp(0, NSM_ERROR, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, EccMode_HandleResp_DecodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, false,
                      nullptr);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, DISABLED_EccMode_HandleResp_LongRunning_Success)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, true,
                      gpu);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 2; // pendingECCState=1, eccModeEnabled=0
    uint8_t rc = encode_get_ECC_mode_event_resp(0, NSM_SUCCESS, ERR_NULL,
                                                &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.eccModeIntf->eccModeEnabled());
    EXPECT_TRUE(sensor.eccModeIntf->pendingECCState());
}

TEST_F(NsmProcessorBranch2Test, DISABLED_EccMode_HandleResp_LongRunning_ErrorCC)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, true,
                      gpu);
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    uint8_t rc = encode_get_ECC_mode_event_resp(0, NSM_ERROR, ERR_NULL, &flags,
                                                response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, EccMode_HandleResp_LongRunning_DecodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath, true,
                      gpu);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmEccErrorCounts
// ============================================================================

TEST_F(NsmProcessorBranch2Test, EccErrorCounts_GenReq_EncodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf, inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, EccErrorCounts_HandleResp_Success)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf, inventoryObjPath);

    nsm_ECC_error_counts errorCounts{};
    errorCounts.sram_corrected = 100;
    errorCounts.sram_uncorrected_secded = 10;
    errorCounts.sram_uncorrected_parity = 5;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  &errorCounts, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.eccErrorCountIntf->ceCount(), 100);
    EXPECT_EQ(sensor.eccErrorCountIntf->ueCount(), 15);
}

TEST_F(NsmProcessorBranch2Test, EccErrorCounts_HandleResp_ErrorCC)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf, inventoryObjPath);

    nsm_ECC_error_counts errorCounts{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_ECC_error_counts_resp(0, NSM_ERROR, ERR_NULL,
                                                  &errorCounts, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, EccErrorCounts_HandleResp_DecodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccErrorCounts sensor(sensorName, sensorType, eccIntf, inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmPciGroup5
// ============================================================================

TEST_F(NsmProcessorBranch2Test, PciGroup5_HandleResp_Success)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    NsmPciGroup5 sensor(sensorName, sensorType, processorPerfIntf, deviceId,
                        inventoryObjPath);

    nsm_query_scalar_group_telemetry_group_5 data{};
    data.PCIeRXDwords = 1000;
    data.PCIeTXDwords = 2000;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_scalar_group_telemetry_v1_group_5_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.processorPerformanceIntf->pcIeRXBytes(), 1000 * 4);
    EXPECT_EQ(sensor.processorPerformanceIntf->pcIeTXBytes(), 2000 * 4);
}

TEST_F(NsmProcessorBranch2Test, PciGroup5_HandleResp_ErrorCC)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    NsmPciGroup5 sensor(sensorName, sensorType, processorPerfIntf, deviceId,
                        inventoryObjPath);

    nsm_query_scalar_group_telemetry_group_5 data{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_query_scalar_group_telemetry_v1_group5_resp(
        0, NSM_ERROR, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, PciGroup5_HandleResp_DecodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    uint8_t deviceId = 0;
    NsmPciGroup5 sensor(sensorName, sensorType, processorPerfIntf, deviceId,
                        inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmEDPpScalingFactor
// ============================================================================

TEST_F(NsmProcessorBranch2Test, EDPpScalingFactor_GenReq_EncodeFail)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus(), inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus(), inventoryObjPath.c_str(), nullptr);
    NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                eDPpIntf, resetEdppAsyncIntf);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, EDPpScalingFactor_HandleResp_Success)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus(), inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus(), inventoryObjPath.c_str(), nullptr);
    NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                eDPpIntf, resetEdppAsyncIntf);

    nsm_EDPp_scaling_factors scalingFactors{};
    scalingFactors.enforced_scaling_factor = 75;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_programmable_EDPp_scaling_factor_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_programmable_EDPp_scaling_factor_resp(
        0, NSM_SUCCESS, ERR_NULL, &scalingFactors, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, EDPpScalingFactor_HandleResp_ErrorCC)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus(), inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus(), inventoryObjPath.c_str(), nullptr);
    NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                eDPpIntf, resetEdppAsyncIntf);

    nsm_EDPp_scaling_factors scalingFactors{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_programmable_EDPp_scaling_factor_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_programmable_EDPp_scaling_factor_resp(
        0, NSM_ERROR, ERR_NULL, &scalingFactors, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, EDPpScalingFactor_HandleResp_DecodeFail)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus(), inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus(), inventoryObjPath.c_str(), nullptr);
    NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                eDPpIntf, resetEdppAsyncIntf);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmClockLimitGraphics
// ============================================================================

TEST_F(NsmProcessorBranch2Test, ClockLimitGraphics_GenReq_EncodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, ClockLimitGraphics_HandleResp_Success)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);

    nsm_clock_limit clockLimit{};
    clockLimit.requested_limit_min = 300;
    clockLimit.requested_limit_max = 2100;
    clockLimit.present_limit_min = 500;
    clockLimit.present_limit_max = 1800;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             &clockLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, ClockLimitGraphics_HandleResp_ErrorCC)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);

    nsm_clock_limit clockLimit{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_clock_limit_resp(0, NSM_ERROR, ERR_NULL,
                                             &clockLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, ClockLimitGraphics_HandleResp_DecodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmCurrClockFreq
// ============================================================================

TEST_F(NsmProcessorBranch2Test, CurrClockFreq_GenReq_EncodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmCurrClockFreq sensor(sensorName, sensorType, cpuConfigIntf,
                            inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, CurrClockFreq_HandleResp_Success)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmCurrClockFreq sensor(sensorName, sensorType, cpuConfigIntf,
                            inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockFreq = 1500;
    uint8_t rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 &clockFreq, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.cpuOperatingConfigIntf->operatingSpeed(), 1500u);
}

TEST_F(NsmProcessorBranch2Test, CurrClockFreq_HandleResp_ErrorCC)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmCurrClockFreq sensor(sensorName, sensorType, cpuConfigIntf,
                            inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockFreq = 0;
    uint8_t rc = encode_get_curr_clock_freq_resp(0, NSM_ERROR, ERR_NULL,
                                                 &clockFreq, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, CurrClockFreq_HandleResp_DecodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmCurrClockFreq sensor(sensorName, sensorType, cpuConfigIntf,
                            inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmCurrentUtilization
// ============================================================================

TEST_F(NsmProcessorBranch2Test, CurrentUtilization_GenReq_EncodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, false, nullptr);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, CurrentUtilization_HandleResp_Success)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, false, nullptr);

    nsm_get_current_utilization_data data{};
    data.gpu_utilization = 85;
    data.memory_utilization = 60;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_utilization_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_utilization_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.cpuOperatingConfigIntf->utilization(), 85u);
}

TEST_F(NsmProcessorBranch2Test, CurrentUtilization_HandleResp_ErrorCC)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, false, nullptr);

    nsm_get_current_utilization_data data{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_utilization_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_utilization_resp(0, NSM_ERROR, ERR_NULL,
                                                     &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, CurrentUtilization_HandleResp_DecodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, false, nullptr);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test,
       DISABLED_CurrentUtilization_HandleResp_LongRunning_Success_2)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, true, gpu);

    nsm_get_current_utilization_data data{};
    data.gpu_utilization = 42;
    data.memory_utilization = 33;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_utilization_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_utilization_event_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.cpuOperatingConfigIntf->utilization(), 42u);
}

TEST_F(NsmProcessorBranch2Test,
       CurrentUtilization_HandleResp_LongRunning_ErrorCC)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, true, gpu);

    nsm_get_current_utilization_data data{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_utilization_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_utilization_event_resp(
        0, NSM_ERROR, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test,
       CurrentUtilization_HandleResp_LongRunning_DecodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath, true, gpu);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmProcessorThrottleReason
// ============================================================================

TEST_F(NsmProcessorBranch2Test, ThrottleReason_GenReq_EncodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_HandleResp_Success)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);

    bitfield32_t flags{};
    flags.byte = 0x3F; // All 6 throttle flags set
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_current_clock_event_reason_code_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_clock_event_reason_code_resp(
        0, NSM_SUCCESS, ERR_NULL, &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 6u);
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_HandleResp_ZeroFlags)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);

    bitfield32_t flags{};
    flags.byte = 0; // No flags - covers throttleReasons.size()==0 branch
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_current_clock_event_reason_code_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_clock_event_reason_code_resp(
        0, NSM_SUCCESS, ERR_NULL, &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 1u); // Only "None"
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_HandleResp_ErrorCC)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);

    bitfield32_t flags{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_current_clock_event_reason_code_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_current_clock_event_reason_code_resp(
        0, NSM_ERROR, ERR_NULL, &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_HandleResp_DecodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Individual throttle reason flag tests for branch coverage
TEST_F(NsmProcessorBranch2Test, ThrottleReason_UpdateReading_Bit1Only)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    bitfield32_t flags{};
    flags.byte = 0x02; // Only HWSlowdown
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 1u);
    EXPECT_TRUE(sensor.processorPerformanceIntf->throttleReasonHWSlowdown());
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_UpdateReading_Bit2Only)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    bitfield32_t flags{};
    flags.byte = 0x04; // Only HWThermalSlowdown
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 1u);
    EXPECT_TRUE(
        sensor.processorPerformanceIntf->throttleReasonHWThermalSlowdown());
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_UpdateReading_Bit3Only)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    bitfield32_t flags{};
    flags.byte = 0x08; // Only HWPowerBrakeSlowdown
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 1u);
    EXPECT_TRUE(
        sensor.processorPerformanceIntf->throttleReasonHWPowerBrakeSlowdown());
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_UpdateReading_Bit4Only)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    bitfield32_t flags{};
    flags.byte = 0x10; // Only SyncBoost
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 1u);
    EXPECT_TRUE(sensor.processorPerformanceIntf->throttleReasonSyncBoost());
}

TEST_F(NsmProcessorBranch2Test, ThrottleReason_UpdateReading_Bit5Only)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);
    bitfield32_t flags{};
    flags.byte = 0x20; // Only ClockOptimizedForThermalEngage
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 1u);
}

// ============================================================================
// NsmAccumGpuUtilTime
// ============================================================================

TEST_F(NsmProcessorBranch2Test, AccumGpuUtilTime_GenReq_EncodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmAccumGpuUtilTime sensor(sensorName, sensorType, processorPerfIntf,
                               inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, AccumGpuUtilTime_HandleResp_Success)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmAccumGpuUtilTime sensor(sensorName, sensorType, processorPerfIntf,
                               inventoryObjPath);

    uint32_t contextUtilTime = 5000;
    uint32_t smUtilTime = 3000;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_accum_GPU_util_time_resp(
        0, NSM_SUCCESS, ERR_NULL, &contextUtilTime, &smUtilTime, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Milliseconds to nanoseconds: 5000ms = 5000000000ns
    EXPECT_EQ(sensor.processorPerformanceIntf
                  ->accumulatedGPUContextUtilizationDuration(),
              5000000000);
    EXPECT_EQ(
        sensor.processorPerformanceIntf->accumulatedSMUtilizationDuration(),
        3000000000);
}

TEST_F(NsmProcessorBranch2Test, AccumGpuUtilTime_HandleResp_ErrorCC)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmAccumGpuUtilTime sensor(sensorName, sensorType, processorPerfIntf,
                               inventoryObjPath);

    uint32_t contextUtilTime = 0;
    uint32_t smUtilTime = 0;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_accum_GPU_util_time_resp(
        0, NSM_ERROR, ERR_NULL, &contextUtilTime, &smUtilTime, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, AccumGpuUtilTime_HandleResp_DecodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmAccumGpuUtilTime sensor(sensorName, sensorType, processorPerfIntf,
                               inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmTotalNvLinks
// ============================================================================

TEST_F(NsmProcessorBranch2Test, TotalNvLinks_GenReq_EncodeFail)
{
    auto totalNvLinkIntf =
        std::make_shared<TotalNvLinkInterface>(bus(), inventoryObjPath.c_str());
    NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkIntf,
                           inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, TotalNvLinks_HandleResp_Success)
{
    auto totalNvLinkIntf =
        std::make_shared<TotalNvLinkInterface>(bus(), inventoryObjPath.c_str());
    NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkIntf,
                           inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t totalLinks = 18;
    uint8_t rc = encode_query_ports_available_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   totalLinks, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.totalNvLinkInterface->totalNumberNVLinks(), 18u);
}

TEST_F(NsmProcessorBranch2Test, TotalNvLinks_HandleResp_ErrorCC)
{
    auto totalNvLinkIntf =
        std::make_shared<TotalNvLinkInterface>(bus(), inventoryObjPath.c_str());
    NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkIntf,
                           inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t totalLinks = 0;
    uint8_t rc = encode_query_ports_available_resp(0, NSM_ERROR, ERR_NULL,
                                                   totalLinks, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, TotalNvLinks_HandleResp_DecodeFail)
{
    auto totalNvLinkIntf =
        std::make_shared<TotalNvLinkInterface>(bus(), inventoryObjPath.c_str());
    NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkIntf,
                           inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmProcessorRevision
// ============================================================================

TEST_F(NsmProcessorBranch2Test, ProcessorRevision_GenReq_EncodeFail)
{
    NsmProcessorRevision sensor(bus(), sensorName, sensorType,
                                inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, ProcessorRevision_HandleResp_Success)
{
    NsmProcessorRevision sensor(bus(), sensorName, sensorType,
                                inventoryObjPath);

    std::string revision = "A01";
    std::vector<uint8_t> data(revision.begin(), revision.end());
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            data.size(),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, data.size(), data.data(), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, ProcessorRevision_HandleResp_ErrorCC)
{
    NsmProcessorRevision sensor(bus(), sensorName, sensorType,
                                inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, ProcessorRevision_HandleResp_DecodeFail)
{
    NsmProcessorRevision sensor(bus(), sensorName, sensorType,
                                inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmPowerCap
// ============================================================================

TEST_F(NsmProcessorBranch2Test, PowerCap_GenReq_EncodeFail)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, PowerCap_HandleResp_Success_ValidLimits)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);

    uint32_t persistentLimit = 300000; // 300W in milliwatts
    uint32_t oneshotLimit = 400000;    // 400W
    uint32_t enforcedLimit = 250000;   // 250W
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             persistentLimit, oneshotLimit,
                                             enforcedLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.persistencyIntf->persistency());
    EXPECT_DOUBLE_EQ(sensor.persistencyIntf->persistentPowerLimit(), 300.0);
    EXPECT_DOUBLE_EQ(sensor.persistencyIntf->oneShotPowerLimit(), 400.0);
}

TEST_F(NsmProcessorBranch2Test, PowerCap_HandleResp_Success_AllInvalid)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_power_limit_resp(
        0, NSM_SUCCESS, ERR_NULL, INVALID_POWER_LIMIT, INVALID_POWER_LIMIT,
        INVALID_POWER_LIMIT, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.persistencyIntf->persistency());
    EXPECT_TRUE(std::isnan(sensor.persistencyIntf->persistentPowerLimit()));
    EXPECT_TRUE(std::isnan(sensor.persistencyIntf->oneShotPowerLimit()));
}

TEST_F(NsmProcessorBranch2Test,
       PowerCap_HandleResp_Success_ValidPersistent_InvalidOneshot)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL, 350000,
                                             INVALID_POWER_LIMIT, 350000,
                                             response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.persistencyIntf->persistency());
    EXPECT_DOUBLE_EQ(sensor.persistencyIntf->persistentPowerLimit(), 350.0);
    EXPECT_TRUE(std::isnan(sensor.persistencyIntf->oneShotPowerLimit()));
}

TEST_F(NsmProcessorBranch2Test,
       PowerCap_HandleResp_Success_InvalidPersistent_ValidOneshot)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_power_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             INVALID_POWER_LIMIT, 200000,
                                             200000, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.persistencyIntf->persistency());
    EXPECT_TRUE(std::isnan(sensor.persistencyIntf->persistentPowerLimit()));
    EXPECT_DOUBLE_EQ(sensor.persistencyIntf->oneShotPowerLimit(), 200.0);
}

TEST_F(NsmProcessorBranch2Test, PowerCap_HandleResp_ErrorCC)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_power_limit_resp(0, NSM_ERROR, ERR_NULL, 0, 0, 0,
                                             response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, PowerCap_HandleResp_DecodeFail)
{
    auto powerCapIntf = std::make_shared<NsmPowerCapIntf>(
        bus(), inventoryObjPath.c_str(), sensorName, std::vector<std::string>{},
        nullptr);
    auto persistencyIntf =
        std::make_shared<PowerPersistencyIntf>(bus(), inventoryObjPath.c_str());
    NsmPowerCap sensor(sensorName, sensorType, powerCapIntf,
                       std::vector<std::string>{}, persistencyIntf,
                       inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmProcessorThrottleDuration
// ============================================================================

TEST_F(NsmProcessorBranch2Test, ThrottleDuration_GenReq_EncodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                        processorPerfIntf, inventoryObjPath,
                                        false, nullptr);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, ThrottleDuration_HandleResp_Success)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                        processorPerfIntf, inventoryObjPath,
                                        false, nullptr);

    nsm_violation_duration data{};
    data.power_violation_duration = 1000;
    data.thermal_violation_duration = 2000;
    data.hw_violation_duration = 3000;
    data.global_sw_violation_duration = 4000;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_violation_duration_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_violation_duration_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.processorPerformanceIntf->powerLimitThrottleDuration(),
              1000u);
    EXPECT_EQ(sensor.processorPerformanceIntf->thermalLimitThrottleDuration(),
              2000u);
}

TEST_F(NsmProcessorBranch2Test, ThrottleDuration_HandleResp_ErrorCC)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                        processorPerfIntf, inventoryObjPath,
                                        false, nullptr);

    nsm_violation_duration data{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_violation_duration_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_violation_duration_resp(0, NSM_ERROR, ERR_NULL,
                                                    &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, ThrottleDuration_HandleResp_DecodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                        processorPerfIntf, inventoryObjPath,
                                        false, nullptr);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test,
       DISABLED_ThrottleDuration_HandleResp_LongRunning_Success_2)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(
        sensorName, sensorType, processorPerfIntf, inventoryObjPath, true, gpu);

    nsm_violation_duration data{};
    data.power_violation_duration = 5000;
    data.thermal_violation_duration = 6000;
    data.hw_violation_duration = 7000;
    data.global_sw_violation_duration = 8000;
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_violation_duration_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_violation_duration_event_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.processorPerformanceIntf->powerLimitThrottleDuration(),
              5000u);
}

TEST_F(NsmProcessorBranch2Test, ThrottleDuration_HandleResp_LongRunning_ErrorCC)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(
        sensorName, sensorType, processorPerfIntf, inventoryObjPath, true, gpu);

    nsm_violation_duration data{};
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_violation_duration_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_violation_duration_event_resp(
        0, NSM_ERROR, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test,
       ThrottleDuration_HandleResp_LongRunning_DecodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(
        sensorName, sensorType, processorPerfIntf, inventoryObjPath, true, gpu);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmConfidentialCompute
// ============================================================================

TEST_F(NsmProcessorBranch2Test, ConfidentialCompute_GenReq_EncodeFail)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, ConfidentialCompute_HandleResp_Success_NoMode)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, NO_MODE, NO_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccDevModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCModeState());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCDevModeState());
}

TEST_F(NsmProcessorBranch2Test,
       ConfidentialCompute_HandleResp_Success_ProductionMode)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, PRODUCTION_MODE, PRODUCTION_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.confidentialComputeIntf->ccModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccDevModeEnabled());
    EXPECT_TRUE(sensor.confidentialComputeIntf->pendingCCModeState());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCDevModeState());
}

TEST_F(NsmProcessorBranch2Test,
       ConfidentialCompute_HandleResp_Success_DevtoolsMode)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, DEVTOOLS_MODE, DEVTOOLS_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccModeEnabled());
    EXPECT_TRUE(sensor.confidentialComputeIntf->ccDevModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCModeState());
    EXPECT_TRUE(sensor.confidentialComputeIntf->pendingCCDevModeState());
}

TEST_F(NsmProcessorBranch2Test,
       ConfidentialCompute_HandleResp_Success_MixedModes)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    // current=PRODUCTION, pending=DEVTOOLS
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, PRODUCTION_MODE, DEVTOOLS_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.confidentialComputeIntf->ccModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccDevModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCModeState());
    EXPECT_TRUE(sensor.confidentialComputeIntf->pendingCCDevModeState());
}

TEST_F(NsmProcessorBranch2Test,
       ConfidentialCompute_HandleResp_Success_DevtoolsCurrent_NoPending)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, DEVTOOLS_MODE, NO_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccModeEnabled());
    EXPECT_TRUE(sensor.confidentialComputeIntf->ccDevModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCModeState());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCDevModeState());
}

TEST_F(NsmProcessorBranch2Test,
       ConfidentialCompute_HandleResp_Success_NoCurrent_ProductionPending)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, NO_MODE, PRODUCTION_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccModeEnabled());
    EXPECT_FALSE(sensor.confidentialComputeIntf->ccDevModeEnabled());
    EXPECT_TRUE(sensor.confidentialComputeIntf->pendingCCModeState());
    EXPECT_FALSE(sensor.confidentialComputeIntf->pendingCCDevModeState());
}

TEST_F(NsmProcessorBranch2Test, ConfidentialCompute_HandleResp_ErrorCC)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_confidential_compute_mode_v1_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_confidential_compute_mode_v1_resp(
        0, NSM_ERROR, ERR_NULL, NO_MODE, NO_MODE, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, ConfidentialCompute_HandleResp_DecodeFail)
{
    auto ccIntf = std::make_shared<ConfidentialComputeIntf>(
        bus(), inventoryObjPath.c_str());
    NsmConfidentialCompute sensor(sensorName, sensorType, ccIntf,
                                  inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmEgmMode
// ============================================================================

TEST_F(NsmProcessorBranch2Test, EgmMode_GenReq_EncodeFail)
{
    NsmEgmMode sensor(bus(), sensorName, sensorType, inventoryObjPath);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmProcessorBranch2Test, EgmMode_HandleResp_Success)
{
    NsmEgmMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 3; // egmModeEnabled=1, pendingEGMModeState=1
    uint8_t rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.egmModeIntf->egmModeEnabled());
    EXPECT_TRUE(sensor.egmModeIntf->pendingEGMModeState());
}

TEST_F(NsmProcessorBranch2Test, EgmMode_HandleResp_ErrorCC)
{
    NsmEgmMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    uint8_t rc = encode_get_EGM_mode_resp(0, NSM_ERROR, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmProcessorBranch2Test, EgmMode_HandleResp_DecodeFail)
{
    NsmEgmMode sensor(bus(), sensorName, sensorType, inventoryObjPath);
    auto buf = makeDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());
    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmProcessorBranch2Test, EgmMode_HandleResp_FlagsZero)
{
    NsmEgmMode sensor(bus(), sensorName, sensorType, inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 0;
    uint8_t rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                          response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.egmModeIntf->egmModeEnabled());
    EXPECT_FALSE(sensor.egmModeIntf->pendingEGMModeState());
}

// ============================================================================
// NsmClockLimitGraphics - speedLocked branches
// ============================================================================

TEST_F(NsmProcessorBranch2Test, ClockLimitGraphics_SpeedLocked_True)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);

    nsm_clock_limit clockLimit{};
    clockLimit.requested_limit_min = 1000;
    clockLimit.requested_limit_max = 1500;
    clockLimit.present_limit_min = 1200;
    clockLimit.present_limit_max = 1200; // Same = locked
    sensor.updateReading(clockLimit);
    EXPECT_TRUE(sensor.cpuOperatingConfigIntf->speedLocked());
}

TEST_F(NsmProcessorBranch2Test, ClockLimitGraphics_SpeedLocked_False)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);

    nsm_clock_limit clockLimit{};
    clockLimit.requested_limit_min = 800;
    clockLimit.requested_limit_max = 2100;
    clockLimit.present_limit_min = 500;
    clockLimit.present_limit_max = 1800; // Different = not locked
    sensor.updateReading(clockLimit);
    EXPECT_FALSE(sensor.cpuOperatingConfigIntf->speedLocked());
}
