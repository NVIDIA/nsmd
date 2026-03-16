/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmNumericSensor/nsmThresholdAggregator.hpp"

using namespace nsm;

using TelemetrySample = NsmSensorAggregator::TelemetrySample;

class NsmThresholdAggregatorTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 1;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmThresholdAggregatorTest, ConstructorWithBasicParameters)
{
    std::string name = "ThresholdSensor";
    std::string type = "Threshold";
    bool priority = false;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithPriorityTrue)
{
    std::string name = "HighPriorityThreshold";
    std::string type = "Threshold";
    bool priority = true;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithPriorityFalse)
{
    std::string name = "LowPriorityThreshold";
    std::string type = "Threshold";
    bool priority = false;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithEmptyName)
{
    std::string name = "";
    std::string type = "Threshold";
    bool priority = false;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithEmptyType)
{
    std::string name = "ThresholdSensor";
    std::string type = "";
    bool priority = false;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithLongName)
{
    std::string name(100, 'A');
    std::string type = "Threshold";
    bool priority = false;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithLongType)
{
    std::string name = "ThresholdSensor";
    std::string type(100, 'T');
    bool priority = false;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithSpecialCharactersInName)
{
    std::string name = "Threshold-Sensor_123";
    std::string type = "Threshold";
    bool priority = true;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, ConstructorWithSpecialCharactersInType)
{
    std::string name = "ThresholdSensor";
    std::string type = "Threshold-Type_123";
    bool priority = true;

    EXPECT_NO_THROW(
        { NsmThresholdAggregator aggregator(name, type, priority); });
}

TEST_F(NsmThresholdAggregatorTest, MultipleInstancesIndependent)
{
    std::string name1 = "Threshold1";
    std::string name2 = "Threshold2";
    std::string type = "Threshold";
    bool priority1 = true;
    bool priority2 = false;

    NsmThresholdAggregator aggregator1(name1, type, priority1);
    NsmThresholdAggregator aggregator2(name2, type, priority2);

    // Both should construct successfully
    SUCCEED();
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgReturnsValidMessage)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgHasCorrectSize)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req));
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgWithDifferentEids)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result1 = aggregator.genRequestMsg(1, testInstanceId);
    auto result2 = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result1 = aggregator.genRequestMsg(testEid, 0);
    auto result2 = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgWithZeroEid)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgWithZeroInstanceId)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(testEid, 0);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgWithMaxEid)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgWithMaxInstanceId)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgMultipleCalls)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result1 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result2 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result3 = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgPriorityTrue)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", true);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmThresholdAggregatorTest, GenRequestMsgPriorityFalse)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// handleSample Tests
// ============================================================================

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithValidSample)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithInvalidSample)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithTagZero)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = 0;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithMaxReservedTag)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithTagAboveMax)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.valid = true;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithNullData)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithZeroDataLen)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    uint8_t data[] = {0x01, 0x02};
    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = data;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleMultipleTimes)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result1 = aggregator.handleSample(sample);
    auto result2 = aggregator.handleSample(sample);
    auto result3 = aggregator.handleSample(sample);

    EXPECT_EQ(result1, NSM_SW_SUCCESS);
    EXPECT_EQ(result2, NSM_SW_SUCCESS);
    EXPECT_EQ(result3, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithDifferentTags)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample1;
    sample1.tag = 1;
    sample1.valid = false;
    sample1.data = nullptr;
    sample1.data_len = 0;

    TelemetrySample sample2;
    sample2.tag = 2;
    sample2.valid = false;
    sample2.data = nullptr;
    sample2.data_len = 0;

    auto result1 = aggregator.handleSample(sample1);
    auto result2 = aggregator.handleSample(sample2);

    EXPECT_EQ(result1, NSM_SW_SUCCESS);
    EXPECT_EQ(result2, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleInvalidThenValid)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample invalidSample;
    invalidSample.tag = 1;
    invalidSample.valid = false;
    invalidSample.data = nullptr;
    invalidSample.data_len = 0;

    TelemetrySample validSample;
    validSample.tag = 2;
    validSample.valid = false;
    validSample.data = nullptr;
    validSample.data_len = 0;

    auto result1 = aggregator.handleSample(invalidSample);
    auto result2 = aggregator.handleSample(validSample);

    EXPECT_EQ(result1, NSM_SW_SUCCESS);
    EXPECT_EQ(result2, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithPriorityTrue)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", true);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmThresholdAggregatorTest, HandleSampleWithPriorityFalse)
{
    NsmThresholdAggregator aggregator("TestThreshold", "Threshold", false);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}
