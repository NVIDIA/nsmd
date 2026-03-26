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
 * Branch coverage batch 3 for device-configuration.c
 *
 * Targets: NULL pointer and length validation branches across
 * encode/decode functions not yet exercised by branch tests 1-2.
 */

#include "base.h"
#include "device-configuration.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// 11-byte buf with data_size=1 to trigger decode resp data_size != 0 checks.
static std::vector<uint8_t> makeNonZeroDataSizeBuf()
{
	std::vector<uint8_t> buf(11, 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // nsm_common_resp.data_size = 1
	return buf;
}

// ===========================================================================
// encode_set_protection_options_req: pack fail (iid=32)
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetProtOptionsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_protection_options_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_set_protection_options_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetProtOptionsReq_NullMsg)
{
	auto rc = encode_set_protection_options_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_protection_options_req: msg too short
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetProtOptionsReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_set_protection_options_req(msg, buf.size(), &mode);
	// decode_common_req will fail with short length
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_protection_options_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetProtOptionsResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_set_protection_options_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_protection_options_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetProtOptionsReq_NullMsg)
{
	auto rc = encode_get_protection_options_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_protection_options_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetProtOptionsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_protection_options_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_protection_options_resp: NULL protection_mode
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetProtOptionsResp_NullMode)
{
	// Need cc=NSM_SUCCESS so decode_common_resp succeeds and we reach the
	// NULL check. Use 12 bytes (larger than nsm_msg_hdr + nsm_common_resp).
	const size_t respLen = sizeof(nsm_msg_hdr) +
			       sizeof(struct nsm_get_protection_options_resp);
	std::vector<uint8_t> buf(respLen, 0);
	// Set data_size=1 so it equals sizeof(uint8_t)
	buf[sizeof(nsm_msg_hdr) + 4] = 1;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_protection_options_resp(msg, buf.size(), &cc,
						     &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_protection_options_resp: msg too short (cc=0=success)
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetProtOptionsResp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, mode = 0;
	uint16_t reason = 0;
	auto rc = decode_get_protection_options_resp(msg, buf.size(), &cc,
						     &reason, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_EGM_mode_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetEgmModeReq_NullMsg)
{
	auto rc = encode_get_EGM_mode_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_EGM_mode_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetEgmModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_EGM_mode_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_EGM_mode_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetEgmModeResp_NullMsg)
{
	bitfield8_t flags = {0};
	auto rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, 0, &flags, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_EGM_mode_resp: NULL flags
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetEgmModeResp_NullFlags)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_EGM_mode_resp: NULL cc
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetEgmModeResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), nullptr, &ds,
					   &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_EGM_mode_resp: NULL flags
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetEgmModeResp_NullFlags)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_EGM_mode_resp: msg_len mismatch (cc=0=success)
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetEgmModeResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_EGM_mode_req: NULL msg (via encode_common_req)
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetEgmModeReq_NullMsg)
{
	auto rc = encode_set_EGM_mode_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_EGM_mode_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetEgmModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_EGM_mode_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_EGM_mode_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetEgmModeReq_NullMsg)
{
	uint8_t mode = 0;
	auto rc = decode_set_EGM_mode_req(nullptr, 100, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_EGM_mode_req: NULL requested_mode
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetEgmModeReq_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_set_EGM_mode_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_EGM_mode_req: msg_len mismatch
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetEgmModeReq_MsgLenMismatch)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_set_EGM_mode_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_confidential_compute_mode_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetCCModeV1Req_NullMsg)
{
	auto rc = encode_set_confidential_compute_mode_v1_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_confidential_compute_mode_v1_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetCCModeV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_confidential_compute_mode_v1_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_confidential_compute_mode_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetCCModeV1Req_NullMsg)
{
	uint8_t mode = 0;
	auto rc =
	    decode_set_confidential_compute_mode_v1_req(nullptr, 100, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_confidential_compute_mode_v1_req: NULL mode
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetCCModeV1Req_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_set_confidential_compute_mode_v1_req(msg, buf.size(),
							      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_confidential_compute_mode_v1_req: msg_len mismatch
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetCCModeV1Req_MsgLenMismatch)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc =
	    decode_set_confidential_compute_mode_v1_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_confidential_compute_mode_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetCCModeV1Req_NullMsg)
{
	auto rc = encode_get_confidential_compute_mode_v1_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_confidential_compute_mode_v1_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetCCModeV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_confidential_compute_mode_v1_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_set_device_mode_setting_req: NULL msg (via encode_common_req)
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetDeviceModeSettingReq_NullMsg)
{
	auto rc = encode_set_device_mode_setting_req(
	    0, 0, static_cast<enum nsm_l1_prediction_mode_config>(0), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_device_mode_setting_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetDeviceModeSettingReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_device_mode_setting_req(
	    kBadIid, 0, static_cast<enum nsm_l1_prediction_mode_config>(0),
	    msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_device_mode_settings_req: NULL device_mode_index
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetDeviceModeSettingsReq_NullIndex)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_l1_prediction_mode_config mode =
	    static_cast<enum nsm_l1_prediction_mode_config>(0);
	auto rc = decode_set_device_mode_settings_req(msg, buf.size(), nullptr,
						      &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_device_mode_settings_req: NULL device_mode
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetDeviceModeSettingsReq_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	auto rc =
	    decode_set_device_mode_settings_req(msg, buf.size(), &idx, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_device_mode_settings_req: msg_len mismatch
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetDeviceModeSettingsReq_MsgLenMismatch)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	enum nsm_l1_prediction_mode_config mode =
	    static_cast<enum nsm_l1_prediction_mode_config>(0);
	auto rc =
	    decode_set_device_mode_settings_req(msg, buf.size(), &idx, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_set_device_mode_setting_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetDeviceModeSettingResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_set_device_mode_setting_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_error_injection_mode_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetErrInjModeV1Req_NullMsg)
{
	auto rc = encode_get_error_injection_mode_v1_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_error_injection_mode_v1_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetErrInjModeV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_mode_v1_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_error_injection_mode_v1_resp: NULL data
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetErrInjModeV1Resp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_mode_v1_resp(0, NSM_SUCCESS, 0,
							  nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_error_injection_mode_v1_resp: NULL data
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetErrInjModeV1Resp_NullData)
{
	// Need cc=NSM_SUCCESS, so use a properly sized zero buffer.
	const size_t respLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_get_error_injection_mode_v1_resp);
	std::vector<uint8_t> buf(respLen, 0);
	// Set data_size = sizeof(nsm_error_injection_mode_v1) at offset
	buf[sizeof(nsm_msg_hdr) + 4] =
	    sizeof(struct nsm_error_injection_mode_v1);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_error_injection_mode_v1_resp(msg, buf.size(), &cc,
							  &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_error_injection_mode_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetErrInjModeV1Req_NullMsg)
{
	auto rc = encode_set_error_injection_mode_v1_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_error_injection_mode_v1_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetErrInjModeV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_error_injection_mode_v1_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_error_injection_mode_v1_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetErrInjModeV1Resp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_error_injection_mode_v1_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_current_error_injection_types_v1_req: NULL data
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetCurrErrInjTypesV1Req_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_set_current_error_injection_types_v1_req(0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_current_error_injection_types_v1_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetCurrErrInjTypesV1Resp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_current_error_injection_types_v1_resp(
	    msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_supported_error_injection_types_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetSupportedErrInjTypesV1Req_NullMsg)
{
	auto rc = encode_get_supported_error_injection_types_v1_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_supported_error_injection_types_v1_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetSupportedErrInjTypesV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_get_supported_error_injection_types_v1_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_get_current_error_injection_types_v1_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetCurrErrInjTypesV1Req_NullMsg)
{
	auto rc = encode_get_current_error_injection_types_v1_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_error_injection_types_v1_req: pack fail
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetCurrErrInjTypesV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_error_injection_types_v1_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_activate_error_injection_payload_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeActivateErrInjReq_NullMsg)
{
	auto rc = encode_activate_error_injection_payload_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_activate_error_injection_payload_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch3, DecodeActivateErrInjResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_activate_error_injection_payload_resp(msg, buf.size(),
							       &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_error_injection_payload_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetErrInjPayloadReq_NullMsg)
{
	auto rc = encode_get_error_injection_payload_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_error_injection_payload_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch3, DecodeSetErrInjPayloadResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_error_injection_payload_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_fpga_diagnostics_settings_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetFpgaDiagReq_NullMsg)
{
	auto rc = encode_get_fpga_diagnostics_settings_req(0, GET_WP_SETTINGS,
							   nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_fpga_diagnostics_settings_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetFpgaDiagReq_NullMsg)
{
	enum fpga_diagnostics_settings_data_index idx = GET_WP_SETTINGS;
	auto rc = decode_get_fpga_diagnostics_settings_req(nullptr, 100, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_fpga_diagnostics_settings_req: NULL data_index
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetFpgaDiagReq_NullDataIndex)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_fpga_diagnostics_settings_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_fpga_diagnostics_settings_req: msg too short
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetFpgaDiagReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum fpga_diagnostics_settings_data_index idx = GET_WP_SETTINGS;
	auto rc =
	    decode_get_fpga_diagnostics_settings_req(msg, buf.size(), &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_EGM_mode_resp: NULL msg (via encode_common_resp)
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetEgmModeResp_NullMsg)
{
	auto rc = encode_set_EGM_mode_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_device_mode_settings_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetDeviceModeSettingsResp_NullMsg)
{
	auto rc =
	    encode_set_device_mode_settings_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_protection_options_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetProtOptionsResp_NullMsg)
{
	auto rc =
	    encode_set_protection_options_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_protection_options_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetProtOptionsResp_NullMsg)
{
	auto rc =
	    encode_get_protection_options_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_error_injection_mode_v1_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetErrInjModeV1Resp_NullMsg)
{
	auto rc =
	    encode_set_error_injection_mode_v1_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_activate_error_injection_payload_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeActivateErrInjResp_NullMsg)
{
	auto rc = encode_activate_error_injection_payload_resp(0, NSM_SUCCESS,
							       0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_error_injection_payload_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetErrInjPayloadResp_NullMsg)
{
	auto rc =
	    encode_set_error_injection_payload_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_current_error_injection_types_v1_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetCurrErrInjTypesV1Resp_NullMsg)
{
	auto rc = encode_set_current_error_injection_types_v1_resp(
	    0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_confidential_compute_mode_v1_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetCCModeV1Resp_NullMsg)
{
	auto rc = encode_set_confidential_compute_mode_v1_resp(0, NSM_SUCCESS,
							       0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_supported_error_injection_types_v1_resp: NULL data
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetSupportedErrInjTypesV1Resp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_supported_error_injection_types_v1_resp(
	    0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_current_error_injection_types_v1_resp: NULL data
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetCurrErrInjTypesV1Resp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_error_injection_types_v1_resp(
	    0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_error_injection_types_v1_resp: NULL data
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetErrInjTypesV1Resp_NullData)
{
	// Need cc=NSM_SUCCESS with properly sized buf
	const size_t respLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_get_error_injection_types_mask_resp);
	std::vector<uint8_t> buf(respLen, 0);
	buf[sizeof(nsm_msg_hdr) + 4] =
	    sizeof(struct nsm_error_injection_types_mask);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_error_injection_types_v1_resp(msg, buf.size(), &cc,
							   &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_error_injection_payload_resp: NULL data
// ===========================================================================
TEST(DevConfigBranch3, EncodeGetErrInjPayloadResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_error_injection_payload_resp(0, NSM_SUCCESS, 0, 0,
							  0, nullptr, 4, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_confidential_compute_mode_v1_resp: NULL data_size (args)
// ===========================================================================
TEST(DevConfigBranch3, DecodeGetCCModeV1Resp_NullDataSize)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cur = 0, pend = 0;
	uint16_t reason = 0;
	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    msg, buf.size(), nullptr, nullptr, &reason, &cur, &pend);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_reconfiguration_permissions_v1_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeSetReconfigPermV1Resp_NullMsg)
{
	auto rc = encode_set_reconfiguration_permissions_v1_resp(0, NSM_SUCCESS,
								 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_enable_disable_gpu_ist_mode_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch3, EncodeEnableDisableGpuIstModeResp_NullMsg)
{
	auto rc =
	    encode_enable_disable_gpu_ist_mode_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
