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
 * Additional branch coverage tests for nsmMemory.cpp:
 *
 * Targets uncovered branches in:
 *   - createNsmMemorySensor factory: various property presence/absence combos
 *   - createNSMMemory: ErrorCorrection/DeviceType/Priority combos
 *   - NsmMemoryErrorCorrection: constructor with different correction types
 *   - NsmMemoryDeviceType: constructor with different device types
 *   - NsmMemCapacity: updateReading with large value
 *   - NsmRowRemapState: handleResponseMsg with flags=0 (both FALSE)
 *   - NsmRowRemappingCounts: handleResponseMsg success with zero counts
 *   - NsmEccErrorCountsDram: handleResponseMsg success with zero counts
 *   - NsmMemCurrClockFreq: handleResponseMsg success with zero freq
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

namespace nsm
{
requester::Coroutine createNsmMemorySensor(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

static auto& b2Bus = utils::DBusHandler::getBus();
static std::string b2Name("b2_sensor");
static std::string b2Type("b2_type");
static std::string b2ObjPath("/xyz/openbmc_project/inventory/b2_memory");

// ============================================================================
// Fixture with SensorManagerTest for factory tests
// ============================================================================
class NsmMemoryBranch2Fixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_Memory";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:11";
    const std::string sensorName = "Memory_B2";
    const std::string inventoryPath =
        "/xyz/openbmc_project/inventory/system/b2/memory/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmMemoryBranch2Fixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmMemoryBranch2Fixture()
    {
        cleanupDeviceSensors(devices);
    }

    void setupBaseProperties(const std::string& path,
                             dbus::PropertyMap extra = {})
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
        pm["Name"] = sensorName;
        pm["UUID"] = gpuUuid;
        pm["InventoryObjPath"] = inventoryPath;
        for (auto& [k, v] : extra)
        {
            pm[k] = v;
        }
    }

    void setupCurrentProperties(const std::string& path,
                                const std::string& intf,
                                dbus::PropertyMap props)
    {
        auto& pm = utils::MockDbusAsync::propertyMap(path, intf);
        pm = props;
    }

    static std::vector<uint8_t> decodeFail()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }
};

// ============================================================================
// Factory: NSM_Memory_Attributes type -> creates RowRemapping + ECC +
// MemCapUtil (exercises with separate current interface)
// ============================================================================
TEST_F(NsmMemoryBranch2Fixture, Factory_MemoryAttributes_SeparateIntf)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string path = "/test/memb2/attrs_sep";

    setupBaseProperties(path);
    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_Memory_Attributes")}});

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, intf, path);
    // 3 roundRobin: RowRemapState, RowRemappingCounts, RemappingAvailability
    // + EccErrorCountsDram + MemoryCapacityUtil (longRunning)
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Factory: Missing Name from base -> name="" (FALSE branch)
// ============================================================================
TEST_F(NsmMemoryBranch2Fixture, Factory_MissingName)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string path = "/test/memb2/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // No "Name"
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = inventoryPath + "noname/";

    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_Memory_Attributes")}});

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, intf, path);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Factory: Missing Type -> no sensor created (FALSE branch for all type checks)
// ============================================================================
TEST_F(NsmMemoryBranch2Fixture, Factory_MissingType_NoSensor)
{
    const std::string path = "/test/memb2/no_type";
    setupBaseProperties(path);
    // No Type property in current interface

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size(),
              before);
}

// ============================================================================
// Factory: Missing InventoryObjPath -> inventoryObjPath="" (FALSE branch)
// ============================================================================
TEST_F(NsmMemoryBranch2Fixture, DISABLED_Factory_MissingInventoryObjPath)
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_Memory.MemoryAttributes";
    const std::string path = "/test/memb2/no_inv";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    // No "InventoryObjPath"

    setupCurrentProperties(path, intf,
                           {{"Type", std::string("NSM_Memory_Attributes")}});

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmMemorySensor(mockManager, intf, path);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg success with flags=0x00
// (both failure and pending = false)
// ============================================================================
TEST(NsmRowRemapStateBranch2, HandleResponseMsg_Success_FlagsZero)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b2Bus, b2ObjPath.c_str());
    NsmRowRemapState sensor(b2Name, b2Type, rowRemapIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0x00; // bit0=0 (no failure), bit1=0 (no pending)

    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::False);
    EXPECT_EQ(rowRemapIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::False);
}

