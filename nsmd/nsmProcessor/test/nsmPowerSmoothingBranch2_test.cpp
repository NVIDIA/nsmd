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
// Branch coverage tests for nsmPowerSmoothing.cpp (V1):
//   NsmPowerSmoothing, NsmHwCircuitryTelemetry,
//   NsmCurrentPowerSmoothingProfile, NsmPowerSmoothingAdminOverride,
//   NsmPowerProfileCollection, NsmPowerSmoothingAction
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
struct NsmPowerSmoothingBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerSmoothingBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerSmoothingBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // --- helper: tiny decode-fail response (valgrind-safe) ---
    std::vector<uint8_t> tinyErrorResp()
    {
        std::vector<uint8_t> buf(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return buf;
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

    // Build a proper feat info success response
    std::vector<uint8_t>
        makeFeatInfoResp(nsm_pwr_smoothing_featureinfo_data& data)
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_powersmoothing_featinfo_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                                msg);
        return resp;
    }

    // Build a proper hw circuitry success response
    std::vector<uint8_t> makeHwCircuitryResp(nsm_hardwarecircuitry_data& data)
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_hardwareciruitry_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_hardware_lifetime_cricuitry_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    &data, msg);
        return resp;
    }

    // Build a proper current profile success response
    std::vector<uint8_t> makeCurProfileResp(nsm_get_current_profile_data& data)
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_profile_info_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_current_profile_info_resp(0, NSM_SUCCESS, ERR_NULL, &data,
                                             msg);
        return resp;
    }

    // Build a proper admin override success response
    std::vector<uint8_t> makeAdminOverrideResp(nsm_admin_override_data& data)
    {
        std::vector<uint8_t> resp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_query_admin_override_resp(0, NSM_SUCCESS, ERR_NULL, &data, msg);
        return resp;
    }

    // Build preset profile response with N profiles
    std::vector<uint8_t>
        makePresetProfileResp(nsm_preset_profile_data* profiles, uint8_t count)
    {
        nsm_get_all_preset_profile_meta_data meta{};
        meta.max_profiles_supported = count;
        // generous buffer: header + common_resp + metadata + profiles
        size_t bufSize = sizeof(nsm_msg_hdr) +
                         sizeof(nsm_get_all_preset_profile_resp) +
                         count * sizeof(nsm_preset_profile_data);
        std::vector<uint8_t> resp(bufSize, 0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_get_preset_profile_resp(0, NSM_SUCCESS, ERR_NULL, &meta,
                                       profiles, count, msg);
        return resp;
    }

    // Build activate preset profile success response
    std::vector<uint8_t> makeActivatePresetResp(uint8_t cc = NSM_SUCCESS)
    {
        std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
                                  0);
        auto msg = reinterpret_cast<nsm_msg*>(resp.data());
        encode_set_active_preset_profile_resp(0, cc, ERR_NULL, msg);
        return resp;
    }

    // Build apply admin override success response
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
// NsmPowerSmoothing
// =============================================================================

// genRequestMsg: success (instanceId valid)
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_GenRequestMsg_Success)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto req = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// genRequestMsg: encode fail (bad instanceId)
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_GenRequestMsg_EncodeFail)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto req = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// handleResponseMsg: decode fail (tiny response)
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_HandleResponse_DecodeFail)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: decode success -> updateReading called
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_HandleResponse_Success)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x07; // all bits set
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = 3000;
    data.maxTmpFloorSettingInPercent = 0x0C00; // ~75%
    data.minTmpFloorSettingInPercent = 0x0400; // ~25%
    auto resp = makeFeatInfoResp(data);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// updateReading: null pointer
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_Null)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    sensor.updateReading(nullptr);
    // Should just return without crashing
}

// updateReading: bit0 set, bit1/bit2 clear
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_Bit0Only)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x01; // only bit0
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = 3000;
    sensor.updateReading(&data);

    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// updateReading: bit1 set, bit0/bit2 clear
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_Bit1Only)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x02; // only bit1
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = 3000;
    sensor.updateReading(&data);

    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// updateReading: bit2 set, bit0/bit1 clear
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_Bit2Only)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x04; // only bit2
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = 3000;
    sensor.updateReading(&data);

    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// updateReading: no bits set
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_NoBits)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x00;
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = 3000;
    sensor.updateReading(&data);

    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// updateReading: all bits set
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_AllBits)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x07;
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = 3000;
    sensor.updateReading(&data);

    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// updateReading: currentTmpSetting == INVALID_POWER_LIMIT
