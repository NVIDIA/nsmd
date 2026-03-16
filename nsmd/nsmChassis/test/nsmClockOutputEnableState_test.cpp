/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmClockOutputEnableState.hpp"

using namespace nsm;

// Concrete subclass for testing the abstract NsmClockOutputEnableStateBase
class TestNsmClockOutputEnableState : public NsmClockOutputEnableStateBase
{
  protected:
    void handleResponse(const uint32_t& /*data*/) override {}

  public:
    TestNsmClockOutputEnableState(const NsmObject& provider,
                                  clock_output_enable_state_index bufferIndex,
                                  NsmDeviceIdentification deviceType,
                                  uint8_t instanceNumber,
                                  bool retimer = false) :
        NsmClockOutputEnableStateBase(provider, bufferIndex, deviceType,
                                      instanceNumber, retimer)
    {}
};

class NsmClockOutputEnableStateBaseTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId =
        1; // must be <= NSM_INSTANCE_MAX (31)
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithGPU)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_GPU;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithSwitch)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_SWITCH;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithPCIeBridge)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_PCIE_BRIDGE;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithBaseboard)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_BASEBOARD;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithRetimerTrue)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_BASEBOARD;
    uint8_t instanceNumber = 0;
    bool retimer = true;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithRetimerFalse)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_BASEBOARD;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       ConstructorWithDifferentInstanceNumbers)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_GPU;
    bool retimer = false;

    for (uint8_t i = 0; i < 8; i++)
    {
        EXPECT_NO_THROW({
            TestNsmClockOutputEnableState state(provider, bufferIndex,
                                                deviceType, i, retimer);
        });
    }
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorWithNVHSBuffer)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = NVHS_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_GPU;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    EXPECT_NO_THROW({
        TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                            instanceNumber, retimer);
    });
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorSetsBufferIndex)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_GPU;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                        instanceNumber, retimer);

    EXPECT_EQ(state.bufferIndex, bufferIndex);
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorSetsDeviceType)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_SWITCH;
    uint8_t instanceNumber = 0;
    bool retimer = false;

    TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                        instanceNumber, retimer);

    EXPECT_EQ(state.deviceType, deviceType);
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorSetsInstanceNumber)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_GPU;
    uint8_t instanceNumber = 5;
    bool retimer = false;

    TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                        instanceNumber, retimer);

    EXPECT_EQ(state.instanceNumber, instanceNumber);
}

TEST_F(NsmClockOutputEnableStateBaseTest, ConstructorSetsRetimer)
{
    NsmObject provider("test", "test");
    clock_output_enable_state_index bufferIndex = PCIE_CLKBUF_INDEX;
    NsmDeviceIdentification deviceType = NSM_DEV_ID_BASEBOARD;
    uint8_t instanceNumber = 0;
    bool retimer = true;

    TestNsmClockOutputEnableState state(provider, bufferIndex, deviceType,
                                        instanceNumber, retimer);

    EXPECT_EQ(state.retimer, retimer);
}

// ============================================================================
// genRequestMsg Tests
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgReturnsValidMessage)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result = state.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgHasCorrectSize)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result = state.genRequestMsg(testEid, testInstanceId);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_clock_output_enabled_state_req));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgWithDifferentEids)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result1 = state.genRequestMsg(1, testInstanceId);
    auto result2 = state.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgWithDifferentInstanceIds)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result1 = state.genRequestMsg(testEid, 0);
    auto result2 = state.genRequestMsg(
        testEid, 31); // max valid MCTP instance_id (NSM_INSTANCE_MAX)

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgWithZeroEid)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result = state.genRequestMsg(0, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgWithMaxEid)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result = state.genRequestMsg(255, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgMultipleCalls)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result1 = state.genRequestMsg(testEid, testInstanceId);
    auto result2 = state.genRequestMsg(testEid, testInstanceId);
    auto result3 = state.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgWithNVHSBuffer)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    auto result = state.genRequestMsg(testEid, testInstanceId);

    EXPECT_TRUE(result.has_value());
}

TEST_F(NsmClockOutputEnableStateBaseTest, GenRequestMsgWithDifferentDeviceTypes)
{
    NsmObject provider("test", "test");

    TestNsmClockOutputEnableState gpu(provider, PCIE_CLKBUF_INDEX,
                                      NSM_DEV_ID_GPU, 0, false);
    TestNsmClockOutputEnableState sw(provider, PCIE_CLKBUF_INDEX,
                                     NSM_DEV_ID_SWITCH, 0, false);
    TestNsmClockOutputEnableState bridge(provider, PCIE_CLKBUF_INDEX,
                                         NSM_DEV_ID_PCIE_BRIDGE, 0, false);

    EXPECT_TRUE(gpu.genRequestMsg(testEid, testInstanceId).has_value());
    EXPECT_TRUE(sw.genRequestMsg(testEid, testInstanceId).has_value());
    EXPECT_TRUE(bridge.genRequestMsg(testEid, testInstanceId).has_value());
}

// ============================================================================
// handleResponseMsg Tests
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, HandleResponseMsgWithNullResponse)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    uint8_t result = state.handleResponseMsg(nullptr, 0);

    EXPECT_GE(result, 0);
}

TEST_F(NsmClockOutputEnableStateBaseTest, HandleResponseMsgWithZeroLength)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    nsm_msg response{};
    uint8_t result = state.handleResponseMsg(&response, 0);

    EXPECT_GE(result, 0);
}

TEST_F(NsmClockOutputEnableStateBaseTest, HandleResponseMsgMultipleTimes)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    nsm_msg response{};

    uint8_t result1 = state.handleResponseMsg(&response, sizeof(response));
    uint8_t result2 = state.handleResponseMsg(&response, sizeof(response));
    uint8_t result3 = state.handleResponseMsg(&response, sizeof(response));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

TEST_F(NsmClockOutputEnableStateBaseTest, HandleResponseMsgWithSmallResponse)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    nsm_msg response{};
    uint8_t result = state.handleResponseMsg(&response, 10);

    EXPECT_GE(result, 0);
}

TEST_F(NsmClockOutputEnableStateBaseTest, HandleResponseMsgWithLargeResponse)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    nsm_msg response{};
    uint8_t result = state.handleResponseMsg(&response, 1000);

    EXPECT_GE(result, 0);
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       HandleResponseMsgWithDifferentDeviceTypes)
{
    NsmObject provider("test", "test");
    nsm_msg response{};

    TestNsmClockOutputEnableState gpu(provider, PCIE_CLKBUF_INDEX,
                                      NSM_DEV_ID_GPU, 0, false);
    TestNsmClockOutputEnableState sw(provider, PCIE_CLKBUF_INDEX,
                                     NSM_DEV_ID_SWITCH, 0, false);
    TestNsmClockOutputEnableState bridge(provider, PCIE_CLKBUF_INDEX,
                                         NSM_DEV_ID_PCIE_BRIDGE, 0, false);

    uint8_t result1 = gpu.handleResponseMsg(&response, sizeof(response));
    uint8_t result2 = sw.handleResponseMsg(&response, sizeof(response));
    uint8_t result3 = bridge.handleResponseMsg(&response, sizeof(response));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       HandleResponseMsgWithDifferentInstanceNumbers)
{
    NsmObject provider("test", "test");
    nsm_msg response{};

    for (uint8_t i = 0; i < 8; i++)
    {
        TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                            NSM_DEV_ID_GPU, i, false);

        uint8_t result = state.handleResponseMsg(&response, sizeof(response));
        EXPECT_GE(result, 0);
    }
}
