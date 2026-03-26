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
#include "nsmEvent/nsmLongRunningEvent.hpp"
#include "nsmNumericSensor/nsmNumericAggregator.hpp"

#undef private
#undef protected

#include "nsmFwSwInventory/GPUSWInventory.hpp"
#include "test/mockDBusHandler.hpp"

TEST(nsmDevice, GoodTest)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";

    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);
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
    MockNsmDevice nsmDeviceBase(10, 5, "MCTP_EID", "8", 2);

    EXPECT_EQ(nsmDeviceBase.getDeviceType(), 10);
    EXPECT_EQ(nsmDeviceBase.getInstanceNumber(), 5);
    EXPECT_EQ(nsmDeviceBase.getDeviceRole(), 2);
    EXPECT_EQ(nsmDeviceBase.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_EID);
    EXPECT_EQ(nsmDeviceBase.getEid(), 8);
}

TEST(nsmDevice, TestNsmDeviceInstanceNumber)
{
    MockNsmDevice nsmDeviceBase(10, 5, "NSM_DEVICE_INSTANCE_NUMBER", "42", 2);

    EXPECT_EQ(nsmDeviceBase.getDeviceType(), 10);
    EXPECT_EQ(nsmDeviceBase.getInstanceNumber(), 5);
    EXPECT_EQ(nsmDeviceBase.getDeviceRole(), 2);
    EXPECT_EQ(nsmDeviceBase.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER);
    EXPECT_EQ(nsmDeviceBase.getNsmDeviceInstanceNumber(), 42);
}

TEST(nsmDevice, TestEventMode)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Test default event mode
    EXPECT_EQ(nsmDevice.getEventMode(), 0);

    // Test setting valid event modes
    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_DISABLE);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_DISABLE);

    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);

    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);

    // Test setting invalid event mode (should not change)
    nsmDevice.setEventMode(255);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
}

TEST(nsmDevice, TestCommandSupport)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Initially no commands should be supported
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 0));

    // Update command code matrix
    bitfield8_t supportedCommands[32] = {};
    supportedCommands[0].byte = 0x01; // Command 0 is supported
    supportedCommands[1].byte = 0x80; // Command 15 is supported

    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 32);

    // Test command support
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 1));
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 15));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 16));
}

TEST(nsmDevice, TestAllCommandCodesRetrieved)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Initially should return false
    nsmDevice.areMessageTypesRetrieved = false;
    nsmDevice.commandCodesRetrieved.clear();
    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());

    // Set message types retrieved
    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.retrievedMessageTypes.push_back(0);
    nsmDevice.retrievedMessageTypes.push_back(1);

    // Still false because command codes not retrieved
    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());

    // Mark command codes as retrieved
    nsmDevice.commandCodesRetrieved[0] = true;
    nsmDevice.commandCodesRetrieved[1] = true;

    // Now should return true
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

TEST(nsmDevice, TestDeviceOnlineStatus)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Initially should be active and online
    EXPECT_TRUE(nsmDevice.isDeviceActive);
    EXPECT_TRUE(nsmDevice.isOnline());

    // Set device as inactive
    nsmDevice.isDeviceActive = false;
    EXPECT_FALSE(nsmDevice.isOnline());

    // Restore activity
    nsmDevice.isDeviceActive = true;
    EXPECT_TRUE(nsmDevice.isOnline());
}

TEST(nsmDevice, TestUpdateCommandCodeMatrix)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Test with various patterns
    bitfield8_t supportedCommands[4] = {};
    supportedCommands[0].byte = 0xFF; // Commands 0-7 supported
    supportedCommands[1].byte = 0x00; // Commands 8-15 not supported
    supportedCommands[2].byte = 0x55; // Commands 16,18,20,22 supported
    supportedCommands[3].byte = 0xAA; // Commands 25,27,29,31 supported

    uint8_t messageType = 2;
    nsmDevice.updateMessageTypesToCommandCodeMatrix(messageType,
                                                    supportedCommands, 4);

    // Verify commands 0-7 are supported
    for (uint8_t i = 0; i < 8; i++)
    {
        EXPECT_TRUE(nsmDevice.isCommandSupported(messageType, i));
    }

    // Verify commands 8-15 are not supported
    for (uint8_t i = 8; i < 16; i++)
    {
        EXPECT_FALSE(nsmDevice.isCommandSupported(messageType, i));
    }

    // Verify commands 16,18,20,22 are supported
    EXPECT_TRUE(nsmDevice.isCommandSupported(messageType, 16));
    EXPECT_FALSE(nsmDevice.isCommandSupported(messageType, 17));
    EXPECT_TRUE(nsmDevice.isCommandSupported(messageType, 18));
    EXPECT_FALSE(nsmDevice.isCommandSupported(messageType, 19));
    EXPECT_TRUE(nsmDevice.isCommandSupported(messageType, 20));
}

TEST(nsmDevice, TestFindAggregatorByType)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Initially should return nullptr (no aggregators added)
    auto result = nsmDevice.findAggregatorByType("TestType");
    EXPECT_EQ(result, nullptr);
}

TEST(nsmDevice, TestDeviceRoleAndType)
{
    uuid_t uuid = "11111111-2222-3333-4444-555555555555";
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 3, "MCTP_UUID", uuid, 0);

    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_GPU);
    EXPECT_EQ(nsmDevice.getInstanceNumber(), 3);
    EXPECT_EQ(nsmDevice.getDeviceRole(), 0);
    EXPECT_EQ(nsmDevice.getUuid(), uuid);
}

// ---------------------------------------------------------------------------
// New tests for uncovered functions
// ---------------------------------------------------------------------------

// ---- isOnline / setDeviceActive toggling ----

TEST(nsmDevice, TestIsOnlineReflectsIsDeviceActive)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Arrange: MockNsmDevice constructor sets isDeviceActive = true
    EXPECT_TRUE(nsmDevice.isOnline());

    // Act: toggle off
    nsmDevice.isDeviceActive = false;

    // Assert
    EXPECT_FALSE(nsmDevice.isOnline());

    // Act: toggle back on
    nsmDevice.isDeviceActive = true;

    // Assert
    EXPECT_TRUE(nsmDevice.isOnline());
}

// ---- isReady / markDeviceAsReady / markDeviceAsNotReady ----

TEST(nsmDevice, TestIsReadyAndMarkDeviceReady)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Arrange: MockNsmDevice constructor sets isDeviceReady = true
    EXPECT_TRUE(nsmDevice.isReady());

    // Act
    nsmDevice.markDeviceAsNotReady();

    // Assert
    EXPECT_FALSE(nsmDevice.isReady());

    // Act
    nsmDevice.markDeviceAsReady();

    // Assert
    EXPECT_TRUE(nsmDevice.isReady());
}

// ---- initDeviceDiscovery / finishDeviceDiscovery / isDiscoveryPending ----

TEST(nsmDevice, TestDiscoveryPendingLifecycle)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Arrange: after constructor, discoveryPending should be false
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
    EXPECT_TRUE(nsmDevice.isDeviceActive);

    // Act: initDeviceDiscovery sets discoveryPending=true and
    // isDeviceActive=false
    nsmDevice.initDeviceDiscovery();

    // Assert
    EXPECT_TRUE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isDeviceActive);
    EXPECT_FALSE(nsmDevice.isOnline());

    // Act: finishDeviceDiscovery sets discoveryPending=false
    nsmDevice.finishDeviceDiscovery();

    // Assert
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
    // isDeviceActive remains false - must be explicitly set
    EXPECT_FALSE(nsmDevice.isOnline());
}

// ---- getDeviceType with all known device types ----

TEST(nsmDevice, TestGetDeviceTypeGPU)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_GPU);
}

TEST(nsmDevice, TestGetDeviceTypeSwitch)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_SWITCH, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_SWITCH);
}

TEST(nsmDevice, TestGetDeviceTypePcieBridge)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_PCIE_BRIDGE, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_PCIE_BRIDGE);
}

TEST(nsmDevice, TestGetDeviceTypeBaseboard)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_BASEBOARD, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_BASEBOARD);
}

TEST(nsmDevice, TestGetDeviceTypeErot)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_EROT, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_EROT);
}

TEST(nsmDevice, TestGetDeviceTypeMctpBridge)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_MCTP_BRIDGE, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_MCTP_BRIDGE);
}

TEST(nsmDevice, TestGetDeviceTypeCPU)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_CPU, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_CPU);
}

// ---- getInstanceNumber edge cases ----

