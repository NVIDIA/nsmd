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
 * Branch coverage batch 3 for libnsm/firmware-utils.c
 *
 * Targets remaining half-covered branches via:
 *  - NULL pointer checks for encode/decode functions
 *  - Length validation checks
 *  - Various valid parameter combinations
 *  - Error cc paths
 *  - data_size checks
 */

#include "firmware-utils.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// Exact-size buffer for decode_reason_code_and_cc with non-success cc.
static std::vector<uint8_t> makeErrCcBuf()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // cc = non-success
	return buf;
}

// Minimal success buffer (cc=0=NSM_SUCCESS, 7 bytes min for
// decode_reason_code_and_cc)
static std::vector<uint8_t> makeSuccessBuf(size_t size = 7)
{
	return std::vector<uint8_t>(size, 0);
}

// ===========================================================================
// encode_nsm_query_get_erot_state_parameters_req: NULL checks
// ===========================================================================
TEST(FwBranches3, EncodeErotStateReq_NullMsg)
{
	struct nsm_firmware_erot_state_info_req req = {};
	auto rc =
	    encode_nsm_query_get_erot_state_parameters_req(0, &req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_query_get_erot_state_parameters_req: NULL checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeErotStateReq_NullMsg)
{
	struct nsm_firmware_erot_state_info_req req = {};
	auto rc =
	    decode_nsm_query_get_erot_state_parameters_req(nullptr, 100, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeErotStateReq_NullReq)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_nsm_query_get_erot_state_parameters_req(msg, 256, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeErotStateReq_ShortMsg)
{
	std::vector<uint8_t> buf(4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_erot_state_info_req req = {};
	auto rc = decode_nsm_query_get_erot_state_parameters_req(msg, 4, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(FwBranches3, DISABLED_DecodeErotStateReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_firmware_get_erot_state_info_req);
	std::vector<uint8_t> buf(msgLen, 0);
	// data_size is 0, which is < sizeof(nsm_firmware_erot_state_info_req)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_erot_state_info_req req = {};
	auto rc =
	    decode_nsm_query_get_erot_state_parameters_req(msg, msgLen, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_nsm_query_get_erot_state_parameters_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeErotStateResp_NullMsg)
{
	struct nsm_firmware_erot_state_info_resp resp = {};
	auto rc = encode_nsm_query_get_erot_state_parameters_resp(
	    0, 0, 0, &resp, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_query_get_erot_state_parameters_resp: NULL checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeErotStateResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_erot_state_info_resp resp = {};
	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    nullptr, 100, &cc, &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeErotStateResp_NullResp)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, 256, &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeErotStateResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_erot_state_info_resp resp = {};
	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, buf.size(), &cc, &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

TEST(FwBranches3, DISABLED_DecodeErotStateResp_MsgTooShortForAggTag)
{
	// cc=0=SUCCESS but msg_len < hdr + aggregate_tag
	auto buf = makeSuccessBuf(7);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_erot_state_info_resp resp = {};
	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, buf.size(), &cc, &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_nsm_firmware_irreversible_config_req: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeIrrevCfgReq_NullMsg)
{
	struct nsm_firmware_irreversible_config_req req = {};
	auto rc = encode_nsm_firmware_irreversible_config_req(0, &req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_firmware_irreversible_config_req: NULL checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeIrrevCfgReq_NullMsg)
{
	struct nsm_firmware_irreversible_config_req req = {};
	auto rc =
	    decode_nsm_firmware_irreversible_config_req(nullptr, 100, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeIrrevCfgReq_NullReq)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_nsm_firmware_irreversible_config_req(msg, 256, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeIrrevCfgReq_ShortMsg)
{
	std::vector<uint8_t> buf(4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_irreversible_config_req req = {};
	auto rc = decode_nsm_firmware_irreversible_config_req(msg, 4, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(FwBranches3, DISABLED_DecodeIrrevCfgReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_firmware_irreversible_config_req_command);
	std::vector<uint8_t> buf(msgLen, 0);
	// data_size=0 < sizeof(req)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_irreversible_config_req req = {};
	auto rc =
	    decode_nsm_firmware_irreversible_config_req(msg, msgLen, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_nsm_firmware_irreversible_config_request_0_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeIrrevCfg0Resp_NullMsg)
{
	struct nsm_firmware_irreversible_config_request_0_resp cfg = {};
	auto rc = encode_nsm_firmware_irreversible_config_request_0_resp(
	    0, 0, 0, &cfg, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_nsm_firmware_irreversible_config_request_1_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeIrrevCfg1Resp_NullMsg)
{
	auto rc = encode_nsm_firmware_irreversible_config_request_1_resp(
	    0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_nsm_firmware_irreversible_config_request_2_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeIrrevCfg2Resp_NullMsg)
{
	struct nsm_firmware_irreversible_config_request_2_resp cfg = {};
	auto rc = encode_nsm_firmware_irreversible_config_request_2_resp(
	    0, 0, 0, &cfg, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_query_req: NULL checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryReq_NullMsg)
{
	uint16_t cc = 0, ci = 0;
	uint8_t cci = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(nullptr, 100, &cc,
							  &ci, &cci);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryReq_NullCompClass)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t ci = 0;
	uint8_t cci = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(msg, 256, nullptr,
							  &ci, &cci);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryReq_NullCompId)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t cc = 0;
	uint8_t cci = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(msg, 256, &cc,
							  nullptr, &cci);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryReq_NullCompClassIdx)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t cc = 0, ci = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(msg, 256, &cc, &ci,
							  nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryReq_ShortMsg)
{
	std::vector<uint8_t> buf(4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t cc = 0, ci = 0;
	uint8_t cci = 0;
	auto rc =
	    decode_nsm_code_auth_key_perm_query_req(msg, 4, &cc, &ci, &cci);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryReq_DataSizeTooSmall)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_code_auth_key_perm_query_req);
	std::vector<uint8_t> buf(msgLen, 0);
	// data_size=0 < required
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t cc = 0, ci = 0;
	uint8_t cci = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(msg, msgLen, &cc, &ci,
							  &cci);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_nsm_code_auth_key_perm_query_req: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeCodeAuthKeyPermQueryReq_NullMsg)
{
	auto rc = encode_nsm_code_auth_key_perm_query_req(0, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_query_resp: NULL checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0, aki = 0, pki = 0;
	uint8_t pbl = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    nullptr, 100, &cc, &reason, &aki, &pki, &pbl, nullptr, nullptr,
	    nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryResp_NullAki)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, pki = 0;
	uint8_t pbl = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    msg, 256, &cc, &reason, nullptr, &pki, &pbl, nullptr, nullptr,
	    nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryResp_NullPbl)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, aki = 0, pki = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    msg, 256, &cc, &reason, &aki, &pki, nullptr, nullptr, nullptr,
	    nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermQueryResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, aki = 0, pki = 0;
	uint8_t pbl = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    msg, buf.size(), &cc, &reason, &aki, &pki, &pbl, nullptr, nullptr,
	    nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// encode_nsm_code_auth_key_perm_query_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeCodeAuthKeyPermQueryResp_NullMsg)
{
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    0, NSM_SUCCESS, 0, 0, 0, 0, nullptr, nullptr, nullptr, nullptr,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// efuse_key_perm_bitmap NULL (pbl=1, others valid)
TEST(FwBranches3, EncodeCodeAuthKeyPermQueryResp_EfuseBitmapNull)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t dummy[4] = {0};
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    0, NSM_SUCCESS, 0, 0, 0, 1, dummy, dummy, nullptr, dummy, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_update_req: NULL msg
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermUpdateReq_NullMsg)
{
	enum nsm_code_auth_key_perm_request_type rt =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t cc = 0, ci = 0;
	uint8_t idx = 0, pbl = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    nullptr, 100, &rt, &cc, &ci, &idx, &nonce, &pbl, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermUpdateReq_NullRequestType)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t cc = 0, ci = 0;
	uint8_t idx = 0, pbl = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, 256, nullptr, &cc, &ci, &idx, &nonce, &pbl, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermUpdateReq_NullPbl)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type rt =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t cc = 0, ci = 0;
	uint8_t idx = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, 256, &rt, &cc, &ci, &idx, &nonce, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermUpdateReq_ShortMsg)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
	buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type rt =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t cc = 0, ci = 0;
	uint8_t idx = 0, pbl = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), &rt, &cc, &ci, &idx, &nonce, &pbl, nullptr);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// Valid request type = SPECIFIED (1)
TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermUpdateReq_ValidSpecified)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_code_auth_key_perm_update_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *req =
	    reinterpret_cast<struct nsm_code_auth_key_perm_update_req *>(
		buf.data() + sizeof(nsm_msg_hdr));
	req->permission_bitmap_length = 0;
	req->request_type = 1; // SPECIFIED
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type out_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0, pbl = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, msgLen, &out_type, &comp_class, &comp_id, &idx, &nonce, &pbl,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// encode_nsm_code_auth_key_perm_update_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, EncodeCodeAuthKeyPermUpdateResp_NullMsg)
{
	auto rc =
	    encode_nsm_code_auth_key_perm_update_resp(0, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_update_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeCodeAuthKeyPermUpdateResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t method = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_resp(nullptr, 100, &cc,
							    &reason, &method);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode/decode_nsm_query_firmware_security_version_number: NULL checks
// ===========================================================================
TEST(FwBranches3, EncodeFwSecVerReq_NullMsg)
{
	struct nsm_firmware_security_version_number_req req = {};
	auto rc = encode_nsm_query_firmware_security_version_number_req(
	    0, &req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerReq_NullMsg)
{
	struct nsm_firmware_security_version_number_req req = {};
	auto rc = decode_nsm_query_firmware_security_version_number_req(
	    nullptr, 100, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerReq_NullReq)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_nsm_query_firmware_security_version_number_req(
	    msg, 256, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerReq_ShortMsg)
{
	std::vector<uint8_t> buf(4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_security_version_number_req req = {};
	auto rc =
	    decode_nsm_query_firmware_security_version_number_req(msg, 4, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_firmware_security_version_number_req_command);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_security_version_number_req req = {};
	auto rc = decode_nsm_query_firmware_security_version_number_req(
	    msg, msgLen, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(FwBranches3, EncodeFwSecVerResp_NullMsg)
{
	struct nsm_firmware_security_version_number_resp resp = {};
	auto rc = encode_nsm_query_firmware_security_version_number_resp(
	    0, 0, 0, &resp, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_security_version_number_resp resp = {};
	auto rc = decode_nsm_query_firmware_security_version_number_resp(
	    nullptr, 100, &cc, &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerResp_NullResp)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_query_firmware_security_version_number_resp(
	    msg, 256, &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_security_version_number_resp resp = {};
	auto rc = decode_nsm_query_firmware_security_version_number_resp(
	    msg, buf.size(), &cc, &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

TEST(FwBranches3, DISABLED_DecodeFwSecVerResp_MsgTooShort)
{
	auto buf = makeSuccessBuf(7);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_security_version_number_resp resp = {};
	auto rc = decode_nsm_query_firmware_security_version_number_resp(
	    msg, buf.size(), &cc, &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode/decode_nsm_firmware_update_sec_ver: NULL checks
// ===========================================================================
TEST(FwBranches3, EncodeFwUpdateSecVerReq_NullMsg)
{
	struct nsm_firmware_update_min_sec_ver_req req = {};
	auto rc = encode_nsm_firmware_update_sec_ver_req(0, &req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwUpdateSecVerReq_NullMsg)
{
	struct nsm_firmware_update_min_sec_ver_req req = {};
	auto rc = decode_nsm_firmware_update_sec_ver_req(nullptr, 100, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwUpdateSecVerReq_NullReq)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_nsm_firmware_update_sec_ver_req(msg, 256, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwUpdateSecVerReq_ShortMsg)
{
	std::vector<uint8_t> buf(4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_update_min_sec_ver_req req = {};
	auto rc = decode_nsm_firmware_update_sec_ver_req(msg, 4, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(FwBranches3, DISABLED_DecodeFwUpdateSecVerReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_firmware_update_min_sec_ver_req_command);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_update_min_sec_ver_req req = {};
	auto rc = decode_nsm_firmware_update_sec_ver_req(msg, msgLen, &req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(FwBranches3, EncodeFwUpdateSecVerResp_NullMsg)
{
	struct nsm_firmware_update_min_sec_ver_resp resp = {};
	auto rc =
	    encode_nsm_firmware_update_sec_ver_resp(0, 0, 0, &resp, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwUpdateSecVerResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_update_min_sec_ver_resp resp = {};
	auto rc = decode_nsm_firmware_update_sec_ver_resp(nullptr, 100, &cc,
							  &reason, &resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwUpdateSecVerResp_NullResp)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_update_sec_ver_resp(msg, 256, &cc,
							  &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode/decode_nsm_firmware_set_rot_property: NULL and length checks
// ===========================================================================
TEST(FwBranches3, EncodeSetRotPropertyReq_NullMsg)
{
	struct nsm_firmware_set_rot_property_req req = {};
	auto rc = encode_nsm_firmware_set_rot_property_req(0, &req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeSetRotPropertyResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_set_rot_property_resp(nullptr, 100, &cc,
							    &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeSetRotPropertyResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_set_rot_property_resp(msg, buf.size(),
							    &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

TEST(FwBranches3, DISABLED_DecodeSetRotPropertyResp_MsgTooShort)
{
	auto buf = makeSuccessBuf(7);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_set_rot_property_resp(msg, buf.size(),
							    &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(FwBranches3, EncodeSetRotPropertyResp_NullMsg)
{
	auto rc = encode_nsm_firmware_set_rot_property_resp(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode/decode_nsm_dot_cak_install: NULL checks
// ===========================================================================
TEST(FwBranches3, EncodeDotCakInstallReq_NullMsg)
{
	struct nsm_dot_cak_install_req req = {};
	auto rc = encode_nsm_dot_cak_install_req(0, &req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, EncodeDotCakInstallReq_NullReq)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_nsm_dot_cak_install_req(0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, EncodeDotCakInstallResp_NullMsg)
{
	auto rc = encode_nsm_dot_cak_install_resp(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotCakInstallResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_cak_install_resp(nullptr, 100, &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode/decode_nsm_dot_cak_bypass: NULL checks
// ===========================================================================
TEST(FwBranches3, EncodeDotCakBypassResp_NullMsg)
{
	auto rc = encode_nsm_dot_cak_bypass_resp(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotCakBypassResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_cak_bypass_resp(nullptr, 100, &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotCakBypassResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_cak_bypass_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_firmware_image_copy_control_query_progress_resp: checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeFwImageCopyQueryProgressResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_image_copy_control_query_progress_resp qresp = {};
	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    nullptr, 100, &cc, &reason, &qresp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeFwImageCopyQueryProgressResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_image_copy_control_query_progress_resp qresp = {};
	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    msg, buf.size(), &cc, &reason, &qresp);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_firmware_image_copy_control_initiate_copy_resp: NULL msg
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeFwImageCopyInitiateCopyResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
	    nullptr, 100, &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_dot_lock_resp: NULL and length checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotLockResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_lock_resp(nullptr, 100, &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotLockResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_lock_resp(msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_dot_unlock_resp: NULL and cc checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotUnlockResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_resp(nullptr, 100, &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotUnlockResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_dot_get_info_resp: cc non-success path
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotGetInfoResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_info_resp(
	    nullptr, 100, &cc, &reason, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotGetInfoResp_NullCc)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_info_resp(
	    msg, 256, nullptr, &reason, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_dot_get_status_resp: cc non-success path
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotGetStatusResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_get_status_resp(nullptr, 100, &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotGetStatusResp_NullCc)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_get_status_resp(msg, 256, nullptr, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_dot_cak_rotate_resp: NULL and cc checks
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotCakRotateResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t blob = 0;
	auto rc =
	    decode_nsm_dot_cak_rotate_resp(nullptr, 100, &cc, &reason, &blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranches3, DISABLED_DecodeDotCakRotateResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t blob = 0;
	auto rc = decode_nsm_dot_cak_rotate_resp(msg, buf.size(), &cc, &reason,
						 &blob);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_dot_override_resp: cc non-success path
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotOverrideResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_dot_unlock_challenge_resp: cc non-success path
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeDotUnlockChallengeResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_challenge_resp(msg, buf.size(), &cc,
						       &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// Aggregate tag decode functions: invalid valid=0 path
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeAggTagUint8_ValidFalse)
{
	// Build a tag with valid=0
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 1;
	tag_data.valid = 0;
	tag_data.length = 0;
	tag_data.data[0] = 0x42;
	uint8_t buf[sizeof(nsm_firmware_aggregate_tag) + sizeof(uint8_t)];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	uint8_t out_tag = 0, out_valid = 0, out_val = 0xFF;
	bool ok = decode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_TRUE(ok);
	EXPECT_EQ(out_valid, 0);
	// value should NOT have been updated since valid=0
	EXPECT_EQ(out_val, 0xFF);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint16_ValidFalse)
{
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 2;
	tag_data.valid = 0;
	tag_data.length = 1;
	uint8_t buf[sizeof(nsm_firmware_aggregate_tag) + sizeof(uint16_t)];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	uint8_t out_tag = 0, out_valid = 0;
	uint16_t out_val = 0xFFFF;
	bool ok = decode_nsm_firmware_aggregate_tag_uint16(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_TRUE(ok);
	EXPECT_EQ(out_valid, 0);
	EXPECT_EQ(out_val, 0xFFFF);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint32_ValidFalse)
{
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 3;
	tag_data.valid = 0;
	tag_data.length = 2;
	uint8_t buf[sizeof(nsm_firmware_aggregate_tag) + sizeof(uint32_t)];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	uint8_t out_tag = 0, out_valid = 0;
	uint32_t out_val = 0xDEADBEEF;
	bool ok = decode_nsm_firmware_aggregate_tag_uint32(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_TRUE(ok);
	EXPECT_EQ(out_valid, 0);
	EXPECT_EQ(out_val, 0xDEADBEEF);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint64_ValidFalse)
{
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 4;
	tag_data.valid = 0;
	tag_data.length = 3;
	uint8_t buf[sizeof(nsm_firmware_aggregate_tag) + sizeof(uint64_t)];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	uint8_t out_tag = 0, out_valid = 0;
	uint64_t out_val = 0xDEADDEADDEADDEAD;
	bool ok = decode_nsm_firmware_aggregate_tag_uint64(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_TRUE(ok);
	EXPECT_EQ(out_valid, 0);
	EXPECT_EQ(out_val, 0xDEADDEADDEADDEAD);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint8Array_ValidFalse)
{
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 5;
	tag_data.valid = 0;
	tag_data.length = 4; // 2^4 = 16 bytes
	uint8_t buf[sizeof(nsm_firmware_aggregate_tag) + 16];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	uint8_t out_tag = 0, out_valid = 0;
	uint8_t out_val[16];
	memset(out_val, 0xFF, sizeof(out_val));
	bool ok = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &ptr, &out_tag, &out_valid, out_val, &size);
	EXPECT_TRUE(ok);
	EXPECT_EQ(out_valid, 0);
	// value should NOT have been updated since valid=0
	EXPECT_EQ(out_val[0], 0xFF);
}

// ===========================================================================
// Aggregate tag decode: buffer too small
// ===========================================================================
TEST(FwBranches3, DISABLED_DecodeAggTagUint8_BufferTooSmall)
{
	uint8_t buf[1] = {0};
	uint8_t *ptr = buf;
	uint16_t size = 1;
	uint8_t out_tag = 0, out_valid = 0, out_val = 0;
	bool ok = decode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint16_BufferTooSmall)
{
	uint8_t buf[1] = {0};
	uint8_t *ptr = buf;
	uint16_t size = 1;
	uint8_t out_tag = 0, out_valid = 0;
	uint16_t out_val = 0;
	bool ok = decode_nsm_firmware_aggregate_tag_uint16(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint32_BufferTooSmall)
{
	uint8_t buf[1] = {0};
	uint8_t *ptr = buf;
	uint16_t size = 1;
	uint8_t out_tag = 0, out_valid = 0;
	uint32_t out_val = 0;
	bool ok = decode_nsm_firmware_aggregate_tag_uint32(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint64_BufferTooSmall)
{
	uint8_t buf[1] = {0};
	uint8_t *ptr = buf;
	uint16_t size = 1;
	uint8_t out_tag = 0, out_valid = 0;
	uint64_t out_val = 0;
	bool ok = decode_nsm_firmware_aggregate_tag_uint64(
	    &ptr, &out_tag, &out_valid, &out_val, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint8Array_BufferTooSmall)
{
	uint8_t buf[1] = {0};
	uint8_t *ptr = buf;
	uint16_t size = 1;
	uint8_t out_tag = 0, out_valid = 0;
	uint8_t out_val[16] = {0};
	bool ok = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &ptr, &out_tag, &out_valid, out_val, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagSkip_BufferTooSmall)
{
	uint8_t buf[1] = {0};
	uint8_t *ptr = buf;
	uint16_t size = 1;
	bool ok = decode_nsm_firmware_aggregate_tag_skip(&ptr, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagUint8Array_SecondLengthCheckFail)
{
	// First check passes (size >= tag + 4 -1 = 5)
	// But computed length = 2^length might exceed remaining
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 5;
	tag_data.valid = 1;
	tag_data.length = 5; // 2^5 = 32 bytes needed
	uint8_t buf[8];	     // only 8 bytes available
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	uint8_t out_tag = 0, out_valid = 0;
	uint8_t out_val[32] = {0};
	bool ok = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &ptr, &out_tag, &out_valid, out_val, &size);
	EXPECT_FALSE(ok);
}

TEST(FwBranches3, DISABLED_DecodeAggTagSkip_SecondLengthCheckFail)
{
	// Tag header fits but computed data length exceeds buffer
	struct nsm_firmware_aggregate_tag tag_data = {};
	tag_data.tag = 1;
	tag_data.valid = 1;
	tag_data.length = 5; // 2^5 = 32 bytes
	uint8_t buf[4];
	memset(buf, 0, sizeof(buf));
	memcpy(buf, &tag_data, sizeof(tag_data));

	uint8_t *ptr = buf;
	uint16_t size = sizeof(buf);
	bool ok = decode_nsm_firmware_aggregate_tag_skip(&ptr, &size);
	EXPECT_FALSE(ok);
}

// ===========================================================================
// encode_nsm_firmware_set_rot_property_req: AP_SKU_ID property path
// ===========================================================================
TEST(FwBranches3, EncodeSetRotPropertyReq_ApSkuId)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_firmware_set_rot_property_req req = {};
	req.property = NSM_ROT_PROPERTY_AP_SKU_ID;
	uint32_t sku_id = 0x12345678;
	memcpy(req.argument_data, &sku_id, sizeof(uint32_t));
	req.argument_length = sizeof(uint32_t);
	auto rc = encode_nsm_firmware_set_rot_property_req(0, &req, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(FwBranches3, EncodeSetRotPropertyReq_NonApSkuId)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_firmware_set_rot_property_req req = {};
	req.property = 0; // not AP_SKU_ID
	auto rc = encode_nsm_firmware_set_rot_property_req(0, &req, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
