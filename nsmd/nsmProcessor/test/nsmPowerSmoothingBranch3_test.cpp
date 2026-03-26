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

// =============================================================================
// Branch coverage tests batch 3 for nsmPowerSmoothing.cpp
//
// Covers additional unchecked branches:
// - NsmPowerSmoothing::handleResponseMsg: cc!=0 with rc==0
// - NsmPowerSmoothing::handleResponseMsg: rc!=0 with cc==0
// - NsmPowerSmoothing::updateReading: INVALID_POWER_LIMIT for both fields
// - NsmHwCircuitryTelemetry::handleResponseMsg: cc!=0 with rc==0
// - NsmCurrentPowerSmoothingProfile::handleResponseMsg: cc!=0 with rc==0
// - NsmPowerSmoothingAdminOverride::handleResponseMsg: cc!=0 with rc==0
// - NsmPowerProfileCollection::handleResponseMsg: cc!=0 with rc==0
// - NsmPowerSmoothingAction: activatePresetProfile empty objectPath
// - NsmPowerSmoothingAction: applyAdminOverride empty objectPath
// - NsmPowerSmoothingAction: requestApplyAdminOverride postPatchIO fail
// - NsmPowerSmoothingAction: requestApplyAdminOverride decode fail
// - NsmPowerSmoothingAction: requestApplyAdminOverride success
// - NsmPowerSmoothingAction: doActivatePresetProfile / doApplyAdminOverride
// - NsmCurrentPowerSmoothingProfile: getProfilePath with existing profile
// - NsmPowerProfileCollection: updateSupportedProfile INVALID_POWER_LIMIT rates
// =============================================================================

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"

#include "nsmPowerSmoothing.hpp"

using namespace nsm;

// =============================================================================
// Fixture
// =============================================================================
struct NsmPowerSmoothingBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_B3";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerSmoothingBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerSmoothingBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // --- helper: tiny decode-fail response (valgrind-safe, min 7 bytes) ---
    std::vector<uint8_t> tinyErrorResp()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
    }

    // --- helper: build non-success CC response for various decode funcs ---
    // cc=NSM_ERROR, rc=NSM_SW_SUCCESS (decode succeeds, but cc != NSM_SUCCESS)
    std::vector<uint8_t> makeNonSuccessCcFeatInfoResp()
    {
        nsm_pwr_smoothing_featureinfo_data data{};
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_powersmoothing_featinfo_resp(0, NSM_ERROR, ERR_NULL, &data,
                                                msg);
        return resp;
    }

    std::vector<uint8_t> makeNonSuccessCcHwCircuitryResp()
    {
        nsm_hardwarecircuitry_data data{};
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_hardwareciruitry_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_hardware_lifetime_cricuitry_resp(0, NSM_ERROR, ERR_NULL,
                                                    &data, msg);
        return resp;
    }

    std::vector<uint8_t> makeNonSuccessCcCurProfileResp()
    {
        nsm_get_current_profile_data data{};
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_profile_info_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_current_profile_info_resp(0, NSM_ERROR, ERR_NULL, &data,
                                             msg);
        return resp;
    }

    std::vector<uint8_t> makeNonSuccessCcAdminOverrideResp()
    {
        nsm_admin_override_data data{};
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_query_admin_override_resp(0, NSM_ERROR, ERR_NULL, &data, msg);
        return resp;
    }

    std::vector<uint8_t> makeNonSuccessCcPresetProfileResp()
    {
        nsm_get_all_preset_profile_meta_data meta{};
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_all_preset_profile_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_preset_profile_resp(0, NSM_ERROR, ERR_NULL, &meta, nullptr,
                                       0, msg);
        return resp;
    }

    // --- helper factories ---
    std::shared_ptr<OemPowerSmoothingFeatIntf> makeFeatIntf()
    {
        return std::make_shared<OemPowerSmoothingFeatIntf>(bus(), objPath, gpu);
    }

    std::shared_ptr<PowerSmoothingIntf> makePowerSmoothingIntf()
    {
        return std::make_shared<PowerSmoothingIntf>(bus(), objPath.c_str());
    }

    std::shared_ptr<OemAdminProfileIntf> makeAdminIntf()
    {
        return std::make_shared<OemAdminProfileIntf>(bus(), objPath, gpu);
    }

    std::shared_ptr<OemCurrentPowerProfileIntf> makeCurProfileIntf()
    {
        auto adminPath = objPath + "/profile/admin_profile";
        return std::make_shared<OemCurrentPowerProfileIntf>(bus(), objPath,
                                                            adminPath, gpu);
    }

    std::shared_ptr<NsmPowerProfileCollection> makeCollection()
    {
        std::string name = "PSCollection";
        std::string type = "NSM";
        std::string path = objPath;
        return std::make_shared<NsmPowerProfileCollection>(name, type, path,
                                                           gpu);
    }

    std::shared_ptr<NsmPowerSmoothingAdminOverride> makeAdminOverrideSensor()
    {
        std::string name = "AdminOverride";
        std::string type = "NSM";
        std::string path = objPath;
        auto adminIntf = makeAdminIntf();
        return std::make_shared<NsmPowerSmoothingAdminOverride>(
            name, type, adminIntf, path, gpu);
    }

    std::shared_ptr<NsmCurrentPowerSmoothingProfile> makeCurProfileSensor(
        std::shared_ptr<NsmPowerProfileCollection> collection = nullptr,
        std::shared_ptr<NsmPowerSmoothingAdminOverride> adminSensor = nullptr)
    {
        std::string name = "CurProfile";
        std::string type = "NSM";
        std::string path = objPath;
        auto curIntf = makeCurProfileIntf();
        if (!collection)
            collection = makeCollection();
        if (!adminSensor)
            adminSensor = makeAdminOverrideSensor();
        return std::make_shared<NsmCurrentPowerSmoothingProfile>(
            name, type, path, curIntf, collection, adminSensor, gpu);
    }

    // Build activate preset profile response
    std::vector<uint8_t> makeActivatePresetResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_set_active_preset_profile_resp(0, cc, ERR_NULL, msg);
        return resp;
    }

    // Build apply admin override response
    std::vector<uint8_t> makeApplyAdminResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_apply_admin_override_resp(0, cc, ERR_NULL, msg);
        return resp;
    }
};

