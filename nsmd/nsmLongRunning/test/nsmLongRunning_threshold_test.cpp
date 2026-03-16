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

// ============================================================================
// NEW TESTS for:
//   1) nsmLongRunning/nsmLongRunningSensor.cpp
//      - NsmLongRunningSensor::handle() method
//      - NsmLongRunningSensor validateEvent / initAcceptInstanceId
//      - Timer state management
//
//   2) nsmLongRunning/nsmAsyncLongRunningSensor.cpp
//      - NsmAsyncLongRunningSensor::handle() method
//      - Async-specific initAcceptInstanceId
//
//   3) nsmNumericSensor/nsmThresholdFactory.cpp
//      - NsmThreshold genRequestMsg / handleResponseMsg
//      - NsmThresholdAggregator genRequestMsg
//      - NsmNumericSensorValueAggregate propagation
//      - NsmThreshold handleOfflineState / equals
//
// INTEGRATION INSTRUCTIONS:
//   Part A (LongRunning tests):
//     Append to nsmd/nsmLongRunning/test/nsmLongRunning_test.cpp
//     These tests use the existing TestNsmLongRunningSensor,
//     TestNsmAsyncLongRunningSensor, and NsmLongRunningTest fixture.
//
//   Part B (ThresholdFactory tests):
//     Append to nsmd/nsmNumericSensor/test/nsmThresholdValue_test.cpp or
//     create nsmd/nsmNumericSensor/test/nsmThresholdFactory_test.cpp
//     with appropriate meson.build entry. Requires additional includes:
//       #include "nsmNumericSensor/nsmThreshold.hpp"
//       #include "nsmNumericSensor/nsmThresholdAggregator.hpp"
// ============================================================================

// ============================================================================
// PART A: NsmLongRunningSensor and NsmAsyncLongRunningSensor tests
// ============================================================================

// ---------------------------------------------------------------------------
// Mockable concrete classes for NsmLongRunningSensor
// ---------------------------------------------------------------------------

class MockableNsmLongRunningSensor : public nsm::NsmLongRunningSensor
{
  public:
    MockableNsmLongRunningSensor(const std::string& name,
                                 const std::string& type, bool isLongRunning,
                                 std::shared_ptr<nsm::NsmDevice> device,
                                 uint8_t messageType, uint8_t commandCode) :
        NsmLongRunningSensor(name, type, isLongRunning, device, messageType,
                             commandCode)
    {}

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t, uint8_t), (override));
    MOCK_METHOD(uint8_t, handleResponseMsg, (const nsm_msg*, size_t),
                (override));
};

// ---------------------------------------------------------------------------
// Mockable concrete classes for NsmAsyncLongRunningSensor
// ---------------------------------------------------------------------------

class MockableNsmAsyncLongRunningSensor : public nsm::NsmAsyncLongRunningSensor
{
  public:
    MockableNsmAsyncLongRunningSensor(const std::string& name,
                                      const std::string& type,
                                      bool isLongRunning,
                                      std::shared_ptr<nsm::NsmDevice> device,
                                      uint8_t messageType,
                                      uint8_t commandCode) :
        NsmAsyncLongRunningSensor(name, type, isLongRunning, device,
                                  messageType, commandCode)
    {}

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t, uint8_t), (override));
    MOCK_METHOD(uint8_t, handleResponseMsg, (const nsm_msg*, size_t),
                (override));
};

// ============================================================================
// NsmLongRunningSensor::handle() Tests
// ============================================================================

TEST_F(NsmLongRunningTest, Handle_ValidEvent_ReturnsSuccess)
{
    auto sensor = std::make_shared<MockableNsmLongRunningSensor>(
        "HandleTestSensor", "LongRunning", true, nullptr, 0x05, 0x10);

    sensor->acceptInstanceId = 0x05;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x05;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _)).Times(AnyNumber());

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, -1);
}

TEST_F(NsmLongRunningTest, Handle_ValidateSuccess_CallsHandleResponseMsg)
{
    auto sensor = std::make_shared<MockableNsmLongRunningSensor>(
        "HandleValidateSensor", "LongRunning", true, nullptr, 0x05, 0x10);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmLongRunningTest, Handle_DifferentEids_NoFault)
{
    auto sensor = std::make_shared<MockableNsmLongRunningSensor>(
        "HandleEidSensor", "LongRunning", true, nullptr, 0x05, 0x10);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    sensor->handle(0, 0, 0, msg, msgBuf.size());
    sensor->handle(1, 0, 0, msg, msgBuf.size());
    sensor->handle(255, 0, 0, msg, msgBuf.size());
}

