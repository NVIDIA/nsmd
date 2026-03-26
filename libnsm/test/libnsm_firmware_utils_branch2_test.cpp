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
 * Branch coverage batch 2 for libnsm/firmware-utils.c
 *
 * Targets (all using zero-filled or errCcBuf patterns; no source changes):
 *  L1106  decode_nsm_firmware_irreversible_config_request_1_resp: *cc != OK
 *  L1127  decode_nsm_firmware_irreversible_config_request_2_resp: *cc != OK
 *  L1282  encode_nsm_code_auth_key_perm_query_resp: pbl>0 + NULL bitmap
 *  L1284  same: pending_component_key_perm_bitmap == NULL
 *  L1286  same: pending_efuse_key_perm_bitmap == NULL
 *  L1340  decode_nsm_code_auth_key_perm_update_req: component_classification
 * NULL L1341  decode_nsm_code_auth_key_perm_update_req: comp_idx/nonce NULL
 *  L1351  decode_nsm_code_auth_key_perm_update_req: bitmap_length mismatch
 *  L1354  decode_nsm_code_auth_key_perm_update_req: invalid request_type
 *  L1445  decode_nsm_code_auth_key_perm_update_resp: *cc != OK
 *  L1455  decode_nsm_code_auth_key_perm_update_resp: data_size < required
 *  L1739  decode_nsm_firmware_update_sec_ver_resp: msg_len < sizeof(resp)
 *  L1948  decode_nsm_dot_cak_install_resp: *cc != OK
 *  L2156  decode_nsm_firmware_image_copy_control_initiate_copy_resp: short len
 *  L2161  decode_nsm_firmware_image_copy_control_initiate_copy_resp: *cc != OK
 *  L2401  decode_nsm_dot_unlock_challenge_resp: msg_len <
 * sizeof(challenge_resp) L2608  decode_nsm_dot_get_info_resp: msg_len <
 * sizeof(nsm_common_resp) L2614  decode_nsm_dot_get_info_resp: *cc != OK L2621
 * decode_nsm_dot_get_info_resp: msg_len < sizeof(nsm_dot_get_info_resp) L2724
 * decode_nsm_dot_get_status_resp: msg_len < sizeof(nsm_common_resp) L2730
 * decode_nsm_dot_get_status_resp: *cc != OK L2737
 * decode_nsm_dot_get_status_resp: msg_len < sizeof(nsm_dot_get_status_resp)
 *  L2967  decode_nsm_dot_override_resp: msg_len < sizeof(nsm_common_resp)
 *  L3077  decode_nsm_dot_disable_resp: *cc != OK
 *  L3189  decode_nsm_dot_recovery_resp: *cc != OK
 *  L3193  decode_nsm_dot_recovery_resp: msg_len < sizeof(nsm_common_resp)
 */

#include "firmware-utils.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// Exact-size buffer for decode_reason_code_and_cc with non-success cc.
// base.c checks msg_len == sizeof(nsm_msg_hdr) +
// sizeof(nsm_common_non_success_resp) = 5 + 4 = 9 exactly. payload[1] = cc (set
// to 0xFF = non-success).
static std::vector<uint8_t> makeErrCcBuf()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // cc = non-success
	return buf;
}

