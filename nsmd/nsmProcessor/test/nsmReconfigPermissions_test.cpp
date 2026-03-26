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
 * Tests for nsmd/nsmProcessor/nsmReconfigPermissions.cpp
 *
 *   - NsmReconfigPermissions::NsmReconfigPermissions (constructor)
 *   - NsmReconfigPermissions::getIndex (all feature→index mappings,
 *     plus invalid feature throws)
 *   - NsmReconfigPermissions::genRequestMsg (success)
 *   - NsmReconfigPermissions::handleResponseMsg (success, all 6 booleans)
 *   - NsmReconfigPermissions::handleResponseMsg (error CC)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmReconfigPermissions.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// Helper: build a fully constructed NsmReconfigPermissions
static std::shared_ptr<NsmReconfigPermissions>
    makeReconfigSensor(const std::string& basePath,
                       std::shared_ptr<ReconfigSettingsIntf>& outHost,
                       std::shared_ptr<ReconfigSettingsIntf>& outDoe,
                       std::string& hostPath, std::string& doePath,
                       ReconfigSettingsIntf::FeatureType feature =
                           ReconfigSettingsIntf::FeatureType::CCMode)
{
    hostPath = basePath + "/host";
    doePath = basePath + "/doe";
    outHost = std::make_shared<ReconfigSettingsIntf>(testBus, hostPath.c_str());
    outDoe = std::make_shared<ReconfigSettingsIntf>(testBus, doePath.c_str());
    return std::make_shared<NsmReconfigPermissions>(
        "reconfig_test", "NSM_ReconfigPermissions", hostPath, doePath, feature,
        outHost, outDoe);
}

// =============================================================================
// Constructor
// =============================================================================

TEST(NsmReconfigPermissions, Constructor_CreatesObject)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;

    auto sensor = makeReconfigSensor("/test/reconfig/ctor", hostIntf, doeIntf,
                                     hostPath, doePath);

    EXPECT_EQ(sensor->getName(), "reconfig_test");
    EXPECT_EQ(sensor->getType(), "NSM_ReconfigPermissions");
    EXPECT_EQ(sensor->feature, ReconfigSettingsIntf::FeatureType::CCMode);
    EXPECT_EQ(sensor->index, RP_CONFIDENTIAL_COMPUTE);
}

// =============================================================================
// getIndex – mapping each FeatureType to the correct index
// =============================================================================

TEST(NsmReconfigPermissions, GetIndex_InSystemTest_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::InSystemTest),
              RP_IN_SYSTEM_TEST);
}

TEST(NsmReconfigPermissions, GetIndex_FusingMode_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::FusingMode),
              RP_FUSING_MODE);
}

TEST(NsmReconfigPermissions, GetIndex_CCMode_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::CCMode),
              RP_CONFIDENTIAL_COMPUTE);
}

TEST(NsmReconfigPermissions, GetIndex_BAR0Firewall_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::BAR0Firewall),
              RP_BAR0_FIREWALL);
}

TEST(NsmReconfigPermissions, GetIndex_NVLinkDisable_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::NVLinkDisable),
              RP_NVLINK_DISABLE);
}

TEST(NsmReconfigPermissions, GetIndex_ECCEnable_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::ECCEnable),
              RP_ECC_ENABLE);
}

TEST(NsmReconfigPermissions, GetIndex_HBMFrequencyChange_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::HBMFrequencyChange),
              RP_HBM_FREQUENCY_CHANGE);
}

TEST(NsmReconfigPermissions, GetIndex_PowerSmoothingLevel1_ReturnsCorrectIndex)
{
    EXPECT_EQ(
        NsmReconfigPermissions::getIndex(
            ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel1),
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1);
}

TEST(NsmReconfigPermissions, GetIndex_PowerSmoothingLevel2_ReturnsCorrectIndex)
{
    EXPECT_EQ(
        NsmReconfigPermissions::getIndex(
            ReconfigSettingsIntf::FeatureType::PowerSmoothingPrivilegeLevel2),
        RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2);
}

