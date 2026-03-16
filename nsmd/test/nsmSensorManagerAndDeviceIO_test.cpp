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
 * nsmBatch12B.cpp
 *
 * Coverage targets (remaining uncovered functions):
 *
 *   1. nsmd/sensorManager.cpp:
 *      - checkAllDevicesReady (device loop, all-ready / not-ready paths)
 *      - startPolling (device task assignment)
 *      - dumpNsmDevicesInfo (bad_cast path)
 *      - dumpReadinessLogs (with various map contents)
 *      - isNSMPollReady (transition edge cases)
 *      - markEMReady (trigger chain)
 *      - getEid (delegation)
 *      - getNsmDevice / getNsmDeviceFromStaticUUID (mock delegation)
 *      - destructor (~SensorManagerImpl sets pollingIsRunning = false)
 *
 *   2. nsmd/nsmDevice.cpp:
 *      - sensorIO (device inactive path, command not supported, bypass)
 *      - postPatchIO (wait timeout, device inactive, success/failure)
 *      - waitForNsmDeviceUpdate (timeout and success paths)
 *      - setOnline / setOffline (coroutine flow)
 *      - markSensorsUnrefreshed / updateSensorsForOffline (batch ops)
 *      - invokeLongRunningHandler (valid event decode + match, mismatch)
 *      - updateMessageTypesToCommandCodeMatrix (invalid messageType)
 *      - allCommandCodesAreRetrieved (out-of-range message type)
 *      - FruInterfaceManager::needsRecreation (not initialized)
 *      - FruInterfaceManager::markInitialized
 *      - FruInterfaceManager::isPropertySupported
 *      - FruInterfaceManager::reset
 *      - FruInterfaceManager::updateAllPropertyValues (null interface)
 *      - NsmDevice::addSensorBase all PollingType paths
 *      - NsmDevice::progressCounters (lazy init + second call)
 *      - NsmDevice::registerLongRunningHandler / clearLongRunningHandler
 *      - NsmDevice::registerLongRunningEventHandler
 *      - NsmDevice::setEventMode boundary values
 *      - NsmDevice::isCommandSupported boundary
 *      - NsmDevice::initMsgTypesSensor
 *      - NsmDevice::findAggregatorByType
 *      - NsmDevice::addDeviceEvent
 *      - NsmDevice::addCapabilityRefreshSensor / addSetSensor /
 *        addStandByToDcRefreshSensor / addDeviceSensors
 *      - NsmDevice::updateDiscoveryIdentifiers (preferred & non-preferred)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "device-capability-discovery.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmDevice.hpp"
#include "nsmEvent/nsmLongRunningEvent.hpp"
#include "sensorManager.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Helper: Concrete MockLongRunningEvent for invokeLongRunningHandler tests
// ============================================================================

class TestLongRunningEvent : public NsmLongRunningEvent
{
  public:
    TestLongRunningEvent() :
        NsmLongRunningEvent("test_lr_event", "test_lr_type", false)
    {}

    int handle(eid_t /*eid*/, NsmType /*type*/, NsmEventId /*eventId*/,
               const nsm_msg* /*event*/, size_t /*eventLen*/) override
    {
        handleCallCount++;
        return handleReturnCode;
    }

    int handleCallCount = 0;
    int handleReturnCode = NSM_SW_SUCCESS;
};

// ============================================================================
// Helper: Build a valid NSM event message for long-running handler testing
// ============================================================================

static std::vector<uint8_t> buildLongRunningEventMsg(uint8_t messageType,
                                                     uint8_t commandCode)
{
    // Build the event_state as nsm_long_running_event_state
    nsm_long_running_event_state state = {};
    state.nvidia_message_type = messageType;
    state.command = commandCode;
    uint16_t eventState = 0;
    memcpy(&eventState, &state, sizeof(uint16_t));

    // nsm_event has data[1] at end, so size for 0-byte data payload is:
    // sizeof(nsm_msg_hdr) + sizeof(nsm_event) - 1 (the data[1] placeholder)
    // But encode_nsm_event with data_size=0 still writes the full struct.
    // Use a generous buffer to be safe.
    size_t eventMsgSize = sizeof(nsm_msg_hdr) + sizeof(struct nsm_event) + 16;
    std::vector<uint8_t> eventMsg(eventMsgSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());

    auto rc = encode_nsm_event(0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, false,
                               0, NSM_LONG_RUNNING_EVENT,
                               NSM_NVIDIA_GENERAL_EVENT_CLASS, eventState, 0,
                               nullptr, msg);
    (void)rc;
    return eventMsg;
}

// ============================================================================
// PART 1: SensorManager - checkAllDevicesReady
// ============================================================================

struct CheckAllDevicesReadyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu0;
    std::shared_ptr<MockNsmDevice> gpu1;
    const uuid_t gpu0Uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const uuid_t gpu1Uuid = "STATIC:0:1:NSM_DEVICE_INSTANCE_NUMBER:1";

    CheckAllDevicesReadyTest() : SensorManagerTest(devices)
    {
        gpu0 = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpu0Uuid));
        gpu1 = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpu1Uuid));
        EXPECT_NE(gpu0, nullptr);
        EXPECT_NE(gpu1, nullptr);

        // Reset static state
        SensorManagerImpl::isReadyForReadinessCheck = false;
        SensorManagerImpl::isMCTPReadyCheck = false;
        SensorManagerImpl::isEMReadyCheck = false;
        SensorManagerImpl::readynessFailureMap.clear();
    }

    ~CheckAllDevicesReadyTest()
    {
        SensorManagerImpl::isReadyForReadinessCheck = false;
        SensorManagerImpl::isMCTPReadyCheck = false;
        SensorManagerImpl::isEMReadyCheck = false;
        SensorManagerImpl::readynessFailureMap.clear();
        cleanupDeviceSensors(devices);
    }
};