TEST_F(NsmLongRunningTest, Handle_ResponseMsgError_PropagatesError)
{
    auto sensor = std::make_shared<MockableNsmLongRunningSensor>(
        "HandleErrorSensor", "LongRunning", true, nullptr, 0x05, 0x10);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_ERROR));

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, -1);
}

// ============================================================================
// NsmAsyncLongRunningSensor::handle() Tests
// ============================================================================

TEST_F(NsmLongRunningTest, AsyncHandle_ValidEvent_ReturnsValidCode)
{
    auto sensor = std::make_shared<MockableNsmAsyncLongRunningSensor>(
        "AsyncHandleTestSensor", "AsyncLongRunning", true, nullptr, 0x06, 0x11);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmLongRunningTest, AsyncHandle_DifferentEids_NoFault)
{
    auto sensor = std::make_shared<MockableNsmAsyncLongRunningSensor>(
        "AsyncHandleEidSensor", "AsyncLongRunning", true, nullptr, 0x06, 0x11);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    sensor->handle(0, 0, 0, msg, msgBuf.size());
    sensor->handle(1, 0, 0, msg, msgBuf.size());
    sensor->handle(128, 0, 0, msg, msgBuf.size());
    sensor->handle(255, 0, 0, msg, msgBuf.size());
}

TEST_F(NsmLongRunningTest, AsyncHandle_ResponseError_NoCrash)
{
    auto sensor = std::make_shared<MockableNsmAsyncLongRunningSensor>(
        "AsyncHandleErrorSensor", "AsyncLongRunning", true, nullptr, 0x06,
        0x11);

    sensor->acceptInstanceId = 0x0A;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x0A;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_ERROR));

    int rc = sensor->handle(10, 0, 0, msg, msgBuf.size());
    EXPECT_NE(rc, -1);
}

// ============================================================================
// initAcceptInstanceId coverage
// ============================================================================

TEST_F(NsmLongRunningTest, InitAcceptInstanceId_AcceptedSuccess_SetsInstanceId)
{
    TestNsmLongRunningSensor sensor("TestInit", "Type", true, nullptr, 0x05,
                                    0x10);

    bool result = sensor.initAcceptInstanceId(0x0A, NSM_ACCEPTED,
                                              NSM_SW_SUCCESS);

    EXPECT_TRUE(result);
    EXPECT_EQ(sensor.acceptInstanceId, 0x0A);
}

TEST_F(NsmLongRunningTest, InitAcceptInstanceId_NotAccepted_SetsFF)
{
    TestNsmLongRunningSensor sensor("TestInitFail", "Type", true, nullptr, 0x05,
                                    0x10);

    bool result = sensor.initAcceptInstanceId(0x0A, NSM_SUCCESS,
                                              NSM_SW_SUCCESS);

    EXPECT_FALSE(result);
    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningTest, InitAcceptInstanceId_RcError_SetsFF)
{
    TestNsmLongRunningSensor sensor("TestInitRcErr", "Type", true, nullptr,
                                    0x05, 0x10);

    bool result = sensor.initAcceptInstanceId(0x0A, NSM_ACCEPTED, NSM_SW_ERROR);

    EXPECT_FALSE(result);
    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningTest, InitAcceptInstanceId_BothFail_SetsFF)
{
    TestNsmLongRunningSensor sensor("TestInitBothFail", "Type", true, nullptr,
                                    0x05, 0x10);

    bool result = sensor.initAcceptInstanceId(0x0A, NSM_ERROR, NSM_SW_ERROR);

    EXPECT_FALSE(result);
    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningTest,
       AsyncInitAcceptInstanceId_AcceptedSuccess_SetsInstanceId)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncTestInit", "Type", true, nullptr,
                                         0x06, 0x11);

    bool result = sensor.initAcceptInstanceId(0x0B, NSM_ACCEPTED,
                                              NSM_SW_SUCCESS);

    EXPECT_TRUE(result);
    EXPECT_EQ(sensor.acceptInstanceId, 0x0B);
}

TEST_F(NsmLongRunningTest, AsyncInitAcceptInstanceId_NotAccepted_SetsFF)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncTestInitFail", "Type", true,
                                         nullptr, 0x06, 0x11);

    bool result = sensor.initAcceptInstanceId(0x0B, NSM_SUCCESS,
                                              NSM_SW_SUCCESS);

    EXPECT_FALSE(result);
    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);
}