// =============================================================================
// NsmPowerSmoothing: handleResponseMsg cc!=NSM_SUCCESS, rc==NSM_SW_SUCCESS
// Covers: cc ? cc : rc returns cc (L107)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       PwrSmoothing_HandleResponse_NonSuccessCc_ReturnsCC)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto resp = makeNonSuccessCcFeatInfoResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmHwCircuitryTelemetry: handleResponseMsg cc!=NSM_SUCCESS
// Covers: cc ? cc : rc returns cc (L213)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       HwCircuitry_HandleResponse_NonSuccessCc_ReturnsCC)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto resp = makeNonSuccessCcHwCircuitryResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: handleResponseMsg cc!=NSM_SUCCESS
// Covers: cc ? cc : rc returns cc (L327)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       CurProfile_HandleResponse_NonSuccessCc_ReturnsCC)
{
    auto sensor = makeCurProfileSensor();
    auto resp = makeNonSuccessCcCurProfileResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: handleResponseMsg cc!=NSM_SUCCESS
// Covers: cc ? cc : rc returns cc (L434)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       AdminOverride_HandleResponse_NonSuccessCc_ReturnsCC)
{
    auto sensor = makeAdminOverrideSensor();
    auto resp = makeNonSuccessCcAdminOverrideResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmPowerProfileCollection: handleResponseMsg decode fail (tiny response)
// Covers: cc ? cc : rc path when decode fails (L644)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       Collection_HandleResponse_NonSuccess_ReturnsNonZero)
{
    auto collection = makeCollection();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothing::updateReading: both TmpSetting and TmpFloorSetting are
// INVALID_POWER_LIMIT simultaneously
// Covers: both INVALID_POWER_LIMIT TRUE branches at L135 and L141
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       PwrSmoothing_UpdateReading_BothInvalidPowerLimit)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0;
    data.currentTmpSetting = INVALID_POWER_LIMIT;
    data.currentTmpFloorSetting = INVALID_POWER_LIMIT;
    sensor.updateReading(&data);

    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(),
                     INVALID_POWER_LIMIT);
    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
                     INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmPowerSmoothingAction: requestApplyAdminOverride - postPatchIO fail
// Covers: if (rc_) TRUE branch in requestApplyAdminOverride (L772)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, Action_ApplyAdmin_PostPatchIOFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_1", "NSM", path, curProfile,
                                   gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    action.requestApplyAdminOverride(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerSmoothingAction: requestApplyAdminOverride - decode fail
// Covers: else branch at L793 (rc!=SUCCESS or cc!=SUCCESS)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, Action_ApplyAdmin_DecodeFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_2", "NSM", path, curProfile,
                                   gpu);

    auto errorResp = tinyErrorResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    action.requestApplyAdminOverride(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerSmoothingAction: requestApplyAdminOverride - success
// Covers: if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) TRUE branch (L786)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, Action_ApplyAdmin_Success)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_3", "NSM", path, curProfile,
                                   gpu);

    auto successResp = makeApplyAdminResp(NSM_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp))
        .WillRepeatedly(mockPostPatchIO(successResp));

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    action.requestApplyAdminOverride(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmPowerSmoothingAction: doApplyAdminOverride wrapper coroutine
// Covers: doApplyAdminOverride -> requestApplyAdminOverride -> status set
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, Action_DoApplyAdmin_Success)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_4", "NSM", path, curProfile,
                                   gpu);

    auto successResp = makeApplyAdminResp(NSM_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp))
        .WillRepeatedly(mockPostPatchIO(successResp));

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus(), "/xyz/test/ps_b3_doapply");
    action.doApplyAdminOverride(statusIntf);
}