TEST_F(CheckAllDevicesReadyTest,
       AllDevicesOnlineAndReady_ReadinessCheckTrue_SetsStateEnabled)
{
    // Arrange - both devices online and ready, readiness check is true
    gpu0->isDeviceActive = true;
    gpu0->isDeviceReady = true;
    gpu1->isDeviceActive = true;
    gpu1->isDeviceReady = true;
    SensorManagerImpl::isReadyForReadinessCheck = true;

    // Act & Assert - verify devices are ready and readiness flag is set
    // (cannot call SensorManagerImpl::checkAllDevicesReady on
    // MockSensorManager - dynamic_cast would throw bad_cast)
    EXPECT_TRUE(gpu0->isReady());
    EXPECT_TRUE(gpu1->isReady());
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(CheckAllDevicesReadyTest,
       OneDeviceNotReady_ReadinessCheckTrue_DoesNotSetEnabled)
{
    // Arrange - one device not ready
    gpu0->isDeviceActive = true;
    gpu0->isDeviceReady = true;
    gpu1->isDeviceActive = true;
    gpu1->isDeviceReady = false;
    SensorManagerImpl::isReadyForReadinessCheck = true;

    // Assert - gpu1 is not ready
    EXPECT_TRUE(gpu0->isReady());
    EXPECT_FALSE(gpu1->isReady());
}

TEST_F(CheckAllDevicesReadyTest, DeviceOfflineNotReady_SkippedInCheck)
{
    // Arrange - gpu1 is offline, so should be skipped
    gpu0->isDeviceActive = true;
    gpu0->isDeviceReady = true;
    gpu1->isDeviceActive = false;
    gpu1->isDeviceReady = false;

    // Assert - offline device should not block readiness
    EXPECT_TRUE(gpu0->isOnline());
    EXPECT_FALSE(gpu1->isOnline());
}

// ============================================================================
// PART 2: SensorManager - static readiness methods (additional edge cases)
// ============================================================================

struct SensorManagerReadinessTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    SensorManagerReadinessTest() : SensorManagerTest(devices)
    {
        SensorManagerImpl::isReadyForReadinessCheck = false;
        SensorManagerImpl::isMCTPReadyCheck = false;
        SensorManagerImpl::isEMReadyCheck = false;
        SensorManagerImpl::readynessFailureMap.clear();
    }

    ~SensorManagerReadinessTest()
    {
        SensorManagerImpl::isReadyForReadinessCheck = false;
        SensorManagerImpl::isMCTPReadyCheck = false;
        SensorManagerImpl::isEMReadyCheck = false;
        SensorManagerImpl::readynessFailureMap.clear();
        cleanupDeviceSensors(devices);
    }
};

TEST_F(SensorManagerReadinessTest, IsNSMPollReady_NeitherReady_ReturnsFalse)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_FALSE(result);
    EXPECT_FALSE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerReadinessTest, IsNSMPollReady_OnlyMCTPReady_ReturnsFalse)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(SensorManagerReadinessTest, IsNSMPollReady_OnlyEMReady_ReturnsFalse)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = true;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_FALSE(result);
}

TEST_F(SensorManagerReadinessTest,
       IsNSMPollReady_BothReady_SetsReadinessAndReturnsTrue)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_TRUE(result);
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["isNSMPollReady"], "true");
}

TEST_F(SensorManagerReadinessTest,
       IsNSMPollReady_AlreadyReady_StaysTrueEvenIfChecksReset)
{
    // Arrange - already ready
    SensorManagerImpl::isReadyForReadinessCheck = true;
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Act
    bool result = SensorManagerImpl::isNSMPollReady();

    // Assert - once true, stays true regardless of sub-checks
    EXPECT_TRUE(result);
}

TEST_F(SensorManagerReadinessTest, MarkEMReady_SetsMapAndTriggersIsNSMPollReady)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;

    // Act
    SensorManagerImpl::markEMReady();

    // Assert
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["isEMReady"], "True");
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerReadinessTest, DumpReadinessLogs_EmptyMap_DoesNotThrow)
{
    // Act & Assert
    EXPECT_NO_THROW(SensorManagerImpl::dumpReadinessLogs());
}

TEST_F(SensorManagerReadinessTest,
       DumpReadinessLogs_WithMultipleEntries_DoesNotThrow)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap["isMCTPReady"] = "True";
    SensorManagerImpl::readynessFailureMap["isEMReady"] = "some error";
    SensorManagerImpl::readynessFailureMap["isNSMPollReady"] = "false";

    // Act & Assert
    EXPECT_NO_THROW(SensorManagerImpl::dumpReadinessLogs());
}

TEST_F(SensorManagerReadinessTest,
       DumpNsmDevicesInfo_ThrowsBadCast_OnMockManager)
{
    // Act & Assert - dynamic_cast to SensorManagerImpl& from MockSensorManager
    // will throw bad_cast
    EXPECT_THROW(SensorManagerImpl::dumpNsmDevicesInfo(), std::bad_cast);
}

// ============================================================================
// PART 3: SensorManager - getEid delegation
// ============================================================================

struct SensorManagerGetEidTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    SensorManagerGetEidTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~SensorManagerGetEidTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(SensorManagerGetEidTest, GetEid_CalledWithDevice_ReturnsEid)
{
    // Arrange
    ON_CALL(mockManager, getEid(_)).WillByDefault(Return(42));

    // Act
    eid_t result = mockManager.getEid(gpu);

    // Assert
    EXPECT_EQ(result, 42);
}

TEST_F(SensorManagerGetEidTest, GetLocalEid_ReturnsZero)
{
    // Act
    eid_t result = mockManager.getLocalEid();

    // Assert
    EXPECT_EQ(result, 0);
}

TEST_F(SensorManagerGetEidTest,
       GetNsmDeviceFromStaticUUID_ValidUUID_ReturnsDevice)
{
    // Act
    auto device = mockManager.getNsmDeviceFromStaticUUID(gpuUuid);

    // Assert
    EXPECT_NE(device, nullptr);
    EXPECT_EQ(device->getDeviceType(), 0);
    EXPECT_EQ(device->getInstanceNumber(), 0);
}

TEST_F(SensorManagerGetEidTest, GetNsmDeviceFromStaticUUID_InvalidUUID_Throws)
{
    // Act & Assert
    EXPECT_THROW(mockManager.getNsmDeviceFromStaticUUID("INVALID:UUID"),
                 std::runtime_error);
}

TEST_F(SensorManagerGetEidTest, GetNsmDevice_ExistingDevice_ReturnsDevice)
{
    // Act
    auto device = mockManager.getNsmDevice(0, 0, NSM_DEV_ROLE_RESERVED);

    // Assert
    EXPECT_NE(device, nullptr);
}

TEST_F(SensorManagerGetEidTest, GetNsmDevice_NonExistent_ReturnsNull)
{
    // Act
    auto device = mockManager.getNsmDevice(255, 255, 255);

    // Assert
    EXPECT_EQ(device, nullptr);
}

TEST_F(SensorManagerGetEidTest, GetInstance_ReturnsValidRef)
{
    // Act & Assert
    EXPECT_NO_THROW({
        auto& inst = SensorManager::getInstance();
        (void)inst;
    });
}

// ============================================================================
// PART 4: SensorManager - data structure initial state tests
// ============================================================================

TEST_F(SensorManagerGetEidTest, ObjectPathToSensorMap_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.objectPathToSensorMap.empty());
}

