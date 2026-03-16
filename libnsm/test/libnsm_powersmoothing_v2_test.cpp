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

#include "base.h"
#include "powersmoothing-powerprofile-api-v2.h"
#include <cstdint>
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

// ============================================================================
// Test Fixture
// ============================================================================

class LibnsmPowerSmoothingV2Test : public ::testing::Test
{
      protected:
	std::vector<uint8_t> msgBuffer;

	void SetUp() override
	{
		msgBuffer.resize(sizeof(struct nsm_msg) + 1024);
	}
};

// ============================================================================
// decode_get_powersmoothing_featinfo_v2_req Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, DecodeGetFeatInfoV2Req_ValidMessage)
{
	struct nsm_msg *msg =
	    reinterpret_cast<struct nsm_msg *>(msgBuffer.data());

	// Encode a valid request first
	int rc = encode_get_powersmoothing_featinfo_v2_req(1, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Decode it
	size_t msgLen =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req);
	rc = decode_get_powersmoothing_featinfo_v2_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(LibnsmPowerSmoothingV2Test, DecodeGetFeatInfoV2Req_NullMessage)
{
	int rc = decode_get_powersmoothing_featinfo_v2_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeFeatureFlagSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeFeatureFlagSample_ValidData)
{
	uint32_t featureFlag = 0x12345678;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeFeatureFlagSample(featureFlag, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeFeatureFlagSample_NullPointers)
{
	uint32_t featureFlag = 0x12345678;
	size_t dataLen;

	int rc = encodeFeatureFlagSample(featureFlag, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t buffer[4];
	rc = encodeFeatureFlagSample(featureFlag, buffer, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentTMPSettingSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentTMPSettingSample_ValidData)
{
	uint32_t tmpSetting = 1000;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentTMPSettingSample(tmpSetting, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentTMPSettingSample_NullPointers)
{
	uint32_t tmpSetting = 1000;
	size_t dataLen;

	int rc = encodeCurrentTMPSettingSample(tmpSetting, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentTMPFloorSettingSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentTMPFloorSettingSample_ValidData)
{
	uint32_t tmpFloorSetting = 500;
	uint8_t data[4];
	size_t dataLen;

	int rc =
	    encodeCurrentTMPFloorSettingSample(tmpFloorSetting, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentTMPFloorSettingSample_NullPointers)
{
	uint32_t tmpFloorSetting = 500;
	size_t dataLen;

	int rc = encodeCurrentTMPFloorSettingSample(tmpFloorSetting, nullptr,
						    &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeMaxTmpFloorSettingSample Tests (uint16_t)
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeMaxTmpFloorSettingSample_ValidData)
{
	uint16_t maxTmpFloor = 2000;
	uint8_t data[2];
	size_t dataLen;

	int rc = encodeMaxTmpFloorSettingSample(maxTmpFloor, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint16_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeMaxTmpFloorSettingSample_NullPointers)
{
	uint16_t maxTmpFloor = 2000;
	size_t dataLen;

	int rc = encodeMaxTmpFloorSettingSample(maxTmpFloor, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeMinTmpFloorSettingSample Tests (uint16_t)
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeMinTmpFloorSettingSample_ValidData)
{
	uint16_t minTmpFloor = 100;
	uint8_t data[2];
	size_t dataLen;

	int rc = encodeMinTmpFloorSettingSample(minTmpFloor, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint16_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeMinTmpFloorSettingSample_NullPointers)
{
	uint16_t minTmpFloor = 100;
	size_t dataLen;

	int rc = encodeMinTmpFloorSettingSample(minTmpFloor, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeFloorWindowMultiplierSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeFloorWindowMultiplierSample_ValidData)
{
	uint32_t multiplier = 10;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeFloorWindowMultiplierSample(multiplier, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeFloorWindowMultiplierSample_NullPointers)
{
	uint32_t multiplier = 10;
	size_t dataLen;

	int rc =
	    encodeFloorWindowMultiplierSample(multiplier, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeMinPrimaryFloorActivationOffset Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeMinPrimaryFloorActivationOffset_ValidData)
{
	uint32_t offset = 50;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeMinPrimaryFloorActivationOffset(offset, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeMinPrimaryFloorActivationOffset_NullPointers)
{
	uint32_t offset = 50;
	size_t dataLen;

	int rc =
	    encodeMinPrimaryFloorActivationOffset(offset, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeMinPrimaryFloorActivationPoint Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeMinPrimaryFloorActivationPoint_ValidData)
{
	uint32_t point = 75;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeMinPrimaryFloorActivationPoint(point, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeMinPrimaryFloorActivationPoint_NullPointers)
{
	uint32_t point = 75;
	size_t dataLen;

	int rc = encodeMinPrimaryFloorActivationPoint(point, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// decode_get_current_profile_info_v2_req Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       DecodeGetCurrentProfileInfoV2Req_ValidMessage)
{
	struct nsm_msg *msg =
	    reinterpret_cast<struct nsm_msg *>(msgBuffer.data());

	// Encode a valid request first
	int rc = encode_get_current_profile_info_v2_req(1, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Decode it
	size_t msgLen =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req);
	rc = decode_get_current_profile_info_v2_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(LibnsmPowerSmoothingV2Test, DecodeGetCurrentProfileInfoV2Req_NullMessage)
{
	int rc = decode_get_current_profile_info_v2_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeActivePresetProfileSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeActivePresetProfileSample_ValidData)
{
	uint32_t profile = 3;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeActivePresetProfileSample(profile, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint8_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeActivePresetProfileSample_NullPointers)
{
	uint32_t profile = 3;
	size_t dataLen;

	int rc = encodeActivePresetProfileSample(profile, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeAdminOverrideMaskSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeAdminOverrideMaskSample_ValidData)
{
	uint32_t mask = 0xFF00FF00;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeAdminOverrideMaskSample(mask, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint16_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeAdminOverrideMaskSample_NullPointers)
{
	uint32_t mask = 0xFF00FF00;
	size_t dataLen;

	int rc = encodeAdminOverrideMaskSample(mask, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentTMPFloorSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentTMPFloorSample_ValidData)
{
	uint32_t tmpFloor = 800;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentTMPFloorSample(tmpFloor, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint16_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentTMPFloorSample_NullPointers)
{
	uint32_t tmpFloor = 800;
	size_t dataLen;

	int rc = encodeCurrentTMPFloorSample(tmpFloor, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentRampupRateSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentRampupRateSample_ValidData)
{
	uint32_t rampupRate = 150;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentRampupRateSample(rampupRate, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentRampupRateSample_NullPointers)
{
	uint32_t rampupRate = 150;
	size_t dataLen;

	int rc = encodeCurrentRampupRateSample(rampupRate, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentRampdownRateSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentRampdownRateSample_ValidData)
{
	uint32_t rampdownRate = 120;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentRampdownRateSample(rampdownRate, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentRampdownRateSample_NullPointers)
{
	uint32_t rampdownRate = 120;
	size_t dataLen;

	int rc =
	    encodeCurrentRampdownRateSample(rampdownRate, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentRampdownHysteresisSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentRampdownHysteresisSample_ValidData)
{
	uint32_t hysteresis = 25;
	uint8_t data[4];
	size_t dataLen;

	int rc =
	    encodeCurrentRampdownHysteresisSample(hysteresis, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentRampdownHysteresisSample_NullPointers)
{
	uint32_t hysteresis = 25;
	size_t dataLen;

	int rc = encodeCurrentRampdownHysteresisSample(hysteresis, nullptr,
						       &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentSecondaryFloorSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, EncodeCurrentSecondaryFloorSample_ValidData)
{
	uint32_t secondaryFloor = 600;
	uint8_t data[4];
	size_t dataLen;

	int rc =
	    encodeCurrentSecondaryFloorSample(secondaryFloor, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentSecondaryFloorSample_NullPointers)
{
	uint32_t secondaryFloor = 600;
	size_t dataLen;

	int rc = encodeCurrentSecondaryFloorSample(secondaryFloor, nullptr,
						   &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentPrimaryFloorActivationWindowMultiplierSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodePrimaryFloorActivationWindowMultiplierSample_ValidData)
{
	uint32_t multiplier = 5;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
	    multiplier, data, &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint8_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodePrimaryFloorActivationWindowMultiplierSample_NullPointers)
{
	uint32_t multiplier = 5;
	size_t dataLen;

	int rc = encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
	    multiplier, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentPrimaryFloorTargetWindowSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentPrimaryFloorTargetWindowSample_ValidData)
{
	uint32_t targetWindow = 1000;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentPrimaryFloorTargetWindowSample(targetWindow, data,
							     &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint8_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentPrimaryFloorTargetWindowSample_NullPointers)
{
	uint32_t targetWindow = 1000;
	size_t dataLen;

	int rc = encodeCurrentPrimaryFloorTargetWindowSample(targetWindow,
							     nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// encodeCurrentPrimaryFloorActivationOffsetSample Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentPrimaryFloorActivationOffsetSample_ValidData)
{
	uint32_t offset = 30;
	uint8_t data[4];
	size_t dataLen;

	int rc = encodeCurrentPrimaryFloorActivationOffsetSample(offset, data,
								 &dataLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
}

TEST_F(LibnsmPowerSmoothingV2Test,
       EncodeCurrentPrimaryFloorActivationOffsetSample_NullPointers)
{
	uint32_t offset = 30;
	size_t dataLen;

	int rc = encodeCurrentPrimaryFloorActivationOffsetSample(
	    offset, nullptr, &dataLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// decode_get_preset_profile_info_v2_req Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test, DecodeGetPresetProfileInfoV2Req_ValidMessage)
{
	struct nsm_msg *msg =
	    reinterpret_cast<struct nsm_msg *>(msgBuffer.data());

	// Encode a valid request first (no payload)
	int rc = encode_get_preset_profile_info_v2_req(1, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Decode it
	size_t msgLen =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req);
	rc = decode_get_preset_profile_info_v2_req(msg, msgLen);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(LibnsmPowerSmoothingV2Test, DecodeGetPresetProfileInfoV2Req_NullMessage)
{
	int rc = decode_get_preset_profile_info_v2_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// decode_get_admin_override_profile_info_v2_req Tests
// ============================================================================

TEST_F(LibnsmPowerSmoothingV2Test,
       DecodeGetAdminOverrideProfileInfoV2Req_ValidMessage)
{
	struct nsm_msg *msg =
	    reinterpret_cast<struct nsm_msg *>(msgBuffer.data());

	// Encode a valid request first
	int rc = encode_get_admin_override_profile_info_v2_req(1, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// Decode it
	size_t msgLen =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req);
	rc = decode_get_admin_override_profile_info_v2_req(msg, msgLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(LibnsmPowerSmoothingV2Test,
       DecodeGetAdminOverrideProfileInfoV2Req_NullMessage)
{
	int rc = decode_get_admin_override_profile_info_v2_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
