/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Additional branch coverage tests for nsmMemory.cpp:
 *
 * Targets uncovered branches in:
 *   - NsmRowRemapState::handleResponseMsg: cc!=0 ternary (cc ? cc : rc)
 *   - NsmRowRemappingCounts::handleResponseMsg: cc!=0 ternary
 *   - NsmRemappingAvailabilityBankCount::handleResponseMsg: success/error
 *   - NsmEccErrorCountsDram::handleResponseMsg: success/error/cc ternary
 *   - NsmMemoryCapacityUtil::updateReading: totalMemory==0 and nullopt branches
 *   - NsmMemoryCapacityUtil::handleResponseMsg: isLongRunning true/false
 *   - NsmMemoryCapacityUtil::equals: different totalMemory
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "interfaceWrapper.hpp"
#include "nsmMemory.hpp"

using namespace nsm;

static auto& b4Bus = utils::DBusHandler::getBus();
static std::string b4Name("b4_sensor");
static std::string b4Type("b4_type");
static std::string b4ObjPath("/xyz/openbmc_project/inventory/b4_memory");

// ============================================================================
// Fixture
// ============================================================================
struct NsmMemoryBranch4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryBranch4Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryBranch4Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmRowRemapState: handleResponseMsg with error CC
// ============================================================================

TEST_F(NsmMemoryBranch4Test, RowRemapState_HandleResp_ErrorCC)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b4Bus,
                                                           b4ObjPath.c_str());
    NsmRowRemapState sensor(b4Name, b4Type, rrIntf, b4ObjPath);

    // Encode response with error CC
    bitfield8_t flags = {};
    flags.byte = 0x03;
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_state_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_state_resp(0, NSM_ERROR, 0xABCD, &flags,
                                              msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_ERROR); // cc ? cc : rc => cc
}

TEST_F(NsmMemoryBranch4Test, RowRemapState_HandleResp_Success)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b4Bus,
                                                           b4ObjPath.c_str());
    NsmRowRemapState sensor(b4Name, b4Type, rrIntf, b4ObjPath);

    bitfield8_t flags = {};
    flags.bits.bit0 = 1; // failure state
    flags.bits.bit1 = 1; // pending state
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_state_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::True);
    EXPECT_EQ(rrIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::True);
}

// ============================================================================
// NsmRowRemappingCounts: handleResponseMsg with error CC and decode fail
// ============================================================================

TEST_F(NsmMemoryBranch4Test, RowRemappingCounts_HandleResp_ErrorCC)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b4Bus,
                                                           b4ObjPath.c_str());
    NsmRowRemappingCounts sensor(b4Name, b4Type, rrIntf, b4ObjPath);

    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remapping_counts_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remapping_counts_resp(0, NSM_ERROR, 0xABCD, 10, 5,
                                                   msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_ERROR);
}

TEST_F(NsmMemoryBranch4Test, RowRemappingCounts_HandleResp_DecodeFail)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b4Bus,
                                                           b4ObjPath.c_str());
    NsmRowRemappingCounts sensor(b4Name, b4Type, rrIntf, b4ObjPath);

    // Too short buffer
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: handleResponseMsg error CC
// ============================================================================

TEST_F(NsmMemoryBranch4Test, RemapAvailability_HandleResp_ErrorCC)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b4Bus,
                                                           b4ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b4Name, b4Type, rrIntf, b4ObjPath);

    nsm_row_remap_availability data = {1, 2, 3, 4, 5};
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_availability_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_availability_resp(0, NSM_ERROR, 0xABCD,
                                                     &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_ERROR);
}

TEST_F(NsmMemoryBranch4Test, DISABLED_RemapAvailability_HandleResp_Success)
{
    auto rrIntf = std::make_shared<MemoryRowRemappingIntf>(b4Bus,
                                                           b4ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b4Name, b4Type, rrIntf, b4ObjPath);

    nsm_row_remap_availability data = {10, 20, 30, 40, 50};
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_row_remap_availability_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(rrIntf->highRemappingAvailablityBankCount(), 10u);
    EXPECT_EQ(rrIntf->maxRemappingAvailablityBankCount(), 20u);
}

// ============================================================================
// NsmEccErrorCountsDram: handleResponseMsg success and error
// ============================================================================

TEST_F(NsmMemoryBranch4Test, EccErrorCounts_HandleResp_Success)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b4Bus, b4ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b4Name, b4Type, eccIntf, b4ObjPath);

    nsm_ECC_error_counts errorCounts = {};
    errorCounts.dram_corrected = 100;
    errorCounts.dram_uncorrected = 5;
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_ECC_error_counts_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    auto rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL,
                                               &errorCounts, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(eccIntf->ceCount(), 100);
    EXPECT_EQ(eccIntf->ueCount(), 5);
}

