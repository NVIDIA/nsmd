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
 * Branch coverage for platform-environmental.c — batch 4.
 *
 * Targets:
 *  L2794  decode_get_clock_limit_req: hdr.data_size < sizeof(clock_id)
 *  L2919  decode_get_curr_clock_freq_req: same
 *  L2982  decode_get_curr_clock_freq_resp: command != expected
 *  L2988  decode_get_curr_clock_freq_resp: data_size < sizeof(uint32_t)
 *  L3903  decode_set_clock_limit_req: msg_len != expected
 *  L3911  decode_set_clock_limit_req: hdr.data_size != expected
 *  L3991  decode_nsm_xid_event: msg_len too short
 *  L4094  decode_get_supported_gpm_metrics_resp: NULL bitmask arg
 *  L4128  decode_get_supported_gpm_metrics_resp: msg_len too short
 *  L4152  decode_get_supported_gpm_metrics_resp: data_size < min_data_size
 *  L4168  encode_query_aggregate_gpm_metrics_req: pack_nsm_header fail
 *  L4219  decode_query_aggregate_gpm_metrics_req: NULL args
 *  L4225  decode_query_aggregate_gpm_metrics_req: msg_len too short
 *  L4233  decode_query_aggregate_gpm_metrics_req: hdr.data_size too small
 */

#include "base.h"
#include "platform-environmental.h"
#include <gtest/gtest.h>
#include <vector>

// ===========================================================================
// L2794 TRUE: decode_get_clock_limit_req — hdr.data_size=0 < sizeof(clock_id)
// No decode_common_req in this function; zero-filled buffer is fine.
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetClockLimitReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req);
	std::vector<uint8_t> buf(msgLen, 0);
	// hdr.data_size = 0 (default) < sizeof(clock_id) = 1
	uint8_t clock_id = 0;
	auto rc = decode_get_clock_limit_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), msgLen, &clock_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L2919 TRUE: decode_get_curr_clock_freq_req — hdr.data_size=0 <
// sizeof(clock_id)
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetCurrClockFreqReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req);
	std::vector<uint8_t> buf(msgLen, 0);
	// hdr.data_size = 0 < sizeof(clock_id) = 1
	uint8_t clock_id = 0;
	auto rc = decode_get_curr_clock_freq_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), msgLen, &clock_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L2982 TRUE: decode_get_curr_clock_freq_resp — command !=
// NSM_GET_CURRENT_CLOCK_FREQUENCY Uses zero-filled buffer of exact size; cc=0
// (NSM_SUCCESS), command=0 != 0x0B
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetCurrClockFreqResp_CommandMismatch)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp);
	std::vector<uint8_t> buf(msgLen, 0);
	// payload[1] = cc = 0 (NSM_SUCCESS), payload[0] = command = 0 != 0x0B
	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	uint32_t freq = 0;
	auto rc = decode_get_curr_clock_freq_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), msgLen, &cc,
	    &data_size, &reason_code, &freq);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L2988 TRUE: decode_get_curr_clock_freq_resp — data_size < sizeof(uint32_t)
// Encode a valid resp, then corrupt hdr.data_size to 0.
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetCurrClockFreqResp_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint32_t freq_in = 1000;
	encode_get_curr_clock_freq_resp(0, NSM_SUCCESS, 0, &freq_in, msg);
	// Corrupt data_size in nsm_common_resp to 0 (< sizeof(uint32_t)=4)
	auto *resp =
	    reinterpret_cast<nsm_get_curr_clock_freq_resp *>(msg->payload);
	resp->hdr.data_size = 0;
	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	uint32_t freq_out = 0;
	auto rc = decode_get_curr_clock_freq_resp(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, &cc, &data_size,
	    &reason_code, &freq_out);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L3903 TRUE: decode_set_clock_limit_req — msg_len != expected
// ===========================================================================
TEST(PlatEnvBranch4, DecodeSetClockLimitReq_WrongMsgLen)
{
	const size_t correctLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req);
	std::vector<uint8_t> buf(correctLen, 0);
	uint8_t clock_id = 0, flags = 0;
	uint32_t lmin = 0, lmax = 0;
	// Pass correctLen - 1 to trigger != check
	auto rc = decode_set_clock_limit_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), correctLen - 1,
	    &clock_id, &flags, &lmin, &lmax);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L3911 TRUE: decode_set_clock_limit_req — hdr.data_size != expected
// Correct msg_len but data_size = 0
// ===========================================================================
TEST(PlatEnvBranch4, DecodeSetClockLimitReq_WrongDataSize)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req);
	std::vector<uint8_t> buf(msgLen, 0);
	// hdr.data_size = 0 (default) != expected non-zero value
	uint8_t clock_id = 0, flags = 0;
	uint32_t lmin = 0, lmax = 0;
	auto rc = decode_set_clock_limit_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), msgLen, &clock_id,
	    &flags, &lmin, &lmax);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L3991 TRUE: decode_nsm_xid_event — msg_len < nsm_msg_hdr + NSM_EVENT_MIN_LEN