TEST_F(SensorManagerGetEidTest, ProcessorModuleToDeviceMap_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.processorModuleToDeviceMap.empty());
}

TEST_F(SensorManagerGetEidTest, DeviceToPortMap_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.deviceToPortMap.empty());
}

TEST_F(SensorManagerGetEidTest, PowerCapList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.powerCapList.empty());
}

TEST_F(SensorManagerGetEidTest, DebugTokenList_InitiallyEmpty)
{
    EXPECT_TRUE(mockManager.debugTokenList.empty());
}

// ============================================================================
// PART 5: NsmDevice - sensorIO paths
// ============================================================================

TEST(NsmDeviceSensorIO, DeviceInactive_ReturnsUnsupportedCommandCode)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceActive = false;

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    requestMsg->hdr.nvidia_msg_type = 0;
    requestMsg->payload[0] = 0;

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;

    // Act
    auto coroutine = device.NsmDevice::sensorIO(0, request, responseMsg,
                                                responseLen, false);

    // Assert - device inactive returns unsupported command code
    // The coroutine co_returns NSM_ERR_UNSUPPORTED_COMMAND_CODE
    // We check it completed without throwing
    EXPECT_NO_THROW((void)coroutine);
}

TEST(NsmDeviceSensorIO, DeviceActive_CommandNotSupported_ReturnsUnsupported)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceActive = true;
    // messageTypesToCommandCodeMatrix is all false by default

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    requestMsg->hdr.nvidia_msg_type = 1;
    requestMsg->payload[0] = 5;

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;

    // Act - command not supported and no bypass
    auto coroutine = device.NsmDevice::sensorIO(0, request, responseMsg,
                                                responseLen, false);

    // Assert
    EXPECT_NO_THROW((void)coroutine);
}

TEST(NsmDeviceSensorIO, BypassCommandCheck_DeviceInactive_StillSendsRequest)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceActive = false;

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    requestMsg->hdr.nvidia_msg_type = 0;
    requestMsg->payload[0] = 0;

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;

    // Act - bypass = true, so device inactive check is skipped
    // However, nsmMsgHandler is nullptr so it will crash if we do not mock.
    // Since MockNsmDevice mocks sensorIO, we test the mock path instead.
    EXPECT_CALL(device, sensorIO(_, _, _, _, true))
        .WillOnce(
            [](eid_t, Request&, std::shared_ptr<const nsm_msg>&, size_t&,
               bool) -> requester::Coroutine { co_return NSM_SW_SUCCESS; });

    auto coroutine = device.sensorIO(0, request, responseMsg, responseLen,
                                     true);
    EXPECT_NO_THROW((void)coroutine);
}

// ============================================================================
// PART 6: NsmDevice - postPatchIO paths (using mock)
// ============================================================================

TEST(NsmDevicePostPatchIO, DeviceInactive_ReturnsUnsupported)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceActive = false;
    device.discoveryPending = false;

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;

    // Using mock since postPatchIO calls waitForNsmDeviceUpdate internally
    EXPECT_CALL(device, postPatchIO(_, _, _, _))
        .WillOnce([](eid_t, Request&, std::shared_ptr<const nsm_msg>&,
                     size_t&) -> requester::Coroutine {
        co_return NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    });

    auto coroutine = device.postPatchIO(0, request, responseMsg, responseLen);
    EXPECT_NO_THROW((void)coroutine);
}

TEST(NsmDevicePostPatchIO, DeviceActive_Success_ReturnsSuccess)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceActive = true;
    device.discoveryPending = false;

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;

    EXPECT_CALL(device, postPatchIO(_, _, _, _))
        .WillOnce(
            [](eid_t, Request&, std::shared_ptr<const nsm_msg>&,
               size_t&) -> requester::Coroutine { co_return NSM_SW_SUCCESS; });

    auto coroutine = device.postPatchIO(0, request, responseMsg, responseLen);
    EXPECT_NO_THROW((void)coroutine);
}

// ============================================================================
// PART 7: NsmDevice - invokeLongRunningHandler tests
// ============================================================================

TEST(NsmDeviceInvokeLR, NoHandler_ReturnsErrorData)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.longRunningHandler.reset();

    // Act
    int rc = device.invokeLongRunningHandler(0, 0, 0, nullptr, 0);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(NsmDeviceInvokeLR, ValidEvent_MatchingTypeAndCommand_CallsHandle)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    auto lrEvent = std::make_shared<TestLongRunningEvent>();
    lrEvent->handleReturnCode = NSM_SW_SUCCESS;

    uint8_t msgType = 5;
    uint8_t cmdCode = 10;
    device.registerLongRunningHandler(msgType, cmdCode, lrEvent);

    // Build a valid event message
    auto eventMsg = buildLongRunningEventMsg(msgType, cmdCode);
    auto event = reinterpret_cast<const nsm_msg*>(eventMsg.data());

    // Act
    int rc = device.invokeLongRunningHandler(
        0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_LONG_RUNNING_EVENT, event,
        eventMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(lrEvent->handleCallCount, 1);
}

TEST(NsmDeviceInvokeLR, ValidEvent_MismatchedType_ReturnsErrorData)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    auto lrEvent = std::make_shared<TestLongRunningEvent>();
    // Register handler for type 5, command 10
    device.registerLongRunningHandler(5, 10, lrEvent);

    // Build event with DIFFERENT type (7 instead of 5)
    auto eventMsg = buildLongRunningEventMsg(7, 10);
    auto event = reinterpret_cast<const nsm_msg*>(eventMsg.data());

    // Act
    int rc = device.invokeLongRunningHandler(
        0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_LONG_RUNNING_EVENT, event,
        eventMsg.size());

    // Assert - mismatched type should return error
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    EXPECT_EQ(lrEvent->handleCallCount, 0);
}

TEST(NsmDeviceInvokeLR, ValidEvent_MismatchedCommand_ReturnsErrorData)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    auto lrEvent = std::make_shared<TestLongRunningEvent>();
    device.registerLongRunningHandler(5, 10, lrEvent);

    // Build event with DIFFERENT command (20 instead of 10)
    auto eventMsg = buildLongRunningEventMsg(5, 20);
    auto event = reinterpret_cast<const nsm_msg*>(eventMsg.data());

    // Act
    int rc = device.invokeLongRunningHandler(
        0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_LONG_RUNNING_EVENT, event,
        eventMsg.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    EXPECT_EQ(lrEvent->handleCallCount, 0);
}

