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

#include <cmath>

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

    // current 300W, pending 250W → powerCap(300) != pending(250)
    auto response = makeGetDevModeResp(300000, 250000);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(pli->powerCap(), 300u);
    EXPECT_DOUBLE_EQ(perI->persistentPowerLimit(), 250.0);
    // pending (250W) != current powerCap (300W) → persistency false
    EXPECT_FALSE(perI->persistency());
}

// =============================================================================
// handleResponseMsg: pending limit is valid (non-INVALID_POWER_LIMIT) →
// persistentPowerLimit holds the pending value (in Watts)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_ValidPendingLimit_PersistentSet)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    std::shared_ptr<PowerPersistencyIntf> perI;
    auto sensor = makeSensor(GPU_BASE, true, &pli, &perI);

    auto response = makeGetDevModeResp(300000, 250000);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(perI->persistentPowerLimit(), 250.0);
}

// =============================================================================
// handleResponseMsg: pending limit is INVALID -> persistentPowerLimit=nan,
// persistency=false
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PersistentHandleResp_InvalidPendingLimit_Nan)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    std::shared_ptr<PowerPersistencyIntf> perI;
    auto sensor = makeSensor(GPU_BASE, true, &pli, &perI);
    perI->persistentPowerLimit(300.0);
    perI->persistency(true);

    auto response = makeGetDevModeResp(300000, INVALID_POWER_LIMIT);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(std::isnan(perI->persistentPowerLimit()));
    EXPECT_FALSE(perI->persistency());
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
// NsmOneShotPowerLimit: oneShotPowerLimit comes from pending; persistency is
// not modified by this handler
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotHandleResp_OneShotFromPendingValue)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    perI->persistentPowerLimit(200.0);
    perI->persistency(false);
    pli->powerCap(300);

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE, perI,
                                pli);

    auto response = makeGetDevModeResp(250000, 150000); // pending 150W
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(perI->oneShotPowerLimit(), 150.0);
    // one-shot handler does not write persistency
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
    auto capIntf = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    // Use invalid propertyId to hit default case
    NsmDefaultPowerLimit defLimit("TestDef", "NSM_DEF", 0xFF, capIntf);
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

// =============================================================================
// Batch 4 - Additional branch coverage tests
// =============================================================================

// =============================================================================
// NsmPersistentPowerLimit constructor: verifies powerCapEnable(true) is called
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PersistentCtor_PowerCapEnableTrue)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    auto sensor = makeSensor(GPU_BASE, false, &pli);
    EXPECT_TRUE(pli->powerCapEnable());
}

// =============================================================================
// NsmPersistentPowerLimit::genRequestMsg: success path with GPU_BASE
// Verify the returned request has a value and correct size
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PersistentGenRequestMsg_GPUBase_Success)
{
    auto sensor = makeSensor(GPU_BASE, false);
    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) +
                                   sizeof(nsm_get_device_mode_settings_v2_req));
}

// =============================================================================
// NsmPersistentPowerLimit::handleResponseMsg: normal current limit (non-zero,
// non-INVALID) without persistencyIntf -> powerCap is set, no persistency
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_NormalLimit_NoPersistencyIntf)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    auto sensor = makeSensor(GPU_BASE, false, &pli);

    auto response = makeGetDevModeResp(500000, 500000); // 500W
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(pli->powerCap(), 500u);
}

// =============================================================================
// NsmPersistentPowerLimit::handleResponseMsg: cc ? cc : rc when cc=NSM_ERROR
// Returns cc (non-zero) when both decode fails
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PersistentHandleResp_CcNonZero_ReturnsCc)
{
    auto sensor = makeSensor(GPU_BASE, false);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_v2_resp(0, NSM_ERROR, ERR_NULL, nullptr, 0,
                                            nullptr, 0, msg);

    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmOneShotPowerLimit::handleResponseMsg: cc ? cc : rc when cc=NSM_ERROR
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotHandleResp_CcNonZero_ReturnsCc)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE, perI,
                                pli);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_device_mode_settings_v2_resp(0, NSM_ERROR, ERR_NULL, nullptr, 0,
                                            nullptr, 0, msg);

    auto rc = sensor.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmOneShotPowerLimit::genRequestMsg: success path with CPU_LIMIT_GPU_COPY
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotGenRequestMsg_CpuLimitGpuCopy_Success)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT",
                                CPU_LIMIT_GPU_COPY, perI, pli);

    auto request = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

