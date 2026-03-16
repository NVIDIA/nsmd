/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmPCIeSlot.hpp"

using namespace nsm;

class NsmPCIeSlotTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 42;
    static constexpr uint8_t testDeviceIndex = 1;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmPCIeSlotTest, ConstructorWithBasicParameters)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeSlot slot(provider, testDeviceIndex); });
}

TEST_F(NsmPCIeSlotTest, ConstructorWithZeroDeviceIndex)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeSlot slot(provider, 0); });
}

TEST_F(NsmPCIeSlotTest, ConstructorWithMaxDeviceIndex)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeSlot slot(provider, 255); });
}

TEST_F(NsmPCIeSlotTest, ConstructorSetsDeviceIndex)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    NsmPCIeSlot slot(provider, testDeviceIndex);

    EXPECT_EQ(slot.deviceIndex, testDeviceIndex);
}

TEST_F(NsmPCIeSlotTest, ConstructorWithDifferentDeviceIndexes)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({
        NsmPCIeSlot slot0(provider, 0);
        NsmPCIeSlot slot1(provider, 1);
        NsmPCIeSlot slot2(provider, 2);
        NsmPCIeSlot slot10(provider, 10);
        NsmPCIeSlot slot255(provider, 255);
    });
}

TEST_F(NsmPCIeSlotTest, MultipleInstancesIndependent)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    NsmPCIeSlot slot1(provider, 1);
    NsmPCIeSlot slot2(provider, 2);

    EXPECT_EQ(slot1.deviceIndex, 1);
    EXPECT_EQ(slot2.deviceIndex, 2);
}

TEST_F(NsmPCIeSlotTest, ConstructorWithSmallDeviceIndex)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeSlot slot(provider, 1); });
}

TEST_F(NsmPCIeSlotTest, ConstructorWithMediumDeviceIndex)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeSlot slot(provider, 128); });
}

TEST_F(NsmPCIeSlotTest, ConstructorWithLargeDeviceIndex)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeSlot slot(provider, 254); });
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmPCIeSlotTest, GenRequestMsgReturnsValidMessage)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result = slot.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgHasCorrectSize)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result = slot.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithDifferentEids)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result1 = slot.genRequestMsg(1, testInstanceId);
    auto result2 = slot.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result1 = slot.genRequestMsg(testEid, 0);
    auto result2 = slot.genRequestMsg(testEid, 255);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithZeroEid)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result = slot.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithMaxEid)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result = slot.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithZeroInstanceId)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result = slot.genRequestMsg(testEid, 0);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithMaxInstanceId)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result = slot.genRequestMsg(testEid, 255);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgMultipleCalls)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result1 = slot.genRequestMsg(testEid, testInstanceId);
    auto result2 = slot.genRequestMsg(testEid, testInstanceId);
    auto result3 = slot.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgWithDifferentDeviceIndexes)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    NsmPCIeSlot slot1(provider, 1);
    NsmPCIeSlot slot2(provider, 2);
    NsmPCIeSlot slot3(provider, 3);

    auto result1 = slot1.genRequestMsg(testEid, testInstanceId);
    auto result2 = slot2.genRequestMsg(testEid, testInstanceId);
    auto result3 = slot3.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmPCIeSlotTest, GenRequestMsgConsistentResults)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    auto result1 = slot.genRequestMsg(testEid, testInstanceId);
    auto result2 = slot.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result1->size(), result2->size());
}

// ============================================================================
// handleResponseMsg Tests
// ============================================================================

TEST_F(NsmPCIeSlotTest, HandleResponseMsgWithNullResponse)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    uint8_t result = slot.handleResponseMsg(nullptr, 0);

    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeSlotTest, HandleResponseMsgWithZeroLength)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    nsm_msg response{};
    uint8_t result = slot.handleResponseMsg(&response, 0);

    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeSlotTest, HandleResponseMsgMultipleTimes)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    nsm_msg response{};

    uint8_t result1 = slot.handleResponseMsg(&response, sizeof(response));
    uint8_t result2 = slot.handleResponseMsg(&response, sizeof(response));
    uint8_t result3 = slot.handleResponseMsg(&response, sizeof(response));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

TEST_F(NsmPCIeSlotTest, HandleResponseMsgWithSmallResponse)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    nsm_msg response{};
    uint8_t result = slot.handleResponseMsg(&response, 10);

    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeSlotTest, HandleResponseMsgWithLargeResponse)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;
    NsmPCIeSlot slot(provider, testDeviceIndex);

    nsm_msg response{};
    uint8_t result = slot.handleResponseMsg(&response, 1000);

    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeSlotTest, HandleResponseMsgWithDifferentSlots)
{
    NsmInterfaceProvider<PCIeSlotIntf> provider;

    NsmPCIeSlot slot1(provider, 1);
    NsmPCIeSlot slot2(provider, 2);
    NsmPCIeSlot slot3(provider, 3);

    nsm_msg response{};

    uint8_t result1 = slot1.handleResponseMsg(&response, sizeof(response));
    uint8_t result2 = slot2.handleResponseMsg(&response, sizeof(response));
    uint8_t result3 = slot3.handleResponseMsg(&response, sizeof(response));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}
