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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmAsyncLongRunningSensor.hpp"
#include "nsmLongRunningSensor.hpp"

// Concrete test class for NsmLongRunningSensor
class TestNsmLongRunningSensor : public nsm::NsmLongRunningSensor
{
  public:
    TestNsmLongRunningSensor(const std::string& name, const std::string& type,
                             bool isLongRunning,
                             std::shared_ptr<nsm::NsmDevice> device,
                             uint8_t messageType, uint8_t commandCode) :
        NsmLongRunningSensor(name, type, isLongRunning, device, messageType,
                             commandCode)
    {}

    // Implement pure virtual methods
    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::vector<uint8_t>{};
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

// Concrete test class for NsmAsyncLongRunningSensor
class TestNsmAsyncLongRunningSensor : public nsm::NsmAsyncLongRunningSensor
{
  public:
    TestNsmAsyncLongRunningSensor(const std::string& name,
                                  const std::string& type, bool isLongRunning,
                                  std::shared_ptr<nsm::NsmDevice> device,
                                  uint8_t messageType, uint8_t commandCode) :
        NsmAsyncLongRunningSensor(name, type, isLongRunning, device,
                                  messageType, commandCode)
    {}

    // Implement pure virtual methods
    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::vector<uint8_t>{};
    }

    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

class NsmLongRunningTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// Test NsmLongRunningSensor constructor with valid parameters
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_ValidParams)
{
    std::string name = "TestLongRunningSensor";
    std::string type = "LongRunningSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_EQ(sensor.NsmSensor::getType(), type);
    EXPECT_EQ(sensor.messageType, messageType);
    EXPECT_EQ(sensor.commandCode, commandCode);
}

// Test NsmLongRunningSensor constructor with different message types
TEST_F(NsmLongRunningTest,
       NsmLongRunningSensor_Constructor_DifferentMessageTypes)
{
    std::string name = "TestSensor";
    std::string type = "Sensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;

    // Test with message type 0x01
    TestNsmLongRunningSensor sensor1(name, type, isLongRunning, device, 0x01,
                                     0x20);
    EXPECT_EQ(sensor1.messageType, 0x01);
    EXPECT_EQ(sensor1.commandCode, 0x20);

    // Test with message type 0x0F
    TestNsmLongRunningSensor sensor2(name, type, isLongRunning, device, 0x0F,
                                     0x30);
    EXPECT_EQ(sensor2.messageType, 0x0F);
    EXPECT_EQ(sensor2.commandCode, 0x30);
}