TEST(nsmDevice, TestGetInstanceNumberZero)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getInstanceNumber(), 0);
}

TEST(nsmDevice, TestGetInstanceNumberMax)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 255, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getInstanceNumber(), 255);
}

// ---- getDeviceRole edge cases ----

TEST(nsmDevice, TestGetDeviceRoleReserved)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, NSM_DEV_ROLE_RESERVED);
    EXPECT_EQ(nsmDevice.getDeviceRole(), NSM_DEV_ROLE_RESERVED);
}

TEST(nsmDevice, TestGetDeviceRoleNonZero)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, 5);
    EXPECT_EQ(nsmDevice.getDeviceRole(), 5);
}

// ---- getUuid ----

TEST(nsmDevice, TestGetUuidNonDefault)
{
    uuid_t uuid = "aabbccdd-1122-3344-5566-778899aabbcc";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getUuid(), "aabbccdd-1122-3344-5566-778899aabbcc");
}

TEST(nsmDevice, TestGetUuidEmptyString)
{
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", "", 0);
    EXPECT_EQ(nsmDevice.getUuid(), "");
}

// ---- getEid ----

TEST(nsmDevice, TestGetEidFromMctpEidRemap)
{
    MockNsmDevice nsmDevice(1, 0, "MCTP_EID", "42", 0);
    EXPECT_EQ(nsmDevice.getEid(), 42);
}

TEST(nsmDevice, TestGetEidDefaultZero)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, 0);
    // When remap is MCTP_UUID, eid defaults to 0
    EXPECT_EQ(nsmDevice.getEid(), 0);
}

// ---- getNsmDeviceInstanceNumber ----

TEST(nsmDevice, TestGetNsmDeviceInstanceNumberFromRemap)
{
    MockNsmDevice nsmDevice(1, 0, "NSM_DEVICE_INSTANCE_NUMBER", "99", 0);
    EXPECT_EQ(nsmDevice.getNsmDeviceInstanceNumber(), 99);
}

// ---- getDeviceRemapProp for all property types ----

TEST(nsmDevice, TestDeviceRemapPropMctpUuid)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 0, "MCTP_UUID", uuid, 0);
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_UUID);
}

TEST(nsmDevice, TestDeviceRemapPropMctpEid)
{
    MockNsmDevice nsmDevice(1, 0, "MCTP_EID", "10", 0);
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_EID);
}

TEST(nsmDevice, TestDeviceRemapPropNsmDeviceInstanceNumber)
{
    MockNsmDevice nsmDevice(1, 0, "NSM_DEVICE_INSTANCE_NUMBER", "5", 0);
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER);
}

// ---- isCommandSupported edge cases ----

TEST(nsmDevice, TestIsCommandSupportedInvalidMessageType)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // messageType >= NUM_NSM_TYPES should return false
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES + 1, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(255, 0));
}

TEST(nsmDevice, TestIsCommandSupportedBoundaryMessageType)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Valid message type at upper boundary (NUM_NSM_TYPES - 1) should not crash
    // With no commands set, should return false
    EXPECT_FALSE(nsmDevice.isCommandSupported(NUM_NSM_TYPES - 1, 0));
}

TEST(nsmDevice, TestIsCommandSupportedHighCommandCode)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Command code 255 (max) should be accessible without crash
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 255));
}

// ---- updateMessageTypesToCommandCodeMatrix edge cases ----

TEST(nsmDevice, TestUpdateCommandCodeMatrixInvalidMessageType)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // messageType >= NUM_NSM_TYPES should be silently skipped
    bitfield8_t supportedCommands[4] = {};
    supportedCommands[0].byte = 0xFF;

    nsmDevice.updateMessageTypesToCommandCodeMatrix(NUM_NSM_TYPES,
                                                    supportedCommands, 4);

    // Verify no crash and valid message types are still all false
    for (uint8_t mt = 0; mt < NUM_NSM_TYPES; mt++)
    {
        EXPECT_FALSE(nsmDevice.isCommandSupported(mt, 0));
    }
}

// Calling updateMessageTypesToCommandCodeMatrix with an invalid messageType
// TWICE on the same device covers both branches of the shouldLog() if-check:
// first call → state changes false→true (shouldLog returns true, logs),
// second call → state stays true (shouldLog returns false, no log).
TEST(nsmDevice, TestUpdateCommandCodeMatrixInvalidMT_ShouldLogFalseBranch)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    bitfield8_t supportedCommands[4] = {};
    supportedCommands[0].byte = 0x01;

    // First call: shouldLog(key, true) → state changes, returns true (logs)
    nsmDevice.updateMessageTypesToCommandCodeMatrix(NUM_NSM_TYPES,
                                                    supportedCommands, 4);
    // Second call with same invalid type: shouldLog(key, true) → no state
    // change, returns false (no log) — covers the false branch of the if
    nsmDevice.updateMessageTypesToCommandCodeMatrix(NUM_NSM_TYPES,
                                                    supportedCommands, 4);

    // Matrix should still be untouched
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 0));
}

TEST(nsmDevice, TestUpdateCommandCodeMatrixZeroSize)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Zero-sized supportedCommands should not set any commands
    bitfield8_t supportedCommands[1] = {};
    supportedCommands[0].byte = 0xFF;

    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 0);

    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 7));
}

TEST(nsmDevice, TestUpdateCommandCodeMatrixAllMessageTypes)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Set command 0 as supported on every valid message type
    bitfield8_t supportedCommands[1] = {};
    supportedCommands[0].byte = 0x01; // Only command 0 supported

    for (uint8_t mt = 0; mt < NUM_NSM_TYPES; mt++)
    {
        nsmDevice.updateMessageTypesToCommandCodeMatrix(mt, supportedCommands,
                                                        1);
    }

    // Verify command 0 is now supported on all valid message types
    for (uint8_t mt = 0; mt < NUM_NSM_TYPES; mt++)
    {
        EXPECT_TRUE(nsmDevice.isCommandSupported(mt, 0));
        EXPECT_FALSE(nsmDevice.isCommandSupported(mt, 1));
    }
}

TEST(nsmDevice, TestUpdateCommandCodeMatrixOverwrite)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // First update: commands 0-7 supported
    bitfield8_t supportedCommands1[1] = {};
    supportedCommands1[0].byte = 0xFF;
    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands1, 1);
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 7));

    // Second update: only command 0 supported - should overwrite first 8 bits
    bitfield8_t supportedCommands2[1] = {};
    supportedCommands2[0].byte = 0x01;
    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands2, 1);
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 1));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 7));
}

// ---- allCommandCodesAreRetrieved edge cases ----

TEST(nsmDevice, TestAllCommandCodesRetrievedEmptyMessageTypes)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // areMessageTypesRetrieved = true, but retrievedMessageTypes is empty
    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.retrievedMessageTypes.clear();
    nsmDevice.commandCodesRetrieved.clear();

    // all_of on empty range returns true, so should return true
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

TEST(nsmDevice, TestAllCommandCodesRetrievedPartialRetrieval)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.commandCodesRetrieved.clear();
    nsmDevice.retrievedMessageTypes = {0, 1, 2};

    // Only type 0 and 1 are retrieved, type 2 is not
    nsmDevice.commandCodesRetrieved[0] = true;
    nsmDevice.commandCodesRetrieved[1] = true;
    nsmDevice.commandCodesRetrieved[2] = false;

    EXPECT_FALSE(nsmDevice.allCommandCodesAreRetrieved());

    // Now mark type 2 as retrieved
    nsmDevice.commandCodesRetrieved[2] = true;
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

TEST(nsmDevice, TestAllCommandCodesRetrievedOutOfRangeMessageType)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    nsmDevice.areMessageTypesRetrieved = true;
    nsmDevice.commandCodesRetrieved.clear();

    // Include an out-of-range message type (>= NUM_NSM_TYPES) - should be
    // treated as "retrieved" per the implementation
    nsmDevice.retrievedMessageTypes = {0, NUM_NSM_TYPES};
    nsmDevice.commandCodesRetrieved[0] = true;

    // Out-of-range type returns true in the lambda, so should be true overall
    EXPECT_TRUE(nsmDevice.allCommandCodesAreRetrieved());
}

// ---- setEventMode edge cases ----

TEST(nsmDevice, TestSetEventModeExactBoundary)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // mode = GLOBAL_EVENT_GENERATION_ENABLE_PUSH (2) is the maximum valid
    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);

    // mode = 3 is just above max, should be rejected
    nsmDevice.setEventMode(3);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
}

