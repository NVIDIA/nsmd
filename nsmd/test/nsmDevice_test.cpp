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
}