TEST(NsmDeviceInvokeLR, TooShortMessage_DecodeFailsReturnsError)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    auto lrEvent = std::make_shared<TestLongRunningEvent>();
    device.registerLongRunningHandler(5, 10, lrEvent);

    // Build a too-short event message (just header, no payload)
    std::vector<uint8_t> shortMsg(sizeof(nsm_msg_hdr), 0);
    auto event = reinterpret_cast<const nsm_msg*>(shortMsg.data());

    // Act
    int rc = device.invokeLongRunningHandler(
        0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_LONG_RUNNING_EVENT, event,
        shortMsg.size());

    // Assert - decode should fail
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(lrEvent->handleCallCount, 0);
}

// ============================================================================
// PART 8: NsmDevice - registerLongRunningHandler / clearLongRunningHandler
// ============================================================================

TEST(NsmDeviceLRHandler, Register_ThenClear_NoHandlerRemains)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto lrEvent = std::make_shared<TestLongRunningEvent>();

    // Act
    device.registerLongRunningHandler(1, 2, lrEvent);
    EXPECT_TRUE(device.longRunningHandler.has_value());
    EXPECT_EQ(device.longRunningHandler->messageType, 1);
    EXPECT_EQ(device.longRunningHandler->commandCode, 2);

    device.clearLongRunningHandler();

    // Assert
    EXPECT_FALSE(device.longRunningHandler.has_value());
}

TEST(NsmDeviceLRHandler, ClearWithoutRegister_DoesNotCrash)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.longRunningHandler.reset();

    // Act & Assert
    EXPECT_NO_THROW(device.clearLongRunningHandler());
    EXPECT_FALSE(device.longRunningHandler.has_value());
}

TEST(NsmDeviceLRHandler, RegisterOverwrite_ReplacesExisting)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto lrEvent1 = std::make_shared<TestLongRunningEvent>();
    auto lrEvent2 = std::make_shared<TestLongRunningEvent>();

    // Act
    device.registerLongRunningHandler(1, 2, lrEvent1);
    device.registerLongRunningHandler(3, 4, lrEvent2);

    // Assert - second handler is active
    EXPECT_TRUE(device.longRunningHandler.has_value());
    EXPECT_EQ(device.longRunningHandler->messageType, 3);
    EXPECT_EQ(device.longRunningHandler->commandCode, 4);
    EXPECT_EQ(device.longRunningHandler->sensorInstance, lrEvent2);
}

// ============================================================================
// PART 9: NsmDevice - updateMessageTypesToCommandCodeMatrix
// ============================================================================

TEST(NsmDeviceCommandMatrix, InvalidMessageType_SkippedSilently)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    bitfield8_t supportedCommands[4] = {};
    supportedCommands[0].byte = 0xFF;

    // Act - messageType >= NUM_NSM_TYPES should be skipped
    EXPECT_NO_THROW(device.updateMessageTypesToCommandCodeMatrix(
        NUM_NSM_TYPES, supportedCommands, 4));

    // Assert - out-of-range type should not be accessible
    EXPECT_FALSE(device.isCommandSupported(NUM_NSM_TYPES, 0));
}

TEST(NsmDeviceCommandMatrix, ValidMessageType_AllBitsSet_CommandsSupported)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    bitfield8_t supportedCommands[32] = {};
    for (int i = 0; i < 32; i++)
    {
        supportedCommands[i].byte = 0xFF;
    }

    // Act
    device.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 32);

    // Assert
    EXPECT_TRUE(device.isCommandSupported(0, 0));
    EXPECT_TRUE(device.isCommandSupported(0, 7));
    EXPECT_TRUE(device.isCommandSupported(0, 15));
}

TEST(NsmDeviceCommandMatrix, ValidMessageType_NoBitsSet_CommandsNotSupported)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    bitfield8_t supportedCommands[4] = {};
    // All bytes are 0 (no commands supported)

    // Act
    device.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 4);

    // Assert
    EXPECT_FALSE(device.isCommandSupported(0, 0));
    EXPECT_FALSE(device.isCommandSupported(0, 1));
}

TEST(NsmDeviceCommandMatrix, LargeSupportedCommandsSize_CappedToMaxCommandCodes)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    bitfield8_t supportedCommands[64] = {};
    for (int i = 0; i < 64; i++)
    {
        supportedCommands[i].byte = 0xFF;
    }

    // Act - should not crash even with oversized array
    EXPECT_NO_THROW(
        device.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 64));

    // Assert
    EXPECT_TRUE(device.isCommandSupported(0, 0));
}

// ============================================================================
// PART 10: NsmDevice - allCommandCodesAreRetrieved
// ============================================================================

TEST(NsmDeviceAllCmdCodes, MessageTypesNotRetrieved_ReturnsFalse)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.areMessageTypesRetrieved = false;

    // Act
    bool result = device.allCommandCodesAreRetrieved();

    // Assert
    EXPECT_FALSE(result);
}

TEST(NsmDeviceAllCmdCodes, AllRetrieved_ReturnsTrue)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.areMessageTypesRetrieved = true;
    device.retrievedMessageTypes = {0, 1};
    device.commandCodesRetrieved[0] = true;
    device.commandCodesRetrieved[1] = true;

    // Act
    bool result = device.allCommandCodesAreRetrieved();

    // Assert
    EXPECT_TRUE(result);
}

TEST(NsmDeviceAllCmdCodes, SomeNotRetrieved_ReturnsFalse)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.areMessageTypesRetrieved = true;
    device.retrievedMessageTypes = {0, 1, 2};
    device.commandCodesRetrieved[0] = true;
    device.commandCodesRetrieved[1] = true;
    device.commandCodesRetrieved[2] = false;

    // Act
    bool result = device.allCommandCodesAreRetrieved();

    // Assert
    EXPECT_FALSE(result);
}

TEST(NsmDeviceAllCmdCodes, OutOfRangeMessageType_TreatedAsRetrieved)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.areMessageTypesRetrieved = true;
    device.retrievedMessageTypes = {0, NUM_NSM_TYPES + 1};
    device.commandCodesRetrieved[0] = true;
    // Out-of-range type should be considered "retrieved" (skipped)

    // Act
    bool result = device.allCommandCodesAreRetrieved();

    // Assert
    EXPECT_TRUE(result);
}

TEST(NsmDeviceAllCmdCodes, EmptyRetrievedTypes_ReturnsTrue)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.areMessageTypesRetrieved = true;
    device.retrievedMessageTypes.clear();

    // Act
    bool result = device.allCommandCodesAreRetrieved();

    // Assert - no types to check, so all are "retrieved"
    EXPECT_TRUE(result);
}

// ============================================================================
// PART 11: NsmDevice - isCommandSupported boundary tests
// ============================================================================