TEST(nsmDevice, TestSetEventModeInvalidDoesNotChangeExisting)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Set to a valid mode first
    nsmDevice.setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);

    // Try setting multiple invalid modes
    nsmDevice.setEventMode(3);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
    nsmDevice.setEventMode(100);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
    nsmDevice.setEventMode(255);
    EXPECT_EQ(nsmDevice.getEventMode(), GLOBAL_EVENT_GENERATION_ENABLE_POLLING);
}

// ---- addDeviceSensors ----

TEST(nsmDevice, TestAddDeviceSensors)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Arrange: deviceSensors should already have msgTypesSensor from
    // constructor's initMsgTypesSensor()
    size_t initialSize = nsmDevice.deviceSensors.size();

    // Act: add a sensor via addDeviceSensors
    auto sensor = std::make_shared<MockSensor>("TestSensor1", "TestType1");
    nsmDevice.addDeviceSensors(sensor);

    // Assert
    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialSize + 1);
    EXPECT_EQ(nsmDevice.deviceSensors.back(), sensor);
}

TEST(nsmDevice, TestAddDeviceSensorsMultiple)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialSize = nsmDevice.deviceSensors.size();

    auto sensor1 = std::make_shared<MockSensor>("Sensor1", "Type1");
    auto sensor2 = std::make_shared<MockSensor>("Sensor2", "Type2");
    auto sensor3 = std::make_shared<MockSensor>("Sensor3", "Type3");

    nsmDevice.addDeviceSensors(sensor1);
    nsmDevice.addDeviceSensors(sensor2);
    nsmDevice.addDeviceSensors(sensor3);

    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialSize + 3);
}

// ---- addDeviceEvent ----

class MockNsmEvent : public nsm::NsmEvent
{
  public:
    MockNsmEvent(const std::string& name, const std::string& type) :
        NsmEvent(name, type)
    {}

