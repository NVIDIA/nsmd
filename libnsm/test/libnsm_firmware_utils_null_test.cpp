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
 * Branch coverage tests for libnsm/firmware-utils.c
 * Covers "if (msg == NULL)" TRUE branches not covered by the main test file.
 */

#include "firmware-utils.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// encode_nsm_query_get_erot_state_parameters_req — L307
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeGetErotStateParamReq_NullMsg)
{
	struct nsm_firmware_erot_state_info_req fw_req = {};
	auto rc =
	    encode_nsm_query_get_erot_state_parameters_req(0, &fw_req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_query_get_erot_state_parameters_resp — L342
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeGetErotStateParamResp_NullMsg)
{
	auto rc = encode_nsm_query_get_erot_state_parameters_resp(
	    0, NSM_SUCCESS, 0, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_request_0_resp — L930
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeIrreversibleConfigReq0Resp_NullMsg)
{
	auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
	    0, NSM_SUCCESS, 0, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_request_1_resp — L973
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeIrreversibleConfigReq1Resp_NullMsg)
{
	auto rc = encode_nsm_firmware_irreversible_config_request_1_resp(
	    0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_irreversible_config_request_2_resp — L1011
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeIrreversibleConfigReq2Resp_NullMsg)
{
	auto rc = encode_nsm_firmware_irreversible_config_request_2_resp(
	    0, NSM_SUCCESS, 0, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// decode_nsm_firmware_irreversible_config_request_1_resp — L1084
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, DecodeIrreversibleConfigReq1Resp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_nsm_firmware_irreversible_config_request_1_resp(
	    nullptr, 0, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// decode_nsm_firmware_irreversible_config_request_2_resp — L1105
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, DecodeIrreversibleConfigReq2Resp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
	auto rc = decode_nsm_firmware_irreversible_config_request_2_resp(
	    nullptr, 0, &cc, &reason_code, &cfg_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwUtilsNullBranch, DecodeIrreversibleConfigReq2Resp_NullCfgResp)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_nsm_firmware_irreversible_config_request_2_resp(
	    msg, buf.size(), &cc, &reason_code, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_code_auth_key_perm_query_req — L1165
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeCodeAuthKeyPermQueryReq_NullMsg)
{
	auto rc = encode_nsm_code_auth_key_perm_query_req(0, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_code_auth_key_perm_update_req — L1366
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeCodeAuthKeyPermUpdateReq_NullMsg)
{
	auto rc = encode_nsm_code_auth_key_perm_update_req(
	    0, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE, 0, 0,
	    0, 0, 0, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_code_auth_key_perm_update_resp — L1453
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeCodeAuthKeyPermUpdateResp_NullMsg)
{
	auto rc = encode_nsm_code_auth_key_perm_update_resp(0, NSM_SUCCESS, 0,
							    0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_firmware_set_rot_property_req — L1740
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeFwSetRotPropertyReq_NullMsg)
{
	struct nsm_firmware_set_rot_property_req fw_req = {};
	auto rc = encode_nsm_firmware_set_rot_property_req(0, &fw_req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_dot_unlock_challenge_resp — L2333
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeDotUnlockChallengeResp_NullMsg)
{
	auto rc = encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, 0,
						       nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_dot_unlock_resp — L2454
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeDotUnlockResp_NullMsg)
{
	auto rc = encode_nsm_dot_unlock_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_dot_get_info_req — L2502
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeDotGetInfoReq_NullMsg)
{
	auto rc = encode_nsm_dot_get_info_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_dot_get_info_resp — L2545
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeDotGetInfoResp_NullMsg)
{
	auto rc = encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, 0, 0, 0, 0,
					       nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_dot_get_status_req — L2627
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeDotGetStatusReq_NullMsg)
{
	auto rc = encode_nsm_dot_get_status_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// encode_nsm_dot_get_status_resp — L2669
// ---------------------------------------------------------------------------
TEST(FwUtilsNullBranch, EncodeDotGetStatusResp_NullMsg)
{
	auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
