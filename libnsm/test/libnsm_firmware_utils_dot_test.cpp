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
 * Branch coverage tests for DOT functions in libnsm/firmware-utils.c
 *
 * Covers:
 * 1. pack_nsm_header_v2 fail branches (instance_id=32 > NSM_INSTANCE_MAX=31)
 * 2. Optional pointer NULL branches (dot_blob=NULL, challenge=NULL, etc.)
 * 3. Secondary null-check conditions (cc=NULL, reason_code=NULL)
 */

#include "firmware-utils.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t BAD_IIDX = 32; // > NSM_INSTANCE_MAX (31)

// Buffer helpers
static std::vector<uint8_t> g_dot_buf(4096, 0);
static nsm_msg *dotMsg()
{
	return reinterpret_cast<nsm_msg *>(g_dot_buf.data());
}

// Success-CC buffer: payload[0]=0=NSM_SUCCESS, big enough for any DOT resp
static std::vector<uint8_t> makeSuccessBuf()
{
	return std::vector<uint8_t>(sizeof(nsm_msg_hdr) + 2048, 0);
}

// ===========================================================================
// Pack-fail tests: pack_nsm_header_v2 fails when instance_id > NSM_INSTANCE_MAX
// ===========================================================================

// encode_nsm_dot_lock_req — L2165
TEST(DotPackFail, EncodeDotLockReq)
{
	struct nsm_dot_lock_req req = {};
	auto rc = encode_nsm_dot_lock_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_lock_resp — L2217
TEST(DotPackFail, EncodeDotLockResp)
{
	auto rc = encode_nsm_dot_lock_resp(BAD_IIDX, NSM_SUCCESS, 0, nullptr,
					   dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_unlock_challenge_req — L2287
TEST(DotPackFail, EncodeDotUnlockChallengeReq)
{
	struct nsm_dot_unlock_challenge_req req = {};
	auto rc = encode_nsm_dot_unlock_challenge_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_unlock_challenge_resp — L2343
TEST(DotPackFail, EncodeDotUnlockChallengeResp)
{
	auto rc = encode_nsm_dot_unlock_challenge_resp(BAD_IIDX, NSM_SUCCESS, 0,
						       nullptr, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_unlock_req — L2413
TEST(DotPackFail, EncodeDotUnlockReq)
{
	struct nsm_dot_unlock_req req = {};
	auto rc = encode_nsm_dot_unlock_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_unlock_resp — L2464
TEST(DotPackFail, EncodeDotUnlockResp)
{
	auto rc =
	    encode_nsm_dot_unlock_resp(BAD_IIDX, NSM_SUCCESS, 0, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_get_info_req — L2512
TEST(DotPackFail, EncodeDotGetInfoReq)
{
	auto rc = encode_nsm_dot_get_info_req(BAD_IIDX, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_get_info_resp — L2555
TEST(DotPackFail, EncodeDotGetInfoResp)
{
	auto rc = encode_nsm_dot_get_info_resp(BAD_IIDX, NSM_SUCCESS, 0, 0, 0,
					       0, nullptr, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_get_status_req — L2637
TEST(DotPackFail, EncodeDotGetStatusReq)
{
	auto rc = encode_nsm_dot_get_status_req(BAD_IIDX, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_get_status_resp — L2679
TEST(DotPackFail, EncodeDotGetStatusResp)
{
	auto rc = encode_nsm_dot_get_status_resp(BAD_IIDX, NSM_SUCCESS, 0, 0,
						 dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_rotate_req — L2747
TEST(DotPackFail, EncodeDotCAKRotateReq)
{
	struct nsm_dot_cak_rotate_req req = {};
	auto rc = encode_nsm_dot_cak_rotate_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_rotate_resp — L2800
TEST(DotPackFail, EncodeDotCAKRotateResp)
{
	auto rc = encode_nsm_dot_cak_rotate_resp(BAD_IIDX, NSM_SUCCESS, 0,
						 nullptr, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_override_req — L2869
TEST(DotPackFail, EncodeDotOverrideReq)
{
	struct nsm_dot_override_req req = {};
	auto rc = encode_nsm_dot_override_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_override_resp — L2920
TEST(DotPackFail, EncodeDotOverrideResp)
{
	auto rc =
	    encode_nsm_dot_override_resp(BAD_IIDX, NSM_SUCCESS, 0, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_disable_req — L2974
TEST(DotPackFail, EncodeDotDisableReq)
{
	struct nsm_dot_disable_req req = {};
	auto rc = encode_nsm_dot_disable_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_disable_resp — L3026
TEST(DotPackFail, EncodeDotDisableResp)
{
	auto rc = encode_nsm_dot_disable_resp(BAD_IIDX, NSM_SUCCESS, 0, nullptr,
					      dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_recovery_req — L3095
TEST(DotPackFail, EncodeDotRecoveryReq)
{
	struct nsm_dot_recovery_req req = {};
	auto rc = encode_nsm_dot_recovery_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_recovery_resp — L3146
TEST(DotPackFail, EncodeDotRecoveryResp)
{
	auto rc =
	    encode_nsm_dot_recovery_resp(BAD_IIDX, NSM_SUCCESS, 0, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_install_req — L1852
TEST(DotPackFail, EncodeDotCAKInstallReq)
{
	struct nsm_dot_cak_install_req req = {};
	auto rc = encode_nsm_dot_cak_install_req(BAD_IIDX, &req, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_install_resp — L1904
TEST(DotPackFail, EncodeDotCAKInstallResp)
{
	auto rc =
	    encode_nsm_dot_cak_install_resp(BAD_IIDX, NSM_SUCCESS, 0, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_bypass_req — L1957
TEST(DotPackFail, EncodeDotCAKBypassReq)
{
	auto rc = encode_nsm_dot_cak_bypass_req(BAD_IIDX, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_bypass_resp — L1998
TEST(DotPackFail, EncodeDotCAKBypassResp)
{
	auto rc =
	    encode_nsm_dot_cak_bypass_resp(BAD_IIDX, NSM_SUCCESS, 0, dotMsg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Optional pointer NULL in encode functions (dot_blob=NULL → memset path)
// ===========================================================================

// encode_nsm_dot_lock_resp: dot_blob=NULL → memset instead of memcpy (L2233)
TEST(DotOptionalNull, EncodeDotLockResp_NullBlob)
{
	auto rc =
	    encode_nsm_dot_lock_resp(0, NSM_SUCCESS, 0, nullptr, dotMsg());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_unlock_challenge_resp: challenge=NULL → memset (L2359)
TEST(DotOptionalNull, EncodeDotUnlockChallengeResp_NullChallenge)
{
	auto rc = encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, 0,
						       nullptr, dotMsg());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_cak_rotate_resp: dot_blob=NULL → memset (L2816)
TEST(DotOptionalNull, EncodeDotCAKRotateResp_NullBlob)
{
	auto rc = encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, 0, nullptr,
						 dotMsg());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_disable_resp: dot_blob=NULL → memset (L3042)
TEST(DotOptionalNull, EncodeDotDisableResp_NullBlob)
{
	auto rc =
	    encode_nsm_dot_disable_resp(0, NSM_SUCCESS, 0, nullptr, dotMsg());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// encode_nsm_dot_get_info_resp: dot_blob=NULL → skip memcpy (L2574)
TEST(DotOptionalNull, EncodeDotGetInfoResp_NullBlob)
{
	auto rc = encode_nsm_dot_get_info_resp(0, NSM_SUCCESS, 0, 1, 0, 5,
					       nullptr, dotMsg());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Optional pointer NULL in decode functions (skip copy path)
// Uses a success-CC buffer (payload[0]=0=NSM_SUCCESS, large buffer)
// ===========================================================================

// decode_nsm_dot_lock_resp: dot_blob=NULL → skip copy (L2265)
TEST(DotOptionalNull, DecodeDotLockResp_NullBlob)
{
	auto buf = makeSuccessBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_lock_resp(msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// decode_nsm_dot_unlock_challenge_resp: challenge=NULL → skip copy (L2392)
TEST(DotOptionalNull, DecodeDotUnlockChallengeResp_NullChallenge)
{
	auto buf = makeSuccessBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_challenge_resp(msg, buf.size(), &cc,
						       &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// decode_nsm_dot_get_info_resp: all optional pointers NULL (L2609-2618)
TEST(DotOptionalNull, DecodeDotGetInfoResp_AllNullOptionals)
{
	auto buf = makeSuccessBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_info_resp(
	    msg, buf.size(), &cc, &reason, nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// decode_nsm_dot_get_status_resp: status=NULL → skip (L2725)
TEST(DotOptionalNull, DecodeDotGetStatusResp_NullStatus)
{
	auto buf = makeSuccessBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_get_status_resp(msg, buf.size(), &cc, &reason,
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// decode_nsm_dot_cak_rotate_resp: dot_blob=NULL → skip copy (L2848)
TEST(DotOptionalNull, DecodeDotCAKRotateResp_NullBlob)
{
	auto buf = makeSuccessBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_cak_rotate_resp(msg, buf.size(), &cc, &reason,
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// decode_nsm_dot_disable_resp: dot_blob=NULL → skip copy (L3074)
TEST(DotOptionalNull, DecodeDotDisableResp_NullBlob)
{
	auto buf = makeSuccessBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_disable_resp(msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Secondary null conditions in decode functions: cc=NULL or reason_code=NULL
// ===========================================================================

// decode_nsm_dot_unlock_challenge_resp — L2373: cc=NULL
TEST(DotSecondaryNull, DecodeDotUnlockChallengeResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_challenge_resp(msg, buf.size(), nullptr,
						       &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_unlock_challenge_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotUnlockChallengeResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_dot_unlock_challenge_resp(msg, buf.size(), &cc,
						       nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_unlock_resp — L2486: cc=NULL
TEST(DotSecondaryNull, DecodeDotUnlockResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_unlock_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_unlock_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotUnlockResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_dot_unlock_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_get_info_resp — L2587: cc=NULL
TEST(DotSecondaryNull, DecodeDotGetInfoResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_nsm_dot_get_info_resp(msg, buf.size(), nullptr, nullptr,
					 nullptr, nullptr, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_get_status_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotGetStatusResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	uint8_t status = 0;
	auto rc = decode_nsm_dot_get_status_resp(msg, buf.size(), nullptr,
						 &reason, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_cak_rotate_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotCAKRotateResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_cak_rotate_resp(msg, buf.size(), nullptr,
						 &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_cak_rotate_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotCAKRotateResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_dot_cak_rotate_resp(msg, buf.size(), &cc, nullptr,
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_lock_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotLockResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_lock_resp(msg, buf.size(), nullptr, &reason,
					   nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_lock_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotLockResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc =
	    decode_nsm_dot_lock_resp(msg, buf.size(), &cc, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_override_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotOverrideResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_override_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_override_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotOverrideResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_dot_override_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_disable_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotDisableResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc = decode_nsm_dot_disable_resp(msg, buf.size(), nullptr, &reason,
					      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_disable_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotDisableResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc =
	    decode_nsm_dot_disable_resp(msg, buf.size(), &cc, nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_recovery_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotRecoveryResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_recovery_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_recovery_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotRecoveryResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_dot_recovery_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_cak_install_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotCAKInstallResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_cak_install_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_cak_install_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotCAKInstallResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc =
	    decode_nsm_dot_cak_install_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_cak_bypass_resp — cc=NULL
TEST(DotSecondaryNull, DecodeDotCAKBypassResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	auto rc =
	    decode_nsm_dot_cak_bypass_resp(msg, buf.size(), nullptr, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_dot_cak_bypass_resp — reason_code=NULL
TEST(DotSecondaryNull, DecodeDotCAKBypassResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_dot_cak_bypass_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