TEST_F(NsmPowerSmoothingBranch2Test,
       PwrSmoothing_UpdateReading_InvalidTmpSetting)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0;
    data.currentTmpSetting = INVALID_POWER_LIMIT;
    data.currentTmpFloorSetting = 5000;
    sensor.updateReading(&data);

    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(),
                     INVALID_POWER_LIMIT);
    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
                     5.0);
}

// updateReading: currentTmpFloorSetting == INVALID_POWER_LIMIT
TEST_F(NsmPowerSmoothingBranch2Test,
       PwrSmoothing_UpdateReading_InvalidTmpFloorSetting)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0;
    data.currentTmpSetting = 5000;
    data.currentTmpFloorSetting = INVALID_POWER_LIMIT;
    sensor.updateReading(&data);

    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(), 5.0);
    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
                     INVALID_POWER_LIMIT);
}

// updateReading: both valid (non-INVALID)
TEST_F(NsmPowerSmoothingBranch2Test, PwrSmoothing_UpdateReading_BothValidLimits)
{
    std::string name = "PS";
    std::string type = "NSM";
    std::string path = objPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothing sensor(name, type, path, featIntf, gpu);

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0;
    data.currentTmpSetting = 10000;
    data.currentTmpFloorSetting = 8000;
    sensor.updateReading(&data);

    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(), 10.0);
    EXPECT_DOUBLE_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
                     8.0);
}

// =============================================================================
// NsmHwCircuitryTelemetry
// =============================================================================

// genRequestMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, HwCircuitry_GenRequestMsg_Success)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto req = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// genRequestMsg: encode fail
TEST_F(NsmPowerSmoothingBranch2Test, HwCircuitry_GenRequestMsg_EncodeFail)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto req = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// handleResponseMsg: decode fail
TEST_F(NsmPowerSmoothingBranch2Test, HwCircuitry_HandleResponse_DecodeFail)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, HwCircuitry_HandleResponse_Success)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    nsm_hardwarecircuitry_data data{};
    data.reading = 0x00800000; // 0.5 in UFXP8_24
    auto resp = makeHwCircuitryResp(data);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// updateReading: null pointer early return
TEST_F(NsmPowerSmoothingBranch2Test, HwCircuitry_UpdateReading_Null)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    sensor.updateReading(nullptr);
    // Should just return without crashing
}

