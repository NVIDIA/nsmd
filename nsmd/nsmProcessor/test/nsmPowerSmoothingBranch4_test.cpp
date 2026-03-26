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
// Branch coverage tests batch 4 for nsmPowerSmoothing.cpp
//
// Targets remaining unchecked branches:
// - genRequestMsg: encode fail path (instanceId > NSM_INSTANCE_MAX)
// - handleResponseMsg: success path with valid data (opposite of fail)
// - updateReading: nullptr data (NULL guard)
// - updateReading: valid TmpSetting (NOT INVALID_POWER_LIMIT) for both fields
// - updateReading: feature_flag bits TRUE vs FALSE
// - NsmPowerSmoothingAdminOverride: updateReading nullptr
// - NsmPowerSmoothingAdminOverride: updateReading valid + INVALID tmpFloor
// - NsmPowerSmoothingAdminOverride: updateReading valid tmpFloor (non-INVALID)
// - NsmCurrentPowerSmoothingProfile: updateReading nullptr
// - NsmCurrentPowerSmoothingProfile: updateReading INVALID tmpFloor
// - NsmCurrentPowerSmoothingProfile: updateReading valid tmpFloor
// - NsmCurrentPowerSmoothingProfile: admin_override_mask bits all set
// - NsmPowerProfileCollection: getSupportedProfileById not found (throws)
// - NsmPowerProfileCollection: getProfilePathByProfileId not found
// - NsmPowerProfileCollection: updateSupportedProfile with nullptr obj
// - NsmPowerProfileCollection: updateSupportedProfile INVALID tmpFloor
// - NsmPowerProfileCollection: updateSupportedProfile valid tmpFloor
// - NsmPowerSmoothing: updateReading feature_flag all bits set
// - NsmPowerSmoothing: updateReading feature_flag all bits clear
// - NsmPowerSmoothing: updateReading valid (not INVALID) TmpSetting
// - NsmPowerSmoothing: updateReading valid (not INVALID) TmpFloorSetting
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

#undef private
#undef protected

using namespace nsm;

// =============================================================================
// Fixture
// =============================================================================
struct NsmPowerSmoothingBranch4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_B4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerSmoothingBranch4Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerSmoothingBranch4Test()
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

    // --- helper: build success feat info response ---
    std::vector<uint8_t> makeSuccessFeatInfoResp(uint8_t featureFlag = 0x07,
                                                 uint32_t tmpSetting = 5000,
                                                 uint32_t tmpFloor = 3000)
    {
        nsm_pwr_smoothing_featureinfo_data data{};
        data.feature_flag = featureFlag;
        data.currentTmpSetting = tmpSetting;
        data.currentTmpFloorSetting = tmpFloor;
        data.maxTmpFloorSettingInPercent = 0x0C00; // 75%
        data.minTmpFloorSettingInPercent = 0x0400; // 25%
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_powersmoothing_featinfo_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                                msg);
        return resp;
    }

    // --- helper: build success hw circuitry response ---
    std::vector<uint8_t> makeSuccessHwCircuitryResp()
    {
        nsm_hardwarecircuitry_data data{};
        data.reading = 0x01000000; // some fixed-point value
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_hardwareciruitry_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_hardware_lifetime_cricuitry_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &data, msg);
        return resp;
    }

    // --- helper: build success current profile response ---
    std::vector<uint8_t> makeSuccessCurProfileResp(uint16_t tmpFloor = 0x0800,
                                                   uint8_t overrideMask = 0xFF)
    {
        nsm_get_current_profile_data data{};
        data.current_percent_tmp_floor = tmpFloor;
        data.admin_override_mask.byte = overrideMask;
        data.current_rampup_rate_in_miliwatts_per_second = 5000;
        data.current_rampdown_rate_in_miliwatts_per_second = 3000;
        data.current_rampdown_hysteresis_value_in_milisec = 2000;
        data.current_active_profile_id = 0;
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_profile_info_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_current_profile_info_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                             msg);
        return resp;
    }

    // --- helper: build success admin override response ---
    std::vector<uint8_t>
        makeSuccessAdminOverrideResp(uint16_t tmpFloor = 0x0800)
    {
        nsm_admin_override_data data{};
        data.admin_override_percent_tmp_floor = tmpFloor;
        data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
        data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
        data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_query_admin_override_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);
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
};

