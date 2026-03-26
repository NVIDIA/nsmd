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
 * Branch coverage for debug-token.c — cc non-success paths, secondary null
 * checks, and data_size validation branches not exercised by the existing
 * libnsm_debug_token_test.cpp.
 */

#include "base.h"
#include "debug-token.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: 9-byte buffer that triggers *cc = 0xFF path in decode functions
// which call decode_reason_code_and_cc directly (no size pre-check).
// decode_reason_code_and_cc: reads *cc = payload[0], if != NSM_SUCCESS/
// NSM_ACCEPTED checks msg_len == 9 exactly; returns NSM_SW_SUCCESS.
// ---------------------------------------------------------------------------
static std::vector<uint8_t>
errCcBuf9(size_t sz = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp))
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code = non-success
	return buf;
}

// ===========================================================================
// decode_nsm_query_token_parameters_req — secondary null + data_size
// if (msg == NULL || token_opcode == NULL)
// ===========================================================================
TEST(DebugTokenBranch, QueryTokenParametersReq_NullTokenOpcode)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	// msg != NULL, token_opcode == NULL → second condition TRUE
	auto rc =
	    decode_nsm_query_token_parameters_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenParametersReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_req), 0);
	// data_size = 0 < sizeof(token_opcode) = 1 → NSM_SW_ERROR_DATA
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_debug_token_opcode opcode = {};
	auto rc =
	    decode_nsm_query_token_parameters_req(msg, buf.size(), &opcode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_query_token_parameters_resp — cc non-success + secondary null
// if (msg == NULL || token_request == NULL) — only msg=NULL in existing tests
// ===========================================================================
TEST(DebugTokenBranch, QueryTokenParametersResp_NullTokenRequest)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	// msg != NULL, token_request == NULL → second condition TRUE
	auto rc = decode_nsm_query_token_parameters_resp(msg, buf.size(), &cc,
							 &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenParametersResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_debug_token_request token_req = {};
	auto rc = decode_nsm_query_token_parameters_resp(msg, buf.size(), &cc,
							 &reason, &token_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_provide_token_req — secondary null conditions + data_size
// if (msg == NULL || token_data == NULL || token_data_len == NULL)
// ===========================================================================
TEST(DebugTokenBranch, ProvideTokenReq_NullTokenData)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_req) + 4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t token_data_len = 0;
	// msg != NULL, token_data == NULL → second condition TRUE
	auto rc = decode_nsm_provide_token_req(msg, buf.size(), nullptr,
					       &token_data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, ProvideTokenReq_NullTokenDataLen)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_req) + 4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t token_data[4] = {};
	// msg != NULL, token_data != NULL, token_data_len == NULL → third TRUE
	auto rc =
	    decode_nsm_provide_token_req(msg, buf.size(), token_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, ProvideTokenReq_DataSizeZero)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_req) + 4, 0);
	// data_size at payload[1] = 0 → NSM_SW_ERROR_DATA
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t token_data[4] = {};
	uint8_t token_data_len = 0;
	auto rc = decode_nsm_provide_token_req(msg, buf.size(), token_data,
					       &token_data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_provide_token_resp — secondary null conditions + cc non-success
// if (msg == NULL || cc == NULL || reason_code == NULL)
// ===========================================================================
TEST(DebugTokenBranch, ProvideTokenResp_NullCc)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	// msg != NULL, cc == NULL → second condition TRUE
	auto rc =
	    decode_nsm_provide_token_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, ProvideTokenResp_NullReasonCode)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	// msg != NULL, cc != NULL, reason_code == NULL → third condition TRUE
	auto rc = decode_nsm_provide_token_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, ProvideTokenResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_provide_token_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_disable_tokens_resp — secondary null conditions + cc non-success
// if (msg == NULL || cc == NULL || reason_code == NULL)
// ===========================================================================
TEST(DebugTokenBranch, DisableTokensResp_NullCc)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_disable_tokens_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, DisableTokensResp_NullReasonCode)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_disable_tokens_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, DisableTokensResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_disable_tokens_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_query_token_status_req — secondary null + data_size
// if (msg == NULL || token_type == NULL)
// ===========================================================================
TEST(DebugTokenBranch, QueryTokenStatusReq_NullTokenType)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_nsm_query_token_status_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenStatusReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req), 0);
	// data_size = 0 < sizeof(token_type) = 1 → NSM_SW_ERROR_DATA
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum nsm_debug_token_type token_type = {};
	auto rc =
	    decode_nsm_query_token_status_req(msg, buf.size(), &token_type);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_query_token_status_resp — secondary null conditions + cc
