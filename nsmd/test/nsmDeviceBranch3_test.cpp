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
 * Branch coverage tests for nsmd/nsmDevice.cpp (batch 3).
 *
 * Covers:
 * - NsmDevice::recordEventSubscriptionStatus: empty, starts_with "OK",
 *   contains "unsupported command", error path with request/response
 * - NsmDevice::allCommandCodesAreRetrieved: areMessageTypesRetrieved=false,
 *   out-of-range messageType, command not retrieved, all retrieved
 * - NsmDevice::updateMessageTypesToCommandCodeMatrix: invalid messageType
 *   (>= NUM_NSM_TYPES), valid messageType with bit parsing
 * - NsmDevice::setEventMode: valid mode, invalid mode (> max → no change)
 * - NsmDevice::addSensorBase: GpuPerformanceMonitoring, LongRunning,
 *   invalid PollingType (throws)
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#include "commonMock.hpp"
#include "test/mockDBusHandler.hpp"

using namespace nsm;

// ============================================================================
// NsmDevice::recordEventSubscriptionStatus — 4 condition branches
// ============================================================================

TEST(NsmDeviceBranch3, RecordEventStatus_EmptyStatus_SetsAndNoLog)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "50", 0);
    dev.eid = 50;
    // Empty status: !status.empty() is FALSE → no log
    EXPECT_NO_THROW(dev.NsmDevice::recordEventSubscriptionStatus(
        "", std::nullopt, std::nullopt));
    EXPECT_EQ(dev.lastEventSubscriptionStatus, std::string(""));
    EXPECT_FALSE(dev.lastEventSubscriptionRequest.has_value());
    EXPECT_FALSE(dev.lastEventSubscriptionResponse.has_value());
}

TEST(NsmDeviceBranch3, RecordEventStatus_StartsWithOK_NoLog)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "51", 0);
    dev.eid = 51;
    // starts_with("OK") is TRUE → no log
    EXPECT_NO_THROW(dev.NsmDevice::recordEventSubscriptionStatus(
        "OK:subscribed", std::nullopt, std::nullopt));
    EXPECT_EQ(dev.lastEventSubscriptionStatus, std::string("OK:subscribed"));
}

TEST(NsmDeviceBranch3, RecordEventStatus_UnsupportedCommand_NoLog)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "52", 0);
    dev.eid = 52;
    // find("unsupported command") != npos is TRUE → no log
    EXPECT_NO_THROW(dev.NsmDevice::recordEventSubscriptionStatus(
        "failed: unsupported command 0x01", std::nullopt, std::nullopt));
    EXPECT_EQ(dev.lastEventSubscriptionStatus,
              std::string("failed: unsupported command 0x01"));
}

TEST(NsmDeviceBranch3, RecordEventStatus_ErrorWithPayload_Logs)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "53", 0);
    dev.eid = 53;
    // non-empty, not OK, no "unsupported command" → logs error
    std::vector<uint8_t> req = {0x10, 0x20, 0x30};
    std::vector<uint8_t> resp = {0x40, 0x50};
    EXPECT_NO_THROW(dev.NsmDevice::recordEventSubscriptionStatus("WriteFailure",
                                                                 req, resp));
    EXPECT_EQ(dev.lastEventSubscriptionStatus, std::string("WriteFailure"));
    ASSERT_TRUE(dev.lastEventSubscriptionRequest.has_value());
    EXPECT_EQ(dev.lastEventSubscriptionRequest.value(), req);
    ASSERT_TRUE(dev.lastEventSubscriptionResponse.has_value());
    EXPECT_EQ(dev.lastEventSubscriptionResponse.value(), resp);
}

// ============================================================================
// NsmDevice::allCommandCodesAreRetrieved — 4 branches
// ============================================================================

TEST(NsmDeviceBranch3, AllCommandCodesRetrieved_MessageTypesNotRetrieved)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "54", 0);
    dev.areMessageTypesRetrieved = false;
    // Short-circuit: areMessageTypesRetrieved is false → returns false
    EXPECT_FALSE(dev.NsmDevice::allCommandCodesAreRetrieved());
}

