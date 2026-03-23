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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "commonMock.hpp"
#include "nsmDevice.hpp"

#undef private
#undef protected

TEST(nsmDevice, GoodTest)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";

    MockNsmDeviceBase nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    EXPECT_EQ(nsmDevice.getDeviceType(), 1);
    EXPECT_EQ(nsmDevice.getInstanceNumber(), 1);
    EXPECT_EQ(nsmDevice.getDeviceRole(), 1);
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_UUID);
    EXPECT_EQ(nsmDevice.getUuid(), uuid);
    EXPECT_EQ(nsmDevice.isDeviceActive, true);
    EXPECT_EQ(nsmDevice.isOnline(), true);
}

TEST(nsmDevice, TestMctpEid)
{
    MockNsmDeviceBase nsmDeviceBase(10, 5, "MCTP_EID", "8", 2);

    EXPECT_EQ(nsmDeviceBase.getDeviceType(), 10);
    EXPECT_EQ(nsmDeviceBase.getInstanceNumber(), 5);
    EXPECT_EQ(nsmDeviceBase.getDeviceRole(), 2);
    EXPECT_EQ(nsmDeviceBase.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_EID);
    EXPECT_EQ(nsmDeviceBase.getEid(), 8);
}

TEST(nsmDevice, TestNsmDeviceInstanceNumber)
{
    MockNsmDeviceBase nsmDeviceBase(10, 5, "NSM_DEVICE_INSTANCE_NUMBER", "42",
                                    2);

    EXPECT_EQ(nsmDeviceBase.getDeviceType(), 10);
    EXPECT_EQ(nsmDeviceBase.getInstanceNumber(), 5);
    EXPECT_EQ(nsmDeviceBase.getDeviceRole(), 2);
    EXPECT_EQ(nsmDeviceBase.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER);
    EXPECT_EQ(nsmDeviceBase.getNsmDeviceInstanceNumber(), 42);
}

// Test that isCommandSupported returns false for invalid message types
// This prevents core dump when device reports unsupported message types like
// 0xFF
TEST(nsmDevice, IsCommandSupportedRejectsInvalidMessageTypes)
{
    MockNsmDeviceBase nsmDevice(1, 1, "MCTP_EID", "10", 0);

    // Valid message types (0 to NUM_NSM_TYPES-1) should be queryable
    // (returns false because no commands are set, but doesn't crash)
    for (uint8_t msgType = 0; msgType < NUM_NSM_TYPES; msgType++)
    {
        // Should not crash and return false (no commands registered)
        EXPECT_FALSE(nsmDevice.isCommandSupported(msgType, 0));
    }

    // Invalid message types >= NUM_NSM_TYPES should return false without
    // accessing out-of-bounds array indices
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES + 1, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(100, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0xFF, 0)); // Message type 255
}

// Test that updateMessageTypesToCommandCodeMatrix ignores invalid message types
// This prevents core dump when device reports unsupported message types like
// 0xFF
TEST(nsmDevice, UpdateCommandCodeMatrixIgnoresInvalidMessageTypes)
{
    MockNsmDeviceBase nsmDevice(1, 1, "MCTP_EID", "10", 0);

    // Create a bitmask with command code 0 supported
    bitfield8_t supportedCommands[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    supportedCommands[0].byte = 0x01; // Command code 0 is supported

    // Valid message type should update the matrix
    nsmDevice.updateMessageTypesToCommandCodeMatrix(
        0, supportedCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));

    // Invalid message types should be silently ignored (no crash, no update)
    // These should not cause array out-of-bounds access
    nsmDevice.updateMessageTypesToCommandCodeMatrix(
        NUM_NSM_TYPES, supportedCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);
    nsmDevice.updateMessageTypesToCommandCodeMatrix(
        0xFF, supportedCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);

    // Verify no crash occurred and invalid types return false
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0xFF, 0));
}

