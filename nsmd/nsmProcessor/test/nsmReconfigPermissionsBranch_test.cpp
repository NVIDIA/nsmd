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
 * Additional branch coverage tests for nsmReconfigPermissions.cpp
 *
 * Covers:
 * - NsmReconfigPermissions::genRequestMsg encode fail (instanceId=255)
 * - NsmReconfigPermissions::handleResponseMsg success / errorCC / decodeFail
 * - setAllowPermission: encode fail, postPatchIO fail, decode fail, success
 * - setAllowPermission: decode success with cc==0 (FALSE ternary branch)
 * - All remaining getIndex feature type mappings
 * - patch* coroutine invalid type throws
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

static auto& testBus2 = utils::DBusHandler::getBus();

// Helper: build a fully constructed NsmReconfigPermissions
static std::shared_ptr<NsmReconfigPermissions>
    makeReconfigSensor2(const std::string& basePath,
                        std::shared_ptr<ReconfigSettingsIntf>& outHost,
                        std::shared_ptr<ReconfigSettingsIntf>& outDoe,
                        std::string& hostPath, std::string& doePath,
                        ReconfigSettingsIntf::FeatureType feature =
                            ReconfigSettingsIntf::FeatureType::CCMode)
{
    hostPath = basePath + "/host";
    doePath = basePath + "/doe";
    outHost = std::make_shared<ReconfigSettingsIntf>(testBus2,
                                                     hostPath.c_str());
    outDoe = std::make_shared<ReconfigSettingsIntf>(testBus2, doePath.c_str());
    return std::make_shared<NsmReconfigPermissions>(
        "reconfig_br", "NSM_ReconfigPermissions", hostPath, doePath, feature,
        outHost, outDoe);
}

// Helper: build a set_reconfiguration_permissions_v1 response
static std::vector<uint8_t> makeSetReconfigResp2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> resp(256, 0);
    auto ptr = reinterpret_cast<nsm_msg*>(resp.data());
    [[maybe_unused]] auto rc =
        encode_set_reconfiguration_permissions_v1_resp(0, cc, ERR_NULL, ptr);
    return resp;
}

// ============================================================================
// Fixture
// ============================================================================

struct ReconfigBranchTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    ReconfigBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_CALL(mockManager, getEid(testing::_))
            .WillRepeatedly(testing::Return(0x40));
    }

    ~ReconfigBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// getIndex - remaining feature types not covered by existing tests
// ============================================================================

TEST(NsmReconfigPermissionsBranch, GetIndex_CCDevMode)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::CCDevMode),
              RP_CONFIDENTIAL_COMPUTE_DEV_MODE);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_TGPCurrentLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPCurrentLimit),
              RP_TOTAL_GPU_POWER_CURRENT_LIMIT);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_TGPRatedLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPRatedLimit),
              RP_TOTAL_GPU_POWER_RATED_LIMIT);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_TGPMaxLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPMaxLimit),
              RP_TOTAL_GPU_POWER_MAX_LIMIT);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_TGPMinLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::TGPMinLimit),
              RP_TOTAL_GPU_POWER_MIN_LIMIT);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_ClockLimit)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::ClockLimit),
              RP_CLOCK_LIMIT);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_PCIeVFConfiguration)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::PCIeVFConfiguration),
              RP_PCIE_VF_CONFIGURATION);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_RowRemappingAllowed)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::RowRemappingAllowed),
              RP_ROW_REMAPPING_ALLOWED);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_RowRemappingFeature)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::RowRemappingFeature),
              RP_ROW_REMAPPING_FEATURE);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_HULKLicenseUpdate)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::HULKLicenseUpdate),
              RP_HULK_LICENSE_UPDATE);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_ForceTestCoupling)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::ForceTestCoupling),
              RP_FORCE_TEST_COUPLING);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_BAR0TypeConfig)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::BAR0TypeConfig),
              RP_BAR0_TYPE_CONFIG);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_EDPpScalingFactor)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::EDPpScalingFactor),
              RP_EDPP_SCALING_FACTOR);
}

TEST(NsmReconfigPermissionsBranch, GetIndex_InfoROMFileSystemRecreate)
{
    EXPECT_EQ(NsmReconfigPermissions::getIndex(
                  ReconfigSettingsIntf::FeatureType::InfoROMFileSystemRecreate),
              RP_INFOROM_RECREATE_ALLOW_INB);
}

// ============================================================================
// genRequestMsg - encode fail with instanceId=255
// ============================================================================

TEST(NsmReconfigPermissionsBranch, GenRequestMsg_EncodeFail_InstanceId255)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_genreq_fail255",
                                      hostIntf, doeIntf, hostPath, doePath);
    auto request = sensor->genRequestMsg(0, 255);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// genRequestMsg - success with valid instanceId
// ============================================================================

TEST(NsmReconfigPermissionsBranch, GenRequestMsg_Success_ValidInstance)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_genreq_ok", hostIntf,
                                      doeIntf, hostPath, doePath);
    auto request = sensor->genRequestMsg(0x10, 0);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_reconfiguration_permissions_v1_req));
}

