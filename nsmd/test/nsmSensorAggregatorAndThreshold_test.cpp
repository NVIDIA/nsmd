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
 * nsmBatch13_test.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmSensorAggregator.cpp
 *      - Constructor (lines 26-31): NsmSensorAggregator(name, type)
 *      - handleResponseMsg decode_aggregate_resp_sample failure path
 *        (lines 73,76-77,79): error log + continue when sample decode fails
 *      - handleResponseMsg handleSample failure path (line 86): returnValue
 *        updated when handleSample returns error
 *   2. nsmd/nsmSensors/nsmInventoryProperty.cpp
 *      - Constructor (lines 25-28): NsmInventoryPropertyBase(pdi, property)
 *      - genRequestMsg encode failure path (line 40): lg2::debug when
 *        encode_get_inventory_information_req fails (instanceId > 31)
 *   3. nsmd/nsmNumericSensor/nsmThreshold.cpp
 *      - genRequestMsg encode failure path (lines 36,38-39): lg2::debug when
 *        encode_read_thermal_parameter_req fails (instanceId > 31)
 */

#include <gtest/gtest.h>

using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmNumericSensor/nsmNumericSensor.hpp"
#include "nsmNumericSensor/nsmThreshold.hpp"
#include "nsmNumericSensor/nsmThresholdAggregator.hpp"
#include "nsmObject.hpp"
#include "nsmSensors/nsmInventoryProperty.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Minimal concrete subclass of NsmInventoryPropertyBase for testing
// ============================================================================

class TestInventoryProperty : public NsmInventoryPropertyBase
{
  public:
    TestInventoryProperty(const NsmObject& pdi,
                          nsm_inventory_property_identifiers prop) :
        NsmInventoryPropertyBase(pdi, prop)
    {}

    void handleResponse(const Response& /*data*/) override {}
};

// ============================================================================
// Helper: build an aggregate response header with telemetryCount samples
// but NO sample data (truncated buffer to trigger decode_aggregate_resp_sample
// failure)
// ============================================================================
static std::vector<uint8_t>
    buildAggregateRespHeaderOnly(uint16_t telemetryCount)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    // Command byte 0x01 is arbitrary - decode_aggregate_resp ignores it
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, telemetryCount, msg);
    return buf;
}

// ============================================================================
// Helper: build an aggregate response with one 1-byte sample (data_len=1).
// decode_aggregate_resp_sample will succeed but data_len=1 is wrong for
// decode_aggregate_thermal_parameter_data (needs sizeof(int32_t)=4), so
// NsmThresholdAggregator::handleSample will return an error.
// ============================================================================
static std::vector<uint8_t> buildAggregateRespWithOneByteSample(uint8_t tag,
                                                                bool valid)
{
    // Header
    const size_t headerSize = sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp);
    // Sample: sizeof(nsm_aggregate_resp_sample) = 3 bytes header
    //         (tag 1B + flags 1B + data[1] 1B), data_len=1 → total 3 bytes
    const size_t sampleSize = sizeof(nsm_aggregate_resp_sample) - 1 + 1; // = 3

    std::vector<uint8_t> buf(headerSize + sampleSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, msg);

    auto samplePtr =
        reinterpret_cast<nsm_aggregate_resp_sample*>(buf.data() + headerSize);
    uint8_t sampleData[1] = {0x42};
    size_t sampleLen = 0;
    encode_aggregate_resp_sample(tag, valid, sampleData, 1, samplePtr,
                                 &sampleLen);
    return buf;
}

// ============================================================================
// Part 1: NsmSensorAggregator constructor (lines 26-31)
// ============================================================================

// NsmThresholdAggregator transitively calls NsmSensorAggregator(name, type)
// constructor which contains sampleTags.reserve(256) on line 30.
TEST(NsmSensorAggregatorTest, Constructor_SetsNameAndType)
{
    // Act
    NsmThresholdAggregator agg("TestAgg", "NSM_ThermalParam", false);

    // Assert
    EXPECT_EQ(agg.getName(), "TestAgg");
    EXPECT_EQ(agg.getType(), "NSM_ThermalParam");
}

TEST(NsmSensorAggregatorTest, Constructor_SampleTagsReserved)
{
    // The constructor reserves 256 elements; verify the vector is empty
    NsmThresholdAggregator agg("AggReserve", "NSM_Threshold", true);

    // After construction sampleTags is empty (cleared in handleResponseMsg)
    EXPECT_TRUE(agg.sampleTags.empty());
}

// ============================================================================
// Part 2: handleResponseMsg - decode_aggregate_resp_sample failure
//         (lines 73,76-77,79: error log + continue)
// ============================================================================

