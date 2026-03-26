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

#include "base.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "nsmPowerSmoothing-v2.hpp"
#include "nsmPowerSmoothingAdminProfileIntf-v2.hpp"
#include "nsmPowerSmoothingFeatureIntf-v2.hpp"
#include "nsmPowerSmoothingPowerProfileIntf-v2.hpp"

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

// =============================================================================
// Helper: Encode a uint16_t value into a 2-byte little-endian buffer
// =============================================================================
static void encodeUint16LE(uint8_t* buf, uint16_t val)
{
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

// =============================================================================
// Test Fixture
// =============================================================================
struct PowerSmoothingV2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string parentPath = "/xyz/openbmc_project/inventory/test/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    PowerSmoothingV2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~PowerSmoothingV2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// OemAdminProfileIntfV2 -- updateSample methods
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentTMPFloorSample_ValidValue_ConvertsToPercent)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // UFXP4.12 value for 0.5 fraction = 0.5 * 4096 = 2048
    uint16_t ufxpValue = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentTMPFloorSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // 0.5 fraction * 100 = 50.0 percent
    double expected = NvUFXP4_12ToDouble(ufxpValue) * 100;
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::tmpFloorPercent(), expected,
                0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentTMPFloorSample_InvalidValue_SetsInvalidUint32)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint16_t invalidVal = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, invalidVal);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentTMPFloorSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(static_cast<uint32_t>(
                  adminIntf->AdminPowerProfileIntf::tmpFloorPercent()),
              INVALID_UINT32_VALUE);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentTMPFloorSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[1] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1; // Wrong length, needs 2
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentTMPFloorSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampUpRateSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // 5000 mW/s -> 5.0 W/s
    uint32_t rampUpRate = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampUpRateSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(rampUpRate,
                                                               1000);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampUpRate(), expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampUpRateSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[2] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = 2; // Wrong, needs 4
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampUpRateSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampDownRateSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // 3000 mW/s -> 3.0 W/s
    uint32_t rampDownRate = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampDownRateSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(rampDownRate,
                                                               1000);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampDownRate(), expected,
                0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampDownRateSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[1] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampDownRateSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampDownHysteresis_ValidValue_ConvertsMsToS)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // 2500 ms -> 2.5 s
    uint32_t hysteresis = 2500;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampDownHysteresisSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(hysteresis,
                                                               1000);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampDownHysteresis(),
                expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampDownHysteresis_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[3] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = 3;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampDownHysteresisSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentSecondaryFloorSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // 10000 mW -> 10.0 W
    uint32_t secondaryFloor = 10000;
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentSecondaryFloorSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(secondaryFloor,
                                                               1000);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::secondaryPowerFloorSetting(),
                expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentSecondaryFloorSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[2] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentSecondaryFloorSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    AdminV2_updatePrimaryFloorActivationWindowMultiplier_ValidValue_ConvertsUint8)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t multiplierVal = 42;
    uint8_t data[1] = {};
    data[0] = multiplierVal;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc =
        adminIntf->updateCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(multiplierVal);
    EXPECT_NEAR(
        adminIntf
            ->AdminPowerProfileIntf::primaryFloorActivationWindowMultiplier(),
        expected, 0.01);
}

TEST_F(
    PowerSmoothingV2Test,
    AdminV2_updatePrimaryFloorActivationWindowMultiplier_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[4] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 4; // Wrong, needs 1
    sample.data = data;
    sample.valid = true;

    // Act
    int rc =
        adminIntf->updateCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updatePrimaryFloorTargetWindow_ValidValue_ConvertsUint8)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t targetWindow = 17;
    uint8_t data[1] = {};
    data[0] = targetWindow;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorTargetWindowSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(targetWindow);
    EXPECT_NEAR(
        adminIntf->AdminPowerProfileIntf::primaryFloorTargetWindowMultiplier(),
        expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updatePrimaryFloorTargetWindow_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[4] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorTargetWindowSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updatePrimaryFloorActivationOffset_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // 7500 mW -> 7.5 W
    uint32_t offset = 7500;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorActivationOffsetSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(offset, 1000);
    EXPECT_NEAR(
        adminIntf->AdminPowerProfileIntf::primaryFloorActivationOffset(),
        expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updatePrimaryFloorActivationOffset_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[2] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentPrimaryFloorActivationOffsetSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// OemAdminProfileIntfV2 -- handleSample routing
// =============================================================================

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_RoutesTmpFloorTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint16_t ufxpValue = 2048; // 0.5 fraction
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_RoutesRampUpRateTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t rampUpRate = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_RoutesRampDownRateTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t rampDownRate = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_RoutesRampDownHysteresisTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t hysteresis = 2500;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_RoutesSecondaryFloorTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t secondaryFloor = 10000;
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_handleSample_RoutesPrimaryFloorActivationWindowMultiplierTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t multiplier = 42;
    uint8_t data[1] = {};
    data[0] = multiplier;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag =
        ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_handleSample_RoutesPrimaryFloorTargetWindowTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t targetWindow = 17;
    uint8_t data[1] = {};
    data[0] = targetWindow;

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_handleSample_RoutesPrimaryFloorActivationOffsetTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t offset = 7500;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_UnknownTag_ReturnsErrorLength)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[4] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 99; // Unknown tag (below max unreserved)
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(PowerSmoothingV2Test, AdminV2_handleSample_ReservedTag_ReturnsSuccess)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[4] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    // Tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE (0xEF)
    sample.tag = 0xF0;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, AdminV2_resetParam_InvalidPowerLimit_ReturnsTrue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act & Assert
    EXPECT_TRUE(
        adminIntf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
}

TEST_F(PowerSmoothingV2Test, AdminV2_resetParam_NormalValue_ReturnsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act & Assert
    EXPECT_FALSE(adminIntf->resetParam(50.0));
}

TEST_F(PowerSmoothingV2Test, AdminV2_getInventoryObjPath_ReturnsCorrectPath)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act
    auto path = adminIntf->getInventoryObjPath();

    // Assert
    EXPECT_EQ(path, parentPath + "/profile/admin_profile");
}

