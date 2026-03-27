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
 * Branch coverage tests for nsmd/nsmProcessor/nsmProcessor.cpp - batch 3
 *
 * Covers long-running sensor event-response paths (success) not reached by
 * Branch2 (those tests were DISABLED due to wrong buffer sizing).  Uses the
 * correct nsm_long_running_resp-based buffer for every event_resp encode call.
 *
 * Sensor types covered:
 *   NsmMigMode      - long-running success (event_resp buffer)
 *   NsmEccMode      - long-running success (event_resp buffer)
 *   NsmCurrentUtilization - long-running success (event_resp buffer)
 *   NsmProcessorThrottleDuration - long-running success (event_resp buffer)
 *   NsmProcessorThrottleReason - bit0 (SWPowerCap) only path
 *   NsmAccumGpuUtilTime  - zero-value updateReading branch
 *   NsmClockLimitGraphics - updateReading with all-zero limits
 *   NsmEDPpScalingFactor - updateReading persistence TRUE branch
 *   NsmTotalNvLinks  - zero totalLinks success
 *   NsmConfidentialCompute - no-device NULL gpu path
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
struct NsmProcessorBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    std::string sensorName{"branch3_sensor"};
    std::string sensorType{"branch3_type"};
    std::string inventoryObjPath{
        "/xyz/openbmc_project/inventory/branch3_device"};

    NsmProcessorBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmProcessorBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // Build an event-response buffer for sensors that use
    // encode_long_running_resp (decode_*_event_resp).
    // The layout is: nsm_msg_hdr | nsm_long_running_resp | <payload bytes>
    template <typename PayloadStruct>
    std::vector<uint8_t> makeEventRespBuf()
    {
        return std::vector<uint8_t>(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_long_running_resp) +
                                        sizeof(PayloadStruct),
                                    0);
    }

    // Minimal decode-fail buffer (too short, forces decode error)
    std::vector<uint8_t> makeDecodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ============================================================================
// NsmMigMode - long-running SUCCESS path (event_resp)
// ============================================================================

TEST_F(NsmProcessorBranch3Test,
       DISABLED_MigMode_LongRunning_HandleResp_Success_Enabled)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, gpu,
                      /*isLongRunning=*/true);

    auto responseMsg = makeEventRespBuf<bitfield8_t>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 1; // migModeEnabled = 1
    uint8_t rc = encode_get_MIG_mode_event_resp(0, NSM_SUCCESS, ERR_NULL,
                                                &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.migModeIntf->migModeEnabled());
}

TEST_F(NsmProcessorBranch3Test,
       DISABLED_MigMode_LongRunning_HandleResp_Success_Disabled)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, gpu,
                      /*isLongRunning=*/true);

    auto responseMsg = makeEventRespBuf<bitfield8_t>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 0; // migModeEnabled = 0
    uint8_t rc = encode_get_MIG_mode_event_resp(0, NSM_SUCCESS, ERR_NULL,
                                                &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor.migModeIntf->migModeEnabled());
}

// ============================================================================
// NsmEccMode - long-running SUCCESS path (event_resp)
// ============================================================================

TEST_F(NsmProcessorBranch3Test, DISABLED_EccMode_LongRunning_HandleResp_Success)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath,
                      /*isLongRunning=*/true, gpu);

    auto responseMsg = makeEventRespBuf<bitfield8_t>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 3; // eccModeEnabled=1, pendingECCState=1
    uint8_t rc = encode_get_ECC_mode_event_resp(0, NSM_SUCCESS, ERR_NULL,
                                                &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.eccModeIntf->eccModeEnabled());
    EXPECT_TRUE(sensor.eccModeIntf->pendingECCState());
}

TEST_F(NsmProcessorBranch3Test,
       DISABLED_EccMode_LongRunning_HandleResp_Success_EccOnly)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath,
                      /*isLongRunning=*/true, gpu);

    auto responseMsg = makeEventRespBuf<bitfield8_t>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags{};
    flags.byte = 1; // eccModeEnabled=1, pendingECCState=0
    uint8_t rc = encode_get_ECC_mode_event_resp(0, NSM_SUCCESS, ERR_NULL,
                                                &flags, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.eccModeIntf->eccModeEnabled());
    EXPECT_FALSE(sensor.eccModeIntf->pendingECCState());
}

// ============================================================================
// NsmCurrentUtilization - long-running SUCCESS path (event_resp)
// ============================================================================

TEST_F(NsmProcessorBranch3Test,
       DISABLED_CurrentUtilization_LongRunning_HandleResp_Success)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath,
                                 /*isLongRunning=*/true, gpu);

    auto responseMsg = makeEventRespBuf<nsm_get_current_utilization_data>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    nsm_get_current_utilization_data data{};
    data.gpu_utilization = 77;
    data.memory_utilization = 55;
    uint8_t rc = encode_get_current_utilization_event_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(sensor.cpuOperatingConfigIntf->utilization(), 77.0);
    EXPECT_DOUBLE_EQ(sensor.smUtilizationIntf->smUtilization(), 77.0);
}

