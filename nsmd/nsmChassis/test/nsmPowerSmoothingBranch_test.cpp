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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmProcessor/nsmPowerSmoothing.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// SECTION 5: nsmPowerSmoothing.cpp -- Uncovered function tests
// ============================================================================

// -- NsmPowerSmoothing::updateReading with detailed field checks
TEST(NsmPowerSmoothingBatch11F,
     UpdateReading_AllFlagsSet_SetsAllFieldsCorrectly)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/pwrsmooth";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus, path,
                                                                nullptr);
    std::string name = "pwrSmoothTest";
    std::string type = "pwrSmoothType";
    NsmPowerSmoothing sensor(name, type, path, featIntf, nullptr);

    struct nsm_pwr_smoothing_featureinfo_data data = {};
    // bit0=supported, bit1=enabled, bit2=rampDown
    data.feature_flag = 0x07;
    data.currentTmpSetting = 5000;      // 5000 mW -> 5 W
    data.currentTmpFloorSetting = 3000; // 3000 mW -> 3 W
    data.maxTmpFloorSettingInPercent = 0;
    data.minTmpFloorSettingInPercent = 0;

    // Act
    sensor.updateReading(&data);

    // Assert
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
    EXPECT_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(), 5);
    EXPECT_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(), 3);
}

// -- NsmPowerSmoothing::updateReading with INVALID_POWER_LIMIT
TEST(NsmPowerSmoothingBatch11F,
     UpdateReading_InvalidPowerLimit_SetsInvalidValue)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/pwrsmooth_inv";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus, path,
                                                                nullptr);
    std::string name = "pwrSmoothInvalid";
    std::string type = "pwrSmoothType";
    NsmPowerSmoothing sensor(name, type, path, featIntf, nullptr);

    struct nsm_pwr_smoothing_featureinfo_data data = {};
    data.feature_flag = 0;
    data.currentTmpSetting = INVALID_POWER_LIMIT;
    data.currentTmpFloorSetting = INVALID_POWER_LIMIT;

    // Act
    sensor.updateReading(&data);

    // Assert - when INVALID_POWER_LIMIT, should pass through as-is
    EXPECT_EQ(featIntf->PowerSmoothingIntf::currentTempSetting(),
              static_cast<double>(INVALID_POWER_LIMIT));
    EXPECT_EQ(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
              static_cast<double>(INVALID_POWER_LIMIT));
}

// -- NsmPowerSmoothing::updateReading with null data pointer
TEST(NsmPowerSmoothingBatch11F, UpdateReading_NullData_ReturnsEarlyWithoutCrash)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/pwrsmooth_null";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus, path,
                                                                nullptr);
    std::string name = "pwrSmoothNull";
    std::string type = "pwrSmoothType";
    NsmPowerSmoothing sensor(name, type, path, featIntf, nullptr);

    // Act & Assert - should not crash
    EXPECT_NO_THROW(sensor.updateReading(nullptr));
}

// -- NsmPowerSmoothing::updateReading with no flags set
TEST(NsmPowerSmoothingBatch11F, UpdateReading_NoFlags_SetsAllFlagsToFalse)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/pwrsmooth_noflags";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus, path,
                                                                nullptr);
    std::string name = "pwrSmoothNoFlags";
    std::string type = "pwrSmoothType";
    NsmPowerSmoothing sensor(name, type, path, featIntf, nullptr);

    struct nsm_pwr_smoothing_featureinfo_data data = {};
    data.feature_flag = 0;
    data.currentTmpSetting = 1000;
    data.currentTmpFloorSetting = 500;

    // Act
    sensor.updateReading(&data);

    // Assert
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
}

// -- NsmHwCircuitryTelemetry::updateReading with null data
TEST(NsmHwCircuitryBatch11F, UpdateReading_NullData_ReturnsEarlyWithoutCrash)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/hwcircuit_null";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus, path,
                                                                nullptr);
    std::string name = "hwCircuitNull";
    std::string type = "hwCircuitType";
    NsmHwCircuitryTelemetry sensor(name, type, path, featIntf);

    // Act & Assert
    EXPECT_NO_THROW(sensor.updateReading(nullptr));
}

