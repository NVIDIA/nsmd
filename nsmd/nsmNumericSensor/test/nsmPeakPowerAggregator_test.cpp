/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmNumericSensor/nsmPeakPowerAggregator.hpp"

using namespace nsm;

using TelemetrySample = NsmSensorAggregator::TelemetrySample;
static constexpr uint8_t TIMESTAMP = NsmSensorAggregator::SpecialTag::TIMESTAMP;

class NsmPeakPowerAggregatorTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 1;
    static constexpr uint8_t testAveragingInterval = 5;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithBasicParameters)
{
    std::string name = "PeakPowerSensor";
    std::string type = "PeakPower";
    bool priority = false;
    uint8_t avgInterval = 10;

    EXPECT_NO_THROW({
        NsmPeakPowerAggregator aggregator(name, type, priority, avgInterval);
    });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithPriorityTrue)
{
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator("PeakPower", "Power", true, 5); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithPriorityFalse)
{
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator("PeakPower", "Power", false, 5); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithZeroAveragingInterval)
{
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator("PeakPower", "Power", false, 0); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithMaxAveragingInterval)
{
    EXPECT_NO_THROW({
        NsmPeakPowerAggregator aggregator("PeakPower", "Power", false, 255);
    });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithEmptyName)
{
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator("", "Power", false, 5); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithEmptyType)
{
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator("PeakPower", "", false, 5); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithLongName)
{
    std::string longName(100, 'P');
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator(longName, "Power", false, 5); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithSmallAveragingInterval)
{
    EXPECT_NO_THROW(
        { NsmPeakPowerAggregator aggregator("PeakPower", "Power", false, 1); });
}

TEST_F(NsmPeakPowerAggregatorTest, ConstructorWithLargeAveragingInterval)
{
    EXPECT_NO_THROW({
        NsmPeakPowerAggregator aggregator("PeakPower", "Power", false, 200);
    });
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgReturnsValidMessage)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgHasCorrectSize)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_req));
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgWithDifferentEids)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result1 = aggregator.genRequestMsg(1, testInstanceId);
    auto result2 = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result1 = aggregator.genRequestMsg(testEid, 0);
    auto result2 = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgWithZeroEid)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result = aggregator.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgWithMaxEid)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgMultipleCalls)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    auto result1 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result2 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result3 = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgWithZeroAveragingInterval)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false, 0);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPeakPowerAggregatorTest, GenRequestMsgWithMaxAveragingInterval)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false, 255);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// handleSample Tests
// ============================================================================

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithValidSample)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithInvalidSample)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithTimestampTag)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = TIMESTAMP;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithTagZero)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = 0;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithMaxReservedTag)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithTagAboveMax)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithNullData)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample sample;
    sample.tag = 1;
    sample.valid = false;
    sample.data = nullptr;
    sample.data_len = 0;

    auto result = aggregator.handleSample(sample);

    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleMultipleTimes)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

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

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleWithDifferentTags)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

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

TEST_F(NsmPeakPowerAggregatorTest, HandleSampleTimestampThenData)
{
    NsmPeakPowerAggregator aggregator("TestPower", "Power", false,
                                      testAveragingInterval);

    TelemetrySample timestampSample;
    timestampSample.tag = TIMESTAMP;
    timestampSample.valid = false;
    timestampSample.data = nullptr;
    timestampSample.data_len = 0;

    TelemetrySample dataSample;
    dataSample.tag = 1;
    dataSample.valid = false;
    dataSample.data = nullptr;
    dataSample.data_len = 0;

    auto result1 = aggregator.handleSample(timestampSample);
    auto result2 = aggregator.handleSample(dataSample);

    EXPECT_EQ(result1, NSM_SW_SUCCESS);
    EXPECT_EQ(result2, NSM_SW_SUCCESS);
}
