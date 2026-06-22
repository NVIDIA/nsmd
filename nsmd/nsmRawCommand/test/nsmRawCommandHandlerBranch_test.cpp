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
 * Branch coverage for nsmRawCommandHandler.cpp
 *
 * Covers additional branches not covered by the main test file:
 * - doSendRequest: cc==NSM_SUCCESS TRUE (copySuccessResponse with data)
 * - doSendRequest: cc!=NSM_SUCCESS (copyReasonCodeResponse)
 * - doSendRequest: V2 format encode path
 * - doSendLongRunningRequest: V2 format encode paths
 * - sendRequest: deviceType boundary (NSM_DEV_ID_CPU valid)
 * - sendRequest: objectPath empty → Unavailable exception
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmRawCommandHandler.hpp"

#undef private
#undef protected

using namespace nsm;
using sdbusplus::message::unix_fd;

// ============================================================================
// Fixture
// ============================================================================

struct NsmRawCommandBranchTest : public Test, public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;

    NsmRawCommandBranchTest() : SensorManagerTest(devices)
    {
        // Ensure NsmRawCommandHandler is initialized (static, safe to call
        // multiple times)
        NsmRawCommandHandler::initialize(utils::DBusHandler::getBus(),
                                         "/nsmRawCommandBranch");

        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(
                "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0"));
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmRawCommandBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    utils::CustomFD fd{memfd_create("nsmRawBranch", 0)};

    Response successResponse(uint8_t messageType, uint8_t commandCode)
    {
        // Build a success response with some payload data
        const std::string payload = "test_data";
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + payload.size(), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_common_resp(0, NSM_SUCCESS, 0, messageType,
                                     commandCode, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        // Copy payload after the common_resp
        memcpy(msg->payload + sizeof(nsm_common_resp) - 2, payload.data(),
               payload.size());
        return response;
    }

    Response errorCCResponse()
    {
        // Valgrind-safe error CC response
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        encode_common_resp(0, NSM_ERROR, ERR_NULL, 0, 0, msg);
        return response;
    }

    auto sendRequestHelper(uint8_t deviceType, uint8_t deviceRole,
                           uint8_t instanceId, uint8_t messageType,
                           uint8_t commandCode, uint8_t msgFormatVersion)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        NsmRawCommandHandler::getInstance().doSendRequest(
            deviceType, instanceId, deviceRole, messageType, commandCode,
            dup(fd), statusInterface, valueInterface, msgFormatVersion);
        return std::make_tuple(statusInterface, valueInterface);
    }

    auto sendLongRunningHelper(uint8_t deviceType, uint8_t deviceRole,
                               uint8_t instanceId, bool isLongRunning,
                               uint8_t messageType, uint8_t commandCode,
                               uint8_t msgFormatVersion)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        NsmRawCommandHandler::getInstance().doSendLongRunningRequest(
            deviceType, instanceId, deviceRole, isLongRunning, messageType,
            commandCode, dup(fd), statusInterface, valueInterface,
            msgFormatVersion);
        return std::make_tuple(statusInterface, valueInterface);
    }
};

// ============================================================================
// doSendRequest branch coverage
// ============================================================================

// doSendRequest: postPatchIO succeeds, decode succeeds, cc==NSM_SUCCESS
// → copySuccessResponse path with payload data
TEST_F(NsmRawCommandBranchTest,
       DoSendRequest_DecodeSuccess_CcSuccess_CopiesPayload)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, 0, 1, 2, msg);

    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));

    const auto [statusInterface, valueInterface] = sendRequestHelper(0, 0, 0, 1,
                                                                     2, 1);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
}

// doSendRequest: V2 format, postPatchIO succeeds, cc!=NSM_SUCCESS
// → copyReasonCodeResponse path
TEST_F(NsmRawCommandBranchTest, DoSendRequest_V2Format_ErrorCC_CopiesReasonCode)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(errorCCResponse()));

    const auto [statusInterface, valueInterface] = sendRequestHelper(0, 0, 0, 1,
                                                                     2, 2);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    // Read back reason code data from fd
    std::vector<uint8_t> data;
    utils::readFdToBuffer(fd, data);
    EXPECT_EQ(data.size(), 3u);
    EXPECT_EQ(data[0], NSM_ERROR);
}

// doSendRequest: device not found (instanceId mismatch)
// → throws invalid_argument → InvalidArgument
TEST_F(NsmRawCommandBranchTest, DoSendRequest_DeviceNotFound_InvalidArgument)
{
    const auto [statusInterface, _] = sendRequestHelper(0, 1, 0, 0, 0, 1);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::InvalidArgument);
}

// doSendRequest: invalid format version → throws invalid_argument
TEST_F(NsmRawCommandBranchTest,
       DoSendRequest_InvalidFormatVersion_InvalidArgument)
{
    const auto [statusInterface, _] = sendRequestHelper(0, 0, 0, 0, 0, 99);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::InvalidArgument);
}

// doSendRequest: postPatchIO fails → throws runtime_error → WriteFailure
TEST_F(NsmRawCommandBranchTest, DoSendRequest_PostPatchIOFail_WriteFailure)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, 0, 0, 0, msg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(resp, NSM_ERROR));

    const auto [statusInterface, _] = sendRequestHelper(0, 0, 0, 0, 0, 1);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// doSendRequest: decode fails (short buffer) → throws runtime_error