// Test boundary conditions for message type validation
TEST(nsmDevice, MessageTypeBoundaryValidation)
{
    MockNsmDeviceBase nsmDevice(1, 1, "MCTP_EID", "10", 0);

<<<<<<< HEAD
    bitfield8_t supportedCommands[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    supportedCommands[0].byte = 0xFF; // Commands 0-7 supported

    // Test the boundary: NUM_NSM_TYPES - 1 should be valid
    uint8_t lastValidType = NUM_NSM_TYPES - 1;
    nsmDevice.updateMessageTypesToCommandCodeMatrix(
        lastValidType, supportedCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);
    EXPECT_TRUE(nsmDevice.isCommandSupported(lastValidType, 0));

    // Test the boundary: NUM_NSM_TYPES should be invalid
    nsmDevice.updateMessageTypesToCommandCodeMatrix(
        NUM_NSM_TYPES, supportedCommands, SUPPORTED_COMMAND_CODE_DATA_SIZE);
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES, 0));
=======
    // After construction, longRunningHandler might or might not be set
    // (constructor calls registerLongRunningEventHandler which adds an event,
    // not a handler)
    // But the registerLongRunningHandler method is not called in constructor,
    // so it should be empty
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

// ---- FruInterfaceManager: multiple mark/reset cycles ----

TEST(FruInterfaceManager, TestMultipleMarkResetCycles)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> props1 = {"DEVICE_TYPE"};
    std::set<std::string> props2 = {"DEVICE_TYPE", "SERIAL_NUMBER"};

    // Cycle 1
    mgr.markInitialized(props1);
    EXPECT_TRUE(mgr.initialized);
    EXPECT_TRUE(mgr.isPropertySupported("DEVICE_TYPE"));
    EXPECT_FALSE(mgr.isPropertySupported("SERIAL_NUMBER"));

    mgr.reset();
    EXPECT_FALSE(mgr.initialized);

    // Cycle 2
    mgr.markInitialized(props2);
    EXPECT_TRUE(mgr.initialized);
    EXPECT_TRUE(mgr.isPropertySupported("DEVICE_TYPE"));
    EXPECT_TRUE(mgr.isPropertySupported("SERIAL_NUMBER"));

    mgr.reset();
    EXPECT_FALSE(mgr.initialized);
    EXPECT_FALSE(mgr.isPropertySupported("DEVICE_TYPE"));
}

// ===========================================================================
// Additional test cases for uncovered functions in nsmDevice.cpp
// ===========================================================================

// ---- Mock NsmNumericAggregator for findAggregatorByType tests ----