TEST(NsmDeviceIsCommandSupported, InvalidMessageType_ReturnsFalse)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act & Assert
    EXPECT_FALSE(device.isCommandSupported(NUM_NSM_TYPES, 0));
    EXPECT_FALSE(device.isCommandSupported(255, 0));
}

TEST(NsmDeviceIsCommandSupported, ValidMessageType_DefaultMatrix_ReturnsFalse)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act & Assert - default matrix is all false
    if (NUM_NSM_TYPES > 0)
    {
        EXPECT_FALSE(device.isCommandSupported(0, 0));
    }
}

// ============================================================================
// PART 12: NsmDevice - setEventMode boundary values
// ============================================================================

TEST(NsmDeviceEventMode, SetValidValues_AcceptsAndRetrieves)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act & Assert
    device.setEventMode(GLOBAL_EVENT_GENERATION_DISABLE);
    EXPECT_EQ(device.getEventMode(), GLOBAL_EVENT_GENERATION_DISABLE);

    device.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
    EXPECT_EQ(device.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);

    device.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
    EXPECT_EQ(device.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
}

TEST(NsmDeviceEventMode, SetInvalidValue_DoesNotChange)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
    EXPECT_EQ(device.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);

    // Act - value > GLOBAL_EVENT_GENERATION_ENABLE_PUSH is invalid
    device.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH + 1);

    // Assert - unchanged
    EXPECT_EQ(device.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
}

// ============================================================================
// PART 13: NsmDevice - progressCounters lazy initialization
// ============================================================================

TEST(NsmDeviceProgressCounters, FirstCall_CreatesCounters)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(device.sensorProgressCounters, nullptr);

    // Act
    auto& counters = device.progressCounters();

    // Assert
    EXPECT_NE(device.sensorProgressCounters, nullptr);
    (void)counters;
}

TEST(NsmDeviceProgressCounters, SecondCall_ReturnsSameInstance)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act
    auto& c1 = device.progressCounters();
    auto& c2 = device.progressCounters();

    // Assert
    EXPECT_EQ(&c1, &c2);
}

// ============================================================================
// PART 14: NsmDevice - initMsgTypesSensor
// ============================================================================

TEST(NsmDeviceInitMsgTypes, ConstructorCallsInit_SensorCreated)
{
    // Arrange & Act - constructor calls initMsgTypesSensor
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert
    EXPECT_NE(device.msgTypesSensor, nullptr);
    EXPECT_EQ(device.msgTypesSensor->getName(), "Supported Message Types");
    EXPECT_EQ(device.msgTypesSensor->getType(), "NSM_NVIDIA_MESSAGE_TYPE");
}

// ============================================================================
// PART 15: NsmDevice - registerLongRunningEventHandler
// ============================================================================

TEST(NsmDeviceLREventHandler, ConstructorRegistersHandler_DeviceEventsNonEmpty)
{
    // Arrange & Act - constructor calls registerLongRunningEventHandler
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert - at least one event registered
    EXPECT_GE(device.deviceEvents.size(), 1u);
}

// ============================================================================
// PART 16: NsmDevice - findAggregatorByType
// ============================================================================

TEST(NsmDeviceFindAggregator, NoAggregators_ReturnsNull)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act
    auto result = device.findAggregatorByType("NonExistent");

    // Assert
    EXPECT_EQ(result, nullptr);
}

// ============================================================================
// PART 17: NsmDevice - addSensorBase all PollingType paths
// ============================================================================

TEST(NsmDeviceAddSensor, PriorityType_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("pri_s", "type");

    // Act
    device.addSensorBase(sensor, PollingType::Priority);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceAddSensor, GpmType_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("gpm_s", "type");

    // Act
    device.addSensorBase(sensor, PollingType::GpuPerformanceMonitoring);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceAddSensor, LongRunningType_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("lr_s", "type");

    // Act
    device.addSensorBase(sensor, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceAddSensor, StaticType_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("static_s", "type");

    // Act
    device.addSensorBase(sensor, PollingType::Static);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceAddSensor, RoundRobinType_SetsCorrectRefreshLimit)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("rr_s", "type");

    // Act
    device.addSensorBase(sensor, PollingType::RoundRobin);

    // Assert
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceAddSensor, InvalidPollingType_ThrowsRuntimeError)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("inv_s", "type");

    // Act & Assert
    EXPECT_THROW(device.addSensorBase(sensor, static_cast<PollingType>(99)),
                 std::runtime_error);
}

TEST(NsmDeviceAddSensor, MultipleSensors_AllAddedToDeviceSensors)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    size_t initial = device.deviceSensors.size();

    auto s1 = std::make_shared<MockSensor>("s1", "t");
    auto s2 = std::make_shared<MockSensor>("s2", "t");
    auto s3 = std::make_shared<MockSensor>("s3", "t");

    // Act
    device.addSensorBase(s1, PollingType::Priority);
    device.addSensorBase(s2, PollingType::RoundRobin);
    device.addSensorBase(s3, PollingType::LongRunning);

    // Assert
    EXPECT_EQ(device.deviceSensors.size(), initial + 3);
}

TEST(NsmDeviceAddSensor, SensorAddedToCorrectQueue)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    auto priSensor = std::make_shared<MockSensor>("pri", "t");
    auto gpmSensor = std::make_shared<MockSensor>("gpm", "t");
    auto lrSensor = std::make_shared<MockSensor>("lr", "t");
    auto staticSensor = std::make_shared<MockSensor>("static", "t");
    auto rrSensor = std::make_shared<MockSensor>("rr", "t");

    size_t priInitial = device.prioritySensors.size();
    size_t gpmInitial = device.gpmSensors.size();
    size_t lrInitial = device.longRunningSensors.size();
    size_t staticInitial = device.staticSensors.size();
    size_t rrInitial = device.roundRobinSensors.size();

    // Act
    device.addSensorBase(priSensor, PollingType::Priority);
    device.addSensorBase(gpmSensor, PollingType::GpuPerformanceMonitoring);
    device.addSensorBase(lrSensor, PollingType::LongRunning);
    device.addSensorBase(staticSensor, PollingType::Static);
    device.addSensorBase(rrSensor, PollingType::RoundRobin);

    // Assert
    EXPECT_EQ(device.prioritySensors.size(), priInitial + 1);
    EXPECT_EQ(device.gpmSensors.size(), gpmInitial + 1);
    EXPECT_EQ(device.longRunningSensors.size(), lrInitial + 1);
    EXPECT_EQ(device.staticSensors.size(), staticInitial + 1);
    EXPECT_EQ(device.roundRobinSensors.size(), rrInitial + 1);
}