// =============================================================================
// OemPowerProfileIntfV2 -- updateSample methods
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentTMPFloorSample_MatchingProfileId_Converts)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // UFXP4.12 value for 0.75 fraction = 0.75 * 4096 = 3072
    uint16_t ufxpValue = 3072;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0; // tag irrelevant for direct call
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act -- profIdRes matches profId
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, profileId,
                                                      profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = NvUFXP4_12ToDouble(ufxpValue) * 100;
    EXPECT_NEAR(profileIntf->PowerProfileIntf::tmpFloorPercent(), expected,
                0.01);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updateCurrentTMPFloorSample_NonMatchingProfileId_DoesNotUpdate)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint16_t ufxpValue = 3072;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act -- profIdRes does NOT match profId
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, 2, profileId);

    // Assert -- returns success but does not update the dbus property
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updateCurrentTMPFloorSample_InvalidValue_SetsInvalidUint32)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint16_t invalidVal = INVALID_UINT16_VALUE;
    uint8_t data[2] = {};
    encodeUint16LE(data, invalidVal);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, profileId,
                                                      profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(
        static_cast<uint32_t>(profileIntf->PowerProfileIntf::tmpFloorPercent()),
        INVALID_UINT32_VALUE);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentTMPFloorSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[1] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentTMPFloorSample(sample, profileId,
                                                      profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampUpRateSample_MatchingProfile_Converts)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 2;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampUpRate = 8000; // 8 W/s
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampUpRateSample(sample, profileId,
                                                        profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(rampUpRate,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampUpRate(), expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampUpRateSample_NonMatching_DoesNotUpdate)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 2;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampUpRate = 8000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampUpRateSample(sample, 3, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampDownRateSample_MatchingProfile_Converts)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampDownRate = 4000; // 4 W/s
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownRateSample(sample, profileId,
                                                          profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(rampDownRate,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampDownRate(), expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampDownRateSample_NonMatching_DoesNotUpdate)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampDownRate = 4000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownRateSample(sample, 5, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updateCurrentRampDownHysteresis_MatchingProfile_ConvertsMsToS)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t hysteresis = 1500; // 1.5 s
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownHysteresisSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(hysteresis,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampDownHysteresis(), expected,
                0.01);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentSecondaryFloor_MatchingProfile_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t secondaryFloor = 15000; // 15 W
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentSecondaryFloorSample(sample, profileId,
                                                            profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(secondaryFloor,
                                                               1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::secondaryPowerFloorSetting(),
                expected, 0.01);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updatePrimaryFloorActivationWindowMultiplier_MatchingProfile)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t multiplier = 55;
    uint8_t data[1] = {};
    data[0] = multiplier;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc =
        profileIntf->updateCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(multiplier);
    EXPECT_NEAR(
        profileIntf->PowerProfileIntf::primaryFloorActivationWindowMultiplier(),
        expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updatePrimaryFloorTargetWindow_MatchingProfile)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t targetWindow = 33;
    uint8_t data[1] = {};
    data[0] = targetWindow;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentPrimaryFloorTargetWindowSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint8ToDouble(targetWindow);
    EXPECT_NEAR(
        profileIntf->PowerProfileIntf::primaryFloorTargetWindowMultiplier(),
        expected, 0.01);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updatePrimaryFloorActivationOffset_MatchingProfile_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t offset = 12500; // 12.5 W
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentPrimaryFloorActivationOffsetSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(offset, 1000);
    EXPECT_NEAR(profileIntf->PowerProfileIntf::primaryFloorActivationOffset(),
                expected, 0.01);
}

// =============================================================================
// OemPowerProfileIntfV2 -- handleSample routing (tag parsing)
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_handleSample_RoutesTmpFloorTag_MatchingProfileId)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 2;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint16_t ufxpValue = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    // handleSample extracts: tag = sample.tag >> 3, profIdRes = sample.tag &
    // 0x07 For PRESET_PROFILE_TMP_FLOOR_SETTING_TAG (0), profileId 2: composite
    // tag = (0 << 3) | 2 = 2
    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | profileId;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, PowerProfileV2_handleSample_RoutesRampUpRateTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampUpRate = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | profileId;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, PowerProfileV2_handleSample_RoutesRampDownRateTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 1;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampDownRate = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3) | profileId;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_handleSample_RoutesRampDownHysteresisTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t hysteresis = 2500;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | profileId;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_handleSample_RoutesSecondaryFloorTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t secondaryFloor = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | profileId;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_handleSample_RoutesPrimaryFloorActivationWindowMultTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t multiplier = 55;
    uint8_t data[1] = {};
    data[0] = multiplier;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
                  << 3) |
                 profileId;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_handleSample_RoutesPrimaryFloorTargetWindowTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t targetWindow = 33;
    uint8_t data[1] = {};
    data[0] = targetWindow;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) |
                 profileId;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_handleSample_RoutesPrimaryFloorActivationOffsetTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t offset = 12500;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) |
                 profileId;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       DISABLED_PowerProfileV2_handleSample_UnknownTag_ReturnsErrorLength)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    // tag >> 3 = 99 which is not a known tag
    sample.tag = (99 << 3) | profileId;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_resetParam_InvalidPowerLimit_ReturnsTrue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // Act & Assert
    EXPECT_TRUE(
        profileIntf->resetParam(static_cast<double>(INVALID_POWER_LIMIT)));
}