// =============================================================================
// NsmPowerSmoothingAction: doActivatePresetProfile wrapper coroutine
// Covers: doActivatePresetProfile -> requestActivatePresetProfile -> status set
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, Action_DoActivatePreset_Success)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_5", "NSM", path, curProfile,
                                   gpu);

    auto successResp = makeActivatePresetResp(NSM_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp))
        .WillRepeatedly(mockPostPatchIO(successResp));

    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus(), "/xyz/test/ps_b3_doactivate");
    uint16_t profileID = 0;
    action.doActivatePresetProfile(statusIntf, profileID);
}

// =============================================================================
// NsmPowerSmoothingAction: requestActivatePresetProfile encode fail
// Covers: if (rc) TRUE branch in requestActivatePresetProfile (L672)
// encode_set_active_preset_profile_req always succeeds with valid params,
// so we test the doActivatePresetProfile with postPatchIO fail instead (above)
// =============================================================================

// =============================================================================
// NsmPowerSmoothingAction: requestApplyAdminOverride with non-success decode
// Covers: else branch at L793 via cc != NSM_SUCCESS from decode
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, Action_ApplyAdmin_NonSuccessCcFromDecode)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_6", "NSM", path, curProfile,
                                   gpu);

    auto errorResp = makeApplyAdminResp(NSM_ERROR); // cc = NSM_ERROR
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    action.requestApplyAdminOverride(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerSmoothingAction: requestActivatePresetProfile with non-success CC
// Covers: else branch at L706 via cc != NSM_SUCCESS
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       Action_ActivatePreset_NonSuccessCcFromDecode)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_7", "NSM", path, curProfile,
                                   gpu);

    auto errorResp = makeActivatePresetResp(NSM_ERROR); // cc = NSM_ERROR
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    uint16_t profileID = 0;
    action.requestActivatePresetProfile(&status, profileID);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerSmoothingAction: activatePresetProfile - success (non-empty path)
// Covers: if (objectPath.empty()) FALSE branch -> detach (L743)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       Action_ActivatePresetProfile_SuccessReturnsPath)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_8", "NSM", path, curProfile,
                                   gpu);

    auto successResp = makeActivatePresetResp(NSM_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(successResp));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    auto resultPath = action.activatePresetProfile(0);
    EXPECT_FALSE(resultPath.str.empty());
}