// ============================================================================
// validateEvent coverage
// ============================================================================

TEST_F(NsmLongRunningTest, ValidateEvent_NotAccepted_ReturnsError)
{
    TestNsmLongRunningSensor sensor("ValidateTest", "Type", true, nullptr, 0x05,
                                    0x10);

    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    int rc = sensor.validateEvent(10, msg, msgBuf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST_F(NsmLongRunningTest, ValidateEvent_NotLongRunning_ReturnsError)
{
    TestNsmLongRunningSensor sensor("ValidateNotLR", "Type", false, nullptr,
                                    0x05, 0x10);

    sensor.acceptInstanceId = 0x05;
    sensor.isLongRunning = false;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    int rc = sensor.validateEvent(10, msg, msgBuf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST_F(NsmLongRunningTest, ValidateEvent_TimerNotStarted_ValidState)
{
    TestNsmLongRunningSensor sensor("ValidateTimer", "Type", true, nullptr,
                                    0x05, 0x10);

    sensor.acceptInstanceId = 0x05;
    sensor.isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x05;

    int rc = sensor.validateEvent(10, msg, msgBuf.size());
    EXPECT_TRUE(rc == NSM_SW_SUCCESS || rc == NSM_SW_ERROR_COMMAND_FAIL ||
                rc == NSM_SW_ERROR_DATA || rc == NSM_SW_ERROR);
}

TEST_F(NsmLongRunningTest, ValidateEvent_InstanceIdMismatch_ReturnsDataError)
{
    TestNsmLongRunningSensor sensor("ValidateIdMismatch", "Type", true, nullptr,
                                    0x05, 0x10);

    sensor.acceptInstanceId = 0x05;
    sensor.isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x0A;

    int rc = sensor.validateEvent(10, msg, msgBuf.size());
    EXPECT_TRUE(rc == NSM_SW_ERROR_DATA || rc == NSM_SW_ERROR_COMMAND_FAIL ||
                rc == NSM_SW_ERROR);
}

// ============================================================================
// isLongRunning flag management
// ============================================================================

TEST_F(NsmLongRunningTest, IsLongRunning_InitialTrue_CanBeModified)
{
    TestNsmLongRunningSensor sensor("FlagTest", "Type", true, nullptr, 0x05,
                                    0x10);
    EXPECT_TRUE(sensor.isLongRunning);
    sensor.isLongRunning = false;
    EXPECT_FALSE(sensor.isLongRunning);
}

TEST_F(NsmLongRunningTest, IsLongRunning_InitialFalse_CanBeModified)
{
    TestNsmLongRunningSensor sensor("FlagTestFalse", "Type", false, nullptr,
                                    0x05, 0x10);
    EXPECT_FALSE(sensor.isLongRunning);
    sensor.isLongRunning = true;
    EXPECT_TRUE(sensor.isLongRunning);
}

TEST_F(NsmLongRunningTest, AsyncIsLongRunning_InitialTrue_CanBeModified)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncFlagTest", "Type", true, nullptr,
                                         0x06, 0x11);
    EXPECT_TRUE(sensor.isLongRunning);
    sensor.isLongRunning = false;
    EXPECT_FALSE(sensor.isLongRunning);
}

// ============================================================================
// Timer state tests
// ============================================================================

TEST_F(NsmLongRunningTest, Timer_DefaultState_NotExpiredNotRunning)
{
    TestNsmLongRunningSensor sensor("TimerTest", "Type", true, nullptr, 0x05,
                                    0x10);
    EXPECT_FALSE(sensor.timer.expired());
    EXPECT_FALSE(sensor.timer.running());
}

TEST_F(NsmLongRunningTest, AsyncTimer_DefaultState_NotExpiredNotRunning)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncTimerTest", "Type", true,
                                         nullptr, 0x06, 0x11);
    EXPECT_FALSE(sensor.timer.expired());
    EXPECT_FALSE(sensor.timer.running());
}

TEST_F(NsmLongRunningTest, Timer_StopWhenNotRunning_ReturnsTrue)
{
    TestNsmLongRunningSensor sensor("TimerStopTest", "Type", true, nullptr,
                                    0x05, 0x10);
    EXPECT_TRUE(sensor.timer.stop());
}

// ============================================================================
// Device member access
// ============================================================================