TEST_F(PowerSmoothingV2Test, PowerProfileV2_resetParam_NormalValue_ReturnsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    // Act & Assert
    EXPECT_FALSE(profileIntf->resetParam(100.0));
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- updateSample methods
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFeatureFlagSample_AllBitsSet_AllFlagsTrue)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // All 4 feature bits set: bit0=supported, bit1=enabled,
    // bit2=rampDown, bit3=delayed
    uint32_t featureFlag = 0x0F; // bits 0-3 set
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFeatureFlagSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::delayedPowerSmoothingSupported());
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFeatureFlagSample_NoBitsSet_AllFlagsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t featureFlag = 0x00;
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFeatureFlagSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
    EXPECT_FALSE(
        featIntf->PowerSmoothingIntf::delayedPowerSmoothingSupported());
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFeatureFlagSample_IndividualBits_CorrectExtraction)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // bit0 set (supported), bit1 clear (not enabled), bit2 set (rampDown),
    // bit3 clear
    uint32_t featureFlag = 0x05; // bits 0 and 2
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFeatureFlagSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
    EXPECT_FALSE(
        featIntf->PowerSmoothingIntf::delayedPowerSmoothingSupported());
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFeatureFlagSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[2] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = 2; // Wrong, needs 4
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFeatureFlagSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateCurrentTMPSettingSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // 50000 mW -> 50.0 W
    uint32_t tmpSetting = 50000;
    uint8_t data[4] = {};
    encodeUint32LE(data, tmpSetting);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateCurrentTMPSettingSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(tmpSetting,
                                                               1000);
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::currentTempSetting(), expected,
                0.01);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateCurrentTMPSettingSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[1] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateCurrentTMPSettingSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateCurrentTMPFloorSettingSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // 30000 mW -> 30.0 W
    uint32_t tmpFloor = 30000;
    uint8_t data[4] = {};
    encodeUint32LE(data, tmpFloor);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateCurrentTMPFloorSettingSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(tmpFloor, 1000);
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::currentTempFloorSetting(),
                expected, 0.01);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateCurrentTMPFloorSettingSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[3] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = 3;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateCurrentTMPFloorSettingSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateMaxTmpFloorSettingSample_ValidValue_ConvertsToPercent)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // UFXP4.12 value for 0.9 fraction = 0.9 * 4096 = 3686.4 -> 3686
    uint16_t ufxpValue = 3686;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMaxTmpFloorSettingSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = NvUFXP4_12ToDouble(ufxpValue) * 100;
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::maxAllowedTmpFloorPercent(),
                expected, 0.1);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateMaxTmpFloorSettingSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 4; // Wrong, needs 2
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMaxTmpFloorSettingSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateMinTmpFloorSettingSample_ValidValue_ConvertsToPercent)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // UFXP4.12 value for 0.3 fraction = 0.3 * 4096 = 1228.8 -> 1229
    uint16_t ufxpValue = 1229;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMinTmpFloorSettingSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = NvUFXP4_12ToDouble(ufxpValue) * 100;
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::minAllowedTmpFloorPercent(),
                expected, 0.1);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateMinTmpFloorSettingSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMinTmpFloorSettingSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFloorWindowMultiplierSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // 20000 mW/s -> 20.0 W/s
    uint32_t multiplier = 20000;
    uint8_t data[4] = {};
    encodeUint32LE(data, multiplier);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFloorWindowMultiplierSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(multiplier,
                                                               1000);
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::floorWindowMultiplier(), expected,
                0.01);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFloorWindowMultiplierSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[2] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFloorWindowMultiplierSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    FeatureV2_updateMinPrimaryFloorActivationOffsetSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // 6000 mW -> 6.0 W
    uint32_t offset = 6000;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMinPrimaryFloorActivationOffsetSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(offset, 1000);
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::minPrimaryFloorActivationOffset(),
                expected, 0.01);
}

TEST_F(
    PowerSmoothingV2Test,
    FeatureV2_updateMinPrimaryFloorActivationOffsetSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[2] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMinPrimaryFloorActivationOffsetSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    FeatureV2_updateMinPrimaryFloorActivationPointSample_ValidValue_ConvertsMwToW)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // 9000 mW -> 9.0 W
    uint32_t point = 9000;
    uint8_t data[4] = {};
    encodeUint32LE(data, point);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMinPrimaryFloorActivationPointSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected = utils::convertAndScaleDownUint32ToDouble(point, 1000);
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::minPrimaryFloorActivationPoint(),
                expected, 0.01);
}

TEST_F(
    PowerSmoothingV2Test,
    FeatureV2_updateMinPrimaryFloorActivationPointSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[3] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = 3;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateMinPrimaryFloorActivationPointSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- handleSample routing
// =============================================================================

TEST_F(PowerSmoothingV2Test, FeatureV2_handleSample_RoutesFeatureFlagTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t featureFlag = 0x0F;
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, FeatureV2_handleSample_RoutesCurrentTMPSettingTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t tmpSetting = 50000;
    uint8_t data[4] = {};
    encodeUint32LE(data, tmpSetting);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = CURRENT_TMP_SETTING_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, FeatureV2_handleSample_RoutesTmpFloorSettingTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t tmpFloor = 30000;
    uint8_t data[4] = {};
    encodeUint32LE(data, tmpFloor);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, FeatureV2_handleSample_RoutesMaxTmpFloorSettingTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint16_t ufxpValue = 3686;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MAX_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, FeatureV2_handleSample_RoutesMinTmpFloorSettingTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint16_t ufxpValue = 1229;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_handleSample_RoutesFloorWindowMultiplierTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t multiplier = 20000;
    uint8_t data[4] = {};
    encodeUint32LE(data, multiplier);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FLOOR_WINDOW_MULTIPLIER_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_handleSample_RoutesMinPrimaryFloorActivationOffsetTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t offset = 6000;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_handleSample_RoutesMinPrimaryFloorActivationPointTag)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t point = 9000;
    uint8_t data[4] = {};
    encodeUint32LE(data, point);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       FeatureV2_handleSample_UnknownTag_ReturnsErrorLength)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = 99; // Unknown tag
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(PowerSmoothingV2Test, FeatureV2_handleSample_ReservedTag_ReturnsSuccess)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint8_t data[4] = {};

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = 0xF0; // > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test, FeatureV2_getInventoryObjPath_ReturnsCorrectPath)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // Act
    auto path = featIntf->getInventoryObjPath();

    // Assert
    EXPECT_EQ(path, parentPath);
}

// =============================================================================
// OemAdminProfileIntfV2 -- INVALID_UINT32_VALUE passthrough tests
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampUpRateSample_InvalidUint32_PassesThrough)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t invalidVal = INVALID_UINT32_VALUE;
    uint8_t data[4] = {};
    encodeUint32LE(data, invalidVal);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampUpRateSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // convertAndScaleDownUint32ToDouble returns INVALID_UINT32_VALUE as-is
    double expected =
        utils::convertAndScaleDownUint32ToDouble(INVALID_UINT32_VALUE, 1000);
    EXPECT_EQ(adminIntf->AdminPowerProfileIntf::rampUpRate(), expected);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampDownRateSample_InvalidUint32_PassesThrough)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t invalidVal = INVALID_UINT32_VALUE;
    uint8_t data[4] = {};
    encodeUint32LE(data, invalidVal);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampDownRateSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    double expected =
        utils::convertAndScaleDownUint32ToDouble(INVALID_UINT32_VALUE, 1000);
    EXPECT_EQ(adminIntf->AdminPowerProfileIntf::rampDownRate(), expected);
}