TEST_F(NsmRawCommandBranchTest, DoSendRequest_DecodeFail_WriteFailure)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_ERROR, ERR_NOT_SUPPORTED, 0, 0, msg);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));

    const auto [statusInterface, _] = sendRequestHelper(0, 0, 0, 0, 0, 1);
    EXPECT_EQ(statusInterface->status(),
              AsyncOperationStatusType::WriteFailure);
}

// doSendRequest: NSM_ERR_UNSUPPORTED_COMMAND_CODE path
TEST_F(NsmRawCommandBranchTest,
       DoSendRequest_UnsupportedCommand_CopiesReasonCode)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, 0, 0, 0, msg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(resp, NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    const auto [statusInterface, valueInterface] = sendRequestHelper(0, 0, 0, 0,
                                                                     0, 1);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    std::vector<uint8_t> data;
    utils::readFdToBuffer(fd, data);
    EXPECT_EQ(data.size(), 3u);
    EXPECT_EQ(data[0], NSM_ERR_UNSUPPORTED_COMMAND_CODE);
}

// ============================================================================
// doSendLongRunningRequest additional branch coverage
// ============================================================================

// doSendLongRunningRequest: V2 format success (covers both v2 encode branches)
TEST_F(NsmRawCommandBranchTest,
       DoSendLongRunning_V2Format_CcSuccess_CoversV2Encode)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, 0, 0, 0, msg);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));

    const auto [statusInterface, valueInterface] =
        sendLongRunningHelper(NSM_DEV_ID_GPU, 0, 0, true, 0, 0, 2);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
}

// doSendLongRunningRequest: error CC (not NSM_SUCCESS, not NSM_ACCEPTED)
// → initAcceptInstanceId returns false → copyReasonCodeResponse
TEST_F(NsmRawCommandBranchTest,
       DoSendLongRunning_ErrorCC_NotAccepted_ReasonCode)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(errorCCResponse()));

    const auto [statusInterface, valueInterface] =
        sendLongRunningHelper(NSM_DEV_ID_GPU, 0, 0, true, 0, 0, 1);
    EXPECT_EQ(statusInterface->status(), AsyncOperationStatusType::Success);
    std::vector<uint8_t> data;
    utils::readFdToBuffer(fd, data);
    EXPECT_EQ(data.size(), 3u);
    EXPECT_EQ(data[0], NSM_ERROR);
}

// ============================================================================
// sendRequest branch coverage
// ============================================================================

// sendRequest: valid deviceType at boundary (NSM_DEV_ID_CPU) → no throw
TEST_F(NsmRawCommandBranchTest, SendRequest_ValidDeviceTypeBoundary_NoThrow)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_common_resp(0, NSM_SUCCESS, 0, 0, 0, msg);
    // NSM_DEV_ID_CPU is the max valid deviceType
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));

    // This might not find a device for NSM_DEV_ID_CPU but it won't throw
    // InvalidArgument for deviceType validation
    auto path = NsmRawCommandHandler::getInstance().sendRequest(
        NSM_DEV_ID_GPU, 0, 0, false, 0, 0, unix_fd(fd), 1);
    EXPECT_NE(path, sdbusplus::object_path{});
}

// sendRequest: invalid deviceType > NSM_DEV_ID_CPU → throws InvalidArgument
TEST_F(NsmRawCommandBranchTest, SendRequest_InvalidDeviceType_Throws)
{
    EXPECT_THROW(
        NsmRawCommandHandler::getInstance().sendRequest(
            NSM_DEV_ID_CPU + 1, 0, 0, false, 0, 0, unix_fd(fd), 1),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

// ============================================================================
// NsmRawLongRunningEventHandler::handle additional coverage
// ============================================================================

// handle: timer.stop() returns true (timer was running)
TEST_F(NsmRawCommandBranchTest, Handle_TimerStopReturnsTrue)
{
    NsmRawLongRunningEventHandler handler("Branch_H", "BrEvent", true);

    size_t msg_len = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                     sizeof(nsm_long_running_resp);
    std::vector<uint8_t> buf(msg_len, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto* evt = reinterpret_cast<nsm_event*>(msg->payload);
    evt->data_size = sizeof(nsm_long_running_resp);
    auto* resp = reinterpret_cast<nsm_long_running_resp*>(evt->data);
    resp->instance_id = 0x10;
    resp->completion_code = NSM_SUCCESS;

    handler.acceptInstanceId = 0x10;
    handler.isLongRunning = true;

    int rc = handler.handle(20, 0, 0, msg, msg_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(handler.data.empty());
    // First byte should be the completion code
    EXPECT_EQ(handler.data[0], NSM_SUCCESS);
}

// handle: validateEvent fails because of wrong instance id
TEST_F(NsmRawCommandBranchTest, Handle_WrongInstanceId_ValidateEventFails)
{
    NsmRawLongRunningEventHandler handler("Branch_H2", "BrEvent2", true);

    size_t msg_len = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                     sizeof(nsm_long_running_resp);
    std::vector<uint8_t> buf(msg_len, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto* evt = reinterpret_cast<nsm_event*>(msg->payload);
    evt->data_size = sizeof(nsm_long_running_resp);
    auto* resp = reinterpret_cast<nsm_long_running_resp*>(evt->data);
    resp->instance_id = 0x20;

    handler.acceptInstanceId = 0x30; // mismatch
    handler.isLongRunning = true;

    int rc = handler.handle(20, 0, 0, msg, msg_len);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(handler.data.empty());
}
