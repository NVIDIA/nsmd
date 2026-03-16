/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "base.h"
#include "diagnostics.h"

#include <array>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// Tests for encode_get_device_diagnostics_req
TEST(DiagnosticsEncodeDecodeTest, EncodeGetDeviceDiagnosticsReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_device_diagnostics_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 5;
	uint8_t segment_id = 10;

	int rc =
	    encode_get_device_diagnostics_req(instance_id, segment_id, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_get_device_diagnostics_req *>(
		msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_GET_DEVICE_DIAGNOSTICS);
	EXPECT_EQ(request->segment_id, segment_id);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeGetDeviceDiagnosticsReqNullMsg)
{
	uint8_t instance_id = 5;
	uint8_t segment_id = 10;

	int rc =
	    encode_get_device_diagnostics_req(instance_id, segment_id, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_get_device_diagnostics_req
TEST(DiagnosticsEncodeDecodeTest, DecodeGetDeviceDiagnosticsReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_device_diagnostics_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 5;
	uint8_t segment_id = 10;

	encode_get_device_diagnostics_req(instance_id, segment_id, msg);

	uint8_t decoded_segment_id = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_device_diagnostics_req);

	int rc = decode_get_device_diagnostics_req(msg, msg_len,
						   &decoded_segment_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_segment_id, segment_id);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetDeviceDiagnosticsReqNullMsg)
{
	uint8_t segment_id = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_device_diagnostics_req);

	int rc =
	    decode_get_device_diagnostics_req(nullptr, msg_len, &segment_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetDeviceDiagnosticsReqNullOutput)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_device_diagnostics_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_device_diagnostics_req);

	int rc = decode_get_device_diagnostics_req(msg, msg_len, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetDeviceDiagnosticsReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_device_diagnostics_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t segment_id = 0;
	size_t msg_len = 10; // Too short

	int rc = decode_get_device_diagnostics_req(msg, msg_len, &segment_id);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_reset_network_device_req
TEST(DiagnosticsEncodeDecodeTest, EncodeResetNetworkDeviceReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_reset_network_device_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 3;
	uint8_t mode = START_AFTER_RESPONSE;

	int rc = encode_reset_network_device_req(instance_id, mode, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request = reinterpret_cast<struct nsm_reset_network_device_req *>(
	    msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_RESET_NETWORK_DEVICE);
	EXPECT_EQ(request->mode, mode);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeResetNetworkDeviceReqAllModes)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_reset_network_device_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 3;

	std::array<uint8_t, 4> modes = {
	    START_AFTER_RESPONSE, ALL_HOST_PERST_LOW,
	    ALL_HOST_PCIE_LINK_DISABLE, ALLOWED_BY_ALL_HOST};

	for (auto mode : modes) {
		int rc =
		    encode_reset_network_device_req(instance_id, mode, msg);
		EXPECT_EQ(rc, NSM_SW_SUCCESS);

		auto *request =
		    reinterpret_cast<struct nsm_reset_network_device_req *>(
			msg->payload);
		EXPECT_EQ(request->mode, mode);
	}
}

TEST(DiagnosticsEncodeDecodeTest, EncodeResetNetworkDeviceReqNullMsg)
{
	uint8_t instance_id = 3;
	uint8_t mode = START_AFTER_RESPONSE;

	int rc = encode_reset_network_device_req(instance_id, mode, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_reset_network_device_req
TEST(DiagnosticsEncodeDecodeTest, DecodeResetNetworkDeviceReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_reset_network_device_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 3;
	uint8_t mode = ALL_HOST_PERST_LOW;

	encode_reset_network_device_req(instance_id, mode, msg);

	uint8_t decoded_mode = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_reset_network_device_req);

	int rc = decode_reset_network_device_req(msg, msg_len, &decoded_mode);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_mode, mode);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeResetNetworkDeviceReqNullMsg)
{
	uint8_t mode = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_reset_network_device_req);

	int rc = decode_reset_network_device_req(nullptr, msg_len, &mode);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeResetNetworkDeviceReqNullOutput)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_reset_network_device_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_reset_network_device_req);

	int rc = decode_reset_network_device_req(msg, msg_len, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_enable_disable_wp_req
TEST(DiagnosticsEncodeDecodeTest, EncodeEnableDisableWpReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_enable_disable_wp_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 7;
	auto data_index = RETIMER_EEPROM;
	uint8_t value = 1; // Enable

	int rc =
	    encode_enable_disable_wp_req(instance_id, data_index, value, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_enable_disable_wp_req *>(msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_ENABLE_DISABLE_WP);
	EXPECT_EQ(request->data_index, data_index);
	EXPECT_EQ(request->value, value);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeEnableDisableWpReqDisable)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_enable_disable_wp_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 7;
	auto data_index = GPU_SPI_FLASH;
	uint8_t value = 0; // Disable

	int rc =
	    encode_enable_disable_wp_req(instance_id, data_index, value, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_enable_disable_wp_req *>(msg->payload);
	EXPECT_EQ(request->value, value);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeEnableDisableWpReqNullMsg)
{
	uint8_t instance_id = 7;
	auto data_index = RETIMER_EEPROM;
	uint8_t value = 1;

	int rc = encode_enable_disable_wp_req(instance_id, data_index, value,
					      nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_enable_disable_wp_req
TEST(DiagnosticsEncodeDecodeTest, DecodeEnableDisableWpReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_enable_disable_wp_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 7;
	auto data_index = HMC_SPI_FLASH;
	uint8_t value = 1;

	encode_enable_disable_wp_req(instance_id, data_index, value, msg);

	diagnostics_enable_disable_wp_data_index decoded_index;
	uint8_t decoded_value = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_enable_disable_wp_req);

	int rc = decode_enable_disable_wp_req(msg, msg_len, &decoded_index,
					      &decoded_value);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_index, data_index);
	EXPECT_EQ(decoded_value, value);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeEnableDisableWpReqNullMsg)
{
	diagnostics_enable_disable_wp_data_index data_index;
	uint8_t value = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_enable_disable_wp_req);

	int rc =
	    decode_enable_disable_wp_req(nullptr, msg_len, &data_index, &value);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_erase_trace_req
TEST(DiagnosticsEncodeDecodeTest, EncodeEraseTraceReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 9;

	int rc = encode_erase_trace_req(instance_id, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_erase_trace_req *>(msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_ERASE_TRACE);
	EXPECT_EQ(request->hdr.data_size, 0x00);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeEraseTraceReqNullMsg)
{
	uint8_t instance_id = 9;

	int rc = encode_erase_trace_req(instance_id, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_erase_trace_req
TEST(DiagnosticsEncodeDecodeTest, DecodeEraseTraceReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 9;

	encode_erase_trace_req(instance_id, msg);

	size_t msg_len =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_erase_trace_req);

	int rc = decode_erase_trace_req(msg, msg_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeEraseTraceReqNullMsg)
{
	size_t msg_len =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_erase_trace_req);

	int rc = decode_erase_trace_req(nullptr, msg_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeEraseTraceReqInvalidLength)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len = 5; // Too short (need 7 bytes: 5 for hdr + 2 for req)

	int rc = decode_erase_trace_req(msg, msg_len);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for encode_erase_debug_info_req
TEST(DiagnosticsEncodeDecodeTest, EncodeEraseDebugInfoReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 11;
	uint8_t info_type = INFO_TYPE_FW_SAVED_DUMP_INFO;

	int rc = encode_erase_debug_info_req(instance_id, info_type, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_erase_debug_info_req *>(msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_ERASE_DEBUG_INFO);
	EXPECT_EQ(request->debug_info_type, info_type);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeEraseDebugInfoReqNullMsg)
{
	uint8_t instance_id = 11;
	uint8_t info_type = INFO_TYPE_FW_SAVED_DUMP_INFO;

	int rc = encode_erase_debug_info_req(instance_id, info_type, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_erase_debug_info_req
TEST(DiagnosticsEncodeDecodeTest, DecodeEraseDebugInfoReqValid)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 11;
	uint8_t info_type = INFO_TYPE_FW_SAVED_DUMP_INFO;

	encode_erase_debug_info_req(instance_id, info_type, msg);

	uint8_t decoded_info_type = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_erase_debug_info_req);

	int rc = decode_erase_debug_info_req(msg, msg_len, &decoded_info_type);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_info_type, info_type);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeEraseDebugInfoReqNullMsg)
{
	uint8_t info_type = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_erase_debug_info_req);

	int rc = decode_erase_debug_info_req(nullptr, msg_len, &info_type);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeEraseDebugInfoReqNullOutput)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_erase_debug_info_req);

	int rc = decode_erase_debug_info_req(msg, msg_len, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_get_network_device_log_info_req
TEST(DiagnosticsEncodeDecodeTest, EncodeGetNetworkDeviceLogInfoReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 13;
	uint32_t record_handle = 0x12345678;

	int rc = encode_get_network_device_log_info_req(instance_id,
							record_handle, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_get_network_device_log_info_req *>(
		msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_GET_NETWORK_DEVICE_LOG_INFO);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeGetNetworkDeviceLogInfoReqZeroHandle)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 13;
	uint32_t record_handle = 0;

	int rc = encode_get_network_device_log_info_req(instance_id,
							record_handle, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeGetNetworkDeviceLogInfoReqNullMsg)
{
	uint8_t instance_id = 13;
	uint32_t record_handle = 0x12345678;

	int rc = encode_get_network_device_log_info_req(instance_id,
							record_handle, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_get_network_device_log_info_req
TEST(DiagnosticsEncodeDecodeTest, DecodeGetNetworkDeviceLogInfoReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 13;
	uint32_t record_handle = 0xABCDEF01;

	encode_get_network_device_log_info_req(instance_id, record_handle, msg);

	uint32_t decoded_handle = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_network_device_log_info_req);

	int rc = decode_get_network_device_log_info_req(msg, msg_len,
							&decoded_handle);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_handle, record_handle);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetNetworkDeviceLogInfoReqNullMsg)
{
	uint32_t record_handle = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_network_device_log_info_req);

	int rc = decode_get_network_device_log_info_req(nullptr, msg_len,
							&record_handle);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetNetworkDeviceLogInfoReqNullOutput)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_network_device_log_info_req);

	int rc = decode_get_network_device_log_info_req(msg, msg_len, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for encode_get_network_device_debug_info_req
TEST(DiagnosticsEncodeDecodeTest, EncodeGetNetworkDeviceDebugInfoReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 15;
	uint8_t debug_type = INFO_TYPE_DEVICE_INFO;
	uint32_t handle = 0x11223344;

	int rc = encode_get_network_device_debug_info_req(
	    instance_id, debug_type, handle, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *request =
	    reinterpret_cast<struct nsm_get_network_device_debug_info_req *>(
		msg->payload);
	EXPECT_EQ(request->hdr.command, NSM_GET_NETWORK_DEVICE_DEBUG_INFO);
	EXPECT_EQ(request->debug_info_type, debug_type);
}

TEST(DiagnosticsEncodeDecodeTest, EncodeGetNetworkDeviceDebugInfoReqAllTypes)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 15;
	uint32_t handle = 0x11223344;

	std::array<uint8_t, 4> types = {
	    INFO_TYPE_DEVICE_INFO, INFO_TYPE_FW_RUNTIME_INFO,
	    INFO_TYPE_FW_SAVED_INFO, INFO_TYPE_DEVICE_DUMP};

	for (auto debug_type : types) {
		int rc = encode_get_network_device_debug_info_req(
		    instance_id, debug_type, handle, msg);
		EXPECT_EQ(rc, NSM_SW_SUCCESS);

		auto *request = reinterpret_cast<
		    struct nsm_get_network_device_debug_info_req *>(
		    msg->payload);
		EXPECT_EQ(request->debug_info_type, debug_type);
	}
}

TEST(DiagnosticsEncodeDecodeTest, EncodeGetNetworkDeviceDebugInfoReqNullMsg)
{
	uint8_t instance_id = 15;
	uint8_t debug_type = INFO_TYPE_DEVICE_INFO;
	uint32_t handle = 0x11223344;

	int rc = encode_get_network_device_debug_info_req(
	    instance_id, debug_type, handle, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for decode_get_network_device_debug_info_req
TEST(DiagnosticsEncodeDecodeTest, DecodeGetNetworkDeviceDebugInfoReqValid)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 15;
	uint8_t debug_type = INFO_TYPE_FW_RUNTIME_INFO;
	uint32_t handle = 0x55667788;

	encode_get_network_device_debug_info_req(instance_id, debug_type,
						 handle, msg);

	uint8_t decoded_type = 0;
	uint32_t decoded_handle = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_network_device_debug_info_req);

	int rc = decode_get_network_device_debug_info_req(
	    msg, msg_len, &decoded_type, &decoded_handle);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_type, debug_type);
	EXPECT_EQ(decoded_handle, handle);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetNetworkDeviceDebugInfoReqNullMsg)
{
	uint8_t debug_type = 0;
	uint32_t handle = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_network_device_debug_info_req);

	int rc = decode_get_network_device_debug_info_req(nullptr, msg_len,
							  &debug_type, &handle);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DiagnosticsEncodeDecodeTest, DecodeGetNetworkDeviceDebugInfoReqNullOutputs)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));
	auto *msg = reinterpret_cast<struct nsm_msg *>(requestMsg.data());
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_network_device_debug_info_req);

	int rc1 = decode_get_network_device_debug_info_req(msg, msg_len,
							   nullptr, nullptr);
	EXPECT_EQ(rc1, NSM_SW_ERROR_NULL);

	uint8_t debug_type = 0;
	int rc2 = decode_get_network_device_debug_info_req(
	    msg, msg_len, &debug_type, nullptr);
	EXPECT_EQ(rc2, NSM_SW_ERROR_NULL);

	uint32_t handle = 0;
	int rc3 = decode_get_network_device_debug_info_req(msg, msg_len,
							   nullptr, &handle);
	EXPECT_EQ(rc3, NSM_SW_ERROR_NULL);
}