// ============================================================================
// handleResponseMsg - success updates all permissions
// ============================================================================

TEST(NsmReconfigPermissionsBranch, HandleResponseMsg_Success_UpdatesAll)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_handle_ok", hostIntf,
                                      doeIntf, hostPath, doePath);

    nsm_reconfiguration_permissions_v1 data = {};
    data.host_oneshot = 0;
    data.host_persistent = 1;
    data.host_flr_persistent = 0;
    data.DOE_oneshot = 1;
    data.DOE_persistent = 0;
    data.DOE_flr_persistent = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    [[maybe_unused]] auto enc = encode_get_reconfiguration_permissions_v1_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);

    auto rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(hostIntf->allowOneShotConfig(), false);
    EXPECT_EQ(hostIntf->allowPersistentConfig(), true);
    EXPECT_EQ(hostIntf->allowFLRPersistentConfig(), false);
    EXPECT_EQ(doeIntf->allowOneShotConfig(), true);
    EXPECT_EQ(doeIntf->allowPersistentConfig(), false);
    EXPECT_EQ(doeIntf->allowFLRPersistentConfig(), true);
}

// ============================================================================
// handleResponseMsg - error CC
// ============================================================================

TEST(NsmReconfigPermissionsBranch, HandleResponseMsg_ErrorCC_ReturnsCc)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_handle_cc", hostIntf,
                                      doeIntf, hostPath, doePath);

    nsm_reconfiguration_permissions_v1 data = {};
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_get_reconfiguration_permissions_v1_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    [[maybe_unused]] auto enc = encode_get_reconfiguration_permissions_v1_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);

    auto rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg - decode fail (tiny buffer)
// ============================================================================

TEST(NsmReconfigPermissionsBranch, HandleResponseMsg_DecodeFail_TinyBuffer)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_handle_dec_fail",
                                      hostIntf, doeIntf, hostPath, doePath);

    // Minimum safe buffer size for valgrind
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = sensor->handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// setAllowPermission - postPatchIO fails
// ============================================================================

