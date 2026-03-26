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
 * Branch coverage for non-DOT functions in libnsm/firmware-utils.c
 *
 * Covers secondary null conditions, data_size check branches, and
 * compound condition paths not exercised by existing tests.
 */

#include "firmware-utils.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t BAD_IIDX = 32; // > NSM_INSTANCE_MAX (31)

static std::vector<uint8_t> g_buf(4096, 0);
static nsm_msg *fwBrMsg() { return reinterpret_cast<nsm_msg *>(g_buf.data()); }

// ===========================================================================
// decode_nsm_firmware_irreversible_config_req — L916
// Needs a buffer with correct total length but data_size=0 < sizeof(*fw_req)
// ===========================================================================
TEST(FwBranchCoverage, DecodeIrreversibleConfigReq_DataSizeTooSmall)
{
	// Build a buffer large enough to pass the length check but with
	// data_size=0 to trigger the L916 check.
	// Must include nsm_common_req hdr (2 bytes) + req body (1 byte) to
	// avoid out-of-bounds read when the function accesses
	// request->hdr.data_size.
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_firmware_irreversible_config_req_command),
	    0);
	// hdr.data_size = 0 (default zero-fill) → triggers L916 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_irreversible_config_req fw_req = {};
	auto rc = decode_nsm_firmware_irreversible_config_req(msg, buf.size(),
							      &fw_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_query_req — L1136-1141
// Secondary null conditions
// ===========================================================================
TEST(FwBranchCoverage,
     DecodeCodeAuthKeyPermQueryReq_NullComponentClassification)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t comp_id = 0;
	uint8_t idx = 0;
	// component_classification=NULL triggers second condition (L1136)
	auto rc = decode_nsm_code_auth_key_perm_query_req(
	    msg, buf.size(), nullptr, &comp_id, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranchCoverage, DecodeCodeAuthKeyPermQueryReq_NullComponentIdentifier)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t comp_class = 0;
	uint8_t idx = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(
	    msg, buf.size(), &comp_class, nullptr, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_query_req — L1148
// data_size < required → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(FwBranchCoverage, DecodeCodeAuthKeyPermQueryReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_code_auth_key_perm_query_req),
	    0);
	// hdr.data_size = 0 → L1148 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(
	    msg, buf.size(), &comp_class, &comp_id, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_query_resp — L1200-1202
// Secondary null conditions
// ===========================================================================
TEST(FwBranchCoverage,
     DecodeCodeAuthKeyPermQueryResp_NullActiveComponentKeyIndex)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, pending = 0;
	uint8_t pbl = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    msg, buf.size(), &cc, &reason, nullptr, &pending, &pbl, nullptr,
	    nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranchCoverage,
     DecodeCodeAuthKeyPermQueryResp_NullPermissionBitmapLength)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, active = 0, pending = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    msg, buf.size(), &cc, &reason, &active, &pending, nullptr, nullptr,
	    nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_nsm_code_auth_key_perm_query_resp — L1265-1269
