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
 * Branch coverage batch 7 for platform-environmental.c
 *
 * Targets: successful encode/decode round-trips, cc=NSM_ERROR paths,
 * boundary msg_len, different valid parameter values, and aggregate
 * functions not yet exercised by branch tests 1-6.
 */

#include "base.h"
#include "platform-environmental.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// Helper: build an error response buffer with cc=NSM_ERROR
static std::vector<uint8_t> makeErrorResp()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc = non-success
	return buf;
}

// ===========================================================================
// Set Clock Limit: encode req success + decode roundtrip
// ===========================================================================
TEST(PlatEnvBranch7, EncodeSetClockLimitReq_Success)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_set_clock_limit_req(0, GRAPHICS_CLOCK, 1, 100, 1000, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetClockLimitReq_NullMsg)
{
	auto rc = encode_set_clock_limit_req(0, GRAPHICS_CLOCK, 1, 100, 1000,
					     nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeSetClockLimitReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_clock_limit_req(kBadIid, GRAPHICS_CLOCK, 1, 100,
					     1000, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeSetClockLimitReq_NullMsg)
{
	uint8_t clockId = 0, flags = 0;
	uint32_t min = 0, max = 0;
	auto rc = decode_set_clock_limit_req(nullptr, 100, &clockId, &flags,
					     &min, &max);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetClockLimitReq_NullClockId)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t flags = 0;
	uint32_t min = 0, max = 0;
	auto rc = decode_set_clock_limit_req(msg, buf.size(), nullptr, &flags,
					     &min, &max);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetClockLimitReq_NullFlags)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t clockId = 0;
	uint32_t min = 0, max = 0;
	auto rc = decode_set_clock_limit_req(msg, buf.size(), &clockId, nullptr,
					     &min, &max);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetClockLimitReq_NullMin)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t clockId = 0, flags = 0;
	uint32_t max = 0;
	auto rc = decode_set_clock_limit_req(msg, buf.size(), &clockId, &flags,
					     nullptr, &max);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetClockLimitReq_NullMax)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t clockId = 0, flags = 0;
	uint32_t min = 0;
	auto rc = decode_set_clock_limit_req(msg, buf.size(), &clockId, &flags,
					     &min, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetClockLimitReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t clockId = 0, flags = 0;
	uint32_t min = 0, max = 0;
	auto rc = decode_set_clock_limit_req(msg, buf.size(), &clockId, &flags,
					     &min, &max);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetClockLimitResp_NullMsg)
{
	auto rc = encode_set_clock_limit_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Set Clock Limit: encode/decode roundtrip
// ===========================================================================
TEST(PlatEnvBranch7, SetClockLimit_Roundtrip)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_set_clock_limit_req(0, MEMORY_CLOCK, 1, 200, 2000, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t clockId = 0, flags = 0;
	uint32_t min = 0, max = 0;
	rc = decode_set_clock_limit_req(
	    msg, sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req),
	    &clockId, &flags, &min, &max);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(clockId, MEMORY_CLOCK);
	EXPECT_EQ(flags, 1);
	EXPECT_EQ(min, 200u);
	EXPECT_EQ(max, 2000u);
}