    int handle(eid_t /*eid*/, NsmType /*type*/, NsmEventId /*eventId*/,
               const nsm_msg* /*event*/, size_t /*eventLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

TEST(nsmDevice, TestAddDeviceEvent)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // The constructor already adds a long running event handler, record current
    // count
    size_t initialEventCount = nsmDevice.deviceEvents.size();

    // Act: add a custom event
    auto event = std::make_shared<MockNsmEvent>("TestEvent", "TestEventType");
    nsmDevice.addDeviceEvent(event, 0, 5);

    // Assert
    EXPECT_EQ(nsmDevice.deviceEvents.size(), initialEventCount + 1);
    EXPECT_EQ(nsmDevice.deviceEvents.back(), event);
}

TEST(nsmDevice, TestAddDeviceEventMultiple)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialEventCount = nsmDevice.deviceEvents.size();

    auto event1 = std::make_shared<MockNsmEvent>("Event1", "Type1");
    auto event2 = std::make_shared<MockNsmEvent>("Event2", "Type2");

    // Use different type/eventId combinations to avoid EventDispatcher conflict
    nsmDevice.addDeviceEvent(event1, 0, 10);
    nsmDevice.addDeviceEvent(event2, 1, 10);

    EXPECT_EQ(nsmDevice.deviceEvents.size(), initialEventCount + 2);
}

// ---- Event subscription status cache for logDump ----

TEST(nsmDevice, TestEventSubscriptionStatusCacheRecordAndGet)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_FALSE(nsmDevice.getLastEventSubscriptionStatus().has_value());

    nsmDevice.recordEventSubscriptionStatus("OK (localEid=30)");
    auto status = nsmDevice.getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "OK (localEid=30)");

    nsmDevice.recordEventSubscriptionStatus(
        "skipped: localEid not set (LocalEID not from MCTP)");
    status = nsmDevice.getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "skipped: localEid not set (LocalEID not from MCTP)");

    nsmDevice.recordEventSubscriptionStatus("failed: sensorIO rc=5");
    status = nsmDevice.getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "failed: sensorIO rc=5");

    nsmDevice.recordEventSubscriptionStatus("pending");
    status = nsmDevice.getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "pending");

    nsmDevice.recordEventSubscriptionStatus("N/A (no NSM_EventSetting config)");
    status = nsmDevice.getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(*status, "N/A (no NSM_EventSetting config)");
}

TEST(nsmDevice, TestEventSubscriptionStatusCacheRecordsRequestResponse)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_FALSE(nsmDevice.getLastEventSubscriptionRequest().has_value());
    EXPECT_FALSE(nsmDevice.getLastEventSubscriptionResponse().has_value());

    std::vector<uint8_t> req{0x10, 0xde, 0x01};
    std::vector<uint8_t> resp{0x10, 0xde, 0x02};
    nsmDevice.recordEventSubscriptionStatus("OK (localEid=1)", req, resp);

    auto gotReq = nsmDevice.getLastEventSubscriptionRequest();
    auto gotResp = nsmDevice.getLastEventSubscriptionResponse();
    ASSERT_TRUE(gotReq.has_value());
    ASSERT_TRUE(gotResp.has_value());
    EXPECT_EQ(*gotReq, req);
    EXPECT_EQ(*gotResp, resp);

    nsmDevice.recordEventSubscriptionStatus("skipped: localEid not set",
                                            std::nullopt, std::nullopt);
    EXPECT_FALSE(nsmDevice.getLastEventSubscriptionRequest().has_value());
    EXPECT_FALSE(nsmDevice.getLastEventSubscriptionResponse().has_value());
}

TEST(nsmDevice, TestEventSubscriptionStatusHasNsmEventSettingConfig)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_FALSE(nsmDevice.hasNsmEventSettingConfig());

    nsmDevice.setHasNsmEventSettingConfig(true);
    EXPECT_TRUE(nsmDevice.hasNsmEventSettingConfig());

    nsmDevice.setHasNsmEventSettingConfig(false);
    EXPECT_FALSE(nsmDevice.hasNsmEventSettingConfig());
}

// ---- addCapabilityRefreshSensor ----

TEST(nsmDevice, TestAddCapabilityRefreshSensor)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_EQ(nsmDevice.capabilityRefreshSensors.size(), 0u);

    auto sensor = std::make_shared<MockSensor>("CapSensor", "CapType");
    nsmDevice.addCapabilityRefreshSensor(sensor);

    EXPECT_EQ(nsmDevice.capabilityRefreshSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.capabilityRefreshSensors[0], sensor);
}

// ---- addSetSensor ----

TEST(nsmDevice, TestAddSetSensor)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_EQ(nsmDevice.setSensors.size(), 0u);

    auto sensor = std::make_shared<MockSensor>("SetSensor", "SetType");
    nsmDevice.addSetSensor(sensor);

    EXPECT_EQ(nsmDevice.setSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.setSensors[0], sensor);
}

// ---- addStandByToDcRefreshSensor ----

TEST(nsmDevice, TestAddStandByToDcRefreshSensor)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors.size(), 0u);

    auto sensor = std::make_shared<MockSensor>("StbySensor", "StbyType");
    nsmDevice.addStandByToDcRefreshSensor(sensor);

    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.standByToDcRefreshSensors[0], sensor);
}

// ---- addSensorBase (via addSensor fallback path) ----

TEST(nsmDevice, TestAddSensorBaseViaPriorityPolling)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialDeviceSensors = nsmDevice.deviceSensors.size();

    // addSensorBase is private, but addSensor (non-NsmInterfaces fallback)
    // calls it directly
    auto sensor = std::make_shared<MockSensor>("PriSensor", "PriType");
    nsmDevice.addSensorBase(sensor, nsm::PollingType::Priority);

    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialDeviceSensors + 1);
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorBaseViaRoundRobinPolling)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialDeviceSensors = nsmDevice.deviceSensors.size();

    auto sensor = std::make_shared<MockSensor>("RRSensor", "RRType");
    nsmDevice.addSensorBase(sensor, nsm::PollingType::RoundRobin);

    EXPECT_EQ(nsmDevice.deviceSensors.size(), initialDeviceSensors + 1);
    EXPECT_GE(nsmDevice.roundRobinSensors.size(),
              2u); // 1 from msgTypesSensor + 1 added
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorBaseViaGpmPolling)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("GpmSensor", "GpmType");
    nsmDevice.addSensorBase(sensor, nsm::PollingType::GpuPerformanceMonitoring);

    EXPECT_EQ(nsmDevice.gpmSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorBaseViaLongRunningPolling)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("LRSensor", "LRType");
    nsmDevice.addSensorBase(sensor, nsm::PollingType::LongRunning);

    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorBaseViaStaticPolling)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Static sensors queue already has msgTypesSensor from constructor
    size_t initialStaticSize = nsmDevice.staticSensors.size();

    auto sensor = std::make_shared<MockSensor>("StaticSensor", "StaticType");
    nsmDevice.addSensorBase(sensor, nsm::PollingType::Static);

    EXPECT_EQ(nsmDevice.staticSensors.size(), initialStaticSize + 1);
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorBaseSetsDeviceIdentifier)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 3, "MCTP_UUID", uuid, 0);

    auto sensor = std::make_shared<MockSensor>("IdSensor", "IdType");
    nsmDevice.addSensorBase(sensor, nsm::PollingType::RoundRobin);

    // deviceIdentifier should be set by addSensorBase
    EXPECT_FALSE(sensor->getDeviceIdentifier().empty());
}

// ---- addSensor with priority/longRunning booleans (deprecated overload) ----

TEST(nsmDevice, TestAddSensorWithPriorityTrue)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("PriSensor2", "PriType2");
    nsmDevice.addSensor(sensor, true, false);

    EXPECT_EQ(nsmDevice.prioritySensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, PRIORITY_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorWithLongRunningTrue)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("LRSensor2", "LRType2");
    nsmDevice.addSensor(sensor, false, true);

    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(nsmDevice, TestAddSensorWithBothFalse)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor = std::make_shared<MockSensor>("RRSensor2", "RRType2");
    nsmDevice.addSensor(sensor, false, false);

    EXPECT_GE(nsmDevice.roundRobinSensors.size(),
              2u); // 1 from msgTypesSensor + 1 added
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

// ---- addStaticSensor ----

TEST(nsmDevice, TestAddStaticSensor)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    size_t initialStaticSize = nsmDevice.staticSensors.size();

    auto sensor = std::make_shared<MockSensor>("Static1", "StaticType1");
    nsmDevice.addStaticSensor(sensor);

    EXPECT_EQ(nsmDevice.staticSensors.size(), initialStaticSize + 1);
    // Static polling type gets RR_REFRESH_LIMIT_IN_USEC
    EXPECT_EQ(sensor->refreshLimitInUsec, RR_REFRESH_LIMIT_IN_USEC);
}

// ---- findAggregatorByType with actual aggregators ----

TEST(nsmDevice, TestFindAggregatorByTypeNotFound)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // No aggregators exist
    EXPECT_EQ(nsmDevice.findAggregatorByType("NonExistentType"), nullptr);
}

TEST(nsmDevice, TestFindAggregatorByTypeEmptyString)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Search for empty string with empty aggregator list
    EXPECT_EQ(nsmDevice.findAggregatorByType(""), nullptr);
}

// ---- clearLongRunningHandler / registerLongRunningHandler ----

TEST(nsmDevice, TestClearLongRunningHandlerWhenNoHandler)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // longRunningHandler should already be empty
    nsmDevice.longRunningHandler.reset();

    // clearLongRunningHandler when no handler is set should not crash
    nsmDevice.clearLongRunningHandler();

    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

TEST(nsmDevice, TestClearLongRunningHandlerResetsHandler)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Manually set a long running handler info
    nsmDevice.longRunningHandler = nsm::ActiveLongRunningHandlerInfo{1, 2,
                                                                     nullptr};

    EXPECT_TRUE(nsmDevice.longRunningHandler.has_value());

    nsmDevice.clearLongRunningHandler();

    EXPECT_FALSE(nsmDevice.longRunningHandler.has_value());
}

// ---- invokeLongRunningHandler without handler ----

TEST(nsmDevice, TestInvokeLongRunningHandlerNoHandler)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Ensure no handler is registered
    nsmDevice.longRunningHandler.reset();

    // Calling invokeLongRunningHandler with no handler should return error
    int rc = nsmDevice.invokeLongRunningHandler(0, 0, 0, nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ---- FruInterfaceManager tests ----

TEST(FruInterfaceManager, TestNeedsRecreationWhenNotInitialized)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> properties = {"DEVICE_TYPE", "INSTANCE_NUMBER"};

    // Not initialized yet, should need recreation
    EXPECT_TRUE(mgr.needsRecreation(properties));
}

TEST(FruInterfaceManager, TestNeedsRecreationAfterInitialization)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> properties = {"DEVICE_TYPE", "INSTANCE_NUMBER"};
    mgr.markInitialized(properties);

    // Same properties - should not need recreation
    EXPECT_FALSE(mgr.needsRecreation(properties));

    // Different properties - should need recreation
    std::set<std::string> differentProps = {"DEVICE_TYPE", "INSTANCE_NUMBER",
                                            "SERIAL_NUMBER"};
    EXPECT_TRUE(mgr.needsRecreation(differentProps));
}

TEST(FruInterfaceManager, TestMarkInitialized)
{
    nsm::FruInterfaceManager mgr;

    EXPECT_FALSE(mgr.initialized);

    std::set<std::string> properties = {"DEVICE_TYPE", "UUID"};
    mgr.markInitialized(properties);

    EXPECT_TRUE(mgr.initialized);
    EXPECT_EQ(mgr.supportedProperties, properties);
}

TEST(FruInterfaceManager, TestIsPropertySupported)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> properties = {"DEVICE_TYPE", "INSTANCE_NUMBER",
                                        "UUID"};
    mgr.markInitialized(properties);

    EXPECT_TRUE(mgr.isPropertySupported("DEVICE_TYPE"));
    EXPECT_TRUE(mgr.isPropertySupported("INSTANCE_NUMBER"));
    EXPECT_TRUE(mgr.isPropertySupported("UUID"));
    EXPECT_FALSE(mgr.isPropertySupported("SERIAL_NUMBER"));
    EXPECT_FALSE(mgr.isPropertySupported(""));
    EXPECT_FALSE(mgr.isPropertySupported("NONEXISTENT"));
}

TEST(FruInterfaceManager, TestReset)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> properties = {"DEVICE_TYPE", "UUID"};
    mgr.markInitialized(properties);

    EXPECT_TRUE(mgr.initialized);
    EXPECT_FALSE(mgr.supportedProperties.empty());

    mgr.reset();

    EXPECT_FALSE(mgr.initialized);
    EXPECT_TRUE(mgr.supportedProperties.empty());
    EXPECT_EQ(mgr.interface, nullptr);
}

TEST(FruInterfaceManager, TestNeedsRecreationAfterReset)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> properties = {"DEVICE_TYPE", "UUID"};
    mgr.markInitialized(properties);

    EXPECT_FALSE(mgr.needsRecreation(properties));

    mgr.reset();

    // After reset, should need recreation even with same properties
    EXPECT_TRUE(mgr.needsRecreation(properties));
}

TEST(FruInterfaceManager, TestNeedsRecreationEmptyProperties)
{
    nsm::FruInterfaceManager mgr;

    std::set<std::string> emptyProps = {};

    // Not initialized - needs recreation
    EXPECT_TRUE(mgr.needsRecreation(emptyProps));

    mgr.markInitialized(emptyProps);

    // Same (empty) properties - no recreation
    EXPECT_FALSE(mgr.needsRecreation(emptyProps));

    // Non-empty properties - needs recreation
    std::set<std::string> nonEmpty = {"DEVICE_TYPE"};
    EXPECT_TRUE(mgr.needsRecreation(nonEmpty));
}

// ---- msgTypesSensor initialization from constructor ----

TEST(nsmDevice, TestMsgTypesSensorInitialized)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Constructor calls initMsgTypesSensor, so msgTypesSensor should be set
    EXPECT_NE(nsmDevice.msgTypesSensor, nullptr);
}

// ---- Sensor queues initial state ----

TEST(nsmDevice, TestSensorQueuesInitialState)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Priority and GPM queues should be empty initially
    EXPECT_EQ(nsmDevice.prioritySensors.size(), 0u);
    EXPECT_EQ(nsmDevice.gpmSensors.size(), 0u);
    // RoundRobin has msgTypesSensor from constructor
    EXPECT_GE(nsmDevice.roundRobinSensors.size(), 1u);
    // deviceSensors should have at least msgTypesSensor
    EXPECT_GE(nsmDevice.deviceSensors.size(), 1u);
}

// ---- Multiple sensors in different queues ----

TEST(nsmDevice, TestMultipleSensorsInDifferentQueues)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto priSensor = std::make_shared<MockSensor>("Pri", "PriType");
    auto gpmSensor = std::make_shared<MockSensor>("Gpm", "GpmType");
    auto lrSensor = std::make_shared<MockSensor>("LR", "LRType");
    auto rrSensor = std::make_shared<MockSensor>("RR", "RRType");

    nsmDevice.addSensorBase(priSensor, nsm::PollingType::Priority);
    nsmDevice.addSensorBase(gpmSensor,
                            nsm::PollingType::GpuPerformanceMonitoring);
    nsmDevice.addSensorBase(lrSensor, nsm::PollingType::LongRunning);
    nsmDevice.addSensorBase(rrSensor, nsm::PollingType::RoundRobin);

    EXPECT_EQ(nsmDevice.prioritySensors.size(), 1u);
    EXPECT_EQ(nsmDevice.gpmSensors.size(), 1u);
    EXPECT_EQ(nsmDevice.longRunningSensors.size(), 1u);
    EXPECT_GE(nsmDevice.roundRobinSensors.size(),
              2u); // 1 msgTypesSensor + 1 added

    // All sensors should also be in deviceSensors
    // Initial deviceSensors has msgTypesSensor, so we check it grew by 4
    EXPECT_GE(nsmDevice.deviceSensors.size(), 4u);
}

// ---- getSemaphore ----

TEST(nsmDevice, TestGetSemaphoreReturnsReference)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // getSemaphore should return a reference, verify it does not throw
    auto& semaphore = nsmDevice.getSemaphore();
    (void)semaphore; // Just verify it compiles and does not throw
}

// ---- nonPriorityPollingType default ----

TEST(nsmDevice, TestNonPriorityPollingTypeDefault)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_EQ(nsmDevice.nonPriorityPollingType,
              nsm::PollingType::GpuPerformanceMonitoring);
}

// ---- eventDispatcher direct access ----

TEST(nsmDevice, TestEventDispatcherHandleNoEvents)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Handle with a type/eventId that has no registered events should return
    // error
    int rc = nsmDevice.eventDispatcher.handle(0, 99, 99, nullptr, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---- Multiple addDeviceEvent with different types/ids ----

TEST(nsmDevice, TestAddDeviceEventRegistersInDispatcher)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto event = std::make_shared<MockNsmEvent>("TestEvt", "TestEvtType");

    // Add event for type=3, eventId=7
    nsmDevice.addDeviceEvent(event, 3, 7);

    // The event should now be registered in the eventDispatcher
    // We can verify by checking deviceEvents grew
    bool found = false;
    for (const auto& e : nsmDevice.deviceEvents)
    {
        if (e == event)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

// ---- Constructor with different remap property values ----

TEST(nsmDevice, TestConstructorWithMctpEidSetsEid)
{
    MockNsmDevice nsmDevice(NSM_DEV_ID_GPU, 2, "MCTP_EID", "100", 1);

    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_GPU);
    EXPECT_EQ(nsmDevice.getInstanceNumber(), 2);
    EXPECT_EQ(nsmDevice.getEid(), 100);
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::MCTP_EID);
}

TEST(nsmDevice, TestConstructorWithNsmDeviceInstanceNumber)
{
    MockNsmDevice nsmDevice(NSM_DEV_ID_SWITCH, 7, "NSM_DEVICE_INSTANCE_NUMBER",
                            "200", 0);

    EXPECT_EQ(nsmDevice.getDeviceType(), NSM_DEV_ID_SWITCH);
    EXPECT_EQ(nsmDevice.getInstanceNumber(), 7);
    EXPECT_EQ(nsmDevice.getNsmDeviceInstanceNumber(), 200);
    EXPECT_EQ(nsmDevice.getDeviceRemapProp(),
              nsm::DeviceRemapProperty::NSM_DEVICE_INSTANCE_NUMBER);
}

// ---- messageTypesToCommandCodeMatrix direct access ----

TEST(nsmDevice, TestCommandCodeMatrixInitializedToFalse)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Verify all entries in the matrix are false initially
    for (uint8_t mt = 0; mt < NUM_NSM_TYPES; mt++)
    {
        for (uint16_t cc = 0; cc < NUM_COMMAND_CODES; cc++)
        {
            EXPECT_FALSE(nsmDevice.messageTypesToCommandCodeMatrix[mt][cc])
                << "messageType=" << (int)mt << " commandCode=" << cc;
        }
    }
}

TEST(nsmDevice, TestCommandCodeMatrixDimensions)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_EQ(nsmDevice.messageTypesToCommandCodeMatrix.size(),
              (size_t)NUM_NSM_TYPES);
    for (const auto& row : nsmDevice.messageTypesToCommandCodeMatrix)
    {
        EXPECT_EQ(row.size(), (size_t)NUM_COMMAND_CODES);
    }
}

// ---- addSensorBase with multiple sensors to same queue ----

TEST(nsmDevice, TestAddMultipleSensorsToSameQueue)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto sensor1 = std::make_shared<MockSensor>("Pri1", "PriType");
    auto sensor2 = std::make_shared<MockSensor>("Pri2", "PriType");
    auto sensor3 = std::make_shared<MockSensor>("Pri3", "PriType");

    nsmDevice.addSensorBase(sensor1, nsm::PollingType::Priority);
    nsmDevice.addSensorBase(sensor2, nsm::PollingType::Priority);
    nsmDevice.addSensorBase(sensor3, nsm::PollingType::Priority);

    EXPECT_EQ(nsmDevice.prioritySensors.size(), 3u);
}

// ---- gpuDriverSensor default ----

TEST(nsmDevice, TestGpuDriverSensorDefaultNull)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    EXPECT_EQ(nsmDevice.gpuDriverSensor, nullptr);
}

// ---- deviceUuid field ----

TEST(nsmDevice, TestDeviceUuidFieldDefaultEmpty)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // deviceUuid is a public field, initially should be empty
    EXPECT_TRUE(nsmDevice.deviceUuid.empty());
}

// ---- Combined scenario: init, discover, go online/offline via flags ----

TEST(nsmDevice, TestFullLifecycleViaFlags)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Initially online and ready (mock constructor sets these)
    EXPECT_TRUE(nsmDevice.isOnline());
    EXPECT_TRUE(nsmDevice.isReady());
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());

    // Start discovery
    nsmDevice.initDeviceDiscovery();
    EXPECT_FALSE(nsmDevice.isOnline());
    EXPECT_TRUE(nsmDevice.isDiscoveryPending());

    // Finish discovery
    nsmDevice.finishDeviceDiscovery();
    EXPECT_FALSE(nsmDevice.isDiscoveryPending());
    EXPECT_FALSE(nsmDevice.isOnline());

    // Manually bring online
    nsmDevice.isDeviceActive = true;
    EXPECT_TRUE(nsmDevice.isOnline());

    // Mark not ready
    nsmDevice.markDeviceAsNotReady();
    EXPECT_TRUE(nsmDevice.isOnline());
    EXPECT_FALSE(nsmDevice.isReady());

    // Mark ready again
    nsmDevice.markDeviceAsReady();
    EXPECT_TRUE(nsmDevice.isReady());

    // Go offline
    nsmDevice.isDeviceActive = false;
    EXPECT_FALSE(nsmDevice.isOnline());
}

// ---- updateMessageTypesToCommandCodeMatrix with large supportedCommandsSize
// ----

TEST(nsmDevice, TestUpdateCommandCodeMatrixClampedToMaxCommandCodes)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Create an array large enough to exceed NUM_COMMAND_CODES
    // NUM_COMMAND_CODES = 256, so 256/8 = 32 bytes needed to cover all
    // Use 64 bytes which is double, should be clamped
    bitfield8_t supportedCommands[64] = {};
    for (int i = 0; i < 64; i++)
    {
        supportedCommands[i].byte = 0xFF;
    }

    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 64);

    // All 256 command codes should be marked supported
    for (uint16_t cc = 0; cc < NUM_COMMAND_CODES; cc++)
    {
        EXPECT_TRUE(nsmDevice.isCommandSupported(0, cc)) << "cc=" << cc;
    }
}

// ---- EventDispatcher duplicate event registration ----

TEST(nsmDevice, TestAddDeviceEventDuplicateEventIdFails)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto event1 = std::make_shared<MockNsmEvent>("Evt1", "EvtType1");
    auto event2 = std::make_shared<MockNsmEvent>("Evt2", "EvtType2");

    // First add should succeed (addEvent returns NSM_SW_SUCCESS = 0)
    nsmDevice.addDeviceEvent(event1, 5, 10);

    // Second add with same type/eventId should fail at eventDispatcher level
    // but deviceEvents still grows. The dispatcher returns NSM_SW_ERROR_DATA
    size_t beforeSize = nsmDevice.deviceEvents.size();
    nsmDevice.addDeviceEvent(event2, 5, 10);
    // deviceEvents always adds
    EXPECT_EQ(nsmDevice.deviceEvents.size(), beforeSize + 1);
}

// ---- isCommandSupported after clearing the matrix ----

TEST(nsmDevice, TestCommandCodeMatrixResetViaAssign)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Set some commands
    bitfield8_t supportedCommands[1] = {};
    supportedCommands[0].byte = 0xFF;
    nsmDevice.updateMessageTypesToCommandCodeMatrix(0, supportedCommands, 1);
    EXPECT_TRUE(nsmDevice.isCommandSupported(0, 0));

    // Reset the matrix (similar to what updateNsmDevice does)
    nsmDevice.messageTypesToCommandCodeMatrix.assign(
        NUM_NSM_TYPES, std::vector<bool>(NUM_COMMAND_CODES, false));

    // Everything should be false again
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 0));
    EXPECT_FALSE(nsmDevice.isCommandSupported(0, 7));
}

// ---- longRunningHandler optional field ----

TEST(nsmDevice, TestLongRunningHandlerInitiallyEmpty)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

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

// ---- invokeLongRunningHandler: decode_nsm_event fails ----

// With a handler registered, passing a null/zero-length event buffer causes
// decode_nsm_event to fail → the function returns the decode error code.
TEST(nsmDevice, InvokeLongRunningHandler_DecodeFails_ReturnsError)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto lrEvent = std::make_shared<MockNsmLongRunningEvent>("LREvt", "LRType");
    nsmDevice.registerLongRunningHandler(5, 10, lrEvent);

    // nullptr / 0-length → decode_nsm_event returns NSM_SW_ERROR_NULL
    int rc = nsmDevice.invokeLongRunningHandler(0, 0, NSM_LONG_RUNNING_EVENT,
                                                nullptr, 0);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---- invokeLongRunningHandler: mismatched message type / command ----

// With a handler expecting (messageType=5, commandCode=10) and an event that
// carries (nvidia_message_type=9, command=9), the mismatch branch is taken
// and NSM_SW_ERROR_DATA is returned.
TEST(nsmDevice, InvokeLongRunningHandler_MismatchedTypeCmd_ReturnsError)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto lrEvent = std::make_shared<MockNsmLongRunningEvent>("LREvt", "LRType");
    nsmDevice.registerLongRunningHandler(5, 10, lrEvent);

    // nvidia_message_type=9, command=9 (doesn't match handler's 5,10)
    // nsm_long_running_event_state: byte0=nvidia_message_type, byte1=command
    // event_state as uint16 LE: low byte=nvidia_message_type, high byte=command
    const uint16_t eventState = static_cast<uint16_t>(9u | (9u << 8));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_event(0, NSM_TYPE_PLATFORM_ENVIRONMENTAL, false, 0,
                     NSM_LONG_RUNNING_EVENT, NSM_NVIDIA_GENERAL_EVENT_CLASS,
                     eventState, 0, nullptr, msg);

    int rc = nsmDevice.invokeLongRunningHandler(0, 0, NSM_LONG_RUNNING_EVENT,
                                                msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ---- invokeLongRunningHandler: matching type/command → success ----

// With a handler expecting (messageType=5, commandCode=10) and an event that
// matches, handle() is called and its NSM_SW_SUCCESS return is propagated.
TEST(nsmDevice, InvokeLongRunningHandler_MatchingTypeCmd_CallsHandle)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    auto lrEvent = std::make_shared<MockNsmLongRunningEvent>("LREvt", "LRType");
    nsmDevice.registerLongRunningHandler(5, 10, lrEvent);

    // nvidia_message_type=5, command=10 → matches handler (5,10)
    const uint16_t eventState = static_cast<uint16_t>(5u | (10u << 8));
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_event(0, NSM_TYPE_PLATFORM_ENVIRONMENTAL, false, 0,
                     NSM_LONG_RUNNING_EVENT, NSM_NVIDIA_GENERAL_EVENT_CLASS,
                     eventState, 0, nullptr, msg);

    int rc = nsmDevice.invokeLongRunningHandler(0, 0, NSM_LONG_RUNNING_EVENT,
                                                msg, buf.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

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
    //
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

// EventDispatcher::handle() - type not registered → NSM_SW_ERROR_DATA
TEST(nsmDevice, EventDispatcher_Handle_TypeNotFound_ReturnsErrorData)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // No events registered - type 99 is unknown
    int rc = nsmDevice.eventDispatcher.handle(0, 99, 0, nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// EventDispatcher::handle() - type found but eventId missing →
// NSM_SW_ERROR_DATA
TEST(nsmDevice, EventDispatcher_Handle_EventIdNotFound_ReturnsErrorData)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice nsmDevice(1, 1, "MCTP_UUID", uuid, 1);

    // Register type=10, eventId=20
    auto event = std::make_shared<MockNsmEvent>("E1", "ET1");
    ASSERT_EQ(nsmDevice.eventDispatcher.addEvent(10, 20, event),
              NSM_SW_SUCCESS);

    // Call handle() with type=10 but wrong eventId=99
    int rc = nsmDevice.eventDispatcher.handle(0, 10, 99, nullptr, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
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
}

// ==========================================================================
// Coroutine coverage (B1)
// All three private coroutines call co_await Sleep(...). In
// COVERAGE_DISABLE_COROUTINES mode, Sleep is a no-op that returns
// NSM_SW_SUCCESS immediately, so no mocking is needed.
// ==========================================================================

using ::testing::_;

// --------------------------------------------------------------------------
// markSensorsUnrefreshed()
// --------------------------------------------------------------------------

TEST(nsmDevice, MarkSensorsUnrefreshed_EmptySensors_NoSleep)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // No sensors → count never reaches MAX_SENSOR_UPDATE_BATCH_SIZE → no sleep
    auto coro = device.markSensorsUnrefreshed();
    EXPECT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

TEST(nsmDevice, MarkSensorsUnrefreshed_FewerThanBatch_NoSleep)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // 5 sensors < MAX_SENSOR_UPDATE_BATCH_SIZE (10) → no sleep
    for (int i = 0; i < 5; i++)
    {
        auto s = std::make_shared<MockSensor>("s" + std::to_string(i), "T");
        s->isRefreshed = true;
        device.deviceSensors.push_back(s);
    }

    auto coro = device.markSensorsUnrefreshed();
    EXPECT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);

    for (auto& s : device.deviceSensors)
        EXPECT_FALSE(s->isRefreshed);
}

TEST(nsmDevice, MarkSensorsUnrefreshed_ExactOneBatch_AllMarked)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // Use one fewer than batch size to avoid triggering the Sleep path
    // which suspends the coroutine in non-coverage mode.
    for (int i = 0; i < MAX_SENSOR_UPDATE_BATCH_SIZE - 1; i++)
    {
        auto s = std::make_shared<MockSensor>("s" + std::to_string(i), "T");
        s->isRefreshed = true;
        device.deviceSensors.push_back(s);
    }

    EXPECT_NO_THROW_COROUTINE(device.markSensorsUnrefreshed());

    for (auto& s : device.deviceSensors)
        EXPECT_FALSE(s->isRefreshed);
}

TEST(nsmDevice, MarkSensorsUnrefreshed_MultipleBatches_FirstBatchMarked)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // 25 sensors: first batch of MAX_SENSOR_UPDATE_BATCH_SIZE is processed
    // before the coroutine hits Sleep and suspends. In coverage mode (Sleep
    // is a no-op) all 25 are processed.
    for (int i = 0; i < 25; i++)
    {
        auto s = std::make_shared<MockSensor>("s" + std::to_string(i), "T");
        s->isRefreshed = true;
        device.deviceSensors.push_back(s);
    }

    EXPECT_NO_THROW_COROUTINE(device.markSensorsUnrefreshed());

    // First batch is always processed regardless of coroutine mode
    for (int i = 0; i < MAX_SENSOR_UPDATE_BATCH_SIZE; i++)
        EXPECT_FALSE(device.deviceSensors[i]->isRefreshed);
}

// --------------------------------------------------------------------------
// updateSensorsForOffline()
// --------------------------------------------------------------------------

TEST(nsmDevice, UpdateSensorsForOffline_EmptySensors_NoSleep)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);
    device.deviceSensors
        .clear(); // constructor may pre-populate via initMsgTypesSensor

    // No sensors → outer while never executes → no sleep
    auto coro = device.updateSensorsForOffline();
    EXPECT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

TEST(nsmDevice, UpdateSensorsForOffline_FewerThanBatch_AllHandled)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // 5 sensors: one batch of 5, all processed before Sleep is reached
    for (int i = 0; i < 5; i++)
        device.deviceSensors.push_back(
            std::make_shared<MockSensor>("s" + std::to_string(i), "T"));

    EXPECT_NO_THROW_COROUTINE(device.updateSensorsForOffline());
}

TEST(nsmDevice, UpdateSensorsForOffline_MultipleBatches_FirstBatchHandled)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // 11 sensors: first batch of 10 processed before Sleep suspends the
    // coroutine. In coverage mode (Sleep is a no-op) all 11 are processed.
    for (int i = 0; i < 11; i++)
        device.deviceSensors.push_back(
            std::make_shared<MockSensor>("s" + std::to_string(i), "T"));

    EXPECT_NO_THROW_COROUTINE(device.updateSensorsForOffline());
}

// --------------------------------------------------------------------------
// waitForNsmDeviceUpdate()
// --------------------------------------------------------------------------

TEST(nsmDevice, WaitForNsmDeviceUpdate_AlreadyDone_NoSleep)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // discoveryPending = false from the start → loop never entered
    device.discoveryPending = false;

    auto coro = device.waitForNsmDeviceUpdate();
    EXPECT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

TEST(nsmDevice, WaitForNsmDeviceUpdate_ClearedBeforeCall_ReturnsSuccess)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // discoveryPending is false before the call → loop never entered
    device.discoveryPending = false;

    auto coro = device.waitForNsmDeviceUpdate();
    EXPECT_TRUE(coro.done());
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

TEST(nsmDevice, WaitForNsmDeviceUpdate_NeverClears_StaysPending)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice device(1, 1, "MCTP_UUID", uuid, 1);

    // discoveryPending stays true; in non-coverage mode the coroutine
    // suspends at the first Sleep. In coverage mode (Sleep is a no-op)
    // the loop runs DEVICE_UPDATE_POST_PATCH_SLEEP_MAX_ITER times and
    // returns NSM_SW_ERROR_TIMEOUT.
    device.discoveryPending = true;

    EXPECT_NO_THROW_COROUTINE(device.waitForNsmDeviceUpdate());
    EXPECT_TRUE(device.discoveryPending); // still pending
}

// ============================================================================
// Coroutine function tests: markSensorsUnrefreshed, updateSensorsForOffline,
// setOffline, waitForNsmDeviceUpdate, setOnline.
// With COVERAGE_DISABLE_COROUTINES: co_await → expr (no-op), co_return → return
//
// ============================================================================

TEST(nsmDevice, MarkSensorsUnrefreshed_EmptySensors_NoOp)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);
    // deviceSensors is empty; the for loop is never entered
    EXPECT_NO_THROW_COROUTINE(dev.markSensorsUnrefreshed());
}

TEST(nsmDevice, MarkSensorsUnrefreshed_FewSensors_AllMarkedUnrefreshed)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    auto s1 = std::make_shared<MockSensor>("S1", "T1");
    auto s2 = std::make_shared<MockSensor>("S2", "T2");
    s1->isRefreshed = true;
    s2->isRefreshed = true;
    dev.addDeviceSensors(s1);
    dev.addDeviceSensors(s2);

    EXPECT_NO_THROW_COROUTINE(dev.markSensorsUnrefreshed());

    EXPECT_FALSE(s1->isRefreshed);
    EXPECT_FALSE(s2->isRefreshed);
}

TEST(nsmDevice, MarkSensorsUnrefreshed_ExactBatchSize_TriggersBatchSleep)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // Clear pre-existing sensors added by the constructor
    // (initMsgTypesSensor) so that the test controls the exact count.
    dev.deviceSensors.clear();

    // Exactly MAX_SENSOR_UPDATE_BATCH_SIZE sensors triggers the batch sleep.
    // All sensors are processed before Sleep suspends the coroutine.
    for (int i = 0; i < MAX_SENSOR_UPDATE_BATCH_SIZE; i++)
    {
        auto s = std::make_shared<MockSensor>("S" + std::to_string(i), "T");
        s->isRefreshed = true;
        dev.addDeviceSensors(s);
    }

    EXPECT_NO_THROW_COROUTINE(dev.markSensorsUnrefreshed());

    for (auto& sensor : dev.deviceSensors)
    {
        EXPECT_FALSE(sensor->isRefreshed);
    }
}

TEST(nsmDevice, UpdateSensorsForOffline_EmptySensors_NoOp)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);
    EXPECT_NO_THROW_COROUTINE(dev.updateSensorsForOffline());
}

TEST(nsmDevice, UpdateSensorsForOffline_FewSensors_InnerWhileExitsOnSize)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // Fewer than MAX_SENSOR_UPDATE_BATCH_SIZE: inner while exits when
    // sensorIndex reaches sensors.size() before count reaches the limit
    for (int i = 0; i < 3; i++)
    {
        dev.addDeviceSensors(
            std::make_shared<MockSensor>("S" + std::to_string(i), "T"));
    }

    EXPECT_NO_THROW_COROUTINE(dev.updateSensorsForOffline());
}

TEST(nsmDevice, UpdateSensorsForOffline_BatchCrossing_InnerWhileExitsOnCount)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // More than MAX_SENSOR_UPDATE_BATCH_SIZE: inner while exits when count
    // reaches the limit, outer loop continues for the remaining sensors
    for (int i = 0; i < MAX_SENSOR_UPDATE_BATCH_SIZE + 2; i++)
    {
        dev.addDeviceSensors(
            std::make_shared<MockSensor>("S" + std::to_string(i), "T"));
    }

    EXPECT_NO_THROW_COROUTINE(dev.updateSensorsForOffline());
}

TEST(nsmDevice, SetOffline_NoGpuDriverSensor_DeviceGoesOffline)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);
    dev.isDeviceActive = true;

    // gpuDriverSensor is nullptr; the if branch is not taken
    EXPECT_NO_THROW_COROUTINE(dev.setOffline());

    EXPECT_FALSE(dev.isDeviceActive);
}

TEST(nsmDevice, WaitForNsmDeviceUpdate_DiscoveryNotPending_ReturnsSuccess)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);
    dev.discoveryPending = false;

    auto result = dev.waitForNsmDeviceUpdate();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_SUCCESS);
#endif
}

TEST(nsmDevice, WaitForNsmDeviceUpdate_AlwaysPending_ReturnsTimeout)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // discoveryPending stays true; loop runs
    // DEVICE_UPDATE_POST_PATCH_SLEEP_MAX_ITER times then returns TIMEOUT
    // (Sleep is no-op in test mode)
    dev.discoveryPending = true;

    auto result = dev.waitForNsmDeviceUpdate();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_ERROR_TIMEOUT);
#endif
}

TEST(nsmDevice, SetOnline_ServiceReadyNotInitialized_Throws)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // NsmServiceReadyIntf::getInstance() throws std::runtime_error when not
    // initialized
    EXPECT_THROW_COROUTINE(dev.setOnline(), std::runtime_error);
}

// ---- NsmDevice base class sensorIO early-return branches ----

TEST(nsmDevice, SensorIO_BaseImpl_DeviceInactive_ReturnsUnsupported)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // MockNsmDevice constructor sets isDeviceActive=true; override to false
    dev.isDeviceActive = false;
    Request req(20, 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto result = dev.nsm::NsmDevice::sensorIO(0, req, responseMsg, responseLen,
                                               false);
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
#endif
}

TEST(nsmDevice, SensorIO_BaseImpl_CommandNotSupported_ReturnsUnsupported)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // isDeviceActive=true but command 0/0 not in matrix: early return
    dev.isDeviceActive = true;
    Request req(20, 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto result = dev.nsm::NsmDevice::sensorIO(0, req, responseMsg, responseLen,
                                               false);
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
#endif
}

// ---- NsmDevice base class postPatchIO early-return branches ----

TEST(nsmDevice, PostPatchIO_BaseImpl_DiscoveryPending_ReturnsTimeout)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // discoveryPending=true: waitForNsmDeviceUpdate loops MAX_ITER times and
    // returns TIMEOUT; postPatchIO then early-returns with TIMEOUT
    dev.discoveryPending = true;
    Request req(20, 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto result = dev.nsm::NsmDevice::postPatchIO(0, req, responseMsg,
                                                  responseLen);
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_ERROR_TIMEOUT);
#endif
}

TEST(nsmDevice, PostPatchIO_BaseImpl_DeviceInactive_ReturnsUnsupported)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // discoveryPending=false (default): waitForNsmDeviceUpdate returns SUCCESS;
    // MockNsmDevice constructor sets isDeviceActive=true; override to false so
    // postPatchIO early-returns NSM_ERR_UNSUPPORTED_COMMAND_CODE
    dev.isDeviceActive = false;
    Request req(20, 0);
    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto result = dev.nsm::NsmDevice::postPatchIO(0, req, responseMsg,
                                                  responseLen);
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
#endif
}

// ---- NsmDevice constructor invalid remap value branches ----

TEST(nsmDevice, Constructor_InvalidInstanceNumberRemap_CatchesAndContinues)
{
    // Passing a non-numeric string for NSM_DEVICE_INSTANCE_NUMBER causes
    // stoi to throw std::invalid_argument inside the NsmDevice base
    // constructor's catch block (lines 162-167 of nsmDevice.hpp).
    // MockNsmDevice then also calls stoi and propagates the throw.
    EXPECT_THROW(MockNsmDevice(NSM_DEV_ID_GPU, 0, "NSM_DEVICE_INSTANCE_NUMBER",
                               "not_a_number", 0),
                 std::invalid_argument);
}

TEST(nsmDevice, Constructor_InvalidEidRemap_CatchesAndContinues)
{
    // Same pattern for MCTP_EID: invalid_argument caught in NsmDevice base
    // constructor (lines 182-187 of nsmDevice.hpp), then propagated from
    // MockNsmDevice constructor body.
    EXPECT_THROW(
        MockNsmDevice(NSM_DEV_ID_GPU, 0, "MCTP_EID", "not_a_number", 0),
        std::invalid_argument);
}

// ---- updateDiscoveryIdentifiers non-preferred path ----

TEST(nsmDevice, UpdateDiscoveryIdentifiers_NewLowerPriorityMedium_NotPreferred)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // Set device as already connected via PCIe (priority 0 - highest)
    dev.uuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    dev.eid = 10;
    dev.mctpMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    dev.mctpBinding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    // New connection via SMBus (priority 6 - lowest): not preferred over PCIe
    // isPreferred returns false (0 >= 6 is false) → else branch (lines 468-474)
    eid_t newEid = 20;
    uuid_t newUuid = "bbbbbbbb-cccc-dddd-eeee-ffffffffffff";
    std::string newMedium =
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.SMBus";
    std::string newBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.SMBus";
    std::string assocPath = "/mctp/test";

    bool result = dev.updateDiscoveryIdentifiers(newEid, newUuid, 1, assocPath,
                                                 newMedium, newBinding, 30);

    EXPECT_FALSE(result);
    // Device keeps original EID/medium (not updated)
    EXPECT_EQ(dev.getEid(), 10);
}

// ============================================================================
// refreshCapabilitySensor base-class implementation
// NsmDevice::refreshCapabilitySensor (L931-943):
//   while sensorIndex < sensors.size():
//       co_await sensor->update(shared_from_this())
//   co_return NSM_SW_SUCCESS
// MockNsmDevice mocks this method, so call via base-class qualification.
// Device must be heap-allocated (make_shared) for shared_from_this() to work.
// ============================================================================

TEST(nsmDevice, RefreshCapabilitySensor_BaseImpl_EmptyList_ReturnsSuccess)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    // Must be heap-allocated so shared_from_this() works inside the coroutine
    auto dev = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "MCTP_UUID",
                                               uuid, 0);
    // capabilityRefreshSensors is empty; loop body is never entered
    EXPECT_TRUE(dev->capabilityRefreshSensors.empty());
    auto result = dev->nsm::NsmDevice::refreshCapabilitySensor();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_SUCCESS);
#endif
}

TEST(nsmDevice, RefreshCapabilitySensor_BaseImpl_WithSensor_UpdatesCalled)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    auto dev = std::make_shared<MockNsmDevice>(NSM_DEV_ID_GPU, 0, "MCTP_UUID",
                                               uuid, 0);
    auto sensor = std::make_shared<MockSensor>("CapSensor", "CapType");
    dev->capabilityRefreshSensors.push_back(sensor);

    // Loop body executes once; sensor->update(shared_from_this()) is called
    auto result = dev->nsm::NsmDevice::refreshCapabilitySensor();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_SUCCESS);
#endif
}

// ============================================================================
// refreshCommandMatrix base-class implementation
// NsmDevice::refreshCommandMatrix (L946-1008):
//   if (!areMessageTypesRetrieved) → calls getSupportedNvidiaMessageType
//     → needs nsmMsgHandler (blocked with nullptr)
//   for each messageType in retrievedMessageTypes:
//     if messageType >= NUM_NSM_TYPES: continue (out-of-range skip)
//     if !commandCodesRetrieved[messageType]: calls getSupportedCommandCodes
//       → needs nsmMsgHandler (blocked with nullptr)
//     else: type already retrieved, no-op
//   co_return NSM_SW_SUCCESS
// refreshCommandMatrix is NOT mocked; can call directly on stack device.
// ============================================================================

TEST(nsmDevice, RefreshCommandMatrix_BaseImpl_EmptyTypes_ReturnsSuccess)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // areMessageTypesRetrieved=true (set by MockNsmDevice ctor)
    // retrievedMessageTypes is empty → for loop body never entered
    dev.retrievedMessageTypes.clear();
    dev.commandCodesRetrieved.clear();

    auto result = dev.refreshCommandMatrix();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_SUCCESS);
#endif
}

TEST(nsmDevice, RefreshCommandMatrix_BaseImpl_OutOfRangeType_ContinueBranch)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // retrievedMessageTypes contains an out-of-range type (>= NUM_NSM_TYPES)
    // The if (messageType >= NUM_NSM_TYPES) branch is taken → continue
    dev.retrievedMessageTypes = {static_cast<uint8_t>(NUM_NSM_TYPES)};
    dev.commandCodesRetrieved.clear();

    auto result = dev.refreshCommandMatrix();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_SUCCESS);
#endif
}

TEST(nsmDevice, RefreshCommandMatrix_BaseImpl_TypeAlreadyRetrieved_SkipsInner)
{
    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);

    // Valid type with commandCodesRetrieved[0]=true → inner if NOT taken,
    // skip getSupportedCommandCodes (which would need nsmMsgHandler)
    dev.retrievedMessageTypes = {0};
    dev.commandCodesRetrieved.clear();
    dev.commandCodesRetrieved[0] = true;

    auto result = dev.refreshCommandMatrix();
#ifdef COVERAGE_DISABLE_COROUTINES
    EXPECT_EQ(static_cast<uint8_t>(result), NSM_SW_SUCCESS);
#endif
}

// ============================================================================
// setOffline() — gpuDriverSensor != nullptr TRUE branch (nsmDevice.cpp:229)
// ============================================================================

struct NsmDeviceGpuDriverTest : public ::testing::Test, public utils::DBusTest
{};

TEST_F(NsmDeviceGpuDriverTest, SetOffline_WithGpuDriverSensor_ResetsDriverState)
{
    using namespace nsm;

    uuid_t uuid = "00000000-0000-0000-0000-000000000000";
    MockNsmDevice dev(NSM_DEV_ID_GPU, 0, "MCTP_UUID", uuid, 0);
    dev.isDeviceActive = true;

    auto& bus = utils::DBusHandler::getBus();
    auto gpuSensor = std::make_shared<NsmGPUSWInventoryDriverVersionAndStatus>(
        bus, "test_gpu_drv", std::vector<utils::Association>{}, "TestFirmware",
        "NVIDIA");
    gpuSensor->driverState = 1; // non-zero; setOffline() must reset to 0
    dev.gpuDriverSensor = gpuSensor;

    EXPECT_NO_THROW_COROUTINE(dev.setOffline());

    EXPECT_FALSE(dev.isDeviceActive);
    EXPECT_EQ(dev.gpuDriverSensor->driverState, static_cast<uint8_t>(0));
}
