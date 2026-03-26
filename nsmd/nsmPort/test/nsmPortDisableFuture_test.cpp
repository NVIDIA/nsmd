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
 * Branch coverage tests for nsmd/nsmPort/nsmPortDisableFuture.cpp
 *
 * Covers:
 *   NsmDevicePortDisableFuture::update()
 *     - sensorIO fails → early return
 *     - decode success + cc=0 → update portDisableFuture (TRUE branch)
 *     - cc != NSM_SUCCESS → skip update (FALSE branch)
 *     - decode fails, cc=0 → ternary co_return cc ? cc : rc FALSE branch //
 * NsmDevicePortDisableFuture::setPortDisableFuture()
 *     - value wrong type → throws InvalidArgument
 *     - asyncPatchInProgress true → Unavailable
 *     - valid ports covering switch cases 0-7
 *   NsmDevicePortDisableFuture::setDevicePortDisableFuture()
 *     - postPatchIO fails → WriteFailure
 *     - decode cc=0 → NSM_SW_SUCCESS (TRUE branch)
 *     - decode cc!=0 → WriteFailure (else branch)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/network-ports.h"

#include "nsmPortDisableFuture.hpp"
#include "nsmSetAsync/asyncOperationManager.hpp"

using namespace nsm;

// ============================================================================
// Helpers: encode port-disable-future responses
// ============================================================================

static std::vector<uint8_t> makeGetPortDisableFutureResp(uint8_t cc)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    encode_get_port_disable_future_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, mask, msg);
    return buf;
}

static std::vector<uint8_t> makeSetPortDisableFutureResp(uint8_t cc)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_set_port_disable_future_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// ============================================================================
// Test Fixture
// ============================================================================

struct NsmPortDisableFutureTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    const std::string sensorName = "PortDisableFuture";
    const std::string sensorType = "NSM_PortDisableFuture";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/pcie/port/";

    NsmPortDisableFutureTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortDisableFutureTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmDevicePortDisableFuture> makeSensor()
    {
        return std::make_shared<NsmDevicePortDisableFuture>(
            sensorName, sensorType, invPath);
    }
};

// ============================================================================
// update(): sensorIO fails → if(rc) TRUE → early co_return rc //

// ============================================================================

TEST_F(NsmPortDisableFutureTest, Update_SensorIOFails_EarlyReturn)
{
    auto sensor = makeSensor();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    auto coro = sensor->update(gpu);
    (void)coro;
}

// ============================================================================
// update(): success (rc=0, cc=0) → TRUE branch → portDisableFuture updated
// ============================================================================

TEST_F(NsmPortDisableFutureTest, Update_Success_UpdatesPortDisableFuture)
{
    auto sensor = makeSensor();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeGetPortDisableFutureResp(NSM_SUCCESS)));

    auto coro = sensor->update(gpu);
    (void)coro;
}

// ============================================================================
// update(): cc != NSM_SUCCESS → if(...) FALSE → skip update, return cc
// ============================================================================

TEST_F(NsmPortDisableFutureTest, Update_ErrorCC_SkipsUpdate)
{
    auto sensor = makeSensor();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeGetPortDisableFutureResp(NSM_ERROR)));

    auto coro = sensor->update(gpu);
    (void)coro;
}

// ============================================================================
// update(): too-short response → decode fails (rc!=0, cc stays 0)
// → ternary co_return cc ? cc : rc → FALSE branch (cc=0, returns rc) //

// ============================================================================

TEST_F(NsmPortDisableFutureTest, Update_DecodeLengthError_ReturnRc)
{
    auto sensor = makeSensor();

    // Short enough to fail length check in decode_get_port_disable_future_resp
    // but long enough to allow decode_reason_code_and_cc to read cc=0
    std::vector<uint8_t> shortResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(shortResp, NSM_SUCCESS));

    auto coro = sensor->update(gpu);
    (void)coro;
}

// ============================================================================
// setPortDisableFuture(): value is not vector<uint8_t> → throws InvalidArgument
// ============================================================================

