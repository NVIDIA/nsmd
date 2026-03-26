/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Branch coverage tests for nsmMemory.cpp
 *
 * Targets uncovered branches in:
 *   - NsmRowRemapState: handleResponseMsg cc!=0 (decode OK, non-zero CC)
 *   - NsmRowRemappingCounts: handleResponseMsg cc!=0
 *   - NsmRemappingAvailabilityBankCount: genReq encode fail, handleResponseMsg
 *     cc!=0, decode fail (valgrind-safe)
 *   - NsmEccErrorCountsDram: genReq encode fail, handleResponseMsg cc!=0,
 *     decode fail (valgrind-safe)
 *   - NsmMemCurrClockFreq: genReq encode fail, handleResponseMsg cc!=0,
 *     decode fail (valgrind-safe)
 *   - NsmMemCapacity: updateReading with nullopt, updateReading with value
 *   - NsmMinMemoryClockLimit/NsmMaxMemoryClockLimit update(): dataSize mismatch
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

static auto& brBus = utils::DBusHandler::getBus();
static std::string brName("br_sensor");
static std::string brType("br_type");
static std::string brObjPath("/xyz/openbmc_project/inventory/br_memory");

// ============================================================================
// NsmRowRemapState - handleResponseMsg with non-zero CC (cc ? cc : rc -> true)
// ============================================================================