// ===========================================================================
// L1106 TRUE: decode_nsm_firmware_irreversible_config_request_1_resp
// rc == NSM_SW_SUCCESS but *cc != NSM_SUCCESS → return rc (=NSM_SW_SUCCESS)
// ===========================================================================
TEST(FwBranches2, DecodeIrrevCfgReq1Resp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_irreversible_config_request_1_resp(
	    msg, buf.size(), &cc, &reason);
	// rc = NSM_SW_SUCCESS (decode_reason_code_and_cc returned OK)
	// cc = 0xFF (non-success), function returns rc early at L1106
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L1127 TRUE: decode_nsm_firmware_irreversible_config_request_2_resp
// rc == NSM_SW_SUCCESS but *cc != NSM_SUCCESS
// ===========================================================================
TEST(FwBranches2, DecodeIrrevCfgReq2Resp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_irreversible_config_request_2_resp cfg_resp = {};
	auto rc = decode_nsm_firmware_irreversible_config_request_2_resp(
	    msg, buf.size(), &cc, &reason, &cfg_resp);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L1282 TRUE (inner): encode_nsm_code_auth_key_perm_query_resp
// permission_bitmap_length=1 but active_component_key_perm_bitmap=NULL
// → inner condition TRUE → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(FwBranches2, EncodeCodeAuthKeyPermQueryResp_ActiveBitmapNull)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t dummy[4] = {0};
	// pbl=1, active=NULL → hits L1282 inner TRUE (active_bitmap==NULL)
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    0, NSM_SUCCESS, 0, 0, 0, 1, nullptr, dummy, dummy, dummy, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1284 TRUE: encode_nsm_code_auth_key_perm_query_resp
// pbl=1, active!=NULL, pending_component_key_perm_bitmap=NULL
// ===========================================================================
TEST(FwBranches2, EncodeCodeAuthKeyPermQueryResp_PendingBitmapNull)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t dummy[4] = {0};
	// pbl=1, active=valid, pending=NULL → L1284 TRUE
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    0, NSM_SUCCESS, 0, 0, 0, 1, dummy, nullptr, dummy, dummy, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1286 TRUE: encode_nsm_code_auth_key_perm_query_resp
// pbl=1, active!=NULL, pending!=NULL, efuse!=NULL, pending_efuse=NULL
// ===========================================================================
TEST(FwBranches2, EncodeCodeAuthKeyPermQueryResp_PendingEfuseBitmapNull)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t dummy[4] = {0};
	// pbl=1, active=valid, pending=valid, efuse=valid, pending_efuse=NULL
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    0, NSM_SUCCESS, 0, 0, 0, 1, dummy, dummy, dummy, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1340 TRUE: decode_nsm_code_auth_key_perm_update_req
// msg!=NULL, request_type!=NULL, component_classification=NULL
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateReq_NullCompClass)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type req_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_id = 0;
	uint8_t idx = 0;
	uint64_t nonce = 0;
	uint8_t pbl = 0;
	// component_classification=NULL → L1340 TRUE
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), &req_type, nullptr, &comp_id, &idx, &nonce, &pbl,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1340 TRUE (2nd sub-expr): component_identifier=NULL
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateReq_NullCompId)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type req_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0;
	uint8_t idx = 0;
	uint64_t nonce = 0;
	uint8_t pbl = 0;
	// comp_class!=NULL, comp_id=NULL → L1340 2nd condition TRUE
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), &req_type, &comp_class, nullptr, &idx, &nonce,
	    &pbl, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1341 TRUE: component_classification_index=NULL
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateReq_NullCompIdx)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type req_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0, comp_id = 0;
	uint64_t nonce = 0;
	uint8_t pbl = 0;
	// comp_class!=NULL, comp_id!=NULL, comp_idx=NULL → L1341 TRUE
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), &req_type, &comp_class, &comp_id, nullptr, &nonce,
	    &pbl, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1341 TRUE (2nd): nonce=NULL
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateReq_NullNonce)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type req_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0, pbl = 0;
	// comp_idx!=NULL, nonce=NULL → L1341 nonce condition TRUE
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), &req_type, &comp_class, &comp_id, &idx, nullptr,
	    &pbl, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1351 TRUE: decode_nsm_code_auth_key_perm_update_req