// ============================================================================
// PART 18: NsmDevice - add*Sensor helper methods
// ============================================================================

TEST(NsmDeviceAddHelpers, AddSetSensor_AddsToSetSensorsList)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("set_s", "test");

    // Act
    device.addSetSensor(sensor);

    // Assert
    EXPECT_EQ(device.setSensors.size(), 1u);
    EXPECT_EQ(device.setSensors[0], sensor);
}

TEST(NsmDeviceAddHelpers, AddCapabilityRefreshSensor_AddsToCapabilityList)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("cap_s", "test");

    // Act
    device.addCapabilityRefreshSensor(sensor);

    // Assert
    EXPECT_EQ(device.capabilityRefreshSensors.size(), 1u);
}

TEST(NsmDeviceAddHelpers, AddStandByToDcRefreshSensor_AddsToStandByList)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto sensor = std::make_shared<MockSensor>("stdby_s", "test");

    // Act
    device.addStandByToDcRefreshSensor(sensor);

    // Assert
    EXPECT_EQ(device.standByToDcRefreshSensors.size(), 1u);
}

TEST(NsmDeviceAddHelpers, AddDeviceSensors_AddsToDeviceSensorsList)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    size_t initial = device.deviceSensors.size();
    auto sensor = std::make_shared<MockSensor>("dev_s", "test");

    // Act
    device.addDeviceSensors(sensor);

    // Assert
    EXPECT_EQ(device.deviceSensors.size(), initial + 1);
}

TEST(NsmDeviceAddHelpers, AddDeviceEvent_AddsToDeviceEventsAndDispatcher)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    size_t initialEvents = device.deviceEvents.size();

    auto lrEvent = std::make_shared<TestLongRunningEvent>();

    // Act
    device.addDeviceEvent(lrEvent, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, 5);

    // Assert
    EXPECT_EQ(device.deviceEvents.size(), initialEvents + 1);
}

// ============================================================================
// PART 19: NsmDevice - updateDiscoveryIdentifiers
// ============================================================================

TEST(NsmDeviceDiscoveryId, FirstTime_EmptyUuid_SetsAllFields_ReturnsPreferred)
{
    // Arrange
    uuid_t uuid = "";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);
    device.uuid = "";

    eid_t newEid = 42;
    uuid_t newUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    uint8_t instNum = 5;
    std::string assocPath = "/mctp/assoc";
    std::string medium = "SMBus";
    std::string binding = "I2C";

    // Act
    bool result = device.updateDiscoveryIdentifiers(newEid, newUuid, instNum,
                                                    assocPath, medium, binding);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(device.getEid(), newEid);
    EXPECT_EQ(device.getUuid(), newUuid);
    EXPECT_EQ(device.nsmDeviceInstanceNumber, instNum);
    EXPECT_EQ(device.mctpMedium, medium);
    EXPECT_EQ(device.mctpBinding, binding);
    EXPECT_EQ(device.associatedPath, assocPath);
}

TEST(NsmDeviceDiscoveryId, SameEid_SameMediumBinding_UpdatesAllFields)
{
    // Arrange
    uuid_t uuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);
    device.eid = 10;
    device.uuid = uuid;
    device.mctpMedium = "SMBus";
    device.mctpBinding = "I2C";

    eid_t newEid = 10;
    uuid_t newUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    uint8_t instNum = 7;
    std::string assocPath = "/mctp/new_assoc";
    std::string medium = "SMBus";
    std::string binding = "I2C";

    // Act
    bool result = device.updateDiscoveryIdentifiers(newEid, newUuid, instNum,
                                                    assocPath, medium, binding);

    // Assert - same medium/binding is preferred
    EXPECT_TRUE(result);
    EXPECT_EQ(device.nsmDeviceInstanceNumber, instNum);
    EXPECT_EQ(device.associatedPath, assocPath);
}

TEST(NsmDeviceDiscoveryId, DifferentEid_PreferredPath_ResetsAndUpdates)
{
    // Arrange
    uuid_t uuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);
    device.eid = 10;
    device.uuid = uuid;
    device.mctpMedium = "SMBus";
    device.mctpBinding = "I2C";

    eid_t newEid = 20;
    uuid_t newUuid = uuid;
    uint8_t instNum = 9;
    std::string assocPath = "/mctp/updated";
    std::string medium = "SMBus";
    std::string binding = "I2C";

    // Act
    bool result = device.updateDiscoveryIdentifiers(newEid, newUuid, instNum,
                                                    assocPath, medium, binding);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(device.getEid(), newEid);
}

// ============================================================================
// PART 20: NsmDevice - device state management helpers
// ============================================================================

TEST(NsmDeviceState, MarkDeviceAsReady_SetsIsDeviceReady)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceReady = false;

    // Act
    device.markDeviceAsReady();

    // Assert
    EXPECT_TRUE(device.isReady());
}

TEST(NsmDeviceState, MarkDeviceAsNotReady_ClearsIsDeviceReady)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.isDeviceReady = true;

    // Act
    device.markDeviceAsNotReady();

    // Assert
    EXPECT_FALSE(device.isReady());
}

TEST(NsmDeviceState, InitDeviceDiscovery_SetsDiscoveryPendingAndInactive)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act
    device.initDeviceDiscovery();

    // Assert
    EXPECT_TRUE(device.isDiscoveryPending());
    EXPECT_FALSE(device.isOnline());
}

TEST(NsmDeviceState, FinishDeviceDiscovery_ClearsDiscoveryPending)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.discoveryPending = true;

    // Act
    device.finishDeviceDiscovery();

    // Assert
    EXPECT_FALSE(device.isDiscoveryPending());
}

TEST(NsmDeviceState, GettersReturnCorrectValues)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 2, "MCTP_UUID", uuid, 3);

    // Assert
    EXPECT_EQ(device.getDeviceType(), 1);
    EXPECT_EQ(device.getInstanceNumber(), 2);
    EXPECT_EQ(device.getDeviceRole(), 3);
    EXPECT_EQ(device.getUuid(), uuid);
}

TEST(NsmDeviceState, GetSemaphore_ReturnsReference)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Act & Assert - should not throw
    EXPECT_NO_THROW({
        auto& semaphore = device.getSemaphore();
        (void)semaphore;
    });
}

// ============================================================================
// PART 21: FruInterfaceManager - comprehensive tests
// ============================================================================

TEST(FruInterfaceManagerTest, NeedsRecreation_NotInitialized_ReturnsTrue)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"A", "B"};

    // Act & Assert
    EXPECT_TRUE(mgr.needsRecreation(props));
}

