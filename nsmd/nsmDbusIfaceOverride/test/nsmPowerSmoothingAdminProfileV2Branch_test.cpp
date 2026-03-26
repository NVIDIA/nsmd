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

// Branch coverage for nsmPowerSmoothingAdminProfileIntf-v2.hpp
// Targets: handleSample, updateCurrent*Sample, overrideAdminProfileParam,
//          resetAdminProfileParam, all set* methods,
//          getAdminProfileFromDeviceV2

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmDbusIfaceOverride/nsmPowerSmoothingAdminProfileIntf-v2.hpp"

using namespace nsm;

// Helper: encode setup-admin-override response
static std::vector<uint8_t> makeSetupResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_setup_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// Helper: encode query-admin-override V1 response (for fallback path)
static std::vector<uint8_t>
    makeQueryAdminV1Resp(uint8_t cc, nsm_admin_override_data data = {})
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, &data, msg);
    return buf;
}

// Helper: build a V2 response with given samples
static std::vector<uint8_t> makeV2Response(
    uint8_t cc, uint16_t telemetry_count,
    const std::vector<std::tuple<uint8_t, std::vector<uint8_t>>>& samples = {})
{
    // Start with header + V2 response header
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_admin_override_profile_info_v2_resp(0, cc, telemetry_count, msg);

    // Append aggregate samples
    for (const auto& [tag, data] : samples)
    {
        size_t sample_len = 0;
        std::vector<uint8_t> sampleBuf(sizeof(nsm_aggregate_resp_sample) +
                                       data.size());
        auto samplePtr =
            reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
        encode_aggregate_resp_sample(tag, true, data.data(), data.size(),
                                     samplePtr, &sample_len);
        buf.insert(buf.end(), sampleBuf.begin(),
                   sampleBuf.begin() + sample_len);
    }
    return buf;
}

// Test Fixture
struct OemAdminProfileV2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    OemAdminProfileV2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~OemAdminProfileV2Test()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<OemAdminProfileIntfV2> makeIntf(const std::string& path)
    {
        return std::make_shared<OemAdminProfileIntfV2>(
            utils::DBusHandler::getBus(), path, gpu);
    }
};

// =========================================================================
// handleSample: tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_TagAboveMax_ReturnsSuccess)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_abovemax");
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: unknown tag (within range but not recognized)
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_UnknownTag_ReturnsLengthError)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_unknown");
    OemAdminProfileIntfV2::TelemetrySample sample{};
    // Tag 200 is within range but not a known admin profile tag
    sample.tag = 200;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_ERROR_LENGTH);
}

// =========================================================================
// handleSample: TMP floor tag with valid data
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_TmpFloor_ValidData)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_tmpfloor");
    uint16_t floorVal = 2048; // 0.5 * 4096 = 50%
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(floorVal);
    sample.data = reinterpret_cast<const uint8_t*>(&floorVal);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: TMP floor with INVALID_UINT16_VALUE
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_TmpFloor_InvalidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_tmpfloor_inv");
    uint16_t floorVal = INVALID_UINT16_VALUE;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(floorVal);
    sample.data = reinterpret_cast<const uint8_t*>(&floorVal);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_EQ(intf->AdminPowerProfileIntf::tmpFloorPercent(),
              static_cast<double>(INVALID_UINT32_VALUE));
}

// =========================================================================
// handleSample: TMP floor decode failure (bad data_len)
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_TmpFloor_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_tmpfloor_fail");
    uint8_t badData = 0;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1; // too short
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: ramp up rate tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_RampUp_ValidData)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_rampup");
    uint32_t rampUp = 5000; // 5 W/s
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = sizeof(rampUp);
    sample.data = reinterpret_cast<const uint8_t*>(&rampUp);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampUpRate(), 5.0);
}

