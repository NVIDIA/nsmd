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

// Branch coverage for nsmPowerSmoothingFeatureIntf-v2.hpp
// Targets: handleSample all tags, getPwrSmoothingControlsFromDeviceV2,
//          togglePowerSmoothingOnDevice V2/V1 branches,
//          toggleImmediateRampDownOnDevice V2/V1 branches,
//          setPowerSmoothingEnabled, setImmediateRampDownEnabled

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmDbusIfaceOverride/nsmPowerSmoothingFeatureIntf-v2.hpp"

using namespace nsm;

// Helper: encode toggle-feature-state response
static std::vector<uint8_t> makeToggleFeatureResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_feature_state_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// Helper: encode toggle-immediate-rampdown response
static std::vector<uint8_t> makeToggleRampdownResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_immediate_rampdown_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// Helper: build a V2 feature info response
static std::vector<uint8_t> makeV2FeatResp(
    uint8_t cc, uint16_t telemetry_count,
    const std::vector<std::tuple<uint8_t, std::vector<uint8_t>>>& samples = {})
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_powersmoothing_featinfo_v2_resp(0, cc, telemetry_count, msg);

    for (const auto& [tag, data] : samples)
    {
        std::vector<uint8_t> sampleBuf(sizeof(nsm_aggregate_resp_sample) +
                                       data.size());
        size_t sample_len = 0;
        auto samplePtr =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
        encode_aggregate_resp_sample(tag, true, data.data(), data.size(),
                                     samplePtr, &sample_len);
        buf.insert(buf.end(), sampleBuf.begin(),
                   sampleBuf.begin() + sample_len);
    }
    return buf;
}

// Helper: V1 feature info response
static std::vector<uint8_t>
    makeV1FeatResp(uint8_t cc, nsm_pwr_smoothing_featureinfo_data data = {})
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_powersmoothing_featinfo_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, &data, msg);
    return buf;
}

// Test Fixture
struct OemPowerSmoothingFeatV2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    OemPowerSmoothingFeatV2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~OemPowerSmoothingFeatV2Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<OemPowerSmoothingFeatIntfV2> makeIntf(const char* path)
    {
        return std::make_shared<OemPowerSmoothingFeatIntfV2>(
            utils::DBusHandler::getBus(), path, gpu);
    }
};

// =========================================================================
// handleSample: all tag paths
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_TagAboveMax_ReturnsSuccess)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_above");
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_UnknownTag)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_unk");
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = 200;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_ERROR_LENGTH);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_FeatureFlag_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_ff");
    uint32_t flags = 0x0F; // all 4 bits set
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(flags);
    sample.data = reinterpret_cast<const uint8_t*>(&flags);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_TRUE(intf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(intf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(intf->PowerSmoothingIntf::immediateRampDownEnabled());
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_FeatureFlag_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_ff_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_CurrentTmpSetting_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_cts");
    uint32_t val = 5000;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_CurrentTmpSetting_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_cts_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_TmpFloorSetting_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_tfs");
    uint32_t val = 3000;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_TmpFloorSetting_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_tfs_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MaxTmpFloor_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_maxfl");
    uint16_t val = 2048;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MaxTmpFloor_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_maxfl_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MinTmpFloor_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_minfl");
    uint16_t val = 1024;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MinTmpFloor_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_minfl_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_FloorWindowMultiplier_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_fwm");
    uint32_t val = 2000;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test,
       HandleSample_FloorWindowMultiplier_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_fwm_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MinPFAOffset_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_pfao");
    uint32_t val = 1000;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MinPFAOffset_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_pfao_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MinPFAPoint_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_pfap");
    uint32_t val = 500;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(OemPowerSmoothingFeatV2Test, HandleSample_MinPFAPoint_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/hs_pfap_fail");
    uint8_t badData = 0;
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample{};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// getPwrSmoothingControlsFromDeviceV2: postPatchIO fails
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, GetV2_PostPatchIOFails)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/getv2_piof");

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeV2FeatResp(NSM_SUCCESS, 0), NSM_ERROR));

    intf->getPwrSmoothingControlsFromDeviceV2(gpu);
}

// =========================================================================
// getPwrSmoothingControlsFromDeviceV2: error CC
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, GetV2_ErrorCC)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/getv2_errcc");

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeV2FeatResp(NSM_ERROR, 0)));

    intf->getPwrSmoothingControlsFromDeviceV2(gpu);
}

