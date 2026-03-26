/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Coverage tests for:
 *   nsmd/nsmSetAsync/nsmSetCpuOperatingConfig.cpp
 *   nsmd/nsmSetAsync/nsmAsyncSensor.cpp
 *
 * Branches targeted:
 *
 * nsmSetCpuOperatingConfig.cpp:
 *   [1] getMinGraphicsClockLimit: if(rc) TRUE – postPatchIO fails
 *   [2] getMinGraphicsClockLimit: ELSE branch – decode fails (too-small resp)
 *   [3] getMaxGraphicsClockLimit: if(rc) TRUE – postPatchIO fails
 *   [4] setClockLimitOnDevice: bounds check TRUE (speed < min or > max)
 *   [5] setClockLimitOnDevice: if(rc) TRUE after final postPatchIO fails
 *   [6] setClockLimitOnDevice: ELSE after decode_set_clock_limit_resp fails
 *   [7] setClockLimitOnDevice: success path (if TRUE)
 *   [8] setClockLimitOnDevice: speedLocked=true branch
 *   [9] setCPUSpeedConfig: wrong variant type → throws
 *
 * nsmAsyncSensor.cpp:
 *   [A] update(): if(!requestMsg.has_value()) TRUE – genRequestMsg nullopt
 *   [B] update(): if(rc) TRUE – sensorIO fails
 *   [C] update(): if(rc != NSM_SW_SUCCESS) TRUE – handleResponseMsg fails
 *   [D] update(): if(rc != NSM_SW_SUCCESS) FALSE – success path
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmAsyncSensor.hpp"
#include "nsmSetCpuOperatingConfig.hpp"

using namespace ::testing;
using namespace nsm;

// ============================================================================
// Helpers
// ============================================================================

// Encode a valid getInventoryInformation success response carrying a uint32_t.
static std::vector<uint8_t> makeInvResp(uint32_t value)
{
    std::vector<uint8_t> buf(256, 0);
    uint32_t le = htole32(value);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, static_cast<uint16_t>(sizeof(uint32_t)),
        reinterpret_cast<uint8_t*>(&le), msg);
    return buf;
}

// Encode a valid setClockLimit success response.
static std::vector<uint8_t> makeSetClockSuccessResp()
{
    std::vector<uint8_t> buf(256, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_clock_limit_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    return buf;
}

// Encode a setClockLimit error-CC response at EXACTLY the right size so that
// decode_reason_code_and_cc returns NSM_SW_SUCCESS with cc=NSM_ERROR.
static std::vector<uint8_t> makeSetClockErrorResp()
{
    size_t len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(len, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_clock_limit_resp(0, NSM_ERROR, ERR_NULL, msg);
    return buf;
}

// ============================================================================
// nsmSetCpuOperatingConfig tests (via setCPUSpeedConfig free function)
// ============================================================================

struct NsmSetCpuOperatingConfigTest :
    public testing::Test,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmSetCpuOperatingConfigTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        ASSERT_NE(gpu, nullptr);
    }

    ~NsmSetCpuOperatingConfigTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// [1] getMinGraphicsClockLimit postPatchIO fails → if(rc) TRUE in getMin
//     → if(rc) TRUE in setClockLimitOnDevice after getMin call
TEST_F(NsmSetCpuOperatingConfigTest, GetMin_PostPatchIOFail_WriteFail)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(1000));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [2] getMinGraphicsClockLimit: too-small response → decode_reason_code_and_cc
//     returns NSM_SW_ERROR_LENGTH → rc != NSM_SW_SUCCESS → ELSE branch in
//     getMin → co_return NSM_SW_ERROR_COMMAND_FAIL
//     → if(rc) TRUE in setClockLimitOnDevice after getMin call
TEST_F(NsmSetCpuOperatingConfigTest, GetMin_DecodeFail_WriteFail)
{
    // Buffer must be at least sizeof(nsm_msg_hdr) to avoid out-of-bounds reads
    // in decode_reason_code_and_cc, but still too small for a valid
    // get_inventory_information response (needs sizeof(nsm_msg_hdr) +
    // NSM_RESPONSE_CONVENTION_LEN = 11 bytes).
    std::vector<uint8_t> tooSmall(sizeof(nsm_msg_hdr) + NSM_RESPONSE_ERROR_LEN,
                                  0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(tooSmall));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(1000));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [3] getMaxGraphicsClockLimit postPatchIO fails → if(rc) TRUE in getMax