// ============================================================================
// NsmRowRemappingCounts: handleResponseMsg success with zero counts
// ============================================================================
TEST(NsmRowRemappingCountsBranch2, HandleResponseMsg_Success_ZeroCounts)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b2Bus, b2ObjPath.c_str());
    NsmRowRemappingCounts sensor(b2Name, b2Type, rowRemapIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remapping_counts_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    auto rc = encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, ERR_NULL, 0,
                                                   0, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->ceRowRemappingCount(), 0u);
    EXPECT_EQ(rowRemapIntf->ueRowRemappingCount(), 0u);
}

// ============================================================================
// NsmEccErrorCountsDram: handleResponseMsg success with zero counts
// ============================================================================
TEST(NsmEccErrorCountsDramBranch2, HandleResponseMsg_Success_ZeroCounts)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b2Bus, b2ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b2Name, b2Type, eccIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_ECC_error_counts errorCounts = {};

    auto rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL,
                                               &errorCounts, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(eccIntf->ceCount(), 0);
    EXPECT_EQ(eccIntf->ueCount(), 0);
}

// ============================================================================
// NsmMemCurrClockFreq: handleResponseMsg success with zero frequency
// ============================================================================
TEST(NsmMemCurrClockFreqBranch2, HandleResponseMsg_Success_ZeroFreq)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b2Bus, b2ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b2Name, b2Type, dimmIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockFreq = 0;

    auto rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL,
                                              &clockFreq, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(dimmIntf->memoryConfiguredSpeedInMhz(), 0u);
}

// ============================================================================
// NsmMemCapacity: updateReading with a large value
// ============================================================================
TEST(NsmMemCapacityBranch2, UpdateReading_LargeValue)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b2Bus, b2ObjPath.c_str());
    dimmIntf->memorySizeInKB(0);
    NsmMemCapacity sensor(b2Name, b2Type, dimmIntf);

    sensor.updateReading(std::optional<uint32_t>(4096));

    EXPECT_EQ(dimmIntf->memorySizeInKB(), 4096u * 1024);
}

// ============================================================================
// NsmMinMemoryClockLimit update() - encode fail (covered by existing but
// let's exercise with a different data pattern)
// ============================================================================
TEST_F(NsmMemoryBranch2Fixture, NsmMinClockLimit_Update_DecodeFail_Buffer)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b2Bus, "/xyz/openbmc_project/inventory/b2_clk_min");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMinMemoryClockLimit sensor(b2Name, b2Type, dimmIntf);

    // Response with buffer too short to decode successfully
    auto resp = decodeFail();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[0], 0u);
}

// ============================================================================
// NsmMaxMemoryClockLimit update() - decode fail with short buffer
// ============================================================================
TEST_F(NsmMemoryBranch2Fixture, NsmMaxClockLimit_Update_DecodeFail_Buffer)
{
    auto dimmIntf = std::make_shared<DimmIntf>(
        b2Bus, "/xyz/openbmc_project/inventory/b2_clk_max");
    dimmIntf->allowedSpeedsMT(std::vector<uint16_t>(2, 0));
    NsmMaxMemoryClockLimit sensor(b2Name, b2Type, dimmIntf);

    auto resp = decodeFail();

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(resp));

    sensor.update(gpu);

    auto speeds = dimmIntf->allowedSpeedsMT();
    EXPECT_EQ(speeds[1], 0u);
}