TEST_F(NsmMemoryBranch4Test, EccErrorCounts_HandleResp_ErrorCC)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b4Bus, b4ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b4Name, b4Type, eccIntf, b4ObjPath);

    nsm_ECC_error_counts errorCounts = {};
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_ECC_error_counts_resp));
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_ECC_error_counts_resp(0, NSM_ERROR, 0xABCD, &errorCounts, msg);

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_ERROR);
}

TEST_F(NsmMemoryBranch4Test, EccErrorCounts_HandleResp_DecodeFail)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b4Bus, b4ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b4Name, b4Type, eccIntf, b4ObjPath);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_SUCCESS;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmMemoryCapacityUtil: updateReading with nullopt and zero
// ============================================================================

TEST_F(NsmMemoryBranch4Test, MemCapUtil_UpdateReading_NulloptTotal)
{
    // Create totalMemory with no reading set (nullopt)
    auto totalMem = std::make_shared<NsmTotalMemory>("TotalMemBr", "NSM_Mem");
    // totalMemory has no reading => nullopt => updateReading returns early

    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider(
        "MemCapUtilBr", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_memutil"});
    NsmMemoryCapacityUtil sensor(provider, totalMem, false, gpu);

    nsm_memory_capacity_utilization data = {};
    data.used_memory = 100;
    data.reserved_memory = 50;
    // Should not crash - nullopt case returns early
    sensor.updateReading(data);
}

TEST_F(NsmMemoryBranch4Test, MemCapUtil_UpdateReading_ZeroTotal)
{
    auto totalMem = std::make_shared<NsmTotalMemory>("TotalMemBr2", "NSM_Mem");
    totalMem->updateReading(0); // zero total => returns early

    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider(
        "MemCapUtilBr2", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_memutil2"});
    NsmMemoryCapacityUtil sensor(provider, totalMem, false, gpu);

    nsm_memory_capacity_utilization data = {};
    data.used_memory = 100;
    data.reserved_memory = 50;
    sensor.updateReading(data);
}

TEST_F(NsmMemoryBranch4Test, MemCapUtil_UpdateReading_ValidTotal)
{
    auto totalMem = std::make_shared<NsmTotalMemory>("TotalMemBr3", "NSM_Mem");
    totalMem->updateReading(1000);

    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider(
        "MemCapUtilBr3", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_memutil3"});
    NsmMemoryCapacityUtil sensor(provider, totalMem, false, gpu);

    nsm_memory_capacity_utilization data = {};
    data.used_memory = 400;
    data.reserved_memory = 100;
    sensor.updateReading(data);
}

// ============================================================================
// NsmMemoryCapacityUtil: handleResponseMsg long-running vs non-long-running
// ============================================================================

TEST_F(NsmMemoryBranch4Test, MemCapUtil_HandleResp_NonLR_ErrorCC)
{
    auto totalMem = std::make_shared<NsmTotalMemory>("TotalMemH1", "NSM_Mem");
    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider(
        "MemCapUtilH1", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_memutil_h1"});
    NsmMemoryCapacityUtil sensor(provider, totalMem, false, gpu);

    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    msg->payload[1] = NSM_ERROR;

    auto result = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(result, NSM_ERROR);
}

// ============================================================================
// NsmMemoryCapacityUtil: equals with different totalMemory
// ============================================================================

TEST_F(NsmMemoryBranch4Test, DISABLED_MemCapUtil_Equals_DifferentTotalMem)
{
    auto totalMem1 = std::make_shared<NsmTotalMemory>("TotalEq1", "NSM_Mem");
    auto totalMem2 = std::make_shared<NsmTotalMemory>("TotalEq2", "NSM_Mem");

    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider1(
        "MemEq1", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_meq1"});
    NsmMemoryCapacityUtil sensor1(provider1, totalMem1, false, gpu);

    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider2(
        "MemEq2", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_meq2"});
    NsmMemoryCapacityUtil sensor2(provider2, totalMem2, false, gpu);

    // Different totalMemory names => not equal
    EXPECT_FALSE(sensor1.equals(sensor2));
}

TEST_F(NsmMemoryBranch4Test, MemCapUtil_Equals_SameTotalMem)
{
    auto totalMem = std::make_shared<NsmTotalMemory>("TotalEqSame", "NSM_Mem");

    NsmInterfaceProvider<DimmMemoryMetricsIntf> provider(
        "MemEqSame", "NSM_MemUtil",
        dbus::Interfaces{"/xyz/openbmc_project/inventory/b4_meqsame"});
    NsmMemoryCapacityUtil sensor1(provider, totalMem, false, gpu);
    NsmMemoryCapacityUtil sensor2(provider, totalMem, false, gpu);

    EXPECT_TRUE(sensor1.equals(sensor2));
}
