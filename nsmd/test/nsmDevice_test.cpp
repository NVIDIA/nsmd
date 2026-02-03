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
