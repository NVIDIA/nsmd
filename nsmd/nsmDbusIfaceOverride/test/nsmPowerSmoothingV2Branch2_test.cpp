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

/*
 * Branch coverage tests batch 2 for nsmPowerSmoothing-v2.hpp
 *
 * Targets half-covered branches in the V2 sensor classes:
 *   - NsmPowerSmoothingV2::genRequestMsg (V2 supported vs fallback)
 *   - NsmPowerSmoothingV2::handleResponseMsg (V2 decode success/fail/cc)
 *   - NsmPowerSmoothingV2::handleSample dispatch branches
 *   - NsmCurrentPowerSmoothingProfileV2::genRequestMsg, handleResponseMsg
 *   - NsmCurrentPowerSmoothingProfileV2::handleSample all tags
 *   - NsmCurrentPowerSmoothingProfileV2::updateCurrentTMPFloorSample
 *     INVALID_UINT16_VALUE branch
 *   - NsmCurrentPowerSmoothingProfileV2::updateAdminOverrideMaskSample
 *     bits TRUE/FALSE
 *   - NsmPowerSmoothingAdminOverrideV2::genRequestMsg, handleResponseMsg
 *   - NsmPowerSmoothingAdminOverrideV2::handleSample + update methods
 *     decode-fail paths
 *   - NsmPowerProfileCollectionV2::genRequestMsg, handleResponseMsg
 *   - NsmPowerProfileCollectionV2::handleSample with auto-creation +
 *     each tag
 *   - NsmPowerSmoothingSupportedVersion::genRequestMsg encode fail,
 *     handleResponseMsg cc!=0, "nan" path
 *   - OemAdminProfileIntfV2 overrideAdminProfileParam parameterId
 *     dispatch (0, 5, 6, else)
 *   - OemPowerSmoothingFeatIntfV2 setter valid-value paths
 *   - OemPowerProfileIntfV2 setter valid-value paths
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmPowerSmoothing-v2.hpp"
#include "nsmPowerSmoothingAdminProfileIntf-v2.hpp"
#include "nsmPowerSmoothingFeatureIntf-v2.hpp"
#include "nsmPowerSmoothingPowerProfileIntf-v2.hpp"

#undef private
#undef protected

using namespace nsm;

// =============================================================================
// Helpers
// =============================================================================
static void encodeUint32LE(uint8_t* buf, uint32_t val)
{
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((val >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((val >> 24) & 0xFF);
}

static void encodeUint16LE(uint8_t* buf, uint16_t val)
{
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

// Enable V2 feature info command
static void enableFeatInfoV2(std::shared_ptr<NsmDevice> dev)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 % 8));
    dev->updateMessageTypesToCommandCodeMatrix(
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, codes,
        SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// Enable V2 current profile info command
static void enableCurProfileV2(std::shared_ptr<NsmDevice> dev)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2 % 8));
    dev->updateMessageTypesToCommandCodeMatrix(
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, codes,
        SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// Enable V2 admin override query command
static void enableAdminOverrideV2(std::shared_ptr<NsmDevice> dev)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 % 8));
    dev->updateMessageTypesToCommandCodeMatrix(
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, codes,
        SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// Enable V2 preset profile info command
static void enablePresetProfileV2(std::shared_ptr<NsmDevice> dev)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2 % 8));
    dev->updateMessageTypesToCommandCodeMatrix(
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, codes,
        SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// Valgrind-safe tiny error response (min 7 bytes)
static std::vector<uint8_t> tinyErrorResp()
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    return buf;
}

// Aggregate response with 0 samples for feat info V2
static std::vector<uint8_t> makeFeatInfoV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_powersmoothing_featinfo_v2_resp(0, cc, 0, msg);
    return buf;
}

// Aggregate response with 0 samples for current profile V2
static std::vector<uint8_t> makeCurProfileV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_current_profile_info_v2_resp(0, cc, 0, msg);
    return buf;
}

// Aggregate response with 0 samples for admin override V2
static std::vector<uint8_t> makeAdminOverrideV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_admin_override_profile_info_v2_resp(0, cc, 0, msg);
    return buf;
}

// Aggregate response with 0 samples for preset profile V2
static std::vector<uint8_t> makePresetProfileV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_preset_profile_info_v2_resp(0, cc, 0, msg);
    return buf;
}

// Setup admin override response
static std::vector<uint8_t> makeSetupAdminResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_setup_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// Toggle feature state response
static std::vector<uint8_t> makeToggleFeatureResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_feature_state_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// Toggle immediate rampdown response
static std::vector<uint8_t> makeToggleRampdownResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_immediate_rampdown_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// Update preset profile param response
static std::vector<uint8_t>
    makeUpdatePresetProfileResp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_update_preset_profile_param_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// =============================================================================
// Fixture
// =============================================================================
struct PwrSmoothV2Branch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string parentPath = "/xyz/openbmc_project/inventory/v2br2/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:8";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    PwrSmoothV2Branch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~PwrSmoothV2Branch2Test()
    {
        cleanupDeviceSensors(devices);
    }

    auto& bus()
    {
        return utils::DBusHandler::getBus();
    }

    // --- Sensor factories ---

    auto makeFeatIntf()
    {
        return std::make_shared<OemPowerSmoothingFeatIntfV2>(bus(), parentPath,
                                                             gpu);
    }

    auto makeAdminIntf()
    {
        return std::make_shared<OemAdminProfileIntfV2>(bus(), parentPath, gpu);
    }

    auto makeCurProfileIntf()
    {
        auto adminPath = parentPath + "/profile/admin_profile";
        return std::make_shared<OemCurrentPowerProfileIntf>(bus(), parentPath,
                                                            adminPath, gpu);
    }

    auto makeCollection()
    {
        std::string name = "PSCollV2B2";
        std::string type = "NSM";
        std::string path = parentPath;
        return std::make_shared<NsmPowerProfileCollectionV2>(name, type, path,
                                                             gpu);
    }

    auto makeAdminOverrideSensor()
    {
        std::string name = "AdminOvV2B2";
        std::string type = "NSM";
        std::string path = parentPath;
        auto adminIntf = makeAdminIntf();
        return std::make_shared<NsmPowerSmoothingAdminOverrideV2>(
            name, type, adminIntf, path, gpu);
    }

    auto makeCurProfileSensor(
        std::shared_ptr<NsmPowerProfileCollection> collection = nullptr,
        std::shared_ptr<NsmPowerSmoothingAdminOverride> admin = nullptr)
    {
        std::string name = "CurProfV2B2";
        std::string type = "NSM";
        std::string path = parentPath;
        auto curIntf = makeCurProfileIntf();
        if (!collection)
            collection = makeCollection();
        if (!admin)
            admin = makeAdminOverrideSensor();
        return std::make_shared<NsmCurrentPowerSmoothingProfileV2>(
            name, type, path, curIntf, collection, admin, gpu);
    }
};

// =============================================================================
// NsmPowerSmoothingV2::genRequestMsg - V2 supported path
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_GenReq_V2Supported_ReturnsRequest)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    enableFeatInfoV2(gpu);
    auto req = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// =============================================================================
// NsmPowerSmoothingV2::genRequestMsg - V2 NOT supported, fallback to V1
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_GenReq_NotSupported_FallsBackV1)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    // Do not enable V2 command -> fallback
    auto req = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// =============================================================================
// NsmPowerSmoothingV2::handleResponseMsg - V2 path, success (0 samples)
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleResp_V2Success_ReturnsSuccess)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    enableFeatInfoV2(gpu);
    auto resp = makeFeatInfoV2Resp(NSM_SUCCESS);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothingV2::handleResponseMsg - V2 path, non-success cc
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleResp_V2NonSuccessCc_ReturnsCc)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    enableFeatInfoV2(gpu);
    auto resp = makeFeatInfoV2Resp(NSM_ERROR);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// =============================================================================
// NsmPowerSmoothingV2::handleResponseMsg - V2 NOT supported, falls to V1
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleResp_NotSupported_FallsBackV1)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    // V1 decode will likely fail on this V2 buffer, but exercises fallback
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothingV2::handleSample - all tag dispatch branches
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_FeatureFlag_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t flags = 0x0F; // all 4 flags set
    uint8_t data[4] = {};
    encodeUint32LE(data, flags);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_FeatureFlag_NoFlags)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t flags = 0x00; // no flags
    uint8_t data[4] = {};
    encodeUint32LE(data, flags);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_FeatureFlag_BadLen)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint8_t data[2] = {};
    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleSample_CurrentTmpSetting_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t val = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleSample_TmpFloorSetting_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t val = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_MaxTmpFloor_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint16_t val = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_MinTmpFloor_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint16_t val = 1024;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleSample_FloorWindowMult_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t val = 7000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleSample_MinPrimFloorActOffset_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t val = 9000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PwrSmoothingV2_HandleSample_MinPrimFloorActPoint_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint32_t val = 11000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);

    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_ReservedTag_Success)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint8_t data[4] = {};
    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, PwrSmoothingV2_HandleSample_UnknownTag_Error)
{
    std::string name = "PSV2";
    std::string type = "NSM";
    std::string path = parentPath;
    auto featIntf = makeFeatIntf();
    NsmPowerSmoothingV2 sensor(name, type, path, featIntf, gpu);

    uint8_t data[4] = {};
    NsmPowerSmoothingV2::TelemetrySample sample = {};
    sample.tag = 100; // unreserved but unknown
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor.handleSample(sample), NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfileV2 - genRequestMsg
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_GenReq_V2Supported_ReturnsRequest)
{
    auto sensor = makeCurProfileSensor();
    enableCurProfileV2(gpu);
    auto req = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_GenReq_NotSupported_FallsBackV1)
{
    auto sensor = makeCurProfileSensor();
    auto req = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// =============================================================================
// NsmCurrentPowerSmoothingProfileV2 - handleResponseMsg
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleResp_V2Success_Returns0)
{
    auto sensor = makeCurProfileSensor();
    enableCurProfileV2(gpu);
    auto resp = makeCurProfileV2Resp(NSM_SUCCESS);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleResp_V2NonSuccessCc_ReturnsCc)
{
    auto sensor = makeCurProfileSensor();
    enableCurProfileV2(gpu);
    auto resp = makeCurProfileV2Resp(NSM_ERROR);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleResp_NotSupported_FallsBackV1)
{
    auto sensor = makeCurProfileSensor();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmCurrentPowerSmoothingProfileV2::handleSample - all tag dispatch
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_ActivePresetProfileId_Success)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[1] = {0};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = ACTIVE_PRESET_PROFILE_ID_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_ActivePresetProfileId_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[4] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = ACTIVE_PRESET_PROFILE_ID_TAG;
    sample.data_len = 4; // needs 1
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_AdminOverrideMask_AllBitsSet)
{
    auto sensor = makeCurProfileSensor();
    uint16_t mask = 0xFF; // bits 0-7 set
    uint8_t data[2] = {};
    encodeUint16LE(data, mask);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_MASK_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_AdminOverrideMask_NoBitsSet)
{
    auto sensor = makeCurProfileSensor();
    uint16_t mask = 0x00;
    uint8_t data[2] = {};
    encodeUint16LE(data, mask);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_MASK_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_AdminOverrideMask_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[1] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_MASK_TAG;
    sample.data_len = 1; // needs 2
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_TmpFloor_ValidValue)
{
    auto sensor = makeCurProfileSensor();
    uint16_t val = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_TmpFloor_InvalidUint16)
{
    auto sensor = makeCurProfileSensor();
    uint16_t val = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_TmpFloor_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[1] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_RampUpRate_Success)
{
    auto sensor = makeCurProfileSensor();
    uint32_t val = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_RAMPUP_RATE_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_RampDownRate_Success)
{
    auto sensor = makeCurProfileSensor();
    uint32_t val = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_RAMPDOWN_RATE_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_RampDownHysteresis_Success)
{
    auto sensor = makeCurProfileSensor();
    uint32_t val = 2000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_SecondaryFloor_Success)
{
    auto sensor = makeCurProfileSensor();
    uint32_t val = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_SECONDARY_FLOOR_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_PrimFloorActWinMult_Success)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[1] = {42};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_PrimFloorTargetWin_Success)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[1] = {33};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_PrimFloorActOffset_Success)
{
    auto sensor = makeCurProfileSensor();
    uint32_t val = 12000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_ReservedTag_Success)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[4] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_UnknownTag_Error)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[4] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = 100;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_ERROR_LENGTH);
}

// Decode-fail paths for remaining tags
TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_RampUpRate_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[2] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_RAMPUP_RATE_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_RampDownRate_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[1] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_RAMPDOWN_RATE_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_RampDownHysteresis_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[2] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CurProfileV2_HandleSample_SecondaryFloor_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[2] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_SECONDARY_FLOOR_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_PrimFloorActWinMult_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[4] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_PrimFloorTargetWin_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[4] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CurProfileV2_HandleSample_PrimFloorActOffset_BadLen)
{
    auto sensor = makeCurProfileSensor();
    uint8_t data[2] = {};
    NsmCurrentPowerSmoothingProfileV2::TelemetrySample sample = {};
    sample.tag = CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothingAdminOverrideV2 - genRequestMsg
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_GenReq_V2Supported_ReturnsRequest)
{
    auto sensor = makeAdminOverrideSensor();
    enableAdminOverrideV2(gpu);
    auto req = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_GenReq_NotSupported_FallsBackV1)
{
    auto sensor = makeAdminOverrideSensor();
    auto req = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// =============================================================================
// NsmPowerSmoothingAdminOverrideV2 - handleResponseMsg
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleResp_V2Success_Returns0)
{
    auto sensor = makeAdminOverrideSensor();
    enableAdminOverrideV2(gpu);
    auto resp = makeAdminOverrideV2Resp(NSM_SUCCESS);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleResp_V2NonSuccessCc_ReturnsCc)
{
    auto sensor = makeAdminOverrideSensor();
    enableAdminOverrideV2(gpu);
    auto resp = makeAdminOverrideV2Resp(NSM_ERROR);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleResp_NotSupported_FallsBackV1)
{
    auto sensor = makeAdminOverrideSensor();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothingAdminOverrideV2::handleSample - all tags + decode-fail
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_TmpFloor_ValidValue)
{
    auto sensor = makeAdminOverrideSensor();
    uint16_t val = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_TmpFloor_InvalidUint16)
{
    auto sensor = makeAdminOverrideSensor();
    uint16_t val = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_TmpFloor_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[1] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_RampUpRate_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint32_t val = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_RampUpRate_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[2] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_RampDownRate_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint32_t val = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_RampDownRate_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[1] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_Hysteresis_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint32_t val = 2500;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_Hysteresis_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[2] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_SecondaryFloor_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint32_t val = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_SecondaryFloor_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[2] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_PrimFloorActWinMult_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[1] = {55};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_PrimFloorActWinMult_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[4] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_PrimFloorTargetWin_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[1] = {33};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_PrimFloorTargetWin_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[4] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_PrimFloorActOffset_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint32_t val = 12500;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminOverrideV2_HandleSample_PrimFloorActOffset_BadLen)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[2] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_ReservedTag_Success)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[4] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, AdminOverrideV2_HandleSample_UnknownTag_Error)
{
    auto sensor = makeAdminOverrideSensor();
    uint8_t data[4] = {};
    NsmPowerSmoothingAdminOverrideV2::TelemetrySample sample = {};
    sample.tag = 100;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(sensor->handleSample(sample), NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// NsmPowerProfileCollectionV2 - genRequestMsg
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_GenReq_V2Supported_ReturnsRequest)
{
    auto collection = makeCollection();
    enablePresetProfileV2(gpu);
    auto req = collection->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_GenReq_NotSupported_FallsBackV1)
{
    auto collection = makeCollection();
    auto req = collection->genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// =============================================================================
// NsmPowerProfileCollectionV2 - handleResponseMsg
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleResp_V2Success_Returns0)
{
    auto collection = makeCollection();
    enablePresetProfileV2(gpu);
    auto resp = makePresetProfileV2Resp(NSM_SUCCESS);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleResp_V2NonSuccessCc_ReturnsCc)
{
    auto collection = makeCollection();
    enablePresetProfileV2(gpu);
    auto resp = makePresetProfileV2Resp(NSM_ERROR);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleResp_NotSupported_FallsBackV1)
{
    auto collection = makeCollection();
    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = collection->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerProfileCollectionV2::handleSample - auto-creation + tags
// Tag format: (tag_type << 3) | profId
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_TmpFloor_AutoCreatesProfile)
{
    auto collection = makeCollection();
    uint8_t profId = 0;
    uint8_t tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | profId;
    uint16_t val = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_TRUE(collection->hasProfileId(profId));
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_TmpFloor_InvalidUint16)
{
    auto collection = makeCollection();
    uint8_t profId = 1;
    uint8_t tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | profId;
    uint16_t val = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_TmpFloor_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | 0;
    uint8_t data[1] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_RampUpRate_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | 0;
    uint32_t val = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_RampDownRate_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3) | 0;
    uint32_t val = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_RampDownHysteresis_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | 0;
    uint32_t val = 2000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_SecondaryFloor_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | 0;
    uint32_t val = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_PrimFloorActWinMult_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
                   << 3) |
                  0;
    uint8_t data[1] = {55};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_PrimFloorTargetWin_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) | 0;
    uint8_t data[1] = {33};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_PrimFloorActOffset_Success)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) | 0;
    uint32_t val = 12000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_UnknownTag_Error)
{
    auto collection = makeCollection();
    // tag_type = 20 (unknown) >> 3 extracted to unknown
    uint8_t tag = (20 << 3) | 0;
    uint8_t data[4] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_ERROR_LENGTH);
}

// Existing profile: second call with same profId should NOT re-create
TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_ExistingProfile_NoReCreation)
{
    auto collection = makeCollection();
    uint8_t profId = 2;
    uint8_t tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | profId;
    uint32_t val = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // First call creates the profile
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
    EXPECT_TRUE(collection->hasProfileId(profId));

    // Second call with different data, same profId
    encodeUint32LE(data, 9000);
    EXPECT_EQ(collection->handleSample(sample), NSM_SW_SUCCESS);
}

// Decode-fail paths for collection
TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_RampUpRate_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | 0;
    uint8_t data[2] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_RampDownRate_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3) | 0;
    uint8_t data[1] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_Hysteresis_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | 0;
    uint8_t data[2] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test, CollectionV2_HandleSample_SecondaryFloor_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | 0;
    uint8_t data[2] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_PrimFloorActWinMult_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
                   << 3) |
                  0;
    uint8_t data[4] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_PrimFloorTargetWin_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) | 0;
    uint8_t data[4] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       CollectionV2_HandleSample_PrimFloorActOffset_BadLen)
{
    auto collection = makeCollection();
    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) | 0;
    uint8_t data[2] = {};
    NsmPowerProfileCollectionV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_NE(collection->handleSample(sample), NSM_SW_SUCCESS);
}

// =============================================================================
// NsmPowerSmoothingSupportedVersion - genRequestMsg encode fail
// =============================================================================

struct SupportedVersionBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string parentPath = "/xyz/openbmc_project/inventory/v2br2/sv";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:7";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    std::string name = "SVB2";
    std::string type = "NSM";
    std::string inventoryObjPath = parentPath;
    uint8_t messageType = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    std::shared_ptr<RevisionIntf> revisionIntf;

    SupportedVersionBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        revisionIntf = std::make_shared<RevisionIntf>(
            utils::DBusHandler::getBus(), parentPath.c_str());
    }

    ~SupportedVersionBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// handleResponseMsg: cc != NSM_SUCCESS → returns cc (cc ? cc : rc)
TEST_F(SupportedVersionBranch2Test, HandleResp_NonSuccessCc_ReturnsCc)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    encode_get_supported_command_codes_resp(0, NSM_ERROR, ERR_NULL, codes,
                                            responseMsg);

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// handleResponseMsg: decode fails (tiny buffer) → returns rc
TEST_F(SupportedVersionBranch2Test, HandleResp_DecodeFail_ReturnsNonZero)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    auto resp = tinyErrorResp();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor.handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// handleResponseMsg: no V1 or V2 commands → version = "nan"
TEST_F(SupportedVersionBranch2Test, HandleResp_NoV1OrV2Commands_SetsNan)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // No command bits set at all
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            responseMsg);

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.getSupportedVersion(), "nan");
}

// genRequestMsg: success path (always succeeds for valid instanceId)
TEST_F(SupportedVersionBranch2Test, GenReq_Success_ReturnsRequest)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);
    auto req = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(req.has_value());
}

// =============================================================================
// OemAdminProfileIntfV2 - overrideAdminProfileParam parameterId dispatch
// parameterId==0 → doubleToNvUFXP4_12, ==5 → doubleToNvU8, ==6 → doubleToNvU8,
// else → paramValue * 1000
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_OverrideParam_ParamId0_FxpConversion)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(0, 0.5, &status, gpu);
    // No crash; encode with FXP conversion
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_OverrideParam_ParamId5_NvU8Conversion)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(5, 1.5, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_OverrideParam_ParamId6_NvU8Conversion)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(6, 2.0, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_OverrideParam_ParamIdElse_ScaledBy1000)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(3, 5.0, &status, gpu);
}

// overrideAdminProfileParam: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_OverrideParam_PostPatchIOFail)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// overrideAdminProfileParam: decode fails (cc != SUCCESS)
TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_OverrideParam_DecodeFail)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// resetAdminProfileParam: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_ResetParam_PostPatchIOFail)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// resetAdminProfileParam: decode fails
TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_ResetParam_DecodeFail)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// OemAdminProfileIntfV2 - setter valid-value paths (non-reset)
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_SetTmpFloorPercent_ValidValue_Calls_Override)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{50.0};
    adminIntf->setTmpFloorPercent(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_SetRampUpRate_ValidValue_Calls_Override)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{5.0};
    adminIntf->setRampUpRate(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_SetRampDownRate_ValidValue_Calls_Override)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{3.0};
    adminIntf->setRampDownRate(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_SetRampDownHysteresis_ValidValue)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{2.0};
    adminIntf->setRampDownHysteresis(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_SetSecondaryPowerFloor_ValidValue)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{15.0};
    adminIntf->setSecondaryPowerFloor(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_SetPrimFloorActWinMult_ValidValue)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{1.5};
    adminIntf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_SetPrimFloorTargetWinMult_ValidValue)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{2.0};
    adminIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_SetPrimFloorActOffset_ValidValue)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{100.0};
    adminIntf->setPrimaryFloorActivationOffset(value, &status, gpu);
}

// Setter reset paths (INVALID_POWER_LIMIT value)
TEST_F(PwrSmoothV2Branch2Test,
       AdminIntfV2_SetTmpFloorPercent_ResetValue_Calls_Reset)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    adminIntf->setTmpFloorPercent(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_SetRampUpRate_ResetValue)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    adminIntf->setRampUpRate(value, &status, gpu);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 - valid-value paths for
// setPowerSmoothingEnabled / setImmediateRampDownEnabled
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_SetPowerSmoothingEnabled_True)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    featIntf->setPowerSmoothingEnabled(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_SetPowerSmoothingEnabled_False)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    featIntf->setPowerSmoothingEnabled(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_SetImmediateRampDownEnabled_True)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    featIntf->setImmediateRampDownEnabled(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_SetImmediateRampDownEnabled_False)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = false;
    featIntf->setImmediateRampDownEnabled(value, &status, gpu);
}

// togglePowerSmoothingOnDevice: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_TogglePwrSmoothing_PostPatchIOFail)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// togglePowerSmoothingOnDevice: decode fails (cc != SUCCESS)
TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_TogglePwrSmoothing_DecodeFail)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// toggleImmediateRampDownOnDevice: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_ToggleRampDown_PostPatchIOFail)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// toggleImmediateRampDownOnDevice: decode fails
TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_ToggleRampDown_DecodeFail)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// OemPowerProfileIntfV2 - setter valid-value (non-reset) paths
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetTmpFloorPercent_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{50.0};
    profileIntf->setTmpFloorPercent(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetRampUpRate_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{5.0};
    profileIntf->setRampUpRate(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetRampDownRate_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{3.0};
    profileIntf->setRampDownRate(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetRampDownHysteresis_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{2.0};
    profileIntf->setRampDownHysteresis(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetSecondaryPowerFloor_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{15.0};
    profileIntf->setSecondaryPowerFloor(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetPrimFloorActWinMult_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{1.5};
    profileIntf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_SetPrimFloorTargetWinMult_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{2.0};
    profileIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetPrimFloorActOffset_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{100.0};
    profileIntf->setPrimaryFloorActivationOffset(value, &status, gpu);
}

// Setter reset paths (INVALID_POWER_LIMIT)
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetTmpFloorPercent_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setTmpFloorPercent(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetRampUpRate_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setRampUpRate(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetRampDownRate_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setRampDownRate(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetRampDownHysteresis_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setRampDownHysteresis(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetSecondaryPowerFloor_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setSecondaryPowerFloor(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetPrimFloorActWinMult_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_SetPrimFloorTargetWinMult_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_SetPrimFloorActOffset_ResetValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setPrimaryFloorActivationOffset(value, &status, gpu);
}

// =============================================================================
// OemPowerProfileIntfV2 - updateProfileInfoOnDevice parameterId dispatch
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_UpdateProfileInfo_ParamId0_FxpConversion)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(0, 0.5, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_UpdateProfileInfo_ParamId5_NvU8)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(5, 1.5, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_UpdateProfileInfo_ParamId6_NvU8)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(6, 2.0, &status, gpu);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_UpdateProfileInfo_ParamIdElse_ScaledBy1000)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(3, 5.0, &status, gpu);
}

// resetProfileInfoOnDevice: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_ResetProfileInfo_PostPatchIOFail)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->resetProfileInfoOnDevice(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// resetProfileInfoOnDevice: decode fails
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_ResetProfileInfo_DecodeFail)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->resetProfileInfoOnDevice(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// updateProfileInfoOnDevice: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_UpdateProfileInfo_PostPatchIOFail)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// updateProfileInfoOnDevice: decode fails
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_UpdateProfileInfo_DecodeFail)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileResp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// OemPowerProfileIntfV2::handleSample - profId mismatch (profId != profIdRes)
// Covers FALSE branch of (profId == profIdRes) in each update method
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_TmpFloor_ProfIdMismatch)
{
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(
        bus(), parentPath, 0, gpu); // profId=0

    // tag encodes profIdRes=1 (mismatch with profId=0)
    uint8_t tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | 1;
    uint16_t val = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    // Succeeds but does not update the property
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_RampUp_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | 1;
    uint32_t val = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_RampDown_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3) | 1;
    uint32_t val = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_Hysteresis_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | 1;
    uint32_t val = 2000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_SecFloor_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | 1;
    uint32_t val = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_PrimActWinMult_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
                   << 3) |
                  1;
    uint8_t data[1] = {55};
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_PrimTargetWin_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) | 1;
    uint8_t data[1] = {33};
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_PrimActOffset_ProfIdMismatch)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) | 1;
    uint32_t val = 12000;
    uint8_t data[4] = {};
    encodeUint32LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

// OemPowerProfileIntfV2::handleSample - unknown tag
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_HandleSample_UnknownTag_Error)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (20 << 3) | 0;
    uint8_t data[4] = {};
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_ERROR_LENGTH);
}

// OemPowerProfileIntfV2::handleSample - profId match, TmpFloor InvalidUint16
TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_TmpFloor_Match_InvalidUint16)
{
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(
        bus(), parentPath, 0, gpu); // profId=0

    uint8_t tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | 0;
    uint16_t val = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

// OemPowerProfileIntfV2::handleSample - profId match, TmpFloor valid value
TEST_F(PwrSmoothV2Branch2Test,
       PowerProfileV2_HandleSample_TmpFloor_Match_ValidValue)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    uint8_t tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | 0;
    uint16_t val = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, val);
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = tag;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;
    EXPECT_EQ(profileIntf->handleSample(sample), NSM_SW_SUCCESS);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 - V2 path (isCommandSupported)
// togglePowerSmoothingOnDevice with V2 enabled → calls
// getPwrSmoothingControlsFromDeviceV2
// =============================================================================

TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_TogglePwrSmoothing_V2Path_Success)
{
    auto featIntf = makeFeatIntf();
    enableFeatInfoV2(gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureResp()))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_ToggleRampDown_V2Path_Success)
{
    auto featIntf = makeFeatIntf();
    enableFeatInfoV2(gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownResp()))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// getPwrSmoothingControlsFromDeviceV2: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test,
       FeatIntfV2_GetPwrSmoothingControlsV2_PostPatchIOFail)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    featIntf->getPwrSmoothingControlsFromDeviceV2(gpu);
    // No crash expected
}

// getPwrSmoothingControlsFromDeviceV2: decode fails (error CC)
TEST_F(PwrSmoothV2Branch2Test, FeatIntfV2_GetPwrSmoothingControlsV2_DecodeFail)
{
    auto featIntf = makeFeatIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp(NSM_ERROR)));

    featIntf->getPwrSmoothingControlsFromDeviceV2(gpu);
}

// getAdminProfileFromDeviceV2: decode fails (error CC)
TEST_F(PwrSmoothV2Branch2Test, AdminIntfV2_GetAdminProfileV2_DecodeFail)
{
    auto adminIntf = makeAdminIntf();

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeAdminOverrideV2Resp(NSM_ERROR)));

    adminIntf->getAdminProfileFromDeviceV2(gpu);
}

// getProfileInfoFromDeviceV2: postPatchIO fail
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_GetProfileInfoV2_PostPatchIOFail)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    profileIntf->getProfileInfoFromDeviceV2(gpu);
}

// getProfileInfoFromDeviceV2: decode fails (error CC)
TEST_F(PwrSmoothV2Branch2Test, PowerProfileV2_GetProfileInfoV2_DecodeFail)
{
    auto profileIntf =
        std::make_shared<OemPowerProfileIntfV2>(bus(), parentPath, 0, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makePresetProfileV2Resp(NSM_ERROR)));

    profileIntf->getProfileInfoFromDeviceV2(gpu);
}