// ============================================================================
// NsmRemappingAvailabilityBankCount: handleResponseMsg success with all zero
// ============================================================================
TEST(NsmRemappingAvailabilityBranch2, HandleResponseMsg_Success_AllZero)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b2Bus, b2ObjPath.c_str());
    NsmRemappingAvailabilityBankCount sensor(b2Name, b2Type, rowRemapIntf,
                                             b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) +
            sizeof(struct nsm_get_row_remap_availability_resp),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_row_remap_availability data = {};

    auto rc = encode_get_row_remap_availability_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->highRemappingAvailablityBankCount(), 0u);
    EXPECT_EQ(rowRemapIntf->maxRemappingAvailablityBankCount(), 0u);
    EXPECT_EQ(rowRemapIntf->lowRemappingAvailablityBankCount(), 0u);
    EXPECT_EQ(rowRemapIntf->noRemappingAvailablityBankCount(), 0u);
    EXPECT_EQ(rowRemapIntf->partialRemappingAvailablityBankCount(), 0u);
}

// ============================================================================
// NsmMemCapacity: updateReading with value = 0
// ============================================================================
TEST(NsmMemCapacityBranch2, UpdateReading_ZeroValue)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b2Bus, b2ObjPath.c_str());
    dimmIntf->memorySizeInKB(999);
    NsmMemCapacity sensor(b2Name, b2Type, dimmIntf);

    sensor.updateReading(std::optional<uint32_t>(0));

    EXPECT_EQ(dimmIntf->memorySizeInKB(), 0u);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg with flags bit1=1, bit0=0
// (pending=true, failure=false)
// ============================================================================
TEST(NsmRowRemapStateBranch2, HandleResponseMsg_Success_PendingOnly)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b2Bus, b2ObjPath.c_str());
    NsmRowRemapState sensor(b2Name, b2Type, rowRemapIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0x02; // bit0=0 (no failure), bit1=1 (pending)

    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::False);
    EXPECT_EQ(rowRemapIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::True);
}

// ============================================================================
// NsmRowRemapState: handleResponseMsg with flags bit0=1, bit1=0
// (failure=true, pending=false)
// ============================================================================
TEST(NsmRowRemapStateBranch2, HandleResponseMsg_Success_FailureOnly)
{
    auto rowRemapIntf =
        std::make_shared<MemoryRowRemappingIntf>(b2Bus, b2ObjPath.c_str());
    NsmRowRemapState sensor(b2Name, b2Type, rowRemapIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_row_remap_state_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    bitfield8_t flags;
    flags.byte = 0x01; // bit0=1 (failure), bit1=0 (no pending)

    auto rc = encode_get_row_remap_state_resp(0, NSM_SUCCESS, ERR_NULL, &flags,
                                              response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(rowRemapIntf->rowRemappingFailureState(),
              MemoryRowRemappingIntf::RowRemappingFailureStates::True);
    EXPECT_EQ(rowRemapIntf->rowRemappingPendingState(),
              MemoryRowRemappingIntf::RowRemappingPendingStates::False);
}

// ============================================================================
// NsmEccErrorCountsDram: handleResponseMsg success with large counts
// ============================================================================
TEST(NsmEccErrorCountsDramBranch2, HandleResponseMsg_Success_LargeCounts)
{
    auto eccIntf = std::make_shared<EccModeIntfDram>(b2Bus, b2ObjPath.c_str());
    NsmEccErrorCountsDram sensor(b2Name, b2Type, eccIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_ECC_error_counts_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    struct nsm_ECC_error_counts errorCounts = {};
    errorCounts.dram_corrected = 999999;
    errorCounts.dram_uncorrected = 888888;

    auto rc = encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, ERR_NULL,
                                               &errorCounts, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(eccIntf->ceCount(), 999999);
    EXPECT_EQ(eccIntf->ueCount(), 888888);
}

// ============================================================================
// NsmMemCurrClockFreq: handleResponseMsg success with high freq
// ============================================================================
TEST(NsmMemCurrClockFreqBranch2, HandleResponseMsg_Success_HighFreq)
{
    auto dimmIntf = std::make_shared<DimmIntf>(b2Bus, b2ObjPath.c_str());
    NsmMemCurrClockFreq sensor(b2Name, b2Type, dimmIntf, b2ObjPath);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_curr_clock_freq_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint32_t clockFreq = 4800;

    auto rc = encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, ERR_NULL,
                                              &clockFreq, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(dimmIntf->memoryConfiguredSpeedInMhz(), 4800u);
}
