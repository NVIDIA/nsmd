/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "platform-environmental.h"

#include "nsmNumericSensor/nsmVoltageAggregator.hpp"

#include <gtest/gtest.h>

using namespace nsm;

class NsmVoltageAggregatorTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 1;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmVoltageAggregatorTest, ConstructorWithBasicParameters)
{
    EXPECT_NO_THROW({
        NsmVoltageAggregator aggregator("VoltageSensor", "Voltage", false);
    });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithPriorityTrue)
{
    EXPECT_NO_THROW(
        { NsmVoltageAggregator aggregator("Voltage", "Voltage", true); });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithPriorityFalse)
{
    EXPECT_NO_THROW(
        { NsmVoltageAggregator aggregator("Voltage", "Voltage", false); });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithEmptyName)
{
    EXPECT_NO_THROW({ NsmVoltageAggregator aggregator("", "Voltage", false); });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithEmptyType)
{
    EXPECT_NO_THROW({ NsmVoltageAggregator aggregator("Voltage", "", false); });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithLongName)
{
    std::string longName(100, 'V');
    EXPECT_NO_THROW(
        { NsmVoltageAggregator aggregator(longName, "Voltage", false); });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithLongType)
{
    std::string longType(100, 'T');
    EXPECT_NO_THROW(
        { NsmVoltageAggregator aggregator("Voltage", longType, false); });
}

TEST_F(NsmVoltageAggregatorTest, ConstructorWithSpecialCharacters)
{
    EXPECT_NO_THROW({
        NsmVoltageAggregator aggregator("Voltage-Sensor_123", "Type-123", true);
    });
}

TEST_F(NsmVoltageAggregatorTest, MultipleInstancesIndependent)
{
    NsmVoltageAggregator agg1("Voltage1", "Voltage", true);
    NsmVoltageAggregator agg2("Voltage2", "Voltage", false);

    SUCCEED();
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgReturnsValidMessage)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgHasCorrectSize)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgWithDifferentEids)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result1 = aggregator.genRequestMsg(1, testInstanceId);
    auto result2 = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result1 = aggregator.genRequestMsg(testEid, 0);
    auto result2 = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgWithZeroEid)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgWithMaxEid)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgWithZeroInstanceId)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(testEid, 0);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgWithMaxInstanceId)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgMultipleCalls)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result1 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result2 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result3 = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgPriorityTrue)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", true);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgPriorityFalse)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmVoltageAggregatorTest, GenRequestMsgConsistentResults)
{
    NsmVoltageAggregator aggregator("TestVoltage", "Voltage", false);

    auto result1 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result2 = aggregator.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1->size(), result2->size());
}
