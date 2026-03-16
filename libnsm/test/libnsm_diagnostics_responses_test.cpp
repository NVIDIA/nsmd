/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "base.h"
#include "diagnostics.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// Tests for encode_enable_disable_wp_resp
TEST(DiagnosticsResponsesTest, EncodeEnableDisableWpRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 7;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x1234;

	int rc =
	    encode_enable_disable_wp_resp(instance_id, cc, reason_code, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_ENABLE_DISABLE_WP);
	EXPECT_EQ(resp->completion_code, cc);
}

TEST(DiagnosticsResponsesTest, EncodeEnableDisableWpRespNullMsg)
{
	uint8_t instance_id = 7;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x1234;

	int rc = encode_enable_disable_wp_resp(instance_id, cc, reason_code,
					       nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsResponsesTest, EncodeEnableDisableWpRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 7;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0xABCD;

	int rc =
	    encode_enable_disable_wp_resp(instance_id, cc, reason_code, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(resp->completion_code, cc);
}

// Tests for encode_reset_network_device_resp
TEST(DiagnosticsResponsesTest, EncodeResetNetworkDeviceRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_reset_network_device_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint16_t reason_code = 0x5678;

	int rc =
	    encode_reset_network_device_resp(instance_id, reason_code, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<nsm_reset_network_device_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_RESET_NETWORK_DEVICE);
	EXPECT_EQ(resp->completion_code, NSM_SUCCESS);
}

TEST(DiagnosticsResponsesTest, EncodeResetNetworkDeviceRespNullMsg)
{
	uint8_t instance_id = 3;
	uint16_t reason_code = 0x5678;

	int rc =
	    encode_reset_network_device_resp(instance_id, reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_reset_network_device_resp
TEST(DiagnosticsResponsesTest, DecodeResetNetworkDeviceRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_reset_network_device_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint16_t reason_code = 0x9ABC;

	encode_reset_network_device_resp(instance_id, reason_code, msg);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_resp);

	int rc = decode_reset_network_device_resp(msg, msg_len, &decoded_cc,
						  &decoded_reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_cc, NSM_SUCCESS);
	// Note: reason_code is only encoded when cc != NSM_SUCCESS
	// For success responses, reason_code is not meaningful
}

TEST(DiagnosticsResponsesTest, DecodeResetNetworkDeviceRespNullMsg)
{
	uint8_t cc;
	uint16_t reason_code;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_resp);

	int rc = decode_reset_network_device_resp(nullptr, msg_len, &cc,
						  &reason_code);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsResponsesTest, DecodeResetNetworkDeviceRespNullOutputs)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_reset_network_device_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 3;
	uint16_t reason_code = 0x1111;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_resp);

	encode_reset_network_device_resp(instance_id, reason_code, msg);

	int rc =
	    decode_reset_network_device_resp(msg, msg_len, nullptr, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_erase_trace_resp
TEST(DiagnosticsResponsesTest, EncodeEraseTraceRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 9;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x2222;
	uint8_t result_status = 0x01;

	int rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					 result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<struct nsm_erase_trace_resp *>(msg->payload);
	EXPECT_EQ(resp->hdr.command, NSM_ERASE_TRACE);
	EXPECT_EQ(resp->hdr.completion_code, cc);
	EXPECT_EQ(resp->result_status, result_status);
}

TEST(DiagnosticsResponsesTest, EncodeEraseTraceRespNullMsg)
{
	uint8_t instance_id = 9;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x2222;
	uint8_t result_status = 0x01;

	int rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					 result_status, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsResponsesTest, EncodeEraseTraceRespErrorStatus)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 9;
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0x3333;
	uint8_t result_status = 0xFF;

	// When cc != NSM_SUCCESS, function returns early with only reason_code
	// encoded
	int rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					 result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp = reinterpret_cast<struct nsm_common_resp *>(msg->payload);
	EXPECT_EQ(resp->completion_code, cc);
	// result_status is NOT set when cc indicates error
}

// Tests for decode_erase_trace_resp
TEST(DiagnosticsResponsesTest, DecodeEraseTraceRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 9;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x4444;
	uint8_t result_status = 0x02;

	encode_erase_trace_resp(instance_id, cc, reason_code, result_status,
				msg);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0;
	uint8_t decoded_result_status = 0;
	size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp);

	int rc = decode_erase_trace_resp(msg, msg_len, &decoded_cc,
					 &decoded_reason_code,
					 &decoded_result_status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_cc, cc);
	// Note: reason_code only meaningful when cc != NSM_SUCCESS
	EXPECT_EQ(decoded_result_status, result_status);
}

