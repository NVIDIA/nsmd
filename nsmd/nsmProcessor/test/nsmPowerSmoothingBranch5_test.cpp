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
 * Branch coverage batch 5 for nsmPowerSmoothing.cpp
 *
 * Targets remaining uncovered branches:
 * - NsmPowerProfileCollection: genRequestMsg encode failure
 * - NsmPowerProfileCollection: handleResponseMsg decode failure
 * - NsmPowerProfileCollection: getProfilePathByProfileId not found
 * - NsmPowerProfileCollection: updateSupportedProfile with nullptr obj
 * - NsmPowerProfileCollection: INVALID_UINT16_VALUE tmpFloor in profile
 * - NsmCurrentPowerSmoothingProfile: getProfilePath hasProfileId false
 * - NsmCurrentPowerSmoothingProfile: updateReading INVALID_UINT16_VALUE
 * - NsmPowerSmoothingAdminOverride: INVALID_UINT16_VALUE tmpFloor
 * - NsmHwCircuitryTelemetry: updateReading nullptr
 * - NsmPowerSmoothing: genRequestMsg encode failure
 * - NsmHwCircuitryTelemetry: genRequestMsg encode failure
 * - NsmCurrentPowerSmoothingProfile: genRequestMsg encode failure
 * - NsmPowerSmoothingAdminOverride: genRequestMsg encode failure
 */

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
struct NsmPowerSmoothingBranch5Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:1";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_PSB5";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPowerSmoothingBranch5Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPowerSmoothingBranch5Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }
};