// updateReading: valid data
TEST_F(NsmPowerSmoothingBranch2Test, HwCircuitry_UpdateReading_Valid)
{
    std::string name = "HW";
    std::string type = "NSM";
    std::string path = objPath;
    auto pwrIntf = makePowerSmoothingIntf();
    NsmHwCircuitryTelemetry sensor(name, type, path, pwrIntf);

    nsm_hardwarecircuitry_data data{};
    data.reading = 0x01000000; // 1.0 in UFXP8_24
    sensor.updateReading(&data);

    EXPECT_DOUBLE_EQ(pwrIntf->PowerSmoothingIntf::lifeTimeRemaining(), 1.0);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride
// =============================================================================

// genRequestMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_GenRequestMsg_Success)
{
    auto sensor = makeAdminOverrideSensor();
    auto req = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// genRequestMsg: encode fail
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_GenRequestMsg_EncodeFail)
{
    auto sensor = makeAdminOverrideSensor();
    auto req = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// handleResponseMsg: decode fail
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_HandleResponse_DecodeFail)
{
    auto sensor = makeAdminOverrideSensor();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_HandleResponse_Success)
{
    auto sensor = makeAdminOverrideSensor();
    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = 0x0800; // ~50%
    data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;
    auto resp = makeAdminOverrideResp(data);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// updateReading: null pointer
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_UpdateReading_Null)
{
    auto sensor = makeAdminOverrideSensor();
    sensor->updateReading(nullptr);
}

// updateReading: INVALID_UINT16_VALUE tmpFloor
TEST_F(NsmPowerSmoothingBranch2Test,
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

// updateReading: valid tmpFloor
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_UpdateReading_ValidTmpFloor)
{
    auto sensor = makeAdminOverrideSensor();
    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = 0x0800; // ~50%
    data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_NE(
        sensor->getAdminProfileIntf()->AdminPowerProfileIntf::tmpFloorPercent(),
        INVALID_UINT32_VALUE);
}

// updateReading: INVALID_POWER_LIMIT ramp rates (convertAndScaleDown)
TEST_F(NsmPowerSmoothingBranch2Test,
       AdminOverride_UpdateReading_InvalidRampRates)
{
    auto sensor = makeAdminOverrideSensor();
    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = 0x0400;
    data.admin_override_ramup_rate_in_miliwatts_per_second =
        INVALID_POWER_LIMIT;
    data.admin_override_rampdown_rate_in_miliwatts_per_second =
        INVALID_POWER_LIMIT;
    data.admin_override_rampdown_hysteresis_value_in_milisec =
        INVALID_POWER_LIMIT;
    sensor->updateReading(&data);

    EXPECT_DOUBLE_EQ(
        sensor->getAdminProfileIntf()->AdminPowerProfileIntf::rampUpRate(),
        INVALID_POWER_LIMIT);
}

// getInventoryObjPath
TEST_F(NsmPowerSmoothingBranch2Test, AdminOverride_GetInventoryObjPath)
{
    auto sensor = makeAdminOverrideSensor();
    EXPECT_EQ(sensor->getInventoryObjPath(), objPath);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile
// =============================================================================

// genRequestMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_GenRequestMsg_Success)
{
    auto sensor = makeCurProfileSensor();
    auto req = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// genRequestMsg: encode fail
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_GenRequestMsg_EncodeFail)
{
    auto sensor = makeCurProfileSensor();
    auto req = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// handleResponseMsg: decode fail
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_HandleResponse_DecodeFail)
{
    auto sensor = makeCurProfileSensor();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_HandleResponse_Success)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_active_profile_id = 0;
    data.admin_override_mask.byte = 0x0F; // all overrides
    data.current_percent_tmp_floor = 0x0800;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    auto resp = makeCurProfileResp(data);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// updateReading: null pointer
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_UpdateReading_Null)
{
    auto sensor = makeCurProfileSensor();
    sensor->updateReading(nullptr);
}

// updateReading: INVALID_UINT16_VALUE tmpFloor
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_UpdateReading_InvalidTmpFloor)
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

// updateReading: valid tmpFloor
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_UpdateReading_ValidTmpFloor)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800; // ~50%
    data.admin_override_mask.byte = 0;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_NE(sensor->getPwrSmoothingCurProfileIntf()
                  ->CurrentPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// updateReading: admin override mask bits - tmp_floor_override TRUE
TEST_F(NsmPowerSmoothingBranch2Test,
       CurProfile_UpdateReading_TmpFloorOverrideSet)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800;
    data.admin_override_mask.bits.tmp_floor_override = 1;
    data.admin_override_mask.bits.rampup_rate_override = 0;
    data.admin_override_mask.bits.rampdown_rate_override = 0;
    data.admin_override_mask.bits.hysteresis_value_override = 0;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::tmpFloorPercentApplied());
    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::rampUpRateApplied());
    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::rampDownRateApplied());
    EXPECT_FALSE(sensor->getPwrSmoothingCurProfileIntf()
                     ->CurrentPowerProfileIntf::rampDownHysteresisApplied());
}

// updateReading: admin override mask bits - all set
TEST_F(NsmPowerSmoothingBranch2Test,
       CurProfile_UpdateReading_AllOverrideBitsSet)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800;
    data.admin_override_mask.bits.tmp_floor_override = 1;
    data.admin_override_mask.bits.rampup_rate_override = 1;
    data.admin_override_mask.bits.rampdown_rate_override = 1;
    data.admin_override_mask.bits.hysteresis_value_override = 1;
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;
    sensor->updateReading(&data);

    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::tmpFloorPercentApplied());
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::rampUpRateApplied());
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::rampDownRateApplied());
    EXPECT_TRUE(sensor->getPwrSmoothingCurProfileIntf()
                    ->CurrentPowerProfileIntf::rampDownHysteresisApplied());
}