// =============================================================================
// NsmPowerSmoothing: genRequestMsg encode fail (instanceId > NSM_INSTANCE_MAX)
// Covers: if (rc != NSM_SW_SUCCESS) TRUE branch at L78
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_GenReq_EncodeFail)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto result = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_EQ(result, std::nullopt);
}

// =============================================================================
// NsmPowerSmoothing: genRequestMsg encode success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE branch at L78
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_GenReq_Success)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto result = sensor.genRequestMsg(0, 0);
    EXPECT_NE(result, std::nullopt);
    EXPECT_GT(result->size(), 0u);
}

// =============================================================================
// NsmPowerSmoothing: handleResponseMsg success path
// Covers: if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) TRUE at L103
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_HandleResp_Success)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto resp = makeSuccessFeatInfoResp(0x07, 5000, 3000);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify feature flags decoded
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// =============================================================================
// NsmPowerSmoothing: updateReading nullptr data
// Covers: if (data == nullptr) TRUE at L113
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_UpdateReading_Nullptr)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    sensor.updateReading(nullptr);
    // No crash, no change to interface values
}

// =============================================================================
// NsmPowerSmoothing: updateReading valid TmpSetting (not INVALID_POWER_LIMIT)
// Covers: (currentTmpSetting == INVALID_POWER_LIMIT) FALSE at L135
// And: (currentTmpFloorSetting == INVALID_POWER_LIMIT) FALSE at L141
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_UpdateReading_ValidTmp)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0;
    data.currentTmpSetting = 5000;      // 5W
    data.currentTmpFloorSetting = 3000; // 3W
    sensor.updateReading(&data);

    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(), 5.0);
    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
                     3.0);
}

// =============================================================================
// NsmPowerSmoothing: updateReading feature_flag all bits set
// Covers: TRUE branches for bit0, bit1, bit2 at L121, L125, L129
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_UpdateReading_AllBitsSet)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x07; // bits 0,1,2 set
    data.currentTmpSetting = 1000;
    data.currentTmpFloorSetting = 1000;
    sensor.updateReading(&data);

    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// =============================================================================
// NsmPowerSmoothing: updateReading feature_flag all bits clear
// Covers: FALSE branches for bit0, bit1, bit2 at L121, L125, L129
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_UpdateReading_AllBitsClear)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x00; // no bits set
    data.currentTmpSetting = 1000;
    data.currentTmpFloorSetting = 1000;
    sensor.updateReading(&data);

    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// =============================================================================
// NsmHwCircuitryTelemetry: genRequestMsg encode fail
// Covers: if (rc != NSM_SW_SUCCESS) TRUE at L184
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, HwCircuitry_GenReq_EncodeFail)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto result = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_EQ(result, std::nullopt);
}

// =============================================================================
// NsmHwCircuitryTelemetry: genRequestMsg encode success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE at L184
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, HwCircuitry_GenReq_Success)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto result = sensor.genRequestMsg(0, 0);
    EXPECT_NE(result, std::nullopt);
}

