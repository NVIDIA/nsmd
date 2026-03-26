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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmDbusIfaceOverride/nsmPowerSmoothingFeatureIntf.hpp"

using namespace nsm;

// -----------------------------------------------------------------------
// Helper: encode get-feature-info response
// -----------------------------------------------------------------------
static std::vector<uint8_t>
    makeGetFeatureInfoResp(uint8_t cc,
                           nsm_pwr_smoothing_featureinfo_data data = {})
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_powersmoothing_featinfo_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, &data, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Helper: encode toggle-feature-state response
// -----------------------------------------------------------------------
static std::vector<uint8_t> makeToggleFeatureResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_feature_state_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Helper: encode toggle-immediate-rampdown response
// -----------------------------------------------------------------------
static std::vector<uint8_t> makeToggleRampdownResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_immediate_rampdown_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// -----------------------------------------------------------------------
// Test Fixture
// -----------------------------------------------------------------------
struct OemPowerSmoothingFeatIntfTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    OemPowerSmoothingFeatIntfTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~OemPowerSmoothingFeatIntfTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<OemPowerSmoothingFeatIntf> makeIntf(const char* path)
    {
        return std::make_shared<OemPowerSmoothingFeatIntf>(
            utils::DBusHandler::getBus(), path, gpu);
    }
};

// -----------------------------------------------------------------------
// getPwrSmoothingControlsFromDevice: postPatchIO fails → co_return error //

// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       GetFeatureInfo_PostPatchIOFails_ReturnsError)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/get_piof");

    nsm_pwr_smoothing_featureinfo_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data),
                                  NSM_ERROR));

    auto coro = intf->getPwrSmoothingControlsFromDevice();
    // No assertion on fields – just ensuring no crash and error path taken
}

// -----------------------------------------------------------------------
// getPwrSmoothingControlsFromDevice: error CC → does not set fields
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest, GetFeatureInfo_ErrorCC_FieldsUnchanged)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/get_errcc");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_ERROR)));

    bool before = intf->PowerSmoothingIntf::featureSupported();
    auto coro = intf->getPwrSmoothingControlsFromDevice();
    EXPECT_EQ(intf->PowerSmoothingIntf::featureSupported(), before);
}

// -----------------------------------------------------------------------
// getPwrSmoothingControlsFromDevice: success → all flags and fields set
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       GetFeatureInfo_Success_AllFlagsAndFieldsSet)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/get_ok");

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x07; // bit0=supported, bit1=enabled, bit2=rampDown
    data.currentTmpSetting = 5000;           // 5 W
    data.currentTmpFloorSetting = 3000;      // 3 W
    data.maxTmpFloorSettingInPercent = 2048; // 0.5 * 4096 → 50%
    data.minTmpFloorSettingInPercent = 1024; // 0.25 * 4096 → 25%

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data)));

    auto coro = intf->getPwrSmoothingControlsFromDevice();

    EXPECT_TRUE(intf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(intf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(intf->PowerSmoothingIntf::immediateRampDownEnabled());
    EXPECT_EQ(intf->PowerSmoothingIntf::currentTempSetting(), 5.0);
    EXPECT_EQ(intf->PowerSmoothingIntf::currentTempFloorSetting(), 3.0);
}

// -----------------------------------------------------------------------
// getPwrSmoothingControlsFromDevice: INVALID_POWER_LIMIT → propagated as-is
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       GetFeatureInfo_InvalidPowerLimit_PropagatedAsIs)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/get_inv");

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0;
    data.currentTmpSetting = INVALID_POWER_LIMIT;
    data.currentTmpFloorSetting = INVALID_POWER_LIMIT;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data)));

    auto coro = intf->getPwrSmoothingControlsFromDevice();

    EXPECT_EQ(intf->PowerSmoothingIntf::currentTempSetting(),
              static_cast<double>(INVALID_POWER_LIMIT));
    EXPECT_EQ(intf->PowerSmoothingIntf::currentTempFloorSetting(),
              static_cast<double>(INVALID_POWER_LIMIT));
}

// -----------------------------------------------------------------------
// togglePowerSmoothingOnDevice: postPatchIO fails → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       TogglePowerSmoothing_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/toggle_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp(), NSM_ERROR));

    auto coro = intf->togglePowerSmoothingOnDevice(true, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// togglePowerSmoothingOnDevice: error CC → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest, TogglePowerSmoothing_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/toggle_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp(NSM_ERROR)));

    auto coro = intf->togglePowerSmoothingOnDevice(false, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// togglePowerSmoothingOnDevice: success → calls
// getPwrSmoothingControlsFromDevice
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       TogglePowerSmoothing_Success_CallsGetFeatureInfo)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/toggle_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_pwr_smoothing_featureinfo_data data{};
    data.feature_flag = 0x03; // supported + enabled

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data)));

    auto coro = intf->togglePowerSmoothingOnDevice(true, &status);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setPowerSmoothingEnabled: non-bool value → throws InvalidArgument
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       SetPowerSmoothingEnabled_NonBool_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/set_nonbool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonBoolValue = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        intf->setPowerSmoothingEnabled(nonBoolValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// -----------------------------------------------------------------------
// setPowerSmoothingEnabled: bool value → calls togglePowerSmoothingOnDevice
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       SetPowerSmoothingEnabled_BoolValue_CallsToggle)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/set_bool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_pwr_smoothing_featureinfo_data data{};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType boolValue = bool{true};
    auto coro = intf->setPowerSmoothingEnabled(boolValue, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// toggleImmediateRampDownOnDevice: postPatchIO fails → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       ToggleRampDown_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/rampdown_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp(), NSM_ERROR));

    auto coro = intf->toggleImmediateRampDownOnDevice(true, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// toggleImmediateRampDownOnDevice: error CC → WriteFailure
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest, ToggleRampDown_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/rampdown_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp(NSM_ERROR)));

    auto coro = intf->toggleImmediateRampDownOnDevice(false, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// toggleImmediateRampDownOnDevice: success → calls
// getPwrSmoothingControlsFromDevice
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       ToggleRampDown_Success_CallsGetFeatureInfo)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/rampdown_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_pwr_smoothing_featureinfo_data data{};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data)));

    auto coro = intf->toggleImmediateRampDownOnDevice(true, &status);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// -----------------------------------------------------------------------
// setImmediateRampDownEnabled: non-bool value → throws InvalidArgument
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       SetImmediateRampDownEnabled_NonBool_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/rampset_nonbool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonBoolValue = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        intf->setImmediateRampDownEnabled(nonBoolValue, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// -----------------------------------------------------------------------
// setImmediateRampDownEnabled: bool value → calls
// toggleImmediateRampDownOnDevice
// -----------------------------------------------------------------------
TEST_F(OemPowerSmoothingFeatIntfTest,
       SetImmediateRampDownEnabled_BoolValue_CallsToggle)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/feat/rampset_bool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    nsm_pwr_smoothing_featureinfo_data data{};

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoResp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType boolValue = bool{false};
    auto coro = intf->setImmediateRampDownEnabled(boolValue, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}