// if (msg==NULL || status==NULL || additional_info==NULL ||
//     token_type==NULL || time_left==NULL)
// ===========================================================================
TEST(DebugTokenBranch, QueryTokenStatusResp_NullStatus)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status_additional_info add_info = {};
	enum nsm_debug_token_type token_type = {};
	uint32_t time_left = 0;
	// msg != NULL, status == NULL → second condition TRUE
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason, nullptr, &add_info, &token_type,
	    &time_left);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenStatusResp_NullAdditionalInfo)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_type token_type = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(msg, buf.size(), &cc,
						     &reason, &status, nullptr,
						     &token_type, &time_left);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenStatusResp_NullTokenType)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info add_info = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason, &status, &add_info, nullptr,
	    &time_left);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenStatusResp_NullTimeLeft)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info add_info = {};
	enum nsm_debug_token_type token_type = {};
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason, &status, &add_info, &token_type,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenStatusResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info add_info = {};
	enum nsm_debug_token_type token_type = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason, &status, &add_info, &token_type,
	    &time_left);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_query_device_ids_resp — secondary null + cc non-success
// if (msg==NULL || cc==NULL || reason_code==NULL || device_id_len==NULL)
// ===========================================================================
TEST(DebugTokenBranch, QueryDeviceIdsResp_NullDeviceIdLen)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t device_id[16] = {};
	// msg, cc, reason_code all non-NULL; device_id_len == NULL → 4th TRUE
	auto rc = decode_nsm_query_device_ids_resp(msg, buf.size(), &cc,
						   &reason, device_id, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryDeviceIdsResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	size_t device_id_len = 0;
	auto rc = decode_nsm_query_device_ids_resp(
	    msg, buf.size(), &cc, &reason, nullptr, &device_id_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_erase_token_req — data_size mismatch
// data_size = 0 != sizeof(token_type) = 4 → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(DebugTokenBranch, EraseTokenReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_erase_token_req), 0);
	// data_size = 0 != sizeof(uint32_t) = 4 → NSM_SW_ERROR_DATA
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t token_type = 0;
	auto rc = decode_nsm_erase_token_req(msg, buf.size(), &token_type);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_erase_token_resp — secondary null conditions + cc non-success
// if (msg == NULL || cc == NULL || reason_code == NULL)
// ===========================================================================
TEST(DebugTokenBranch, EraseTokenResp_NullCc)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_erase_token_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, EraseTokenResp_NullReasonCode)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_erase_token_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, EraseTokenResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_erase_token_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_nsm_query_token_req — data_size != 0 branch
// nsm_query_token_req is typedef for nsm_common_req; data_size at payload[1]
// ===========================================================================
TEST(DebugTokenBranch, QueryTokenReq_DataSizeNonZero)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_req), 0);
	// data_size = 1 (non-zero) → NSM_SW_ERROR_DATA
	buf[sizeof(nsm_msg_hdr) + 1] = 1;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_nsm_query_token_req(msg, buf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_nsm_query_token_resp — secondary null + cc non-success + data_size==0
// if (msg==NULL || cc==NULL || reason_code==NULL || tlv_payload_len==NULL)
// ===========================================================================
TEST(DebugTokenBranch, QueryTokenResp_NullCc)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	size_t tlv_len = 0;
	auto rc = decode_nsm_query_token_resp(msg, buf.size(), nullptr, &reason,
					      nullptr, &tlv_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenResp_NullReasonCode)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	size_t tlv_len = 0;
	auto rc = decode_nsm_query_token_resp(msg, buf.size(), &cc, nullptr,
					      nullptr, &tlv_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenResp_NullTlvPayloadLen)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	// msg, cc, reason_code non-NULL; tlv_payload_len == NULL → 4th TRUE
	auto rc = decode_nsm_query_token_resp(msg, buf.size(), &cc, &reason,
					      nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenBranch, QueryTokenResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	size_t tlv_len = 0;
	auto rc = decode_nsm_query_token_resp(msg, buf.size(), &cc, &reason,
					      nullptr, &tlv_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}
