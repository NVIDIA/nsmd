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

/**
 * Branch coverage for powersmoothing-powerprofile-api-v2.c — secondary null
 * checks and data_len mismatch branches in decode sample helpers not exercised
 * by the existing libnsm_powersmoothing_v2_test.cpp.
 */

#include "base.h"
#include "powersmoothing-powerprofile-api-v2.h"
#include <gtest/gtest.h>

// ===========================================================================
// decode4ByteOfUint32Sample — secondary null (data==NULL) and data_len check
// if (data == NULL || sample == NULL)
// if (data_len != sizeof(uint32_t))
// ===========================================================================
TEST(PwrSmthV2Branch, Decode4ByteUint32Sample_NullData)
{
	uint32_t sample = 0;
	// data == NULL → first condition TRUE
	auto rc = decode4ByteOfUint32Sample(nullptr, sizeof(uint32_t), &sample);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PwrSmthV2Branch, Decode4ByteUint32Sample_DataLenMismatch)
{
	uint8_t data[3] = {0};
	uint32_t sample = 0;
	// data_len = 3 != sizeof(uint32_t) = 4 → NSM_SW_ERROR_LENGTH
	auto rc = decode4ByteOfUint32Sample(data, sizeof(data), &sample);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode2ByteOfUint16Sample — secondary null (data==NULL) and data_len check
// ===========================================================================
TEST(PwrSmthV2Branch, Decode2ByteUint16Sample_NullData)
{
	uint16_t sample = 0;
	auto rc = decode2ByteOfUint16Sample(nullptr, sizeof(uint16_t), &sample);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PwrSmthV2Branch, Decode2ByteUint16Sample_DataLenMismatch)
{
	uint8_t data[1] = {0};
	uint16_t sample = 0;
	// data_len = 1 != sizeof(uint16_t) = 2 → NSM_SW_ERROR_LENGTH
	auto rc = decode2ByteOfUint16Sample(data, sizeof(data), &sample);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decodeActivePresetProfileSample — secondary null (data==NULL) and data_len
// ===========================================================================
TEST(PwrSmthV2Branch, DecodeActivePresetProfileSample_NullData)
{
	uint8_t id = 0;
	auto rc = decodeActivePresetProfileSample(nullptr, 1, &id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PwrSmthV2Branch, DecodeActivePresetProfileSample_DataLenMismatch)
{
	uint8_t data[2] = {0};
	uint8_t id = 0;
	// data_len = 2 != sizeof(uint8_t) = 1 → NSM_SW_ERROR_LENGTH
	auto rc = decodeActivePresetProfileSample(data, sizeof(data), &id);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decodeCurrentPrimaryFloorActivationWindowMultiplierSample — secondary null
// if (data==NULL || current_primary_floor_activation_window_multiplier==NULL)
// ===========================================================================
TEST(PwrSmthV2Branch, DecodeCurrentPrimaryFloorActivWindowMultiplier_NullData)
{
	uint8_t val = 0;
	auto rc = decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
	    nullptr, 1, &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PwrSmthV2Branch,
     DecodeCurrentPrimaryFloorActivWindowMultiplier_DataLenMismatch)
{
	uint8_t data[2] = {0};
	uint8_t val = 0;
	auto rc = decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
	    data, sizeof(data), &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decodeCurrentPrimaryFloorTargetWindowSample — secondary null (data==NULL)
// ===========================================================================
TEST(PwrSmthV2Branch, DecodeCurrentPrimaryFloorTargetWindow_NullData)
{
	uint8_t val = 0;
	auto rc = decodeCurrentPrimaryFloorTargetWindowSample(nullptr, 1, &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PwrSmthV2Branch, DecodeCurrentPrimaryFloorTargetWindow_DataLenMismatch)
{
	uint8_t data[2] = {0};
	uint8_t val = 0;
	auto rc = decodeCurrentPrimaryFloorTargetWindowSample(
	    data, sizeof(data), &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