TEST_F(ReconfigBranchTest, SetAllowPermission_PostPatchIOFails_WriteFailure)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_set_ppfail", hostIntf,
                                      doeIntf, hostPath, doePath);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    [[maybe_unused]] auto coro = sensor->setAllowPermission(RP_PERSISTENT, 1,
                                                            status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setAllowPermission - decode fail (corrupt response)
// ============================================================================

TEST_F(ReconfigBranchTest, SetAllowPermission_DecodeFail_WriteFailure)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_set_decfail", hostIntf,
                                      doeIntf, hostPath, doePath);

    // Very small response that causes decode to fail with non-success CC
    std::vector<uint8_t> badResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    badResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(badResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    [[maybe_unused]] auto coro = sensor->setAllowPermission(RP_PERSISTENT, 1,
                                                            status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// setAllowPermission - success (cc==NSM_SUCCESS) FALSE ternary branch
// ============================================================================

TEST_F(ReconfigBranchTest, SetAllowPermission_Success_FalseTernaryBranch)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_set_ok", hostIntf,
                                      doeIntf, hostPath, doePath);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    [[maybe_unused]] auto coro = sensor->setAllowPermission(RP_PERSISTENT, 1,
                                                            status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// setAllowPermission - error CC TRUE ternary branch
// ============================================================================

TEST_F(ReconfigBranchTest, SetAllowPermission_ErrorCC_TrueTernaryBranch)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_set_cc_err", hostIntf,
                                      doeIntf, hostPath, doePath);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    [[maybe_unused]] auto coro = sensor->setAllowPermission(RP_ONESHOT_FLR, 1,
                                                            status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// patch*Config - invalid type throws
// These cover the if (!allowValue) TRUE branch for all 6 patch methods
// ============================================================================

TEST_F(ReconfigBranchTest, PatchHostOneShot_InvalidType_Throws)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phos_inv", hostIntf,
                                      doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint64_t(42);
    EXPECT_THROW_COROUTINE(
        sensor->patchHostOneShotConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(ReconfigBranchTest, PatchDOEOneShot_InvalidType_Throws)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdos_inv", hostIntf,
                                      doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint64_t(42);
    EXPECT_THROW_COROUTINE(
        sensor->patchDOEOneShotConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(ReconfigBranchTest, PatchHostPersistent_InvalidType_Throws)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phpc_inv", hostIntf,
                                      doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint64_t(42);
    EXPECT_THROW_COROUTINE(
        sensor->patchHostPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(ReconfigBranchTest, PatchDOEPersistent_InvalidType_Throws)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdpc_inv", hostIntf,
                                      doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint64_t(42);
    EXPECT_THROW_COROUTINE(
        sensor->patchDOEPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(ReconfigBranchTest, PatchHostFLR_InvalidType_Throws)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phflr_inv", hostIntf,
                                      doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint64_t(42);
    EXPECT_THROW_COROUTINE(
        sensor->patchHostFLRPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(ReconfigBranchTest, PatchDOEFLR_InvalidType_Throws)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdflr_inv", hostIntf,
                                      doeIntf, hostPath, doePath);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint64_t(42);
    EXPECT_THROW_COROUTINE(
        sensor->patchDOEFLRPersistentConfig(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// patch*Config - all 4 permission combos for remaining methods
// These cover both the true/false branch of if(*allowValue) and the
// inner if/else for the counterpart config
// ============================================================================

// --- patchHostOneShotConfig: true + doe true ---
TEST_F(ReconfigBranchTest, PatchHostOneShot_True_DoeTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phos_tt", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro = sensor->patchHostOneShotConfig(value, &status,
                                                                gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostOneShotConfig: true + doe false ---
TEST_F(ReconfigBranchTest, PatchHostOneShot_True_DoeFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phos_tf", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro = sensor->patchHostOneShotConfig(value, &status,
                                                                gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostOneShotConfig: false + doe true ---
TEST_F(ReconfigBranchTest, PatchHostOneShot_False_DoeTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phos_ft", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro = sensor->patchHostOneShotConfig(value, &status,
                                                                gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostOneShotConfig: false + doe false ---
TEST_F(ReconfigBranchTest, PatchHostOneShot_False_DoeFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phos_ff", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro = sensor->patchHostOneShotConfig(value, &status,
                                                                gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEOneShotConfig: true + host true ---
TEST_F(ReconfigBranchTest, PatchDOEOneShot_True_HostTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdos_tt", hostIntf,
                                      doeIntf, hostPath, doePath);
    hostIntf->allowOneShotConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro = sensor->patchDOEOneShotConfig(value, &status,
                                                               gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEOneShotConfig: false + host false ---
TEST_F(ReconfigBranchTest, PatchDOEOneShot_False_HostFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdos_ff", hostIntf,
                                      doeIntf, hostPath, doePath);
    hostIntf->allowOneShotConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro = sensor->patchDOEOneShotConfig(value, &status,
                                                               gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostPersistentConfig: true + doe true ---
TEST_F(ReconfigBranchTest, PatchHostPersistent_True_DoeTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phpc_tt", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro =
        sensor->patchHostPersistentConfig(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostPersistentConfig: false + doe false ---
TEST_F(ReconfigBranchTest, PatchHostPersistent_False_DoeFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phpc_ff", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro =
        sensor->patchHostPersistentConfig(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEPersistentConfig: true + host true ---
TEST_F(ReconfigBranchTest, PatchDOEPersistent_True_HostTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdpc_tt", hostIntf,
                                      doeIntf, hostPath, doePath);
    hostIntf->allowPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro = sensor->patchDOEPersistentConfig(value,
                                                                  &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEPersistentConfig: false + host false ---
TEST_F(ReconfigBranchTest, PatchDOEPersistent_False_HostFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdpc_ff", hostIntf,
                                      doeIntf, hostPath, doePath);
    hostIntf->allowPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro = sensor->patchDOEPersistentConfig(value,
                                                                  &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostFLRPersistentConfig: true + doe true ---
TEST_F(ReconfigBranchTest, PatchHostFLR_True_DoeTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phflr_tt", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowFLRPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro =
        sensor->patchHostFLRPersistentConfig(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchHostFLRPersistentConfig: false + doe false ---
TEST_F(ReconfigBranchTest, PatchHostFLR_False_DoeFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_phflr_ff", hostIntf,
                                      doeIntf, hostPath, doePath);
    doeIntf->allowFLRPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro =
        sensor->patchHostFLRPersistentConfig(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEFLRPersistentConfig: true + host true ---
TEST_F(ReconfigBranchTest, PatchDOEFLR_True_HostTrue)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdflr_tt", hostIntf,
                                      doeIntf, hostPath, doePath);
    hostIntf->allowFLRPersistentConfig(true);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    [[maybe_unused]] auto coro =
        sensor->patchDOEFLRPersistentConfig(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// --- patchDOEFLRPersistentConfig: false + host false ---
TEST_F(ReconfigBranchTest, PatchDOEFLR_False_HostFalse)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2("/test/reconfig/br_pdflr_ff", hostIntf,
                                      doeIntf, hostPath, doePath);
    hostIntf->allowFLRPersistentConfig(false);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetReconfigResp2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    [[maybe_unused]] auto coro =
        sensor->patchDOEFLRPersistentConfig(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// ============================================================================
// Constructor with different feature types - exercises getIndex branches
// ============================================================================

TEST(NsmReconfigPermissionsBranch, Constructor_InSystemTest)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2(
        "/test/reconfig/br_ctor_ist", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::InSystemTest);
    EXPECT_EQ(sensor->index, RP_IN_SYSTEM_TEST);
}

TEST(NsmReconfigPermissionsBranch, Constructor_EGMMode)
{
    std::shared_ptr<ReconfigSettingsIntf> hostIntf, doeIntf;
    std::string hostPath, doePath;
    auto sensor = makeReconfigSensor2(
        "/test/reconfig/br_ctor_egm", hostIntf, doeIntf, hostPath, doePath,
        ReconfigSettingsIntf::FeatureType::EGMMode);
    EXPECT_EQ(sensor->index, RP_EGM_MODE);
}