// -- NsmHwCircuitryTelemetry::updateReading with valid data
TEST(NsmHwCircuitryBatch11F, UpdateReading_ValidData_UpdatesLifeTimeRemaining)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/hwcircuit_valid";
    auto featIntf = std::make_shared<OemPowerSmoothingFeatIntf>(bus, path,
                                                                nullptr);
    std::string name = "hwCircuitValid";
    std::string type = "hwCircuitType";
    NsmHwCircuitryTelemetry sensor(name, type, path, featIntf);

    struct nsm_hardwarecircuitry_data data = {};
    data.reading = 0; // NvUFXP8_24 format, 0 means 0.0

    // Act
    sensor.updateReading(&data);

    // Assert
    EXPECT_EQ(featIntf->PowerSmoothingIntf::lifeTimeRemaining(), 0.0);
}

// -- NsmPowerSmoothingAdminOverride::updateReading with null data
TEST(NsmAdminOverrideBatch11F, UpdateReading_NullData_ReturnsEarlyWithoutCrash)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/admin_null";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus, path, nullptr);
    std::string name = "adminNull";
    std::string type = "adminType";
    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, path, nullptr);

    // Act & Assert
    EXPECT_NO_THROW(sensor.updateReading(nullptr));
}

// -- NsmPowerSmoothingAdminOverride::updateReading with INVALID_UINT16_VALUE
TEST(NsmAdminOverrideBatch11F,
     UpdateReading_InvalidTmpFloor_SetsInvalidUint32Value)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/admin_invalid";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus, path, nullptr);
    std::string name = "adminInvalid";
    std::string type = "adminType";
    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, path, nullptr);

    struct nsm_admin_override_data data = {};
    data.admin_override_percent_tmp_floor = INVALID_UINT16_VALUE;
    data.admin_override_ramup_rate_in_miliwatts_per_second = 1000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 2000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 3000;

    // Act
    sensor.updateReading(&data);

    // Assert
    EXPECT_EQ(adminIntf->AdminPowerProfileIntf::tmpFloorPercent(),
              static_cast<double>(INVALID_UINT32_VALUE));
}

// -- NsmPowerSmoothingAdminOverride::updateReading with valid data
TEST(NsmAdminOverrideBatch11F, UpdateReading_ValidData_SetsRatesCorrectly)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/admin_valid";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus, path, nullptr);
    std::string name = "adminValid";
    std::string type = "adminType";
    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, path, nullptr);

    struct nsm_admin_override_data data = {};
    data.admin_override_percent_tmp_floor = 0; // valid
    data.admin_override_ramup_rate_in_miliwatts_per_second = 5000;
    data.admin_override_rampdown_rate_in_miliwatts_per_second = 3000;
    data.admin_override_rampdown_hysteresis_value_in_milisec = 2000;

    // Act
    sensor.updateReading(&data);

    // Assert - mW/sec -> W/sec (divide by 1000)
    EXPECT_DOUBLE_EQ(adminIntf->AdminPowerProfileIntf::rampUpRate(), 5.0);
    EXPECT_DOUBLE_EQ(adminIntf->AdminPowerProfileIntf::rampDownRate(), 3.0);
    EXPECT_DOUBLE_EQ(adminIntf->AdminPowerProfileIntf::rampDownHysteresis(),
                     2.0);
}

// -- NsmPowerSmoothingAdminOverride::getInventoryObjPath
TEST(NsmAdminOverrideBatch11F, GetInventoryObjPath_ReturnsStoredPath)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path = "/xyz/openbmc_project/inventory/batch11f/admin_path";
    auto adminIntf = std::make_shared<OemAdminProfileIntf>(bus, path, nullptr);
    std::string name = "adminPath";
    std::string type = "adminType";
    NsmPowerSmoothingAdminOverride sensor(name, type, adminIntf, path, nullptr);

    // Act
    auto result = sensor.getInventoryObjPath();

    // Assert
    EXPECT_EQ(result, path);
}

// -- NsmPowerProfileCollection helper functions
TEST(NsmPowerProfileCollectionBatch11F, HasProfileId_NoProfiles_ReturnsFalse)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollNone";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_none";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    // Act & Assert
    EXPECT_FALSE(sensor.hasProfileId(0));
    EXPECT_FALSE(sensor.hasProfileId(5));
}