// ===========================================================================
TEST(PlatEnvBranch4, DecodeNsmXidEvent_MsgTooShort)
{
	// NSM_EVENT_MIN_LEN=6; minimum would be 11; use 5 bytes (only header)
	std::vector<uint8_t> buf(5, 0);
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	nsm_xid_event_payload payload{};
	char msg_text[64] = {};
	size_t msg_text_size = sizeof(msg_text);
	auto rc = decode_nsm_xid_event(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(),
	    &event_class, &event_state, &payload, msg_text, &msg_text_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L4094 TRUE: decode_get_supported_gpm_metrics_resp — NULL bitmask arg
// cc=NSM_SUCCESS (payload[1]=0); buf large enough for decode_common_resp
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetSupportedGpmMetricsResp_NullBitmask)
{
	// Size: nsm_msg_hdr(5) + nsm_common_resp(6) = 11, cc=0, data_size=4
	std::vector<uint8_t> buf(11, 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 4; // data_size = 4 (>= min_data_size)
	uint8_t cc = 0;
	uint16_t reason_code = 0, mask_size = 0, max_metrics = 0;
	uint16_t bitmask_size = 0;
	auto rc = decode_get_supported_gpm_metrics_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &reason_code, &mask_size, &max_metrics, nullptr, &bitmask_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L4128 TRUE: decode_get_supported_gpm_metrics_resp — msg_len too short
// cc=NSM_SUCCESS; NULL checks pass; but msg_len < min required
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetSupportedGpmMetricsResp_MsgTooShort)
{
	// nsm_msg_hdr(5) + nsm_get_supported_gpm_metrics_resp-1 = 5+10 = 15
	// Use 14 bytes (< 15) with cc=0, enough for decode_common_resp (>=11)
	std::vector<uint8_t> buf(14, 0);
	uint8_t cc = 0;
	uint16_t reason_code = 0, mask_size = 0, max_metrics = 0;
	uint16_t bitmask_size = 0;
	uint8_t bitmask[64] = {};
	auto rc = decode_get_supported_gpm_metrics_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &reason_code, &mask_size, &max_metrics, bitmask, &bitmask_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L4152 TRUE: decode_get_supported_gpm_metrics_resp — data_size < min_data_size
// cc=NSM_SUCCESS; msg_len >= 15; data_size=0 < min_data_size(4)
// ===========================================================================
TEST(PlatEnvBranch4, DecodeGetSupportedGpmMetricsResp_DataSizeTooSmall)
{
	// 15 bytes: nsm_msg_hdr(5) + nsm_get_supported_gpm_metrics_resp-1(10)
	std::vector<uint8_t> buf(15, 0);
	// cc=0 (NSM_SUCCESS), data_size=0 (payload[4..5]=0) < min_data_size=4
	uint8_t cc = 0;
	uint16_t reason_code = 0, mask_size = 0, max_metrics = 0;
	uint16_t bitmask_size = 0;
	uint8_t bitmask[64] = {};
	auto rc = decode_get_supported_gpm_metrics_resp(
	    reinterpret_cast<const nsm_msg *>(buf.data()), buf.size(), &cc,
	    &reason_code, &mask_size, &max_metrics, bitmask, &bitmask_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L4168 TRUE: encode_query_aggregate_gpm_metrics_req — pack_nsm_header fail
// instance_id=32 > NSM_INSTANCE_MAX(31)
// ===========================================================================
TEST(PlatEnvBranch4, EncodeQueryAggregateGpmMetricsReq_PackFail)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_aggregate_gpm_metrics_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t bitfield = 0;
	auto rc = encode_query_aggregate_gpm_metrics_req(32, 0, 0, 0, &bitfield,
							 1, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L4219 TRUE: decode_query_aggregate_gpm_metrics_req — NULL args
// ===========================================================================
TEST(PlatEnvBranch4, DecodeQueryAggregateGpmMetricsReq_NullArgs)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_aggregate_gpm_metrics_req);
	std::vector<uint8_t> buf(msgLen, 0);
	const uint8_t *bitfield = nullptr;
	size_t bfLen = 0;
	auto rc = decode_query_aggregate_gpm_metrics_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), msgLen, nullptr,
	    nullptr, nullptr, &bitfield, &bfLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L4225 TRUE: decode_query_aggregate_gpm_metrics_req — msg_len too short
// ===========================================================================
TEST(PlatEnvBranch4, DecodeQueryAggregateGpmMetricsReq_MsgTooShort)
{
	const size_t fullLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_aggregate_gpm_metrics_req);
	std::vector<uint8_t> buf(fullLen, 0);
	uint8_t src = 0, gpu = 0, ci = 0;
	const uint8_t *bitfield = nullptr;
	size_t bfLen = 0;
	auto rc = decode_query_aggregate_gpm_metrics_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), fullLen - 1, &src,
	    &gpu, &ci, &bitfield, &bfLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L4233 TRUE: decode_query_aggregate_gpm_metrics_req — hdr.data_size too small
// Correct msg_len; hdr.data_size=0 < (sizeof(req)-sizeof(common_req))
// ===========================================================================
TEST(PlatEnvBranch4, DecodeQueryAggregateGpmMetricsReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_aggregate_gpm_metrics_req);
	std::vector<uint8_t> buf(msgLen, 0);
	// hdr.data_size=0 (default) < sizeof(req)-sizeof(nsm_common_req)
	uint8_t src = 0, gpu = 0, ci = 0;
	const uint8_t *bitfield = nullptr;
	size_t bfLen = 0;
	auto rc = decode_query_aggregate_gpm_metrics_req(
	    reinterpret_cast<const nsm_msg *>(buf.data()), msgLen, &src, &gpu,
	    &ci, &bitfield, &bfLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