TEST(NsmReconfigPermissions, GetIndex_EGMMode_ReturnsCorrectIndex)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::EGMMode),
              RP_EGM_MODE);
}

TEST(NsmReconfigPermissions, GetIndex_InvalidFeature_Throws)
{
    // Cast an out-of-range int to FeatureType to trigger the default case
    auto invalidFeature = static_cast<ReconfigSettingsIntf::FeatureType>(0xFF);
    EXPECT_THROW(NsmReconfigPermissions::getIndex(invalidFeature),
                 std::invalid_argument);
}

// =============================================================================
// genRequestMsg
// =============================================================================

TEST(NsmReconfigPermissions, GenRequestMsg_ReturnsValidRequest)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/genreq", hostIntf, doeIntf,
                                     hostPath, doePath);

    auto request = sensor->genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_reconfiguration_permissions_v1_req));
}

TEST(NsmReconfigPermissions, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/badreq", hostIntf, doeIntf,
                                     hostPath, doePath);
    auto request = sensor->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// handleResponseMsg – success: all six booleans updated
// =============================================================================

TEST(NsmReconfigPermissions, HandleResponseMsg_Success_UpdatesAllPermissions)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/handle_ok", hostIntf,
                                     doeIntf, hostPath, doePath);

    nsm_reconfiguration_permissions_v1 data = {};
    data.host_oneshot = 1;
    data.host_persistent = 0;
    data.host_flr_persistent = 1;
    data.DOE_oneshot = 0;
    data.DOE_persistent = 1;
    data.DOE_flr_persistent = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_reconfiguration_permissions_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(hostIntf->allowOneShotConfig(), true);
    EXPECT_EQ(hostIntf->allowPersistentConfig(), false);
    EXPECT_EQ(hostIntf->allowFLRPersistentConfig(), true);
    EXPECT_EQ(doeIntf->allowOneShotConfig(), false);
    EXPECT_EQ(doeIntf->allowPersistentConfig(), true);
    EXPECT_EQ(doeIntf->allowFLRPersistentConfig(), false);
}

// =============================================================================
// handleResponseMsg – error CC: returns error
// =============================================================================

TEST(NsmReconfigPermissions, HandleResponseMsg_ErrorCC_ReturnsError)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/handle_err", hostIntf,
                                     doeIntf, hostPath, doePath);

    nsm_reconfiguration_permissions_v1 data = {};
    data.host_oneshot = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_reconfiguration_permissions_v1_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// handleResponseMsg – decode failure: too-short buffer triggers length error
// =============================================================================

TEST(NsmReconfigPermissions, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/handle_decode_fail",
                                     hostIntf, doeIntf, hostPath, doePath);

    // 7-byte buffer: safely reads cc at payload[1] (=0=NSM_SUCCESS) but
    // too short for nsm_common_resp, so decode_common_resp returns
    // NSM_SW_ERROR_LENGTH without any out-of-bounds access
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    auto rc = sensor->handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// setAllowPermission – coroutine branch coverage
//
// cc is initialised to NSM_ERROR (non-zero) in the coroutine.
//   FALSE branch (cc==0): success response → decode sets cc=NSM_SUCCESS=0
//                         co_return 0 ? 0 : rc → returns rc //
//
//   TRUE  branch (cc!=0): error-CC response → decode sets cc=NSM_ERROR!=0
//                         co_return cc ? cc : rc → returns cc //
//
// =============================================================================

struct NsmReconfigPermissionsSetTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmReconfigPermissionsSetTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmReconfigPermissionsSetTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// setAllowPermission – success response → FALSE branch of co_return cc ? cc :
// rc (cc = NSM_SUCCESS = 0 after decode).
TEST_F(NsmReconfigPermissionsSetTest,
       SetAllowPermission_Success_CoversFalseBranch)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/set_succ", hostIntf,
                                     doeIntf, hostPath, doePath);

    std::vector<uint8_t> successResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(successResp.data());
    ASSERT_EQ(encode_set_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS,
                                                             ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setAllowPermission(RP_ONESHOOT_HOT_RESET, 1, status,
                                           gpu);

    // cc = NSM_SUCCESS → false branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setAllowPermission – error-CC response → TRUE branch of co_return cc ? cc :
// rc (cc = NSM_ERROR != 0 after decode).
TEST_F(NsmReconfigPermissionsSetTest,
       SetAllowPermission_ErrorCC_CoversTrueBranch)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/set_err", hostIntf,
                                     doeIntf, hostPath, doePath);

    std::vector<uint8_t> errorResp(256, 0);
    auto respPtr = reinterpret_cast<nsm_msg*>(errorResp.data());
    ASSERT_EQ(encode_set_reconfiguration_permissions_v1_resp(0, NSM_ERROR,
                                                             ERR_NULL, respPtr),
              NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setAllowPermission(RP_ONESHOOT_HOT_RESET, 1, status,
                                           gpu);

    // cc = NSM_ERROR → WriteFailure; true branch of co_return cc ? cc : rc //

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// patch*Config – branch coverage for all 6 patch methods
//
// Each method has two inner if/else branches:
//   1. if (*allowValue)   → ALLOW_HOST_*
//   2. else               → DISALLOW_HOST_*
// Each branch further checks the other side's config for ALLOW/DISALLOW DOE.
//
// We exercise all 4 combinations for patchHostOneShotConfig, and 2 each for the
// rest.
// =============================================================================

// Helper: build a success response for set_reconfiguration_permissions_v1
static std::vector<uint8_t> makeSetReconfigSuccessResp()
{
    std::vector<uint8_t> resp(256, 0);
    auto ptr = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS, ERR_NULL,
                                                   ptr);
    return resp;
}

