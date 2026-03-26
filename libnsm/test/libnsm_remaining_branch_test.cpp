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
 * Remaining branch coverage for platform-environmental.c and
 * device-configuration.c encode/decode functions.
 *
 * Targets uncovered branches in:
 *  - leak detection info encode/decode (NULL, cc, length paths)
 *  - set leak detection thresholds encode/decode (NULL, length, pack paths)
 *  - decode_set_leak_detection_thresholds_resp (NULL, cc, length)
 *  - encode/decode_get_EGM_mode_resp (NULL flags, cc non-success, msg_len)
 *  - decode_set_EGM_mode_req (NULL, length, data_size)
 *  - encode/decode get_protection_options_resp (NULL, length, data_size)
 *  - encode_set_error_injection_payload_req (NULL, pack fail, EI branches)
 *  - decode_set_error_injection_payload_req (NULL, length, subtype branches)
 *  - decode_set_error_injection_payload_resp (data_size != 0)
 *  - encode_get_error_injection_payload_req (NULL, pack fail, type/subtype)
 */

#include "base.h"
#include "device-configuration.h"
#include "platform-environmental.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// 9-byte buf with cc=0xFF: for decode_reason_code_and_cc cc!=NSM_SUCCESS.
static std::vector<uint8_t> makeErrCcBuf()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // cc = non-success
	return buf;
}

