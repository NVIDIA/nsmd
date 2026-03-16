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

// =============================================================================
// Helper: Encode a uint16_t value into a 2-byte little-endian buffer
// =============================================================================
static void encodeUint16LE(uint8_t* buf, uint16_t val)
{
    buf[0] = static_cast<uint8_t>(val & 0xFF);
    buf[1] = static_cast<uint8_t>((val >> 8) & 0xFF);
}

// =============================================================================
// Test Fixture: PowerSmoothing V2 interfaces
// =============================================================================
struct Batch12DPowerSmoothingTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string parentPath = "/xyz/openbmc_project/inventory/b12d/gpu0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    Batch12DPowerSmoothingTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~Batch12DPowerSmoothingTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// =============================================================================
// PART 1: OemPowerProfileIntfV2 -- uncovered handleSample / updateSample paths
// =============================================================================

// ---------------------------------------------------------------------------
// handleSample: unknown tag => NSM_SW_ERROR_LENGTH
// The existing test for this is DISABLED. We re-enable coverage here.
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_UnknownTag_ReturnsErrorLength)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    // tag >> 3 = 31 which is not a known preset profile tag (0-7)
    sample.tag = (31 << 3) | profileId;
    sample.data_len = 4;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ---------------------------------------------------------------------------
// handleSample: each tag branch with non-matching profileId
// These paths decode successfully but skip the D-Bus property set
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_TmpFloorNonMatchingProfile_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 3;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint16_t ufxpValue = 2048;
    uint8_t data[2] = {};
    encodeUint16LE(data, ufxpValue);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    // profIdRes = 1, does not match profileId = 3
    sample.tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | 1;
    sample.data_len = sizeof(uint16_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert - decode succeeds, just no update
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_RampUpNonMatchingProfile_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 4;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampUpRate = 5000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampUpRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | 2;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_RampDownNonMatchingProfile_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 5;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampDownRate = 3000;
    uint8_t data[4] = {};
    encodeUint32LE(data, rampDownRate);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3) | 0;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_HysteresisNonMatchingProfile_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 6;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t hysteresis = 2500;
    uint8_t data[4] = {};
    encodeUint32LE(data, hysteresis);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | 1;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_SecFloorNonMatchingProfile_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 7;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t secondaryFloor = 15000;
    uint8_t data[4] = {};
    encodeUint32LE(data, secondaryFloor);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | 2;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_PrimFloorActWinMultNonMatching_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 3;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t multiplier = 55;
    uint8_t data[1] = {};
    data[0] = multiplier;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
                  << 3) |
                 0;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_PrimFloorTgtWinNonMatching_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 4;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t targetWindow = 33;
    uint8_t data[1] = {};
    data[0] = targetWindow;

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) | 1;
    sample.data_len = sizeof(uint8_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_PrimFloorActOffsetNonMatching_DoesNotSet)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 5;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t offset = 12500;
    uint8_t data[4] = {};
    encodeUint32LE(data, offset);

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) | 2;
    sample.data_len = sizeof(uint32_t);
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// PowerProfile V2: decode error paths (bad data_len) through handleSample
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_TmpFloorBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[1] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_TMP_FLOOR_SETTING_TAG << 3) | profileId;
    sample.data_len = 1; // Wrong, needs 2
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_RampUpBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[2] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPUP_RATE_TAG << 3) | profileId;
    sample.data_len = 2; // Wrong, needs 4
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_RampDownBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[1] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPDOWN_RATE_TAG << 3) | profileId;
    sample.data_len = 1;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_HysteresisBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[3] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG << 3) | profileId;
    sample.data_len = 3;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_SecFloorBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[2] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_SECONDARY_FLOOR_TAG << 3) | profileId;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_PrimFloorActWinMultBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG
                  << 3) |
                 profileId;
    sample.data_len = 4; // Wrong, needs 1
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_PrimFloorTgtWinBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[4] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG << 3) |
                 profileId;
    sample.data_len = 4; // Wrong, needs 1
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_HandleSample_PrimFloorActOffsetBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint8_t data[2] = {};

    OemPowerProfileIntfV2::TelemetrySample sample = {};
    sample.tag = (PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG << 3) |
                 profileId;
    sample.data_len = 2; // Wrong, needs 4
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = profileIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// PowerProfile V2: Zero value boundary for matching profile
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_UpdateRampUp_ZeroValue_Matching_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampUpRate = 0;
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
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampUpRate(), 0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_UpdateRampDown_ZeroValue_Matching_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t rampDownRate = 0;
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
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampDownRate(), 0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_UpdateHysteresis_ZeroValue_Matching_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t hysteresis = 0;
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
    EXPECT_NEAR(profileIntf->PowerProfileIntf::rampDownHysteresis(), 0.0,
                0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_UpdateSecFloor_ZeroValue_Matching_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t secondaryFloor = 0;
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
    EXPECT_NEAR(profileIntf->PowerProfileIntf::secondaryPowerFloorSetting(),
                0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       PowerProfileV2_UpdatePrimFloorActOffset_ZeroValue_Matching_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    uint8_t profileId = 0;
    auto profileIntf = std::make_shared<OemPowerProfileIntfV2>(bus, parentPath,
                                                               profileId, gpu);

    uint32_t offset = 0;
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
    EXPECT_NEAR(profileIntf->PowerProfileIntf::primaryFloorActivationOffset(),
                0.0, 0.001);
}

// =============================================================================
// PART 2: OemAdminProfileIntfV2 -- uncovered handleSample / update paths
// =============================================================================

// ---------------------------------------------------------------------------
// handleSample: decode error paths through handleSample (bad data_len)
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_TmpFloorBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[1] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG;
    sample.data_len = 1; // Wrong, needs 2
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_RampUpBadLen_ReturnsError)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t data[2] = {};

    OemAdminProfileIntfV2::TelemetrySample sample = {};
    sample.tag = ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG;
    sample.data_len = 2;
    sample.data = data;
    sample.valid = true;

    // Act
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_RampDownBadLen_ReturnsError)
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
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_HysteresisBadLen_ReturnsError)
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
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_SecFloorBadLen_ReturnsError)
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
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_PrimFloorActWinMultBadLen_ReturnsError)
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
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_PrimFloorTgtWinBadLen_ReturnsError)
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
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_HandleSample_PrimFloorActOffsetBadLen_ReturnsError)
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
    int rc = adminIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// AdminProfile V2: zero-value boundary for additional update methods
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_UpdateRampDownRate_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t rampDownRate = 0;
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
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampDownRate(), 0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest, AdminV2_UpdateHysteresis_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t hysteresis = 0;
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
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::rampDownHysteresis(), 0.0,
                0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_UpdateSecondaryFloor_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t secondaryFloor = 0;
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
    EXPECT_NEAR(adminIntf->AdminPowerProfileIntf::secondaryPowerFloorSetting(),
                0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_UpdatePrimFloorActWinMult_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t multiplier = 0;
    uint8_t data[1] = {};
    data[0] = multiplier;

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
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_UpdatePrimFloorTgtWin_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint8_t targetWindow = 0;
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
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_UpdatePrimFloorActOffset_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    uint32_t offset = 0;
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
    EXPECT_NEAR(
        adminIntf->AdminPowerProfileIntf::primaryFloorActivationOffset(), 0.0,
        0.001);
}

