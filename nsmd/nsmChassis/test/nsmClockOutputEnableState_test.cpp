/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#define private public
#define protected public

#include "libnsm/platform-environmental.h"

#include "nsmChassis/nsmClockOutputEnableState.hpp"

using namespace nsm;

// Concrete test subclass of NsmClockOutputEnableStateBase
// (base class has pure virtual handleResponse)
class TestNsmClockOutputEnableState : public NsmClockOutputEnableStateBase
{
  public:
    using NsmClockOutputEnableStateBase::NsmClockOutputEnableStateBase;

    void handleResponse(const uint32_t& /*data*/) override
    {
        // No-op for testing
    }
};

class NsmClockOutputEnableStateBaseTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
    static constexpr uint8_t testInstanceId = 5;
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
    auto result2 = state.genRequestMsg(testEid, 31);

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

// Error-CC path: decode succeeds but cc = NSM_ERROR (non-zero)
// → return cc ? cc : rc; takes the true branch (return cc).
TEST_F(NsmClockOutputEnableStateBaseTest,
       HandleResponseMsg_ErrorCC_CoversBranch)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_output_enabled_state_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t rc = encode_get_clock_output_enable_state_resp(
        testInstanceId, NSM_ERROR, ERR_NULL, 0, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    uint8_t result = state.handleResponseMsg(response, responseData.size());
    EXPECT_NE(result, NSM_SW_SUCCESS);
}

// ============================================================================
// genRequestMsg Failure Path
// ============================================================================

// instanceId > NSM_INSTANCE_MAX (31) → encode function returns non-zero →
// genRequestMsg returns std::nullopt (lines 40-46)
TEST_F(NsmClockOutputEnableStateBaseTest,
       GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);
    // NSM_INSTANCE_MAX = 31; instanceId = 32 triggers encode failure
    auto result = state.genRequestMsg(testEid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// getPCIeClockBufferData — GPU instances 0-7 and default (>7)
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst0)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu1 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst1)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 1, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu2 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst2)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 2, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu3 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst3)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 3, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu4 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst4)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 4, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu5 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst5)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 5, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu6 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst6)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 6, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu7 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_GPU_Inst7)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 7, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_gpu8 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

// GPU instanceNumber >= 8 → inner default branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_GPU_DefaultInstance)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 8, false);
    nsm_pcie_clock_buffer_data data{};
    EXPECT_FALSE(state.getPCIeClockBufferData(data));
}

// ============================================================================
// getPCIeClockBufferData — NSM_DEV_ID_SWITCH instances 0, 1, and default
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_Switch_Inst0)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 0, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_nvSwitch_1 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_Switch_Inst1)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 1, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_nvSwitch_2 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

// instanceNumber >= 2 → inner default branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Switch_DefaultInstance)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 5, false);
    nsm_pcie_clock_buffer_data data{};
    EXPECT_FALSE(state.getPCIeClockBufferData(data));
}

// ============================================================================
// getPCIeClockBufferData — NSM_DEV_ID_PCIE_BRIDGE
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_PCIeBridge)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_PCIE_BRIDGE, 0, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_nvlinkMgmt_nic = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

// ============================================================================
// getPCIeClockBufferData — NSM_DEV_ID_BASEBOARD
// ============================================================================

// retimer=false → if(retimer) false branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_NoRetimer)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 0, false);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer1 = 1; // set but retimer=false so ignored
    EXPECT_FALSE(state.getPCIeClockBufferData(data));
}

// retimer=true, instances 0-7 → clk_buf_retimer1..8
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst0)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 0, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer1 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst1)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 1, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer2 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst2)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 2, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer3 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst3)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 3, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer4 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst4)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 4, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer5 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst5)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 5, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer6 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst6)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 6, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer7 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_Inst7)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 7, true);
    nsm_pcie_clock_buffer_data data{};
    data.clk_buf_retimer8 = 1;
    EXPECT_TRUE(state.getPCIeClockBufferData(data));
}

// retimer=true, instanceNumber >= 8 → inner default branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetPCIeClockBufferData_Baseboard_Retimer_DefaultInstance)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 10, true);
    nsm_pcie_clock_buffer_data data{};
    EXPECT_FALSE(state.getPCIeClockBufferData(data));
}