TEST(NsmPowerProfileCollectionBatch11F,
     AddSupportedProfile_AddsAndHasReturnsTrue)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollAdd";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_add";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, path, 0,
                                                             nullptr);

    // Act
    sensor.addSupportedProfile(0, profileIntf);

    // Assert
    EXPECT_TRUE(sensor.hasProfileId(0));
    EXPECT_FALSE(sensor.hasProfileId(1));
}

TEST(NsmPowerProfileCollectionBatch11F,
     GetSupportedProfileById_ValidId_ReturnsProfile)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollGet";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_get";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, path, 0,
                                                             nullptr);
    sensor.addSupportedProfile(0, profileIntf);

    // Act
    auto result = sensor.getSupportedProfileById(0);

    // Assert
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result, profileIntf);
}

TEST(NsmPowerProfileCollectionBatch11F,
     GetSupportedProfileById_InvalidId_ThrowsOutOfRange)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollBad";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_bad";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    // Act & Assert
    EXPECT_THROW(sensor.getSupportedProfileById(99), std::out_of_range);
}

TEST(NsmPowerProfileCollectionBatch11F,
     GetProfilePathByProfileId_ValidId_ReturnsProfilePath)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollPath";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_path";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, path, 0,
                                                             nullptr);
    sensor.addSupportedProfile(0, profileIntf);

    // Act
    auto result = sensor.getProfilePathByProfileId(0);

    // Assert - should return the profile's inventory obj path
    EXPECT_NE(result, "/");
    EXPECT_NE(result, "");
}

TEST(NsmPowerProfileCollectionBatch11F,
     GetProfilePathByProfileId_InvalidId_ReturnsSlash)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollNoPath";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_nopath";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    // Act
    auto result = sensor.getProfilePathByProfileId(99);

    // Assert
    EXPECT_EQ(result, "/");
}

// -- NsmPowerProfileCollection::updateSupportedProfile
TEST(NsmPowerProfileCollectionBatch11F,
     UpdateSupportedProfile_ValidData_SetsProfileProperties)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollUpdate";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_update";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, path, 0,
                                                             nullptr);

    struct nsm_preset_profile_data profileData = {};
    profileData.tmp_floor_setting_in_percent = 0; // valid
    profileData.ramp_up_rate_in_miliwattspersec = 2000;
    profileData.ramp_down_rate_in_miliwattspersec = 1000;
    profileData.ramp_hysterisis_rate_in_milisec = 500;

    // Act
    sensor.updateSupportedProfile(profileIntf, &profileData);

    // Assert
    EXPECT_DOUBLE_EQ(profileIntf->PowerProfileIntf::rampUpRate(), 2.0);
    EXPECT_DOUBLE_EQ(profileIntf->PowerProfileIntf::rampDownRate(), 1.0);
    EXPECT_DOUBLE_EQ(profileIntf->PowerProfileIntf::rampDownHysteresis(), 0.5);
}

// -- NsmPowerProfileCollection::updateSupportedProfile with INVALID tmp floor
TEST(NsmPowerProfileCollectionBatch11F,
     UpdateSupportedProfile_InvalidTmpFloor_SetsInvalidUint32)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollUpdateInv";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_updinv";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, path, 0,
                                                             nullptr);

    struct nsm_preset_profile_data profileData = {};
    profileData.tmp_floor_setting_in_percent = INVALID_UINT16_VALUE;

    // Act
    sensor.updateSupportedProfile(profileIntf, &profileData);

    // Assert
    EXPECT_EQ(profileIntf->PowerProfileIntf::tmpFloorPercent(),
              static_cast<double>(INVALID_UINT32_VALUE));
}

// -- NsmPowerProfileCollection::updateSupportedProfile with null obj
TEST(NsmPowerProfileCollectionBatch11F,
     UpdateSupportedProfile_NullObj_DoesNotCrash)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string name = "profileCollNullObj";
    std::string type = "profileCollType";
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/profilecoll_nullobj";
    NsmPowerProfileCollection sensor(name, type, path, nullptr);

    struct nsm_preset_profile_data profileData = {};

    // Act & Assert - should not crash
    EXPECT_NO_THROW(sensor.updateSupportedProfile(nullptr, &profileData));
}

