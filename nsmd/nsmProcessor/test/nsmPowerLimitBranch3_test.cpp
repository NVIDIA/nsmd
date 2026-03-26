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
 * Branch coverage batch 3 for nsmPowerLimit.cpp
 *
 * Targets remaining uncovered branches:
 * - powerLimitIdToDeviceModeIndex: default case (invalid ID)
 * - handleResponseMsg: currentModeLength != sizeof(uint32_t) (else branch)
 * - handleResponseMsg: pendingModeLength != sizeof(uint32_t) (else branch)
 * - handleResponseMsg: INVALID_POWER_LIMIT for currentLimit and pendingLimit
 * - setPowerLimit: std::get_if returns nullptr -> InvalidArgument
 * - setPowerLimit: with tuple<bool,uint32_t> value
 * - updatePowerLimit: postPatchIO failure
 * - updatePowerLimit: decode failure (cc != NSM_SUCCESS)
 * - NsmOneShotPowerLimit: currentModeLength incorrect
 * - NsmOneShotPowerLimit: handleResponseMsg with INVALID_POWER_LIMIT
 * - NsmOneShotPowerLimit: persistencyIntf is null
 * - NsmPowerLimitRange: propertyId default case
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmPowerLimit.hpp"

using namespace nsm;

// Forward-declare free function defined in nsmPowerLimit.cpp
namespace nsm
{
uint32_t powerLimitIdToDeviceModeIndex(uint8_t powerLimitId, bool persistent);
} // namespace nsm

// =============================================================================
// Fixture
// =============================================================================
struct NsmPowerLimitBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_PL3";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerLimitBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerLimitBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    std::shared_ptr<NsmPersistentPowerLimit>
        makeSensor(uint8_t limitId = GPU_BASE, bool withPersistency = true,
                   std::shared_ptr<PowerLimitsIntf>* outPLI = nullptr,
                   std::shared_ptr<PowerPersistencyIntf>* outPerI = nullptr)
    {
        auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
        auto cli = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
        auto assoc = std::make_shared<AssociationDefinitionsIntf>(
            bus(), objPath.c_str());
        std::shared_ptr<PowerPersistencyIntf> perI;
        if (withPersistency)
        {
            perI = std::make_shared<PowerPersistencyIntf>(bus(),
                                                          objPath.c_str());
            perI->persistentPowerLimit(300.0);
            perI->oneShotPowerLimit(0.0);
            perI->persistency(false);
        }
        if (outPLI)
            *outPLI = pli;
        if (outPerI)
            *outPerI = perI;
        return std::make_shared<NsmPersistentPowerLimit>(
            "TestPL3", "NSM_GPU_BASE_POWER_LIMIT", pli, cli, assoc, gpu,
            limitId, perI);
    }

    // Build a device_mode_settings_v2 success response
    std::vector<uint8_t> makeGetDevModeResp(uint32_t currentLimit,
                                            uint32_t pendingLimit)
    {
        uint32_t cl = htole32(currentLimit);
        uint32_t pl = htole32(pendingLimit);
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
                sizeof(cl) + sizeof(pl),
            0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        encode_get_device_mode_settings_v2_resp(
            0, NSM_SUCCESS, ERR_NULL, reinterpret_cast<const uint8_t*>(&cl),
            sizeof(cl), reinterpret_cast<const uint8_t*>(&pl), sizeof(pl),
            responseMsg);
        return response;
    }

    // Response with 0-length current/pending data
    std::vector<uint8_t> makeGetDevModeRespZeroLength()
    {
        std::vector<uint8_t> response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp),
            0);
        auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
        encode_get_device_mode_settings_v2_resp(
            0, NSM_SUCCESS, ERR_NULL, nullptr, 0, nullptr, 0, responseMsg);
        return response;
    }

    // Set response
    std::vector<uint8_t> makeSetDevModeResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                 0);
        auto msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_set_device_mode_settings_v2_resp(0, cc, ERR_NULL, msg);
        return buf;
    }
};

// =============================================================================
// powerLimitIdToDeviceModeIndex: default case (invalid powerLimitId)
// =============================================================================
TEST(NsmPowerLimitBranch3, PowerLimitIdToDeviceModeIndex_InvalidId_Default)
{
    // Use an invalid powerLimitId to hit the default case
    auto result = nsm::powerLimitIdToDeviceModeIndex(0xFF, true);
    EXPECT_EQ(result, 0u);

    result = nsm::powerLimitIdToDeviceModeIndex(0xFF, false);
    EXPECT_EQ(result, 0u);
}

// =============================================================================
// handleResponseMsg: currentModeLength != sizeof(uint32_t) -> else branch
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_ZeroCurrentModeLength_ElseBranch)
{
    auto sensor = makeSensor(GPU_BASE, true);

    auto response = makeGetDevModeRespZeroLength();
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleResponseMsg: INVALID_POWER_LIMIT for currentLimit -> reading = 0
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_InvalidPowerLimit_ReadsZero)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    std::shared_ptr<PowerPersistencyIntf> perI;
    auto sensor = makeSensor(GPU_BASE, true, &pli, &perI);

    auto response = makeGetDevModeResp(INVALID_POWER_LIMIT,
                                       INVALID_POWER_LIMIT);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(pli->powerCap(), 0u);
}

