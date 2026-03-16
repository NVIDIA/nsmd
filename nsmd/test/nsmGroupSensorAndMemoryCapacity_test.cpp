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
 * nsmBatch14_test.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmGroupSensor.hpp line 57:
 *      - break in handleResponseMsg when a subsensor's handleResponse
 *        returns non-SUCCESS (genuinely uncovered branch)
 *   2. nsmd/nsmCommon/nsmCommon.cpp lines 56,59,60:
 *      - lg2::error + return std::nullopt in
 *        NsmMemoryCapacity::genRequestMsg when
 * encode_get_inventory_information_req fails (instanceId=32 >
 * NSM_INSTANCE_MAX=31)
 */

#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmCommon/nsmCommon.hpp"
#include "nsmGroupSensor.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Minimal concrete subclass of NsmGroupSensor for testing.
// NsmGroupSensor is abstract because NsmSensor::genRequestMsg and
// NsmSubSensor::handleResponse are pure virtual.
// ============================================================================

class ConcreteGroupSensor : public NsmGroupSensor
{
  public:
    using NsmGroupSensor::NsmGroupSensor;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::vector<uint8_t>{};
    }

    uint8_t handleResponse(const nsm_msg* /*responseMsg*/,
                           size_t /*responseLen*/) override
    {
        return selfResult;
    }

    // Controls what the group sensor's own handleResponse returns
    uint8_t selfResult = NSM_SW_SUCCESS;
};

// ============================================================================
// Minimal subsensor implementations
// ============================================================================

class FailingSubSensor : public NsmSubSensor
{
  public:
    uint8_t handleResponse(const nsm_msg* /*responseMsg*/,
                           size_t /*responseLen*/) override
    {
        return NSM_SW_ERROR;
    }
};

class SuccessSubSensor : public NsmSubSensor
{
  public:
    uint8_t handleResponse(const nsm_msg* /*responseMsg*/,
                           size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// Helper: build a minimal nsm_msg buffer (implementations ignore msg contents)
static std::vector<uint8_t> makeMinimalMsgBuf()
{
    return std::vector<uint8_t>(sizeof(nsm_msg_hdr), 0);
}

// ============================================================================
// Part 1: NsmGroupSensor::handleResponseMsg
// ============================================================================

// Line 47-51: group sensor's own handleResponse fails → early return
TEST(NsmGroupSensorTest, HandleResponseMsg_SelfFails_ReturnsErrorEarly)
{
    // Arrange
    ConcreteGroupSensor sensor("GroupSelfFail", "NSM_Group");
    sensor.selfResult = NSM_SW_ERROR;
    sensor.sensors.push_back(std::make_shared<SuccessSubSensor>());

    auto buf = makeMinimalMsgBuf();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Act
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());

    // Assert: returned before iterating subsensors
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Line 54-58: first subsensor fails → break (LINE 57 - the coverage target)
TEST(NsmGroupSensorTest, HandleResponseMsg_SubsensorFails_BreaksLoop)
{
    // Arrange: group sensor passes, first subsensor fails
    ConcreteGroupSensor sensor("GroupSubFail", "NSM_Group");
    sensor.selfResult = NSM_SW_SUCCESS;
    sensor.sensors.push_back(std::make_shared<FailingSubSensor>());

    auto buf = makeMinimalMsgBuf();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Act
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());

    // Assert: break taken on subsensor failure, returns subsensor error
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// First subsensor fails → break stops second subsensor from running
TEST(NsmGroupSensorTest, HandleResponseMsg_FirstSubsensorFails_SecondNotInvoked)
{
    // Arrange: second subsensor is Success; if loop didn't break, rc would be
    // SUCCESS after second subsensor, but break means rc stays as first error
    ConcreteGroupSensor sensor("GroupBreak2", "NSM_Group");
    sensor.selfResult = NSM_SW_SUCCESS;
    sensor.sensors.push_back(std::make_shared<FailingSubSensor>());
    sensor.sensors.push_back(std::make_shared<SuccessSubSensor>());

    auto buf = makeMinimalMsgBuf();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Act
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());

    // Assert: break occurred after first failing subsensor
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// All subsensors succeed → no break, returns SUCCESS
TEST(NsmGroupSensorTest, HandleResponseMsg_AllSubsensorsPass_ReturnsSuccess)
{
    // Arrange
    ConcreteGroupSensor sensor("GroupAllPass", "NSM_Group");
    sensor.selfResult = NSM_SW_SUCCESS;
    sensor.sensors.push_back(std::make_shared<SuccessSubSensor>());
    sensor.sensors.push_back(std::make_shared<SuccessSubSensor>());

    auto buf = makeMinimalMsgBuf();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Act
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// No subsensors → loop never executes → returns SUCCESS
TEST(NsmGroupSensorTest, HandleResponseMsg_NoSubsensors_ReturnsSuccess)
{
    // Arrange
    ConcreteGroupSensor sensor("GroupNoSub", "NSM_Group");
    sensor.selfResult = NSM_SW_SUCCESS;

    auto buf = makeMinimalMsgBuf();
    auto* msg = reinterpret_cast<const nsm_msg*>(buf.data());

    // Act
    uint8_t rc = sensor.handleResponseMsg(msg, buf.size());

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// Part 2: NsmMemoryCapacity::genRequestMsg error path
//         Lines 56,59,60 in nsmCommon.cpp
// ============================================================================

// instanceId=32 > NSM_INSTANCE_MAX=31 → encode fails → lines 56,59,60 hit
TEST(NsmTotalMemoryTest, GenRequestMsg_InstanceIdTooLarge_ReturnsNullopt)
{
    // Arrange: NsmTotalMemory has no D-Bus dependency in its constructor
    NsmTotalMemory sensor("TotalMem", "NSM_Memory");

    // Act: instanceId=32 triggers encode_get_inventory_information_req failure
    auto result = sensor.genRequestMsg(0, 32);

    // Assert: error path returns std::nullopt (lines 56,59,60)
    EXPECT_FALSE(result.has_value());
}

// instanceId=0 (valid) → encode succeeds → success path
TEST(NsmTotalMemoryTest, GenRequestMsg_ValidInstanceId_ReturnsRequest)
{
    NsmTotalMemory sensor("TotalMem2", "NSM_Memory");

    auto result = sensor.genRequestMsg(0, 0);

    EXPECT_TRUE(result.has_value());
}

// instanceId=31 = NSM_INSTANCE_MAX → boundary: encode succeeds
TEST(NsmTotalMemoryTest, GenRequestMsg_MaxValidInstanceId_ReturnsRequest)
{
    NsmTotalMemory sensor("TotalMem3", "NSM_Memory");

    auto result = sensor.genRequestMsg(5, 31);

    EXPECT_TRUE(result.has_value());
}