// =============================================================================
// NsmHwCircuitryTelemetry: handleResponseMsg success
// Covers: if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) TRUE at L209
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, HwCircuitry_HandleResp_Success)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto resp = makeSuccessHwCircuitryResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmHwCircuitryTelemetry: updateReading nullptr
// Covers: if (data == nullptr) TRUE at L219
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, HwCircuitry_UpdateReading_Nullptr)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    sensor.updateReading(nullptr);
    // No crash
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: genRequestMsg encode fail
// Covers: if (rc != NSM_SW_SUCCESS) TRUE at L298
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_GenReq_EncodeFail)
{
    auto sensor = makeCurProfileSensor();
    auto result = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_EQ(result, std::nullopt);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: genRequestMsg success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE at L298
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_GenReq_Success)
{
    auto sensor = makeCurProfileSensor();
    auto result = sensor->genRequestMsg(0, 0);
    EXPECT_NE(result, std::nullopt);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: handleResponseMsg success
// Covers: if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) TRUE at L323
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_HandleResp_Success)
{
    auto sensor = makeCurProfileSensor();
    auto resp = makeSuccessCurProfileResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading nullptr
// Covers: if (data == nullptr) TRUE at L344
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_UpdateReading_Nullptr)
{
    auto sensor = makeCurProfileSensor();
    sensor->updateReading(nullptr);
    // No crash
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading INVALID tmpFloor
// Covers: if (data->current_percent_tmp_floor == INVALID_UINT16_VALUE) TRUE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_UpdateReading_InvalidTmpFloor)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = INVALID_UINT16_VALUE;
    data.admin_override_mask.byte = 0;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_EQ(sensor->getPwrSmoothingCurProfileIntf()
                  ->CurrentPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading valid tmpFloor
// Covers: (data->current_percent_tmp_floor == INVALID_UINT16_VALUE) FALSE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_UpdateReading_ValidTmpFloor)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800;
    data.admin_override_mask.byte = 0xFF; // all bits set
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_NE(sensor->getPwrSmoothingCurProfileIntf()
                  ->CurrentPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);

    // Verify admin override mask bits are all true
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::tmpFloorPercentApplied());
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::rampUpRateApplied());
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::rampDownRateApplied());
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::rampDownHysteresisApplied());
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: admin_override_mask all clear
// Covers: admin_override_mask bits FALSE at L364, L370, L376, L382
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, CurProfile_UpdateReading_OverrideMaskClear)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800;
    data.admin_override_mask.byte = 0x00; // all bits clear
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::tmpFloorPercentApplied());
    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::rampUpRateApplied());
    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::rampDownRateApplied());
    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::rampDownHysteresisApplied());
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: genRequestMsg encode fail
// Covers: if (rc != NSM_SW_SUCCESS) TRUE at L405
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, AdminOverride_GenReq_EncodeFail)
{
    auto sensor = makeAdminOverrideSensor();
    auto result = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_EQ(result, std::nullopt);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: genRequestMsg success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE at L405
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, AdminOverride_GenReq_Success)
{
    auto sensor = makeAdminOverrideSensor();
    auto result = sensor->genRequestMsg(0, 0);
    EXPECT_NE(result, std::nullopt);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: handleResponseMsg success
// Covers: if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) TRUE at L430
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, AdminOverride_HandleResp_Success)
{
    auto sensor = makeAdminOverrideSensor();
    auto resp = makeSuccessAdminOverrideResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: updateReading nullptr
// Covers: if (data == nullptr) TRUE at L439
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, AdminOverride_UpdateReading_Nullptr)
{
    auto sensor = makeAdminOverrideSensor();
    sensor->updateReading(nullptr);
    // No crash
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: updateReading INVALID tmpFloor
// Covers: if (admin_override_percent_tmp_floor == INVALID_UINT16_VALUE) TRUE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test,
       AdminOverride_UpdateReading_InvalidTmpFloor)
{
    auto sensor = makeAdminOverrideSensor();
    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = INVALID_UINT16_VALUE;
    data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_EQ(
        sensor->getAdminProfileIntf()->AdminPowerProfileIntf::tmpFloorPercent(),
        INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: updateReading valid tmpFloor
// Covers: (admin_override_percent_tmp_floor == INVALID_UINT16_VALUE) FALSE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, AdminOverride_UpdateReading_ValidTmpFloor)
{
    auto sensor = makeAdminOverrideSensor();
    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = 0x0800;
    data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_NE(
        sensor->getAdminProfileIntf()->AdminPowerProfileIntf::tmpFloorPercent(),
        INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerProfileCollection: genRequestMsg encode fail
// Covers: if (rc != NSM_SW_SUCCESS) TRUE at L557
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_GenReq_EncodeFail)
{
    auto collection = makeCollection();
    auto result = collection->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_EQ(result, std::nullopt);
}

// =============================================================================
// NsmPowerProfileCollection: genRequestMsg success
// Covers: if (rc != NSM_SW_SUCCESS) FALSE at L557
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_GenReq_Success)
{
    auto collection = makeCollection();
    auto result = collection->genRequestMsg(0, 0);
    EXPECT_NE(result, std::nullopt);
}

// =============================================================================
// NsmPowerProfileCollection: getSupportedProfileById not found (throws)
// Covers: if (it != supportedPowerProfiles.end()) FALSE at L497
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_GetProfileById_NotFound)
{
    auto collection = makeCollection();
    EXPECT_THROW(collection->getSupportedProfileById(99), std::out_of_range);
}

// =============================================================================
// NsmPowerProfileCollection: getSupportedProfileById found
// Covers: if (it != supportedPowerProfiles.end()) TRUE at L497
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_GetProfileById_Found)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);
    auto result = collection->getSupportedProfileById(0);
    EXPECT_EQ(result, profile);
}