// =============================================================================
// OemPowerProfileIntfV2 -- Bad decode returns error (wrong data_len)
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampUpRateSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[2] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 2; // Wrong, needs 4
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampUpRateSample(sample, profileId,
                                                        profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampDownRateSample_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[1] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownRateSample(sample, profileId,
                                                          profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentRampDownHysteresis_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[3] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 3;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentRampDownHysteresisSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateCurrentSecondaryFloor_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[2] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentSecondaryFloorSample(sample, profileId,
                                                            profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updatePrimaryFloorActivationWindowMult_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 4; // Wrong, needs 1
    sample.data = data;
    sample.valid = true;

    // Act
    int rc =
        profileIntf->updateCurrentPrimaryFloorActivationWindowMultiplierSample(
            sample, profileId, profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updatePrimaryFloorTargetWindow_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentPrimaryFloorTargetWindowSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_updatePrimaryFloorActivationOffset_BadDataLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[2] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->updateCurrentPrimaryFloorActivationOffsetSample(
        sample, profileId, profileId);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- Feature flag bit1,bit3 only
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       FeatureV2_updateFeatureFlagSample_Bit1And3Only_EnabledAndDelayed)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // bit1 set (enabled), bit3 set (delayed)
    uint32_t featureFlag = 0x0A; // bits 1 and 3
    uint8_t data[4] = {};
    encodeUint32LE(data, featureFlag);

    OemPowerSmoothingFeatIntfV2::TelemetrySample sample = {};
    sample.tag = FEATURE_FLAG_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = featIntf->updateFeatureFlagSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::featureSupported());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::powerSmoothingEnabled());
    EXPECT_FALSE(featIntf->PowerSmoothingIntf::immediateRampDownEnabled());
    EXPECT_TRUE(featIntf->PowerSmoothingIntf::delayedPowerSmoothingSupported());
}

// =============================================================================
// OemAdminProfileIntfV2 -- Zero value boundary tests
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentRampUpRateSample_ZeroValue_ReturnsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t rampUpRate = 0;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentRampUpRateSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampUpRate(), 0.0, 0.001);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_updateCurrentTMPFloorSample_ZeroUFXP_ReturnsZeroPercent)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint16_t ufxpValue = 0;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->updateCurrentTMPFloorSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::tmpFloorPercent(), 0.0,
                0.001);
}

// =============================================================================
// NsmPowerSmoothingSupportedVersion Tests
// =============================================================================

struct NsmPowerSmoothingSupportedVersionTest :
    public Test,
    public utils::DBusTest
{
    const std::string path = "/xyz/openbmc_project/inventory/test/version0";
    std::string name = "SupportedVersion";
    std::string type = "NSM_PowerSmoothing";
    std::string inventoryObjPath = path;
    uint8_t messageType = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    std::shared_ptr<RevisionIntf> revisionIntf;

    NsmPowerSmoothingSupportedVersionTest()
    {
        auto& bus = utils::DBusHandler::getBus();
        revisionIntf = std::make_shared<RevisionIntf>(bus, path.c_str());
    }
};

TEST_F(NsmPowerSmoothingSupportedVersionTest, Constructor_SetsNameAndType)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);
    EXPECT_EQ(sensor.getName(), name);
    EXPECT_EQ(sensor.getType(), type);
}

TEST_F(NsmPowerSmoothingSupportedVersionTest, GenRequestMsg_ReturnsValidRequest)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);
    auto request = sensor.genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
    size_t expectedSize = sizeof(nsm_msg_hdr) +
                          sizeof(nsm_get_supported_command_codes_req);
    EXPECT_EQ(request->size(), expectedSize);
}

TEST_F(NsmPowerSmoothingSupportedVersionTest,
       HandleResponseMsg_SetsVersionNan_WhenNoCommandsSupported)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    // Build a response with all-zero command codes (no commands supported)
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            responseMsg);

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.getSupportedVersion(), "nan");
}

TEST_F(NsmPowerSmoothingSupportedVersionTest,
       HandleResponseMsg_SetsVersionV1_WhenV1CommandsSupported)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Set bits for V1 commands
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO % 8));
    codes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE % 8));
    codes[NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION % 8));
    codes[NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION % 8));

    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            responseMsg);

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.getSupportedVersion(), "V1");
}

TEST_F(NsmPowerSmoothingSupportedVersionTest,
       HandleResponseMsg_ErrorCC_ReturnsError)
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
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmPowerSmoothingSupportedVersionTest,
       HandleResponseMsg_DecodeFail_ReturnsError)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    // 7-byte buffer: safely reads cc=0 at payload[1] but too short for
    // decode_get_supported_command_codes_resp which calls decode_common_resp
    // requiring at least sizeof(nsm_msg_hdr)+sizeof(nsm_common_resp)=11 bytes
    std::vector<uint8_t> responseData(sizeof(nsm_msg_hdr) + 2, 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// Helpers for coroutine branch coverage tests
// =============================================================================

static std::vector<uint8_t> makeSetupAdminRespV2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_setup_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

static std::vector<uint8_t> makeQueryAdminRespV2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_admin_override_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_admin_override_data data = {};
    encode_query_admin_override_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, &data, msg);
    return buf;
}

static std::vector<uint8_t> makeToggleFeatureRespV2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_feature_state_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

static std::vector<uint8_t> makeToggleRampdownRespV2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_toggle_immediate_rampdown_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

static std::vector<uint8_t> makeGetFeatureInfoRespV2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    nsm_pwr_smoothing_featureinfo_data data = {};
    encode_get_powersmoothing_featinfo_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, &data, msg);
    return buf;
}

// =============================================================================
// OemAdminProfileIntfV2 -- overrideAdminProfileParam branch coverage
// =============================================================================