TEST(NsmDeviceBranch3, AllCommandCodesRetrieved_OutOfRangeType_Skipped)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "55", 0);
    dev.areMessageTypesRetrieved = true;
    // Out-of-range messageType → treated as retrieved (returns true)
    dev.retrievedMessageTypes.push_back(
        static_cast<uint8_t>(NUM_NSM_TYPES + 5));
    EXPECT_TRUE(dev.NsmDevice::allCommandCodesAreRetrieved());
}

TEST(NsmDeviceBranch3, AllCommandCodesRetrieved_CommandNotRetrieved)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "56", 0);
    dev.areMessageTypesRetrieved = true;
    dev.retrievedMessageTypes.push_back(0);
    dev.commandCodesRetrieved[0] = false; // not retrieved yet
    EXPECT_FALSE(dev.NsmDevice::allCommandCodesAreRetrieved());
}

TEST(NsmDeviceBranch3, AllCommandCodesRetrieved_AllRetrieved_ReturnsTrue)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "57", 0);
    dev.areMessageTypesRetrieved = true;
    dev.retrievedMessageTypes.push_back(1);
    dev.commandCodesRetrieved[1] = true;
    EXPECT_TRUE(dev.NsmDevice::allCommandCodesAreRetrieved());
}

// ============================================================================
// NsmDevice::updateMessageTypesToCommandCodeMatrix
// ============================================================================

TEST(NsmDeviceBranch3, UpdateCommandMatrix_InvalidMsgType_ReturnsEarly)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "58", 0);
    bitfield8_t cmds[4] = {};
    cmds[0].byte = 0xFF;
    // messageType >= NUM_NSM_TYPES → early return, matrix unchanged
    EXPECT_NO_THROW(dev.NsmDevice::updateMessageTypesToCommandCodeMatrix(
        static_cast<uint8_t>(NUM_NSM_TYPES), cmds, 4));
    // Matrix not set — all should remain false
    for (int i = 0; i < 32; i++)
    {
        EXPECT_FALSE(dev.messageTypesToCommandCodeMatrix[0][i]);
    }
}

TEST(NsmDeviceBranch3, UpdateCommandMatrix_ValidMsgType_SetsBits)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "59", 0);
    bitfield8_t cmds[4] = {};
    cmds[0].byte = 0b00000101; // commands 0 and 2 supported
    dev.NsmDevice::updateMessageTypesToCommandCodeMatrix(0, cmds, 4);
    EXPECT_TRUE(dev.messageTypesToCommandCodeMatrix[0][0]);
    EXPECT_FALSE(dev.messageTypesToCommandCodeMatrix[0][1]);
    EXPECT_TRUE(dev.messageTypesToCommandCodeMatrix[0][2]);
    EXPECT_FALSE(dev.messageTypesToCommandCodeMatrix[0][3]);
}

TEST(NsmDeviceBranch3, UpdateCommandMatrix_SupportedCommandsSizeLargerThanMax)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "60", 0);
    // supportedCommandsSize * 8 > NUM_COMMAND_CODES → uses min
    // Allocate more than NUM_COMMAND_CODES/8 entries → capped
    const size_t bigSize = NUM_COMMAND_CODES / 8 + 10;
    std::vector<bitfield8_t> cmds(bigSize);
    cmds[0].byte = 0b00000001; // command 0 supported
    EXPECT_NO_THROW(dev.NsmDevice::updateMessageTypesToCommandCodeMatrix(
        0, cmds.data(), bigSize));
    EXPECT_TRUE(dev.messageTypesToCommandCodeMatrix[0][0]);
}

// ============================================================================
// NsmDevice::setEventMode
// ============================================================================