// --- patchHostOneShotConfig ---

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostOneShot_TrueValue_DoeAllows_PermAllowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phos1", hostIntf, doeIntf,
                                     hostPath, doePath);
    // doeConfig->allowOneShotConfig() = true
    doeIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchHostOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostOneShot_TrueValue_DoeDisallows_PermAllowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phos2", hostIntf, doeIntf,
                                     hostPath, doePath);
    // doeConfig->allowOneShotConfig() = false
    doeIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchHostOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostOneShot_FalseValue_DoeAllows_PermDisallowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phos3", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchHostOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostOneShot_FalseValue_DoeDisallows_PermDisallowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phos4", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchHostOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEOneShotConfig ---

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeOneShot_TrueValue_HostAllows_PermAllowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdos1", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchDOEOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeOneShot_FalseValue_HostDisallows_PermDisallowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdos2", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchDOEOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeOneShot_TrueValue_HostDisallows_PermDisallowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdos3", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchDOEOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeOneShot_FalseValue_HostAllows_PermAllowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdos4", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchDOEOneShotConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostPersistentConfig ---

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostPersistent_TrueValue_DoeAllows_PermAllowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phpc1", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchHostPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostPersistent_FalseValue_DoeDisallows_PermDisallowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phpc2", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchHostPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostPersistent_TrueValue_DoeDisallows_PermAllowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phpc3", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchHostPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostPersistent_FalseValue_DoeAllows_PermDisallowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phpc4", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchHostPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEPersistentConfig ---

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoePersistent_TrueValue_HostAllows_PermAllowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdpc1", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchDOEPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoePersistent_FalseValue_HostDisallows_PermDisallowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdpc2", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchDOEPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoePersistent_TrueValue_HostDisallows_PermDisallowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdpc3", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchDOEPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoePersistent_FalseValue_HostAllows_PermAllowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdpc4", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchDOEPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostFLRPersistentConfig ---

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostFLR_TrueValue_DoeAllows_PermAllowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phflr1", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowFLRPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchHostFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostFLR_FalseValue_DoeDisallows_PermDisallowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phflr2", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowFLRPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchHostFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostFLR_TrueValue_DoeDisallows_PermAllowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phflr3", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowFLRPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchHostFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostFLR_FalseValue_DoeAllows_PermDisallowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phflr4", hostIntf, doeIntf,
                                     hostPath, doePath);
    doeIntf->allowFLRPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchHostFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEFLRPersistentConfig ---

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeFLR_TrueValue_HostAllows_PermAllowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdflr1", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowFLRPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchDOEFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeFLR_FalseValue_HostDisallows_PermDisallowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdflr2", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowFLRPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchDOEFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeFLR_TrueValue_HostDisallows_PermDisallowHostAllowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdflr3", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowFLRPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->patchDOEFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeFLR_FalseValue_HostAllows_PermAllowHostDisallowDoe)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdflr4", hostIntf, doeIntf,
                                     hostPath, doePath);
    hostIntf->allowFLRPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    auto coro = sensor->patchDOEFLRPersistentConfig(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// patch*Config – non-bool value → !allowValue TRUE → throws InvalidArgument
// Covers the TRUE branch of if (!allowValue) in all 6 patch functions.
// =============================================================================

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostOneShot_NonBoolValue_ThrowsInvalidArgument)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phos_nonbool", hostIntf,
                                     doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad");

    EXPECT_THROW_COROUTINE(
        sensor->patchHostOneShotConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeOneShot_NonBoolValue_ThrowsInvalidArgument)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdos_nonbool", hostIntf,
                                     doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad");

    EXPECT_THROW_COROUTINE(
        sensor->patchDOEOneShotConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostPersistent_NonBoolValue_ThrowsInvalidArgument)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phpc_nonbool", hostIntf,
                                     doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad");

    EXPECT_THROW_COROUTINE(
        sensor->patchHostPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoePersistent_NonBoolValue_ThrowsInvalidArgument)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdpc_nonbool", hostIntf,
                                     doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad");

    EXPECT_THROW_COROUTINE(
        sensor->patchDOEPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchHostFLR_NonBoolValue_ThrowsInvalidArgument)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/phflr_nonbool", hostIntf,
                                     doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad");

    EXPECT_THROW_COROUTINE(
        sensor->patchHostFLRPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(NsmReconfigPermissionsSetTest,
       PatchDoeFLR_NonBoolValue_ThrowsInvalidArgument)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/pdflr_nonbool", hostIntf,
                                     doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("bad");

    EXPECT_THROW_COROUTINE(
        sensor->patchDOEFLRPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// setAllowPermission – postPatchIO fails → if (rc) TRUE → WriteFailure
// =============================================================================

TEST_F(NsmReconfigPermissionsSetTest,
       SetAllowPermission_PostPatchIOFails_SetsWriteFailure)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/set_patchfail", hostIntf,
                                     doeIntf, hostPath, doePath);

    // postPatchIO returns NSM_ERROR (non-zero rc) → if (rc) TRUE
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigSuccessResp(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    auto coro = sensor->setAllowPermission(RP_ONESHOOT_HOT_RESET, 1, status,
                                           gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// genRequestMsg – encode failure: instanceNumber > NSM_INSTANCE_MAX
// → if (rc != NSM_SW_SUCCESS) TRUE → returns std::nullopt
// =============================================================================

TEST(NsmReconfigPermissions, GenRequestMsg_EncodeFailure_ReturnsNullopt)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/genreq_fail", hostIntf,
                                     doeIntf, hostPath, doePath);

    // instanceNumber > NSM_INSTANCE_MAX causes encode to fail
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch →
// permissions NOT updated (9-byte buffer pattern).
TEST(NsmReconfigPermissions,
     HandleResponseMsg_DecodeSuccessNonZeroCC_SkipsUpdate)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor("/test/reconfig/ccbranch", hostIntf,
                                     doeIntf, hostPath, doePath);

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());

    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