// =========================================================================
// handleSample: ramp up rate decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_RampUp_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_rampup_fail");
    uint8_t badData = 0;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: ramp down rate tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_RampDown_ValidData)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_rampdown");
    uint32_t rampDown = 3000;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = sizeof(rampDown);
    sample.data = reinterpret_cast<const uint8_t*>(&rampDown);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampDownRate(), 3.0);
}

// =========================================================================
// handleSample: ramp down rate decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_RampDown_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_rampdown_fail");
    uint8_t badData = 0;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: ramp down hysteresis tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_Hysteresis_ValidData)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_hyst");
    uint32_t hyst = 2000;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = sizeof(hyst);
    sample.data = reinterpret_cast<const uint8_t*>(&hyst);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampDownHysteresis(), 2.0);
}

// =========================================================================
// handleSample: ramp down hysteresis decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_Hysteresis_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_hyst_fail");
    uint8_t badData = 0;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: secondary floor tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_SecondaryFloor_ValidData)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_secfloor");
    uint32_t secFloor = 4000;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = sizeof(secFloor);
    sample.data = reinterpret_cast<const uint8_t*>(&secFloor);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: secondary floor decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_SecondaryFloor_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_secfloor_fail");
    uint8_t badData = 0;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: activation window multiplier tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_ActivationWindowMultiplier_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_actwin");
    uint8_t val = 10;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(val);
    sample.data = &val;
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: activation window multiplier decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       HandleSample_ActivationWindowMultiplier_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_actwin_fail");
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: target window multiplier tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_TargetWindowMultiplier_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_targwin");
    uint8_t val = 5;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = sizeof(val);
    sample.data = &val;
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: target window multiplier decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_TargetWindowMultiplier_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_targwin_fail");
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: activation offset tag
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_ActivationOffset_Valid)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_actoff");
    uint32_t val = 1000;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(val);
    sample.data = reinterpret_cast<const uint8_t*>(&val);
    sample.valid = true;
    EXPECT_EQ(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// handleSample: activation offset decode failure
// =========================================================================
TEST_F(OemAdminProfileV2Test, HandleSample_ActivationOffset_DecodeFail)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/hs_actoff_fail");
    uint8_t badData = 0;
    OemAdminProfileIntfV2::TelemetrySample sample{};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 1;
    sample.data = &badData;
    sample.valid = true;
    EXPECT_NE(intf->handleSample(sample), NSM_SW_SUCCESS);
}

// =========================================================================
// overrideAdminProfileParam: postPatchIO fails
// =========================================================================
TEST_F(OemAdminProfileV2Test, OverrideParam_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/override_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp(), NSM_ERROR));

    intf->overrideAdminProfileParam(1, 5.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// overrideAdminProfileParam: decode error CC
// =========================================================================
TEST_F(OemAdminProfileV2Test, OverrideParam_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/override_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp(NSM_ERROR)));

    intf->overrideAdminProfileParam(1, 5.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// overrideAdminProfileParam: success + V2 path (isCommandSupported true)
// =========================================================================
TEST_F(OemAdminProfileV2Test, OverrideParam_Success_V2Path)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/override_v2ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = true;
    // First call: setup override, second: V2 get
    auto v2Resp = makeV2Response(NSM_SUCCESS, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(v2Resp));

    intf->overrideAdminProfileParam(0, 50.0, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// overrideAdminProfileParam: success + V1 fallback (isCommandSupported false)
// =========================================================================
TEST_F(OemAdminProfileV2Test, OverrideParam_Success_V1Fallback)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/override_v1fb");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    intf->overrideAdminProfileParam(1, 10.0, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// overrideAdminProfileParam: parameterId 5 or 6 path (doubleToNvU8)
// =========================================================================
TEST_F(OemAdminProfileV2Test, OverrideParam_ParamId5_UsesNvU8)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/override_pid5");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    intf->overrideAdminProfileParam(5, 3.0, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(OemAdminProfileV2Test, OverrideParam_ParamId6_UsesNvU8)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/override_pid6");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    intf->overrideAdminProfileParam(6, 2.0, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// resetAdminProfileParam: postPatchIO fails
// =========================================================================
TEST_F(OemAdminProfileV2Test, ResetParam_PostPatchIOFails_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/reset_piof");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp(), NSM_ERROR));

    intf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// resetAdminProfileParam: error CC
// =========================================================================
TEST_F(OemAdminProfileV2Test, ResetParam_ErrorCC_WriteFailure)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/reset_errcc");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp(NSM_ERROR)));

    intf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// resetAdminProfileParam: success + V2 path
// =========================================================================
TEST_F(OemAdminProfileV2Test, ResetParam_Success_V2Path)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/reset_v2ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = true;
    auto v2Resp = makeV2Response(NSM_SUCCESS, 0);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(v2Resp));

    intf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// resetAdminProfileParam: success + V1 fallback
// =========================================================================
TEST_F(OemAdminProfileV2Test, ResetParam_Success_V1Fallback)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/reset_v1fb");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    intf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// resetParam: check both branches
// =========================================================================
TEST_F(OemAdminProfileV2Test, ResetParam_InvalidLimit_True)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/resetp_inv");
    EXPECT_TRUE(intf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
    EXPECT_FALSE(intf->resetParam(100.0));
}

// =========================================================================
// getAdminProfileFromDeviceV2: postPatchIO fails
// =========================================================================
TEST_F(OemAdminProfileV2Test, GetV2_PostPatchIOFails)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/getv2_piof");

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeV2Response(NSM_SUCCESS, 0), NSM_ERROR));

    intf->getAdminProfileFromDeviceV2(gpu);
}

