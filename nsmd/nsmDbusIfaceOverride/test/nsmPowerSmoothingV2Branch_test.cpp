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
 * Branch coverage tests for PowerSmoothing V2 D-Bus interface override
 * headers:
 *   nsmPowerSmoothingAdminProfileIntf-v2.hpp
 *   nsmPowerSmoothingPowerProfileIntf-v2.hpp
 *   nsmPowerSmoothingFeatureIntf-v2.hpp
 *
 * Targets setter coroutine error paths:
 *   - InvalidArgument throw when value type is wrong
 *   - resetParam() TRUE branch (INVALID_POWER_LIMIT value)
 *   - overrideAdminProfileParam / updateProfileInfoOnDevice postPatchIO fail
 *   - decode response failure paths
 *   - Feature toggle: setPowerSmoothingEnabled, setImmediateRampDownEnabled
 *   - handleSample: reserved tag (>
 * NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
 *   - handleSample: success paths with matching data
 *   - Admin profile: INVALID_UINT16_VALUE for tmpFloorPercent
 *   - parameterId dispatch branches (0, 5, 6, else)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmPowerSmoothingAdminProfileIntf-v2.hpp"
#include "nsmPowerSmoothingFeatureIntf-v2.hpp"
#include "nsmPowerSmoothingPowerProfileIntf-v2.hpp"

#undef private
#undef protected

using namespace nsm;

// =============================================================================
// Helper: Encode a uint32_t value into a 4-byte little-endian buffer
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

// =============================================================================
// Fixture
// =============================================================================
struct PowerSmoothingV2BranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string parentPath = "/xyz/openbmc_project/inventory/v2br/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:9";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    PowerSmoothingV2BranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~PowerSmoothingV2BranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// PART 1: OemAdminProfileIntfV2 - setter InvalidArgument throw paths //

// =============================================================================

TEST_F(PowerSmoothingV2BranchTest, AdminV2_SetTmpFloorPercent_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass bool instead of double
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setTmpFloorPercent(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_SetRampUpRate_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setRampUpRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_SetRampDownRate_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setRampDownRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_SetRampDownHysteresis_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setRampDownHysteresis(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_SetSecondaryPowerFloor_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setSecondaryPowerFloor(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_SetPrimaryFloorActivationWindowMultiplier_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_SetPrimaryFloorTargetWindowMultiplier_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_SetPrimaryFloorActivationOffset_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorActivationOffset(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// PART 2: OemAdminProfileIntfV2 - handleSample success paths with matching
// data and reserved tag
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_ReservedTag_ReturnsSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_TmpFloor_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint16_t value = 2048; // Valid UFXP4.12
    uint8_t data[2] = {};
    encodeUint16LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_TmpFloor_InvalidValue)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    // INVALID_UINT16_VALUE triggers the special branch
    uint16_t value = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_RampUpRate_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint32_t value = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_RampDownRate_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint32_t value = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_Hysteresis_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint32_t value = 2500;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_SecondaryFloor_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint32_t value = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_PrimFloorActWinMult_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[1] = {55};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_PrimFloorTargetWindow_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[1] = {33};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_PrimFloorActOffset_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint32_t value = 12500;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_UnknownTag_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    // Tag 100 is unknown but <= NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
    sample.tag = 100;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// PART 3: OemAdminProfileIntfV2 - decode failure paths (bad data_len)
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_TmpFloorBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[1] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1; // needs 2
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_RampUpBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[2] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = 2; // needs 4
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_RampDownBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[1] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_HysteresisBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[3] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 3;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_HandleSample_SecFloorBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[2] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_PrimFloorActWinMultBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 4; // needs 1
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_PrimFloorTargetWinBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 4; // needs 1
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_HandleSample_PrimFloorActOffsetBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    uint8_t data[2] = {};
    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2; // needs 4
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// PART 4: OemAdminProfileIntfV2 - resetParam() TRUE/FALSE branches
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest,
       AdminV2_ResetParam_InvalidPowerLimit_ReturnsTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    EXPECT_TRUE(intf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
}

TEST_F(PowerSmoothingV2BranchTest, AdminV2_ResetParam_NormalValue_ReturnsFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath, gpu);

    EXPECT_FALSE(intf->resetParam(42.0));
}

// =============================================================================
// PART 5: OemPowerSmoothingFeatIntfV2 - setter error paths
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_SetPowerSmoothingEnabled_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass double instead of bool
    AsyncSetOperationValueType value = 1.0;

    EXPECT_THROW_COROUTINE(
        intf->setPowerSmoothingEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_SetImmediateRampDownEnabled_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = 1.0;

    EXPECT_THROW_COROUTINE(
        intf->setImmediateRampDownEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// PART 6: OemPowerSmoothingFeatIntfV2 - handleSample paths
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_ReservedTag_ReturnsSuccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[4] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_FeatureFlag_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    // All flags set: supported, enabled, rampdown, delayed
    uint32_t featureFlag = 0x0F;
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_FeatureFlag_NoFlags)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    // No flags set
    uint32_t featureFlag = 0x00;
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_CurrentTmpSetting_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint32_t value = 10000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_TmpFloorSetting_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint32_t value = 8000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_MaxTmpFloor_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint16_t value = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_MinTmpFloor_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint16_t value = 1024;
    uint8_t data[2] = {};
    encodeUint16LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_FloorWindowMult_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint32_t value = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_MinPrimFloorActOffset_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint32_t value = 7000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_MinPrimFloorActPoint_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint32_t value = 9000;
    uint8_t data[4] = {};
    encodeUint32LE(data, value);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_UnknownTag_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[4] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = 100; // Unknown but within unreserved range
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// PART 7: OemPowerSmoothingFeatIntfV2 - decode failure (bad data_len)
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_FeatureFlagBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[2] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = 2; // needs 4
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_CurrentTmpSettingBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[1] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_TmpFloorSettingBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[1] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_MaxTmpFloorBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[1] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1; // needs 2
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_MinTmpFloorBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[1] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1; // needs 2
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest, FeatureV2_HandleSample_FloorWindowMultBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[2] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_MinPrimFloorActOffsetBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[2] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2BranchTest,
       FeatureV2_HandleSample_MinPrimFloorActPointBadLen)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath,
                                                              gpu);

    uint8_t data[2] = {};
    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    int rc = intf->handleSample(sample);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// PART 8: OemPowerProfileIntfV2 - setter InvalidArgument throw paths //

// =============================================================================

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetTmpFloorPercent_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setTmpFloorPercent(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetRampUpRate_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setRampUpRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetRampDownRate_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setRampDownRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetRampDownHysteresis_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setRampDownHysteresis(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetSecondaryPowerFloor_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setSecondaryPowerFloor(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetPrimFloorActWinMult_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetPrimFloorTargetWin_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2BranchTest,
       PowerProfileV2_SetPrimFloorActOffset_WrongType_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;

    EXPECT_THROW_COROUTINE(
        intf->setPrimaryFloorActivationOffset(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// PART 9: OemPowerProfileIntfV2 - resetParam() branches
// =============================================================================

TEST_F(PowerSmoothingV2BranchTest, PowerProfileV2_ResetParam_InvalidPowerLimit)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    EXPECT_TRUE(intf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
}

TEST_F(PowerSmoothingV2BranchTest, PowerProfileV2_ResetParam_NormalValue)
{
    auto& bus = utils::DBusHandler::getBus();
    auto intf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath, 0,
                                                        gpu);

    EXPECT_FALSE(intf->resetParam(42.0));
}