// =============================================================================
// NsmPowerProfileCollection: getProfilePathByProfileId not found
// Covers: if (hasProfileId(profileId)) FALSE at L546
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_GetPathByProfileId_NotFound)
{
    auto collection = makeCollection();
    auto path = collection->getProfilePathByProfileId(99);
    EXPECT_EQ(path, "/");
}

// =============================================================================
// NsmPowerProfileCollection: getProfilePathByProfileId found
// Covers: if (hasProfileId(profileId)) TRUE at L546
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_GetPathByProfileId_Found)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);
    auto path = collection->getProfilePathByProfileId(0);
    EXPECT_EQ(path, profile->getInventoryObjPath());
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile with nullptr obj
// Covers: if (obj) FALSE at L517
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_UpdateSupportedProfile_NullObj)
{
    auto collection = makeCollection();
    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = 0x0800;
    collection->updateSupportedProfile(nullptr, &data);
    // No crash, no-op
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile INVALID tmpFloor
// Covers: if (tmp_floor_setting_in_percent == INVALID_UINT16_VALUE) TRUE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test,
       Collection_UpdateSupportedProfile_InvalidTmpFloor)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = INVALID_UINT16_VALUE;
    data.ramp_up_rate_in_miliwattspersec = 5000;
    data.ramp_down_rate_in_miliwattspersec = 3000;
    data.ramp_hysterisis_rate_in_milisec = 2000;
    collection->updateSupportedProfile(profile, &data);

    EXPECT_EQ(profile->PowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile valid tmpFloor
// Covers: (tmp_floor_setting_in_percent == INVALID_UINT16_VALUE) FALSE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test,
       Collection_UpdateSupportedProfile_ValidTmpFloor)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = 0x0800;
    data.ramp_up_rate_in_miliwattspersec = 5000;
    data.ramp_down_rate_in_miliwattspersec = 3000;
    data.ramp_hysterisis_rate_in_milisec = 2000;
    collection->updateSupportedProfile(profile, &data);

    EXPECT_NE(profile->PowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: getInventoryObjPath
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, AdminOverride_GetInventoryObjPath)
{
    auto sensor = makeAdminOverrideSensor();
    EXPECT_EQ(sensor->getInventoryObjPath(), objPath);
}

// =============================================================================
// NsmPowerProfileCollection: hasProfileId TRUE/FALSE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Collection_HasProfileId)
{
    auto collection = makeCollection();
    EXPECT_FALSE(collection->hasProfileId(0));

    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);
    EXPECT_TRUE(collection->hasProfileId(0));
    EXPECT_FALSE(collection->hasProfileId(1));
}

// =============================================================================
// NsmPowerSmoothing: handleResponseMsg with valid data, cc=0, rc=0
// Return value should be 0 (success) -> cc ? cc : rc returns rc=0
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, PwrSmoothing_HandleResp_SuccessZeroReturn)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto resp = makeSuccessFeatInfoResp(0x00, 1000, 1000);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, 0);
}

// =============================================================================
// NsmPowerSmoothingAction: requestActivatePresetProfile postPatchIO fail
// Covers: if (rc_) TRUE in requestActivatePresetProfile at L685
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Action_ActivatePreset_PostPatchIOFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB4_1", "NSM", path, curProfile,
                                   gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    uint16_t profileID = 0;
    action.requestActivatePresetProfile(&status, profileID);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerSmoothingAction: requestActivatePresetProfile success
// Covers: if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) TRUE at L699
// =============================================================================
TEST_F(NsmPowerSmoothingBranch4Test, Action_ActivatePreset_Success)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "ActionB4_2", "NSM", path, curProfile,
                                   gpu);

    // Build a valid activate preset response
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_active_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, msg);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(resp))
        .WillRepeatedly(mockPostPatchIO(resp));
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    uint16_t profileID = 0;
    action.requestActivatePresetProfile(&status, profileID);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}