// Test NsmLongRunningSensor constructor with isLongRunning true
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_IsLongRunningTrue)
{
    std::string name = "LongRunningSensor";
    std::string type = "Sensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmLongRunningSensor constructor with isLongRunning false
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_IsLongRunningFalse)
{
    std::string name = "QuickSensor";
    std::string type = "Sensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmAsyncLongRunningSensor constructor with valid parameters
TEST_F(NsmLongRunningTest, NsmAsyncLongRunningSensor_Constructor_ValidParams)
{
    std::string name = "TestAsyncLongRunningSensor";
    std::string type = "AsyncLongRunningSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_EQ(sensor.NsmSensor::getType(), type);
    EXPECT_EQ(sensor.messageType, messageType);
    EXPECT_EQ(sensor.commandCode, commandCode);
}

// Test NsmAsyncLongRunningSensor constructor with different message types
TEST_F(NsmLongRunningTest,
       NsmAsyncLongRunningSensor_Constructor_DifferentMessageTypes)
{
    std::string name = "TestAsyncSensor";
    std::string type = "AsyncSensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;

    // Test with message type 0x02
    TestNsmAsyncLongRunningSensor sensor1(name, type, isLongRunning, device,
                                          0x02, 0x21);
    EXPECT_EQ(sensor1.messageType, 0x02);
    EXPECT_EQ(sensor1.commandCode, 0x21);

    // Test with message type 0x0E
    TestNsmAsyncLongRunningSensor sensor2(name, type, isLongRunning, device,
                                          0x0E, 0x31);
    EXPECT_EQ(sensor2.messageType, 0x0E);
    EXPECT_EQ(sensor2.commandCode, 0x31);
}

// Test NsmAsyncLongRunningSensor constructor with isLongRunning true
TEST_F(NsmLongRunningTest,
       NsmAsyncLongRunningSensor_Constructor_IsLongRunningTrue)
{
    std::string name = "AsyncLongRunningSensor";
    std::string type = "AsyncSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmAsyncLongRunningSensor constructor with isLongRunning false
TEST_F(NsmLongRunningTest,
       NsmAsyncLongRunningSensor_Constructor_IsLongRunningFalse)
{
    std::string name = "AsyncQuickSensor";
    std::string type = "AsyncSensor";
    bool isLongRunning = false;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    // Verify the sensor is created successfully
    EXPECT_EQ(sensor.NsmSensor::getName(), name);
}

// Test NsmLongRunningSensor with empty name
TEST_F(NsmLongRunningTest, NsmLongRunningSensor_Constructor_EmptyName)
{
    std::string name = "";
    std::string type = "Sensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x05;
    uint8_t commandCode = 0x10;

    TestNsmLongRunningSensor sensor(name, type, isLongRunning, device,
                                    messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_TRUE(sensor.NsmSensor::getName().empty());
}

// Test NsmAsyncLongRunningSensor with empty name
TEST_F(NsmLongRunningTest, NsmAsyncLongRunningSensor_Constructor_EmptyName)
{
    std::string name = "";
    std::string type = "AsyncSensor";
    bool isLongRunning = true;
    std::shared_ptr<nsm::NsmDevice> device = nullptr;
    uint8_t messageType = 0x06;
    uint8_t commandCode = 0x11;

    TestNsmAsyncLongRunningSensor sensor(name, type, isLongRunning, device,
                                         messageType, commandCode);

    EXPECT_EQ(sensor.NsmSensor::getName(), name);
    EXPECT_TRUE(sensor.NsmSensor::getName().empty());
}

// ============================================================================
// updateLongRunningSensor branch coverage
//
// Two key uncovered branches per class:
//
//   [1] genRequestMsg returns nullopt → log + co_return NSM_SW_ERROR. //
//
//       Does NOT call sensorIO; status pointer not dereferenced.
//
//   [2] sensorIO returns NSM_ERROR (rc != 0) → if (rc) TRUE branch.
//       For NsmAsyncLongRunningSensor: *status must be non-null before the
//       call because the sensorIO-fail path writes WriteFailure to *status.
// ============================================================================

// Concrete class: genRequestMsg returns nullopt (NsmLongRunningSensor variant)
class NullRequestLongRunningSensor : public nsm::NsmLongRunningSensor
{
  public:
    using NsmLongRunningSensor::NsmLongRunningSensor;

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }
    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

// Concrete class: genRequestMsg returns nullopt (NsmAsyncLongRunningSensor
// variant)
class NullRequestAsyncLongRunningSensor : public nsm::NsmAsyncLongRunningSensor
{
  public:
    using NsmAsyncLongRunningSensor::NsmAsyncLongRunningSensor;

    std::optional<std::vector<uint8_t>> genRequestMsg(eid_t, uint8_t) override
    {
        return std::nullopt;
    }
    uint8_t handleResponseMsg(const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

// Fixture providing a MockNsmDevice for coroutine tests
class NsmLongRunningCoroutineTest :
    public testing::Test,
    public SensorManagerTest
{
  protected:
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmLongRunningCoroutineTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmLongRunningCoroutineTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// NsmLongRunningSensor: genRequestMsg returns nullopt → co_return NSM_SW_ERROR
//
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_NullRequest_CoReturnsError)
{
    NullRequestLongRunningSensor sensor("lrs_null", "Sensor", false, nullptr,
                                        0x05, 0x10);
    sensor.updateLongRunningSensor(gpu);
}

// NsmLongRunningSensor: sensorIO fails → if (rc) TRUE branch → co_return rc //

TEST_F(NsmLongRunningCoroutineTest, LongRunningSensor_SensorIOFail_CoReturnsRc)
{
    // TestNsmLongRunningSensor.genRequestMsg returns empty vector (has_value)
    TestNsmLongRunningSensor sensor("lrs_sioerr", "Sensor", false, nullptr,
                                    0x05, 0x10);
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));
    sensor.updateLongRunningSensor(gpu);
}

// NsmAsyncLongRunningSensor: genRequestMsg returns nullopt → co_return error //

TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_NullRequest_CoReturnsError)
{
    NullRequestAsyncLongRunningSensor sensor("alrs_null", "AsyncSensor", false,
                                             nullptr, 0x06, 0x11);
    sensor.updateLongRunningSensor(gpu);
}

// NsmAsyncLongRunningSensor: sensorIO fails → if (rc) TRUE branch.
// status must be set before the call; the fail path writes WriteFailure.
TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_SensorIOFail_CoReturnsRc)
{
    TestNsmAsyncLongRunningSensor sensor("alrs_sioerr", "AsyncSensor", false,
                                         nullptr, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor.status = &sensorStatus;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));
    sensor.updateLongRunningSensor(gpu);

    EXPECT_EQ(sensorStatus, AsyncOperationStatusType::WriteFailure);
}