// ===========================================================================
// Get Clock Limit: NULL checks and pack fail
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetClockLimitReq_NullMsg)
{
	auto rc = encode_get_clock_limit_req(0, GRAPHICS_CLOCK, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetClockLimitReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_clock_limit_req(kBadIid, GRAPHICS_CLOCK, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeGetClockLimitReq_NullMsg)
{
	uint8_t clockId = 0;
	auto rc = decode_get_clock_limit_req(nullptr, 100, &clockId);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockLimitReq_NullClockId)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_clock_limit_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockLimitReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t clockId = 0;
	auto rc = decode_get_clock_limit_req(msg, buf.size(), &clockId);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetClockLimitResp_NullMsg)
{
	struct nsm_clock_limit cl = {};
	auto rc = encode_get_clock_limit_resp(0, NSM_SUCCESS, 0, &cl, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockLimitResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_clock_limit cl = {};
	auto rc = decode_get_clock_limit_resp(msg, buf.size(), &cc, &ds,
					      &reason, &cl);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Current Clock Frequency: NULL checks and pack fail
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetCurrClockFreqReq_NullMsg)
{
	auto rc = encode_get_curr_clock_freq_req(0, GRAPHICS_CLOCK, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetCurrClockFreqReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_curr_clock_freq_req(kBadIid, GRAPHICS_CLOCK, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeGetCurrClockFreqReq_NullMsg)
{
	uint8_t clockId = 0;
	auto rc = decode_get_curr_clock_freq_req(nullptr, 100, &clockId);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetCurrClockFreqReq_NullClockId)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_curr_clock_freq_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetCurrClockFreqReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t clockId = 0;
	auto rc = decode_get_curr_clock_freq_req(msg, buf.size(), &clockId);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetCurrClockFreqResp_NullMsg)
{
	uint32_t freq = 1000;
	auto rc =
	    encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, 0, &freq, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetCurrClockFreqResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	uint32_t freq = 0;
	auto rc = decode_get_curr_clock_freq_resp(msg, buf.size(), &cc, &ds,
						  &reason, &freq);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Current Clock Event Reason Code: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetClockEventReasonReq_NullMsg)
{
	auto rc = encode_get_current_clock_event_reason_code_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetClockEventReasonReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_clock_event_reason_code_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetClockEventReasonResp_NullMsg)
{
	bitfield32_t flags = {0};
	auto rc = encode_get_current_clock_event_reason_code_resp(
	    0, NSM_SUCCESS, 0, &flags, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockEventReasonResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield32_t flags = {0};
	auto rc = decode_get_current_clock_event_reason_code_resp(
	    msg, buf.size(), &cc, &ds, &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Accumulated GPU Utilization Time: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetAccumGpuUtilReq_NullMsg)
{
	auto rc = encode_get_accum_GPU_util_time_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetAccumGpuUtilReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_accum_GPU_util_time_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetAccumGpuUtilResp_NullMsg)
{
	uint32_t ctx = 100, sm = 200;
	auto rc = encode_get_accum_GPU_util_time_resp(0, NSM_SUCCESS, 0, &ctx,
						      &sm, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetAccumGpuUtilResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	uint32_t ctx = 0, sm = 0;
	auto rc = decode_get_accum_GPU_util_time_resp(msg, buf.size(), &cc, &ds,
						      &reason, &ctx, &sm);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Current Utilization: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetCurrUtilReq_NullMsg)
{
	auto rc = encode_get_current_utilization_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetCurrUtilReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_current_utilization_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetCurrUtilResp_NullMsg)
{
	struct nsm_get_current_utilization_data data = {};
	auto rc = encode_get_current_utilization_resp(0, NSM_SUCCESS, 0, &data,
						      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetCurrUtilResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_get_current_utilization_data data = {};
	auto rc = decode_get_current_utilization_resp(msg, buf.size(), &cc, &ds,
						      &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Row Remap State: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetRowRemapStateReq_NullMsg)
{
	auto rc = encode_get_row_remap_state_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetRowRemapStateReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_row_remap_state_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetRowRemapStateResp_NullMsg)
{
	bitfield8_t flags = {0};
	auto rc =
	    encode_get_row_remap_state_resp(0, NSM_SUCCESS, 0, &flags, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetRowRemapStateResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_row_remap_state_resp(msg, buf.size(), &cc, &ds,
						  &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Row Remapping Counts: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetRowRemappingCountsReq_NullMsg)
{
	auto rc = encode_get_row_remapping_counts_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetRowRemappingCountsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_row_remapping_counts_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetRowRemappingCountsResp_NullMsg)
{
	auto rc = encode_get_row_remapping_counts_resp(0, NSM_SUCCESS, 0, 0, 0,
						       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetRowRemappingCountsResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	uint32_t corr = 0, uncorr = 0;
	auto rc = decode_get_row_remapping_counts_resp(
	    msg, buf.size(), &cc, &ds, &reason, &corr, &uncorr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Row Remap Availability: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetRowRemapAvailReq_NullMsg)
{
	auto rc = encode_get_row_remap_availability_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetRowRemapAvailReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_row_remap_availability_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetRowRemapAvailResp_NullMsg)
{
	struct nsm_row_remap_availability data = {};
	auto rc = encode_get_row_remap_availability_resp(0, NSM_SUCCESS, 0,
							 &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetRowRemapAvailResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_row_remap_availability data = {};
	auto rc = decode_get_row_remap_availability_resp(msg, buf.size(), &cc,
							 &ds, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Memory Capacity Utilization: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetMemCapUtilReq_NullMsg)
{
	auto rc = encode_get_memory_capacity_util_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetMemCapUtilReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_memory_capacity_util_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetMemCapUtilResp_NullMsg)
{
	struct nsm_memory_capacity_utilization data = {};
	auto rc = encode_get_memory_capacity_util_resp(0, NSM_SUCCESS, 0, &data,
						       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetMemCapUtilResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_memory_capacity_utilization data = {};
	auto rc = decode_get_memory_capacity_util_resp(msg, buf.size(), &cc,
						       &ds, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Clock Output Enable State: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetClockOutputEnableStateReq_NullMsg)
{
	auto rc = encode_get_clock_output_enable_state_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetClockOutputEnableStateReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_clock_output_enable_state_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeGetClockOutputEnableStateReq_NullMsg)
{
	uint8_t index = 0;
	auto rc =
	    decode_get_clock_output_enable_state_req(nullptr, 100, &index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockOutputEnableStateReq_NullIndex)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_clock_output_enable_state_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockOutputEnableStateReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t index = 0;
	auto rc =
	    decode_get_clock_output_enable_state_req(msg, buf.size(), &index);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetClockOutputEnableStateResp_NullMsg)
{
	auto rc = encode_get_clock_output_enable_state_resp(0, NSM_SUCCESS, 0,
							    0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetClockOutputEnableStateResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	uint32_t data = 0;
	auto rc = decode_get_clock_output_enable_state_resp(
	    msg, buf.size(), &cc, &ds, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// ECC mode: NULL checks and pack fail
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetEccModeReq_NullMsg)
{
	auto rc = encode_get_ECC_mode_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetEccModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_ECC_mode_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetEccModeResp_NullMsg)
{
	bitfield8_t flags = {0};
	auto rc = encode_get_ECC_mode_resp(0, NSM_SUCCESS, 0, &flags, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetEccModeResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_ECC_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(PlatEnvBranch7, EncodeSetEccModeReq_NullMsg)
{
	auto rc = encode_set_ECC_mode_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeSetEccModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_ECC_mode_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeSetEccModeReq_NullMsg)
{
	uint8_t mode = 0;
	auto rc = decode_set_ECC_mode_req(nullptr, 100, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetEccModeReq_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_set_ECC_mode_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetEccModeReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_set_ECC_mode_req(msg, buf.size(), &mode);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetEccModeResp_NullMsg)
{
	auto rc = encode_set_ECC_mode_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Get ECC Error Counts: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetEccErrorCountsResp_NullMsg)
{
	struct nsm_ECC_error_counts ec = {};
	auto rc =
	    encode_get_ECC_error_counts_resp(0, NSM_SUCCESS, 0, &ec, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetEccErrorCountsResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_ECC_error_counts ec = {};
	auto rc = decode_get_ECC_error_counts_resp(msg, buf.size(), &cc, &ds,
						   &reason, &ec);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// EDPp Scaling Factor: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetEdppScalingFactorReq_NullMsg)
{
	auto rc = encode_get_programmable_EDPp_scaling_factor_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetEdppScalingFactorReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_programmable_EDPp_scaling_factor_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetEdppScalingFactorResp_NullMsg)
{
	struct nsm_EDPp_scaling_factors sf = {};
	auto rc = encode_get_programmable_EDPp_scaling_factor_resp(
	    0, NSM_SUCCESS, 0, &sf, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetEdppScalingFactorResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_EDPp_scaling_factors sf = {};
	auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    msg, buf.size(), &cc, &ds, &reason, &sf);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(PlatEnvBranch7, EncodeSetEdppScalingFactorReq_NullMsg)
{
	auto rc = encode_set_programmable_EDPp_scaling_factor_req(0, 0, 0, 50,
								  nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeSetEdppScalingFactorReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_programmable_EDPp_scaling_factor_req(kBadIid, 0, 0,
								  50, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeSetEdppScalingFactorReq_NullMsg)
{
	uint8_t action = 0, persistence = 0, factor = 0;
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    nullptr, 100, &action, &persistence, &factor);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetEdppScalingFactorReq_NullAction)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t persistence = 0, factor = 0;
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    msg, buf.size(), nullptr, &persistence, &factor);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetEdppScalingFactorReq_NullPersistence)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t action = 0, factor = 0;
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    msg, buf.size(), &action, nullptr, &factor);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetEdppScalingFactorReq_NullFactor)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t action = 0, persistence = 0;
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    msg, buf.size(), &action, &persistence, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetEdppScalingFactorReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t action = 0, persistence = 0, factor = 0;
	auto rc = decode_set_programmable_EDPp_scaling_factor_req(
	    msg, buf.size(), &action, &persistence, &factor);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetEdppScalingFactorResp_NullMsg)
{
	auto rc = encode_set_programmable_EDPp_scaling_factor_resp(
	    0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Set/Get Power Limit: NULL checks and pack fail
// ===========================================================================
TEST(PlatEnvBranch7, EncodeSetPowerLimitReq_NullMsg)
{
	auto rc = encode_set_power_limit_req(0, DEVICE, NEW_LIMIT, ONE_SHOT,
					     100, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeSetPowerLimitReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_power_limit_req(kBadIid, DEVICE, NEW_LIMIT,
					     ONE_SHOT, 100, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeSetPowerLimitReq_NullMsg)
{
	uint32_t id = 0;
	uint8_t action = 0, persist = 0;
	uint32_t limit = 0;
	auto rc = decode_set_power_limit_req(nullptr, 100, &id, &action,
					     &persist, &limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetPowerLimitReq_NullId)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t action = 0, persist = 0;
	uint32_t limit = 0;
	auto rc = decode_set_power_limit_req(msg, buf.size(), nullptr, &action,
					     &persist, &limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetPowerLimitReq_NullAction)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t id = 0, limit = 0;
	uint8_t persist = 0;
	auto rc = decode_set_power_limit_req(msg, buf.size(), &id, nullptr,
					     &persist, &limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetPowerLimitReq_NullPersist)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t id = 0, limit = 0;
	uint8_t action = 0;
	auto rc = decode_set_power_limit_req(msg, buf.size(), &id, &action,
					     nullptr, &limit);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetPowerLimitReq_NullLimit)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t id = 0;
	uint8_t action = 0, persist = 0;
	auto rc = decode_set_power_limit_req(msg, buf.size(), &id, &action,
					     &persist, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetPowerLimitReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t id = 0, limit = 0;
	uint8_t action = 0, persist = 0;
	auto rc = decode_set_power_limit_req(msg, buf.size(), &id, &action,
					     &persist, &limit);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetPowerLimitResp_NullMsg)
{
	auto rc = encode_set_power_limit_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetPowerLimitReq_NullMsg)
{
	auto rc = encode_get_power_limit_req(0, DEVICE, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetPowerLimitReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_power_limit_req(kBadIid, DEVICE, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeGetPowerLimitReq_NullMsg)
{
	uint32_t id = 0;
	auto rc = decode_get_power_limit_req(nullptr, 100, &id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetPowerLimitReq_NullId)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_power_limit_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetPowerLimitReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t id = 0;
	auto rc = decode_get_power_limit_req(msg, buf.size(), &id);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetPowerLimitResp_NullMsg)
{
	auto rc = encode_get_power_limit_resp(0, NSM_SUCCESS, 0, 100, 200, 150,
					      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetPowerLimitResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	uint32_t persist = 0, oneshot = 0, enforced = 0;
	auto rc = decode_get_power_limit_resp(
	    msg, buf.size(), &cc, &ds, &reason, &persist, &oneshot, &enforced);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Set MIG mode: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeSetMigModeReq_NullMsg)
{
	auto rc = encode_set_MIG_mode_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeSetMigModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_MIG_mode_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeSetMigModeReq_NullMsg)
{
	uint8_t mode = 0;
	auto rc = decode_set_MIG_mode_req(nullptr, 100, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetMigModeReq_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_set_MIG_mode_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeSetMigModeReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_set_MIG_mode_req(msg, buf.size(), &mode);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetMigModeResp_NullMsg)
{
	auto rc = encode_set_MIG_mode_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Get Supported GPM Metrics: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetSupportedGpmMetricsReq_NullMsg)
{
	auto rc = encode_get_supported_gpm_metrics_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetSupportedGpmMetricsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_supported_gpm_metrics_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, DecodeGetSupportedGpmMetricsReq_NullMsg)
{
	uint8_t type = 0;
	auto rc = decode_get_supported_gpm_metrics_req(nullptr, 100, &type);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetSupportedGpmMetricsReq_NullType)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_supported_gpm_metrics_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeGetSupportedGpmMetricsReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t type = 0;
	auto rc = decode_get_supported_gpm_metrics_req(msg, buf.size(), &type);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetSupportedGpmMetricsResp_NullMsg)
{
	uint8_t bitmask[4] = {};
	auto rc = encode_get_supported_gpm_metrics_resp(0, NSM_SUCCESS, 0, 4,
							10, bitmask, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Aggregate Timestamp: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeAggregateTimestamp_NullData)
{
	size_t len = 0;
	auto rc = encode_aggregate_timestamp_data(1234, nullptr, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeAggregateTimestamp_NullLen)
{
	uint8_t data[8] = {};
	auto rc = encode_aggregate_timestamp_data(1234, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeAggregateTimestamp_NullData)
{
	uint64_t ts = 0;
	auto rc = decode_aggregate_timestamp_data(nullptr, 8, &ts);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeAggregateTimestamp_NullOutput)
{
	uint8_t data[8] = {};
	auto rc = decode_aggregate_timestamp_data(data, 8, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeAggregateTimestamp_WrongLen)
{
	uint8_t data[4] = {};
	uint64_t ts = 0;
	auto rc = decode_aggregate_timestamp_data(data, 4, &ts);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Driver Info: cc=NSM_ERROR path in decode_get_driver_info_resp
// ===========================================================================
TEST(PlatEnvBranch7, DecodeGetDriverInfoResp_CcError)
{
	auto buf = makeErrorResp();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum8 state = 0;
	char version[256] = {};
	auto rc = decode_get_driver_info_resp(msg, buf.size(), &cc, &reason,
					      &state, version);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetDriverInfoReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_driver_info_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeGetDriverInfoResp_NullMsg)
{
	uint8_t data[2] = {1, 0};
	auto rc =
	    encode_get_driver_info_resp(0, NSM_SUCCESS, 0, 2, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Leak Detection: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeGetLeakDetectionInfoReq_NullMsg)
{
	auto rc = encode_get_leak_detection_info_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeGetLeakDetectionInfoReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_leak_detection_info_req(kBadIid, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(PlatEnvBranch7, EncodeSetLeakDetThresholdsResp_NullMsg)
{
	auto rc = encode_set_leak_detection_thresholds_resp(0, NSM_SUCCESS, 0,
							    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Workload Power Profile: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeEnableWorkloadPowerProfileReq_NullMsg)
{
	bitfield256_t mask = {};
	auto rc = encode_enable_workload_power_profile_req(0, &mask, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeDisableWorkloadPowerProfileReq_NullMsg)
{
	bitfield256_t mask = {};
	auto rc = encode_disable_workload_power_profile_req(0, &mask, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeEnableWorkloadPowerProfileResp_NullMsg)
{
	auto rc = encode_enable_workload_power_profile_resp(0, NSM_SUCCESS, 0,
							    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeDisableWorkloadPowerProfileResp_NullMsg)
{
	auto rc = encode_disable_workload_power_profile_resp(0, NSM_SUCCESS, 0,
							     nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Query Aggregate GPM Metrics: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeQueryAggGpmMetricsReq_NullMsg)
{
	uint8_t bitfield[4] = {};
	auto rc = encode_query_aggregate_gpm_metrics_req(0, 0, 0, 0, bitfield,
							 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeQueryAggGpmMetricsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t bitfield[4] = {};
	auto rc = encode_query_aggregate_gpm_metrics_req(kBadIid, 0, 0, 0,
							 bitfield, 4, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Query Per-Instance GPM Metrics: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeQueryPerInstGpmMetricsReq_NullMsg)
{
	auto rc = encode_query_per_instance_gpm_metrics_req(0, 0, 0, 0, 0, 0,
							    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeQueryPerInstGpmMetricsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_per_instance_gpm_metrics_req(kBadIid, 0, 0, 0, 0,
							    0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Query Per-Instance GPM Metrics V2: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeQueryPerInstGpmMetricsV2Req_NullMsg)
{
	bitfield8_t bitmask[4] = {};
	auto rc = encode_query_per_instance_gpm_metrics_v2_req(
	    0, 0, 0, 0, 0, bitmask, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, EncodeQueryPerInstGpmMetricsV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	bitfield8_t bitmask[4] = {};
	auto rc = encode_query_per_instance_gpm_metrics_v2_req(
	    kBadIid, 0, 0, 0, 0, bitmask, 4, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Encode XID Event: NULL msg
// ===========================================================================
TEST(PlatEnvBranch7, EncodeXidEvent_NullMsg)
{
	struct nsm_xid_event_payload payload = {};
	const char text[] = "test";
	auto rc = encode_nsm_xid_event(0, false, payload, text,
				       sizeof(text) - 1, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Encode Reset Required Event: NULL msg
// ===========================================================================
TEST(PlatEnvBranch7, EncodeResetRequiredEvent_NullMsg)
{
	auto rc = encode_nsm_reset_required_event(0, false, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Query Admin Override: NULL checks
// ===========================================================================
TEST(PlatEnvBranch7, EncodeQueryAdminOverrideResp_NullMsg)
{
	struct nsm_admin_override_data data = {};
	auto rc =
	    encode_query_admin_override_resp(0, NSM_SUCCESS, 0, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(PlatEnvBranch7, DecodeQueryAdminOverrideResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	struct nsm_admin_override_data data = {};
	auto rc = decode_query_admin_override_resp(msg, buf.size(), &cc, &ds,
						   &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
