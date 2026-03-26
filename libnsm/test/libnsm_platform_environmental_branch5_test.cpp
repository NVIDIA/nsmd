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
 * Branch coverage batch 5 for platform-environmental.c
 *
 * Targets:
 *  L2413  decode_set_programmable_EDPp_scaling_factor_req: secondary NULLs
 *  L3167  decode_get_accum_GPU_util_time_resp: *cc != NSM_SUCCESS
 *  L3172  decode_get_accum_GPU_util_time_resp: msg_len != expected
 *  L3487  decode_get_row_remapping_counts_resp: *cc != NSM_SUCCESS
 *  L4265  encode_query_per_instance_gpm_metrics_req: pack fail
 *  L4300  decode_query_per_instance_gpm_metrics_req: msg_len too short
 *  L4301  decode_query_per_instance_gpm_metrics_req: secondary null
 *  L4302  decode_query_per_instance_gpm_metrics_req: further nulls
 *  L4306  decode_query_per_instance_gpm_metrics_req: data_size too small
 *  L4336  encode_query_per_instance_gpm_metrics_v2_req: pack fail
 *  L4371  decode_query_per_instance_gpm_metrics_v2_req: secondary null
 *  L4372  decode_query_per_instance_gpm_metrics_v2_req: msg_len too short
 *  L4373  decode_query_per_instance_gpm_metrics_v2_req: data_size too small
 *  L4386  decode_query_per_instance_gpm_metrics_v2_req: more nulls
 *  L4412  encode_aggregate_gpm_metric_percentage_data: negative percentage
 *  L4490  encode_get_violation_duration_req: pack fail
 *  L5096  encode_set_active_preset_profile_req: pack fail
 *  L5150  encode_set_active_preset_profile_resp: NULL msg
 *  L5196  decode_set_active_preset_profile_resp: msg_len too short
 *  L5200  decode_set_active_preset_profile_resp: data_size != 0
 *  L5206  encode_setup_admin_override_req: pack fail
 *  L5264  encode_setup_admin_override_resp: NULL msg
 *  L5310  decode_setup_admin_override_resp: data_size != 0
 *  L5319  encode_apply_admin_override_req via helper: NULL msg
 *  L5386  decode_apply_admin_override_resp: data_size != 0
 */

#include "base.h"
#include "platform-environmental.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// Min exact-size buf for decode_reason_code_and_cc with cc != NSM_SUCCESS.
// base.c decode_reason_code_and_cc requires msg_len == 5+4=9 exactly.
static std::vector<uint8_t> makeErrCcBuf()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // cc = non-success
	return buf;
}

// 11-byte buffer with data_size=1 set in nsm_common_resp.
// payload[4..5] = data_size; buf[sizeof(nsm_msg_hdr)+4] = 1.
static std::vector<uint8_t> makeNonZeroDataSizeBuf()
{
	std::vector<uint8_t> buf(11, 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // nsm_common_resp.data_size = 1
	return buf;
}

// ===========================================================================
// L2413 TRUE: decode_set_programmable_EDPp_scaling_factor_req
// Secondary null conditions: persistence==NULL, scaling_factor==NULL
// ===========================================================================
TEST(PlatEnvBranch5, DecodeProgrammableEdppScaling_NullPersistence)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t action = 0, scaling_factor = 0;
	// action != NULL, persistence = NULL → L2413 secondary condition TRUE
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    msg, buf.size(), &action, nullptr, &scaling_factor);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch5, DecodeProgrammableEdppScaling_NullScalingFactor)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t action = 0, persistence = 0;
	// scaling_factor = NULL → last null condition TRUE
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    msg, buf.size(), &action, &persistence, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L3167 TRUE: decode_get_accum_GPU_util_time_resp
// rc==NSM_SW_SUCCESS, *cc != NSM_SUCCESS (no pre-check for msg_len)
// Use 9-byte errCcBuf (exact size for decode_reason_code_and_cc)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeAccumGpuUtilTimeResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, data_size = 0;
	uint32_t ctx = 0, sm = 0;
	auto rc = decode_get_accum_GPU_util_time_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &ctx, &sm);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L3172 TRUE: decode_get_accum_GPU_util_time_resp