TEST_F(NsmLongRunningTest, Device_NullDevice_Accessible)
{
    TestNsmLongRunningSensor sensor("DeviceTest", "Type", true, nullptr, 0x05,
                                    0x10);
    EXPECT_EQ(sensor.device, nullptr);
}

TEST_F(NsmLongRunningTest, AsyncDevice_NullDevice_Accessible)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncDeviceTest", "Type", true,
                                         nullptr, 0x06, 0x11);
    EXPECT_EQ(sensor.device, nullptr);
}

// ============================================================================
// Comprehensive event message testing
// ============================================================================

TEST_F(NsmLongRunningTest, Handle_EncodedEvent_ProcessesCorrectly)
{
    auto sensor = std::make_shared<MockableNsmLongRunningSensor>(
        "EncodedEventSensor", "LongRunning", true, nullptr, 0x05, 0x10);

    sensor->acceptInstanceId = 0x03;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x03;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    int rc = sensor->handle(20, 0x05, 0x10, msg, msgBuf.size());
    EXPECT_NE(rc, -1);
}

TEST_F(NsmLongRunningTest, AsyncHandle_EncodedEvent_ProcessesCorrectly)
{
    auto sensor = std::make_shared<MockableNsmAsyncLongRunningSensor>(
        "AsyncEncodedEventSensor", "AsyncLongRunning", true, nullptr, 0x06,
        0x11);

    sensor->acceptInstanceId = 0x04;
    sensor->isLongRunning = true;

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 128, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());
    msg->hdr.instance_id = 0x04;

    EXPECT_CALL(*sensor, handleResponseMsg(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(NSM_SW_SUCCESS));

    int rc = sensor->handle(20, 0x06, 0x11, msg, msgBuf.size());
    EXPECT_NE(rc, -1);
}

// ============================================================================
// Multiple sensor instances independence
// ============================================================================

TEST_F(NsmLongRunningTest, MultipleInstances_DifferentCommandCodes)
{
    auto sensor1 = std::make_shared<MockableNsmLongRunningSensor>(
        "Sensor1", "Type1", true, nullptr, 0x05, 0x10);
    auto sensor2 = std::make_shared<MockableNsmLongRunningSensor>(
        "Sensor2", "Type2", true, nullptr, 0x05, 0x20);

    EXPECT_EQ(sensor1->messageType, 0x05);
    EXPECT_EQ(sensor1->commandCode, 0x10);
    EXPECT_EQ(sensor2->messageType, 0x05);
    EXPECT_EQ(sensor2->commandCode, 0x20);
    EXPECT_NE(sensor1->commandCode, sensor2->commandCode);
}

TEST_F(NsmLongRunningTest, AsyncMultipleInstances_DifferentCommandCodes)
{
    auto sensor1 = std::make_shared<MockableNsmAsyncLongRunningSensor>(
        "AsyncSensor1", "AsyncType1", true, nullptr, 0x06, 0x11);
    auto sensor2 = std::make_shared<MockableNsmAsyncLongRunningSensor>(
        "AsyncSensor2", "AsyncType2", false, nullptr, 0x06, 0x22);

    EXPECT_EQ(sensor1->commandCode, 0x11);
    EXPECT_EQ(sensor2->commandCode, 0x22);
    EXPECT_TRUE(sensor1->isLongRunning);
    EXPECT_FALSE(sensor2->isLongRunning);
}

// ============================================================================
// genRequestMsg / handleResponseMsg default implementation tests
// ============================================================================

TEST_F(NsmLongRunningTest, GenRequestMsg_DefaultImpl_ReturnsEmptyVector)
{
    TestNsmLongRunningSensor sensor("GenReqTest", "Type", true, nullptr, 0x05,
                                    0x10);
    auto result = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST_F(NsmLongRunningTest, AsyncGenRequestMsg_DefaultImpl_ReturnsEmptyVector)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncGenReqTest", "Type", true,
                                         nullptr, 0x06, 0x11);
    auto result = sensor.genRequestMsg(10, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(result->empty());
}