// Compound null: permission_bitmap_length>0 with valid bitmap pointers
// (covers the FALSE branch of the compound condition → proceeds to pack)
// ===========================================================================
TEST(FwBranchCoverage, EncodeCodeAuthKeyPermQueryResp_NonZeroBitmapValidPtrs)
{
	uint8_t bm[4] = {0};
	// permission_bitmap_length=1, all bitmaps non-NULL → condition FALSE
	// BAD_IIDX=32 causes pack_nsm_header to fail (covering the FALSE path
	// of L1265 with all non-null valid pointers)
	auto rc = encode_nsm_code_auth_key_perm_query_resp(
	    BAD_IIDX, NSM_SUCCESS, 0, 0, 0, 1, bm, bm, bm, bm, fwBrMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_update_req — L1322-1337
// Secondary null conditions + validation checks
// ===========================================================================
TEST(FwBranchCoverage, DecodeCodeAuthKeyPermUpdateReq_NullRequestType)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0;
	uint64_t nonce = 0;
	uint8_t pbl = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), nullptr, &comp_class, &comp_id, &idx, &nonce, &pbl,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(FwBranchCoverage,
     DecodeCodeAuthKeyPermUpdateReq_NullPermissionBitmapLength)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_code_auth_key_perm_request_type req_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0;
	uint64_t nonce = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_req(
	    msg, buf.size(), &req_type, &comp_class, &comp_id, &idx, &nonce,
	    nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_update_resp — L1423
// update_method=NULL (secondary null condition)
// ===========================================================================
TEST(FwBranchCoverage, DecodeCodeAuthKeyPermUpdateResp_NullUpdateMethod)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_resp(
	    msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_nsm_firmware_update_sec_ver_req — L1659
// data_size < sizeof(*fw_req) → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(FwBranchCoverage, DecodeFwUpdateSecVerReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_firmware_update_min_sec_ver_req),
	    0);
	// hdr.data_size = 0 → L1659 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_update_min_sec_ver_req fw_req = {};
	auto rc =
	    decode_nsm_firmware_update_sec_ver_req(msg, buf.size(), &fw_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_query_firmware_security_version_number_req — L1532
// data_size < sizeof(*fw_req) → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(FwBranchCoverage, DecodeQueryFwSecVerNumReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_firmware_security_version_number_req_command),
	    0);
	// hdr.data_size = 0 → L1532 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_firmware_security_version_number_req fw_req = {};
	auto rc = decode_nsm_query_firmware_security_version_number_req(
	    msg, buf.size(), &fw_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_nsm_firmware_image_copy_control_req — L2045
// component_count>0 + valid component_entries → FALSE branch (proceeds to pack)
// ===========================================================================
TEST(FwBranchCoverage, EncodeFwImageCopyControlReq_ValidEntries_PackFail)
{
	struct nsm_firmware_image_copy_control_req fw_req = {};
	fw_req.component_count = 1; // > 0
	struct nsm_firmware_image_copy_component_entry entry = {};
	// condition FALSE (entries non-NULL) → proceeds to pack with BAD_IIDX
	auto rc = encode_nsm_firmware_image_copy_control_req(BAD_IIDX, &fw_req,
							     &entry, fwBrMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_nsm_query_firmware_header_information — L556, L571, L585
// if (rc_ok) { (*telemetry_count)-- } for tags:
//   17 = NSM_FIRMWARE_INBAND_UPDATE_POLICY_CURRENT
//   18 = NSM_FIRMWARE_BACKGROUND_COPY_POLICY_CURRENT
//   19 = NSM_FIRMWARE_AP_SKU_ID
// Build a raw response containing only these 3 tags (telemetry_count=3),
// no SLOT_COUNT tag → loop exits when count reaches 0.
// Called via decode_nsm_query_get_erot_state_parameters_resp.
// ===========================================================================
TEST(FwBranchCoverage, DecodeErotHdrInfo_NewTags_AllDecoded)
{
	// clang-format off
	std::vector<uint8_t> buf{
	    // nsm_msg_hdr (5 bytes)
	    0x10, 0xDE, 0x00, 0x81, NSM_TYPE_FIRMWARE,
	    // nsm_common_telemetry_resp (4 bytes):
	    //   command, completion_code=0, telemetry_count=3 (LE)
	    NSM_FW_GET_EROT_STATE_INFORMATION, 0x00, 0x03, 0x00,
	    // tag 17 = NSM_FIRMWARE_INBAND_UPDATE_POLICY_CURRENT
	    // format: tag(1) | valid/len(1) | data(1) = 3 bytes
	    0x11, 0x01, 0x07,
	    // tag 18 = NSM_FIRMWARE_BACKGROUND_COPY_POLICY_CURRENT
	    0x12, 0x01, 0x02,
	    // tag 19 = NSM_FIRMWARE_AP_SKU_ID (uint32 = 6 bytes)
	    // format: tag(1) | valid/len(1) | data[0:3](4) = 6 bytes
	    0x13, 0x01, 0xAB, 0x00, 0x00, 0x00,
	};
	// clang-format on
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc = 0xFF;
	uint16_t reason_code = 0xFFFF;
	struct nsm_firmware_erot_state_info_resp fw_resp = {};

	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, buf.size(), &cc, &reason_code, &fw_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	// Verify decoded values
	EXPECT_EQ(fw_resp.fq_resp_hdr.inband_update_policy_current, 0x07);
	EXPECT_EQ(fw_resp.fq_resp_hdr.background_copy_policy_current, 0x02);
	EXPECT_EQ(fw_resp.fq_resp_hdr.ap_sku_id, 0x000000ABU);
	// No slots since NSM_FIRMWARE_FIRMWARE_SLOT_COUNT was not included
	EXPECT_EQ(fw_resp.slot_info, nullptr);
}

// ===========================================================================
// decode_nsm_query_get_erot_state_parameters_resp — L839 TRUE
// Header decode fails: malformed aggregate tag with length=3 (needs 8 data
// bytes) but buffer only has 2 bytes → decode_nsm_firmware_aggregate_tag_skip
// returns false (L263 TRUE) → returns NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(FwBranchCoverage, DecodeErotStateResp_HeaderDecodeFail)
{
	// sizeof(nsm_firmware_aggregate_tag) = 3 (tag+valid/len+data[1])
	// while loop needs payload_size >= 3; use 3-byte tag payload so loop
	// enters, then skip fails: consumed_len=3+(1<<3)-1=10 > 3 → false
	// clang-format off
	std::vector<uint8_t> buf{
	    // nsm_msg_hdr (5 bytes)
	    0x10, 0xDE, 0x00, 0x81, NSM_TYPE_FIRMWARE,
	    // nsm_common_telemetry_resp (4 bytes):
	    //   command, completion_code=0, telemetry_count=1 (LE)
	    NSM_FW_GET_EROT_STATE_INFORMATION, 0x00, 0x01, 0x00,
	    // Tag payload (3 bytes = payload_size so loop enters):
	    //   tag=0xFF (unknown), valid/length byte=0x06 (length=3→1<<3=8),
	    //   data[0]=0x00; consumed_len=3+8-1=10 > 3 → skip fails (L263)
	    0xFF, 0x06, 0x00,
	};
	// clang-format on
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());

	uint8_t cc = 0xFF;
	uint16_t reason_code = 0;
	struct nsm_firmware_erot_state_info_resp fw_resp = {};

	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, buf.size(), &cc, &reason_code, &fw_resp);

	EXPECT_NE(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS); // cc was set before decode failure
}

// ===========================================================================
// decode_nsm_code_auth_key_perm_query_req — L1141 TRUE (msg_len too small)
// msg_len < sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_req)
// = 5+7=12; use 5-byte buffer
// ===========================================================================
TEST(FwBranchCoverage, DecodeCodeAuthKeyPermQueryReq_MsgLenTooSmall)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0); // only 5 bytes
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t comp_class = 0, comp_id = 0;
	uint8_t idx = 0;
	auto rc = decode_nsm_code_auth_key_perm_query_req(
	    msg, buf.size(), &comp_class, &comp_id, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L616-622: decode_nsm_query_firmware_header_information
// "if (!rc_ok)" TRUE branch after a known tag's decode fails.
// Use tag=NSM_FIRMWARE_AP_SKU_ID(=19=0x13, uint32) with only 3 bytes
// available: decode_uint32 needs 6 bytes (3+4-1), 3 < 6 → rc_ok=false.
// Called via decode_nsm_query_get_erot_state_parameters_resp.
// ===========================================================================
TEST(FwBranchCoverage, DecodeErotHdrInfo_KnownTagDecodeFail)
{
	// clang-format off
	std::vector<uint8_t> buf{
	    // nsm_msg_hdr (5 bytes)
	    0x10, 0xDE, 0x00, 0x81, NSM_TYPE_FIRMWARE,
	    // nsm_common_telemetry_resp (4 bytes): command=1, cc=0, count=1
	    NSM_FW_GET_EROT_STATE_INFORMATION, 0x00, 0x01, 0x00,
	    // tag=19=0x13 (NSM_FIRMWARE_AP_SKU_ID, uint32), valid/len=0x01,
	    // data[0]=0x00 — only 3 bytes; decode_uint32 needs 6 → fails
	    0x13, 0x01, 0x00,
	};
	// clang-format on
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0xFF;
	uint16_t reason_code = 0;
	struct nsm_firmware_erot_state_info_resp fw_resp = {};

	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, buf.size(), &cc, &reason_code, &fw_resp);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L781-788: decode_nsm_query_firmware_slot_information
// Unknown tag in slot section where skip fails (large consumed_len).
// Header section: tag=5 (slot_count=1, 3 bytes) → break exits header loop.
// Slot section: tag=6 (slot_id, 3 bytes) + tag=0xFF (skip fails, 3 bytes).
// skip: consumed_len = 3 + (1<<3) - 1 = 10 > 3 → returns false.
// ===========================================================================
TEST(FwBranchCoverage, DecodeErotSlotInfo_UnknownTagSkipFail)
{
	// clang-format off
	std::vector<uint8_t> buf{
	    // nsm_msg_hdr (5 bytes)
	    0x10, 0xDE, 0x00, 0x81, NSM_TYPE_FIRMWARE,
	    // nsm_common_telemetry_resp (4 bytes): command=1, cc=0, count=4
	    NSM_FW_GET_EROT_STATE_INFORMATION, 0x00, 0x04, 0x00,
	    // tag=5 (NSM_FIRMWARE_FIRMWARE_SLOT_COUNT), valid=1, count=1
	    0x05, 0x01, 0x01,
	    // tag=6 (NSM_FIRMWARE_FIRMWARE_SLOT_ID), valid=1, id=0
	    0x06, 0x01, 0x00,
	    // tag=0xFF (unknown), valid/length=0x07 (valid=1, length=3)
	    // data_len = 1<<3 = 8; consumed_len = 3+8-1 = 10 > 3 → skip fails
	    0xFF, 0x07, 0x00,
	};
	// clang-format on
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0xFF;
	uint16_t reason_code = 0;
	struct nsm_firmware_erot_state_info_resp fw_resp = {};

	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    msg, buf.size(), &cc, &reason_code, &fw_resp);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
	free(fw_resp.slot_info);
}

// ===========================================================================
// L1094: decode_nsm_firmware_irreversible_config_request_1_resp
// "if (msg_len < sizeof(nsm_msg_hdr))" TRUE branch.
// decode_reason_code_and_cc reads payload[1] and returns NSM_SW_SUCCESS
// when cc=NSM_SUCCESS (payload[1]=0). Buffer is 7 bytes for safe access;
// pass msg_len=4 < sizeof(nsm_msg_hdr)=5 → L1094 TRUE.
// ===========================================================================
TEST(FwBranchCoverage, DecodeIrrevConfigReq1Resp_ShortMsgLen)
{
	// 7 bytes: nsm_hdr(5) + payload[0](1) + payload[1]=cc(1)
	// payload[1]=0 = NSM_SUCCESS → decode_reason_code_and_cc returns OK
	std::vector<uint8_t> buf(7, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0xFF;
	uint16_t reason = 0;
	// msg_len=4 < sizeof(nsm_msg_hdr)=5 → L1094 TRUE
	auto rc = decode_nsm_firmware_irreversible_config_request_1_resp(
	    msg, 4, &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