// -- NsmCurrentPowerSmoothingProfile::updateReading with null data
TEST(NsmCurrentPowerProfileBatch11F,
     UpdateReading_NullData_ReturnsEarlyWithoutCrash)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/curprofile_null";
    auto adminProfileIntf = std::make_shared<OemAdminProfileIntf>(bus, path,
                                                                  nullptr);
    std::string adminName = "adminCurProfNull";
    std::string adminType = "adminType";
    auto adminSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        adminName, adminType, adminProfileIntf, path, nullptr);
    auto curProfileIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus, path, adminProfileIntf->getInventoryObjPath(), nullptr);
    std::string collName = "profileCollCurNull";
    std::string collType = "profileCollType";
    auto profileColl = std::make_shared<NsmPowerProfileCollection>(
        collName, collType, path, nullptr);

    std::string name = "curProfileNull";
    std::string type = "curProfileType";
    NsmCurrentPowerSmoothingProfile sensor(name, type, path, curProfileIntf,
                                           profileColl, adminSensor, nullptr);

    // Act & Assert
    EXPECT_NO_THROW(sensor.updateReading(nullptr));
}

// -- NsmCurrentPowerSmoothingProfile::updateReading with valid data
TEST(NsmCurrentPowerProfileBatch11F,
     UpdateReading_ValidData_SetsProfileFieldsCorrectly)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/curprofile_valid";
    auto adminProfileIntf = std::make_shared<OemAdminProfileIntf>(bus, path,
                                                                  nullptr);
    std::string adminName = "adminCurProfValid";
    std::string adminType = "adminType";
    auto adminSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        adminName, adminType, adminProfileIntf, path, nullptr);
    auto curProfileIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus, path, adminProfileIntf->getInventoryObjPath(), nullptr);
    std::string collName = "profileCollCurValid";
    std::string collType = "profileCollType";
    auto profileColl = std::make_shared<NsmPowerProfileCollection>(
        collName, collType, path, nullptr);

    std::string name = "curProfileValid";
    std::string type = "curProfileType";
    NsmCurrentPowerSmoothingProfile sensor(name, type, path, curProfileIntf,
                                           profileColl, adminSensor, nullptr);

    struct nsm_get_current_profile_data data = {};
    data.current_active_profile_id = 0;
    data.admin_override_mask.byte = 0x0F; // all overrides applied
    data.current_percent_tmp_floor = 0;   // valid (not INVALID_UINT16_VALUE)
    data.current_rampup_rate_in_miliwatts_per_second = 5000;
    data.current_rampdown_rate_in_miliwatts_per_second = 3000;
    data.current_rampdown_hysteresis_value_in_milisec = 2000;

    // Act
    sensor.updateReading(&data);

    // Assert
    EXPECT_DOUBLE_EQ(curProfileIntf->CurrentPowerProfileIntf::rampUpRate(),
                     5.0);
    EXPECT_DOUBLE_EQ(curProfileIntf->CurrentPowerProfileIntf::rampDownRate(),
                     3.0);
    EXPECT_DOUBLE_EQ(
        curProfileIntf->CurrentPowerProfileIntf::rampDownHysteresis(), 2.0);
    EXPECT_TRUE(curProfileIntf->CurrentPowerProfileIntf::rampUpRateApplied());
    EXPECT_TRUE(curProfileIntf->CurrentPowerProfileIntf::rampDownRateApplied());
    EXPECT_TRUE(
        curProfileIntf->CurrentPowerProfileIntf::rampDownHysteresisApplied());
}