TEST(NsmRowRemapStateBranch, HandleResponseMsg_NonZeroCC_ReturnsCc)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemapState sensor(brName, brType, rowRemapIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0;
    uint16_t reason_code = ERR_NULL;

    auto rc = encode_get_row_remap_state_resp(0, NSM_ERROR, reason_code, &flags,
                                              response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmRowRemapStateBranch, HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemapState sensor(brName, brType, rowRemapIntf, brObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmRowRemappingCounts - handleResponseMsg with non-zero CC
// ============================================================================

TEST(NsmRowRemappingCountsBranch, HandleResponseMsg_NonZeroCC_ReturnsCc)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemappingCounts sensor(brName, brType, rowRemapIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t reason_code = ERR_NULL;
    uint32_t correctable = 0;
    uint32_t uncorrectable = 0;

    auto rc = encode_get_row_remapping_counts_resp(
        0, NSM_ERROR, reason_code, correctable, uncorrectable, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmRowRemappingCountsBranch, HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemappingCounts sensor(brName, brType, rowRemapIntf, brObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount
// ============================================================================

TEST(NsmRemappingAvailabilityBranch, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(brName, brType, rowRemapIntf,
                                             brObjPath);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmRemappingAvailabilityBranch, HandleResponseMsg_NonZeroCC_ReturnsCc)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(brName, brType, rowRemapIntf,
                                             brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_row_remap_availability data = {};

    auto rc = encode_get_row_remap_availability_resp(0, NSM_ERROR, ERR_NULL,
                                                     &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmRemappingAvailabilityBranch, HandleResponseMsg_Success_UpdatesReading)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(brName, brType, rowRemapIntf,
                                             brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_row_remap_availability data = {};
    data.high_remapping = 10;
    data.max_remapping = 20;
    data.low_remapping = 5;
    data.no_remapping = 3;
    data.partial_remapping = 7;

    auto rc = encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->highRemappingAvailablityBankCount(), 10);
    EXPECT_EQ(rowRemapIntf->maxRemappingAvailablityBankCount(), 20);
    EXPECT_EQ(rowRemapIntf->lowRemappingAvailablityBankCount(), 5);
    EXPECT_EQ(rowRemapIntf->noRemappingAvailablityBankCount(), 3);
    EXPECT_EQ(rowRemapIntf->partialRemappingAvailablityBankCount(), 7);
}

TEST(NsmRemappingAvailabilityBranch, HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(brName, brType, rowRemapIntf,
                                             brObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmEccErrorCountsDram
// ============================================================================

TEST(NsmEccErrorCountsDramBranch, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(brBus, brObjPath.c_str());
    NsmEccErrorCountsDram sensor(brName, brType, eccIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmEccErrorCountsDramBranch, HandleResponseMsg_Success_UpdatesReading)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(brBus, brObjPath.c_str());
    NsmEccErrorCountsDram sensor(brName, brType, eccIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_ECC_error_counts errorCounts = {};
    errorCounts.dram_corrected = 42;
    errorCounts.dram_uncorrected = 7;

    auto rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL,
                                               &errorCounts, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(eccIntf->ceCount(), 42);
    EXPECT_EQ(eccIntf->ueCount(), 7);
}

TEST(NsmEccErrorCountsDramBranch, HandleResponseMsg_NonZeroCC_ReturnsCc)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(brBus, brObjPath.c_str());
    NsmEccErrorCountsDram sensor(brName, brType, eccIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_ECC_error_counts errorCounts = {};

    auto rc = encode_get_ECC_error_counts_resp(0, NSM_ERROR, ERR_NULL,
                                               &errorCounts, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmEccErrorCountsDramBranch, HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(brBus, brObjPath.c_str());
    NsmEccErrorCountsDram sensor(brName, brType, eccIntf, brObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmMemCurrClockFreq
// ============================================================================

TEST(NsmMemCurrClockFreqBranch, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    NsmMemCurrClockFreq sensor(brName, brType, dimmIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmMemCurrClockFreqBranch, HandleResponseMsg_Success_UpdatesReading)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    NsmMemCurrClockFreq sensor(brName, brType, dimmIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockFreq = 2400;

    auto rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL,
                                              &clockFreq, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(dimmIntf->memoryConfiguredSpeedInMhz(), 2400u);
}

TEST(NsmMemCurrClockFreqBranch, HandleResponseMsg_NonZeroCC_ReturnsCc)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    NsmMemCurrClockFreq sensor(brName, brType, dimmIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockFreq = 0;

    auto rc = encode_get_curr_clock_freq_resp(0, NSM_ERROR, ERR_NULL,
                                              &clockFreq, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST(NsmMemCurrClockFreqBranch, HandleResponseMsg_DecodeFail_ValgrindSafe)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    NsmMemCurrClockFreq sensor(brName, brType, dimmIntf, brObjPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// NsmMemCapacity - updateReading branches
// ============================================================================

TEST(NsmMemCapacityBranch, UpdateReading_Nullopt_NoUpdate)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    dimmIntf->memorySizeInKB(0);
    NsmMemCapacity sensor(brName, brType, dimmIntf);

    sensor.updateReading(std::nullopt);

    EXPECT_EQ(dimmIntf->memorySizeInKB(), 0u);
}

TEST(NsmMemCapacityBranch, UpdateReading_WithValue_UpdatesSize)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    dimmIntf->memorySizeInKB(0);
    NsmMemCapacity sensor(brName, brType, dimmIntf);

    sensor.updateReading(std::optional<uint32_t>(128));

    EXPECT_EQ(dimmIntf->memorySizeInKB(), 128u * 1024);
}

// ============================================================================
// NsmMinMemoryClockLimit / NsmMaxMemoryClockLimit update() - dataSize mismatch
//
// When decode succeeds (rc==0, cc==0) but dataSize != sizeof(uint32_t),
// the inner if (rc==0 && cc==0 && dataSize==sizeof(value)) is FALSE,
// so updateReading is NOT called. This covers the dataSize mismatch branch.
// ============================================================================

class NsmMemoryBranchFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string memPath = "/xyz/openbmc_project/inventory/br_mem_clock";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryBranchFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryBranchFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmMemoryBranchFixture,
       NsmMinClockLimit_Update_DataSizeMismatch_NoUpdate)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(brName, brType, dimmIntf);

    // Build response with dataSize=2 (not sizeof(uint32_t)=4)
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t shortData = 1234;
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(shortData),
        reinterpret_cast<uint8_t*>(&shortData), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseMsg));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 0u);
}

TEST_F(NsmMemoryBranchFixture,
       NsmMaxClockLimit_Update_DataSizeMismatch_NoUpdate)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(brName, brType, dimmIntf);

    // Build response with dataSize=2 (not sizeof(uint32_t)=4)
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint16_t shortData = 5678;
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(shortData),
        reinterpret_cast<uint8_t*>(&shortData), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseMsg));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 0u);
}

// ============================================================================
// NsmRowRemapState - genRequestMsg encode fail (instanceId > MAX)
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch in genRequestMsg
// ============================================================================

TEST(NsmRowRemapStateBranch, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemapState sensor(brName, brType, rowRemapIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmRowRemappingCounts - genRequestMsg encode fail (instanceId > MAX)
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch in genRequestMsg
// ============================================================================

TEST(NsmRowRemappingCountsBranch, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemappingCounts sensor(brName, brType, rowRemapIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmRowRemapState - genRequestMsg success path
// Covers: if (rc != NSM_SW_SUCCESS) FALSE branch in genRequestMsg
// ============================================================================

TEST(NsmRowRemapStateBranch, GenRequestMsg_Success_ReturnsRequest)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemapState sensor(brName, brType, rowRemapIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmRowRemappingCounts - genRequestMsg success path
// Covers: if (rc != NSM_SW_SUCCESS) FALSE branch in genRequestMsg
// ============================================================================

TEST(NsmRowRemappingCountsBranch, GenRequestMsg_Success_ReturnsRequest)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemappingCounts sensor(brName, brType, rowRemapIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmRemappingAvailabilityBankCount - genRequestMsg success path
// ============================================================================

TEST(NsmRemappingAvailabilityBranch, GenRequestMsg_Success_ReturnsRequest)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(brName, brType, rowRemapIntf,
                                             brObjPath);

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmEccErrorCountsDram - genRequestMsg success path
// ============================================================================

TEST(NsmEccErrorCountsDramBranch, GenRequestMsg_Success_ReturnsRequest)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(brBus, brObjPath.c_str());
    NsmEccErrorCountsDram sensor(brName, brType, eccIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmMemCurrClockFreq - genRequestMsg success path
// ============================================================================

TEST(NsmMemCurrClockFreqBranch, GenRequestMsg_Success_ReturnsRequest)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, brObjPath.c_str());
    NsmMemCurrClockFreq sensor(brName, brType, dimmIntf, brObjPath);

    auto request = sensor.genRequestMsg(12, 0);
    EXPECT_TRUE(request.has_value());
}

// ============================================================================
// NsmMinMemoryClockLimit update() - sensorIO fail path
// Covers: if (rc) TRUE branch in NsmMinMemoryClockLimit::update
// ============================================================================

TEST_F(NsmMemoryBranchFixture, NsmMinClockLimit_Update_SensorIOFail_ReturnsRc)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(brName, brType, dimmIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 0u);
}

// ============================================================================
// NsmMaxMemoryClockLimit update() - sensorIO fail path
// Covers: if (rc) TRUE branch in NsmMaxMemoryClockLimit::update
// ============================================================================

TEST_F(NsmMemoryBranchFixture, NsmMaxClockLimit_Update_SensorIOFail_ReturnsRc)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(brName, brType, dimmIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 0u);
}

// ============================================================================
// NsmMinMemoryClockLimit update() - success path (dataSize == sizeof(uint32_t))
// Covers: if (rc==0 && cc==0 && dataSize==sizeof(value)) TRUE branch
// ============================================================================

TEST_F(NsmMemoryBranchFixture, NsmMinClockLimit_Update_Success_UpdatesReading)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(brName, brType, dimmIntf);

    // Build response with dataSize=4 (== sizeof(uint32_t))
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockVal = htole32(1600);
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(clockVal),
        reinterpret_cast<uint8_t*>(&clockVal), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseMsg));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 1600u);
}

// ============================================================================
// NsmMaxMemoryClockLimit update() - success path (dataSize == sizeof(uint32_t))
// Covers: if (rc==0 && cc==0 && dataSize==sizeof(value)) TRUE branch
// ============================================================================

TEST_F(NsmMemoryBranchFixture, NsmMaxClockLimit_Update_Success_UpdatesReading)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(brName, brType, dimmIntf);

    // Build response with dataSize=4 (== sizeof(uint32_t))
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockVal = htole32(2400);
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(clockVal),
        reinterpret_cast<uint8_t*>(&clockVal), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseMsg));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 2400u);
}

