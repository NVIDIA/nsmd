/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "platform-environmental.h"

#include "nsmNumericSensor/nsmPowerAggregator.hpp"

#include <gtest/gtest.h>

using namespace nsm;

class NsmPowerAggregatorTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 1;
    static constexpr uint8_t testAveragingInterval = 5;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmPowerAggregatorTest, ConstructorWithBasicParameters)
{
    EXPECT_NO_THROW(
        { NsmPowerAggregator aggregator("PowerSensor", "Power", false, 10); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithPriorityTrue)
{
    EXPECT_NO_THROW(
        { NsmPowerAggregator aggregator("Power", "Power", true, 5); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithPriorityFalse)
{
    EXPECT_NO_THROW(
        { NsmPowerAggregator aggregator("Power", "Power", false, 5); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithZeroAveragingInterval)
{
    EXPECT_NO_THROW(
        { NsmPowerAggregator aggregator("Power", "Power", false, 0); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithMaxAveragingInterval)
{
    EXPECT_NO_THROW(
        { NsmPowerAggregator aggregator("Power", "Power", false, 255); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithEmptyName)
{
    EXPECT_NO_THROW({ NsmPowerAggregator aggregator("", "Power", false, 5); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithEmptyType)
{
    EXPECT_NO_THROW({ NsmPowerAggregator aggregator("Power", "", false, 5); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithLongName)
{
    std::string longName(100, 'P');
    EXPECT_NO_THROW(
        { NsmPowerAggregator aggregator(longName, "Power", false, 5); });
}

TEST_F(NsmPowerAggregatorTest, ConstructorWithDifferentAveragingIntervals)
{
    EXPECT_NO_THROW({
        NsmPowerAggregator agg1("Power1", "Power", false, 1);
        NsmPowerAggregator agg2("Power2", "Power", false, 10);
        NsmPowerAggregator agg3("Power3", "Power", false, 100);
    });
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmPowerAggregatorTest, GenRequestMsgReturnsValidMessage)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgHasCorrectSize)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_req));
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgWithDifferentEids)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result1 = aggregator.genRequestMsg(1, testInstanceId);
    auto result2 = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result1 = aggregator.genRequestMsg(testEid, 0);
    auto result2 = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgWithZeroEid)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result = aggregator.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgWithMaxEid)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgMultipleCalls)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false,
                                  testAveragingInterval);

    auto result1 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result2 = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgWithZeroAveragingInterval)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false, 0);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgWithMaxAveragingInterval)
{
    NsmPowerAggregator aggregator("TestPower", "Power", false, 255);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPowerAggregatorTest, GenRequestMsgPriorityVariations)
{
    NsmPowerAggregator agg1("Power1", "Power", true, 5);
    NsmPowerAggregator agg2("Power2", "Power", false, 5);

    auto result1 = agg1.genRequestMsg(testEid, testInstanceId);
    auto result2 = agg2.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}