class MockNsmNumericAggregator : public nsm::NsmNumericAggregator
{
  public:
    MockNsmNumericAggregator(const std::string& name, const std::string& type,
                             bool priority) :
        NsmNumericAggregator(name, type, priority)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::vector<uint8_t>{};
    }

  private:
    int handleSample(
        const nsm::NsmSensorAggregator::TelemetrySample& /*sample*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// ---- Mock NsmLongRunningEvent for registerLongRunningHandler tests ----

class MockNsmLongRunningEvent : public nsm::NsmLongRunningEvent
{
  public:
    MockNsmLongRunningEvent(const std::string& name, const std::string& type) :
        NsmLongRunningEvent(name, type, true)
    {}

    int handle(eid_t /*eid*/, NsmType /*type*/, NsmEventId /*eventId*/,
               const nsm_msg* /*event*/, size_t /*eventLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// ---- findAggregatorByType: found case ----

TEST(nsmDevice, FindAggregatorByType_AggregatorExists_ReturnsMatchingAggregator)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto aggregator1 = std::make_shared<MockNsmNumericAggregator>(
        "Aggregator1", "TypeA", false);
    auto aggregator2 = std::make_shared<MockNsmNumericAggregator>(
        "Aggregator2", "TypeB", true);
    auto aggregator3 = std::make_shared<MockNsmNumericAggregator>(
        "Aggregator3", "TypeC", false);

    nsmDevice.sensorAggregators.push_back(aggregator1);
    nsmDevice.sensorAggregators.push_back(aggregator2);
    nsmDevice.sensorAggregators.push_back(aggregator3);

    // Act
    auto result = nsmDevice.findAggregatorByType("TypeB");

    // Assert
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, aggregator2);
    EXPECT_EQ(result->getType(), "TypeB");
}

TEST(nsmDevice, FindAggregatorByType_FirstMatch_ReturnsFirstOfDuplicates)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto aggregator1 = std::make_shared<MockNsmNumericAggregator>(
        "Aggregator1", "SameType", false);
    auto aggregator2 = std::make_shared<MockNsmNumericAggregator>(
        "Aggregator2", "SameType", true);

    nsmDevice.sensorAggregators.push_back(aggregator1);
    nsmDevice.sensorAggregators.push_back(aggregator2);

    // Act
    auto result = nsmDevice.findAggregatorByType("SameType");

    // Assert: should return the first match
    EXPECT_EQ(result, aggregator1);
}

TEST(nsmDevice, FindAggregatorByType_NoMatch_ReturnsNullptr)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto aggregator = std::make_shared<MockNsmNumericAggregator>(
        "Aggregator", "TypeX", false);
    nsmDevice.sensorAggregators.push_back(aggregator);

    // Act
    auto result = nsmDevice.findAggregatorByType("TypeY");

    // Assert
    EXPECT_EQ(result, nullptr);
}

// ---- registerLongRunningHandler: full lifecycle ----

TEST(nsmDevice, RegisterLongRunningHandler_ValidHandler_SetsLongRunningHandler)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());

    auto lrEvent = std::make_shared<MockNsmLongRunningEvent>("LREvt", "LRType");

    // Act
    nsmDevice.registerLongRunningHandler(5, 10, lrEvent);

    // Assert
    ASSERT_TRUE(nsmDevice.longRunningHandler.has_value());
    const auto& [msgType, cmdCode, sensor] = *nsmDevice.longRunningHandler;
    EXPECT_EQ(msgType, 5);
    EXPECT_EQ(cmdCode, 10);
    EXPECT_EQ(sensor, lrEvent);
}

TEST(nsmDevice, RegisterLongRunningHandler_ReRegister_ClearsPreviousAndSetsNew)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto lrEvent1 = std::make_shared<MockNsmLongRunningEvent>("LREvt1",
                                                              "LRType1");
    auto lrEvent2 = std::make_shared<MockNsmLongRunningEvent>("LREvt2",
                                                              "LRType2");

    nsmDevice.registerLongRunningHandler(1, 2, lrEvent1);
    ASSERT_TRUE(nsmDevice.longRunningHandler.has_value());

    // Act: re-register with different handler
    nsmDevice.registerLongRunningHandler(3, 4, lrEvent2);

    // Assert: old handler is replaced
    ASSERT_TRUE(nsmDevice.longRunningHandler.has_value());
    const auto& [msgType, cmdCode, sensor] = *nsmDevice.longRunningHandler;
    EXPECT_EQ(msgType, 3);
    EXPECT_EQ(cmdCode, 4);
    EXPECT_EQ(sensor, lrEvent2);
}

TEST(nsmDevice, RegisterLongRunningHandler_ClearAfterRegister_HandlerIsEmpty)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto lrEvent = std::make_shared<MockNsmLongRunningEvent>("LREvt", "LRType");
    nsmDevice.registerLongRunningHandler(1, 2, lrEvent);
    ASSERT_TRUE(nsmDevice.longRunningHandler.has_value());

    // Act
    nsmDevice.clearLongRunningHandler();

    // Assert
    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

// ---- addSensorBase with invalid PollingType ----

TEST(nsmDevice, AddSensorBase_InvalidPollingType_ThrowsRuntimeError)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("BadSensor", "BadType");

    // Act & Assert: cast an invalid int to PollingType
    EXPECT_THROW(
        nsmDevice.addSensorBase(sensor, static_cast<nsm::PollingType>(99)),
        std::runtime_error);
}

// ---- MCTP_ASSOCIATION remap property ----

TEST(nsmDevice, Constructor_MctpAssociationRemap_SetsRemapProperty)
{
    // Arrange & Act
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 1, "MCTP_ASSOCIATION", "/some/path",
                            0);

    // Assert
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_ASSOCIATION);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_GPU);
}

// ---- getDeviceRemapValues ----

TEST(nsmDevice, GetDeviceRemapValues_MctpUuid_ReturnsUuidVector)
{
    // Arrange
    uuid_t uuid = "aabbccdd-1111-2222-3333-444455556666";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, 0);

    // Act
    auto values = nsmDevice.getDeviceRemapValues();

    // Assert: should hold vector<uuid_t> (vector<string>)
    ASSERT_TRUE(std::holds_alternative<std::vector<uuid_t>>(values));
    auto& uuidVec = std::get<std::vector<uuid_t>>(values);
    ASSERT_EQ(uuidVec.size(), 1u);
    EXPECT_EQ(uuidVec[0], "aabbccdd-1111-2222-3333-444455556666");
}

TEST(nsmDevice, GetDeviceRemapValues_MctpEid_ReturnsUint8Vector)
{
    // Arrange
    MockNsmDevice nsmDevice(1, 0, "MCTP_EID", "42", 0);

    // Act
    auto values = nsmDevice.getDeviceRemapValues();

    // Assert: should hold vector<uint8_t>
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(values));
    auto& eidVec = std::get<std::vector<uint8_t>>(values);
    ASSERT_EQ(eidVec.size(), 1u);
    EXPECT_EQ(eidVec[0], 42);
}

TEST(nsmDevice, GetDeviceRemapValues_NsmDeviceInstanceNumber_ReturnsUint8Vector)
{
    // Arrange
    MockNsmDevice nsmDevice(1, 0, "NSM_DEVICE_INSTANCE_NUMBER", "55", 0);

    // Act
    auto values = nsmDevice.getDeviceRemapValues();

    // Assert: should hold vector<uint8_t>
    ASSERT_TRUE(std::holds_alternative<std::vector<uint8_t>>(values));
    auto& instVec = std::get<std::vector<uint8_t>>(values);
    ASSERT_EQ(instVec.size(), 1u);
    EXPECT_EQ(instVec[0], 55);
}

TEST(nsmDevice, GetDeviceRemapValues_MctpAssociation_ReturnsStringVector)
{
    // Arrange
    MockNsmDevice nsmDevice(1, 0, "MCTP_ASSOCIATION", "/path/to/device", 0);

    // Act
    auto values = nsmDevice.getDeviceRemapValues();

    // Assert: MCTP_ASSOCIATION stores vector<uuid_t> which is vector<string>
    ASSERT_TRUE(std::holds_alternative<std::vector<uuid_t>>(values));
    auto& pathVec = std::get<std::vector<uuid_t>>(values);
    ASSERT_EQ(pathVec.size(), 1u);
    EXPECT_EQ(pathVec[0], "/path/to/device");
}

// ---- addSensor with both priority=true and isLongRunning=true ----

TEST(nsmDevice,
     AddSensor_BothPriorityAndLongRunningTrue_LongRunningTakesPrecedence)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("DualSensor", "DualType");

    // Act: when both priority=true and isLongRunning=true, isLongRunning wins
    nsmDevice.addSensor(sensor, true, true);

    // Assert: sensor should be in longRunningSensors, not prioritySensors
    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 0u);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

// ---- addStaticSensor verifies correct queue and refresh limit ----

TEST(nsmDevice, AddStaticSensor_MultipleSensors_AllGoToStaticQueue)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialStaticSize = nsmDevice.staticSensors.size();

    auto sensor1 = std::make_shared<MockSensor>("Static1", "StaticT1");
    auto sensor2 = std::make_shared<MockSensor>("Static2", "StaticT2");
    auto sensor3 = std::make_shared<MockSensor>("Static3", "StaticT3");

    // Act
    nsmDevice.addStaticSensor(sensor1);
    nsmDevice.addStaticSensor(sensor2);
    nsmDevice.addStaticSensor(sensor3);

    // Assert
    EXPECT_EQ(nsmDevice.staticSensors.size(), initialStaticSize + 3);
    EXPECT_EQ(sensor1->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
    EXPECT_EQ(sensor2->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
    EXPECT_EQ(sensor3->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

// ---- addDeviceEvent verifies eventDispatcher integration ----

TEST(nsmDevice, AddDeviceEvent_RegisteredEvent_CanBeHandledByDispatcher)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto event = std::make_shared<MockNsmEvent>("DispEvt", "DispEvtType");

    // Act: register event for type=4, eventId=8
    nsmDevice.addDeviceEvent(event, 4, 8);

    // Assert: dispatching for the same type/eventId should succeed
    int rc = nsmDevice.eventDispatcher.handle(0, 4, 8, nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(nsmDevice, AddDeviceEvent_UnregisteredTypeEventId_DispatcherReturnsError)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto event = std::make_shared<MockNsmEvent>("Evt", "EvtType");
    nsmDevice.addDeviceEvent(event, 4, 8);

    // Act: try to dispatch for a different type/eventId
    int rc = nsmDevice.eventDispatcher.handle(0, 4, 9, nullptr, 0);

    // Assert: should fail because (4, 9) is not registered
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---- initMsgTypesSensor ----

TEST(nsmDevice, InitMsgTypesSensor_CalledInConstructor_SensorNotNull)
{
    // Arrange & Act
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert
    EXPECT_NE(nsmDevice.msgTypesSensor, nullptr);
    EXPECT_EQ(nsmDevice.msgTypesSensor->getName(), "Supported Message Types");
    EXPECT_EQ(nsmDevice.msgTypesSensor->getType(), "NSM_NVIDIA_MESSAGE_TYPE");
}

TEST(nsmDevice, InitMsgTypesSensor_SensorIsInStaticQueue)
{
    // Arrange & Act
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert: msgTypesSensor is added via addSensor(false) -> roundRobinSensors
    EXPECT_GE(nsmDevice.roundRobinSensors.size(), 1u);

    // msgTypesSensor should be in deviceSensors
    bool found = false;
    for (const auto& s : nsmDevice.deviceSensors)
    {
        if (s == nsmDevice.msgTypesSensor)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ---- registerLongRunningEventHandler ----

TEST(nsmDevice, RegisterLongRunningEventHandler_CalledInConstructor_EventExists)
{
    // Arrange & Act
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Assert
    EXPECT_GE(nsmDevice.deviceEvents.size(), 1u);
}

// ---- updateDiscoveryIdentifiers ----

TEST(nsmDevice, UpdateDiscoveryIdentifiers_FirstTimeEmptyUuid_UpdatesAllFields)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.uuid = "";

    eid_t newEid = 42;
    uuid_t newUuid = "11111111-2222-3333-4444-555555555555";
    uint8_t devInstNum = 7;
    std::string assocPath = "/mctp/test/path";
    std::string medium = "xyz.openbmc_project.MCTP.Binding.MCTPoverPCIe";
    std::string binding = "xyz.openbmc_project.MCTP.Binding.PCIe";

    // Act
    bool result = nsmDevice.updateDiscoveryIdentifiers(
        newEid, newUuid, devInstNum, assocPath, medium, binding, 30);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(nsmDevice.getEid(), newEid);
    EXPECT_EQ(nsmDevice.getUuid(), newUuid);
    EXPECT_EQ(nsmDevice.getNsmDeviceInstanceNumber(), devInstNum);
}

TEST(nsmDevice, UpdateDiscoveryIdentifiers_SameEidSameMedium_UpdatesFields)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.uuid = "";
    eid_t eid1 = 10;
    uuid_t uuid1 = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    std::string assocPath = "/path1";
    std::string medium = "xyz.openbmc_project.MCTP.Binding.MCTPoverPCIe";
    std::string binding = "xyz.openbmc_project.MCTP.Binding.PCIe";

    nsmDevice.updateDiscoveryIdentifiers(eid1, uuid1, 1, assocPath, medium,
                                         binding, 30);

    // Act: update with same eid
    uuid_t uuid2 = "ffffffff-1111-2222-3333-444444444444";
    bool result = nsmDevice.updateDiscoveryIdentifiers(
        eid1, uuid2, 2, assocPath, medium, binding, 30);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(nsmDevice.getEid(), eid1);
    EXPECT_EQ(nsmDevice.getUuid(), uuid2);
}

TEST(nsmDevice, UpdateDiscoveryIdentifiers_DifferentEid_UpdatesEidAndUuid)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    nsmDevice.uuid = "";
    eid_t eid1 = 10;
    uuid_t uuid1 = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    std::string assocPath = "/path1";
    std::string medium = "xyz.openbmc_project.MCTP.Binding.MCTPoverPCIe";
    std::string binding = "xyz.openbmc_project.MCTP.Binding.PCIe";

    nsmDevice.updateDiscoveryIdentifiers(eid1, uuid1, 1, assocPath, medium,
                                         binding, 30);

    // Act
    eid_t eid2 = 20;
    uuid_t uuid2 = "ffffffff-1111-2222-3333-444444444444";
    bool result = nsmDevice.updateDiscoveryIdentifiers(
        eid2, uuid2, 2, assocPath, medium, binding, 30);

    // Assert
    EXPECT_TRUE(result);
    EXPECT_EQ(nsmDevice.getEid(), eid2);
    EXPECT_EQ(nsmDevice.getUuid(), uuid2);
}

// ---- setEventMode: all valid modes ----

TEST(nsmDevice, SetEventMode_AllValidModes_VerifyEachTransition)
{
    // Arrange
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_DISABLE);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_DISABLE);

    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);

    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);

    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_DISABLE);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_DISABLE);
}

TEST(nsmDevice, SetEventMode_DefaultIsDisable_VerifyInitialState)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_DISABLE);
}

// ---- addSensor with PollingType enum directly ----

TEST(nsmDevice, AddSensorWithPollingType_GpuPerformanceMonitoring_GpmQueue)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("GpmSensor", "GpmType");
    nsmDevice.addSensor(sensor, nsm::PollingType::GpuPerformanceMonitoring);

    EXPECT_EQ(nsmDevice.gpmSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
}

// ---- deviceSensors accumulation across different add methods ----

TEST(nsmDevice, DeviceSensors_AccrossMultipleAddMethods_AllAppear)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialCount = nsmDevice.deviceSensors.size();

    auto priSensor = std::make_shared<MockSensor>("Pri", "PriType");
    auto staticSensor = std::make_shared<MockSensor>("Static", "StaticType");
    auto rrSensor = std::make_shared<MockSensor>("RR", "RRType");
    auto devSensor = std::make_shared<MockSensor>("Dev", "DevType");

    nsmDevice.addSensor(priSensor, true, false);
    nsmDevice.addStaticSensor(staticSensor);
    nsmDevice.addSensorBase(rrSensor, nsm::PollingType::RoundRobin);
    nsmDevice.addDeviceSensors(devSensor);

    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialCount + 4);
}

// ---- addCapabilityRefreshSensor: multiple sensors ----

TEST(nsmDevice, AddCapabilityRefreshSensor_MultipleSensors_AllStored)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockSensor>("Cap1", "CapType1");
    auto sensor2 = std::make_shared<MockSensor>("Cap2", "CapType2");

    nsmDevice.addCapabilityRefreshSensor(sensor1);
    nsmDevice.addCapabilityRefreshSensor(sensor2);

    EXPECT_EQ(nsmDevice.capabilityRefreshSensors.size(), 2u);
    EXPECT_EQ(nsmDevice.capabilityRefreshSensors[0], sensor1);
    EXPECT_EQ(nsmDevice.capabilityRefreshSensors[1], sensor2);
}

// ---- addSetSensor: multiple sensors ----

TEST(nsmDevice, AddSetSensor_MultipleSensors_AllStored)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockSensor>("Set1", "SetType1");
    auto sensor2 = std::make_shared<MockSensor>("Set2", "SetType2");

    nsmDevice.addSetSensor(sensor1);
    nsmDevice.addSetSensor(sensor2);

    EXPECT_EQ(nsmDevice.setSensors.size(), 2u);
    EXPECT_EQ(nsmDevice.setSensors[0], sensor1);
    EXPECT_EQ(nsmDevice.setSensors[1], sensor2);
}

// ---- addStandByToDcRefreshSensor: multiple sensors ----

TEST(nsmDevice, AddStandByToDcRefreshSensor_MultipleSensors_AllStored)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockSensor>("Stby1", "StbyType1");
    auto sensor2 = std::make_shared<MockSensor>("Stby2", "StbyType2");
    auto sensor3 = std::make_shared<MockSensor>("Stby3", "StbyType3");

    nsmDevice.addStandByToDcRefreshSensor(sensor1);
    nsmDevice.addStandByToDcRefreshSensor(sensor2);
    nsmDevice.addStandByToDcRefreshSensor(sensor3);

    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors.size(), 3u);
    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors[0], sensor1);
    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors[1], sensor2);
    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors[2], sensor3);
}

// ---- allCommandCodesAreRetrieved edge cases ----

TEST(nsmDevice,
     AllCommandCodesAreRetrieved_MessageTypesNotRetrieved_ReturnsFalse)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = false;
    nsmDevice.commandCodesRetrieved[0] = true;
    nsmDevice.commandCodesRetrieved[1] = true;
    nsmDevice.retrievedMessageTypes = {0, 1};

    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());
}

TEST(nsmDevice,
     AllCommandCodesAreRetrieved_MixedStates_ReturnsFalseUntilAllTrue)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.commandCodesRetrieved.clear();
    nsmDevice.retrievedMessageTypes = {0, 1, 2, 3};

    nsmDevice.commandCodesRetrieved[0] = true;
    nsmDevice.commandCodesRetrieved[1] = true;
    nsmDevice.commandCodesRetrieved[2] = true;
    nsmDevice.commandCodesRetrieved[3] = false;

    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());

    nsmDevice.commandCodesRetrieved[3] = true;
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

// ---- isCommandSupported boundary ----

TEST(nsmDevice, IsCommandSupported_CommandCode0SetAndMaxSet_BothReturnTrue)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    bitfield8_t supportedCommands[32] = {};
    supportedCommands[0].byte = 0x01;
    supportedCommands[31].byte = 0x80;

    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 32);

    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 255));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 1));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 254));
}

// ---- updateMessageTypesToCommandCodeMatrix: single byte ----

TEST(nsmDevice,
     UpdateCommandCodeMatrix_SingleByteAllBitsSet_Commands0To7Supported)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    bitfield8_t supportedCommands[1] = {};
    supportedCommands[0].byte = 0xFF;

    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 1);

    for (uint8_t cc = 0; cc < 8; cc++)
    {
        EXPECT_TRUE(nsmDevice.isCommandSupported(0, cc))
            << "Command " << (int)cc << " should be supported";
    }
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 8));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 255));
}

// ---- Discovery lifecycle ----

TEST(nsmDevice, DiscoveryLifecycle_MultipleInitFinishCycles_StateCorrect)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
    EXPECT_TRUE(nsmDevice.isOnline());

    nsmDevice.initDeviceDiscovery();
    EXPECT_TRUE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isOnline());

    nsmDevice.finishDeviceDiscovery();
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isOnline());

    nsmDevice.isDeviceActive = true;
    EXPECT_TRUE(nsmDevice.isOnline());

    nsmDevice.initDeviceDiscovery();
    EXPECT_TRUE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isOnline());

    nsmDevice.finishDeviceDiscovery();
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
}

// ---- Combined ready/online ----

TEST(nsmDevice, ReadyAndOnline_IndependentFlags_CombinedBehavior)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_TRUE(nsmDevice.isOnline());
    EXPECT_TRUE(nsmDevice.isReady());

    nsmDevice.markDeviceAsNotReady();
    EXPECT_TRUE(nsmDevice.isOnline());
    EXPECT_FALSE(nsmDevice.isReady());

    nsmDevice.isDeviceActive = false;
    EXPECT_FALSE(nsmDevice.isOnline());
    EXPECT_FALSE(nsmDevice.isReady());

    nsmDevice.markDeviceAsReady();
    EXPECT_FALSE(nsmDevice.isOnline());
    EXPECT_TRUE(nsmDevice.isReady());

    nsmDevice.isDeviceActive = true;
    EXPECT_TRUE(nsmDevice.isOnline());
    EXPECT_TRUE(nsmDevice.isReady());
}

// ---- FruInterfaceManager: updateAllPropertyValues with null interface ----

TEST(FruInterfaceManager, UpdateAllPropertyValues_NullInterface_NoOp)
{
    nsm::FruInterfaceManager mgr;
    EXPECT_EQ(mgr.interface, nullptr);

    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsm::InventoryProperties props;
    mgr.updateAllPropertyValues(
        std::shared_ptr<nsm::NsmDevice>(&nsmDevice, [](nsm::NsmDevice*) {}),
        props);

    EXPECT_EQ(mgr.interface, nullptr);
}

// ---- FruInterfaceManager: needsRecreation property comparisons ----

TEST(FruInterfaceManager, NeedsRecreation_SubsetProperties_ReturnsTrue)
{
    nsm::FruInterfaceManager mgr;
    std::set<std::string> fullProps = {"A", "B", "C"};
    mgr.markInitialized(fullProps);

    std::set<std::string> subset = {"A", "B"};
    EXPECT_TRUE(mgr.needsRecreation(subset));
}

TEST(FruInterfaceManager, NeedsRecreation_SupersetProperties_ReturnsTrue)
{
    nsm::FruInterfaceManager mgr;
    std::set<std::string> initialProps = {"A", "B"};
    mgr.markInitialized(initialProps);

    std::set<std::string> superset = {"A", "B", "C"};
    EXPECT_TRUE(mgr.needsRecreation(superset));
}

TEST(FruInterfaceManager, NeedsRecreation_IdenticalProperties_ReturnsFalse)
{
    nsm::FruInterfaceManager mgr;
    std::set<std::string> props = {"X", "Y", "Z"};
    mgr.markInitialized(props);
    EXPECT_FALSE(mgr.needsRecreation(props));
}

// ---- sensorAggregators initial state ----

TEST(nsmDevice, SensorAggregators_InitiallyEmpty_ReturnsEmptyVector)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
    EXPECT_TRUE(nsmDevice.sensorAggregators.empty());
}

// ---- findAggregatorByType after clear ----

TEST(nsmDevice, FindAggregatorByType_AfterClear_ReturnsNullptr)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto aggregator =
        std::make_shared<MockNsmNumericAggregator>("Agg", "AggType", false);
    nsmDevice.sensorAggregators.push_back(aggregator);
    EXPECT_NE(nsmDevice.findAggregatorByType("AggType"), nullptr);

    nsmDevice.sensorAggregators.clear();
    EXPECT_EQ(nsmDevice.findAggregatorByType("AggType"), nullptr);
}

// ---- EventDispatcher duplicate event ----

TEST(nsmDevice, EventDispatcher_AddDuplicateTypeEventId_ReturnsError)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto event1 = std::make_shared<MockNsmEvent>("E1", "ET1");
    auto event2 = std::make_shared<MockNsmEvent>("E2", "ET2");

    int rc1 = nsmDevice.eventDispatcher.addEvent(10, 20, event1);
    EXPECT_EQ(rc1, NSM_SW_SUCCESS);

    int rc2 = nsmDevice.eventDispatcher.addEvent(10, 20, event2);
    EXPECT_NE(rc2, NSM_SW_SUCCESS);
}

// ---- addSensorBase: device identifier ----

TEST(nsmDevice, AddSensorBase_MultipleSensors_EachGetsDeviceIdentifier)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 5, "MCTP_UUID", uuid, 0);

    auto sensor1 = std::make_shared<MockSensor>("S1", "T1");
    auto sensor2 = std::make_shared<MockSensor>("S2", "T2");

    nsmDevice.addSensorBase(sensor1, nsm::PollingType::RoundRobin);
    nsmDevice.addSensorBase(sensor2, nsm::PollingType::Priority);

    EXPECT_FALSE(sensor1->getDeviceIdentifier().empty());
    EXPECT_FALSE(sensor2->getDeviceIdentifier().empty());
    EXPECT_EQ(sensor1->getDeviceIdentifier(), sensor2->getDeviceIdentifier());
>>>>>>> a3b71947 (feat(nsmd): per-device Local EID from MCTP)
}