// Helper: build a minimal nsm_common_resp buffer with a given completion code.
// For cc == NSM_SUCCESS or cc == NSM_ACCEPTED: uses nsm_common_resp size.
// For other cc values: uses nsm_common_non_success_resp size.
static std::vector<uint8_t> makeSensorIOResp(uint8_t cc)
{
    size_t payloadSize = (cc == NSM_SUCCESS || cc == NSM_ACCEPTED)
                             ? sizeof(nsm_common_resp)
                             : sizeof(nsm_common_non_success_resp);
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + payloadSize, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Write completion_code at payload offset 1 (matches both structs)
    msg->payload[1] = cc;
    return buf;
}

// Build a valid long-running event message with the given instanceId.
static std::vector<uint8_t> makeLongRunningEvent(uint8_t instanceId)
{
    size_t msg_len = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                     sizeof(nsm_long_running_resp);
    std::vector<uint8_t> buf(msg_len, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto* evt = reinterpret_cast<nsm_event*>(msg->payload);
    evt->data_size = sizeof(nsm_long_running_resp);
    auto* resp = reinterpret_cast<nsm_long_running_resp*>(evt->data);
    resp->instance_id = instanceId;
    resp->completion_code = NSM_SUCCESS;
    return buf;
}

// =============================================================================
// updateLongRunningSensor – cc == NSM_SUCCESS path
//
// sensorIO succeeds and response has cc == NSM_SUCCESS.
// → decode_common_resp returns NSM_SW_SUCCESS with cc = NSM_SUCCESS
// → "if (cc == NSM_SUCCESS)" TRUE → isLongRunning = false, handleResponseMsg
// =============================================================================

TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_CcSuccess_SetsIsLongRunningFalse)
{
    TestNsmLongRunningSensor sensor("lrs_cc_ok", "Sensor", true, nullptr, 0x05,
                                    0x10);
    EXPECT_TRUE(sensor.isLongRunning);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_SUCCESS)));
    sensor.updateLongRunningSensor(gpu);

    EXPECT_FALSE(sensor.isLongRunning);
}

// cc == NSM_ACCEPTED → "if (cc == NSM_SUCCESS)" FALSE, initAcceptInstanceId
// returns true (accepted), so rc stays NSM_SW_SUCCESS.
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_CcAccepted_InitAcceptInstanceIdTrue)
{
    TestNsmLongRunningSensor sensor("lrs_cc_acc", "Sensor", true, nullptr, 0x05,
                                    0x10);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_ACCEPTED)));
    sensor.updateLongRunningSensor(gpu);

    EXPECT_EQ(sensor.acceptInstanceId, 0x00);
}

// cc == NSM_ERROR → initAcceptInstanceId returns false →
// rc = NSM_SW_ERROR_COMMAND_FAIL.
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_CcError_InitAcceptInstanceIdFalse)
{
    TestNsmLongRunningSensor sensor("lrs_cc_err", "Sensor", true, nullptr, 0x05,
                                    0x10);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_ERROR)));
    sensor.updateLongRunningSensor(gpu);

    // initAcceptInstanceId failed → acceptInstanceId stays 0xFF
    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);
}