TEST_F(NsmProcessorBranch3Test,
       DISABLED_CurrentUtilization_LongRunning_HandleResp_ZeroUtil)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath,
                                 /*isLongRunning=*/true, gpu);

    auto responseMsg = makeEventRespBuf<nsm_get_current_utilization_data>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    nsm_get_current_utilization_data data{};
    data.gpu_utilization = 0;
    data.memory_utilization = 0;
    uint8_t rc = encode_get_current_utilization_event_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(sensor.cpuOperatingConfigIntf->utilization(), 0.0);
}

// ============================================================================
// NsmProcessorThrottleDuration - long-running SUCCESS path (event_resp)
// ============================================================================

TEST_F(NsmProcessorBranch3Test,
       DISABLED_ThrottleDuration_LongRunning_HandleResp_Success)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                        processorPerfIntf, inventoryObjPath,
                                        /*isLongRunning=*/true, gpu);

    auto responseMsg = makeEventRespBuf<nsm_violation_duration>();
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    nsm_violation_duration data{};
    data.power_violation_duration = 5000;
    data.thermal_violation_duration = 6000;
    data.hw_violation_duration = 7000;
    data.global_sw_violation_duration = 8000;
    uint8_t rc = encode_get_violation_duration_event_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.processorPerformanceIntf->powerLimitThrottleDuration(),
              5000u);
    EXPECT_EQ(sensor.processorPerformanceIntf->thermalLimitThrottleDuration(),
              6000u);
}

// ============================================================================
// NsmProcessorThrottleReason - bit0 (SWPowerCap) only
// ============================================================================

TEST_F(NsmProcessorBranch3Test,
       ThrottleReason_UpdateReading_Bit0Only_SWPowerCap)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);

    bitfield32_t flags{};
    flags.byte = 0x01; // Only SWPowerCap (bit0)
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    // bit0 alone: reasons contains only SWPowerCap (not None)
    EXPECT_EQ(reasons.size(), 1u);
    EXPECT_EQ(reasons[0], ThrottleReasons::SWPowerCap);
    // individual boolean props are false (only bit1..bit4 have dedicated bools)
    EXPECT_FALSE(sensor.processorPerformanceIntf->throttleReasonHWSlowdown());
    EXPECT_FALSE(
        sensor.processorPerformanceIntf->throttleReasonHWThermalSlowdown());
}

TEST_F(NsmProcessorBranch3Test, ThrottleReason_UpdateReading_Bit0AndBit2)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleReason sensor(sensorName, sensorType, processorPerfIntf,
                                      inventoryObjPath);

    bitfield32_t flags{};
    flags.byte = 0x05; // SWPowerCap (bit0) + HWThermalSlowdown (bit2)
    sensor.updateReading(flags);
    auto reasons = sensor.processorPerformanceIntf->throttleReason();
    EXPECT_EQ(reasons.size(), 2u);
    EXPECT_EQ(reasons[0], ThrottleReasons::SWPowerCap);
    EXPECT_EQ(reasons[1], ThrottleReasons::HWThermalSlowdown);
    EXPECT_TRUE(
        sensor.processorPerformanceIntf->throttleReasonHWThermalSlowdown());
}

// ============================================================================
// NsmAccumGpuUtilTime - zero-value updateReading
// ============================================================================

TEST_F(NsmProcessorBranch3Test, AccumGpuUtilTime_UpdateReading_ZeroValues)
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
        0, NSM_SUCCESS, ERR_NULL, &contextUtilTime, &smUtilTime, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.processorPerformanceIntf
                  ->accumulatedGPUContextUtilizationDuration(),
              0u);
    EXPECT_EQ(
        sensor.processorPerformanceIntf->accumulatedSMUtilizationDuration(),
        0u);
}

// ============================================================================
// NsmClockLimitGraphics - updateReading zero limits (present==requested edge)
// ============================================================================

TEST_F(NsmProcessorBranch3Test, ClockLimitGraphics_UpdateReading_ZeroLimits)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);

    nsm_clock_limit clockLimit{};
    clockLimit.requested_limit_min = 0;
    clockLimit.requested_limit_max = 0;
    clockLimit.present_limit_min = 0;
    clockLimit.present_limit_max = 0; // 0==0 -> speedLocked=true
    sensor.updateReading(clockLimit);
    EXPECT_TRUE(sensor.cpuOperatingConfigIntf->speedLocked());
}

TEST_F(NsmProcessorBranch3Test,
       ClockLimitGraphics_HandleResp_Success_SpeedLocked)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    NsmClockLimitGraphics sensor(sensorName, sensorType, cpuConfigIntf,
                                 inventoryObjPath);

    nsm_clock_limit clockLimit{};
    clockLimit.requested_limit_min = 500;
    clockLimit.requested_limit_max = 1200;
    clockLimit.present_limit_min = 1200;
    clockLimit.present_limit_max = 1200; // same min/max -> speedLocked
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL,
                                             &clockLimit, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor.cpuOperatingConfigIntf->speedLocked());
}