// updateReading: admin override mask bits - none set
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_UpdateReading_NoOverrideBitsSet)
{
    auto sensor = makeCurProfileSensor();
    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800;
    data.admin_override_mask.byte = 0;
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

// getProfilePath: hasProfileId TRUE
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_GetProfilePath_Found)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);
    auto sensor = makeCurProfileSensor(collection);

    auto path = sensor->getProfilePath(0);
    EXPECT_EQ(path, profile->getInventoryObjPath());
}

// getProfilePath: hasProfileId FALSE
TEST_F(NsmPowerSmoothingBranch2Test, CurProfile_GetProfilePath_NotFound)
{
    auto collection = makeCollection();
    auto sensor = makeCurProfileSensor(collection);

    auto path = sensor->getProfilePath(99);
    EXPECT_EQ(path, objPath); // falls back to inventoryObjPath
}

// =============================================================================
// NsmPowerProfileCollection
// =============================================================================

// hasProfileId: TRUE / FALSE
TEST_F(NsmPowerSmoothingBranch2Test, Collection_HasProfileId)
{
    auto collection = makeCollection();
    EXPECT_FALSE(collection->hasProfileId(0));

    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);
    EXPECT_TRUE(collection->hasProfileId(0));
    EXPECT_FALSE(collection->hasProfileId(1));
}

// getSupportedProfileById: found
TEST_F(NsmPowerSmoothingBranch2Test, Collection_GetSupportedProfileById_Found)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);

    auto result = collection->getSupportedProfileById(0);
    EXPECT_EQ(result, profile);
}

// getSupportedProfileById: not found -> throws
TEST_F(NsmPowerSmoothingBranch2Test,
       Collection_GetSupportedProfileById_NotFound)
{
    auto collection = makeCollection();
    EXPECT_THROW(collection->getSupportedProfileById(99), std::out_of_range);
}

// getProfilePathByProfileId: found and not found
TEST_F(NsmPowerSmoothingBranch2Test, Collection_GetProfilePathByProfileId)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);

    auto path = collection->getProfilePathByProfileId(0);
    EXPECT_EQ(path, profile->getInventoryObjPath());

    auto pathNotFound = collection->getProfilePathByProfileId(99);
    EXPECT_EQ(pathNotFound, "/");
}