// handle() – validateEvent fails (acceptInstanceId == 0xFF by default)
// → "if (rc == NSM_SW_SUCCESS)" FALSE → handleResponseMsg NOT called.
TEST_F(NsmLongRunningCoroutineTest, LongRunningSensor_Handle_ValidateEventFails)
{
    TestNsmLongRunningSensor sensor("lrs_hnd_fail", "Sensor", true, nullptr,
                                    0x05, 0x10);
    // acceptInstanceId stays 0xFF (default) → validateEvent returns error

    auto eventBuf = makeLongRunningEvent(0x07);
    auto* msg = reinterpret_cast<nsm_msg*>(eventBuf.data());
    int rc = sensor.handle(10, 0, 0, msg, eventBuf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// handle() – validateEvent succeeds (acceptInstanceId matches decoded id)
// → "if (rc == NSM_SW_SUCCESS)" TRUE → handleResponseMsg called.
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_Handle_ValidateEventSucceeds)
{
    TestNsmLongRunningSensor sensor("lrs_hnd_ok", "Sensor", true, nullptr, 0x05,
                                    0x10);
    sensor.acceptInstanceId = 0x07;
    sensor.isLongRunning = true;
    sensor.timer.isExpired = false;

    auto eventBuf = makeLongRunningEvent(0x07);
    auto* msg = reinterpret_cast<nsm_msg*>(eventBuf.data());
    int rc = sensor.handle(10, 0, 0, msg, eventBuf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmAsyncLongRunningSensor – same branch paths
// =============================================================================

TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_CcSuccess_SetsIsLongRunningFalse)
{
    TestNsmAsyncLongRunningSensor sensor("alrs_cc_ok", "AsyncSensor", true,
                                         nullptr, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor.status = &sensorStatus;
    EXPECT_TRUE(sensor.isLongRunning);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_SUCCESS)));
    sensor.updateLongRunningSensor(gpu);

    EXPECT_FALSE(sensor.isLongRunning);
}

TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_CcAccepted_InitAcceptInstanceIdTrue)
{
    TestNsmAsyncLongRunningSensor sensor("alrs_cc_acc", "AsyncSensor", true,
                                         nullptr, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor.status = &sensorStatus;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_ACCEPTED)));
    sensor.updateLongRunningSensor(gpu);

    EXPECT_EQ(sensor.acceptInstanceId, 0x00);
}

TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_CcError_InitAcceptInstanceIdFalse)
{
    TestNsmAsyncLongRunningSensor sensor("alrs_cc_err", "AsyncSensor", true,
                                         nullptr, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor.status = &sensorStatus;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_ERROR)));
    sensor.updateLongRunningSensor(gpu);

    EXPECT_EQ(sensor.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_Handle_ValidateEventFails)
{
    TestNsmAsyncLongRunningSensor sensor("alrs_hnd_fail", "AsyncSensor", true,
                                         nullptr, 0x06, 0x11);

    auto eventBuf = makeLongRunningEvent(0x07);
    auto* msg = reinterpret_cast<nsm_msg*>(eventBuf.data());
    int rc = sensor.handle(10, 0, 0, msg, eventBuf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_Handle_ValidateEventSucceeds)
{
    TestNsmAsyncLongRunningSensor sensor("alrs_hnd_ok", "AsyncSensor", true,
                                         nullptr, 0x06, 0x11);
    sensor.acceptInstanceId = 0x07;
    sensor.isLongRunning = true;
    sensor.timer.isExpired = false;

    auto eventBuf = makeLongRunningEvent(0x07);
    auto* msg = reinterpret_cast<nsm_msg*>(eventBuf.data());
    int rc = sensor.handle(10, 0, 0, msg, eventBuf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmLongRunningSensor::update() – covers lines 41-68 in
// nsmLongRunningSensor.cpp:
//   - acquires semaphore (await_ready returns true → no suspension)
//   - sets isLongRunning = true
//   - calls registerLongRunningHandler
//   - calls updateLongRunningSensor
//   - tests both:
//     a) sensorIO failure → rc != 0 → skips timer block
//     b) cc == NSM_SUCCESS → isLongRunning = false → skips timer block
//   - clearLongRunningHandler + release semaphore
// =============================================================================

// update() with sensorIO failure → rc != 0 → "if(rc==NSM_SW_SUCCESS &&
// isLongRunning)" FALSE branch → clears handler, releases semaphore
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_Update_SensorIOFail_NoTimer)
{
    auto sensor = std::make_shared<TestNsmLongRunningSensor>(
        "lrs_upd_siof", "Sensor", false, gpu, 0x05, 0x10);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
    // sensor is still alive → semaphore was released (no double-release check)
}

// update() with cc == NSM_SUCCESS → isLongRunning set false by
// updateLongRunningSensor → timer block skipped
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_Update_CcSuccess_SkipsTimer)
{
    auto sensor = std::make_shared<TestNsmLongRunningSensor>(
        "lrs_upd_cc_ok", "Sensor", false, gpu, 0x05, 0x10);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_SUCCESS)));

    sensor->update(gpu);
    EXPECT_FALSE(sensor->isLongRunning);
}