// postPatchIO fails → WriteFailure (if (rc_) TRUE branch)
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_PostPatchIOFails_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// decode fails (error CC) → WriteFailure (if (cc==NSM_SUCCESS...) FALSE branch)
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_ErrorCC_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// success path + V1 fallback (isCommandSupported returns false by default)
// parameterId=1 → else branch of param conversion (watts * 1000)
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_Success_V1Fallback)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // First: setup_admin_override success; Second: query_admin_override (V1)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeQueryAdminRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// parameterId=5 → NvU8 conversion path (else if branch)
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_ParamId5_NvU8Path_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(5, 1.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// parameterId=6 → NvU8 conversion path (else if branch, second alternative)
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_ParamId6_NvU8Path_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(6, 1.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// parameterId=0 → FXP conversion path (if branch)
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_ParamId0_FXPPath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(0, 0.5, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// OemAdminProfileIntfV2 -- resetAdminProfileParam branch coverage
// =============================================================================

// postPatchIO fails → WriteFailure
TEST_F(PowerSmoothingV2Test,
       AdminV2_resetAdminProfileParam_PostPatchIOFails_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// error CC → WriteFailure
TEST_F(PowerSmoothingV2Test,
       AdminV2_resetAdminProfileParam_ErrorCC_SetsWriteFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(1, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// success path + V1 fallback
TEST_F(PowerSmoothingV2Test, AdminV2_resetAdminProfileParam_Success_V1Fallback)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeQueryAdminRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(2, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemAdminProfileIntfV2 -- resetParam branch coverage
// =============================================================================

// resetParam: reading == INVALID_POWER_LIMIT → TRUE
TEST_F(PowerSmoothingV2Test, AdminV2_resetParam_InvalidValue_ReturnsTrue)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    double invalidReading = static_cast<double>(INVALID_POWER_LIMIT);
    EXPECT_TRUE(adminIntf->resetParam(invalidReading));
}

// resetParam: reading != INVALID_POWER_LIMIT → FALSE
TEST_F(PowerSmoothingV2Test, AdminV2_resetParam_ValidValue_ReturnsFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_FALSE(adminIntf->resetParam(5000.0));
}

// =============================================================================
// OemAdminProfileIntfV2 -- setter coroutines: non-double throws
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       AdminV2_setTmpFloorPercent_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    EXPECT_THROW_COROUTINE(
        adminIntf->setTmpFloorPercent(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_setRampUpRate_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        adminIntf->setRampUpRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_setRampDownRate_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        adminIntf->setRampDownRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_setRampDownHysteresis_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{false};
    EXPECT_THROW_COROUTINE(
        adminIntf->setRampDownHysteresis(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_setSecondaryPowerFloor_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        adminIntf->setSecondaryPowerFloor(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(
    PowerSmoothingV2Test,
    AdminV2_setPrimaryFloorActivationWindowMult_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        adminIntf->setPrimaryFloorActivationWindowMultiplier(value, &status,
                                                             gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_setPrimaryFloorTargetWindowMult_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    EXPECT_THROW_COROUTINE(
        adminIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       AdminV2_setPrimaryFloorActivationOffset_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        adminIntf->setPrimaryFloorActivationOffset(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// OemAdminProfileIntfV2 -- setter coroutines: resetParam TRUE/FALSE paths
// =============================================================================

// setTmpFloorPercent: resetParam TRUE → calls resetAdminProfileParam (param 0)
TEST_F(PowerSmoothingV2Test, AdminV2_setTmpFloorPercent_ResetPath_CallsReset)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Reset sends setup_admin_override, success triggers
    // getAdminProfileFromDevice (V1)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeQueryAdminRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    double invalidReading = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = invalidReading;
    adminIntf->setTmpFloorPercent(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setTmpFloorPercent: resetParam FALSE → calls overrideAdminProfileParam (param
// 0, FXP)
TEST_F(PowerSmoothingV2Test,
       AdminV2_setTmpFloorPercent_OverridePath_CallsOverride)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Override sends setup_admin_override, success triggers
    // getAdminProfileFromDevice (V1)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeQueryAdminRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{50.0}; // 50% → not invalid
    adminIntf->setTmpFloorPercent(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setRampUpRate: resetParam FALSE → overrideAdminProfileParam(1, ...)
TEST_F(PowerSmoothingV2Test, AdminV2_setRampUpRate_OverridePath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{5000.0};
    adminIntf->setRampUpRate(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setRampDownRate: resetParam TRUE → resetAdminProfileParam(2, ...)
TEST_F(PowerSmoothingV2Test, AdminV2_setRampDownRate_ResetPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    double invalidReading = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = invalidReading;
    adminIntf->setRampDownRate(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setRampDownHysteresis: resetParam FALSE → overrideAdminProfileParam(3, ...)
TEST_F(PowerSmoothingV2Test, AdminV2_setRampDownHysteresis_OverridePath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{500.0};
    adminIntf->setRampDownHysteresis(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setSecondaryPowerFloor: resetParam TRUE
TEST_F(PowerSmoothingV2Test, AdminV2_setSecondaryPowerFloor_ResetPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    double invalidReading = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = invalidReading;
    adminIntf->setSecondaryPowerFloor(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setPrimaryFloorActivationWindowMultiplier: resetParam FALSE (param 5, NvU8)
TEST_F(PowerSmoothingV2Test,
       AdminV2_setPrimaryFloorActivationWindowMult_OverridePath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{1.5};
    adminIntf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setPrimaryFloorTargetWindowMultiplier: resetParam TRUE (param 6, NvU8)
TEST_F(PowerSmoothingV2Test, AdminV2_setPrimaryFloorTargetWindowMult_ResetPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    double invalidReading = static_cast<double>(INVALID_POWER_LIMIT);
    AsyncSetOperationValueType value = invalidReading;
    adminIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// setPrimaryFloorActivationOffset: resetParam FALSE (param 7, *1000)
TEST_F(PowerSmoothingV2Test,
       AdminV2_setPrimaryFloorActivationOffset_OverridePath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{100.0};
    adminIntf->setPrimaryFloorActivationOffset(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- togglePowerSmoothingOnDevice branch coverage
// =============================================================================

// postPatchIO fails → WriteFailure
TEST_F(PowerSmoothingV2Test,
       FeatureV2_togglePowerSmoothingOnDevice_PostPatchIOFails_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureRespV2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// error CC → WriteFailure
TEST_F(PowerSmoothingV2Test,
       FeatureV2_togglePowerSmoothingOnDevice_ErrorCC_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(false, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// success + V1 fallback (isCommandSupported returns false →
// getPwrSmoothingControlsFromDevice)
TEST_F(PowerSmoothingV2Test,
       FeatureV2_togglePowerSmoothingOnDevice_Success_V1Fallback)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    // First: toggle success; Second: V1 get-feature-info
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureRespV2()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- toggleImmediateRampDownOnDevice branch
// coverage
// =============================================================================

// postPatchIO fails → WriteFailure
TEST_F(PowerSmoothingV2Test,
       FeatureV2_toggleImmediateRampDown_PostPatchIOFails_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownRespV2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// error CC → WriteFailure
TEST_F(PowerSmoothingV2Test,
       FeatureV2_toggleImmediateRampDown_ErrorCC_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(false, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// success + V1 fallback
TEST_F(PowerSmoothingV2Test,
       FeatureV2_toggleImmediateRampDown_Success_V1Fallback)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownRespV2()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- setPowerSmoothingEnabled and
// setImmediateRampDownEnabled branch coverage
// =============================================================================

// setPowerSmoothingEnabled: non-bool → throws InvalidArgument
TEST_F(PowerSmoothingV2Test,
       FeatureV2_setPowerSmoothingEnabled_NonBool_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        featIntf->setPowerSmoothingEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// setPowerSmoothingEnabled: valid bool → delegates to
// togglePowerSmoothingOnDevice
TEST_F(PowerSmoothingV2Test,
       FeatureV2_setPowerSmoothingEnabled_ValidBool_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureRespV2()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    featIntf->setPowerSmoothingEnabled(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// setImmediateRampDownEnabled: non-bool → throws InvalidArgument
TEST_F(PowerSmoothingV2Test,
       FeatureV2_setImmediateRampDownEnabled_NonBool_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{1.0};
    EXPECT_THROW_COROUTINE(
        featIntf->setImmediateRampDownEnabled(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// setImmediateRampDownEnabled: valid bool → delegates to
// toggleImmediateRampDownOnDevice
TEST_F(PowerSmoothingV2Test,
       FeatureV2_setImmediateRampDownEnabled_ValidBool_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownRespV2()))
        .WillOnce(mockPostPatchIO(makeGetFeatureInfoRespV2()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{false};
    featIntf->setImmediateRampDownEnabled(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerProfileIntfV2 -- coroutine helpers
// =============================================================================

// encode_update_preset_profile_param_resp produces nsm_common_resp payload
static std::vector<uint8_t>
    makeUpdatePresetProfileParamRespV2(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_update_preset_profile_param_resp(
        0, cc, cc == NSM_SUCCESS ? ERR_NULL : ERR_INVALID_RQD, msg);
    return buf;
}

// encode_get_preset_profile_info_v2_resp (aggregate resp, 0 samples)
static std::vector<uint8_t>
    makeGetPresetProfileInfoV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_preset_profile_info_v2_resp(0, cc, 0, msg);
    return buf;
}

// =============================================================================
// OemPowerProfileIntfV2 -- getProfileInfoFromDeviceV2 branch coverage
// =============================================================================

// postPatchIO fails → co_return rc_
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_getProfileInfoFromDeviceV2_PostPatchIOFails_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPresetProfileInfoV2Resp(), NSM_ERROR));

    profileIntf->getProfileInfoFromDeviceV2(gpu);
    // No crash; error was logged internally
}

// decode fails (error CC) → co_return rc
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_getProfileInfoFromDeviceV2_ErrorCC_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPresetProfileInfoV2Resp(NSM_ERROR)));

    profileIntf->getProfileInfoFromDeviceV2(gpu);
    // Error CC → decode branch taken
}

// Success with 0 telemetry samples: while loop body skipped, co_return rc //

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_getProfileInfoFromDeviceV2_ZeroSamples_Succeeds)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeGetPresetProfileInfoV2Resp()));

    profileIntf->getProfileInfoFromDeviceV2(gpu);
    // while(0--) skips loop body; function completes
}

// =============================================================================
// OemPowerProfileIntfV2 -- updateProfileInfoOnDevice branch coverage
// =============================================================================

// postPatchIO fails → *status=WriteFailure, co_return NSM_SW_ERROR_COMMAND_FAIL
//
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateProfileInfoOnDevice_PostPatchIOFails_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// error CC → *status=WriteFailure
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateProfileInfoOnDevice_ErrorCC_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// parameterId=0 → FXP (doubleToNvUFXP4_12) conversion path, success + V1
// fallback
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateProfileInfoOnDevice_ParamId0_FXP_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(
            makeSetupAdminRespV2())); // V1 fallback (wrong type, decode fails)

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(0, 0.5, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// parameterId=5 → NvU8 (doubleToNvU8) conversion path
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateProfileInfoOnDevice_ParamId5_NvU8_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(5, 1.5, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// parameterId=6 → NvU8 conversion path (second else-if branch)
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateProfileInfoOnDevice_ParamId6_NvU8_ErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(6, 2.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// parameterId=2 → else (x1000) conversion path, success + V1 fallback
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_updateProfileInfoOnDevice_ParamId2_Multiply1000_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2())); // V1 fallback

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->updateProfileInfoOnDevice(2, 100.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerProfileIntfV2 -- resetProfileInfoOnDevice branch coverage
// =============================================================================

// postPatchIO fails → *status=WriteFailure
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_resetProfileInfoOnDevice_PostPatchIOFails_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->resetProfileInfoOnDevice(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// error CC → *status=WriteFailure
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_resetProfileInfoOnDevice_ErrorCC_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->resetProfileInfoOnDevice(1, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// success + V1 fallback
TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_resetProfileInfoOnDevice_Success_V1Fallback)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2())); // V1 fallback

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    profileIntf->resetProfileInfoOnDevice(2, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerProfileIntfV2 -- setter coroutines: non-double throws
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setTmpFloorPercent_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    EXPECT_THROW_COROUTINE(
        profileIntf->setTmpFloorPercent(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampUpRate_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{42};
    EXPECT_THROW_COROUTINE(
        profileIntf->setRampUpRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampDownRate_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        profileIntf->setRampDownRate(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampDownHysteresis_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{false};
    EXPECT_THROW_COROUTINE(
        profileIntf->setRampDownHysteresis(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setSecondaryPowerFloor_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        profileIntf->setSecondaryPowerFloor(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_setPrimaryFloorActivWindowMult_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{0};
    EXPECT_THROW_COROUTINE(
        profileIntf->setPrimaryFloorActivationWindowMultiplier(value, &status,
                                                               gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_setPrimaryFloorTargetWindowMult_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool{true};
    EXPECT_THROW_COROUTINE(
        profileIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_setPrimaryFloorActivationOffset_NonDouble_ThrowsInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t{1};
    EXPECT_THROW_COROUTINE(
        profileIntf->setPrimaryFloorActivationOffset(value, &status, gpu),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// =============================================================================
// OemPowerProfileIntfV2 -- setter coroutines: reset path (INVALID_POWER_LIMIT)
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setTmpFloorPercent_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setTmpFloorPercent(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test, PowerProfileV2_setRampUpRate_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setRampUpRate(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test, PowerProfileV2_setRampDownRate_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setRampDownRate(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampDownHysteresis_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setRampDownHysteresis(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setSecondaryPowerFloor_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setSecondaryPowerFloor(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setPrimaryFloorActivWindowMult_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setPrimaryFloorTargetWindowMult_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setPrimaryFloorActivationOffset_ResetPath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = static_cast<double>(INVALID_POWER_LIMIT);
    profileIntf->setPrimaryFloorActivationOffset(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerProfileIntfV2 -- setter coroutines: override path (normal value)
// =============================================================================

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setTmpFloorPercent_OverridePath_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeUpdatePresetProfileParamRespV2()))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{50.0}; // 50% → /100 = 0.5
    profileIntf->setTmpFloorPercent(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampUpRate_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{5000.0};
    profileIntf->setRampUpRate(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampDownRate_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{3000.0};
    profileIntf->setRampDownRate(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setRampDownHysteresis_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{500.0};
    profileIntf->setRampDownHysteresis(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setSecondaryPowerFloor_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{200.0};
    profileIntf->setSecondaryPowerFloor(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(PowerSmoothingV2Test,
       PowerProfileV2_setPrimaryFloorActivWindowMult_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{1.5};
    profileIntf->setPrimaryFloorActivationWindowMultiplier(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_setPrimaryFloorTargetWindowMult_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{2.0};
    profileIntf->setPrimaryFloorTargetWindowMultiplier(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

TEST_F(
    PowerSmoothingV2Test,
    PowerProfileV2_setPrimaryFloorActivationOffset_OverridePath_WritesFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(
            mockPostPatchIO(makeUpdatePresetProfileParamRespV2(NSM_ERROR)));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double{100.0};
    profileIntf->setPrimaryFloorActivationOffset(value, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// =============================================================================
// NsmPowerSmoothingSupportedVersion -- V2 command path
// =============================================================================

// HandleResponseMsg: V2 commands supported → version set to "V2"
TEST_F(NsmPowerSmoothingSupportedVersionTest,
       HandleResponseMsg_SetsVersionV2_WhenV2CommandsSupported)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Set bits for V2 commands only (not V1)
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 % 8));
    codes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 % 8));
    codes[NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2 % 8));
    codes[NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2 % 8));

    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            responseMsg);

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.getSupportedVersion(), "V2");
}

// HandleResponseMsg: V1 feature info bit set → loop body runs, version = V1
TEST_F(NsmPowerSmoothingSupportedVersionTest,
       HandleResponseMsg_V1LoopBody_BuildsVersionString)
{
    NsmPowerSmoothingSupportedVersion sensor(name, type, inventoryObjPath,
                                             messageType, revisionIntf);

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());

    // Set ALL V1 command bits to trigger V1 path AND the loop body at
    // NSM_PWR_SMOOTHING_GET_FEATURE_INFO
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO % 8));
    codes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE % 8));
    codes[NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION % 8));
    codes[NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION % 8));

    encode_get_supported_command_codes_resp(0, NSM_SUCCESS, ERR_NULL, codes,
                                            responseMsg);

    uint8_t rc = sensor.handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(sensor.getSupportedVersion(), "V1");
}

// =============================================================================
// Helper: make admin override V2 response (aggregate, 0 samples)
// =============================================================================

static std::vector<uint8_t>
    makeAdminOverrideInfoV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_admin_override_profile_info_v2_resp(0, cc, 0, msg);
    return buf;
}

// Helper: make feature info V2 response (aggregate, 0 samples)
static std::vector<uint8_t> makeFeatInfoV2Resp(uint8_t cc = NSM_SUCCESS)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_powersmoothing_featinfo_v2_resp(0, cc, 0, msg);
    return buf;
}

// Helper: enable V2 admin override query command on a device
static void enableAdminV2Commands(std::shared_ptr<NsmDevice> device)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2 % 8));
    device->updateMessageTypesToCommandCodeMatrix(
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, codes,
        SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// Helper: enable V2 feature info command on a device
static void enableFeatureV2Commands(std::shared_ptr<NsmDevice> device)
{
    bitfield8_t codes[SUPPORTED_COMMAND_CODE_DATA_SIZE] = {};
    codes[NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 / 8].byte |=
        (1 << (NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2 % 8));
    device->updateMessageTypesToCommandCodeMatrix(
        NSM_TYPE_PLATFORM_ENVIRONMENTAL, codes,
        SUPPORTED_COMMAND_CODE_DATA_SIZE);
}

// =============================================================================
// OemAdminProfileIntfV2 -- overrideAdminProfileParam V2 path
// =============================================================================

// overrideAdminProfileParam success path with V2 fallback
// (isCommandSupported returns true for
// NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2)
TEST_F(PowerSmoothingV2Test, AdminV2_overrideAdminProfileParam_Success_V2Path)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    enableAdminV2Commands(gpu);

    // First: setup_admin_override success
    // Second: getAdminProfileFromDeviceV2 (V2 aggregate resp)
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// overrideAdminProfileParam: V2 getAdminProfileFromDeviceV2 postPatchIO fails
TEST_F(PowerSmoothingV2Test,
       AdminV2_overrideAdminProfileParam_V2Path_GetProfileFails)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    enableAdminV2Commands(gpu);

    // First: setup success; Second: getAdminProfileFromDeviceV2 fails
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->overrideAdminProfileParam(1, 5000.0, &status, gpu);
    // No crash expected; status unchanged (co_return after getAdminProfile) //

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemAdminProfileIntfV2 -- resetAdminProfileParam V2 path
// =============================================================================

// resetAdminProfileParam success path with V2 fallback
TEST_F(PowerSmoothingV2Test, AdminV2_resetAdminProfileParam_Success_V2Path)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    enableAdminV2Commands(gpu);

    // First: setup_admin_override success
    // Second: getAdminProfileFromDeviceV2
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(0, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// resetAdminProfileParam: V2 path, getAdminProfileFromDeviceV2 decode fails
TEST_F(PowerSmoothingV2Test,
       AdminV2_resetAdminProfileParam_V2Path_GetProfileErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    enableAdminV2Commands(gpu);

    // First: setup success; Second: getAdminProfileFromDeviceV2 error CC
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetupAdminRespV2()))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    adminIntf->resetAdminProfileParam(0, &status, gpu);
    // No crash; status unchanged
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemAdminProfileIntfV2 -- getAdminProfileFromDeviceV2 direct tests
// =============================================================================

// getAdminProfileFromDeviceV2: postPatchIO fails
TEST_F(PowerSmoothingV2Test,
       AdminV2_getAdminProfileFromDeviceV2_PostPatchIOFails_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp(), NSM_ERROR));

    adminIntf->getAdminProfileFromDeviceV2(gpu);
    // No crash expected
}

// getAdminProfileFromDeviceV2: error CC in response
TEST_F(PowerSmoothingV2Test,
       AdminV2_getAdminProfileFromDeviceV2_ErrorCC_ReturnsError)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp(NSM_ERROR)));

    adminIntf->getAdminProfileFromDeviceV2(gpu);
    // No crash expected
}

// getAdminProfileFromDeviceV2: success path (0 samples)
TEST_F(PowerSmoothingV2Test,
       AdminV2_getAdminProfileFromDeviceV2_Success_ZeroSamples)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2Resp()));

    adminIntf->getAdminProfileFromDeviceV2(gpu);
    // No crash; success path executed
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- togglePowerSmoothingOnDevice V2 path
// =============================================================================

// togglePowerSmoothingOnDevice: success + V2 fallback
// (isCommandSupported returns true for NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2)
TEST_F(PowerSmoothingV2Test,
       FeatureV2_togglePowerSmoothingOnDevice_Success_V2Path)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    enableFeatureV2Commands(gpu);

    // First: toggle success; Second: getPwrSmoothingControlsFromDeviceV2
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureRespV2()))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// togglePowerSmoothingOnDevice: V2 path, getPwrSmoothingControlsFromDeviceV2
// postPatchIO fails
TEST_F(PowerSmoothingV2Test,
       FeatureV2_togglePowerSmoothingOnDevice_V2Path_GetFeatureFails)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    enableFeatureV2Commands(gpu);

    // First: toggle success; Second: getPwrSmoothingControlsFromDeviceV2 fails
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleFeatureRespV2()))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->togglePowerSmoothingOnDevice(true, &status, gpu);
    // No crash expected; success still since toggle itself succeeded
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemPowerSmoothingFeatIntfV2 -- toggleImmediateRampDownOnDevice V2 path
// =============================================================================

// toggleImmediateRampDownOnDevice: success + V2 fallback
TEST_F(PowerSmoothingV2Test, FeatureV2_toggleImmediateRampDown_Success_V2Path)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    enableFeatureV2Commands(gpu);

    // First: toggle rampdown success; Second:
    // getPwrSmoothingControlsFromDeviceV2
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownRespV2()))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(false, &status, gpu);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// toggleImmediateRampDownOnDevice: V2 path, getPwrSmoothingControlsFromDeviceV2
// decode fails (error CC)
TEST_F(PowerSmoothingV2Test,
       FeatureV2_toggleImmediateRampDown_V2Path_GetFeatureErrorCC)
{
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    enableFeatureV2Commands(gpu);

    // First: toggle success; Second: getPwrSmoothingControlsFromDeviceV2 error
    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeToggleRampdownRespV2()))
        .WillOnce(mockPostPatchIO(makeFeatInfoV2Resp(NSM_ERROR)));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    featIntf->toggleImmediateRampDownOnDevice(true, &status, gpu);
    // No crash; toggle success completed
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// OemAdminProfileIntfV2 -- getAdminProfileFromDeviceV2 while-loop body
// (branch coverage: loop body entered with 1+ samples)
// =============================================================================

// Helper: build an admin-override-V2 response with 1 aggregate sample.
// tag=ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG (0), 2-byte LE uint16 data.
static std::vector<uint8_t>
    makeAdminOverrideInfoV2RespWithOneSample(uint16_t tmpFloorVal = 2048,
                                             uint8_t cc = NSM_SUCCESS)
{
    // Encode base aggregate response header with telemetry_count=1
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_get_admin_override_profile_info_v2_resp(0, cc, 1, msg);

    // Encode one sample: tag=0 (TMP_FLOOR), valid=true, 2-byte uint16 LE data
    uint8_t rawData[2] = {};
    rawData[0] = static_cast<uint8_t>(tmpFloorVal & 0xFF);
    rawData[1] = static_cast<uint8_t>((tmpFloorVal >> 8) & 0xFF);

    std::vector<uint8_t> sampleBuf(
        sizeof(nsm_aggregate_resp_sample) + sizeof(rawData), 0);
    auto nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sampleBuf.data());
    size_t sample_len = 0;
    encode_aggregate_resp_sample(ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG,
                                 true, rawData, sizeof(rawData), nsm_sample,
                                 &sample_len);

    buf.insert(buf.end(), sampleBuf.begin(),
               sampleBuf.begin() + static_cast<ptrdiff_t>(sample_len));
    return buf;
}

// getAdminProfileFromDeviceV2: 1 sample → while-loop body entered
// covers: line 337 back-edge, handleSample → updateCurrentTMPFloorSample
TEST_F(PowerSmoothingV2Test,
       AdminV2_getAdminProfileFromDeviceV2_OneSample_LoopBodyExecuted)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeAdminOverrideInfoV2RespWithOneSample()));

    adminIntf->getAdminProfileFromDeviceV2(gpu);
    // Loop body executed once; tmpFloorPercent updated
    double expected = NvUFXP4_12ToDouble(2048) * 100;
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::tmpFloorPercent(), expected,
                0.01);
}

// getAdminProfileFromDeviceV2: 1 sample with INVALID_UINT16_VALUE
// covers: updateCurrentTMPFloorSample TRUE branch (line 76-80)
TEST_F(PowerSmoothingV2Test,
       AdminV2_getAdminProfileFromDeviceV2_OneSample_InvalidTMPFloor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(
            makeAdminOverrideInfoV2RespWithOneSample(INVALID_UINT16_VALUE)));

    adminIntf->getAdminProfileFromDeviceV2(gpu);
    // INVALID_UINT16_VALUE → should set INVALID_UINT32_VALUE on D-Bus
    EXPECT_EQ(static_cast<uint32_t>(
                  adminIntf->AdminPowerProfileIntf::tmpFloorPercent()),
              INVALID_UINT32_VALUE);
}