// =============================================================================
// NsmOneShotPowerLimit::handleResponseMsg with CPU_LIMIT_GPU_COPY:
// oneShotPowerLimit is sourced from pending; persistency is not written here.
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       OneShotHandleResp_CpuLimitGpuCopy_OneShotFromPending)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    pli->powerCap(400);
    perI->persistentPowerLimit(400.0);
    perI->persistency(false);

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT",
                                CPU_LIMIT_GPU_COPY, perI, pli);

    auto response = makeGetDevModeResp(400000, 400000); // pending 400W
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(perI->oneShotPowerLimit(), 400.0);
    // one-shot handler does not write persistency
    EXPECT_FALSE(perI->persistency());
}

// =============================================================================
// NsmPowerLimitRange constructor: MINIMUM_GPU_BASE_POWER_LIMIT propertyName
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PowerLimitRange_MinPropertyId_PropertyName)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmPowerLimitRange range("TestRange", "NSM_RANGE",
                             MINIMUM_GPU_BASE_POWER_LIMIT, pli);
    EXPECT_EQ(range.propertyName, "MINIMUM_GPU_BASE_POWER_LIMIT");
}

// =============================================================================
// NsmPowerLimitRange constructor: MAXIMUM_GPU_BASE_POWER_LIMIT propertyName
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PowerLimitRange_MaxPropertyId_PropertyName)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmPowerLimitRange range("TestRange", "NSM_RANGE",
                             MAXIMUM_GPU_BASE_POWER_LIMIT, pli);
    EXPECT_EQ(range.propertyName, "MAXIMUM_GPU_BASE_POWER_LIMIT");
}

// =============================================================================
// NsmDefaultPowerLimit constructor: RATED_GPU_BASE_POWER_LIMIT propertyName
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, DefaultPowerLimit_RatedPropertyId_PropertyName)
{
    auto capIntf = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());

    NsmDefaultPowerLimit defLimit("TestDef", "NSM_DEF",
                                  RATED_GPU_BASE_POWER_LIMIT, capIntf);
    EXPECT_EQ(defLimit.propertyName, "RATED_GPU_BASE_POWER_LIMIT");
}

// =============================================================================
// NsmPersistentPowerLimit::handleOfflineState: GPU_BASE (default) - no change
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, HandleOfflineState_GPUBase_NoChange)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    auto sensor = makeSensor(GPU_BASE, false, &pli);
    pli->powerCap(350);
    sensor->handleOfflineState();
    EXPECT_EQ(pli->powerCap(), 350u);
}

// =============================================================================
// NsmPersistentPowerLimit::handleOfflineState: CPU_LIMIT_GPU_COPY sets invalid
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, HandleOfflineState_CpuLimit_SetsInvalid)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false, &pli);
    pli->powerCap(350);
    sensor->handleOfflineState();
    EXPECT_EQ(pli->powerCap(), INVALID_POWER_LIMIT);
}

// =============================================================================
// updatePowerLimit: success path (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)
// Verify status is Success
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, UpdatePowerLimit_Success_StatusSuccess)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    auto coro = sensor->updatePowerLimit(&status, gpu, true, 300);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// updatePowerLimit: with CPU_LIMIT_GPU_COPY and persistent=false
// Covers ONE_SHOT path in deviceModeIndexToName map lookup
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, UpdatePowerLimit_CpuLimitOneShot_Success)
{
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    auto coro = sensor->updatePowerLimit(&status, gpu, false, 200);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// updatePowerLimit: cc != NSM_SUCCESS in decode response
// cc ? cc : rc returns cc
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, UpdatePowerLimit_CcError_ReturnsWriteFailure)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->updatePowerLimit(&status, gpu, true, 250);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerLimitRange::update: encode failure (bad propertyId causing encode
// failure is unlikely but we can test the sensorIO failure with non-zero rc)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PowerLimitRange_Update_SensorIOFails_ReturnsError)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    NsmPowerLimitRange sensor("TestRange", "NSM_RANGE",
                              MAXIMUM_GPU_BASE_POWER_LIMIT, pli);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmDefaultPowerLimit::update: sensorIO failure path
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       DefaultPowerLimit_Update_SensorIOFails_ReturnsError)
{
    auto capIntf = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    NsmDefaultPowerLimit sensor("TestDef", "NSM_DEF",
                                RATED_GPU_BASE_POWER_LIMIT, capIntf);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillOnce(mockSensorIO(NSM_ERR_UNSUPPORTED_COMMAND_CODE));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmDefaultPowerLimit::update: success with INVALID_POWER_LIMIT value
// -> defaultPowerCap = 0
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       DefaultPowerLimit_Update_InvalidValue_ReadsZero)
{
    auto capIntf = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    NsmDefaultPowerLimit sensor("TestDef", "NSM_DEF",
                                RATED_GPU_BASE_POWER_LIMIT, capIntf);

    uint32_t limitVal = htole32(INVALID_POWER_LIMIT);
    std::vector<uint8_t> limitData(sizeof(limitVal));
    std::memcpy(limitData.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData.size(), limitData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(capIntf->defaultPowerCap(), 0u);
}

// =============================================================================
// NsmPowerLimitRange::update: success with MAX property - valid value
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PowerLimitRange_Update_MaxProperty_ValidValue)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    NsmPowerLimitRange sensor("TestRange", "NSM_RANGE",
                              MAXIMUM_GPU_BASE_POWER_LIMIT, pli);

    uint32_t limitVal = htole32(600000); // 600W
    std::vector<uint8_t> limitData(sizeof(limitVal));
    std::memcpy(limitData.data(), &limitVal, sizeof(limitVal));
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN + limitData.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_SUCCESS, ERR_NULL,
                                          limitData.size(), limitData.data(),
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
    EXPECT_EQ(pli->maxPowerCapValue(), 600u);
}