// =========================================================================
// getPwrSmoothingControlsFromDeviceV2: success with feature flag sample
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, GetV2_SuccessWithSamples)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/getv2_ok");

    uint32_t flags = 0x03;
    std::vector<uint8_t> flagsData(reinterpret_cast<uint8_t*>(&flags),
                                   reinterpret_cast<uint8_t*>(&flags) + 4);
    auto resp = makeV2FeatResp(NSM_SUCCESS, 1, {{FEATURE_FLAG_TAG, flagsData}});

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    intf->getPwrSmoothingControlsFromDeviceV2(gpu);
    EXPECT_TRUE(intf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(intf->PowerSmoothingIntf::powerSmoothingEnabled());
}

// =========================================================================
// togglePowerSmoothingOnDevice: success + V2 path
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, TogglePowerSmoothing_Success_V2Path)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/toggle_v2");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2] = true;
    auto v2Resp = makeV2FeatResp(NSM_SUCCESS, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillOnce(mockPostPatchIO(v2Resp));

    intf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// togglePowerSmoothingOnDevice: success + V1 fallback
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, TogglePowerSmoothing_Success_V1Fallback)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/toggle_v1");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2] = false;
    nsm_pwr_smoothing_featureinfo_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillOnce(mockPostPatchIO(makeV1FeatResp(NSM_SUCCESS, data)));

    intf->togglePowerSmoothingOnDevice(false, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// togglePowerSmoothingOnDevice: postPatchIO fails
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test,
       TogglePowerSmoothing_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/toggle_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp(), NSM_ERROR));

    intf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// togglePowerSmoothingOnDevice: error CC
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, TogglePowerSmoothing_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/toggle_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp(NSM_ERROR)));

    intf->togglePowerSmoothingOnDevice(false, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPowerSmoothingEnabled: non-bool throws
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test,
       SetPowerSmoothingEnabled_NonBool_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/set_nonbool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonBool = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        intf->setPowerSmoothingEnabled(nonBool, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// setPowerSmoothingEnabled: valid bool
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test,
       SetPowerSmoothingEnabled_BoolValue_CallsToggle)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/set_bool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2] = true;
    auto v2Resp = makeV2FeatResp(NSM_SUCCESS, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillOnce(mockPostPatchIO(v2Resp));

    AsyncSetOperationValueType boolValue = bool{true};
    intf->setPowerSmoothingEnabled(boolValue, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// toggleImmediateRampDownOnDevice: success + V2 path
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, ToggleRampDown_Success_V2Path)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/rampdown_v2");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2] = true;
    auto v2Resp = makeV2FeatResp(NSM_SUCCESS, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillOnce(mockPostPatchIO(v2Resp));

    intf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// toggleImmediateRampDownOnDevice: success + V1 fallback
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, ToggleRampDown_Success_V1Fallback)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/rampdown_v1");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2] = false;
    nsm_pwr_smoothing_featureinfo_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillOnce(mockPostPatchIO(makeV1FeatResp(NSM_SUCCESS, data)));

    intf->toggleImmediateRampDownOnDevice(false, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// toggleImmediateRampDownOnDevice: postPatchIO fails
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test,
       ToggleRampDown_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/rampdown_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp(), NSM_ERROR));

    intf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// toggleImmediateRampDownOnDevice: error CC
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, ToggleRampDown_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/rampdown_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp(NSM_ERROR)));

    intf->toggleImmediateRampDownOnDevice(false, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setImmediateRampDownEnabled: non-bool throws
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test,
       SetImmediateRampDown_NonBool_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/setramp_nonbool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonBool = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        intf->setImmediateRampDownEnabled(nonBool, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// setImmediateRampDownEnabled: valid bool
// =========================================================================
TEST_F(OemPowerSmoothingFeatV2Test, SetImmediateRampDown_BoolValue_CallsToggle)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2feat/setramp_bool");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2] = true;
    auto v2Resp = makeV2FeatResp(NSM_SUCCESS, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillOnce(mockPostPatchIO(v2Resp));

    AsyncSetOperationValueType boolValue = bool{false};
    intf->setImmediateRampDownEnabled(boolValue, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}