TEST(DiagnosticsResponsesTest, DecodeEraseTraceRespNullMsg)
{
	uint8_t cc, result_status;
	uint16_t reason_code;
	size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp);

	int rc = decode_erase_trace_resp(nullptr, msg_len, &cc, &reason_code,
					 &result_status);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsResponsesTest, DecodeEraseTraceRespNullResultStatus)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 9;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x5555;
	uint8_t result_status = 0x03;
	size_t msg_len = sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp);

	encode_erase_trace_resp(instance_id, cc, reason_code, result_status,
				msg);

	uint8_t decoded_cc;
	uint16_t decoded_reason_code;

	int rc = decode_erase_trace_resp(msg, msg_len, &decoded_cc,
					 &decoded_reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_erase_debug_info_resp
TEST(DiagnosticsResponsesTest, EncodeEraseDebugInfoRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 11;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x6666;
	uint8_t result_status = 0x04;

	int rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					      result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	auto *resp =
	    reinterpret_cast<struct nsm_erase_debug_info_resp *>(msg->payload);
	EXPECT_EQ(resp->hdr.command, NSM_ERASE_DEBUG_INFO);
	EXPECT_EQ(resp->hdr.completion_code, cc);
	EXPECT_EQ(resp->result_status, result_status);
}

TEST(DiagnosticsResponsesTest, EncodeEraseDebugInfoRespNullMsg)
{
	uint8_t instance_id = 11;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x6666;
	uint8_t result_status = 0x04;

	int rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					      result_status, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsResponsesTest, EncodeEraseDebugInfoRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x15;
	uint8_t result_status = 0x07;

	// Test error response with reason code
	int rc = encode_erase_debug_info_resp(instance_id, NSM_ERR_INVALID_DATA,
					      0xABCD, result_status, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DIAGNOSTIC);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_ERASE_TRACE);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0xABCD);
}

// Tests for decode_erase_debug_info_resp
TEST(DiagnosticsResponsesTest, DecodeEraseDebugInfoRespValid)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 11;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x7777;
	uint8_t result_status = 0x05;

	encode_erase_debug_info_resp(instance_id, cc, reason_code,
				     result_status, msg);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0;
	uint8_t decoded_result_status = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_resp);

	int rc = decode_erase_debug_info_resp(msg, msg_len, &decoded_cc,
					      &decoded_reason_code,
					      &decoded_result_status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_cc, cc);
	// Note: reason_code only meaningful when cc != NSM_SUCCESS
	EXPECT_EQ(decoded_result_status, result_status);
}

TEST(DiagnosticsResponsesTest, DecodeEraseDebugInfoRespNullMsg)
{
	uint8_t cc, result_status;
	uint16_t reason_code;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_resp);

	int rc = decode_erase_debug_info_resp(nullptr, msg_len, &cc,
					      &reason_code, &result_status);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsResponsesTest, DecodeEraseDebugInfoRespNullResultStatus)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 11;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0x8888;
	uint8_t result_status = 0x06;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_resp);

	encode_erase_debug_info_resp(instance_id, cc, reason_code,
				     result_status, msg);

	uint8_t decoded_cc;
	uint16_t decoded_reason_code;

	int rc = decode_erase_debug_info_resp(msg, msg_len, &decoded_cc,
					      &decoded_reason_code, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_get_device_diagnostics_resp
TEST(DiagnosticsResponsesTest, EncodeGetDeviceDiagnosticsRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x12;
	std::vector<uint8_t> segment_data{0x01, 0x02, 0x03};
	uint8_t next_segment_id = 0x05;

	// Test error response with reason code
	int rc = encode_get_device_diagnostics_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0x1234, segment_data.data(),
	    segment_data.size(), next_segment_id, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DIAGNOSTIC);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_DEVICE_DIAGNOSTICS);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0x1234);
}

// Tests for encode_get_network_device_debug_info_resp
TEST(DiagnosticsResponsesTest, EncodeGetNetworkDeviceDebugInfoRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x13;
	std::vector<uint8_t> segment_data{0xAA, 0xBB, 0xCC};
	uint32_t next_handle = 0x12345678;

	// Test error response with reason code
	int rc = encode_get_network_device_debug_info_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0x5678, segment_data.data(),
	    segment_data.size(), next_handle, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DIAGNOSTIC);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_NETWORK_DEVICE_DEBUG_INFO);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0x5678);
}

// Tests for encode_get_network_device_log_info_resp
TEST(DiagnosticsResponsesTest, EncodeGetNetworkDeviceLogInfoRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x14;
	uint32_t next_handle = 0x87654321;
	struct nsm_device_log_info_breakdown log_info_breakdown = {};
	std::vector<uint8_t> log_data{0x11, 0x22, 0x33, 0x44};

	// Test error response with reason code
	int rc = encode_get_network_device_log_info_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0x9ABC, next_handle,
	    log_info_breakdown, log_data.data(), log_data.size(), msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DIAGNOSTIC);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_NETWORK_DEVICE_LOG_INFO);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0x9ABC);
}

// Tests for encode_get_device_debug_parameters_resp
TEST(DiagnosticsResponsesTest, EncodeGetDeviceDebugParametersRespErrorCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x16;
	std::vector<uint8_t> data{0xDE, 0xAD, 0xBE, 0xEF};
	uint16_t data_size = data.size();

	// Test error response with reason code
	int rc = encode_get_device_debug_parameters_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0xDEF0, &data_size, data.data(),
	    msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DIAGNOSTIC);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_DEVICE_DEBUG_PARAMETERS);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0xDEF0);
}