// -- NsmCurrentPowerSmoothingProfile::updateReading with
// INVALID_UINT16_VALUE for tmp floor
TEST(NsmCurrentPowerProfileBatch11F,
     UpdateReading_InvalidTmpFloor_SetsInvalidUint32Value)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/curprofile_invtmp";
    auto adminProfileIntf = std::make_shared<OemAdminProfileIntf>(bus, path,
                                                                  nullptr);
    std::string adminName = "adminCurProfInvTmp";
    std::string adminType = "adminType";
    auto adminSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        adminName, adminType, adminProfileIntf, path, nullptr);
    auto curProfileIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus, path, adminProfileIntf->getInventoryObjPath(), nullptr);
    std::string collName = "profileCollCurInvTmp";
    std::string collType = "profileCollType";
    auto profileColl = std::make_shared<NsmPowerProfileCollection>(
        collName, collType, path, nullptr);

    std::string name = "curProfileInvTmp";
    std::string type = "curProfileType";
    NsmCurrentPowerSmoothingProfile sensor(name, type, path, curProfileIntf,
                                           profileColl, adminSensor, nullptr);

    struct nsm_get_current_profile_data data = {};
    data.current_percent_tmp_floor = INVALID_UINT16_VALUE;

    // Act
    sensor.updateReading(&data);

    // Assert
    EXPECT_EQ(curProfileIntf->CurrentPowerProfileIntf::tmpFloorPercent(),
              static_cast<double>(INVALID_UINT32_VALUE));
}

// -- NsmCurrentPowerSmoothingProfile::getProfilePath with unknown profile ID
TEST(NsmCurrentPowerProfileBatch11F,
     GetProfilePath_UnknownProfileId_ReturnsInventoryPath)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/curprofile_path";
    auto adminProfileIntf = std::make_shared<OemAdminProfileIntf>(bus, path,
                                                                  nullptr);
    std::string adminName = "adminCurProfPath";
    std::string adminType = "adminType";
    auto adminSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        adminName, adminType, adminProfileIntf, path, nullptr);
    auto curProfileIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus, path, adminProfileIntf->getInventoryObjPath(), nullptr);
    std::string collName = "profileCollCurPath";
    std::string collType = "profileCollType";
    auto profileColl = std::make_shared<NsmPowerProfileCollection>(
        collName, collType, path, nullptr);

    std::string name = "curProfilePath";
    std::string type = "curProfileType";
    NsmCurrentPowerSmoothingProfile sensor(name, type, path, curProfileIntf,
                                           profileColl, adminSensor, nullptr);

    // Act - profileId 99 does not exist
    auto result = sensor.getProfilePath(99);

    // Assert - should return inventoryObjPath since profileId not found
    EXPECT_EQ(result, path);
}

// -- NsmCurrentPowerSmoothingProfile::getProfilePath with valid profile ID
TEST(NsmCurrentPowerProfileBatch11F,
     GetProfilePath_ValidProfileId_ReturnsProfilePath)
{
    // Arrange
    auto bus = sdbusplus::bus::new_default();
    std::string path =
        "/xyz/openbmc_project/inventory/batch11f/curprofile_validpath";
    auto adminProfileIntf = std::make_shared<OemAdminProfileIntf>(bus, path,
                                                                  nullptr);
    std::string adminName = "adminCurProfValidPath";
    std::string adminType = "adminType";
    auto adminSensor = std::make_shared<NsmPowerSmoothingAdminOverride>(
        adminName, adminType, adminProfileIntf, path, nullptr);
    auto curProfileIntf = std::make_shared<OemCurrentPowerProfileIntf>(
        bus, path, adminProfileIntf->getInventoryObjPath(), nullptr);
    std::string collName = "profileCollCurValidPath";
    std::string collType = "profileCollType";
    auto profileColl = std::make_shared<NsmPowerProfileCollection>(
        collName, collType, path, nullptr);

    // Add a profile with ID 0
    auto profileIntf = std::make_shared<OemPowerProfileIntf>(bus, path, 0,
                                                             nullptr);
    profileColl->addSupportedProfile(0, profileIntf);

    std::string name = "curProfileValidPath";
    std::string type = "curProfileType";
    NsmCurrentPowerSmoothingProfile sensor(name, type, path, curProfileIntf,
                                           profileColl, adminSensor, nullptr);

    // Act
    auto result = sensor.getProfilePath(0);

    // Assert - should return profile's path, not inventoryObjPath
    EXPECT_NE(result, path);
    EXPECT_NE(result, "");
}
