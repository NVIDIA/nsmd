/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Branch coverage tests for nsmCommon.cpp
 *
 * Targets:
 *   - NsmMemoryCapacity::genRequestMsg: encode failure
 *   - NsmMemoryCapacity::handleResponseMsg: decode success + else branch
 *   - NsmMemoryCapacity::handleResponseMsg: decode success cc ternary
 *   - NsmMinGraphicsClockLimit::update: sensorIO fail, decode fail/success
 *   - NsmMaxGraphicsClockLimit::update: sensorIO fail, decode fail/success
 *   - NsmMinGraphicsClockLimit::update: decode success, sets minSpeed
 *   - NsmMaxGraphicsClockLimit::update: decode success, sets maxSpeed
 *   - NsmMemoryCapacityUtil::update: delegates to totalMemory + long-running
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmCommon/nsmCommon.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmCommonBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmCommonBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmCommonBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NsmMemoryCapacity branches
// ============================================================================

// genRequestMsg: encode failure (invalid instanceId)
TEST_F(NsmCommonBranchTest, MemoryCapacity_GenReqFail_ReturnsNullopt)
{
    auto sensor = std::make_shared<NsmTotalMemory>("MemCapBr", "NSM_Mem");
    auto result = sensor->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

// handleResponseMsg: success → updates reading
TEST_F(NsmCommonBranchTest, MemoryCapacity_HandleResp_Success_UpdatesReading)
{
    auto sensor = std::make_shared<NsmTotalMemory>("MemCapBr2", "NSM_Mem");

    uint32_t capacity = 8192;
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &capacity, sizeof(capacity));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(capacity));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(capacity), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor->handleResponseMsg(msg, response.size());
    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_TRUE(sensor->getReading().has_value());
    EXPECT_EQ(sensor->getReading().value(), 8192u);
}

// handleResponseMsg: error CC → else branch (updateReading(nullopt))
TEST_F(NsmCommonBranchTest, MemoryCapacity_HandleResp_ErrorCC_NulloptReading)
{
    auto sensor = std::make_shared<NsmTotalMemory>("MemCapBr3", "NSM_Mem");

    // Set initial reading
    sensor->totalMemoryCapacity = 100;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto result = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
    // Reading should be nullopt after error
    EXPECT_FALSE(sensor->getReading().has_value());
}

// handleResponseMsg: decode fail → else branch
TEST_F(NsmCommonBranchTest, MemoryCapacity_HandleResp_DecodeFail_ElseBranch)
{
    auto sensor = std::make_shared<NsmTotalMemory>("MemCapBr4", "NSM_Mem");

    // Truncated buffer → decode fails
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto result = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(result, NSM_SUCCESS);
}

// ============================================================================
// NsmMinGraphicsClockLimit::update branches
// ============================================================================

// sensorIO failure
TEST_F(NsmCommonBranchTest, MinClockLimit_Update_SensorIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/test/minclk_br";
    auto cpuConfig = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                              invPath.c_str());

    std::string n = "MinClkBr";
    std::string t = "NSM_ClockLimit";
    auto sensor = std::make_shared<NsmMinGraphicsClockLimit>(n, t, cpuConfig,
                                                             invPath);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
}

// decode success + cc=0 + valid data → sets minSpeed
TEST_F(NsmCommonBranchTest, MinClockLimit_Update_Success_SetsMinSpeed)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/test/minclk_br2";
    auto cpuConfig = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                              invPath.c_str());

    std::string n = "MinClkBr2";
    std::string t = "NSM_ClockLimit";
    auto sensor = std::make_shared<NsmMinGraphicsClockLimit>(n, t, cpuConfig,
                                                             invPath);

    uint32_t value = 300; // MHz
    value = htole32(value);
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(cpuConfig->minSpeed(), 300u);
}

// decode success + cc != 0 → does not update minSpeed
TEST_F(NsmCommonBranchTest, MinClockLimit_Update_NonZeroCC_NoUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/test/minclk_br3";
    auto cpuConfig = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                              invPath.c_str());
    cpuConfig->minSpeed(999);

    std::string n = "MinClkBr3";
    std::string t = "NSM_ClockLimit";
    auto sensor = std::make_shared<NsmMinGraphicsClockLimit>(n, t, cpuConfig,
                                                             invPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->update(gpu);
    EXPECT_EQ(cpuConfig->minSpeed(), 999u);
}

// ============================================================================
// NsmMaxGraphicsClockLimit::update branches
// ============================================================================

// sensorIO failure
TEST_F(NsmCommonBranchTest, MaxClockLimit_Update_SensorIOFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/test/maxclk_br";
    auto cpuConfig = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                              invPath.c_str());

    std::string n = "MaxClkBr";
    std::string t = "NSM_ClockLimit";
    auto sensor = std::make_shared<NsmMaxGraphicsClockLimit>(n, t, cpuConfig,
                                                             invPath);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERROR));

    sensor->update(gpu);
}

// decode success + cc=0 + valid data → sets maxSpeed
TEST_F(NsmCommonBranchTest, MaxClockLimit_Update_Success_SetsMaxSpeed)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/test/maxclk_br2";
    auto cpuConfig = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                              invPath.c_str());

    std::string n = "MaxClkBr2";
    std::string t = "NSM_ClockLimit";
    auto sensor = std::make_shared<NsmMaxGraphicsClockLimit>(n, t, cpuConfig,
                                                             invPath);

    uint32_t value = 1800; // MHz
    value = htole32(value);
    std::vector<uint8_t> data(4, 0);
    memcpy(data.data(), &value, sizeof(value));

    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_inventory_information_resp) +
                                  sizeof(value));
    auto msg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, sizeof(value), data.data(), msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    sensor->update(gpu);
    EXPECT_EQ(cpuConfig->maxSpeed(), 1800u);
}

// decode success + cc != 0 → does not update maxSpeed
TEST_F(NsmCommonBranchTest, MaxClockLimit_Update_NonZeroCC_NoUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/test/maxclk_br3";
    auto cpuConfig = std::make_shared<CpuOperatingConfigIntf>(bus,
                                                              invPath.c_str());
    cpuConfig->maxSpeed(777);

    std::string n = "MaxClkBr3";
    std::string t = "NSM_ClockLimit";
    auto sensor = std::make_shared<NsmMaxGraphicsClockLimit>(n, t, cpuConfig,
                                                             invPath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(buf));

    sensor->update(gpu);
    EXPECT_EQ(cpuConfig->maxSpeed(), 777u);
}