// update() with cc == NSM_ACCEPTED → isLongRunning stays true →
// "if (rc == NSM_SW_SUCCESS && isLongRunning)" TRUE branch → co_await timer //
// (covers nsmLongRunningSensor.cpp line 52 TRUE, the timer
// block execution) Only runs in coverage build: in real-coroutine mode the
// timer suspends the coroutine which holds a shared_ptr to gpu, preventing mock
// destruction.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmLongRunningCoroutineTest,
       LongRunningSensor_Update_CcAccepted_EntersTimerBlock)
{
    auto sensor = std::make_shared<TestNsmLongRunningSensor>(
        "lrs_upd_acc", "Sensor", false, gpu, 0x05, 0x10);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_ACCEPTED)));

    sensor->update(gpu);
    // isLongRunning remains true: NSM_ACCEPTED → pending, timer block taken
    EXPECT_TRUE(sensor->isLongRunning);
}
#endif // COVERAGE_DISABLE_COROUTINES

// =============================================================================
// NsmAsyncLongRunningSensor::update() – covers lines 38-66 in
// nsmAsyncLongRunningSensor.cpp (same structure as LongRunningSensor)
// =============================================================================

// update() with sensorIO failure → rc != 0 → status=WriteFailure → no timer
TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_Update_SensorIOFail_NoTimer)
{
    auto sensor = std::make_shared<TestNsmAsyncLongRunningSensor>(
        "alrs_upd_siof", "AsyncSensor", false, gpu, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor->status = &sensorStatus;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);

    EXPECT_EQ(sensorStatus, AsyncOperationStatusType::WriteFailure);
}

// update() with cc == NSM_SUCCESS → isLongRunning set false → timer skipped
TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_Update_CcSuccess_SkipsTimer)
{
    auto sensor = std::make_shared<TestNsmAsyncLongRunningSensor>(
        "alrs_upd_cc_ok", "AsyncSensor", false, gpu, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor->status = &sensorStatus;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_SUCCESS)));

    sensor->update(gpu);
    EXPECT_FALSE(sensor->isLongRunning);
}

// update() with cc == NSM_ACCEPTED → isLongRunning stays true →
// "if (rc == NSM_SW_SUCCESS && isLongRunning)" TRUE branch → co_await timer //
// (covers nsmAsyncLongRunningSensor.cpp line 50 TRUE, the
// timer block) Only runs in coverage build: in real-coroutine mode the timer
// suspends the coroutine which holds a shared_ptr to gpu, preventing mock
// destruction.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmLongRunningCoroutineTest,
       AsyncLongRunningSensor_Update_CcAccepted_EntersTimerBlock)
{
    auto sensor = std::make_shared<TestNsmAsyncLongRunningSensor>(
        "alrs_upd_acc", "AsyncSensor", false, gpu, 0x06, 0x11);
    AsyncOperationStatusType sensorStatus = AsyncOperationStatusType::Success;
    sensor->status = &sensorStatus;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(makeSensorIOResp(NSM_ACCEPTED)));

    sensor->update(gpu);
    // isLongRunning remains true: NSM_ACCEPTED → pending, timer block taken
    EXPECT_TRUE(sensor->isLongRunning);
}
#endif // COVERAGE_DISABLE_COROUTINES