// Unknown device type → outer default branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest, GetPCIeClockBufferData_UnknownDevice)
{
    NsmObject provider("test", "test");
    // Cast an unused enum value to get the outer default branch
    TestNsmClockOutputEnableState state(
        provider, PCIE_CLKBUF_INDEX, (NsmDeviceIdentification)0xFE, 0, false);
    nsm_pcie_clock_buffer_data data{};
    EXPECT_FALSE(state.getPCIeClockBufferData(data));
}

// ============================================================================
// getNVHSClockBufferData — GPU instances 0-7 and default (>7)
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst0)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs1 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst1)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 1, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs2 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst2)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 2, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs3 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst3)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 3, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs4 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst4)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 4, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs5 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst5)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 5, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs6 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst6)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 6, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs7 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_GPU_Inst7)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 7, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_sxm_nvhs8 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

// GPU instanceNumber >= 8 → inner default branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetNVHSClockBufferData_GPU_DefaultInstance)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 8, false);
    nsm_nvhs_clock_buffer_data data{};
    EXPECT_FALSE(state.getNVHSClockBufferData(data));
}

// ============================================================================
// getNVHSClockBufferData — NSM_DEV_ID_SWITCH instances 0-3 and default (>3)
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_Switch_Inst0)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 0, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_nvSwitch_nvhs1 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_Switch_Inst1)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 1, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_nvSwitch_nvhs2 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_Switch_Inst2)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 2, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_nvSwitch_nvhs3 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest, GetNVHSClockBufferData_Switch_Inst3)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 3, false);
    nsm_nvhs_clock_buffer_data data{};
    data.clk_buf_nvSwitch_nvhs4 = 1;
    EXPECT_TRUE(state.getNVHSClockBufferData(data));
}

// instanceNumber >= 4 → inner default branch, result stays false
TEST_F(NsmClockOutputEnableStateBaseTest,
       GetNVHSClockBufferData_Switch_DefaultInstance)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_SWITCH, 5, false);
    nsm_nvhs_clock_buffer_data data{};
    EXPECT_FALSE(state.getNVHSClockBufferData(data));
}

// ============================================================================
// getNVHSClockBufferData — NSM_DEV_ID_PCIE_BRIDGE, NSM_DEV_ID_BASEBOARD,
// and unknown device type all map to the same default path → return false
// ============================================================================

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetNVHSClockBufferData_PCIeBridge_ReturnsFalse)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_PCIE_BRIDGE, 0, false);
    nsm_nvhs_clock_buffer_data data{};
    EXPECT_FALSE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetNVHSClockBufferData_Baseboard_ReturnsFalse)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, NVHS_CLKBUF_INDEX,
                                        NSM_DEV_ID_BASEBOARD, 0, false);
    nsm_nvhs_clock_buffer_data data{};
    EXPECT_FALSE(state.getNVHSClockBufferData(data));
}

TEST_F(NsmClockOutputEnableStateBaseTest,
       GetNVHSClockBufferData_UnknownDevice_ReturnsFalse)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(
        provider, NVHS_CLKBUF_INDEX, (NsmDeviceIdentification)0xFE, 0, false);
    nsm_nvhs_clock_buffer_data data{};
    EXPECT_FALSE(state.getNVHSClockBufferData(data));
}

// nsmClockOutputEnableState.cpp L65 second operand of ||:
// rc==NSM_SW_SUCCESS but cc==NSM_ERROR.
// 9-byte buffer: decode_get_clock_output_enable_state_resp calls
// decode_reason_code_and_cc which returns NSM_SW_SUCCESS with cc=NSM_ERROR
// → second operand of (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS) is TRUE.
TEST_F(NsmClockOutputEnableStateBaseTest,
       HandleResponseMsg_DecodeSuccessNonZeroCC_ReturnsCCError)
{
    NsmObject provider("test", "test");
    TestNsmClockOutputEnableState state(provider, PCIE_CLKBUF_INDEX,
                                        NSM_DEV_ID_GPU, 0, false);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR

    auto response = reinterpret_cast<nsm_msg*>(buf.data());
    uint8_t result = state.handleResponseMsg(response, buf.size());
    EXPECT_NE(result, NSM_SW_SUCCESS);
}