// expected_bitmap_length (=1 extra byte) != req->permission_bitmap_length(=0)
// No unpack_nsm_header call; zero-filled buffer is fine.
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateReq_BitmapLenMismatch)
{
	// 1 extra byte → expected_bitmap_length=1; req->pbl=0 → mismatch
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_code_auth_key_perm_update_req) +
			      1;
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type req_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0, pbl = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, msgLen, &req_type, &comp_class, &comp_id, &idx, &nonce, &pbl,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1354 TRUE: decode_nsm_code_auth_key_perm_update_req
// request_type not MOST_RESTRICTIVE(0) or SPECIFIED(1) → NSM_SW_ERROR_DATA
// expected_bitmap_length=0, req->pbl=0 → L1351 passes;
// req->request_type=5 → L1354 TRUE
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateReq_InvalidRequestType)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_code_auth_key_perm_update_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *req =
	    reinterpret_cast<struct nsm_code_auth_key_perm_update_req *>(
		buf.data() + sizeof(nsm_msg_hdr));
	req->permission_bitmap_length = 0; // matches expected=0
	req->request_type = 5;		   // invalid (not 0 or 1)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type out_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0, pbl = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, msgLen, &out_type, &comp_class, &comp_id, &idx, &nonce, &pbl,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1445 TRUE: decode_nsm_code_auth_key_perm_update_resp
// rc==NSM_SW_SUCCESS, *cc != NSM_SUCCESS
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t method = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_resp(
	    msg, buf.size(), &cc, &reason, &method);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L1455 TRUE: decode_nsm_code_auth_key_perm_update_resp
// msg_len OK, cc=NSM_SUCCESS, but resp->hdr.data_size=0 < required (4)
// sizeof(nsm_code_auth_key_perm_update_resp) = 10;
// NSM_RESPONSE_CONVENTION_LEN=6 Required data_size = 10-6=4; data_size=0 < 4 →
// NSM_SW_ERROR_DATA
// ===========================================================================
TEST(FwBranches2, DecodeCodeAuthKeyPermUpdateResp_DataSizeTooSmall)
{
	// 15 bytes: nsm_msg_hdr(5) + nsm_code_auth_key_perm_update_resp(10)
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_code_auth_key_perm_update_resp);
	std::vector<uint8_t> buf(msgLen, 0);
	// cc=0 (NSM_SUCCESS), data_size=0 (default) < 4 → L1455 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t method = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_resp(msg, msgLen, &cc,
							    &reason, &method);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1739 TRUE: decode_nsm_firmware_update_sec_ver_resp