// genRequestMsg: success
TEST_F(NsmPowerSmoothingBranch2Test, Collection_GenRequestMsg_Success)
{
    auto collection = makeCollection();
    auto req = collection->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// genRequestMsg: encode fail
TEST_F(NsmPowerSmoothingBranch2Test, Collection_GenRequestMsg_EncodeFail)
{
    auto collection = makeCollection();
    auto req = collection->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// handleResponseMsg: decode fail
TEST_F(NsmPowerSmoothingBranch2Test, Collection_HandleResponse_DecodeFail)
{
    auto collection = makeCollection();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// handleResponseMsg: success with 2 profiles (creates new + updates)
TEST_F(NsmPowerSmoothingBranch2Test, Collection_HandleResponse_Success)
{
    auto collection = makeCollection();

    nsm_preset_profile_data profiles[2]{};
    profiles[0].tmp_floor_setting_in_percent = 0x0400;
    profiles[0].ramp_up_rate_in_miliwattspersec = 5000;
    profiles[0].ramp_down_rate_in_miliwattspersec = 3000;
    profiles[0].ramp_hysterisis_rate_in_milisec = 2000;
    profiles[1].tmp_floor_setting_in_percent = INVALID_UINT16_VALUE;
    profiles[1].ramp_up_rate_in_miliwattspersec = 7000;
    profiles[1].ramp_down_rate_in_miliwattspersec = 4000;
    profiles[1].ramp_hysterisis_rate_in_milisec = 1000;

    auto resp = makePresetProfileResp(profiles, 2);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Profiles should now exist
    EXPECT_TRUE(collection->hasProfileId(0));
    EXPECT_TRUE(collection->hasProfileId(1));
}

// handleResponseMsg: success with existing profile (hasProfileId TRUE path)
TEST_F(NsmPowerSmoothingBranch2Test, Collection_HandleResponse_ExistingProfile)
{
    auto collection = makeCollection();
    auto profile = std::make_shared<OemPowerProfileIntf>(bus(), objPath, 0,
                                                         gpu);
    collection->addSupportedProfile(0, profile);

    nsm_preset_profile_data profiles[1]{};
    profiles[0].tmp_floor_setting_in_percent = 0x0400;
    profiles[0].ramp_up_rate_in_miliwattspersec = 5000;
    profiles[0].ramp_down_rate_in_miliwattspersec = 3000;
    profiles[0].ramp_hysterisis_rate_in_milisec = 2000;

    auto resp = makePresetProfileResp(profiles, 1);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

// updateSupportedProfile: INVALID_UINT16_VALUE tmpFloor
TEST_F(NsmPowerSmoothingBranch2Test,
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

// updateSupportedProfile: valid tmpFloor
TEST_F(NsmPowerSmoothingBranch2Test,
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

// updateSupportedProfile: null obj (no-op)
TEST_F(NsmPowerSmoothingBranch2Test, Collection_UpdateSupportedProfile_NullObj)
{
    auto collection = makeCollection();
    nsm_preset_profile_data data{};
    collection->updateSupportedProfile(nullptr, &data);
    // Should not crash
}

// =============================================================================
// NsmPowerSmoothingAction - requestActivatePresetProfile
// =============================================================================

// requestActivatePresetProfile: postPatchIO fail
TEST_F(NsmPowerSmoothingBranch2Test, Action_ActivatePreset_PostPatchIOFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "Action", "NSM", path, curProfile,
                                   gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    uint16_t profileID = 0;
    action.requestActivatePresetProfile(&status, profileID);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestActivatePresetProfile: decode fail (error response)
TEST_F(NsmPowerSmoothingBranch2Test, Action_ActivatePreset_DecodeFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "Action", "NSM", path, curProfile,
                                   gpu);

    auto errorResp = tinyErrorResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    uint16_t profileID = 0;
    action.requestActivatePresetProfile(&status, profileID);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestActivatePresetProfile: success
TEST_F(NsmPowerSmoothingBranch2Test, Action_ActivatePreset_Success)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "Action", "NSM", path, curProfile,
                                   gpu);

    auto successResp = makeActivatePresetResp(NSM_SUCCESS);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(successResp))
        // The update() call inside success path also does postPatchIO
        .WillRepeatedly(mockPostPatchIO(successResp));

    // update() on curProfile will call genRequestMsg + sensorIO
    // We also need to mock the sensorIO for curProfile->update(device)
    EXPECT_CALL(*gpu, sensorIO(_, _, _, _, _))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    uint16_t profileID = 0;
    action.requestActivatePresetProfile(&status, profileID);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmPowerSmoothingAction - requestApplyAdminOverride
// =============================================================================

// requestApplyAdminOverride: postPatchIO fail
TEST_F(NsmPowerSmoothingBranch2Test, Action_ApplyAdmin_PostPatchIOFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "Action2", "NSM", path, curProfile,
                                   gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    action.requestApplyAdminOverride(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestApplyAdminOverride: decode fail (error response)
TEST_F(NsmPowerSmoothingBranch2Test, Action_ApplyAdmin_DecodeFail)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "Action3", "NSM", path, curProfile,
                                   gpu);

    auto errorResp = tinyErrorResp();
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(errorResp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    action.requestApplyAdminOverride(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// requestApplyAdminOverride: success
TEST_F(NsmPowerSmoothingBranch2Test, Action_ApplyAdmin_Success)
{
    auto collection = makeCollection();
    auto adminSensor = makeAdminOverrideSensor();
    auto curProfile = makeCurProfileSensor(collection, adminSensor);
    std::string path = objPath;
    NsmPowerSmoothingAction action(bus(), "Action4", "NSM", path, curProfile,
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