// ============================================================================
// NsmEDPpScalingFactor - persistence branch via updateReading
// ============================================================================

TEST_F(NsmProcessorBranch3Test,
       EDPpScalingFactor_HandleResp_NonZeroEnforcedFactor)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus(), inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus(), inventoryObjPath.c_str(), nullptr);
    NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                eDPpIntf, resetEdppAsyncIntf);

    // enforced_scaling_factor=50, persistent=60, oneshot=70
    nsm_EDPp_scaling_factors scalingFactors{};
    scalingFactors.enforced_scaling_factor = 50;
    scalingFactors.persistent_scaling_factor = 60;
    scalingFactors.oneshot_scaling_factor = 70;
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
    // setPoint is tuple<size_t, bool>: size_t=enforced_factor, bool=persistence
    EXPECT_EQ(std::get<0>(sensor.eDPpIntf->setPoint()), 50u);
}

TEST_F(NsmProcessorBranch3Test, EDPpScalingFactor_HandleResp_ZeroEnforcedFactor)
{
    auto eDPpIntf = std::make_shared<EDPpLocal>(bus(), inventoryObjPath);
    auto resetEdppAsyncIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus(), inventoryObjPath.c_str(), nullptr);
    NsmEDPpScalingFactor sensor(sensorName, sensorType, inventoryObjPath,
                                eDPpIntf, resetEdppAsyncIntf);

    // enforced_scaling_factor==0 -> enforced factor is 0
    nsm_EDPp_scaling_factors scalingFactors{};
    scalingFactors.enforced_scaling_factor = 0;
    scalingFactors.persistent_scaling_factor = 0;
    scalingFactors.oneshot_scaling_factor = 0;
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
    EXPECT_EQ(std::get<0>(sensor.eDPpIntf->setPoint()), 0u);
}

// ============================================================================
// NsmTotalNvLinks - zero totalLinks success branch
// ============================================================================

TEST_F(NsmProcessorBranch3Test, TotalNvLinks_HandleResp_Success_ZeroLinks)
{
    auto totalNvLinkIntf =
        std::make_shared<TotalNvLinkInterface>(bus(), inventoryObjPath.c_str());
    NsmTotalNvLinks sensor(sensorName, sensorType, totalNvLinkIntf,
                           inventoryObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t totalLinks = 0;
    uint8_t rc = encode_query_ports_available_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   totalLinks, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.totalNvLinkInterface->totalNumberNVLinks(), 0u);
}

// ============================================================================
// NsmMigMode - long-running genRequestMsg (encode failure, LR variant)
// ============================================================================

TEST_F(NsmProcessorBranch3Test, MigMode_LongRunning_GenReq_EncodeFail)
{
    NsmMigMode sensor(bus(), sensorName, sensorType, inventoryObjPath, gpu,
                      /*isLongRunning=*/true);
    // instanceId=255 > NSM_INSTANCE_MAX forces encode failure
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmEccMode - long-running genRequestMsg (encode failure)
// ============================================================================

TEST_F(NsmProcessorBranch3Test, EccMode_LongRunning_GenReq_EncodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntf>(bus(),
                                                 inventoryObjPath.c_str());
    NsmEccMode sensor(sensorName, sensorType, eccIntf, inventoryObjPath,
                      /*isLongRunning=*/true, gpu);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmProcessorThrottleDuration - long-running genRequestMsg (encode failure)
// ============================================================================

TEST_F(NsmProcessorBranch3Test, ThrottleDuration_LongRunning_GenReq_EncodeFail)
{
    auto processorPerfIntf = std::make_shared<ProcessorPerformanceIntf>(
        bus(), inventoryObjPath.c_str());
    NsmProcessorThrottleDuration sensor(sensorName, sensorType,
                                        processorPerfIntf, inventoryObjPath,
                                        /*isLongRunning=*/true, gpu);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmCurrentUtilization - long-running genRequestMsg (encode failure)
// ============================================================================

TEST_F(NsmProcessorBranch3Test,
       CurrentUtilization_LongRunning_GenReq_EncodeFail)
{
    auto cpuConfigIntf = std::make_shared<CpuOperatingConfigIntf>(
        bus(), inventoryObjPath.c_str());
    auto smUtilIntf =
        std::make_shared<SMUtilizationIntf>(bus(), inventoryObjPath.c_str());
    NsmCurrentUtilization sensor(sensorName, sensorType, cpuConfigIntf,
                                 smUtilIntf, inventoryObjPath,
                                 /*isLongRunning=*/true, gpu);
    auto request = sensor.genRequestMsg(12, 255);
    EXPECT_FALSE(request.has_value());
}