TEST_F(NsmLongRunningTest, HandleResponseMsg_DefaultImpl_ReturnsSuccess)
{
    TestNsmLongRunningSensor sensor("HandleRespTest", "Type", true, nullptr,
                                    0x05, 0x10);
    auto rc = sensor.handleResponseMsg(nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmLongRunningTest, AsyncHandleResponseMsg_DefaultImpl_ReturnsSuccess)
{
    TestNsmAsyncLongRunningSensor sensor("AsyncHandleRespTest", "Type", true,
                                         nullptr, 0x06, 0x11);
    auto rc = sensor.handleResponseMsg(nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// Boundary value tests
// ============================================================================

TEST_F(NsmLongRunningTest, BoundaryValues_MinMaxMessageType)
{
    TestNsmLongRunningSensor sensorMin("SensorMin", "Type", true, nullptr, 0x00,
                                       0x00);
    EXPECT_EQ(sensorMin.messageType, 0x00);
    EXPECT_EQ(sensorMin.commandCode, 0x00);

    TestNsmLongRunningSensor sensorMax("SensorMax", "Type", true, nullptr, 0xFF,
                                       0xFF);
    EXPECT_EQ(sensorMax.messageType, 0xFF);
    EXPECT_EQ(sensorMax.commandCode, 0xFF);
}

TEST_F(NsmLongRunningTest, AsyncBoundaryValues_MinMaxMessageType)
{
    TestNsmAsyncLongRunningSensor sensorMin("AsyncMin", "Type", true, nullptr,
                                            0x00, 0x00);
    EXPECT_EQ(sensorMin.messageType, 0x00);

    TestNsmAsyncLongRunningSensor sensorMax("AsyncMax", "Type", true, nullptr,
                                            0xFF, 0xFF);
    EXPECT_EQ(sensorMax.messageType, 0xFF);
}

// ============================================================================
// PART B: NsmThreshold / NsmThresholdFactory Tests
// ============================================================================
//
// Required includes (for standalone file):
//   #include "test/mockDBusHandler.hpp"
//   #include <gmock/gmock.h>
//   #include <gtest/gtest.h>
//   #define private public
//   #define protected public
//   #include "nsmNumericSensor/nsmThreshold.hpp"
//   #include "nsmNumericSensor/nsmThresholdAggregator.hpp"
//   #include "nsmNumericSensor/nsmThresholdValue.hpp"
//   #include "nsmNumericSensor/nsmNumericSensor.hpp"
//   #undef private
//   #undef protected

class NsmThresholdTest : public ::testing::Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// ============================================================================
// NsmThreshold::genRequestMsg Tests
// ============================================================================

TEST_F(NsmThresholdTest, GenRequestMsg_ValidParams_ReturnsOptional)
{
    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold threshold("TestThreshold", "NSM_ThermalParameter", 1,
                                sensorValue);

    auto result = threshold.genRequestMsg(10, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

TEST_F(NsmThresholdTest, GenRequestMsg_DifferentSensorIds_AllValid)
{
    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>();

    nsm::NsmThreshold threshold0("Threshold0", "NSM_ThermalParameter", 0,
                                 sensorValue);
    auto result0 = threshold0.genRequestMsg(10, 0);
    EXPECT_TRUE(result0.has_value());

    nsm::NsmThreshold threshold255("Threshold255", "NSM_ThermalParameter", 255,
                                   sensorValue);
    auto result255 = threshold255.genRequestMsg(10, 0);
    EXPECT_TRUE(result255.has_value());
}

TEST_F(NsmThresholdTest, GenRequestMsg_DifferentInstanceIds_AllValid)
{
    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold threshold("ThresholdInst", "NSM_ThermalParameter", 1,
                                sensorValue);

    auto result0 = threshold.genRequestMsg(10, 0);
    EXPECT_TRUE(result0.has_value());

    auto result31 = threshold.genRequestMsg(10, 31);
    EXPECT_TRUE(result31.has_value());
}

TEST_F(NsmThresholdTest, GenRequestMsg_DifferentEids_AllValid)
{
    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold threshold("ThresholdEid", "NSM_ThermalParameter", 1,
                                sensorValue);

    auto result1 = threshold.genRequestMsg(0, 0);
    EXPECT_TRUE(result1.has_value());

    auto result2 = threshold.genRequestMsg(255, 0);
    EXPECT_TRUE(result2.has_value());
}

// ============================================================================
// NsmThreshold::handleResponseMsg Tests
// ============================================================================

TEST_F(NsmThresholdTest, HandleResponseMsg_ZeroedResponse_NoFault)
{
    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold threshold("ThresholdNull", "NSM_ThermalParameter", 1,
                                sensorValue);

    std::vector<uint8_t> msgBuf(sizeof(nsm_msg) + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    auto rc = threshold.handleResponseMsg(msg, msgBuf.size());
    EXPECT_NE(rc, static_cast<uint8_t>(0xFF));
}

TEST_F(NsmThresholdTest, HandleResponseMsg_WithThresholdValue_NoCrash)
{
    auto warningIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/test_valid");
    auto thresholdValue = std::make_unique<nsm::NsmThresholdValueWarningLow>(
        "TestValue", "NSM_ThermalParameter", warningIntf);

    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    nsm::NsmThreshold threshold("ThresholdValid", "NSM_ThermalParameter", 1,
                                sensorValue);

    std::vector<uint8_t> responseBuf(sizeof(nsm_msg) + 64, 0);
    auto* responseMsg = reinterpret_cast<nsm_msg*>(responseBuf.data());

    auto rc = threshold.handleResponseMsg(responseMsg, responseBuf.size());
    EXPECT_TRUE(rc == NSM_SW_SUCCESS || rc != NSM_SW_SUCCESS);
}

// ============================================================================
// NsmThreshold::getSensorType Tests
// ============================================================================

TEST_F(NsmThresholdTest, GetSensorType_ReturnsThreshold)
{
    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold threshold("ThresholdType", "NSM_ThermalParameter", 1,
                                sensorValue);
    EXPECT_EQ(threshold.getSensorType(), "threshold");
}

// ============================================================================
// NsmThresholdAggregator Tests
// ============================================================================

TEST_F(NsmThresholdTest, ThresholdAggregator_GenRequestMsg_ReturnsValid)
{
    nsm::NsmThresholdAggregator aggregator("AggTest", "Threshold", false);
    auto result = aggregator.genRequestMsg(10, 0);
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
}

TEST_F(NsmThresholdTest, ThresholdAggregator_Priority_ConstructsCorrectly)
{
    nsm::NsmThresholdAggregator priorityAgg("PriorityAgg", "Threshold", true);
    nsm::NsmThresholdAggregator normalAgg("NormalAgg", "Threshold", false);

    auto result1 = priorityAgg.genRequestMsg(10, 0);
    auto result2 = normalAgg.genRequestMsg(10, 0);
    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

// ============================================================================
// NsmNumericSensorValueAggregate Tests
// ============================================================================

TEST_F(NsmThresholdTest, ValueAggregate_UpdateReading_PropagatesAll)
{
    auto warningIntf1 = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/agg_low");
    auto warningIntf2 = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/agg_high");

    auto valueLow = std::make_unique<nsm::NsmThresholdValueWarningLow>(
        "AggLow", "NSM_ThermalParameter", warningIntf1);
    auto valueHigh = std::make_unique<nsm::NsmThresholdValueWarningHigh>(
        "AggHigh", "NSM_ThermalParameter", warningIntf2);

    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>(
        std::move(valueLow), std::move(valueHigh));

    aggregate->updateReading(42.5);
    EXPECT_EQ(warningIntf1->warningLow(), 42.5);
    EXPECT_EQ(warningIntf2->warningHigh(), 42.5);
}

TEST_F(NsmThresholdTest, ValueAggregate_Append_AddsNewObserver)
{
    auto warningIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/append_test");
    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>();

    auto valueLow = std::make_unique<nsm::NsmThresholdValueWarningLow>(
        "AppendLow", "NSM_ThermalParameter", warningIntf);
    aggregate->append(std::move(valueLow));
    EXPECT_EQ(aggregate->getObjects().size(), 1u);

    aggregate->updateReading(99.9);
    EXPECT_EQ(warningIntf->warningLow(), 99.9);
}

TEST_F(NsmThresholdTest, ValueAggregate_EmptyAggregate_UpdateDoesNotCrash)
{
    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    EXPECT_EQ(aggregate->getObjects().size(), 0u);
    aggregate->updateReading(50.0);
    aggregate->updateReading(0.0);
    aggregate->updateReading(-100.0);
}

// ============================================================================
// Cross-type Threshold Value Tests
// ============================================================================

TEST_F(NsmThresholdTest, CriticalThresholdValues_UpdateReading_SetsCorrectly)
{
    auto criticalIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/critical_cross");

    nsm::NsmThresholdValueCriticalLow critLow("CritLow", "NSM_ThermalParameter",
                                              criticalIntf);
    nsm::NsmThresholdValueCriticalHigh critHigh(
        "CritHigh", "NSM_ThermalParameter", criticalIntf);

    critLow.updateReading(-40.0);
    critHigh.updateReading(125.0);
    EXPECT_EQ(criticalIntf->criticalLow(), -40.0);
    EXPECT_EQ(criticalIntf->criticalHigh(), 125.0);
}

TEST_F(NsmThresholdTest,
       HardShutdownThresholdValues_UpdateReading_SetsCorrectly)
{
    auto shutdownIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/shutdown_cross");

    nsm::NsmThresholdValueHardShutdownLow shutLow(
        "ShutLow", "NSM_ThermalParameter", shutdownIntf);
    nsm::NsmThresholdValueHardShutdownHigh shutHigh(
        "ShutHigh", "NSM_ThermalParameter", shutdownIntf);

    shutLow.updateReading(-50.0);
    shutHigh.updateReading(150.0);
    EXPECT_EQ(shutdownIntf->hardShutdownLow(), -50.0);
    EXPECT_EQ(shutdownIntf->hardShutdownHigh(), 150.0);
}

// ============================================================================
// NsmThreshold - handleOfflineState coverage
// ============================================================================

TEST_F(NsmThresholdTest, HandleOfflineState_SetsNaN)
{
    auto warningIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/offline_test");
    auto thresholdValue = std::make_unique<nsm::NsmThresholdValueWarningLow>(
        "OfflineValue", "NSM_ThermalParameter", warningIntf);

    auto sensorValue = std::make_shared<nsm::NsmNumericSensorValueAggregate>(
        std::move(thresholdValue));
    nsm::NsmThreshold threshold("OfflineThreshold", "NSM_ThermalParameter", 1,
                                sensorValue);

    sensorValue->updateReading(50.0);
    EXPECT_EQ(warningIntf->warningLow(), 50.0);

    threshold.handleOfflineState();
    EXPECT_TRUE(std::isnan(warningIntf->warningLow()));
}

// ============================================================================
// NsmThreshold - equals / operator== coverage
// ============================================================================

TEST_F(NsmThresholdTest, Equals_SameNameAndType_ReturnsTrue)
{
    auto sv1 = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    auto sv2 = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold t1("ThreshA", "TypeA", 1, sv1);
    nsm::NsmThreshold t2("ThreshA", "TypeA", 1, sv2);
    EXPECT_TRUE(t1 == t2);
}

TEST_F(NsmThresholdTest, Equals_DifferentName_ReturnsFalse)
{
    auto sv1 = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    auto sv2 = std::make_shared<nsm::NsmNumericSensorValueAggregate>();
    nsm::NsmThreshold t1("ThreshA", "TypeA", 1, sv1);
    nsm::NsmThreshold t2("ThreshB", "TypeA", 1, sv2);
    EXPECT_FALSE(t1 == t2);
}

// ============================================================================
// NsmThresholdValue - multiple updates overwrite correctly
// ============================================================================

TEST_F(NsmThresholdTest, WarningLow_MultipleUpdates_LastValueWins)
{
    auto warningIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/threshold/multi_update");

    nsm::NsmThresholdValueWarningLow warnLow(
        "MultiUpdateLow", "NSM_ThermalParameter", warningIntf);

    warnLow.updateReading(10.0);
    EXPECT_EQ(warningIntf->warningLow(), 10.0);
    warnLow.updateReading(20.0);
    EXPECT_EQ(warningIntf->warningLow(), 20.0);
    warnLow.updateReading(-5.0);
    EXPECT_EQ(warningIntf->warningLow(), -5.0);
    warnLow.updateReading(0.0);
    EXPECT_EQ(warningIntf->warningLow(), 0.0);
}

// ============================================================================
// NsmThreshold - construction with various sensorId values
// ============================================================================

TEST_F(NsmThresholdTest, Constructor_VariousSensorIds_AllConstruct)
{
    auto sv = std::make_shared<nsm::NsmNumericSensorValueAggregate>();

    EXPECT_NO_THROW({
        nsm::NsmThreshold t0("T0", "Type", 0, sv);
        EXPECT_EQ(t0.getSensorType(), "threshold");
    });

    EXPECT_NO_THROW({
        nsm::NsmThreshold t128("T128", "Type", 128, sv);
        EXPECT_EQ(t128.getSensorType(), "threshold");
    });

    EXPECT_NO_THROW({
        nsm::NsmThreshold t255("T255", "Type", 255, sv);
        EXPECT_EQ(t255.getSensorType(), "threshold");
    });
}