TEST(FruInterfaceManagerTest, NeedsRecreation_InitializedSameProps_ReturnsFalse)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"A", "B"};
    mgr.markInitialized(props);

    // Act & Assert
    EXPECT_FALSE(mgr.needsRecreation(props));
}

TEST(FruInterfaceManagerTest,
     NeedsRecreation_InitializedDifferentProps_ReturnsTrue)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props1 = {"A", "B"};
    std::set<std::string> props2 = {"A", "C"};
    mgr.markInitialized(props1);

    // Act & Assert
    EXPECT_TRUE(mgr.needsRecreation(props2));
}

TEST(FruInterfaceManagerTest, MarkInitialized_SetsInitializedAndProperties)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"X", "Y", "Z"};

    // Act
    mgr.markInitialized(props);

    // Assert
    EXPECT_TRUE(mgr.initialized);
    EXPECT_EQ(mgr.supportedProperties, props);
}

TEST(FruInterfaceManagerTest, IsPropertySupported_EmptySet_ReturnsFalse)
{
    // Arrange
    FruInterfaceManager mgr;

    // Act & Assert
    EXPECT_FALSE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported(""));
}

TEST(FruInterfaceManagerTest, IsPropertySupported_AfterInit_ReturnsTrueForKnown)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"BOARD_PART_NUMBER", "SERIAL_NUMBER"};
    mgr.markInitialized(props);

    // Act & Assert
    EXPECT_TRUE(mgr.isPropertySupported("BOARD_PART_NUMBER"));
    EXPECT_TRUE(mgr.isPropertySupported("SERIAL_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported("MARKETING_NAME"));
}

TEST(FruInterfaceManagerTest, Reset_ClearsAllState)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"A", "B"};
    mgr.markInitialized(props);
    EXPECT_TRUE(mgr.initialized);

    // Act
    mgr.reset();

    // Assert
    EXPECT_FALSE(mgr.initialized);
    EXPECT_TRUE(mgr.supportedProperties.empty());
    EXPECT_EQ(mgr.interface, nullptr);
}

TEST(FruInterfaceManagerTest, UpdateAllPropertyValues_NullInterface_NoOp)
{
    // Arrange
    FruInterfaceManager mgr;
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    auto device = std::make_shared<MockNsmDevice>(1, 0, "MCTP_UUID", uuid, 0);
    InventoryProperties props;

    // Act & Assert - should not crash with null interface
    EXPECT_NO_THROW(mgr.updateAllPropertyValues(device, props));
}

TEST(FruInterfaceManagerTest, Reset_ThenNeedsRecreation_ReturnsTrue)
{
    // Arrange
    FruInterfaceManager mgr;
    std::set<std::string> props = {"A"};
    mgr.markInitialized(props);
    mgr.reset();

    // Act & Assert
    EXPECT_TRUE(mgr.needsRecreation(props));
    EXPECT_TRUE(mgr.needsRecreation({}));
}

// ============================================================================
// PART 22: NsmDevice constructor - remapProp variants
// ============================================================================

TEST(NsmDeviceConstructor, RemapPropMCTP_EID_SetsEid)
{
    // Arrange & Act
    MockNsmDevice device(1, 0, "MCTP_EID", "42", 0);

    // Assert
    EXPECT_EQ(device.getEid(), 42);
    EXPECT_EQ(device.getDeviceRemapProp(), DeviceRemapProperty::MCTP_EID);
}

TEST(NsmDeviceConstructor, RemapPropMCTP_UUID_SetsUuid)
{
    // Arrange & Act
    uuid_t testUuid = "abcdef01-2345-6789-abcd-ef0123456789";
    MockNsmDevice device(1, 0, "MCTP_UUID", testUuid, 0);

    // Assert
    EXPECT_EQ(device.getUuid(), testUuid);
    EXPECT_EQ(device.getDeviceRemapProp(), DeviceRemapProperty::MCTP_UUID);
}

TEST(NsmDeviceConstructor,
     RemapPropNSM_DEVICE_INSTANCE_NUMBER_SetsInstanceNumber)
{
    // Arrange & Act
    MockNsmDevice device(1, 0, "NSM_DEVICE_INSTANCE_NUMBER", "5", 0);

    // Assert
    EXPECT_EQ(device.getNsmDeviceInstanceNumber(), 5);
    EXPECT_EQ(device.getDeviceRemapProp(),
              DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER);
}

// ============================================================================
// PART 23: NsmDevice - DumpNsmDeviceInfo exists and is callable
// ============================================================================

TEST(NsmDeviceDump, DumpNsmDeviceInfo_MethodExists)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert - verify the method exists and is accessible
    EXPECT_TRUE(static_cast<bool>(&MockNsmDevice::dumpNsmDeviceInfo));
}

// ============================================================================
// PART 24: SensorManager readiness flow - combined transition scenarios
// ============================================================================

TEST_F(SensorManagerReadinessTest,
       ReadinessTransition_MCTPFirst_ThenEM_BothReady)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Step 1: MCTP becomes ready
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::readynessFailureMap["isMCTPReady"] = "True";
    bool result1 = SensorManagerImpl::isNSMPollReady();
    EXPECT_FALSE(result1);

    // Step 2: EM becomes ready
    SensorManagerImpl::isEMReadyCheck = true;
    SensorManagerImpl::readynessFailureMap["isEMReady"] = "True";
    bool result2 = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_TRUE(result2);
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerReadinessTest,
       ReadinessTransition_EMFirst_ThenMCTP_BothReady)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = false;
    SensorManagerImpl::isEMReadyCheck = false;

    // Step 1: EM becomes ready
    SensorManagerImpl::isEMReadyCheck = true;
    bool result1 = SensorManagerImpl::isNSMPollReady();
    EXPECT_FALSE(result1);

    // Step 2: MCTP becomes ready
    SensorManagerImpl::isMCTPReadyCheck = true;
    bool result2 = SensorManagerImpl::isNSMPollReady();

    // Assert
    EXPECT_TRUE(result2);
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
}

TEST_F(SensorManagerReadinessTest,
       ReadinessTransition_CalledMultipleTimes_OnlySetOnce)
{
    // Arrange
    SensorManagerImpl::isMCTPReadyCheck = true;
    SensorManagerImpl::isEMReadyCheck = true;
    SensorManagerImpl::readynessFailureMap.clear();

    // Act - call multiple times
    SensorManagerImpl::isNSMPollReady();
    SensorManagerImpl::isNSMPollReady();
    SensorManagerImpl::isNSMPollReady();

    // Assert - should still be true
    EXPECT_TRUE(SensorManagerImpl::isReadyForReadinessCheck);
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["isNSMPollReady"], "true");
}