// =========================================================================
// getAdminProfileFromDeviceV2: error CC
// =========================================================================
TEST_F(OemAdminProfileV2Test, GetV2_ErrorCC)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/getv2_errcc");

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeV2Response(NSM_ERROR, 0)));

    intf->getAdminProfileFromDeviceV2(gpu);
}

// =========================================================================
// getAdminProfileFromDeviceV2: success with samples
// =========================================================================
TEST_F(OemAdminProfileV2Test, GetV2_Success_WithSamples)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/getv2_samples");

    uint32_t rampUp = 5000;
    std::vector<uint8_t> rampUpData(reinterpret_cast<uint8_t*>(&rampUp),
                                    reinterpret_cast<uint8_t*>(&rampUp) + 4);
    auto v2Resp = makeV2Response(
        NSM_SUCCESS, 1, {{ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG, rampUpData}});

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(v2Resp));

    intf->getAdminProfileFromDeviceV2(gpu);
    EXPECT_DOUBLE_EQ(intf->AdminPowerProfileIntf::rampUpRate(), 5.0);
}

// =========================================================================
// setSecondaryPowerFloor: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       SetSecondaryPowerFloor_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setsec_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        intf->setSecondaryPowerFloor(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// setSecondaryPowerFloor: valid value (non-reset)
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetSecondaryPowerFloor_ValidValue_CallsOverride)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setsec_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{50.0};
    intf->setSecondaryPowerFloor(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setSecondaryPowerFloor: INVALID_POWER_LIMIT calls reset
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetSecondaryPowerFloor_InvalidLimit_CallsReset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setsec_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setSecondaryPowerFloor(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPrimaryFloorActivationWindowMultiplier: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       SetPrimaryFloorActWin_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpfaw_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorActivationWindowMultiplier(nonDouble, &status,
                                                        gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// setPrimaryFloorActivationWindowMultiplier: valid value
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetPrimaryFloorActWin_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpfaw_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{3.0};
    intf->setPrimaryFloorActivationWindowMultiplier(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPrimaryFloorTargetWindowMultiplier: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       SetPrimaryFloorTargWin_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpftw_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = bool{true};
    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorTargetWindowMultiplier(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// setPrimaryFloorTargetWindowMultiplier: valid value
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetPrimaryFloorTargWin_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpftw_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{2.0};
    intf->setPrimaryFloorTargetWindowMultiplier(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPrimaryFloorActivationOffset: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       SetPrimaryFloorActOff_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpfao_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorActivationOffset(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// setPrimaryFloorActivationOffset: valid value
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetPrimaryFloorActOff_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpfao_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{1.0};
    intf->setPrimaryFloorActivationOffset(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// V2 setTmpFloorPercent: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       V2_SetTmpFloorPercent_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2settmp_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        intf->setTmpFloorPercent(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// V2 setRampUpRate: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test, V2_SetRampUpRate_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2setrampup_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = bool{true};
    EXPECT_THROW_COROUTINE(
        intf->setRampUpRate(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// V2 setRampDownRate: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       V2_SetRampDownRate_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2setrampdn_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = bool{false};
    EXPECT_THROW_COROUTINE(
        intf->setRampDownRate(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// V2 setRampDownHysteresis: non-double throws
// =========================================================================
TEST_F(OemAdminProfileV2Test,
       V2_SetRampDownHysteresis_NonDouble_ThrowsInvalidArgument)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2sethyst_nondbl");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    AsyncSetOperationValueType nonDouble = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        intf->setRampDownHysteresis(nonDouble, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =========================================================================
// V2 setTmpFloorPercent: valid value (override path)
// =========================================================================
TEST_F(OemAdminProfileV2Test, V2_SetTmpFloorPercent_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2settmp_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{50.0};
    intf->setTmpFloorPercent(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// V2 setTmpFloorPercent: INVALID_POWER_LIMIT (reset path)
// =========================================================================
TEST_F(OemAdminProfileV2Test, V2_SetTmpFloorPercent_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2settmp_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setTmpFloorPercent(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// V2 setRampUpRate: valid + reset paths
// =========================================================================
TEST_F(OemAdminProfileV2Test, V2_SetRampUpRate_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2setrampup_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{10.0};
    intf->setRampUpRate(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(OemAdminProfileV2Test, V2_SetRampUpRate_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2setrampup_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setRampUpRate(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// V2 setRampDownRate: valid + reset paths
// =========================================================================
TEST_F(OemAdminProfileV2Test, V2_SetRampDownRate_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2setrampdn_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{8.0};
    intf->setRampDownRate(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(OemAdminProfileV2Test, V2_SetRampDownRate_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2setrampdn_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setRampDownRate(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// V2 setRampDownHysteresis: valid + reset paths
// =========================================================================
TEST_F(OemAdminProfileV2Test, V2_SetRampDownHysteresis_ValidValue)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2sethyst_ok");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType val = double{2.0};
    intf->setRampDownHysteresis(val, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(OemAdminProfileV2Test, V2_SetRampDownHysteresis_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/v2sethyst_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setRampDownHysteresis(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPrimaryFloorActivationWindowMultiplier: reset path
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetPrimaryFloorActWin_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpfaw_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setPrimaryFloorActivationWindowMultiplier(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPrimaryFloorTargetWindowMultiplier: reset path
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetPrimaryFloorTargWin_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpftw_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setPrimaryFloorTargetWindowMultiplier(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}

// =========================================================================
// setPrimaryFloorActivationOffset: reset path
// =========================================================================
TEST_F(OemAdminProfileV2Test, SetPrimaryFloorActOff_Reset)
{
    auto intf = makeIntf("/xyz/openbmc_project/ps/v2/setpfao_reset");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_CALL(mockManager, getEid(_)).WillRepeatedly(Return(5));
    gpu->messageTypesToCommandCodeMatrix
        [NSM_TYPE_PLATFORM_ENVIRONMENTAL]
        [NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2] = false;
    nsm_admin_override_data data{};
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupResp()))
        .WillOnce(mockPostPatchIO(makeQueryAdminV1Resp(NSM_SUCCESS, data)));

    AsyncSetOperationValueType invalidVal =
        static_cast<double>(INVALID_POWER_LIMIT);
    intf->setPrimaryFloorActivationOffset(invalidVal, &status, gpu);
    EXPECT_NE(status, AsyncOperationStatusType::WriteFailure);
}