// =============================================================================
// NsmPowerSmoothing: genRequestMsg encode failure
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, PwrSmoothing_GenReq_EncodeFail)
{
    std::string name = "psb5_feat";
    std::string type = "NSM_PS";
    std::string invPath = objPath + "/ps_feat";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus(), invPath,
                                                                gpu);

    NsmPowerSmoothing sensor(name, type, invPath, featIntf, gpu);

    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// NsmHwCircuitryTelemetry: genRequestMsg encode failure
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, HwCircuitry_GenReq_EncodeFail)
{
    std::string name = "psb5_hw";
    std::string type = "NSM_HW";
    std::string invPath = objPath + "/hw_circ";
    auto psIntf = std::make_shared<PowerSmoothingIntf>(bus(), invPath.c_str());

    NsmHwCircuitryTelemetry sensor(name, type, invPath, psIntf);

    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// NsmHwCircuitryTelemetry: updateReading with nullptr
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, HwCircuitry_UpdateReading_Nullptr)
{
    std::string name = "psb5_hw_null";
    std::string type = "NSM_HW";
    std::string invPath = objPath + "/hw_circ_null";
    auto psIntf = std::make_shared<PowerSmoothingIntf>(bus(), invPath.c_str());

    NsmHwCircuitryTelemetry sensor(name, type, invPath, psIntf);
    // Should not crash, just early return
    sensor.updateReading(nullptr);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: genRequestMsg encode failure
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, CurrentProfile_GenReq_EncodeFail)
{
    std::string name = "psb5_cur";
    std::string type = "NSM_CUR";
    std::string invPath = objPath + "/cur_prof";
    auto curProfIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus(), invPath, std::string(""), gpu);
    auto colSensor = std::make_shared<NsmPowerProfileCollection>(name, type,
                                                                 invPath, gpu);

    NsmCurrentPowerSmoothingProfile sensor(name, type, invPath, curProfIntf,
                                           colSensor, nullptr, gpu);

    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: getProfilePath when hasProfileId is false
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, CurrentProfile_GetProfilePath_NotFound)
{
    std::string name = "psb5_cur_nf";
    std::string type = "NSM_CUR";
    std::string invPath = objPath + "/cur_prof_nf";
    auto curProfIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus(), invPath, std::string(""), gpu);
    auto colSensor = std::make_shared<NsmPowerProfileCollection>(name, type,
                                                                 invPath, gpu);

    NsmCurrentPowerSmoothingProfile sensor(name, type, invPath, curProfIntf,
                                           colSensor, nullptr, gpu);

    // profileId 99 does not exist -> returns inventoryObjPath
    auto result = sensor.getProfilePath(99);
    EXPECT_EQ(result, invPath);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading nullptr
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, CurrentProfile_UpdateReading_Nullptr)
{
    std::string name = "psb5_cur_np";
    std::string type = "NSM_CUR";
    std::string invPath = objPath + "/cur_prof_np";
    auto curProfIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus(), invPath, std::string(""), gpu);
    auto colSensor = std::make_shared<NsmPowerProfileCollection>(name, type,
                                                                 invPath, gpu);

    NsmCurrentPowerSmoothingProfile sensor(name, type, invPath, curProfIntf,
                                           colSensor, nullptr, gpu);

    sensor.updateReading(nullptr);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading with INVALID_UINT16_VALUE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, CurrentProfile_UpdateReading_InvalidUint16)
{
    std::string name = "psb5_cur_inv";
    std::string type = "NSM_CUR";
    std::string invPath = objPath + "/cur_prof_inv";
    auto curProfIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus(), invPath, std::string(""), gpu);
    auto colSensor = std::make_shared<NsmPowerProfileCollection>(name, type,
                                                                 invPath, gpu);

    NsmCurrentPowerSmoothingProfile sensor(name, type, invPath, curProfIntf,
                                           colSensor, nullptr, gpu);

    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = INVALID_UINT16_VALUE;
    data.current_rampup_rate_in_miliwatts_per_second = 1000;
    data.current_rampdown_rate_in_miliwatts_per_second = 2000;
    data.current_rampdown_hysteresis_value_in_milisec = 500;
    data.current_active_profile_id = 0;

    sensor.updateReading(&data);

    EXPECT_EQ(curProfIntf->CurrentPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfile: updateReading with valid tmpFloor
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, CurrentProfile_UpdateReading_ValidTmpFloor)
{
    std::string name = "psb5_cur_val";
    std::string type = "NSM_CUR";
    std::string invPath = objPath + "/cur_prof_val";
    auto curProfIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus(), invPath, std::string(""), gpu);
    auto colSensor = std::make_shared<NsmPowerProfileCollection>(name, type,
                                                                 invPath, gpu);

    NsmCurrentPowerSmoothingProfile sensor(name, type, invPath, curProfIntf,
                                           colSensor, nullptr, gpu);

    nsm_get_current_profile_data data{};
    data.current_percent_tmp_floor = 0x0800; // valid UFXP4.12 value
    data.current_rampup_rate_in_miliwatts_per_second = 1000;
    data.current_rampdown_rate_in_miliwatts_per_second = 2000;
    data.current_rampdown_hysteresis_value_in_milisec = 500;
    data.current_active_profile_id = 0;

    sensor.updateReading(&data);

    // tmpFloor should NOT be INVALID_UINT32_VALUE
    EXPECT_NE(curProfIntf->CurrentPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: genRequestMsg encode failure
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, AdminOverride_GenReq_EncodeFail)
{
    std::string name = "psb5_admin";
    std::string type = "NSM_ADMIN";
    std::string invPath = objPath + "/admin_ovr";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus(), invPath, gpu);

    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, invPath, gpu);

    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: updateReading nullptr
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, AdminOverride_UpdateReading_Nullptr)
{
    std::string name = "psb5_admin_np";
    std::string type = "NSM_ADMIN";
    std::string invPath = objPath + "/admin_ovr_np";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus(), invPath, gpu);

    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, invPath, gpu);
    sensor.updateReading(nullptr);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: INVALID_UINT16_VALUE tmpFloor
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, AdminOverride_UpdateReading_InvalidUint16)
{
    std::string name = "psb5_admin_inv";
    std::string type = "NSM_ADMIN";
    std::string invPath = objPath + "/admin_ovr_inv";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus(), invPath, gpu);

    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, invPath, gpu);

    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = INVALID_UINT16_VALUE;
    data.admin_override_ramup_rate_in_miliwatts_per_second = 1000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 2000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 500;

    sensor.updateReading(&data);

    EXPECT_EQ(adminIntf->AdminPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerSmoothingAdminOverride: valid tmpFloor (non-INVALID)
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, AdminOverride_UpdateReading_ValidTmpFloor)
{
    std::string name = "psb5_admin_vl";
    std::string type = "NSM_ADMIN";
    std::string invPath = objPath + "/admin_ovr_vl";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus(), invPath, gpu);

    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, invPath, gpu);

    nsm_admin_override_data data{};
    data.admin_override_percent_tmp_floor = 0x0800; // valid UFXP4.12
    data.admin_override_ramup_rate_in_miliwatts_per_second = 1000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 2000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 500;

    sensor.updateReading(&data);

    EXPECT_NE(adminIntf->AdminPowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerProfileCollection: genRequestMsg encode failure
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, ProfileCollection_GenReq_EncodeFail)
{
    std::string name = "psb5_col";
    std::string type = "NSM_COL";
    std::string invPath = objPath + "/prof_col";

    NsmPowerProfileCollection sensor(name, type, invPath, gpu);

    auto request = sensor.genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// =============================================================================
// NsmPowerProfileCollection: getSupportedProfileById not found -> throws
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, ProfileCollection_GetById_NotFound_Throws)
{
    std::string name = "psb5_col_nf";
    std::string type = "NSM_COL";
    std::string invPath = objPath + "/prof_col_nf";

    NsmPowerProfileCollection sensor(name, type, invPath, gpu);

    EXPECT_THROW(sensor.getSupportedProfileById(99), std::out_of_range);
}