// =============================================================================
// NsmPowerLimitRange::update: decode failure (cc != NSM_SUCCESS)
// cc ? cc : rc returns cc
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PowerLimitRange_Update_DecodeError_CcReturned)
{
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    NsmPowerLimitRange sensor("TestRange", "NSM_RANGE",
                              MAXIMUM_GPU_BASE_POWER_LIMIT, pli);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_ERROR, ERR_NULL, 0, nullptr,
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmDefaultPowerLimit::update: decode failure (cc != NSM_SUCCESS)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       DefaultPowerLimit_Update_DecodeError_CcReturned)
{
    auto capIntf = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    NsmDefaultPowerLimit sensor("TestDef", "NSM_DEF",
                                RATED_GPU_BASE_POWER_LIMIT, capIntf);

    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    encode_get_inventory_information_resp(0, NSM_ERROR, ERR_NULL, 0, nullptr,
                                          responseMsg);

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _)).WillOnce(mockSensorIO(response));

    EXPECT_NO_THROW(sensor.update(gpu));
}

// =============================================================================
// NsmClearPowerLimitIntf: clearPowerCap returns 0 (dummy impl)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, ClearPowerLimitIntf_Returns0)
{
    auto cli = std::make_shared<NsmClearPowerLimitIntf>(bus(), objPath);
    EXPECT_EQ(cli->clearPowerCap(), 0);
}

// =============================================================================
// NsmPersistentPowerLimit::genRequestMsg: CPU_LIMIT_GPU_COPY (persistent=true)
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, PersistentGenRequestMsg_CpuLimit_Success)
{
    auto sensor = makeSensor(CPU_LIMIT_GPU_COPY, false);
    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

// =============================================================================
// NsmPersistentPowerLimit::handleResponseMsg: valid current + pending,
// both non-INVALID, with persistencyIntf -> full path verification
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test,
       PersistentHandleResp_ValidCurrentAndPending_WithPersistency)
{
    std::shared_ptr<PowerLimitsIntf> pli;
    std::shared_ptr<PowerPersistencyIntf> perI;
    auto sensor = makeSensor(GPU_BASE, true, &pli, &perI);

    auto response = makeGetDevModeResp(400000, 500000); // 400W, 500W pending
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(pli->powerCap(), 400u);
    // persistentPowerLimit holds the pending value (500W)
    EXPECT_DOUBLE_EQ(perI->persistentPowerLimit(), 500.0);
    // pending (500W) != current powerCap (400W) → persistency false
    EXPECT_FALSE(perI->persistency());
}

// =============================================================================
// setPowerLimit: tuple with persistent=false
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, SetPowerLimit_TupleNonPersistent_Success)
{
    auto sensor = makeSensor(GPU_BASE, false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetDevModeResp(NSM_SUCCESS)));

    AsyncOperationStatusType status = AsyncOperationStatusType::WriteFailure;
    AsyncSetOperationValueType value = std::make_tuple(false, uint32_t(150));

    auto coro = sensor->setPowerLimit(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmOneShotPowerLimit: handleResponseMsg sources oneShotPowerLimit from the
// pending value; persistency is left untouched by this handler
// =============================================================================
TEST_F(NsmPowerLimitBranch3Test, OneShotHandleResp_ValidPending_OneShotSet)
{
    auto perI = std::make_shared<PowerPersistencyIntf>(bus(), objPath.c_str());
    auto pli = std::make_shared<PowerLimitsIntf>(bus(), objPath.c_str());
    pli->powerCap(250);
    perI->persistentPowerLimit(250.0);
    perI->persistency(false);

    NsmOneShotPowerLimit sensor("TestOneShot", "NSM_ONE_SHOT", GPU_BASE, perI,
                                pli);

    auto response = makeGetDevModeResp(250000, 250000); // pending 250W
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    auto rc = sensor.handleResponseMsg(responseMsg, response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(perI->oneShotPowerLimit(), 250.0);
    // one-shot handler does not write persistency
    EXPECT_FALSE(perI->persistency());
}
