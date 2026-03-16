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

// Branch Coverage Tests for platform-environmental.c
// This file contains tests specifically targeting untested branches:
// - Error paths in encode/decode functions
// - Completion code non-success paths
// - pack_nsm_header failure scenarios
// - NULL pointer validation branches
// - Data validation and boundary conditions

#include "base.h"
#include "common-tests.hpp"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vector>

// =============================================================================
// Branch Coverage Tests for encode_get_temperature_reading_resp
// =============================================================================

TEST(PlatformEnvBranch, EncodeGetTemperatureReadingRespWithErrorCC)
{
	// Test branch: cc != NSM_SUCCESS
	// This tests the error path where completion code is not success
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_ERR_NOT_READY;
	uint16_t reason_code = 0x1234;
	double temperature = 0.0; // Should not be used when cc != NSM_SUCCESS

	int rc = encode_get_temperature_reading_resp(0, cc, reason_code,
						     temperature, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify it encoded as error response with reason code
	auto *payload =
	    reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(payload->completion_code, NSM_ERR_NOT_READY);
}

TEST(PlatformEnvBranch, EncodeGetTemperatureReadingRespSuccess)
{
	// Test branch: cc == NSM_SUCCESS (opposite branch from above)
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	double temperature = 45.5;

	int rc = encode_get_temperature_reading_resp(0, cc, reason_code,
						     temperature, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify temperature encoding
	auto *response =
	    reinterpret_cast<struct nsm_get_temperature_reading_resp *>(
		msg->payload);
	EXPECT_EQ(response->hdr.completion_code, NSM_SUCCESS);

	// Verify temperature value was encoded
	int32_t reading = le32toh(response->reading);
	double decoded = reading / (double)(1 << 8);
	EXPECT_NEAR(decoded, temperature, 0.1);
}

// =============================================================================
// Branch Coverage Tests for decode_get_temperature_reading_resp
// =============================================================================

TEST(PlatformEnvBranch, DecodeGetTemperatureReadingRespWithNonSuccessCC)
{
	// Test branch: *cc != NSM_SUCCESS in decode
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA
	    0x80, // RQ=1, D=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
	    NSM_GET_TEMPERATURE_READING,
	    NSM_ERR_INVALID_DATA, // Non-success CC
	    0x00,
	    0x01 // Reason code
	};

	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	double temperature = 0.0;

	int rc = decode_get_temperature_reading_resp(
	    msg, responseMsg.size(), &cc, &reason_code, &temperature);

	// Should return success but cc will be non-success
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(reason_code, 0x0100);
}

TEST(PlatformEnvBranch, DecodeGetTemperatureReadingRespInvalidDataSize)
{
	// Test branch: data_size != sizeof(int32_t)
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,
	    0x80,
	    0x89,
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
	    NSM_GET_TEMPERATURE_READING,
	    NSM_SUCCESS,
	    0x00,
	    0x00, // reason code
	    0x03,
	    0x00, // data_size = 3 (invalid, should be 4)
	    0x00,
	    0x01,
	    0x02,
	    0x00 // reading (4 bytes for proper struct size, but data_size=3)
	};

	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	double temperature = 0.0;

	int rc = decode_get_temperature_reading_resp(
	    msg, responseMsg.size(), &cc, &reason_code, &temperature);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// =============================================================================
// Branch Coverage Tests for encode_get_current_power_draw_resp
// =============================================================================

TEST(PlatformEnvBranch, EncodeGetCurrentPowerDrawRespWithErrorCC)
{
	// Test branch: cc != NSM_SUCCESS
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_ERR_INVALID_REQUEST_TYPE;
	uint16_t reason_code = 0x5678;
	uint32_t reading = 0; // Should not be used

	int rc = encode_get_current_power_draw_resp(0, cc, reason_code, reading,
						    msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *payload =
	    reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(payload->completion_code, NSM_ERR_INVALID_REQUEST_TYPE);
}

TEST(PlatformEnvBranch, EncodeGetCurrentPowerDrawRespSuccess)
{
	// Test branch: cc == NSM_SUCCESS
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint32_t reading = 25000; // 250.00 Watts

	int rc = encode_get_current_power_draw_resp(0, cc, reason_code, reading,
						    msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *response =
	    reinterpret_cast<nsm_get_current_power_draw_resp *>(msg->payload);
	EXPECT_EQ(response->hdr.completion_code, NSM_SUCCESS);
	EXPECT_EQ(le32toh(response->reading), reading);
}

// =============================================================================
// Branch Coverage Tests for decode_get_current_power_draw_resp
// =============================================================================

TEST(PlatformEnvBranch, DecodeGetCurrentPowerDrawRespWithNonSuccessCC)
{
	// Test branch: *cc != NSM_SUCCESS
	std::vector<uint8_t> responseMsg{
	    0x10,	   0xDE, 0x80, 0x89, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
	    NSM_GET_POWER,
	    NSM_BUSY,		// Non-success CC
	    0x00,	   0x02 // Reason code
	};

	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint32_t reading = 0;

	int rc = decode_get_current_power_draw_resp(
	    msg, responseMsg.size(), &cc, &reason_code, &reading);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_BUSY);
	EXPECT_EQ(reason_code, 0x0200);
}

TEST(PlatformEnvBranch, DecodeGetCurrentPowerDrawRespInvalidLength)
{
	// Test branch: msg_len < required size
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,
	    0x80,
	    0x89,
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL,
	    NSM_GET_POWER,
	    NSM_SUCCESS
	    // Missing data - message too short
	};

	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint32_t reading = 0;

	int rc = decode_get_current_power_draw_resp(
	    msg, responseMsg.size(), &cc, &reason_code, &reading);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// Branch Coverage Tests for decode_get_temperature_reading_req
// =============================================================================

TEST(PlatformEnvBranch, DecodeGetTemperatureReadingReqNullMsg)
{
	// Test branch: msg == NULL
	uint8_t sensor_id = 0;

	int rc = decode_get_temperature_reading_req(nullptr, 100, &sensor_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvBranch, DecodeGetTemperatureReadingReqNullSensorId)
{
	// Test branch: sensor_id == NULL
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());

	int rc =
	    decode_get_temperature_reading_req(msg, requestMsg.size(), nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvBranch, DecodeGetTemperatureReadingReqInvalidDataSize)
{
	// Test branch: request->hdr.data_size < sizeof(request->sensor_id)
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());

	// First encode a valid request
	encode_get_temperature_reading_req(0, 5, msg);

	// Now corrupt the data_size field
	auto *request =
	    reinterpret_cast<nsm_get_temperature_reading_req *>(msg->payload);
	request->hdr.data_size = 0; // Too small

	uint8_t sensor_id = 0;
	int rc = decode_get_temperature_reading_req(msg, requestMsg.size(),
						    &sensor_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// =============================================================================
// Branch Coverage Tests for decode_get_current_power_draw_req
// =============================================================================

TEST(PlatformEnvBranch, DecodeGetCurrentPowerDrawReqNullMsg)
{
	// Test branch: msg == NULL
	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;

	int rc = decode_get_current_power_draw_req(nullptr, 100, &sensor_id,
						   &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvBranch, DecodeGetCurrentPowerDrawReqNullSensorId)
{
	// Test branch: sensor_id == NULL
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t averaging_interval = 0;

	int rc = decode_get_current_power_draw_req(
	    msg, requestMsg.size(), nullptr, &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvBranch, DecodeGetCurrentPowerDrawReqNullAveragingInterval)
{
	// Test branch: averaging_interval == NULL
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t sensor_id = 0;

	int rc = decode_get_current_power_draw_req(msg, requestMsg.size(),
						   &sensor_id, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatformEnvBranch, DecodeGetCurrentPowerDrawReqInvalidDataSize)
{
	// Test branch: request->hdr.data_size < expected size
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_current_power_draw_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());

	// Encode valid request first
	encode_get_current_power_draw_req(0, 3, 100, msg);

	// Corrupt data_size
	auto *request =
	    reinterpret_cast<nsm_get_current_power_draw_req *>(msg->payload);
	request->hdr.data_size = 1; // Too small

	uint8_t sensor_id = 0;
	uint8_t averaging_interval = 0;
	int rc = decode_get_current_power_draw_req(
	    msg, requestMsg.size(), &sensor_id, &averaging_interval);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// =============================================================================
// Branch Coverage Tests for encode_get_max_observed_power_resp
// =============================================================================

// Additional branch coverage tests can be added as needed
// Focus: Error paths, cc != NSM_SUCCESS branches, NULL checks, validation
// branches