// rc==NSM_SW_SUCCESS, cc==NSM_SUCCESS, but msg_len < sizeof(resp)
// Use 7-byte buffer (min for decode_reason_code_and_cc), cc=0
// ===========================================================================
TEST(FwBranches2, DecodeFwUpdateSecVerResp_MsgTooShort)
{
	// 7 bytes: nsm_msg_hdr(5) + payload[0..1]; cc=0 (NSM_SUCCESS)
	std::vector<uint8_t> buf(7, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_firmware_update_min_sec_ver_resp sec_resp = {};
	auto rc = decode_nsm_firmware_update_sec_ver_resp(msg, buf.size(), &cc,
							  &reason, &sec_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1948 TRUE: decode_nsm_dot_cak_install_resp
// rc==NSM_SW_SUCCESS, *cc != NSM_SUCCESS
// ===========================================================================
TEST(FwBranches2, DecodeDotCakInstallResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_cak_install_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L2156 TRUE: decode_nsm_firmware_image_copy_control_initiate_copy_resp
// msg_len < sizeof(nsm_msg_hdr) → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(FwBranches2, DecodeFwImageCopyInitiateCopyResp_ShortHdr)
{
	// 3 bytes < sizeof(nsm_msg_hdr)=5
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
	    msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L2161 TRUE: decode_nsm_firmware_image_copy_control_initiate_copy_resp
// msg_len >= sizeof(nsm_msg_hdr), but *cc != NSM_SUCCESS
// ===========================================================================
TEST(FwBranches2, DecodeFwImageCopyInitiateCopyResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 11 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
	    msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L2401 TRUE: decode_nsm_dot_unlock_challenge_resp
// rc==NSM_SW_SUCCESS, cc==NSM_SUCCESS, msg_len < sizeof(challenge_resp)
// sizeof(nsm_dot_unlock_challenge_resp)=38; use 7-byte buf for safe read
// ===========================================================================
TEST(FwBranches2, DecodeDotUnlockChallengeResp_MsgTooShort)
{
	std::vector<uint8_t> buf(7, 0); // cc=0=NSM_SUCCESS
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_challenge_resp(msg, buf.size(), &cc,
						       &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L2608 TRUE: decode_nsm_dot_get_info_resp
// msg_len < sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) = 11
// Use 3-byte buffer
// ===========================================================================
TEST(FwBranches2, DecodeDotGetInfoResp_MsgTooShortForCommonResp)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_info_resp(
	    msg, buf.size(), &cc, &reason, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L2621 TRUE: decode_nsm_dot_get_info_resp
// msg_len >= 11 (cc ok), *cc==NSM_SUCCESS, but msg_len < sizeof(dot_get_info)
// sizeof(nsm_dot_get_info_resp) = 6+1024+4 = 1034; use 11-byte buf
// ===========================================================================
TEST(FwBranches2, DecodeDotGetInfoResp_MsgTooShortForFullResp)
{
	// 11 bytes: passes L2608 check; cc=0; fails L2621 (11 < 5+1034=1039)
	std::vector<uint8_t> buf(11, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_info_resp(
	    msg, buf.size(), &cc, &reason, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L2724 TRUE: decode_nsm_dot_get_status_resp
// msg_len < sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) = 11
// ===========================================================================
TEST(FwBranches2, DecodeDotGetStatusResp_MsgTooShortForCommonResp)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_status_resp(msg, buf.size(), &cc, &reason,
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L2737 TRUE: decode_nsm_dot_get_status_resp
// msg_len >= 11 (cc ok), *cc==NSM_SUCCESS, msg_len <
// sizeof(nsm_dot_get_status_resp) sizeof(nsm_dot_get_status_resp)=7; need
// 5+7=12; use 11-byte buf < 12
// ===========================================================================
TEST(FwBranches2, DecodeDotGetStatusResp_MsgTooShortForFullResp)
{
	// 11 bytes: passes L2724 (>=11); cc=0; fails L2737 (11 < 5+7=12)
	std::vector<uint8_t> buf(11, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_status_resp(msg, buf.size(), &cc, &reason,
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L2967 TRUE: decode_nsm_dot_override_resp
// rc==NSM_SW_SUCCESS, cc==NSM_SUCCESS, msg_len < sizeof(nsm_common_resp)
// Use 7-byte buf (min for decode_reason_code_and_cc to succeed), cc=0
// ===========================================================================
TEST(FwBranches2, DecodeDotOverrideResp_MsgTooShort)
{
	std::vector<uint8_t> buf(7, 0); // cc=0=NSM_SUCCESS
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L3077 TRUE: decode_nsm_dot_disable_resp
// rc==NSM_SW_SUCCESS, *cc != NSM_SUCCESS
// ===========================================================================
TEST(FwBranches2, DecodeDotDisableResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 11 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_disable_resp(msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L3189 TRUE: decode_nsm_dot_recovery_resp
// rc==NSM_SW_SUCCESS, *cc != NSM_SUCCESS
// ===========================================================================
TEST(FwBranches2, DecodeDotRecoveryResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 11 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_recovery_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L3193 TRUE: decode_nsm_dot_recovery_resp
// rc==NSM_SW_SUCCESS, cc==NSM_SUCCESS, msg_len < sizeof(nsm_common_resp)=11
// Use 7-byte buf (cc=0, min for decode_reason_code_and_cc)
// ===========================================================================
TEST(FwBranches2, DecodeDotRecoveryResp_MsgTooShort)
{
	std::vector<uint8_t> buf(7, 0); // cc=0=NSM_SUCCESS
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_recovery_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