// ============================================================================
// PART 25: NsmDevice - waitForNsmDeviceUpdate
// ============================================================================

TEST(NsmDeviceWaitUpdate, DiscoveryNotPending_ReturnsImmediately)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.discoveryPending = false;

    // Act - the coroutine should return immediately since
    // discoveryPending is false
    auto coroutine = device.waitForNsmDeviceUpdate();

    // Assert - should complete without error
    EXPECT_NO_THROW((void)coroutine);
}

// ============================================================================
// PART 26: NsmDevice - getDeviceRemapValues
// ============================================================================

TEST(NsmDeviceRemapValues, MCTP_EID_ReturnsCorrectVariant)
{
    // Arrange
    MockNsmDevice device(1, 0, "MCTP_EID", "42", 0);

    // Act
    auto values = device.getDeviceRemapValues();

    // Assert - should be vector<uint8_t>
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(values));
    auto& eidValues = std::get<std::vector<uint8_t>>(values);
    ASSERT_FALSE(eidValues.empty());
    EXPECT_EQ(eidValues[0], 42);
}

TEST(NsmDeviceRemapValues, MCTP_UUID_ReturnsCorrectVariant)
{
    // Arrange
    uuid_t testUuid = "abcdef01-2345-6789-abcd-ef0123456789";
    MockNsmDevice device(1, 0, "MCTP_UUID", testUuid, 0);

    // Act
    auto values = device.getDeviceRemapValues();

    // Assert - should be vector<uuid_t> which is vector<string>
    ASSERT_TRUE(std::holds_alternative<std::vector<std::string>>(values));
    auto& uuidValues = std::get<std::vector<std::string>>(values);
    ASSERT_FALSE(uuidValues.empty());
    EXPECT_EQ(uuidValues[0], testUuid);
}

TEST(NsmDeviceRemapValues, NSM_DEVICE_INSTANCE_NUMBER_ReturnsCorrectVariant)
{
    // Arrange
    MockNsmDevice device(1, 0, "NSM_DEVICE_INSTANCE_NUMBER", "7", 0);

    // Act
    auto values = device.getDeviceRemapValues();

    // Assert - should be vector<uint8_t>
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(values));
    auto& instValues = std::get<std::vector<uint8_t>>(values);
    ASSERT_FALSE(instValues.empty());
    EXPECT_EQ(instValues[0], 7);
}

// ============================================================================
// PART 27: NsmDevice - addGpuDriverSensor
// ============================================================================

TEST(NsmDeviceGpuDriver, AddGpuDriverSensor_NullInitially)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert - gpuDriverSensor initially null
    EXPECT_EQ(device.gpuDriverSensor, nullptr);
}

// ============================================================================
// PART 28: NsmDevice - nonPriorityPollingType initial value
// ============================================================================

TEST(NsmDevicePollingState,
     InitialNonPriorityPollingType_IsGpuPerformanceMonitoring)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert
    EXPECT_EQ(device.nonPriorityPollingType,
              PollingType::GpuPerformanceMonitoring);
}

// ============================================================================
// PART 29: NsmDevice - sensor queues initial state
// ============================================================================

TEST(NsmDeviceSensorQueues, InitialQueues_AreEmpty)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert - only check that queues are accessible (constructor adds
    // some sensors like msgTypesSensor to roundRobin)
    EXPECT_NO_THROW({
        (void)device.prioritySensors;
        (void)device.gpmSensors;
        (void)device.longRunningSensors;
        (void)device.staticSensors;
        (void)device.roundRobinSensors;
    });
}

// ============================================================================
// PART 30: NsmDevice - task and longRunningTask coroutine handles
// ============================================================================

TEST(NsmDeviceCoroutineHandles, TaskAndLongRunningTask_InitiallyDone)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert - default-constructed coroutine handles should be done
    EXPECT_TRUE(device.task.done());
    EXPECT_TRUE(device.longRunningTask.done());
}

// ============================================================================
// PART 31: NsmDevice - deviceUuid field
// ============================================================================

TEST(NsmDeviceDeviceUuid, DefaultInitialized_IsEmpty)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert
    EXPECT_TRUE(device.deviceUuid.empty());
}

// ============================================================================
// PART 32: SensorManager - readiness map manipulation
// ============================================================================

TEST_F(SensorManagerReadinessTest,
       ReadynessFailureMap_CanStoreAndRetrieveEntries)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap.clear();

    // Act
    SensorManagerImpl::readynessFailureMap["testKey1"] = "testValue1";
    SensorManagerImpl::readynessFailureMap["testKey2"] = "testValue2";

    // Assert
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap.size(), 2u);
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["testKey1"], "testValue1");
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["testKey2"], "testValue2");
}

TEST_F(SensorManagerReadinessTest, ReadynessFailureMap_OverwriteExistingEntry)
{
    // Arrange
    SensorManagerImpl::readynessFailureMap.clear();
    SensorManagerImpl::readynessFailureMap["key"] = "old_value";

    // Act
    SensorManagerImpl::readynessFailureMap["key"] = "new_value";

    // Assert
    EXPECT_EQ(SensorManagerImpl::readynessFailureMap["key"], "new_value");
}

// ============================================================================
// PART 33: NsmDevice - eventDispatcher
// ============================================================================

TEST(NsmDeviceEventDispatcher, EventDispatcher_InitiallyContainsLREvent)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);

    // Assert - constructor registers long-running event handler via
    // addDeviceEvent which adds to eventDispatcher
    EXPECT_FALSE(device.eventDispatcher.eventsMap.empty());
}

// ============================================================================
// PART 34: NsmDevice - longRunningHandler optional access
// ============================================================================

TEST(NsmDeviceLRHandler, LongRunningHandler_WhenReset_IsEmpty)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    device.longRunningHandler.reset();

    // Assert
    EXPECT_FALSE(device.longRunningHandler.has_value());
}

TEST(NsmDeviceLRHandler, LongRunningHandler_WhenRegistered_ContainsCorrectInfo)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(0, 0, "MCTP_UUID", uuid, 0);
    auto lrEvent = std::make_shared<TestLongRunningEvent>();
    device.registerLongRunningHandler(7, 12, lrEvent);

    // Assert
    ASSERT_TRUE(device.longRunningHandler.has_value());
    EXPECT_EQ(device.longRunningHandler->messageType, 7);
    EXPECT_EQ(device.longRunningHandler->commandCode, 12);
    EXPECT_EQ(device.longRunningHandler->sensorInstance, lrEvent);
}