// =============================================================================
// NsmPowerProfileCollection: getProfilePathByProfileId not found -> returns "/"
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test,
       ProfileCollection_GetPath_NotFound_ReturnsSlash)
{
    std::string name = "psb5_col_path";
    std::string type = "NSM_COL";
    std::string invPath = objPath + "/prof_col_path";

    NsmPowerProfileCollection sensor(name, type, invPath, gpu);

    auto result = sensor.getProfilePathByProfileId(99);
    EXPECT_EQ(result, "/");
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile with nullptr obj
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test, ProfileCollection_UpdateProfile_NullObj)
{
    std::string name = "psb5_col_upd";
    std::string type = "NSM_COL";
    std::string invPath = objPath + "/prof_col_upd";

    NsmPowerProfileCollection sensor(name, type, invPath, gpu);

    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = 0x0800;
    // Should not crash with nullptr
    sensor.updateSupportedProfile(nullptr, &data);
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile INVALID_UINT16_VALUE
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test,
       ProfileCollection_UpdateProfile_InvalidTmpFloor)
{
    std::string name = "psb5_col_inv";
    std::string type = "NSM_COL";
    std::string invPath = objPath + "/prof_col_inv";

    NsmPowerProfileCollection sensor(name, type, invPath, gpu);

    auto profIntf = std::make_shared<OemPowerProfileIntf>(bus(), invPath, 0,
                                                          gpu);

    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = INVALID_UINT16_VALUE;
    data.ramp_up_rate_in_miliwattspersec = 1000;
    data.ramp_down_rate_in_miliwattspersec = 2000;
    data.ramp_hysterisis_rate_in_milisec = 500;

    sensor.updateSupportedProfile(profIntf, &data);

    EXPECT_EQ(profIntf->PowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}

// =============================================================================
// NsmPowerProfileCollection: updateSupportedProfile valid tmpFloor
// =============================================================================
TEST_F(NsmPowerSmoothingBranch5Test,
       ProfileCollection_UpdateProfile_ValidTmpFloor)
{
    std::string name = "psb5_col_vl";
    std::string type = "NSM_COL";
    std::string invPath = objPath + "/prof_col_vl";

    NsmPowerProfileCollection sensor(name, type, invPath, gpu);

    auto profIntf = std::make_shared<OemPowerProfileIntf>(bus(), invPath, 0,
                                                          gpu);

    nsm_preset_profile_data data{};
    data.tmp_floor_setting_in_percent = 0x0800; // valid
    data.ramp_up_rate_in_miliwattspersec = 1000;
    data.ramp_down_rate_in_miliwattspersec = 2000;
    data.ramp_hysterisis_rate_in_milisec = 500;

    sensor.updateSupportedProfile(profIntf, &data);

    EXPECT_NE(profIntf->PowerProfileIntf::tmpFloorPercent(),
              INVALID_UINT32_VALUE);
}