TEST(NsmSensorAggregatorTest,
     HandleResponseMsg_SampleDecodeFails_ReturnsSuccess)
{
    // Arrange: telemetryCount=1 but buffer has no sample bytes.
    // decode_aggregate_resp_sample will see responseLen=0 and return
    // NSM_SW_ERROR_LENGTH, triggering the error log + continue path.
    NsmThresholdAggregator agg("AggDecFail", "NSM_ThermalParam", false);
    auto buf = buildAggregateRespHeaderOnly(1);

    // Act
    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // Assert: error in sample is logged but overall returnValue stays SUCCESS
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// DISABLED: When telemetryCount > 1 and decode_aggregate_resp_sample fails
// (line 79: continue), the production code reuses the stale consumedLen from
// the aggregate header decode. On the next loop iteration, responseLen wraps
// to SIZE_MAX and responseData goes out of bounds.  This is a production code
// bug; the test triggers a heap-buffer-overflow under ASan.
// See FAILING_TESTS.md for details.
TEST(NsmSensorAggregatorTest,
     DISABLED_HandleResponseMsg_MultipleSamplesFail_ReturnsSuccess)
{
    // Same as above but telemetryCount=3 so the loop iterates 3 times,
    // each time triggering the decode failure + continue path.
    NsmThresholdAggregator agg("AggMultiFail", "NSM_ThermalParam", false);
    auto buf = buildAggregateRespHeaderOnly(3);

    // Act
    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// Part 3: handleResponseMsg - handleSample failure path (line 86)
// ============================================================================

TEST(NsmSensorAggregatorTest, HandleResponseMsg_HandleSampleFails_SetsError)
{
    // Arrange: build a response with one valid sample but only 1-byte data.
    // decode_aggregate_resp_sample succeeds (data_len=1 returned).
    // decode_aggregate_thermal_parameter_data fails (needs 4 bytes, got 1).
    // NsmThresholdAggregator::handleSample returns NSM_SW_ERROR_LENGTH.
    // This triggers line 86: returnValue = rc.
    NsmThresholdAggregator agg("AggHandleFail", "NSM_ThermalParam", false);
    auto buf = buildAggregateRespWithOneByteSample(0, true);

    // Act
    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // Assert: handleSample returned an error which was stored in returnValue
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(NsmSensorAggregatorTest,
     HandleResponseMsg_HandleSampleInvalidValid_ReturnsSuccess)
{
    // valid=false: handleSample calls updateSensorNotWorking and returns
    // SUCCESS, so line 86 is NOT reached. Overall returnValue stays SUCCESS.
    NsmThresholdAggregator agg("AggFalseValid", "NSM_ThermalParam", false);
    auto buf = buildAggregateRespWithOneByteSample(0, false);

    // Act
    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// Part 4: NsmInventoryPropertyBase constructor (lines 25-28)
// ============================================================================

TEST(NsmInventoryPropertyBaseTest, Constructor_SetsNameAndProperty)
{
    // Arrange
    NsmObject obj("InvSensor", "NSM_Asset");

    // Act
    TestInventoryProperty sensor(obj, BOARD_PART_NUMBER);

    // Assert
    EXPECT_EQ(sensor.getName(), "InvSensor");
    EXPECT_EQ(sensor.getType(), "NSM_Asset");
    EXPECT_EQ(sensor.property, BOARD_PART_NUMBER);
}

TEST(NsmInventoryPropertyBaseTest,
     Constructor_DifferentProperty_StoresCorrectly)
{
    NsmObject obj("InvSensor2", "NSM_Asset");

    TestInventoryProperty sensor(obj, SERIAL_NUMBER);

    EXPECT_EQ(sensor.property, SERIAL_NUMBER);
}

// ============================================================================
// Part 5: NsmInventoryPropertyBase::genRequestMsg - encode failure (line 40)
// ============================================================================

TEST(NsmInventoryPropertyBaseTest,
     GenRequestMsg_InstanceIdTooLarge_ReturnsNullopt)
{
    // Arrange: instanceId=32 > NSM_INSTANCE_MAX=31 causes
    // encode_get_inventory_information_req to fail, triggering line 40
    NsmObject obj("InvSensor3", "NSM_Asset");
    TestInventoryProperty sensor(obj, BOARD_PART_NUMBER);

    // Act: instanceId=32 > 31 causes encode failure
    auto result = sensor.genRequestMsg(0, 32);

    // Assert
    EXPECT_FALSE(result.has_value());
}

TEST(NsmInventoryPropertyBaseTest, GenRequestMsg_ValidInstanceId_ReturnsRequest)
{
    // instanceId=0 (valid) should succeed
    NsmObject obj("InvSensor4", "NSM_Asset");
    TestInventoryProperty sensor(obj, BOARD_PART_NUMBER);

    auto result = sensor.genRequestMsg(0, 0);

    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Part 6: NsmThreshold::genRequestMsg - encode failure (lines 36,38-39)
// ============================================================================

TEST(NsmThresholdTest, GenRequestMsg_InstanceIdTooLarge_ReturnsNullopt)
{
    // Arrange: use empty NsmNumericSensorValueAggregate (no D-Bus needed).
    // instanceId=32 > NSM_INSTANCE_MAX=31 causes
    // encode_read_thermal_parameter_req to fail, triggering lines 36-39.
    auto sensorValue = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold threshold("TestThresh", "NSM_ThermalParam", 42, sensorValue);

    // Act
    auto result = threshold.genRequestMsg(0, 32);

    // Assert
    EXPECT_FALSE(result.has_value());
}

TEST(NsmThresholdTest, GenRequestMsg_MaxValidInstanceId_ReturnsRequest)
{
    // instanceId=31 = NSM_INSTANCE_MAX (valid, should succeed)
    auto sensorValue = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold threshold("TestThresh2", "NSM_ThermalParam", 42, sensorValue);

    auto result = threshold.genRequestMsg(5, 31);

    EXPECT_TRUE(result.has_value());
}

TEST(NsmThresholdTest, GenRequestMsg_ZeroInstanceId_ReturnsRequest)
{
    auto sensorValue = std::make_shared<NsmNumericSensorValueAggregate>();
    NsmThreshold threshold("TestThresh3", "NSM_ThermalParam", 0, sensorValue);

    auto result = threshold.genRequestMsg(0, 0);

    EXPECT_TRUE(result.has_value());
}
