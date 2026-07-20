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
 * Branch coverage for diagnostics.c — secondary null checks, cc non-success
 * paths in decode_xxx_resp functions, and data_size mismatch branches not
 * exercised by the existing test files.
 */

#include "base.h"
#include "diagnostics.h"
#include <gtest/gtest.h>
#include <string.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// 9-byte buffer: payload[1]=0xFF → decode_reason_code_and_cc returns
// NSM_SW_SUCCESS with *cc=0xFF (non-success completion code path)
static std::vector<uint8_t>
errCcBuf9(size_t sz = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp))
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF;
	return buf;
}

// ===========================================================================
// decode_get_device_diagnostics_resp — secondary null conditions
// if (seg_data == NULL || seg_data_size == NULL || next_segment_id == NULL)
// ===========================================================================
TEST(DiagBranch, GetDeviceDiagnosticsResp_NullSegDataSize)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t seg_data[16] = {};
	uint8_t next_id = 0;
	// seg_data != NULL, seg_data_size == NULL → second condition TRUE
	auto rc = decode_get_device_diagnostics_resp(
	    msg, buf.size(), &cc, &reason, seg_data, nullptr, &next_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetDeviceDiagnosticsResp_NullNextSegmentId)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t seg_data[16] = {};
	uint16_t seg_data_size = 0;
	// seg_data != NULL, seg_data_size != NULL, next_segment_id == NULL
	auto rc = decode_get_device_diagnostics_resp(
	    msg, buf.size(), &cc, &reason, seg_data, &seg_data_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// cc non-success branch — all output params valid, buf triggers *cc = 0xFF
TEST(DiagBranch, GetDeviceDiagnosticsResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t seg_data[16] = {};
	uint16_t seg_data_size = 0;
	uint8_t next_id = 0;
	auto rc = decode_get_device_diagnostics_resp(
	    msg, buf.size(), &cc, &reason, seg_data, &seg_data_size, &next_id);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_reset_network_device_resp — cc non-success branch (decode_cc called
// directly with no pre-size-check, so errCcBuf9 works)
// ===========================================================================
TEST(DiagBranch, ResetNetworkDeviceResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_reset_network_device_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_network_device_debug_info_resp — secondary null conditions
// if (seg_data == NULL || seg_data_size == NULL || next_handle == NULL)
// ===========================================================================
TEST(DiagBranch, GetNetworkDeviceDebugInfoResp_NullSegDataSize)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t seg_data[16] = {};
	uint32_t next_handle = 0;
	// seg_data != NULL, seg_data_size == NULL → second condition TRUE
	auto rc = decode_get_network_device_debug_info_resp(
	    msg, buf.size(), &cc, &reason, nullptr, seg_data, &next_handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetNetworkDeviceDebugInfoResp_NullNextHandle)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t seg_data_size = 0;
	uint8_t seg_data[16] = {};
	// seg_data != NULL, seg_data_size != NULL, next_handle == NULL
	auto rc = decode_get_network_device_debug_info_resp(
	    msg, buf.size(), &cc, &reason, &seg_data_size, seg_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// cc non-success branch
TEST(DiagBranch, GetNetworkDeviceDebugInfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint16_t seg_data_size = 0;
	uint8_t seg_data[16] = {};
	uint32_t next_handle = 0;
	auto rc = decode_get_network_device_debug_info_resp(
	    msg, buf.size(), &cc, &reason, &seg_data_size, seg_data,
	    &next_handle);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_erase_trace_resp — cc non-success branch
// ===========================================================================
TEST(DiagBranch, EraseTraceResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t result_status = 0;
	auto rc = decode_erase_trace_resp(msg, buf.size(), &cc, &reason,
					  &result_status);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_network_device_log_info_resp — secondary null conditions
// if (log_info == NULL || log_data == NULL || log_data_size == NULL ||
//     next_handle == NULL)
// ===========================================================================
TEST(DiagBranch, GetNetworkDeviceLogInfoResp_NullLogData)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t next_handle = 0;
	struct nsm_device_log_info_breakdown log_info = {};
	uint16_t log_data_size = 0;
	// log_info != NULL, log_data == NULL → second condition TRUE
	auto rc = decode_get_network_device_log_info_resp(
	    msg, buf.size(), &cc, &reason, &next_handle, &log_info, nullptr,
	    &log_data_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetNetworkDeviceLogInfoResp_NullLogDataSize)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t next_handle = 0;
	struct nsm_device_log_info_breakdown log_info = {};
	uint8_t log_data[64] = {};
	// log_info != NULL, log_data != NULL, log_data_size == NULL
	auto rc = decode_get_network_device_log_info_resp(
	    msg, buf.size(), &cc, &reason, &next_handle, &log_info, log_data,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetNetworkDeviceLogInfoResp_NullNextHandle)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_device_log_info_breakdown log_info = {};
	uint8_t log_data[64] = {};
	uint16_t log_data_size = 0;
	// log_info != NULL, log_data != NULL, log_data_size != NULL,
	// next_handle == NULL
	auto rc = decode_get_network_device_log_info_resp(
	    msg, buf.size(), &cc, &reason, nullptr, &log_info, log_data,
	    &log_data_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// cc non-success branch
TEST(DiagBranch, GetNetworkDeviceLogInfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t next_handle = 0;
	struct nsm_device_log_info_breakdown log_info = {};
	uint8_t log_data[64] = {};
	uint16_t log_data_size = 0;
	auto rc = decode_get_network_device_log_info_resp(
	    msg, buf.size(), &cc, &reason, &next_handle, &log_info, log_data,
	    &log_data_size);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_erase_debug_info_resp — cc non-success branch
// ===========================================================================
TEST(DiagBranch, EraseDebugInfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t result_status = 0;
	auto rc = decode_erase_debug_info_resp(msg, buf.size(), &cc, &reason,
					       &result_status);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_enable_disable_wp_req — secondary null conditions
// if (msg == NULL || data_index == NULL || value == NULL)
// ===========================================================================
TEST(DiagBranch, EnableDisableWpReq_NullDataIndex)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t value = 0;
	// msg != NULL, data_index == NULL → second condition TRUE
	auto rc =
	    decode_enable_disable_wp_req(msg, buf.size(), nullptr, &value);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, EnableDisableWpReq_NullValue)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum diagnostics_enable_disable_wp_data_index data_index = {};
	// msg != NULL, data_index != NULL, value == NULL → third condition TRUE
	auto rc =
	    decode_enable_disable_wp_req(msg, buf.size(), &data_index, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// data_size too small — triggers NSM_SW_ERROR_DATA branch
TEST(DiagBranch, EnableDisableWpReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req), 0);
	// buf is zeroed: data_size at payload[1] = 0 < required size
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum diagnostics_enable_disable_wp_data_index data_index = {};
	uint8_t value = 0;
	auto rc =
	    decode_enable_disable_wp_req(msg, buf.size(), &data_index, &value);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_network_device_debug_info_req — secondary null conditions
// if (msg == NULL || debug_type == NULL || handle == NULL)
// ===========================================================================
TEST(DiagBranch, GetNetworkDeviceDebugInfoReq_NullDebugType)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t handle = 0;
	// msg != NULL, debug_type == NULL → second condition TRUE
	auto rc = decode_get_network_device_debug_info_req(msg, buf.size(),
							   nullptr, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetNetworkDeviceDebugInfoReq_NullHandle)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t debug_type = 0;
	// msg != NULL, debug_type != NULL, handle == NULL → third condition
	auto rc = decode_get_network_device_debug_info_req(
	    msg, buf.size(), &debug_type, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// data_size mismatch — triggers NSM_SW_ERROR_DATA
TEST(DiagBranch, GetNetworkDeviceDebugInfoReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req),
	    0);
	// Zero-filled: data_size at payload[1] = 0, expected =
	// sizeof(req)-sizeof(common_req) > 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t debug_type = 0;
	uint32_t handle = 0;
	auto rc = decode_get_network_device_debug_info_req(
	    msg, buf.size(), &debug_type, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_device_diagnostics_req — data_size mismatch branch
// ===========================================================================
TEST(DiagBranch, GetDeviceDiagnosticsReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_req), 0);
	// Zero-filled: data_size = 0, expected = sizeof(req)-sizeof(common_req)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t segment_id = 0;
	auto rc =
	    decode_get_device_diagnostics_req(msg, buf.size(), &segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_reset_network_device_req — secondary null (mode=NULL) and data_size
// ===========================================================================
TEST(DiagBranch, ResetNetworkDeviceReq_NullMode)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	// msg != NULL, mode == NULL → second condition TRUE
	auto rc = decode_reset_network_device_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, ResetNetworkDeviceReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req), 0);
	// data_size at payload[1] = 0 < sizeof(mode) = 1 → NSM_SW_ERROR_DATA
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_reset_network_device_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_network_device_log_info_req — data_size mismatch
// ===========================================================================
TEST(DiagBranch, GetNetworkDeviceLogInfoReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req),
	    0);
	// data_size = 0, expected = sizeof(req)-sizeof(common_req) > 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t record_handle = 0;
	auto rc = decode_get_network_device_log_info_req(msg, buf.size(),
							 &record_handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_erase_debug_info_req — secondary null + data_size mismatch
// ===========================================================================
TEST(DiagBranch, EraseDebugInfoReq_NullInfoType)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	// msg != NULL, info_type == NULL → second condition TRUE
	auto rc = decode_erase_debug_info_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, EraseDebugInfoReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req), 0);
	// data_size = 0 but expected = sizeof(req)-sizeof(common_req) > 0
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t info_type = 0;
	auto rc = decode_erase_debug_info_req(msg, buf.size(), &info_type);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_device_reset_statistics_req — data_size != 0 branch
// ===========================================================================
TEST(DiagBranch, GetDeviceResetStatisticsReq_DataSizeNonZero)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req),
				 0);
	// Set data_size = 1 (payload[1] = 1 ≠ 0) → NSM_SW_ERROR_DATA
	buf[sizeof(nsm_msg_hdr) + 1] = 1;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_device_reset_statistics_req(msg, buf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_device_debug_parameters_req — secondary null conditions
// if (msg == NULL || debug_configuration_type == NULL ||
//     parameter_id == NULL || parameter_sub_id == NULL)
// ===========================================================================
TEST(DiagBranch, GetDeviceDebugParametersReq_NullDebugConfigType)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_debug_parameter_id param_id = {};
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	auto rc = decode_get_device_debug_parameters_req(
	    msg, buf.size(), nullptr, &param_id, &sub_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetDeviceDebugParametersReq_NullParameterId)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cfg_type = 0;
	nsm_debug_parameter_sub_id_bitfield sub_id = {};
	auto rc = decode_get_device_debug_parameters_req(
	    msg, buf.size(), &cfg_type, nullptr, &sub_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, GetDeviceDebugParametersReq_NullParameterSubId)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cfg_type = 0;
	struct nsm_debug_parameter_id param_id = {};
	auto rc = decode_get_device_debug_parameters_req(
	    msg, buf.size(), &cfg_type, &param_id, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_device_debug_parameters_resp — cc=NULL (secondary null)
// if (msg == NULL || cc == NULL) — only msg=NULL tested elsewhere
// ===========================================================================
TEST(DiagBranch, SetDeviceDebugParametersResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	// msg != NULL, cc == NULL → second condition TRUE
	auto rc =
	    decode_set_device_debug_parameters_resp(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_reset_count_256data — counter_len != 4 branch
// if (data == NULL || counter == NULL || counter_len != 4)
// ===========================================================================
TEST(DiagBranch, ResetCount256Data_CounterLenNotFour)
{
	uint8_t data[32] = {};
	uint64_t counter[4] = {};
	// counter_len = 3 != 4 → third condition TRUE → NSM_SW_ERROR_NULL
	auto rc = decode_reset_count_256data(data, sizeof(data), counter, 3);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Valid counter_len=4 but wrong data_len — NSM_SW_ERROR_LENGTH
TEST(DiagBranch, ResetCount256Data_CounterLenFourWrongDataLen)
{
	uint8_t data[16] = {}; // 16 != 4*8=32
	uint64_t counter[4] = {};
	auto rc = decode_reset_count_256data(data, sizeof(data), counter, 4);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Encode request functions — pack_nsm_header failure (unmasked instance_id=32)
// and msg==NULL checks
// ===========================================================================
static std::vector<uint8_t> diagBuf() { return std::vector<uint8_t>(4096, 0); }

// encode_get_device_diagnostics_req — L36 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeGetDeviceDiagnosticsReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_diagnostics_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_get_device_diagnostics_resp — L83 TRUE (msg==NULL)
TEST(DiagBranch, EncodeGetDeviceDiagnosticsResp_NullMsg)
{
	uint8_t seg[4] = {};
	auto rc = encode_get_device_diagnostics_resp(0, NSM_SUCCESS, 0, seg, 0,
						     0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_device_diagnostics_resp — L139 TRUE (data_size < 1)
TEST(DiagBranch, DecodeGetDeviceDiagnosticsResp_DataSizeTooSmall)
{
	// Build buffer with cc=NSM_SUCCESS, data_size=0 → <
	// sizeof(next_segment_id)=1
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_device_diagnostics_resp),
	    0);
	// cc=0=NSM_SUCCESS (default), data_size=0 (at payload[2..3] LE16)
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t seg_data[64] = {};
	uint16_t seg_sz = 0;
	uint8_t next_seg = 0;
	auto rc = decode_get_device_diagnostics_resp(
	    msg, buf.size(), &cc, &reason, seg_data, &seg_sz, &next_seg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_get_device_reset_statistics_req — L249 TRUE (msg==NULL)
TEST(DiagBranch, EncodeGetDeviceResetStatisticsReq_NullMsg)
{
	auto rc = encode_get_device_reset_statistics_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// encode_get_device_reset_statistics_req — L258 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeGetDeviceResetStatisticsReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_reset_statistics_req(32, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_reset_network_device_req — L309 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeResetNetworkDeviceReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_reset_network_device_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// decode_reset_network_device_resp — L370 TRUE (data_size != 0)
TEST(DiagBranch, DecodeResetNetworkDeviceResp_DataSizeNonZero)
{
	// nsm_common_resp layout: command(1)+cc(1)+reserved(2)+data_size(2)=6
	// data_size is at payload[4..5]
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_resp), 0);
	// Set data_size (payload[4..5]) = 1
	buf[sizeof(nsm_msg_hdr) + 4] = 1;
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_reset_network_device_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_enable_disable_wp_req — L392 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeEnableDisableWpReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_enable_disable_wp_req(32, RETIMER_EEPROM, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// decode_enable_disable_wp_resp — L446 TRUE (data_size != 0)
TEST(DiagBranch, DecodeEnableDisableWpResp_DataSizeNonZero)
{
	// nsm_common_resp layout: command(1)+cc(1)+reserved(2)+data_size(2)=6
	// data_size is at payload[4..5]
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp) + 4, 0);
	// data_size at payload[4..5]: set to 1
	buf[sizeof(nsm_msg_hdr) + 4] = 1;
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_enable_disable_wp_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// encode_get_network_device_debug_info_req — L467 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeGetNetworkDeviceDebugInfoReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_network_device_debug_info_req(32, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_get_network_device_debug_info_resp — L532 TRUE (msg==NULL)
TEST(DiagBranch, EncodeGetNetworkDeviceDebugInfoResp_NullMsg)
{
	uint8_t seg[4] = {};
	auto rc = encode_get_network_device_debug_info_resp(0, NSM_SUCCESS, 0,
							    seg, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// encode_get_network_device_debug_info_resp — L553 TRUE (seg_data==NULL,
// cc==NSM_SUCCESS)
TEST(DiagBranch, EncodeGetNetworkDeviceDebugInfoResp_NullSegData)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_network_device_debug_info_resp(0, NSM_SUCCESS, 0,
							    nullptr, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// encode_erase_trace_req — L610 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeEraseTraceReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_erase_trace_req(32, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_erase_trace_resp — L648 TRUE (msg==NULL)
TEST(DiagBranch, EncodeEraseTraceResp_NullMsg)
{
	auto rc = encode_erase_trace_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_erase_trace_resp — L700 TRUE (data_size != sizeof(uint8_t))
TEST(DiagBranch, DecodeEraseTraceResp_DataSizeWrong)
{
	// Build resp with cc=NSM_SUCCESS and exact length, but data_size=0
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_erase_trace_resp), 0);
	// data_size at payload[2..3] = 0 != 1 → L700 TRUE
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, result = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_erase_trace_resp(msg, buf.size(), &cc, &reason, &result);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_get_network_device_log_info_req — L713 TRUE (msg==NULL)
TEST(DiagBranch, EncodeGetNetworkDeviceLogInfoReq_NullMsg)
{
	auto rc = encode_get_network_device_log_info_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// encode_get_network_device_log_info_req — L723 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeGetNetworkDeviceLogInfoReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_network_device_log_info_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_get_network_device_log_info_resp — L772 TRUE (msg==NULL)
TEST(DiagBranch, EncodeGetNetworkDeviceLogInfoResp_NullMsg)
{
	struct nsm_device_log_info_breakdown lb{};
	uint8_t log[4] = {};
	auto rc = encode_get_network_device_log_info_resp(0, NSM_SUCCESS, 0, 0,
							  lb, log, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// encode_get_network_device_log_info_resp — L816 TRUE (log_data==NULL,
// cc==NSM_SUCCESS)
TEST(DiagBranch, EncodeGetNetworkDeviceLogInfoResp_NullLogData)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_device_log_info_breakdown lb{};
	auto rc = encode_get_network_device_log_info_resp(0, NSM_SUCCESS, 0, 0,
							  lb, nullptr, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// encode_erase_debug_info_req — L886 TRUE (pack fail, iid=32)
TEST(DiagBranch, EncodeEraseDebugInfoReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_erase_debug_info_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// encode_erase_debug_info_resp — L931 TRUE (msg==NULL)
TEST(DiagBranch, EncodeEraseDebugInfoResp_NullMsg)
{
	auto rc = encode_erase_debug_info_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_device_reset_statistics_req — L273 TRUE (msg==NULL)
// ===========================================================================
TEST(DiagBranch, DecodeGetDeviceResetStatisticsReq_NullMsg)
{
	auto rc = decode_get_device_reset_statistics_req(nullptr, 10);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_device_reset_statistics_req — L278 TRUE (msg_len too small)
// nsm_common_req = 2 bytes; need buf < 5+2=7
TEST(DiagBranch, DecodeGetDeviceResetStatisticsReq_MsgLenTooSmall)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0); // only 5 bytes
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_device_reset_statistics_req(msg, buf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_enable_disable_wp_req — L415 TRUE (msg_len too small)
// nsm_enable_disable_wp_req = nsm_common_req(2)+data_index(1)+value(1)=4
// need buf < 5+4=9
// ===========================================================================
TEST(DiagBranch, DecodeEnableDisableWpReq_MsgLenTooSmall)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0); // only 5 bytes
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum diagnostics_enable_disable_wp_data_index di = RETIMER_EEPROM;
	uint8_t val = 0;
	auto rc = decode_enable_disable_wp_req(msg, buf.size(), &di, &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_erase_debug_info_resp — L986 TRUE (data_size mismatch)
// nsm_erase_debug_info_resp = nsm_common_resp(6)+result_status(1)+reserved(1)=8
// expected data_size = 8-6 = 2; set data_size = 0 (≠ 2)
// ===========================================================================
TEST(DiagBranch, DecodeEraseDebugInfoResp_DataSizeMismatch)
{
	// Exactly sizeof(nsm_msg_hdr)+sizeof(nsm_erase_debug_info_resp) = 13
	// bytes
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_erase_debug_info_resp), 0);
	// cc=NSM_SUCCESS (default 0), data_size at payload[4..5] = 0 (≠ 2)
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t result = 0;
	auto rc = decode_erase_debug_info_resp(msg, buf.size(), &cc, &reason,
					       &result);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_device_debug_parameters_req — L1007 TRUE (pack fail, iid=32)
// encode_common_req_v2 uses pack_nsm_header_v2 → fails when iid>31
// ===========================================================================
TEST(DiagBranch, EncodeGetDeviceDebugParametersReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield sub = {};
	auto rc = encode_get_device_debug_parameters_req(
	    32, DEBUG_CONFIGURATION_TYPE_L1_POWER_DATA, pid, sub, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_device_debug_parameters_resp — L1070 TRUE (pack fail, iid=32)
// Uses unmasked instance_id → pack_nsm_header fails with iid=32
// ===========================================================================
TEST(DiagBranch, EncodeGetDeviceDebugParametersResp_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[4] = {};
	uint16_t data_size = sizeof(data);
	auto rc = encode_get_device_debug_parameters_resp(
	    32, NSM_SUCCESS, 0, &data_size, data, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_get_device_debug_parameters_resp — L1111 TRUE (data_size > available)
// nsm_get_device_debug_parameters_resp = nsm_common_resp(6)+data[1]=7
// min msg_len = 5+7-1=11; use 12-byte buf; available = 12-5-6=1
// set data_size (payload[4..5]) = 255 > 1 → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(DiagBranch, DecodeGetDeviceDebugParametersResp_DataSizeTooLarge)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_device_debug_parameters_resp),
	    0);
	// data_size at payload[4..5] = 255 > available
	buf[sizeof(nsm_msg_hdr) + 4] = 255;
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0;
	uint8_t data[256] = {};
	auto rc = decode_get_device_debug_parameters_resp(msg, buf.size(), &cc,
							  &ds, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_device_debug_parameters_req — L1131 TRUE (pack fail, iid=32)
// ===========================================================================
TEST(DiagBranch, EncodeSetDeviceDebugParametersReq_PackFail)
{
	auto buf = diagBuf();
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield sub = {};
	uint8_t data[1] = {0};
	auto rc = encode_set_device_debug_parameters_req(
	    32, DEBUG_CONFIGURATION_TYPE_L1_POWER_DATA, pid, sub, 0, data, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_device_debug_parameters_req — L1160-1161 secondary nulls
// The null check is the FIRST guard; any valid msg ptr with secondary null
// triggers NSM_SW_ERROR_NULL before header unpack
// ===========================================================================
TEST(DiagBranch, DecodeSetDeviceDebugParametersReq_NullDebugConfigType)
{
	std::vector<uint8_t> buf(64, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield sub = {};
	uint8_t ds = 0;
	uint8_t *data = nullptr;
	auto rc = decode_set_device_debug_parameters_req(
	    msg, buf.size(), nullptr, &pid, &sub, &ds, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, DecodeSetDeviceDebugParametersReq_NullParameterId)
{
	std::vector<uint8_t> buf(64, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dbg_type = 0;
	nsm_debug_parameter_sub_id_bitfield sub = {};
	uint8_t ds = 0;
	uint8_t *data = nullptr;
	auto rc = decode_set_device_debug_parameters_req(
	    msg, buf.size(), &dbg_type, nullptr, &sub, &ds, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, DecodeSetDeviceDebugParametersReq_NullParameterSubId)
{
	std::vector<uint8_t> buf(64, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dbg_type = 0;
	struct nsm_debug_parameter_id pid = {};
	uint8_t ds = 0;
	uint8_t *data = nullptr;
	auto rc = decode_set_device_debug_parameters_req(
	    msg, buf.size(), &dbg_type, &pid, nullptr, &ds, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, DecodeSetDeviceDebugParametersReq_NullDataSize)
{
	std::vector<uint8_t> buf(64, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dbg_type = 0;
	struct nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield sub = {};
	uint8_t *data = nullptr;
	auto rc = decode_set_device_debug_parameters_req(
	    msg, buf.size(), &dbg_type, &pid, &sub, nullptr, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagBranch, DecodeSetDeviceDebugParametersReq_NullData)
{
	std::vector<uint8_t> buf(64, 0);
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dbg_type = 0;
	struct nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield sub = {};
	uint8_t ds = 0;
	auto rc = decode_set_device_debug_parameters_req(
	    msg, buf.size(), &dbg_type, &pid, &sub, &ds, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_device_debug_parameters_req — L1174 TRUE (reserved != 0)
// Build a valid encoded request, then corrupt reserved[0]
// ===========================================================================
TEST(DiagBranch, DecodeSetDeviceDebugParametersReq_ReservedNonZero)
{
	// encode_set_device_debug_parameters_req with iid=0 → valid header
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_set_device_debug_parameters_req) +
		4, // extra for data
	    0);
	auto *enc_msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield sub = {};
	uint8_t data_bytes[1] = {0};
	encode_set_device_debug_parameters_req(
	    0, DEBUG_CONFIGURATION_TYPE_L1_POWER_DATA, pid, sub, 0, data_bytes,
	    enc_msg);
	// Corrupt reserved[0] (offset into set_device_debug_parameters_req
	// struct): nsm_common_req_v2(6) + debug_configuration_type(1) +
	// data_size(1) → offset 8
	enc_msg->payload[8] = 1; // reserved[0] = 1 != 0
	const auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dbg_type = 0;
	struct nsm_debug_parameter_id out_pid = {};
	nsm_debug_parameter_sub_id_bitfield out_sub = {};
	uint8_t ds = 0;
	uint8_t *out_data = nullptr;
	auto rc = decode_set_device_debug_parameters_req(
	    msg, buf.size(), &dbg_type, &out_pid, &out_sub, &ds, &out_data);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ---------------------------------------------------------------------------
// Buffer-overflow guard regression (nvbug 6232725): each decoder must reject
// a message whose on-wire payload length exceeds the bytes actually present
// in msg_len, returning NSM_SW_ERROR_LENGTH instead of an out-of-bounds copy.
// ---------------------------------------------------------------------------

namespace
{
// Payload offset of the data_size field inside struct nsm_common_resp
// (command:1, completion_code:1, reserved:2, data_size:2).
constexpr size_t kRespDataSizeOff = sizeof(nsm_msg_hdr) + 4;

void putU16(std::vector<uint8_t> &buf, size_t off, uint16_t v)
{
	buf[off] = static_cast<uint8_t>(v & 0xFF);
	buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

std::vector<uint8_t> oversizedResp(size_t structSize, uint16_t dataSize)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + structSize, 0);
	putU16(buf, kRespDataSizeOff, dataSize);
	return buf;
}

const nsm_msg *asMsg(const std::vector<uint8_t> &buf)
{
	return reinterpret_cast<const nsm_msg *>(buf.data());
}
} // namespace

TEST(LibnsmOverreadGuard, GetDeviceDiagnosticsResp)
{
	auto buf =
	    oversizedResp(sizeof(nsm_get_device_diagnostics_resp), 0xFFFF);
	uint8_t cc = 0xFF;
	uint16_t rc = 0;
	uint8_t seg[8] = {0};
	uint16_t segSize = 0;
	uint8_t segId = 0;
	EXPECT_EQ(decode_get_device_diagnostics_resp(
		      asMsg(buf), buf.size(), &cc, &rc, seg, &segSize, &segId),
		  NSM_SW_ERROR_LENGTH);
}

TEST(LibnsmOverreadGuard, GetNetworkDeviceDebugInfoResp)
{
	auto buf = oversizedResp(sizeof(nsm_get_network_device_debug_info_resp),
				 0xFFFF);
	uint8_t cc = 0xFF;
	uint16_t rc = 0;
	uint16_t segSize = 0;
	uint8_t seg[8] = {0};
	uint32_t next = 0;
	EXPECT_EQ(decode_get_network_device_debug_info_resp(
		      asMsg(buf), buf.size(), &cc, &rc, &segSize, seg, &next),
		  NSM_SW_ERROR_LENGTH);
}

TEST(LibnsmOverreadGuard, GetNetworkDeviceLogInfoResp)
{
	auto buf =
	    oversizedResp(sizeof(nsm_get_network_device_log_info_resp), 0xFFFF);
	uint8_t cc = 0xFF;
	uint16_t rc = 0;
	uint32_t next = 0;
	// Backing store comfortably larger than nsm_device_log_info, which
	// the decoder writes before reaching the guarded log_data copy.
	std::vector<uint8_t> logInfo(128, 0);
	auto *li =
	    reinterpret_cast<nsm_device_log_info_breakdown *>(logInfo.data());
	uint8_t logData[8] = {0};
	uint16_t logSize = 0;
	EXPECT_EQ(
	    decode_get_network_device_log_info_resp(
		asMsg(buf), buf.size(), &cc, &rc, &next, li, logData, &logSize),
	    NSM_SW_ERROR_LENGTH);
}

TEST(LibnsmOverreadGuard, SetDeviceDebugParametersReq)
{
	// Uses decode_common_req_v2, so a valid v2 request header is
	// required: pci_vendor_id (big-endian 0x10DE), request bit set,
	// ocp_type=8, ocp_version=9.
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) -
		1,
	    0);
	buf[0] = 0x10;
	buf[1] = 0xDE;
	buf[2] = 0x80; // request bit
	buf[3] = (OCP_TYPE << 4) | OCP_VERSION;
	buf[sizeof(nsm_msg_hdr) + 7] = 0xFF; // struct data_size = 255
	uint8_t dct = 0;
	nsm_debug_parameter_id pid = {};
	nsm_debug_parameter_sub_id_bitfield psub = {};
	uint8_t dsz = 0;
	uint8_t *dptr = nullptr;
	EXPECT_EQ(decode_set_device_debug_parameters_req(
		      asMsg(buf), buf.size(), &dct, &pid, &psub, &dsz, &dptr),
		  NSM_SW_ERROR_LENGTH);
}