TEST(NsmDeviceBranch3, SetEventMode_ValidMode_SetsMode)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "61", 0);
    dev.eventMode = 0;
    dev.NsmDevice::setEventMode(GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
    EXPECT_EQ(dev.eventMode, GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
}

TEST(NsmDeviceBranch3, SetEventMode_InvalidMode_NoChange)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "62", 0);
    dev.eventMode = 0;
    // mode > GLOBAL_EVENT_GENERATION_ENABLE_PUSH → rejected
    dev.NsmDevice::setEventMode(
        static_cast<uint8_t>(GLOBAL_EVENT_GENERATION_ENABLE_PUSH + 1));
    EXPECT_EQ(dev.eventMode, 0); // unchanged
}

TEST(NsmDeviceBranch3, SetEventMode_ZeroMode_Accepted)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "63", 0);
    dev.eventMode = 1;
    dev.NsmDevice::setEventMode(0);
    EXPECT_EQ(dev.eventMode, 0);
}

// ============================================================================
// NsmDevice::addSensorBase — GpuPerformanceMonitoring, LongRunning, invalid
// ============================================================================

TEST(NsmDeviceBranch3, AddSensorBase_GpuPerformanceMonitoring_SetsGPMLimit)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "64", 0);
    dev.eid = 64;
    auto sensor = std::make_shared<MockSensor>("gpm_sensor", "test");
    dev.NsmDevice::addSensorBase(sensor, PollingType::GpuPerformanceMonitoring);
    EXPECT_EQ(sensor->refreshLimitInUsec, GPM_REFRESH_LIMIT_IN_USEC);
    EXPECT_FALSE(dev.deviceSensors.empty());
}

TEST(NsmDeviceBranch3, AddSensorBase_LongRunning_SetsLongRunningLimit)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "65", 0);
    dev.eid = 65;
    auto sensor = std::make_shared<MockSensor>("lr_sensor", "test");
    dev.NsmDevice::addSensorBase(sensor, PollingType::LongRunning);
    EXPECT_EQ(sensor->refreshLimitInUsec, LONG_RUNNING_REFRESH_LIMIT_IN_USEC);
}

TEST(NsmDeviceBranch3, AddSensorBase_InvalidPollingType_Throws)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "66", 0);
    dev.eid = 66;
    auto sensor = std::make_shared<MockSensor>("bad_sensor", "test");
    EXPECT_THROW(
        dev.NsmDevice::addSensorBase(sensor, static_cast<PollingType>(0xFF)),
        std::runtime_error);
}

// ============================================================================
// NsmDevice::getLastEventSubscription* getters
// ============================================================================

TEST(NsmDeviceBranch3, GetLastEventSubscriptionStatus_ReturnsOptional)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "67", 0);
    dev.lastEventSubscriptionStatus = std::string("OK:test");
    auto status = dev.NsmDevice::getLastEventSubscriptionStatus();
    ASSERT_TRUE(status.has_value());
    EXPECT_EQ(status.value(), "OK:test");
}

TEST(NsmDeviceBranch3, GetLastEventSubscriptionRequest_ReturnsOptional)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "68", 0);
    std::vector<uint8_t> req = {0xAA, 0xBB};
    dev.lastEventSubscriptionRequest = req;
    auto result = dev.NsmDevice::getLastEventSubscriptionRequest();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), req);
}

TEST(NsmDeviceBranch3, GetLastEventSubscriptionResponse_ReturnsOptional)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "69", 0);
    std::vector<uint8_t> resp = {0xCC, 0xDD};
    dev.lastEventSubscriptionResponse = resp;
    auto result = dev.NsmDevice::getLastEventSubscriptionResponse();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), resp);
}

// ============================================================================
// NsmDevice::setHasNsmEventSettingConfig / hasNsmEventSettingConfig
// ============================================================================

TEST(NsmDeviceBranch3, SetHasNsmEventSettingConfig_TrueAndFalse)
{
    MockNsmDevice dev(0, 0, "MCTP_EID", "70", 0);
    dev.NsmDevice::setHasNsmEventSettingConfig(true);
    EXPECT_TRUE(dev.NsmDevice::hasNsmEventSettingConfig());
    dev.NsmDevice::setHasNsmEventSettingConfig(false);
    EXPECT_FALSE(dev.NsmDevice::hasNsmEventSettingConfig());
}