//     → if(rc) TRUE in setClockLimitOnDevice after getMax call
TEST_F(NsmSetCpuOperatingConfigTest, GetMax_PostPatchIOFail_WriteFail)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000))) // getMin succeeds
        .WillOnce(mockPostPatchIO(NSM_ERROR));        // getMax fails

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(1000));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [4a] Speed below min → bounds check TRUE → status = InvalidArgument
TEST_F(NsmSetCpuOperatingConfigTest, SpeedBelowMin_InvalidArgument)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000)))  // getMin = 1000
        .WillOnce(mockPostPatchIO(makeInvResp(2000))); // getMax = 2000

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // requestedSpeedLimit=500 < minClockLimit=1000
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(500));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// [4b] Speed above max → bounds check TRUE (other direction)
TEST_F(NsmSetCpuOperatingConfigTest, SpeedAboveMax_InvalidArgument)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000)))  // getMin = 1000
        .WillOnce(mockPostPatchIO(makeInvResp(2000))); // getMax = 2000

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // requestedSpeedLimit=3000 > maxClockLimit=2000
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(3000));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// [5] Final postPatchIO fails → setClockLimitOnDevice if(rc) after postPatchIO
TEST_F(NsmSetCpuOperatingConfigTest, SetClock_PostPatchIOFail_WriteFail)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000))) // getMin
        .WillOnce(mockPostPatchIO(makeInvResp(2000))) // getMax
        .WillOnce(mockPostPatchIO(NSM_ERROR));        // setClockLimit

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(1500));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [6] decode_set_clock_limit_resp gets error-CC response (exact size) →
//     decode returns NSM_SW_SUCCESS with cc=NSM_ERROR → ELSE branch in
//     setClockLimitOnDevice
TEST_F(NsmSetCpuOperatingConfigTest, SetClock_ErrorCC_WriteFail)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000)))
        .WillOnce(mockPostPatchIO(makeInvResp(2000)))
        .WillOnce(mockPostPatchIO(makeSetClockErrorResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(1500));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [7] Success path (speedLocked=false) → if(rc==NSM_SW_SUCCESS && cc==SUCCESS)
TEST_F(NsmSetCpuOperatingConfigTest, SetClock_Success_SpeedNotLocked)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000)))
        .WillOnce(mockPostPatchIO(makeInvResp(2000)))
        .WillOnce(mockPostPatchIO(makeSetClockSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(false, uint32_t(1500));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// [8] speedLocked=true → limitMin=limitMax=requestedSpeedLimit branch
TEST_F(NsmSetCpuOperatingConfigTest, SetClock_Success_SpeedLocked)
{
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeInvResp(1000)))
        .WillOnce(mockPostPatchIO(makeInvResp(2000)))
        .WillOnce(mockPostPatchIO(makeSetClockSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = std::make_tuple(true, uint32_t(1500));
    setCPUSpeedConfig(0, val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// NsmAsyncSensor update() branch coverage
// ============================================================================

class TestNsmAsyncSensor : public nsm::NsmAsyncSensor
{
  public:
    using NsmAsyncSensor::NsmAsyncSensor;

    bool returnNullopt = false;
    uint8_t handleRetVal = NSM_SW_SUCCESS;

    std::optional<Request> genRequestMsg(eid_t, uint8_t) override
    {
        if (returnNullopt)
            return std::nullopt;
        return Request(4, 0);
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return handleRetVal;
    }
};

struct NsmAsyncSensorUpdateTest : public testing::Test, public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmAsyncSensorUpdateTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        ASSERT_NE(gpu, nullptr);
    }

    ~NsmAsyncSensorUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// [A] genRequestMsg returns nullopt → if(!requestMsg.has_value()) TRUE
//     → *status = WriteFailure, co_return NSM_SW_ERROR
TEST_F(NsmAsyncSensorUpdateTest, Update_NullRequest_WriteFail)
{
    TestNsmAsyncSensor sensor("test", "type");
    sensor.returnNullopt = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = bool(true);
    sensor.set(val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [B] sensorIO fails (rc != 0) → if(rc) TRUE
//     → *status = WriteFailure, co_return rc
TEST_F(NsmAsyncSensorUpdateTest, Update_SensorIOFail_WriteFail)
{
    TestNsmAsyncSensor sensor("test", "type");

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = bool(true);
    sensor.set(val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [C] sensorIO succeeds but handleResponseMsg returns error →
//     if(rc != NSM_SW_SUCCESS) TRUE → *status = WriteFailure
TEST_F(NsmAsyncSensorUpdateTest, Update_HandleResponseFail_WriteFail)
{
    TestNsmAsyncSensor sensor("test", "type");
    sensor.handleRetVal = NSM_SW_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(Response())); // sensorIO succeeds (rc=0)

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = bool(true);
    sensor.set(val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// [D] Full success → sensorIO ok, handleResponseMsg ok →
//     if(rc != NSM_SW_SUCCESS) FALSE → status unchanged
TEST_F(NsmAsyncSensorUpdateTest, Update_Success_StatusUnchanged)
{
    TestNsmAsyncSensor sensor("test", "type");
    // handleRetVal = NSM_SW_SUCCESS (default)

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(Response())); // sensorIO succeeds (rc=0)

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType val = bool(true);
    sensor.set(val, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// [E] status == nullptr → if(status == nullptr) TRUE → throws InvalidArgument
TEST_F(NsmAsyncSensorUpdateTest, Set_NullStatus_ThrowsInvalidArgument)
{
    TestNsmAsyncSensor sensor("test", "type");
    AsyncSetOperationValueType val = bool(true);
    EXPECT_THROW_COROUTINE(
        sensor.set(val, nullptr, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setCPUSpeedConfig: wrong variant type → if(!config) TRUE → throws
// ============================================================================

// [9] setCPUSpeedConfig: value is not std::tuple<bool,uint32_t> →
//     std::get_if<tuple> returns nullptr → if(!config) TRUE → throws
TEST_F(NsmSetCpuOperatingConfigTest, SetCPUSpeedConfig_WrongVariant_Throws)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass a bool instead of tuple<bool, uint32_t>
    AsyncSetOperationValueType val = bool(true);
    EXPECT_THROW_COROUTINE(
        setCPUSpeedConfig(0, val, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}
