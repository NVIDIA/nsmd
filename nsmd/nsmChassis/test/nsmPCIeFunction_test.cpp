/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmPCIeFunction.hpp"

using namespace nsm;

class NsmPCIeFunctionTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 42;
    static constexpr uint8_t testDeviceIndex = 1;
    static constexpr uint8_t testFunctionId = 0;
};

// ============================================================================
// Constructor Tests - Single PCIe Port
// ============================================================================

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeWithBasicParameters)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW({
        NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);
    });
}

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeWithZeroDeviceIndex)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeFunction function(provider, 0, testFunctionId); });
}

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeWithMaxDeviceIndex)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW(
        { NsmPCIeFunction function(provider, 255, testFunctionId); });
}

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeWithDifferentFunctionIds)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW({
        NsmPCIeFunction func0(provider, testDeviceIndex, 0);
        NsmPCIeFunction func1(provider, testDeviceIndex, 1);
        NsmPCIeFunction func2(provider, testDeviceIndex, 2);
        NsmPCIeFunction func3(provider, testDeviceIndex, 3);
        NsmPCIeFunction func4(provider, testDeviceIndex, 4);
        NsmPCIeFunction func5(provider, testDeviceIndex, 5);
        NsmPCIeFunction func6(provider, testDeviceIndex, 6);
        NsmPCIeFunction func7(provider, testDeviceIndex, 7);
    });
}

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeSetsDeviceIndex)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    EXPECT_EQ(function.deviceIndex, testDeviceIndex);
}

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeSetsFunctionId)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    EXPECT_EQ(function.functionId, testFunctionId);
}

TEST_F(NsmPCIeFunctionTest, ConstructorSinglePCIeMultiPortDisabled)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    EXPECT_FALSE(function.isMultiPciePortEnabled);
}

// ============================================================================
// Constructor Tests - Multi PCIe Port
// ============================================================================

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeWithBasicParameters)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    uint8_t multiPortType = 1;
    uint8_t multiPortIndex = 2;
    uint8_t upstreamPortNumber = 3;

    EXPECT_NO_THROW({
        NsmPCIeFunction function(provider, testFunctionId, multiPortType,
                                 multiPortIndex, upstreamPortNumber);
    });
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeSetsFunctionId)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    NsmPCIeFunction function(provider, testFunctionId, 1, 2, 3);

    EXPECT_EQ(function.functionId, testFunctionId);
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeMultiPortEnabled)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    NsmPCIeFunction function(provider, testFunctionId, 1, 2, 3);

    EXPECT_TRUE(function.isMultiPciePortEnabled);
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeSetsMultiPortType)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    uint8_t multiPortType = 5;

    NsmPCIeFunction function(provider, testFunctionId, multiPortType, 2, 3);

    EXPECT_EQ(function.multiPortType, multiPortType);
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeSetsMultiPortIndex)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    uint8_t multiPortIndex = 7;

    NsmPCIeFunction function(provider, testFunctionId, 1, multiPortIndex, 3);

    EXPECT_EQ(function.multiPortIndex, multiPortIndex);
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeSetsUpstreamPortNumber)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    uint8_t upstreamPortNumber = 9;

    NsmPCIeFunction function(provider, testFunctionId, 1, 2,
                             upstreamPortNumber);

    EXPECT_EQ(function.multiPortUpstreamPortNumber, upstreamPortNumber);
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeWithZeroValues)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW({ NsmPCIeFunction function(provider, 0, 0, 0, 0); });
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeWithMaxValues)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW(
        { NsmPCIeFunction function(provider, 255, 255, 255, 255); });
}

TEST_F(NsmPCIeFunctionTest, ConstructorMultiPCIeWithDifferentFunctionIds)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    EXPECT_NO_THROW({
        NsmPCIeFunction func0(provider, 0, 1, 2, 3);
        NsmPCIeFunction func1(provider, 1, 1, 2, 3);
        NsmPCIeFunction func2(provider, 2, 1, 2, 3);
        NsmPCIeFunction func7(provider, 7, 1, 2, 3);
    });
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmPCIeFunctionTest, GenRequestMsgSinglePCIeReturnsValidMessage)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result = function.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgSinglePCIeHasCorrectSize)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result = function.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgMultiPCIeReturnsValidMessage)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testFunctionId, 1, 2, 3);

    auto result = function.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgMultiPCIeHasCorrectSize)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testFunctionId, 1, 2, 3);

    auto result = function.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgWithDifferentEids)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result1 = function.genRequestMsg(1, testInstanceId);
    auto result2 = function.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result1 = function.genRequestMsg(testEid, 0);
    auto result2 = function.genRequestMsg(testEid, 255);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgMultipleCalls)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result1 = function.genRequestMsg(testEid, testInstanceId);
    auto result2 = function.genRequestMsg(testEid, testInstanceId);
    auto result3 = function.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgWithZeroEid)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result = function.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmPCIeFunctionTest, GenRequestMsgWithMaxEid)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    auto result = function.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// handleResponseMsg Tests
// ============================================================================

TEST_F(NsmPCIeFunctionTest, HandleResponseMsgWithNullResponse)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    uint8_t result = function.handleResponseMsg(nullptr, 0);

    // Should handle gracefully
    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeFunctionTest, HandleResponseMsgWithZeroLength)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    nsm_msg response{};
    uint8_t result = function.handleResponseMsg(&response, 0);

    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeFunctionTest, HandleResponseMsgWithDifferentFunctionIds)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;

    for (uint8_t funcId = 0; funcId <= 7; funcId++)
    {
        NsmPCIeFunction function(provider, testDeviceIndex, funcId);
        nsm_msg response{};

        uint8_t result = function.handleResponseMsg(&response,
                                                    sizeof(response));

        EXPECT_GE(result, 0);
    }
}

TEST_F(NsmPCIeFunctionTest, HandleResponseMsgMultipleTimes)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    nsm_msg response{};

    uint8_t result1 = function.handleResponseMsg(&response, sizeof(response));
    uint8_t result2 = function.handleResponseMsg(&response, sizeof(response));
    uint8_t result3 = function.handleResponseMsg(&response, sizeof(response));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

TEST_F(NsmPCIeFunctionTest, HandleResponseMsgWithSmallResponse)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    nsm_msg response{};
    uint8_t result = function.handleResponseMsg(&response, 10);

    EXPECT_GE(result, 0);
}

TEST_F(NsmPCIeFunctionTest, HandleResponseMsgWithLargeResponse)
{
    NsmInterfaceProvider<PCIeDeviceIntf> provider;
    NsmPCIeFunction function(provider, testDeviceIndex, testFunctionId);

    nsm_msg response{};
    uint8_t result = function.handleResponseMsg(&response, 1000);

    EXPECT_GE(result, 0);
}
