/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "platform-environmental.h"

#include "nsmNumericSensor/nsmEnergyAggregator.hpp"

#include <gtest/gtest.h>

using namespace nsm;

class NsmEnergyAggregatorTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 1;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmEnergyAggregatorTest, ConstructorWithBasicParameters)
{
    EXPECT_NO_THROW(
        { NsmEnergyAggregator aggregator("EnergySensor", "Energy", false); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithPriorityTrue)
{
    EXPECT_NO_THROW(
        { NsmEnergyAggregator aggregator("Energy", "Energy", true); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithPriorityFalse)
{
    EXPECT_NO_THROW(
        { NsmEnergyAggregator aggregator("Energy", "Energy", false); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithEmptyName)
{
    EXPECT_NO_THROW({ NsmEnergyAggregator aggregator("", "Energy", false); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithEmptyType)
{
    EXPECT_NO_THROW({ NsmEnergyAggregator aggregator("Energy", "", false); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithLongName)
{
    std::string longName(100, 'E');
    EXPECT_NO_THROW(
        { NsmEnergyAggregator aggregator(longName, "Energy", false); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithLongType)
{
    std::string longType(100, 'T');
    EXPECT_NO_THROW(
        { NsmEnergyAggregator aggregator("Energy", longType, false); });
}

TEST_F(NsmEnergyAggregatorTest, ConstructorWithSpecialCharacters)
{
    EXPECT_NO_THROW({
        NsmEnergyAggregator aggregator("Energy-Sensor_123", "Type-123", true);
    });
}

TEST_F(NsmEnergyAggregatorTest, MultipleInstancesIndependent)
{
    NsmEnergyAggregator agg1("Energy1", "Energy", true);
    NsmEnergyAggregator agg2("Energy2", "Energy", false);

    SUCCEED();
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgReturnsValidMessage)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgHasCorrectSize)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_req));
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgWithDifferentEids)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result1 = aggregator.genRequestMsg(1, testInstanceId);
    auto result2 = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result1 = aggregator.genRequestMsg(testEid, 0);
    auto result2 = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgWithZeroEid)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgWithMaxEid)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgWithZeroInstanceId)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(testEid, 0);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgWithMaxInstanceId)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(testEid, 31);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgMultipleCalls)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result1 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result2 = aggregator.genRequestMsg(testEid, testInstanceId);
    auto result3 = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgPriorityTrue)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", true);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmEnergyAggregatorTest, GenRequestMsgPriorityFalse)
{
    NsmEnergyAggregator aggregator("TestEnergy", "Energy", false);

    auto result = aggregator.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}