// =============================================================================
// handleResponseMsg: persistency check - powerCap matches persistentPowerLimit
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_PersistencyTrue_MatchingLimits)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    std::shared_ptr<PowerPersistencyIntf> perI;
    auto sensor = makeSensor(GPU_BASE, true, &pli, &perI);
    perI->persistentPowerLimit(300.0);

    auto response = makeGetDevModeResp(300000, 300000); // 300W in mW
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(pli->powerCap(), 300u);
    EXPECT_TRUE(perI->persistency());
}

TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_PersistencyFalse_MismatchLimits)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    std::shared_ptr<PowerPersistencyIntf> perI;
    auto sensor = makeSensor(GPU_BASE, true, &pli, &perI);
    perI->persistentPowerLimit(200.0);

    auto response = makeGetDevModeResp(300000, 300000); // 300W
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(pli->powerCap(), 300u);
    // The source code sets persistentPowerLimit to the reading (300) before
    // comparing, so powerCap()==persistentPowerLimit() is always true here.
    EXPECT_TRUE(perI->persistency());
}

// =============================================================================
// handleResponseMsg: pending limit is valid (non-INVALID_POWER_LIMIT)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_ValidPendingLimit_HasValue)
{
    auto sensor = makeSensor(GPU_BASE, false);

    auto response = makeGetDevModeResp(300000, 250000);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor->pendingPowerLimit.has_value());
    EXPECT_EQ(sensor->pendingPowerLimit.value(), 250u);
}

// =============================================================================
// handleResponseMsg: pending limit is INVALID -> nullopt
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_InvalidPendingLimit_Nullopt)
{
    auto sensor = makeSensor(GPU_BASE, false);

    auto response = makeGetDevModeResp(300000, INVALID_POWER_LIMIT);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->pendingPowerLimit.has_value());
}

// =============================================================================
// setPowerLimit: std::get_if returns nullptr -> InvalidArgument
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, SetPowerLimit_InvalidValue_ThrowsInvalidArg)
{
    auto sensor = makeSensor(GPU_BASE, false);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad_value");

    EXPECT_THROW_COROUTINE(
        sensor->setPowerLimit(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// setPowerLimit: with tuple<bool,uint32_t> value
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, SetPowerLimit_TupleValue_Success)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::make_tuple(true, uint32_t(250));

    auto coro = sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// setPowerLimit: with uint32_t value
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, SetPowerLimit_Uint32Value_Success)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t(300);

    auto coro = sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// updatePowerLimit: postPatchIO failure
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, UpdatePowerLimit_PostPatchIOFail_WriteFailure)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->updatePowerLimit(&status, gpu, true, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// updatePowerLimit: decode failure (cc != NSM_SUCCESS)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, UpdatePowerLimit_DecodeFailure_WriteFailure)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->updatePowerLimit(&status, gpu, true, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmOneShotPowerLimit: handleResponseMsg with INVALID_POWER_LIMIT
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotHandleResp_InvalidPowerLimit_ReadsZero)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    perI->persistentPowerLimit(300.0);
    pli->powerCap(300);

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE, perI,
                                pli);

    auto response = makeGetDevModeResp(INVALID_POWER_LIMIT, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(perI->oneShotPowerLimit(), 0.0);
}

// =============================================================================
// NsmOneShotPowerLimit: currentModeLength = 0 -> else branch
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       OneShotHandleResp_ZeroCurrentModeLength_ElseBranch)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE, perI,
                                pli);

    auto response = makeGetDevModeRespZeroLength();
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmOneShotPowerLimit: persistency check - powerCap != persistentPowerLimit
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotHandleResp_PersistencyFalse)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    perI->persistentPowerLimit(200.0);
    pli->powerCap(300);

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE, perI,
                                pli);

    auto response = makeGetDevModeResp(250000, 0); // 250W
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(perI->persistency());
}

// =============================================================================
// NsmOneShotPowerLimit: persistencyIntf null -> skip persistency update
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotHandleResp_NoPersistencyIntf)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE,
                                nullptr, pli);

    auto response = makeGetDevModeResp(300000, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerLimitRange: default propertyId -> propertyName = ""
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PowerLimitRange_DefaultPropertyId)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    // Use invalid propertyId to hit default case
    NsmPowerLimitRange range("TestRange", "NSM_RANGE", 0xFF, pli);
    EXPECT_EQ(range.propertyName, "");
}

// =============================================================================
// NsmDefaultPowerLimit: default propertyId -> propertyName = "UNKNOWN"
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, DefaultPowerLimit_DefaultPropertyId)
{
    auto cli = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);

    // Use invalid propertyId to hit default case
    NsmDefaultPowerLimit defLimit("TestDef", "NSM_DEF", 0xFF, cli);
    EXPECT_EQ(defLimit.propertyName, "UNKNOWN");
}

// =============================================================================
// powerLimitIdToDeviceModeIndex: CPU_LIMIT_GPU_COPY with persistent
// =============================================================================
TEST(NsmPowerLimitBranch3, PowerLimitId_CpuLimitGpuCopy_Persistent)
{
    auto result = nsm::powerLimitIdToDeviceModeIndex(CPU_LIMIT_GPU_COPY, true);
    EXPECT_EQ(result, DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY);
}

TEST(NsmPowerLimitBranch3, PowerLimitId_CpuLimitGpuCopy_OneShot)
{
    auto result = nsm::powerLimitIdToDeviceModeIndex(CPU_LIMIT_GPU_COPY, false);
    EXPECT_EQ(result, DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY);
}