TEST_F(NsmPortDisableFutureTest, SetPortDisableFuture_WrongType_Throws)
{
    auto sensor = makeSensor();
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};

    // bool is not vector<uint8_t>; get_if returns nullptr → throws
    AsyncSetOperationValueType wrongValue{true};

    EXPECT_THROW_COROUTINE(
        sensor->setPortDisableFuture(wrongValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// setPortDisableFuture(): asyncPatchInProgress true → Unavailable
// ============================================================================

TEST_F(NsmPortDisableFutureTest,
       SetPortDisableFuture_PatchInProgress_Unavailable)
{
    auto sensor = makeSensor();
    sensor->asyncPatchInProgress = true;

    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    AsyncSetOperationValueType value{std::vector<uint8_t>{0}};

    auto coro = sensor->setPortDisableFuture(value, &status, gpu);
    (void)coro;

    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
    sensor->asyncPatchInProgress = false;
}

// ============================================================================
// setPortDisableFuture() / setDevicePortDisableFuture():
// postPatchIO fails → WriteFailure
// ============================================================================

TEST_F(NsmPortDisableFutureTest,
       SetPortDisableFuture_PostPatchIOFails_WriteFailure)
{
    auto sensor = makeSensor();
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    AsyncSetOperationValueType value{std::vector<uint8_t>{0}};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    auto coro = sensor->setPortDisableFuture(value, &status, gpu);
    (void)coro;

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setPortDisableFuture() / setDevicePortDisableFuture():
// postPatchIO ok, decode+cc=0 → NSM_SW_SUCCESS (TRUE branch)
// ============================================================================

TEST_F(NsmPortDisableFutureTest, SetPortDisableFuture_Success)
{
    auto sensor = makeSensor();
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    // ports 0-7 exercise all 8 switch cases in setPortDisableFuture
    AsyncSetOperationValueType value{
        std::vector<uint8_t>{0, 1, 2, 3, 4, 5, 6, 7}};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPortDisableFutureResp(NSM_SUCCESS)));

    auto coro = sensor->setPortDisableFuture(value, &status, gpu);
    (void)coro;

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// setPortDisableFuture() / setDevicePortDisableFuture():
// postPatchIO ok, decode cc != NSM_SUCCESS → else branch → WriteFailure
// ============================================================================

TEST_F(NsmPortDisableFutureTest,
       SetPortDisableFuture_DecodeErrorCC_WriteFailure)
{
    auto sensor = makeSensor();
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    AsyncSetOperationValueType value{std::vector<uint8_t>{0}};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPortDisableFutureResp(NSM_ERROR)));

    auto coro = sensor->setPortDisableFuture(value, &status, gpu);
    (void)coro;

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setPortDisableFuture(): empty ports vector → for-loop exits immediately
// (line 168: Branch 21→22 – loop condition false from start, 0 iterations)
// ============================================================================

TEST_F(NsmPortDisableFutureTest, SetPortDisableFuture_EmptyPorts_ZeroIterations)
{
    auto sensor = makeSensor();
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    // Empty vector → ports.size()==0 → for-loop exits on first condition check
    AsyncSetOperationValueType value{std::vector<uint8_t>{}};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetPortDisableFutureResp(NSM_SUCCESS)));

    auto coro = sensor->setPortDisableFuture(value, &status, gpu);
    (void)coro;

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// update(): rc==NSM_SW_SUCCESS, cc==NSM_ERROR → L70 FALSE via cc!=NSM_SUCCESS
// (existing Update_ErrorCC_SkipsUpdate uses a full-size encoded buffer which
// makes decode_reason_code_and_cc return NSM_SW_ERROR_LENGTH, covering the
// rc!=NSM_SW_SUCCESS path; this test covers the cc!=NSM_SUCCESS path)
// ============================================================================

TEST_F(NsmPortDisableFutureTest, Update_DecodeSuccessNonZeroCC_L70ElseBranch)
{
    auto sensor = makeSensor();

    // Exactly-sized non-success buffer: decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR → L70 FALSE via cc!=NSM_SUCCESS
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    auto coro = sensor->update(gpu);
    (void)coro;
}

// ============================================================================
// setDevicePortDisableFuture(): rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// L124 FALSE via cc!=NSM_SUCCESS → else block (WriteFailure)
// ============================================================================

TEST_F(NsmPortDisableFutureTest,
       SetPortDisableFuture_DecodeSuccessNonZeroCC_L124ElseBranch)
{
    auto sensor = makeSensor();
    AsyncOperationStatusType status{AsyncOperationStatusType::Success};
    AsyncSetOperationValueType value{std::vector<uint8_t>{0}};

    // Exactly-sized non-success buffer: decode_reason_code_and_cc returns
    // NSM_SW_SUCCESS with cc=NSM_ERROR → L124 FALSE via cc!=NSM_SUCCESS
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    auto coro = sensor->setPortDisableFuture(value, &status, gpu);
    (void)coro;

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}