// cc==NSM_SUCCESS, but msg_len != expected (uses '!=' check)
// Use 9-byte buffer (cc=0 → decode_reason_code_and_cc returns OK)
// 9 != sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp) → TRUE
// ===========================================================================
TEST(PlatEnvBranch5, DecodeAccumGpuUtilTimeResp_MsgLenMismatch)
{
	// 9 bytes: cc=0=NSM_SUCCESS; decode_reason_code_and_cc returns OK
	// 9 != expected (large struct) → L3172 TRUE
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, data_size = 0;
	uint32_t ctx = 0, sm = 0;
	auto rc = decode_get_accum_GPU_util_time_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &ctx, &sm);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L3487 TRUE: decode_get_row_remapping_counts_resp
// rc==NSM_SW_SUCCESS, *cc != NSM_SUCCESS
// ===========================================================================
TEST(PlatEnvBranch5, DecodeRowRemappingCountsResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, data_size = 0;
	uint32_t corr = 0, uncorr = 0;
	auto rc = decode_get_row_remapping_counts_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &corr, &uncorr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// L4265 TRUE: encode_query_per_instance_gpm_metrics_req
// pack_nsm_header fails: instance_id=32 > NSM_INSTANCE_MAX(31)
// ===========================================================================
TEST(PlatEnvBranch5, EncodeQueryPerInstanceGpmMetricsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_per_instance_gpm_metrics_req(kBadIid, 0, 0, 0, 0,
							    0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L4300 TRUE: decode_query_per_instance_gpm_metrics_req
// msg_len < sizeof(nsm_msg_hdr) + sizeof(req) → NSM_SW_ERROR_LENGTH
// No decode_common_req → zero-filled buffer works for NULL check (msg != NULL)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsReq_MsgTooShort)
{
	// 3 bytes < required minimum
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t src = 0, gpu = 0, ci = 0, mid = 0;
	uint32_t bitmask = 0;
	auto rc = decode_query_per_instance_gpm_metrics_req(
	    msg, buf.size(), &src, &gpu, &ci, &mid, &bitmask);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L4301 TRUE: decode_query_per_instance_gpm_metrics_req
// retrieval_source == NULL (secondary null condition)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsReq_NullRetrievalSource)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t gpu = 0, ci = 0, mid = 0;
	uint32_t bitmask = 0;
	auto rc = decode_query_per_instance_gpm_metrics_req(
	    msg, buf.size(), nullptr, &gpu, &ci, &mid, &bitmask);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L4302 TRUE: decode_query_per_instance_gpm_metrics_req
// gpu_instance == NULL → further null condition
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsReq_NullGpuInstance)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t src = 0, ci = 0, mid = 0;
	uint32_t bitmask = 0;
	auto rc = decode_query_per_instance_gpm_metrics_req(
	    msg, buf.size(), &src, nullptr, &ci, &mid, &bitmask);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L4306 TRUE: decode_query_per_instance_gpm_metrics_req
// hdr.data_size=0 < required → NSM_SW_ERROR_DATA
// Zero-filled buffer of correct size (data_size=0 < required)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_query_per_instance_gpm_metrics_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t src = 0, gpu = 0, ci = 0, mid = 0;
	uint32_t bitmask = 0;
	// hdr.data_size=0 (default) < sizeof(req)-sizeof(nsm_common_req)
	auto rc = decode_query_per_instance_gpm_metrics_req(
	    msg, msgLen, &src, &gpu, &ci, &mid, &bitmask);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L4336 TRUE: encode_query_per_instance_gpm_metrics_v2_req
// pack_nsm_header fails: instance_id=32
// ===========================================================================
TEST(PlatEnvBranch5, EncodeQueryPerInstanceGpmMetricsV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	bitfield8_t bitmask = {0};
	auto rc = encode_query_per_instance_gpm_metrics_v2_req(
	    kBadIid, 0, 0, 0, 0, &bitmask, 1, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L4371 TRUE: decode_query_per_instance_gpm_metrics_v2_req
// retrieval_source == NULL (secondary null)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsV2Req_NullRetrievalSource)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t gpu = 0, ci = 0, mid = 0;
	bitfield8_t *bitmask = nullptr;
	size_t bflen = 0;
	auto rc = decode_query_per_instance_gpm_metrics_v2_req(
	    msg, buf.size(), nullptr, &gpu, &ci, &mid, &bitmask, &bflen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L4372 TRUE: decode_query_per_instance_gpm_metrics_v2_req
// msg_len < sizeof(nsm_msg_hdr) + sizeof(req) → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsV2Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t src = 0, gpu = 0, ci = 0, mid = 0;
	bitfield8_t *bitmask = nullptr;
	size_t bflen = 0;
	auto rc = decode_query_per_instance_gpm_metrics_v2_req(
	    msg, buf.size(), &src, &gpu, &ci, &mid, &bitmask, &bflen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L4373 / L4376-4379 TRUE:
// decode_query_per_instance_gpm_metrics_v2_req: data_size too small
// Zero-filled buffer of correct size (hdr.data_size=0 < required)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsV2Req_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_query_per_instance_gpm_metrics_v2_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t src = 0, gpu = 0, ci = 0, mid = 0;
	bitfield8_t *bitmask = nullptr;
	size_t bflen = 0;
	auto rc = decode_query_per_instance_gpm_metrics_v2_req(
	    msg, msgLen, &src, &gpu, &ci, &mid, &bitmask, &bflen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L4386 TRUE: decode_query_per_instance_gpm_metrics_v2_req
// instance_bitmask_length == NULL (last null condition)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeQueryPerInstanceGpmMetricsV2Req_NullBitmaskLen)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t src = 0, gpu = 0, ci = 0, mid = 0;
	bitfield8_t *bitmask = nullptr;
	auto rc = decode_query_per_instance_gpm_metrics_v2_req(
	    msg, buf.size(), &src, &gpu, &ci, &mid, &bitmask, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L4412 TRUE: encode_aggregate_gpm_metric_percentage_data
// percentage < 0 → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(PlatEnvBranch5, EncodeAggregateGpmMetricPercentage_Negative)
{
	uint8_t data[8] = {};
	size_t data_len = 0;
	auto rc =
	    encode_aggregate_gpm_metric_percentage_data(-1.0, data, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L4490 TRUE: encode_get_violation_duration_req
// pack_nsm_header fails: instance_id=32
// ===========================================================================
TEST(PlatEnvBranch5, EncodeGetViolationDurationReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_violation_duration_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L5096 TRUE: encode_set_active_preset_profile_req
// pack_nsm_header fails: instance_id=32
// ===========================================================================
TEST(PlatEnvBranch5, EncodeSetActivePresetProfileReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_active_preset_profile_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L5150 TRUE: encode_set_active_preset_profile_resp
// msg == NULL → NSM_SW_ERROR_NULL
// (response functions mask instance_id & 0x1f so pack never fails;
//  test the msg==NULL branch instead)
// ===========================================================================
TEST(PlatEnvBranch5, EncodeSetActivePresetProfileResp_NullMsg)
{
	auto rc =
	    encode_set_active_preset_profile_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L5196 TRUE: decode_set_active_preset_profile_resp
// msg_len < sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) = 11
// ===========================================================================
TEST(PlatEnvBranch5, DecodeSetActivePresetProfileResp_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_active_preset_profile_resp(msg, buf.size(), &cc,
							&reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L5200 TRUE: decode_set_active_preset_profile_resp
// msg_len >= 11, cc==NSM_SUCCESS, data_size != 0 → NSM_SW_ERROR_DATA
// Set nsm_common_resp.data_size = 1 at payload[4]
// ===========================================================================
TEST(PlatEnvBranch5, DecodeSetActivePresetProfileResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_active_preset_profile_resp(msg, buf.size(), &cc,
							&reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L5206 TRUE: encode_setup_admin_override_req
// pack_nsm_header fails: instance_id=32
// ===========================================================================
TEST(PlatEnvBranch5, EncodeSetupAdminOverrideReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_setup_admin_override_req(kBadIid, 0, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L5264 TRUE: encode_setup_admin_override_resp
// msg == NULL → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(PlatEnvBranch5, EncodeSetupAdminOverrideResp_NullMsg)
{
	auto rc = encode_setup_admin_override_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L5310 TRUE: decode_setup_admin_override_resp
// msg_len >= 11, cc==NSM_SUCCESS, data_size != 0 → NSM_SW_ERROR_DATA
// decode_reason_code_and_cc has no pre-check; use 11-byte buf with
// payload[4]=1 so data_size=1 != 0 triggers the check
// ===========================================================================
TEST(PlatEnvBranch5, DecodeSetupAdminOverrideResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_setup_admin_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L5319 TRUE: encode_apply_admin_override_req
// Uses encode_get_platform_env_command_no_payload_req internally.
// Pack fails with instance_id=32.
// ===========================================================================
TEST(PlatEnvBranch5, EncodeApplyAdminOverrideReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_apply_admin_override_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L5386 TRUE: decode_apply_admin_override_resp
// msg_len >= 11, cc==NSM_SUCCESS, data_size != 0 → NSM_SW_ERROR_DATA
// (Has pre-check msg_len >= 11 before decode_reason_code_and_cc)
// ===========================================================================
TEST(PlatEnvBranch5, DecodeApplyAdminOverrideResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_apply_admin_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