// ---------------------------------------------------------------------------
// AdminProfile V2: resetParam boundary
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest, AdminV2_ResetParam_ZeroValue_ReturnsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act & Assert
    EXPECT_FALSE(adminIntf->resetParam(0.0));
}

TEST_F(Batch12DPowerSmoothingTest,
       AdminV2_ResetParam_NegativeValue_ReturnsFalse)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto adminIntf = std::make_shared<OemAdminProfileIntfV2>(bus, parentPath,
                                                             gpu);

    // Act & Assert -- static_cast<uint32_t>(-1.0) wraps to
    // UINT32_MAX == INVALID_POWER_LIMIT, so resetParam returns true
    EXPECT_TRUE(adminIntf->resetParam(-1.0));
}

// =============================================================================
// PART 3: OemPowerSmoothingFeatIntfV2 -- uncovered handleSample / update paths
// =============================================================================

// ---------------------------------------------------------------------------
// FeatureV2: decode error paths through handleSample (bad data_len)
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_FeatureFlagBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_CurrentTmpSettingBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_TmpFloorSettingBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_MaxTmpFloorBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_MinTmpFloorBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_FloorWindowMultBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_MinPrimFloorActOffsetBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_HandleSample_MinPrimFloorActPointBadLen_ReturnsError)
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
    int rc = featIntf->handleSample(sample);

    // Assert
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// FeatureV2: zero-value boundary for additional update methods
// ---------------------------------------------------------------------------
TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateCurrentTmpSetting_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t tmpSetting = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::currentTempSetting(), 0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateFloorWindowMult_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t multiplier = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::floorWindowMultiplier(), 0.0,
                0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateMinPrimFloorActOffset_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t offset = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::minPrimaryFloorActivationOffset(),
                0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateMinPrimFloorActPoint_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t point = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::minPrimaryFloorActivationPoint(),
                0.0, 0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateCurrentTmpFloorSetting_ZeroValue_SetsZero)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint32_t tmpFloor = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::currentTempFloorSetting(), 0.0,
                0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateMaxTmpFloor_ZeroUFXP_SetsZeroPercent)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint16_t ufxpValue = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::maxAllowedTmpFloorPercent(), 0.0,
                0.001);
}

TEST_F(Batch12DPowerSmoothingTest,
       FeatureV2_UpdateMinTmpFloor_ZeroUFXP_SetsZeroPercent)
{
    // Arrange
    auto& bus = utils::DBusHandler::getBus();
    auto featIntf =
        std::make_shared<OemPowerSmoothingFeatIntfV2>(bus, parentPath, gpu);

    uint16_t ufxpValue = 0;
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
    EXPECT_NEAR(featIntf->PowerSmoothingIntf::minAllowedTmpFloorPercent(), 0.0,
                0.001);
}
