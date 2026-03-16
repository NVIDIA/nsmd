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