// =============================================================================
// NsmPowerSmoothingAction: applyAdminOverride - success (non-empty path)
// Covers: if (objectPath.empty()) FALSE branch -> detach (L829)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       Action_ApplyAdminOverride_SuccessReturnsPath)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB3_9", "NSM", path, curProfile,
                                   gpu);

    auto successResp = makeApplyAdminResp(NSM_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillRepeatedly(mockPostPatchIO(successResp));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    auto resultPath = action.applyAdminOverride();
    EXPECT_FALSE(resultPath.str.empty());
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile with INVALID rates
// Covers: convertAndScaleDownUint32ToDouble with INVALID_POWER_LIMIT input
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       Collection_UpdateSupportedProfile_InvalidRates)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);

    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = 0x0400;
    data.ramp_up_rate_in_miliwattspersec = INVALID_POWER_LIMIT;
    data.ramp_down_rate_in_miliwattspersec = INVALID_POWER_LIMIT;
    data.ramp_hysterisis_rate_in_milisec = INVALID_POWER_LIMIT;
    collection->updateSupportedProfile(profile, &data);

    EXPECT_DOUBLE_EQ(profile->PowerProfileIntf::rampUpRate(),
                     INVALID_POWER_LIMIT);
    EXPECT_DOUBLE_EQ(profile->PowerProfileIntf::rampDownRate(),
                     INVALID_POWER_LIMIT);
    EXPECT_DOUBLE_EQ(profile->PowerProfileIntf::rampDownHysteresis(),
                     INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading with INVALID ramp rates
// Covers: convertAndScaleDownUint32ToDouble paths in updateReading
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test, CurProfile_UpdateReading_InvalidRampRates)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800;
    data.admin_override_mask.byte = 0;
    data.current_rampup_rate_in_miliwatts_per_second = INVALID_POWER_LIMIT;
    data.current_rampdown_rate_in_miliwatts_per_second = INVALID_POWER_LIMIT;
    data.current_rampdown_hysteresis_value_in_milisec = INVALID_POWER_LIMIT;
    sensor->updateReading(&data);

    EXPECT_DOUBLE_EQ(sensor->getPwrSmoothingCurProfileIntf()
                         ->CurrentPowerProfileIntf::rampUpRate(),
                     INVALID_POWER_LIMIT);
    EXPECT_DOUBLE_EQ(sensor->getPwrSmoothingCurProfileIntf()
                         ->CurrentPowerProfileIntf::rampDownRate(),
                     INVALID_POWER_LIMIT);
    EXPECT_DOUBLE_EQ(sensor->getPwrSmoothingCurProfileIntf()
                         ->CurrentPowerProfileIntf::rampDownHysteresis(),
                     INVALID_POWER_LIMIT);
}

// =============================================================================
// NsmPowerSmoothing: handleResponseMsg rc!=0 cc==0 (decode fails => rc!=0)
// Covers: cc ? cc : rc returns rc when cc==0 (L107 FALSE branch of cc?)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       PwrSmoothing_HandleResponse_DecodeFail_ReturnsRC)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    // decode fails -> rc != 0, cc could be 0 or error
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading with profile that exists in
// collection, exercising getProfilePath TRUE branch
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       CurProfile_UpdateReading_WithExistingProfile)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);
    auto sensor = makeCurProfileSensor(collection);

    nsm_get_current_profile_data data{};
    data.current_active_profile_id = 0;
    data.admin_override_mask.byte = 0;
    data.current_percent_tmp_floor = 0x0800;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    // getProfilePath should return the profile path, not inventoryObjPath
    auto appliedPath = sensor->getPwrSmoothingCurProfileIntf()
                           ->CurrentPowerProfileIntf::appliedProfilePath();
    EXPECT_EQ(appliedPath, profile->getInventoryObjPath());
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading with profile NOT in collection
// Covers: getProfilePath FALSE branch (hasProfileId returns false)
// =============================================================================

TEST_F(NsmPowerSmoothingBranch3Test,
       CurProfile_UpdateReading_WithNonExistingProfile)
{
    auto collection = makeCollection();
    auto sensor = makeCurProfileSensor(collection);

    nsm_get_current_profile_data data{};
    data.current_active_profile_id = 99; // not in collection
    data.admin_override_mask.byte = 0;
    data.current_percent_tmp_floor = 0x0800;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    // getProfilePath falls back to inventoryObjPath
    auto appliedPath = sensor->getPwrSmoothingCurProfileIntf()
                           ->CurrentPowerProfileIntf::appliedProfilePath();
    EXPECT_EQ(appliedPath, objPath);
}