// ============================================================================
// NsmMinMemoryClockLimit update() - decode fail (non-success cc)
// Covers: shouldLog branch when cc != NSM_SUCCESS
// ============================================================================

TEST_F(NsmMemoryBranchFixture, NsmMinClockLimit_Update_DecodeFail_NonSuccessCc)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(brName, brType, dimmIntf);

    // Build response with non-success CC
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockVal = 0;
    auto rc = encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, sizeof(clockVal),
        reinterpret_cast<uint8_t*>(&clockVal), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseMsg));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 0u);
}

// ============================================================================
// NsmMaxMemoryClockLimit update() - decode fail (non-success cc)
// Covers: shouldLog branch when cc != NSM_SUCCESS
// ============================================================================

TEST_F(NsmMemoryBranchFixture, NsmMaxClockLimit_Update_DecodeFail_NonSuccessCc)
{
    auto dimmIntf = std::make_shared<DimmIntf>(brBus, memPath.c_str());
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(brName, brType, dimmIntf);

    // Build response with non-success CC
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockVal = 0;
    auto rc = encode_get_inventory_information_resp(
        0, NSM_ERROR, ERR_NULL, sizeof(clockVal),
        reinterpret_cast<uint8_t*>(&clockVal), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(responseMsg));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 0u);
}

// ============================================================================
// NsmRowRemapState - handleResponseMsg success verifies reading update
// ============================================================================

TEST(NsmRowRemapStateBranch, HandleResponseMsg_Success_VerifiesUpdateReading)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemapState sensor(brName, brType, rowRemapIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0x03; // bit0=1 (failure), bit1=1 (pending)

    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_TRUE(rowRemapIntf->rowRemappingFailureState());
    EXPECT_TRUE(rowRemapIntf->rowRemappingPendingState());
}

// ============================================================================
// NsmRowRemappingCounts - handleResponseMsg success verifies reading update
// ============================================================================

TEST(NsmRowRemappingCountsBranch,
     HandleResponseMsg_Success_VerifiesUpdateReading)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(brBus, brObjPath.c_str());
    NsmRowRemappingCounts sensor(brName, brType, rowRemapIntf, brObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint32_t correctable = 100;
    uint32_t uncorrectable = 50;

    auto rc = encode_get_row_remapping_counts_resp(
        0, NSM_SUCCESS, ERR_NULL, correctable, uncorrectable, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->ceRowRemappingCount(), 100u);
    EXPECT_EQ(rowRemapIntf->ueRowRemappingCount(), 50u);
}