// ===========================================================================
// encode_get_leak_detection_info_req: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeGetLeakDetInfoReq_NullMsg)
{
	auto rc = encode_get_leak_detection_info_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_leak_detection_info_req: pack fail (bad instance_id)
// ===========================================================================
TEST(RemainingBranch, EncodeGetLeakDetInfoReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_leak_detection_info_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_leak_detection_info_req: NULL msg
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoReq_NullMsg)
{
	auto rc = decode_get_leak_detection_info_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_leak_detection_info_req: msg too short
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_leak_detection_info_req(msg, buf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_leak_detection_info_req: data_size != 0
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoReq_DataSizeNonZero)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_get_leak_detection_info_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *request =
	    reinterpret_cast<struct nsm_get_leak_detection_info_req *>(
		msg->payload);
	request->hdr.data_size = 1; // must be 0
	auto rc = decode_get_leak_detection_info_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_leak_detection_info_resp: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeGetLeakDetInfoResp_NullMsg)
{
	uint8_t data[16] = {};
	auto rc = encode_get_leak_detection_info_resp(
	    0, NSM_SUCCESS, 0, 1, 1, data, sizeof(data), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_leak_detection_info_resp: NULL sensors_data
// ===========================================================================
TEST(RemainingBranch, EncodeGetLeakDetInfoResp_NullSensorsData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_leak_detection_info_resp(0, NSM_SUCCESS, 0, 1, 1,
						      nullptr, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_leak_detection_info_resp: cc != NSM_SUCCESS
// ===========================================================================
TEST(RemainingBranch, EncodeGetLeakDetInfoResp_CcNonSuccess)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[16] = {};
	auto rc = encode_get_leak_detection_info_resp(0, 0xFF, 0, 1, 1, data,
						      sizeof(data), msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_leak_detection_info_resp: NULL args
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoResp_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t num_sensors = 0, num_thresholds = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_get_leak_detection_info_resp(
	    msg, buf.size(), &cc, &reason_code, nullptr, &num_thresholds, data,
	    &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_leak_detection_info_resp(msg, buf.size(), &cc,
						 &reason_code, &num_sensors,
						 nullptr, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_leak_detection_info_resp(
	    msg, buf.size(), &cc, &reason_code, &num_sensors, &num_thresholds,
	    nullptr, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_leak_detection_info_resp(
	    msg, buf.size(), &cc, &reason_code, &num_sensors, &num_thresholds,
	    data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_leak_detection_info_resp: cc != NSM_SUCCESS
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t num_sensors = 0, num_thresholds = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_get_leak_detection_info_resp(
	    msg, buf.size(), &cc, &reason_code, &num_sensors, &num_thresholds,
	    data, &data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_leak_detection_info_resp: msg too short (cc=0 success)
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoResp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0=success, too short
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t num_sensors = 0, num_thresholds = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_get_leak_detection_info_resp(
	    msg, buf.size(), &cc, &reason_code, &num_sensors, &num_thresholds,
	    data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_req: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeSetLeakDetThresholdsReq_NullMsg)
{
	uint8_t data[16] = {};
	auto rc = encode_set_leak_detection_thresholds_req(
	    0, 1, 1, data, sizeof(data), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_req: NULL thresholds_data
// ===========================================================================
TEST(RemainingBranch, EncodeSetLeakDetThresholdsReq_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_set_leak_detection_thresholds_req(0, 1, 1, nullptr, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_req: pack fail
// ===========================================================================
TEST(RemainingBranch, EncodeSetLeakDetThresholdsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[16] = {};
	auto rc = encode_set_leak_detection_thresholds_req(kBadIid, 1, 1, data,
							   sizeof(data), msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_req: thresholds_data_len too short
// ===========================================================================
TEST(RemainingBranch, EncodeSetLeakDetThresholdsReq_DataLenTooShort)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[4] = {};
	// 1 sensor, 2 threshold levels: needs sensor_id+reserved + 2*uint16 +
	// padding but data is only 4 bytes
	auto rc =
	    encode_set_leak_detection_thresholds_req(0, 1, 2, data, 1, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_set_leak_detection_thresholds_req: NULL args
// ===========================================================================
TEST(RemainingBranch, DecodeSetLeakDetThresholdsReq_NullMsg)
{
	uint8_t num_s = 0, num_t = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_set_leak_detection_thresholds_req(
	    nullptr, 100, &num_s, &num_t, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetLeakDetThresholdsReq_NullNumSensors)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t num_t = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_set_leak_detection_thresholds_req(
	    msg, buf.size(), nullptr, &num_t, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetLeakDetThresholdsReq_NullNumThresholds)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t num_s = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_set_leak_detection_thresholds_req(
	    msg, buf.size(), &num_s, nullptr, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetLeakDetThresholdsReq_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t num_s = 0, num_t = 0;
	size_t data_len = 0;
	auto rc = decode_set_leak_detection_thresholds_req(
	    msg, buf.size(), &num_s, &num_t, nullptr, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetLeakDetThresholdsReq_NullDataLen)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t num_s = 0, num_t = 0;
	uint8_t data[64] = {};
	auto rc = decode_set_leak_detection_thresholds_req(
	    msg, buf.size(), &num_s, &num_t, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_leak_detection_thresholds_req: msg too short
// ===========================================================================
TEST(RemainingBranch, DecodeSetLeakDetThresholdsReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t num_s = 0, num_t = 0;
	uint8_t data[64] = {};
	size_t data_len = 0;
	auto rc = decode_set_leak_detection_thresholds_req(
	    msg, buf.size(), &num_s, &num_t, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_resp: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeSetLeakDetThresholdsResp_NullMsg)
{
	auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS, 0,
							    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_resp: cc != NSM_SUCCESS
// ===========================================================================
TEST(RemainingBranch, EncodeSetLeakDetThresholdsResp_CcNonSuccess)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_leak_detection_thresholds_resp(0, 0xFF, 0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_leak_detection_thresholds_resp: NULL args
// ===========================================================================
TEST(RemainingBranch, DecodeSetLeakDetThresholdsResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_set_leak_detection_thresholds_resp(nullptr, 100, &cc,
							    &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetLeakDetThresholdsResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason_code = 0;
	auto rc = decode_set_leak_detection_thresholds_resp(
	    msg, buf.size(), nullptr, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetLeakDetThresholdsResp_NullReasonCode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_set_leak_detection_thresholds_resp(msg, buf.size(),
							    &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_leak_detection_thresholds_resp: cc != NSM_SUCCESS
// ===========================================================================
TEST(RemainingBranch, DecodeSetLeakDetThresholdsResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_set_leak_detection_thresholds_resp(msg, buf.size(),
							    &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_set_leak_detection_thresholds_resp: msg too short (cc=0)
// ===========================================================================
TEST(RemainingBranch, DecodeSetLeakDetThresholdsResp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0, short
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_set_leak_detection_thresholds_resp(msg, buf.size(),
							    &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_EGM_mode_resp: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeGetEgmModeResp_NullMsg)
{
	bitfield8_t flags = {};
	auto rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, 0, &flags, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_EGM_mode_resp: NULL flags
// ===========================================================================
TEST(RemainingBranch, EncodeGetEgmModeResp_NullFlags)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_EGM_mode_resp: cc != NSM_SUCCESS
// ===========================================================================
TEST(RemainingBranch, EncodeGetEgmModeResp_CcNonSuccess)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	bitfield8_t flags = {};
	auto rc = encode_get_EGM_mode_resp(0, 0xFF, 0, &flags, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_EGM_mode_resp: NULL args
// ===========================================================================
TEST(RemainingBranch, DecodeGetEgmModeResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t data_size = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), nullptr, &data_size,
					   nullptr, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeGetEgmModeResp_NullDataSize)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, nullptr,
					   nullptr, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeGetEgmModeResp_NullFlags)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0;
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &data_size,
					   nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_EGM_mode_resp: cc != NSM_SUCCESS
// ===========================================================================
TEST(RemainingBranch, DecodeGetEgmModeResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &data_size,
					   &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_EGM_mode_resp: msg_len mismatch (cc=0 success)
// ===========================================================================
TEST(RemainingBranch, DecodeGetEgmModeResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0); // cc=0, too short
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &data_size,
					   &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_set_EGM_mode_req: NULL
// ===========================================================================
TEST(RemainingBranch, DecodeSetEgmModeReq_NullMsg)
{
	uint8_t mode = 0;
	auto rc = decode_set_EGM_mode_req(nullptr, 100, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RemainingBranch, DecodeSetEgmModeReq_NullRequestedMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_set_EGM_mode_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_EGM_mode_req: length mismatch
// ===========================================================================
TEST(RemainingBranch, DecodeSetEgmModeReq_LengthMismatch)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_set_EGM_mode_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_set_EGM_mode_req: data_size mismatch
// ===========================================================================
TEST(RemainingBranch, DecodeSetEgmModeReq_DataSizeMismatch)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_set_EGM_mode_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *request =
	    reinterpret_cast<struct nsm_set_EGM_mode_req *>(msg->payload);
	request->hdr.data_size = 0; // should be sizeof(uint8_t)
	uint8_t mode = 0;
	auto rc = decode_set_EGM_mode_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_protection_options_resp: NULL protection_mode (cc=0 success path)
// ===========================================================================
TEST(RemainingBranch, DecodeGetProtectionOptionsResp_NullProtectionMode)
{
	// Build a valid-looking success response
	const size_t respLen = sizeof(nsm_msg_hdr) +
			       sizeof(struct nsm_get_protection_options_resp);
	std::vector<uint8_t> buf(respLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *resp = reinterpret_cast<struct nsm_get_protection_options_resp *>(
	    msg->payload);
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->hdr.data_size = htole16(sizeof(uint8_t));
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_get_protection_options_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &reason_code, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_protection_options_resp: msg too short (cc=0 success)
// ===========================================================================
TEST(RemainingBranch, DecodeGetProtectionOptionsResp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0, short
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, protection_mode = 0;
	uint16_t reason_code = 0;
	auto rc = decode_get_protection_options_resp(
	    msg, buf.size(), &cc, &reason_code, &protection_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_protection_options_resp: data_size mismatch
// ===========================================================================
TEST(RemainingBranch, DecodeGetProtectionOptionsResp_DataSizeMismatch)
{
	const size_t respLen = sizeof(nsm_msg_hdr) +
			       sizeof(struct nsm_get_protection_options_resp);
	std::vector<uint8_t> buf(respLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *resp = reinterpret_cast<struct nsm_get_protection_options_resp *>(
	    msg->payload);
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->hdr.data_size = htole16(99); // wrong
	uint8_t cc = 0, protection_mode = 0;
	uint16_t reason_code = 0;
	auto rc = decode_get_protection_options_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &reason_code, &protection_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_error_injection_payload_req: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeSetErrorInjPayloadReq_NullMsg)
{
	uint8_t data[16] = {};
	auto rc = encode_set_error_injection_payload_req(
	    0, data, sizeof(data), EI_MEMORY_ERRORS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_error_injection_payload_req: NULL data
// ===========================================================================
TEST(RemainingBranch, EncodeSetErrorInjPayloadReq_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_error_injection_payload_req(
	    0, nullptr, 0, EI_MEMORY_ERRORS, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_error_injection_payload_req: pack fail
// ===========================================================================
TEST(RemainingBranch, EncodeSetErrorInjPayloadReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[16] = {};
	auto rc = encode_set_error_injection_payload_req(
	    kBadIid, data, sizeof(data), EI_MEMORY_ERRORS, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_set_error_injection_payload_req: EI_DEVICE_ERRORS with bad subtype
// ===========================================================================
TEST(RemainingBranch, EncodeSetErrorInjPayloadReq_DeviceErrors_BadSubtype)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[16] = {};
	auto rc = encode_set_error_injection_payload_req(
	    0, data, sizeof(data), EI_DEVICE_ERRORS, 0xFF, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_set_error_injection_payload_req: unsupported error_injection_type
// ===========================================================================
TEST(RemainingBranch, EncodeSetErrorInjPayloadReq_UnsupportedType)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[16] = {};
	auto rc = encode_set_error_injection_payload_req(0, data, sizeof(data),
							 0xFF, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_error_injection_payload_resp: data_size != 0
// ===========================================================================
TEST(RemainingBranch, DecodeSetErrorInjPayloadResp_DataSizeNonZero)
{
	// Build a response with data_size=1 (should be 0)
	std::vector<uint8_t> buf(11, 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // data_size = 1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_set_error_injection_payload_resp(msg, buf.size(), &cc,
							  &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_error_injection_payload_req: NULL msg
// ===========================================================================
TEST(RemainingBranch, EncodeGetErrorInjPayloadReq_NullMsg)
{
	auto rc = encode_get_error_injection_payload_req(0, EI_MEMORY_ERRORS, 0,
							 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_error_injection_payload_req: pack fail
// ===========================================================================
TEST(RemainingBranch, EncodeGetErrorInjPayloadReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_payload_req(
	    kBadIid, EI_MEMORY_ERRORS, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_error_injection_payload_req: type > EI_GPIO_SPOOFING
// ===========================================================================
TEST(RemainingBranch, EncodeGetErrorInjPayloadReq_TypeTooHigh)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_payload_req(0, 0xFF, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_error_injection_payload_req: EI_DEVICE_ERRORS with bad subtype
// ===========================================================================
TEST(RemainingBranch, EncodeGetErrorInjPayloadReq_DeviceErrors_BadSubtype)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_payload_req(0, EI_DEVICE_ERRORS,
							 0xFF, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_error_injection_payload_req: EI_DEVICE_ERRORS with valid subtype
// ===========================================================================
TEST(RemainingBranch, EncodeGetErrorInjPayloadReq_DeviceErrors_ValidSubtype)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_payload_req(
	    0, EI_DEVICE_ERRORS, EI_DEVICE_ERRORS_SUBTYPE_FATAL, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_leak_detection_info_resp: sensors_data_len < expected
// This triggers the sensors_data_len size validation check.
// ===========================================================================
TEST(RemainingBranch, DecodeGetLeakDetInfoResp_SensorsDataLenTooSmall)
{
	// Build a valid-looking success response with mismatched sensor count
	const size_t respLen = sizeof(nsm_msg_hdr) +
			       sizeof(struct nsm_get_leak_detection_info_resp);
	std::vector<uint8_t> buf(respLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *resp =
	    reinterpret_cast<struct nsm_get_leak_detection_info_resp *>(
		msg->payload);
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->number_of_sensors = 10;	      // many sensors
	resp->number_of_threshold_levels = 2; // each sensor needs 2+4+2=8 bytes
	resp->hdr.data_size = htole16(sizeof(resp->number_of_sensors) +
				      sizeof(resp->number_of_threshold_levels) +
				      1); // data_size tiny

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t num_sensors = 0, num_thresholds = 0;
	uint8_t data[256] = {};
	size_t data_len = 0;
	auto rc = decode_get_leak_detection_info_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &reason_code, &num_sensors, &num_thresholds, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_leak_detection_info_resp: sensors_data_len = 0
// This exercises the sensors_data_len == 0 branch (no sensor loop).
// ===========================================================================
TEST(RemainingBranch, EncodeGetLeakDetInfoResp_ZeroSensorsData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[1] = {0};
	auto rc = encode_get_leak_detection_info_resp(0, NSM_SUCCESS, 0, 0, 0,
						      data, 0, msg);
	EXPECT_EQ(rc, NSM_SUCCESS);
}

// ===========================================================================
// encode_set_leak_detection_thresholds_req + decode round-trip: success path
// ===========================================================================
TEST(RemainingBranch, SetLeakDetThresholds_EncodeDecodeRoundTrip)
{
	// 1 sensor, 1 threshold level
	// nsm_leak_detection_thresholds_data: sensor_id(1) + reserved(1) +
	// thresholds[1](2) = 4 bytes
	struct {
		uint8_t sensor_id;
		uint8_t reserved;
		uint16_t thresholds[1];
	} src_data = {.sensor_id = 5, .reserved = 0, .thresholds = {1234}};

	const size_t expected_size =
	    sizeof(struct nsm_leak_detection_thresholds_data);
	// The struct has thresholds[1], so for 1 threshold level it is the base
	// size

	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_leak_detection_thresholds_req(
	    0, 1, 1, reinterpret_cast<uint8_t *>(&src_data), expected_size,
	    msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode
	uint8_t num_sensors = 0, num_thresholds = 0;
	uint8_t out_data[64] = {};
	size_t out_len = 0;
	const size_t totalLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_set_leak_detection_thresholds_req) +
	    expected_size;
	rc = decode_set_leak_detection_thresholds_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), totalLen,
	    &num_sensors, &num_thresholds, out_data, &out_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(num_sensors, 1);
	EXPECT_EQ(num_thresholds, 1);
}

// ===========================================================================
// decode_get_EGM_mode_resp: success but command mismatch
// ===========================================================================
TEST(RemainingBranch, DecodeGetEgmModeResp_CommandMismatch)
{
	const size_t respLen =
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_EGM_mode_resp);
	std::vector<uint8_t> buf(respLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *resp =
	    reinterpret_cast<struct nsm_get_EGM_mode_resp *>(msg->payload);
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->hdr.command = 0xFF; // wrong command
	resp->hdr.data_size = htole16(sizeof(struct nsm_get_EGM_mode_resp) -
				      sizeof(struct nsm_common_resp));

	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_EGM_mode_resp: success but data_size mismatch
// ===========================================================================
TEST(RemainingBranch, DecodeGetEgmModeResp_DataSizeMismatch)
{
	const size_t respLen =
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_EGM_mode_resp);
	std::vector<uint8_t> buf(respLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto *resp =
	    reinterpret_cast<struct nsm_get_EGM_mode_resp *>(msg->payload);
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->hdr.command = NSM_GET_EGM_MODE;
	resp->hdr.data_size = htole16(99); // wrong

	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &data_size, &reason_code, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
