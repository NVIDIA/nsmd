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

#include "base.h"
#include "diagnostics.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(ResetMetrics, EncodeResetEnumData)
{
	uint8_t resetType = 5; // Example enum value
	uint8_t data[1];
	size_t data_len;

	auto rc = encode_reset_enum_data(resetType, data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data[0], resetType);
	EXPECT_EQ(data_len, sizeof(uint8_t));
}

TEST(ResetMetrics, EncodeResetEnumDataNull)
{
	uint8_t resetType = 5;
	size_t data_len;

	auto rc = encode_reset_enum_data(resetType, nullptr, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_reset_enum_data(resetType, reinterpret_cast<uint8_t *>(0),
				    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for `encode_reset_count_data`
TEST(ResetMetrics, EncodeResetCountData)
{
	uint16_t count = 256; // Example count value
	uint8_t data[2];
	size_t data_len;

	auto rc = encode_reset_count_data(count, data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint16_t));

	uint16_t decodedCount;
	memcpy(&decodedCount, data, sizeof(uint16_t));
	EXPECT_EQ(decodedCount, htole16(count));
}

TEST(ResetMetrics, EncodeResetCountDataNull)
{
	uint16_t count = 256;
	size_t data_len;

	auto rc = encode_reset_count_data(count, nullptr, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_reset_count_data(count, reinterpret_cast<uint8_t *>(0),
				     nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for `decode_reset_enum_data`
TEST(ResetMetrics, DecodeResetEnumData)
{
	uint8_t resetType = 5; // Example encoded value
	uint8_t data[] = {resetType};
	uint8_t decodedResetType;

	auto rc = decode_reset_enum_data(data, sizeof(data), &decodedResetType);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decodedResetType, resetType);
}

TEST(ResetMetrics, DecodeResetEnumDataInvalidLength)
{
	uint8_t data[] = {5};
	uint8_t decodedResetType;

	auto rc =
	    decode_reset_enum_data(data, sizeof(data) - 1, &decodedResetType);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// Tests for `encode_reset_count_256data`
TEST(ResetMetrics, EncodeResetCount256Data)
{
	std::array<uint64_t, 4> count = {256, 256, 256,
					 256}; // Example count value
	uint8_t data[32];
	size_t data_len = 0;

	auto rc = encode_reset_count_256data(count.data(), data, &data_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(data_len, sizeof(uint64_t) * 4);

	std::array<uint64_t, 4> decodedCount;
	memcpy(decodedCount.data(), data, sizeof(uint64_t) * 4);
	EXPECT_EQ(decodedCount, count);
}

TEST(ResetMetrics, EncodeResetCount256DataNull)
{
	std::array<uint64_t, 4> count = {256, 256, 256, 256};
	size_t data_len;

	auto rc = encode_reset_count_256data(count.data(), nullptr, &data_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_reset_count_256data(
	    count.data(), reinterpret_cast<uint8_t *>(0), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for `decode_reset_count_data`
TEST(ResetMetrics, DecodeResetCountData)
{
	uint16_t count = 256; // Example encoded value
	uint8_t data[2];
	memcpy(data, &count, sizeof(count));
	uint16_t decodedCount;

	auto rc = decode_reset_count_data(data, sizeof(data), &decodedCount);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decodedCount, le16toh(count));
}

TEST(ResetMetrics, DecodeResetCountDataInvalidLength)
{
	uint16_t count = 256;
	uint8_t data[2];
	memcpy(data, &count, sizeof(count));
	uint16_t decodedCount;

	auto rc =
	    decode_reset_count_data(data, sizeof(data) - 1, &decodedCount);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(ResetMetrics, DecodeResetCountDataNull)
{
	uint16_t decodedCount;
	auto rc = decode_reset_count_data(nullptr, 2, &decodedCount);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t data[2] = {0};
	rc = decode_reset_count_data(data, 2, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Tests for `decode_reset_count_256data`
TEST(ResetMetrics, DecodeResetCount256Data)
{
	std::array<uint64_t, 4> count = {256, 256, 256,
					 256}; // Example encoded value
	uint8_t data[32];
	memcpy(data, count.data(), sizeof(count));
	std::array<uint64_t, 4> decodedCount;

	auto rc = decode_reset_count_256data(data, sizeof(data),
					     decodedCount.data(), 4);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decodedCount, count);
	EXPECT_EQ(decodedCount[0], le64toh(count[0]));
	EXPECT_EQ(decodedCount[1], le64toh(count[1]));
	EXPECT_EQ(decodedCount[2], le64toh(count[2]));
	EXPECT_EQ(decodedCount[3], le64toh(count[3]));
}

TEST(ResetMetrics, DecodeResetCount256DataInvalidLength)
{
	std::array<uint64_t, 4> count = {256, 256, 256,
					 256}; // Example encoded value
	uint8_t data[32];
	memcpy(data, count.data(), sizeof(count));
	std::array<uint64_t, 4> decodedCount;

	auto rc = decode_reset_count_256data(data, sizeof(data) - 1,
					     decodedCount.data(), 4);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(ResetMetrics, DecodeResetCount256DataNull)
{
	std::array<uint64_t, 4> decodedCount;
	auto rc =
	    decode_reset_count_256data(nullptr, 32, decodedCount.data(), 4);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t data[32] = {0};
	rc = decode_reset_count_256data(data, 32, nullptr, 4);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDiagnostics, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_enable_disable_wp_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	diagnostics_enable_disable_wp_data_index data_index = GPU_1_4_SPI_FLASH;
	uint8_t value = 0;

	auto rc = encode_enable_disable_wp_req(0, data_index, value, request);

	struct nsm_enable_disable_wp_req *req =
	    (struct nsm_enable_disable_wp_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ENABLE_DISABLE_WP, req->hdr.command);
	EXPECT_EQ(2, req->hdr.data_size);
	EXPECT_EQ(data_index, req->data_index);
	EXPECT_EQ(value, req->value);
}

TEST(getDiagnostics, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		   // PCI VID: NVIDIA 0x10DE
	    0x80,		   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_WP, // command
	    2,			   // data size
	    GPU_1_4_SPI_FLASH,	   // data_index
	    1,			   // set
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	diagnostics_enable_disable_wp_data_index data_index;
	uint8_t value = 0;
	auto rc =
	    decode_enable_disable_wp_req(request, msg_len, &data_index, &value);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(GPU_1_4_SPI_FLASH, data_index);
	EXPECT_EQ(1, value);
}

TEST(getDiagnostics, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_enable_disable_wp_resp(0, NSM_SUCCESS, reason_code,
						response);

	auto resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ENABLE_DISABLE_WP, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(getDiagnostics, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		   // PCI VID: NVIDIA 0x10DE
	    0x00,		   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_WP, // command
	    0,			   // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc =
	    decode_enable_disable_wp_resp(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(getDiagnostics, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		   // PCI VID: NVIDIA 0x10DE
	    0x00,		   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_WP, // command
	    0,			   // completion code
	    0,
	    0,
	    1, // incorrect data size
	    0, // data size
	    0, // invalid data byte
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_enable_disable_wp_resp(NULL, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_wp_resp(response, msg_len, NULL,
					   &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_wp_resp(response, msg_len, &cc, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc =
	    decode_enable_disable_wp_resp(response, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_enable_disable_wp_resp(response, msg_len - 1, &cc,
					   &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(resetNetworkDevice, testGoodEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_reset_network_device_req));

	uint8_t mode = 1;
	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	auto rc = encode_reset_network_device_req(0, mode, request);

	struct nsm_reset_network_device_req *req =
	    reinterpret_cast<struct nsm_reset_network_device_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_RESET_NETWORK_DEVICE, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(1, req->mode);
}

TEST(resetNetworkDevice, testBadEncodeRequest)
{
	std::vector<uint8_t> request_msg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_reset_network_device_req));

	auto rc = encode_reset_network_device_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(resetNetworkDevice, testGoodDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    0x80,		      // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_DIAGNOSTIC,      // NVIDIA_MSG_TYPE
	    NSM_RESET_NETWORK_DEVICE, // command
	    1,			      // data size
	    3			      // mode
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	size_t msg_len = request_msg.size();
	uint8_t mode = 0;

	auto rc = decode_reset_network_device_req(request, msg_len, &mode);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(3, mode);
}

TEST(resetNetworkDevice, testBadDecodeRequest)
{
	std::vector<uint8_t> request_msg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    0x80,		      // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=1, OCP_VER=1, OCP=1
	    NSM_TYPE_DIAGNOSTIC,      // NVIDIA_MSG_TYPE
	    NSM_RESET_NETWORK_DEVICE, // command
	    0,			      // data size [it should be 1]
	    3			      // mode
	};

	auto request = reinterpret_cast<nsm_msg *>(request_msg.data());
	uint8_t mode = 0;
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_reset_network_device_req);

	auto rc = decode_reset_network_device_req(nullptr, 0, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_reset_network_device_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_reset_network_device_req(request, 0, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_reset_network_device_req(request, msg_len, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(resetNetworkDevice, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_reset_network_device_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x12;
	uint16_t reason_code = 0;
	auto rc = encode_reset_network_device_resp(instance_id, reason_code,
						   response);

	nsm_reset_network_device_resp *resp =
	    reinterpret_cast<nsm_reset_network_device_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_RESET_NETWORK_DEVICE, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(resetNetworkDevice, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    0x00,		      // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,      // NVIDIA_MSG_TYPE
	    NSM_RESET_NETWORK_DEVICE, // command
	    0,			      // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_reset_network_device_resp(response, msg_len, &cc,
						   &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(getNetworkDeviceDebugInfo, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t debug_type = 0;
	uint32_t handle = 2;
	uint8_t data_size =
	    sizeof(struct nsm_get_network_device_debug_info_req) -
	    sizeof(struct nsm_common_req);

	auto rc = encode_get_network_device_debug_info_req(0, debug_type,
							   handle, request);

	struct nsm_get_network_device_debug_info_req *req =
	    reinterpret_cast<struct nsm_get_network_device_debug_info_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_DEBUG_INFO, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
	EXPECT_EQ(debug_type, req->debug_info_type);
	EXPECT_EQ(htole32(handle), req->record_handle);
}

TEST(getNetworkDeviceDebugInfo, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));

	auto rc = encode_get_network_device_debug_info_req(0, 0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getNetworkDeviceDebugInfo, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    0x06,			       // data size
	    0x01,			       // debug_info_type
	    0x00,			       // reserved
	    0x03,			       // record_handle
	    0x00,
	    0x00,
	    0x00};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t debug_type;
	uint32_t handle;
	auto rc = decode_get_network_device_debug_info_req(
	    request, msg_len, &debug_type, &handle);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(01, debug_type);
	EXPECT_EQ(03, handle);
}

TEST(getNetworkDeviceDebugInfo, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    0x10, // data size [it shouldn't be 0x10]
	    0x01, // debug_info_type
	    0x00, // reserved
	    0x03, // record_handle
	    0x00,
	    0x00,
	    0x00};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t debug_type;
	uint32_t handle;
	auto rc = decode_get_network_device_debug_info_req(
	    nullptr, msg_len, &debug_type, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_debug_info_req(request, msg_len, nullptr,
						      &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_debug_info_req(request, msg_len,
						      &debug_type, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_debug_info_req(request, msg_len - 2,
						      &debug_type, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_network_device_debug_info_req(request, msg_len,
						      &debug_type, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getNetworkDeviceDebugInfo, testGoodEncodeResponse)
{
	// this is some dummy data segment with random size
	std::vector<uint8_t> segment_data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00};
	uint16_t reason_code = ERR_NULL;
	uint32_t nxt_handle = 01;
	uint8_t data_size = segment_data.size() + sizeof(nxt_handle);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN +
		segment_data.size() + sizeof(nxt_handle),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_network_device_debug_info_resp(
	    0, NSM_SUCCESS, reason_code, (uint8_t *)segment_data.data(),
	    segment_data.size(), nxt_handle, response);

	struct nsm_get_network_device_debug_info_resp *resp =
	    reinterpret_cast<struct nsm_get_network_device_debug_info_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_DEBUG_INFO, resp->hdr.command);
	EXPECT_EQ(data_size, le16toh(resp->hdr.data_size));
	EXPECT_EQ(nxt_handle, le32toh(resp->next_record_handle));
}

TEST(getNetworkDeviceDebugInfo, testBadEncodeResponse)
{
	// this is some dummy data segment with random size
	std::vector<uint8_t> segment_data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
					  0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
					  0x00, 0x00, 0x00, 0x00};
	uint16_t reason_code = ERR_NULL;
	uint32_t nxt_handle = 01;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN +
		segment_data.size() + sizeof(nxt_handle),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_network_device_debug_info_resp(
	    0, NSM_SUCCESS, reason_code, (uint8_t *)segment_data.data(),
	    segment_data.size(), nxt_handle, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_get_network_device_debug_info_resp(
	    0, NSM_SUCCESS, reason_code, nullptr, segment_data.size(),
	    nxt_handle, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getNetworkDeviceDebugInfo, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    0x00,			       // completion code
	    0x00,
	    0x00,
	    0x0C, // data size
	    0x00,
	    0x01, // next record handle
	    0x00,
	    0x00,
	    0x00,
	    0x01, // segment data
	    0x02,
	    0x03,
	    0x04,
	    0x05,
	    0x06,
	    0x07,
	    0x08,
	    0x09};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	uint16_t segDataSize = 0;
	uint32_t nextHandle = 0;
	std::vector<uint8_t> segData(65535, 0);

	auto rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reasonCode, &segDataSize, segData.data(),
	    &nextHandle);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(8, segDataSize);
	EXPECT_EQ(1, nextHandle);
}

TEST(getNetworkDeviceDebugInfo, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    0x00,			       // completion code
	    0x00,
	    0x00,
	    0x0C, // data size
	    0x00,
	    0x01, // next record handle
	    0x00,
	    0x00,
	    0x00,
	    0x01, // segment data
	    0x02,
	    0x03,
	    0x04,
	    0x05,
	    0x06,
	    0x07,
	    0x08,
	    0x09};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	uint16_t segDataSize = 0;
	uint32_t nextHandle = 0;
	std::vector<uint8_t> segData(65535, 0);

	auto rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reasonCode, nullptr, segData.data(),
	    &nextHandle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reasonCode, &segDataSize, nullptr,
	    &nextHandle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reasonCode, &segDataSize, segData.data(),
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len - 12, &cc, &reasonCode, &segDataSize,
	    segData.data(), &nextHandle);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(eraseTrace, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t data_size = 0;

	auto rc = encode_erase_trace_req(0, request);

	struct nsm_erase_trace_req *req =
	    reinterpret_cast<struct nsm_erase_trace_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_TRACE, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
}

TEST(eraseTrace, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));

	auto rc = encode_erase_trace_req(0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(eraseTrace, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0x00};		 // data size

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_erase_trace_req(request, msg_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(eraseTrace, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0x04};		 // data size [shouldn't be 4]

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_erase_trace_req(nullptr, msg_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_trace_req(request, msg_len - 2);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_erase_trace_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(eraseTrace, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t resultStatus = ERASE_TRACE_DATA_ERASE_INPROGRESS;
	uint16_t reasonCode = 0;

	auto rc = encode_erase_trace_resp(0, NSM_SUCCESS, reasonCode,
					  resultStatus, response);

	struct nsm_erase_trace_resp *resp =
	    reinterpret_cast<struct nsm_erase_trace_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_TRACE, resp->hdr.command);
	EXPECT_EQ(sizeof(resultStatus), le16toh(resp->hdr.data_size));
	EXPECT_EQ(resultStatus, resp->result_status);
}

TEST(eraseTrace, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0x00,		 // completion code
	    0x00,
	    0x00,
	    0x01, // data size
	    0x00,
	    0x02 // result
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t resultStatus = ERASE_TRACE_NO_DATA_ERASED;

	auto rc = decode_erase_trace_resp(response, msg_len, &cc, &reasonCode,
					  &resultStatus);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(resultStatus, ERASE_TRACE_DATA_ERASE_INPROGRESS);
}

TEST(eraseTrace, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0x00,		 // completion code
	    0x00,
	    0x00,
	    0x05, // data size [shouldn,t be 5]
	    0x00,
	    0x02 // result
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t resultStatus = ERASE_TRACE_NO_DATA_ERASED;

	auto rc = decode_erase_trace_resp(response, msg_len, &cc, &reasonCode,
					  nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_trace_resp(response, msg_len - 2, &cc, &reasonCode,
				     &resultStatus);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_erase_trace_resp(response, msg_len, &cc, &reasonCode,
				     &resultStatus);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getNetworkDeviceLogInfo, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint32_t handle = 2;
	uint8_t data_size = sizeof(struct nsm_get_network_device_log_info_req) -
			    sizeof(struct nsm_common_req);

	auto rc = encode_get_network_device_log_info_req(0, handle, request);

	struct nsm_get_network_device_log_info_req *req =
	    reinterpret_cast<struct nsm_get_network_device_log_info_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_LOG_INFO, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
	EXPECT_EQ(htole32(handle), req->record_handle);
}

TEST(getNetworkDeviceLogInfo, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));

	auto rc = encode_get_network_device_log_info_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getNetworkDeviceLogInfo, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_LOG_INFO, // command
	    0x04,			     // data size
	    0x03,			     // record_handle
	    0x00,
	    0x00,
	    0x00};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint32_t handle;
	auto rc =
	    decode_get_network_device_log_info_req(request, msg_len, &handle);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(03, handle);
}

TEST(getNetworkDeviceLogInfo, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_LOG_INFO, // command
	    0x10,			     // data size [it shouldn't be 0x10]
	    0x03,			     // record_handle
	    0x00,
	    0x00,
	    0x00};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint32_t handle;
	auto rc =
	    decode_get_network_device_log_info_req(nullptr, msg_len, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_log_info_req(request, msg_len, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_network_device_log_info_req(request, msg_len - 2,
						    &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_network_device_log_info_req(request, msg_len, &handle);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getNetworkDeviceLogInfo, testGoodEncodeResponse)
{
	// this is some dummy data segment with random size
	std::vector<uint8_t> log_data{0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
				      0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
				      0x0d, 0x0e, 0x0f, 0x10};
	struct nsm_device_log_info_breakdown log_info;
	log_info.lost_events = 02;
	log_info.unused = 00;
	log_info.synced_time = 00;
	log_info.reserved1 = 00;
	log_info.reserved2 = 00;
	log_info.time_high = 100;
	log_info.time_low = 200;
	log_info.entry_prefix = 33;
	log_info.length = (log_data.size() / 4);
	log_info.entry_suffix = 444;
	struct nsm_device_log_info *info =
	    (struct nsm_device_log_info *)(&log_info);

	uint16_t reason_code = ERR_NULL;
	uint32_t nxt_handle = 01;
	uint8_t data_size =
	    log_data.size() + sizeof(nxt_handle) + sizeof(nsm_device_log_info);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN +
		log_data.size() + sizeof(nxt_handle) +
		sizeof(nsm_device_log_info),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_network_device_log_info_resp(
	    0, NSM_SUCCESS, reason_code, nxt_handle, log_info,
	    (uint8_t *)log_data.data(), log_data.size(), response);

	struct nsm_get_network_device_log_info_resp *resp =
	    reinterpret_cast<struct nsm_get_network_device_log_info_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_LOG_INFO, resp->hdr.command);
	EXPECT_EQ(data_size, le16toh(resp->hdr.data_size));
	EXPECT_EQ(nxt_handle, le32toh(resp->next_record_handle));
	EXPECT_EQ(info->lost_events_and_synced_time,
		  le32toh(resp->log_info.lost_events_and_synced_time));
	EXPECT_EQ(info->time_high, le32toh(resp->log_info.time_high));
	EXPECT_EQ(info->time_low, le32toh(resp->log_info.time_low));
	EXPECT_EQ(info->entry_prefix_and_length,
		  le32toh(resp->log_info.entry_prefix_and_length));
	EXPECT_EQ(info->entry_suffix, le64toh(resp->log_info.entry_suffix));
}

TEST(getNetworkDeviceLogInfo, testBadEncodeResponse)
{
	// this is some dummy data segment with random size
	std::vector<uint8_t> log_data;
	struct nsm_device_log_info_breakdown log_info;
	uint16_t reason_code = ERR_NULL;
	uint32_t nxt_handle = 01;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN +
		log_data.size() + sizeof(nxt_handle) +
		sizeof(nsm_device_log_info),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_network_device_log_info_resp(
	    0, NSM_SUCCESS, reason_code, nxt_handle, log_info,
	    (uint8_t *)log_data.data(), log_data.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_get_network_device_log_info_resp(
	    0, NSM_SUCCESS, reason_code, nxt_handle, log_info, nullptr,
	    log_data.size(), response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(eraseDebugLogInfo, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t info_type = 0;
	uint8_t data_size = sizeof(struct nsm_erase_debug_info_req) -
			    sizeof(struct nsm_common_req);

	auto rc = encode_erase_debug_info_req(0, info_type, request);

	struct nsm_erase_debug_info_req *req =
	    reinterpret_cast<struct nsm_erase_debug_info_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_DEBUG_INFO, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
	EXPECT_EQ(info_type, req->debug_info_type);
}

TEST(eraseDebugLogInfo, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_debug_info_req));

	auto rc = encode_erase_debug_info_req(0, 0, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(eraseDebugLogInfo, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x80,		  // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO, // command
	    0x02,		  // data size
	    0x03,		  // info type
	    0x00};		  // reserved

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t info_type;
	auto rc = decode_erase_debug_info_req(request, msg_len, &info_type);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(03, info_type);
}

TEST(eraseDebugLogInfo, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x80,		  // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO, // command
	    0x10,		  // data size [it shouldn't be 0x10]
	    0x03,		  // info type
	    0x00};		  // reserved

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t info_type;
	auto rc = decode_erase_debug_info_req(nullptr, msg_len, &info_type);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_debug_info_req(request, msg_len, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_debug_info_req(request, msg_len - 2, &info_type);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_erase_debug_info_req(request, msg_len, &info_type);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(eraseDebugLogInfo, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t resultStatus = ERASE_TRACE_DATA_ERASE_INPROGRESS;
	uint16_t reasonCode = 0;

	auto rc = encode_erase_debug_info_resp(0, NSM_SUCCESS, reasonCode,
					       resultStatus, response);

	struct nsm_erase_debug_info_resp *resp =
	    reinterpret_cast<struct nsm_erase_debug_info_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_DEBUG_INFO, resp->hdr.command);
	EXPECT_EQ(2 * sizeof(resultStatus), le16toh(resp->hdr.data_size));
	EXPECT_EQ(resultStatus, resp->result_status);
}

TEST(eraseDebugLogInfo, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x00,		  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO, // command
	    0x00,		  // completion code
	    0x00,
	    0x00,
	    0x02, // data size
	    0x00,
	    0x02, // result
	    0x00  // reserved
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t resultStatus = ERASE_TRACE_NO_DATA_ERASED;

	auto rc = decode_erase_debug_info_resp(response, msg_len, &cc,
					       &reasonCode, &resultStatus);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(resultStatus, ERASE_TRACE_DATA_ERASE_INPROGRESS);
}

TEST(eraseDebugLogInfo, testBadDecodeResponseLength)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0x00,		 // completion code
	    0x00,
	    0x00,
	    0x05, // data size [shouldn,t be 5]
	    0x00,
	    0x02, // result
	    0x00  // reserved
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t resultStatus = ERASE_TRACE_NO_DATA_ERASED;

	auto rc = decode_erase_debug_info_resp(response, msg_len, &cc,
					       &reasonCode, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_debug_info_resp(response, msg_len - 2, &cc,
					  &reasonCode, &resultStatus);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_erase_debug_info_resp(response, msg_len, &cc, &reasonCode,
					  &resultStatus);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getDiagnostics, testGoodEncodeDeviceDiagnosticsRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_get_device_diagnostics_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t segment_id = 0x34;

	auto rc =
	    encode_get_device_diagnostics_req(instance_id, segment_id, request);

	struct nsm_get_device_diagnostics_req *req =
	    (struct nsm_get_device_diagnostics_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(instance_id, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	// Verify payload fields
	EXPECT_EQ(NSM_GET_DEVICE_DIAGNOSTICS, req->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_get_device_diagnostics_req) -
		      sizeof(struct nsm_common_req),
		  req->hdr.data_size);
	EXPECT_EQ(segment_id, req->segment_id);
}

TEST(getDiagnostics, testBadEncodeDeviceDiagnosticsRequest)
{
	auto rc = encode_get_device_diagnostics_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDiagnostics, testGoodDecodeDeviceDiagnosticsRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x80,			// RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	// NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DIAGNOSTICS, // command
	    0x01,			// data size
	    0x34			// segment_id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t segment_id = 0;

	auto rc =
	    decode_get_device_diagnostics_req(request, msg_len, &segment_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0x34, segment_id);
}

TEST(getDiagnostics, testBadDecodeDeviceDiagnosticsRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x80,			// RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	// NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DIAGNOSTICS, // command
	    0x02,			// incorrect data size
	    0x34,			// segment_id
	    0x00			// extra byte
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_get_device_diagnostics_req);
	uint8_t segment_id = 0;

	// Test null pointer checks
	auto rc =
	    decode_get_device_diagnostics_req(nullptr, msg_len, &segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_diagnostics_req(request, msg_len, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test incorrect message length
	rc = decode_get_device_diagnostics_req(request, msg_len - 1,
					       &segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	// Test incorrect data size in payload
	rc = decode_get_device_diagnostics_req(request, msg_len, &segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getDiagnostics, testGoodEncodeDeviceDiagnosticsResponse)
{
	// Setup test data
	std::vector<uint8_t> segment_data{0x01, 0x02, 0x03, 0x04};
	uint16_t segment_data_size = segment_data.size();
	uint8_t next_segment_id = 0x45;
	uint8_t instance_id = 0x12;

	// Create response buffer
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_resp) +
	    segment_data_size);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Test successful response
	auto rc = encode_get_device_diagnostics_resp(
	    instance_id, NSM_SUCCESS,
	    0, // reason_code
	    segment_data.data(), segment_data_size, next_segment_id, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(0, response->hdr.request); // Response message
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);

	// Verify payload fields
	auto resp = reinterpret_cast<nsm_get_device_diagnostics_resp *>(
	    response->payload);
	EXPECT_EQ(NSM_GET_DEVICE_DIAGNOSTICS, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(segment_data_size + sizeof(next_segment_id),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(next_segment_id, resp->next_segment_id);

	// Verify segment data was copied correctly
	EXPECT_EQ(0, memcmp(resp->segment_data, segment_data.data(),
			    segment_data_size));
}

TEST(getDiagnostics, testErrorEncodeDeviceDiagnosticsResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	std::vector<uint8_t> segment_data{0x01, 0x02};

	// Test null pointer check
	auto rc = encode_get_device_diagnostics_resp(
	    0, NSM_SUCCESS, 0, segment_data.data(), segment_data.size(), 0,
	    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test error response encoding
	rc = encode_get_device_diagnostics_resp(
	    0, NSM_ERROR, 0, segment_data.data(), segment_data.size(), 0,
	    response);

	auto resp = reinterpret_cast<nsm_get_device_diagnostics_resp *>(
	    response->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_GET_DEVICE_DIAGNOSTICS, resp->hdr.command);
	EXPECT_EQ(NSM_ERROR, resp->hdr.completion_code);
}

TEST(getDiagnostics, encodeDeviceDiagnosticsResponseErrorCompletionCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x12;
	std::vector<uint8_t> segment_data{0x01, 0x02, 0x03};
	uint8_t next_segment_id = 0x05;

	// Test error response with reason code
	auto rc = encode_get_device_diagnostics_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0x1234, segment_data.data(),
	    segment_data.size(), next_segment_id, response);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify header fields
	EXPECT_EQ(0, response->hdr.request); // Response message
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);

	// Error responses use nsm_common_non_success_resp structure
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(response->payload);
	EXPECT_EQ(NSM_GET_DEVICE_DIAGNOSTICS, resp->command);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, resp->completion_code);
	EXPECT_EQ(0x1234, le16toh(resp->reason_code));
}

TEST(getDiagnostics, testGoodDecodeDeviceDiagnosticsResponse)
{
	// Create response message
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x00,			// RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	// NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DIAGNOSTICS, // command
	    NSM_SUCCESS,		// completion code
	    0x00,			// reserved
	    0x00,
	    0x0A, // data_size
	    0x00,
	    0x01, // next record handle
	    0x01, // segment data
	    0x02,
	    0x03,
	    0x04,
	    0x05,
	    0x06,
	    0x07,
	    0x08,
	    0x09};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	// Output parameters
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	std::vector<uint8_t> decoded_data(65535, 0);
	uint16_t decoded_size = 0;
	uint8_t decoded_next_segment_id = 0;

	auto rc = decode_get_device_diagnostics_resp(
	    response, msg_len, &cc, &reasonCode, decoded_data.data(),
	    &decoded_size, &decoded_next_segment_id);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(9, decoded_size);
	EXPECT_EQ(1, decoded_next_segment_id);
}

TEST(getDiagnostics, testBadDecodeDeviceDiagnosticsResponse)
{
	// Create response message
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x00,			// RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	// NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DIAGNOSTICS, // command
	    NSM_SUCCESS,		// completion code
	    0x00,			// reserved
	    0x00,
	    0x00, // data_size
	    0x00,
	    0x01, // next record handle
	    0x01, // segment data
	    0x02,
	    0x03,
	    0x04,
	    0x05,
	    0x06,
	    0x07,
	    0x08,
	    0x09};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> decoded_data(65535, 0);
	uint16_t decoded_size = 0;
	uint8_t decoded_next_segment_id = 0;

	// Test null pointer checks
	auto rc = decode_get_device_diagnostics_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &decoded_size,
	    &decoded_next_segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_diagnostics_resp(
	    response, msg_len, &cc, &reason_code, decoded_data.data(), nullptr,
	    &decoded_next_segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_diagnostics_resp(
	    response, msg_len, &cc, &reason_code, decoded_data.data(),
	    &decoded_size, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test message length too small
	rc = decode_get_device_diagnostics_resp(
	    response, msg_len - 11, &cc, &reason_code, decoded_data.data(),
	    &decoded_size, &decoded_next_segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	// Test invalid data size in payload
	rc = decode_get_device_diagnostics_resp(
	    response, msg_len, &cc, &reason_code, decoded_data.data(),
	    &decoded_size, &decoded_next_segment_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// Tests for encode_get_device_reset_statistics_req
TEST(getDeviceResetStatistics, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x05;

	auto rc = encode_get_device_reset_statistics_req(instance_id, msg);

	auto req = reinterpret_cast<nsm_common_req *>(msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_RESET_STATISTICS, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getDeviceResetStatistics, badTestEncodeRequest)
{
	uint8_t instance_id = 0x05;
	auto rc = encode_get_device_reset_statistics_req(instance_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Tests for decode_get_device_reset_statistics_req
TEST(getDeviceResetStatistics, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_RESET_STATISTICS, // command
	    0x00			     // data size
	};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = decode_get_device_reset_statistics_req(msg, msgBuf.size());

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(getDeviceResetStatistics, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_RESET_STATISTICS, // command
	    0x01			     // data size (should be 0)
	};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc =
	    decode_get_device_reset_statistics_req(nullptr, msgBuf.size());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_reset_statistics_req(msg, msgBuf.size() - 2);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);

	rc = decode_get_device_reset_statistics_req(msg, msgBuf.size());
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

// Tests for decode_get_network_device_log_info_resp
TEST(getNetworkDeviceLogInfo, goodTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_LOG_INFO, // command
	    0x00,			     // completion code
	    0x00,
	    0x00,
	    0x20, // data size (32 bytes)
	    0x00,
	    0x02, // next_record_handle
	    0x00,
	    0x00,
	    0x00,
	    0x03, // log_info.lost_events_and_synced_time
	    0x00, // reserved1
	    0x00, // reserved2
	    0x01,
	    0x00,
	    0x00,
	    0x00, // time_low
	    0x02,
	    0x00,
	    0x00,
	    0x00, // time_high
	    0x04,
	    0x00,
	    0x00,
	    0x00, // entry_prefix_and_length
	    0x05,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00, // entry_suffix
	    0xAA,
	    0xBB,
	    0xCC,
	    0xDD // log_data
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint32_t next_handle = 0;
	struct nsm_device_log_info_breakdown log_info = {};
	std::vector<uint8_t> log_data(65535, 0);
	uint16_t log_data_size = 0;

	auto rc = decode_get_network_device_log_info_resp(
	    response, msg_len, &cc, &reason_code, &next_handle, &log_info,
	    log_data.data(), &log_data_size);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(2, next_handle);
	EXPECT_EQ(4, log_data_size);
}

TEST(getNetworkDeviceLogInfo, badTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_LOG_INFO, // command
	    0x00,			     // completion code
	    0x00,
	    0x00,
	    0x20, // data size
	    0x00,
	    0x02, // next_record_handle
	    0x00,
	    0x00,
	    0x00,
	    0x03, // log_info
	    0x00,
	    0x00,
	    0x01,
	    0x00,
	    0x00,
	    0x00, // time_low
	    0x02,
	    0x00,
	    0x00,
	    0x00, // time_high
	    0x04,
	    0x00,
	    0x00,
	    0x00, // entry_prefix_and_length
	    0x05,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00,
	    0x00, // entry_suffix
	    0xAA,
	    0xBB,
	    0xCC,
	    0xDD};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint32_t next_handle = 0;
	struct nsm_device_log_info_breakdown log_info = {};
	std::vector<uint8_t> log_data(65535, 0);
	uint16_t log_data_size = 0;

	auto rc = decode_get_network_device_log_info_resp(
	    nullptr, msg_len, &cc, &reason_code, &next_handle, &log_info,
	    log_data.data(), &log_data_size);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &log_info,
	    log_data.data(), &log_data_size);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_resp(
	    response, msg_len, &cc, &reason_code, &next_handle, nullptr,
	    log_data.data(), &log_data_size);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_resp(
	    response, msg_len, &cc, &reason_code, &next_handle, &log_info,
	    nullptr, &log_data_size);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_resp(
	    response, msg_len, &cc, &reason_code, &next_handle, &log_info,
	    log_data.data(), nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_resp(
	    response, msg_len - 20, &cc, &reason_code, &next_handle, &log_info,
	    log_data.data(), &log_data_size);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Tests for encode_get_device_debug_parameters_req
TEST(getDeviceDebugParameters, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x07;
	uint8_t debug_configuration_type = 0x02;
	struct nsm_debug_parameter_id parameter_id = {};
	parameter_id.port_number = 0x1234;
	parameter_id.index = 0x56;
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};
	parameter_sub_id.value = 0xABCDEF00;

	auto rc = encode_get_device_debug_parameters_req(
	    instance_id, debug_configuration_type, parameter_id,
	    parameter_sub_id, msg);

	auto req = reinterpret_cast<nsm_get_device_debug_parameters_req *>(
	    msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_DEBUG_PARAMETERS, req->hdr.command);
	EXPECT_EQ(debug_configuration_type, req->debug_configuration_type);
	EXPECT_EQ(0x1234, le16toh(req->parameter_id.port_number));
	EXPECT_EQ(0x56, req->parameter_id.index);
	EXPECT_EQ(0xABCDEF00, le32toh(req->parameter_sub_id.value));
}

TEST(getDeviceDebugParameters, badTestEncodeRequest)
{
	uint8_t instance_id = 0x07;
	uint8_t debug_configuration_type = 0x02;
	struct nsm_debug_parameter_id parameter_id = {};
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};

	auto rc = encode_get_device_debug_parameters_req(
	    instance_id, debug_configuration_type, parameter_id,
	    parameter_sub_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Tests for decode_get_device_debug_parameters_req
TEST(getDeviceDebugParameters, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DEBUG_PARAMETERS, // command
	    0x00,			     // reserved1
	    0x0B,
	    0x00, // data_size (11 bytes, little-endian uint16_t)
	    0x00,
	    0x00, // reserved2
	    0x02, // debug_configuration_type
	    0x00,
	    0x00,
	    0x00, // reserved[3]
	    0x34,
	    0x12, // port_number (little-endian)
	    0x00, // reserved
	    0x56, // index
	    0x00,
	    0xEF,
	    0xCD,
	    0xAB // parameter_sub_id (little-endian)
	};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t debug_configuration_type = 0;
	struct nsm_debug_parameter_id parameter_id = {};
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};

	auto rc = decode_get_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0x02, debug_configuration_type);
	EXPECT_EQ(0x1234, parameter_id.port_number);
	EXPECT_EQ(0x56, parameter_id.index);
	EXPECT_EQ(0xABCDEF00, parameter_sub_id.value);
}

TEST(getDeviceDebugParameters, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf{0x10,
				    0xDE, // PCI VID: NVIDIA 0x10DE
				    0x80, // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
				    0x89, // OCP_TYPE=8, OCP_VER=9
				    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
				    NSM_GET_DEVICE_DEBUG_PARAMETERS, // command
				    0x00, // reserved1
				    0x0B,
				    0x00, // data_size (little-endian uint16_t)
				    0x00,
				    0x00, // reserved2
				    0x02, // debug_configuration_type
				    0x01,
				    0x00,
				    0x00, // reserved[3] - non-zero!
				    0x34,
				    0x12,
				    0x00,
				    0x56,
				    0x00,
				    0xEF,
				    0xCD,
				    0xAB};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t debug_configuration_type = 0;
	struct nsm_debug_parameter_id parameter_id = {};
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};

	auto rc = decode_get_device_debug_parameters_req(
	    nullptr, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_req(
	    msg, msgBuf.size(), nullptr, &parameter_id, &parameter_sub_id);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_req(msg, msgBuf.size(),
						    &debug_configuration_type,
						    nullptr, &parameter_sub_id);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_req(msg, msgBuf.size(),
						    &debug_configuration_type,
						    &parameter_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

// Tests for encode_get_device_debug_parameters_resp
TEST(getDeviceDebugParameters, goodTestEncodeResponse)
{
	std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04};
	uint16_t data_size = data.size();
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp) +
	    data_size);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x08;
	uint16_t reason_code = 0;

	auto rc = encode_get_device_debug_parameters_resp(
	    instance_id, NSM_SUCCESS, reason_code, &data_size, data.data(),
	    msg);

	auto resp = reinterpret_cast<nsm_get_device_debug_parameters_resp *>(
	    msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_DEBUG_PARAMETERS, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(data_size, le16toh(resp->hdr.data_size));
}

TEST(getDeviceDebugParameters, badTestEncodeResponse)
{
	std::vector<uint8_t> data{0x01, 0x02};
	uint16_t data_size = data.size();
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_resp) +
	    data_size);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x08;
	uint16_t reason_code = 0;

	auto rc = encode_get_device_debug_parameters_resp(
	    instance_id, NSM_SUCCESS, reason_code, &data_size, data.data(),
	    nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = encode_get_device_debug_parameters_resp(
	    instance_id, NSM_SUCCESS, reason_code, &data_size, nullptr, msg);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = encode_get_device_debug_parameters_resp(
	    instance_id, NSM_SUCCESS, reason_code, nullptr, data.data(), msg);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Tests for decode_get_device_debug_parameters_resp
TEST(getDeviceDebugParameters, goodTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DEBUG_PARAMETERS, // command
	    NSM_SUCCESS,		     // completion_code
	    0x00,
	    0x00,
	    0x04, // data_size
	    0x00,
	    0xAA,
	    0xBB,
	    0xCC,
	    0xDD // data
	};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t cc = NSM_ERROR;
	uint16_t data_size = 0;
	std::vector<uint8_t> data(65535, 0);

	auto rc = decode_get_device_debug_parameters_resp(
	    msg, msgBuf.size(), &cc, &data_size, data.data());

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(4, data_size);
	EXPECT_EQ(0xAA, data[0]);
	EXPECT_EQ(0xBB, data[1]);
	EXPECT_EQ(0xCC, data[2]);
	EXPECT_EQ(0xDD, data[3]);
}

TEST(getDeviceDebugParameters, badTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf{0x10,
				    0xDE, // PCI VID: NVIDIA 0x10DE
				    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
				    0x89, // OCP_TYPE=8, OCP_VER=9
				    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
				    NSM_GET_DEVICE_DEBUG_PARAMETERS, // command
				    NSM_SUCCESS, // completion_code
				    0x00,
				    0x00,
				    0x04, // data_size
				    0x00,
				    0xAA,
				    0xBB,
				    0xCC,
				    0xDD};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t cc = NSM_ERROR;
	uint16_t data_size = 0;
	std::vector<uint8_t> data(65535, 0);

	auto rc = decode_get_device_debug_parameters_resp(
	    nullptr, msgBuf.size(), &cc, &data_size, data.data());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_resp(
	    msg, msgBuf.size(), nullptr, &data_size, data.data());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_resp(msg, msgBuf.size(), &cc,
						     nullptr, data.data());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_resp(msg, msgBuf.size(), &cc,
						     &data_size, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_debug_parameters_resp(
	    msg, msgBuf.size() - 10, &cc, &data_size, data.data());
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Tests for encode_set_device_debug_parameters_req
TEST(setDeviceDebugParameters, goodTestEncodeRequest)
{
	std::vector<uint8_t> data{0x11, 0x22, 0x33, 0x44};
	uint8_t data_size = data.size();
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) +
	    data_size);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x09;
	uint8_t debug_configuration_type = 0x03;
	struct nsm_debug_parameter_id parameter_id = {};
	parameter_id.port_number = 0x5678;
	parameter_id.index = 0x9A;
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};
	parameter_sub_id.value = 0x12345678;

	auto rc = encode_set_device_debug_parameters_req(
	    instance_id, debug_configuration_type, parameter_id,
	    parameter_sub_id, data_size, data.data(), msg);

	auto req = reinterpret_cast<nsm_set_device_debug_parameters_req *>(
	    msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_SET_DEVICE_DEBUG_PARAMETERS, req->hdr.command);
	EXPECT_EQ(debug_configuration_type, req->debug_configuration_type);
	EXPECT_EQ(data_size, req->data_size);
	EXPECT_EQ(0x5678, le16toh(req->parameter_id.port_number));
	EXPECT_EQ(0x9A, req->parameter_id.index);
	EXPECT_EQ(0x12345678, le32toh(req->parameter_sub_id.value));
}

TEST(setDeviceDebugParameters, badTestEncodeRequest)
{
	std::vector<uint8_t> data{0x11, 0x22};
	uint8_t data_size = data.size();
	uint8_t instance_id = 0x09;
	uint8_t debug_configuration_type = 0x03;
	struct nsm_debug_parameter_id parameter_id = {};
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_debug_parameters_req) +
	    data_size);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_set_device_debug_parameters_req(
	    instance_id, debug_configuration_type, parameter_id,
	    parameter_sub_id, data_size, data.data(), nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = encode_set_device_debug_parameters_req(
	    instance_id, debug_configuration_type, parameter_id,
	    parameter_sub_id, data_size, nullptr, msg);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Tests for decode_set_device_debug_parameters_req
TEST(setDeviceDebugParameters, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_DEBUG_PARAMETERS, // command
	    0x0F,			     // data_size (15 bytes)
	    0x03,			     // debug_configuration_type
	    0x04,			     // data_size
	    0x00,
	    0x00, // reserved[2]
	    0x78,
	    0x56, // port_number (little-endian)
	    0x00, // reserved
	    0x9A, // index
	    0x78,
	    0x56,
	    0x34,
	    0x12, // parameter_sub_id (little-endian)
	    0x11,
	    0x22,
	    0x33,
	    0x44 // data[4]
	};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t debug_configuration_type = 0;
	struct nsm_debug_parameter_id parameter_id = {};
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};
	uint8_t data_size = 0;
	std::vector<uint8_t> data(65535, 0);
	uint8_t *data_ptr = data.data();

	auto rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id, &data_size, &data_ptr);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0x03, debug_configuration_type);
	EXPECT_EQ(0x5678, parameter_id.port_number);
	EXPECT_EQ(0x9A, parameter_id.index);
	EXPECT_EQ(0x12345678, parameter_sub_id.value);
	EXPECT_EQ(0x04, data_size);
}

TEST(setDeviceDebugParameters, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf{0x10,
				    0xDE, // PCI VID: NVIDIA 0x10DE
				    0x80, // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
				    0x89, // OCP_TYPE=8, OCP_VER=9
				    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
				    NSM_SET_DEVICE_DEBUG_PARAMETERS, // command
				    0x0F, // data_size
				    0x03,
				    0x04,
				    0x01,
				    0x00, // reserved[2] - non-zero!
				    0x78,
				    0x56,
				    0x00,
				    0x9A,
				    0x78,
				    0x56,
				    0x34,
				    0x12,
				    0x11,
				    0x22,
				    0x33,
				    0x44};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t debug_configuration_type = 0;
	struct nsm_debug_parameter_id parameter_id = {};
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id = {};
	uint8_t data_size = 0;
	std::vector<uint8_t> data(65535, 0);
	uint8_t *data_ptr = data.data();

	auto rc = decode_set_device_debug_parameters_req(
	    nullptr, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id, &data_size, &data_ptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), nullptr, &parameter_id, &parameter_sub_id,
	    &data_size, &data_ptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, nullptr,
	    &parameter_sub_id, &data_size, &data_ptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    nullptr, &data_size, &data_ptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id, nullptr, &data_ptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id, &data_size, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_req(
	    msg, msgBuf.size(), &debug_configuration_type, &parameter_id,
	    &parameter_sub_id, &data_size, &data_ptr);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

// Tests for decode_set_device_debug_parameters_resp
TEST(setDeviceDebugParameters, goodTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf{0x10,
				    0xDE, // PCI VID: NVIDIA 0x10DE
				    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
				    0x89, // OCP_TYPE=8, OCP_VER=9
				    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
				    NSM_SET_DEVICE_DEBUG_PARAMETERS, // command
				    NSM_SUCCESS, // completion_code
				    0x00,
				    0x00,
				    0x00, // data_size
				    0x00};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t cc = NSM_ERROR;

	auto rc =
	    decode_set_device_debug_parameters_resp(msg, msgBuf.size(), &cc);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
}

TEST(setDeviceDebugParameters, badTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf{0x10,
				    0xDE, // PCI VID: NVIDIA 0x10DE
				    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
				    0x89, // OCP_TYPE=8, OCP_VER=9
				    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
				    NSM_SET_DEVICE_DEBUG_PARAMETERS, // command
				    NSM_SUCCESS, // completion_code
				    0x00,
				    0x00,
				    0x00,
				    0x00};
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t cc = NSM_ERROR;

	auto rc = decode_set_device_debug_parameters_resp(nullptr,
							  msgBuf.size(), &cc);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_resp(msg, msgBuf.size(),
						     nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_set_device_debug_parameters_resp(msg, msgBuf.size() - 3,
						     &cc);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// ===================================================================
// Tests for Erase Debug Info
// ===================================================================

TEST(EraseDebugInfo, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_erase_debug_info_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x10;
	uint8_t info_type = INFO_TYPE_FW_SAVED_DUMP_INFO;

	auto rc = encode_erase_debug_info_req(instance_id, info_type, msg);

	auto req = reinterpret_cast<nsm_erase_debug_info_req *>(msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_DEBUG_INFO, req->hdr.command);
	EXPECT_EQ(info_type, req->debug_info_type);
}

TEST(EraseDebugInfo, badTestEncodeRequest)
{
	uint8_t instance_id = 0x10;
	uint8_t info_type = INFO_TYPE_FW_SAVED_DUMP_INFO;

	auto rc = encode_erase_debug_info_req(instance_id, info_type, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(EraseDebugInfo, goodTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			  // PCI VID: NVIDIA 0x10DE
	    0x80,			  // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO,	  // command
	    2,				  // data size
	    INFO_TYPE_FW_SAVED_DUMP_INFO, // info type
	    0x00			  // reserved
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t info_type = 0;

	auto rc = decode_erase_debug_info_req(request, msg_len, &info_type);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(INFO_TYPE_FW_SAVED_DUMP_INFO, info_type);
}

TEST(EraseDebugInfo, badTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			  // PCI VID: NVIDIA 0x10DE
	    0x80,			  // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO,	  // command
	    2,				  // data size
	    INFO_TYPE_FW_SAVED_DUMP_INFO, // info type
	    0x00			  // reserved
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t info_type = 0;

	auto rc = decode_erase_debug_info_req(nullptr, msg_len, &info_type);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_debug_info_req(request, msg_len, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_debug_info_req(request, msg_len - 3, &info_type);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(EraseDebugInfo, goodTestEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_debug_info_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x11;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = ERASE_TRACE_DATA_ERASED;

	auto rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					       result_status, response);

	auto resp =
	    reinterpret_cast<nsm_erase_debug_info_resp *>(response->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_DEBUG_INFO, resp->hdr.command);
	EXPECT_EQ(result_status, resp->result_status);
}

TEST(EraseDebugInfo, badTestEncodeResponse)
{
	uint8_t instance_id = 0x11;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = ERASE_TRACE_DATA_ERASED;

	auto rc = encode_erase_debug_info_resp(instance_id, cc, reason_code,
					       result_status, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(EraseDebugInfo, goodTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x00,		  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO, // command
	    NSM_SUCCESS,	  // completion code
	    0x00,
	    0x00,
	    0x02, // data size
	    0x00,
	    ERASE_TRACE_DATA_ERASED, // result status
	    0x00		     // reserved
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint8_t result_status = 0;

	auto rc = decode_erase_debug_info_resp(response, msg_len, &cc,
					       &reason_code, &result_status);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERASE_TRACE_DATA_ERASED, result_status);
}

TEST(EraseDebugInfo, badTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x00,		  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_ERASE_DEBUG_INFO, // command
	    NSM_SUCCESS,	  // completion code
	    0x00,
	    0x00,
	    0x02, // data size
	    0x00,
	    ERASE_TRACE_DATA_ERASED, // result status
	    0x00		     // reserved
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint8_t result_status = 0;

	auto rc = decode_erase_debug_info_resp(nullptr, msg_len, &cc,
					       &reason_code, &result_status);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_debug_info_resp(response, msg_len, nullptr,
					  &reason_code, &result_status);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_debug_info_resp(response, msg_len, &cc, nullptr,
					  &result_status);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_debug_info_resp(response, msg_len, &cc, &reason_code,
					  nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_debug_info_resp(response, msg_len - 5, &cc,
					  &reason_code, &result_status);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// ===================================================================
// Tests for Erase Trace
// ===================================================================

TEST(EraseTrace, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_erase_trace_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x12;

	auto rc = encode_erase_trace_req(instance_id, msg);

	auto req = reinterpret_cast<nsm_erase_trace_req *>(msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_TRACE, req->hdr.command);
	EXPECT_EQ(0, req->hdr.data_size);
}

TEST(EraseTrace, badTestEncodeRequest)
{
	uint8_t instance_id = 0x12;

	auto rc = encode_erase_trace_req(instance_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(EraseTrace, goodTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0			 // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_erase_trace_req(request, msg_len);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(EraseTrace, badTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    0			 // data size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_erase_trace_req(nullptr, msg_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_trace_req(request, msg_len - 3);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(EraseTrace, goodTestEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_erase_trace_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x13;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = ERASE_TRACE_DATA_ERASED;

	auto rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					  result_status, response);

	auto resp = reinterpret_cast<nsm_erase_trace_resp *>(response->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_TRACE, resp->hdr.command);
	EXPECT_EQ(result_status, resp->result_status);
}

TEST(EraseTrace, badTestEncodeResponse)
{
	uint8_t instance_id = 0x13;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint8_t result_status = ERASE_TRACE_DATA_ERASED;

	auto rc = encode_erase_trace_resp(instance_id, cc, reason_code,
					  result_status, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(EraseTrace, goodTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    NSM_SUCCESS,	 // completion code
	    0x00,
	    0x00,
	    0x01, // data size
	    0x00,
	    ERASE_TRACE_DATA_ERASED // result status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint8_t result_status = 0;

	auto rc = decode_erase_trace_resp(response, msg_len, &cc, &reason_code,
					  &result_status);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERASE_TRACE_DATA_ERASED, result_status);
}

TEST(EraseTrace, badTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_ERASE_TRACE,	 // command
	    NSM_SUCCESS,	 // completion code
	    0x00,
	    0x00,
	    0x01, // data size
	    0x00,
	    ERASE_TRACE_DATA_ERASED // result status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint8_t result_status = 0;

	auto rc = decode_erase_trace_resp(nullptr, msg_len, &cc, &reason_code,
					  &result_status);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_trace_resp(response, msg_len, nullptr, &reason_code,
				     &result_status);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_trace_resp(response, msg_len, &cc, nullptr,
				     &result_status);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_trace_resp(response, msg_len, &cc, &reason_code,
				     nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_erase_trace_resp(response, msg_len - 4, &cc, &reason_code,
				     &result_status);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// ===================================================================
// Tests for Get Device Diagnostics
// ===================================================================

TEST(GetDeviceDiagnostics, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_device_diagnostics_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x14;
	uint8_t segment_id = 0x05;

	auto rc =
	    encode_get_device_diagnostics_req(instance_id, segment_id, msg);

	auto req =
	    reinterpret_cast<nsm_get_device_diagnostics_req *>(msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_DIAGNOSTICS, req->hdr.command);
	EXPECT_EQ(segment_id, req->segment_id);
}

TEST(GetDeviceDiagnostics, badTestEncodeRequest)
{
	uint8_t instance_id = 0x14;
	uint8_t segment_id = 0x05;

	auto rc =
	    encode_get_device_diagnostics_req(instance_id, segment_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(GetDeviceDiagnostics, goodTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x80,			// RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	// NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DIAGNOSTICS, // command
	    1,				// data size
	    0x03			// segment id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t segment_id = 0;

	auto rc =
	    decode_get_device_diagnostics_req(request, msg_len, &segment_id);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0x03, segment_id);
}

TEST(GetDeviceDiagnostics, badTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x80,			// RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	// NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_DIAGNOSTICS, // command
	    1,				// data size
	    0x03			// segment id
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t segment_id = 0;

	auto rc =
	    decode_get_device_diagnostics_req(nullptr, msg_len, &segment_id);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_diagnostics_req(request, msg_len, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_device_diagnostics_req(request, msg_len - 3,
					       &segment_id);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(GetDeviceDiagnostics, goodTestEncodeResponse)
{
	uint8_t seg_data[] = {0x01, 0x02, 0x03, 0x04};
	uint16_t seg_data_size = 4;
	uint8_t next_segment_id = 0x05;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_resp) +
	    seg_data_size - 1);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x15;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;

	auto rc = encode_get_device_diagnostics_resp(
	    instance_id, cc, reason_code, seg_data, seg_data_size,
	    next_segment_id, response);

	auto resp = reinterpret_cast<nsm_get_device_diagnostics_resp *>(
	    response->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_DIAGNOSTICS, resp->hdr.command);
	EXPECT_EQ(next_segment_id, resp->next_segment_id);
}

TEST(GetDeviceDiagnostics, badTestEncodeResponse)
{
	uint8_t seg_data[] = {0x01, 0x02, 0x03, 0x04};
	uint16_t seg_data_size = 4;
	uint8_t next_segment_id = 0x05;
	uint8_t instance_id = 0x15;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;

	auto rc = encode_get_device_diagnostics_resp(
	    instance_id, cc, reason_code, seg_data, seg_data_size,
	    next_segment_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_resp) +
	    seg_data_size - 1);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = encode_get_device_diagnostics_resp(instance_id, cc, reason_code,
						nullptr, seg_data_size,
						next_segment_id, response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// ===================================================================
// Tests for Get Network Device Debug Info
// ===================================================================

TEST(GetNetworkDeviceDebugInfo, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x16;
	uint8_t debug_type = INFO_TYPE_DEVICE_INFO;
	uint32_t handle = 0x12345678;

	auto rc = encode_get_network_device_debug_info_req(
	    instance_id, debug_type, handle, msg);

	auto req = reinterpret_cast<nsm_get_network_device_debug_info_req *>(
	    msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_DEBUG_INFO, req->hdr.command);
	EXPECT_EQ(debug_type, req->debug_info_type);
	EXPECT_EQ(handle, le32toh(req->record_handle));
}

TEST(GetNetworkDeviceDebugInfo, badTestEncodeRequest)
{
	uint8_t instance_id = 0x16;
	uint8_t debug_type = INFO_TYPE_DEVICE_INFO;
	uint32_t handle = 0x12345678;

	auto rc = encode_get_network_device_debug_info_req(
	    instance_id, debug_type, handle, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(GetNetworkDeviceDebugInfo, goodTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    6,				       // data size
	    INFO_TYPE_FW_RUNTIME_INFO,	       // debug type
	    0x00,			       // reserved
	    0x78,			       // record handle (little-endian)
	    0x56,
	    0x34,
	    0x12};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t debug_type = 0;
	uint32_t handle = 0;

	auto rc = decode_get_network_device_debug_info_req(
	    request, msg_len, &debug_type, &handle);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(INFO_TYPE_FW_RUNTIME_INFO, debug_type);
	EXPECT_EQ(0x12345678, handle);
}

TEST(GetNetworkDeviceDebugInfo, badTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x80,		 // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    6,				       // data size
	    INFO_TYPE_FW_RUNTIME_INFO,	       // debug type
	    0x00,			       // reserved
	    0x78,			       // record handle
	    0x56,
	    0x34,
	    0x12};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t debug_type = 0;
	uint32_t handle = 0;

	auto rc = decode_get_network_device_debug_info_req(
	    nullptr, msg_len, &debug_type, &handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_req(request, msg_len, nullptr,
						      &handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_req(request, msg_len,
						      &debug_type, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_req(request, msg_len - 4,
						      &debug_type, &handle);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(GetNetworkDeviceDebugInfo, goodTestEncodeResponse)
{
	uint8_t seg_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
	uint16_t seg_data_size = 4;
	uint32_t next_handle = 0x87654321;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp) + seg_data_size - 1);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x17;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;

	auto rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, seg_data, seg_data_size, next_handle,
	    response);

	auto resp = reinterpret_cast<nsm_get_network_device_debug_info_resp *>(
	    response->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_DEBUG_INFO, resp->hdr.command);
	EXPECT_EQ(next_handle, le32toh(resp->next_record_handle));
}

TEST(GetNetworkDeviceDebugInfo, badTestEncodeResponse)
{
	uint8_t seg_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
	uint16_t seg_data_size = 4;
	uint32_t next_handle = 0x87654321;
	uint8_t instance_id = 0x17;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;

	auto rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, seg_data, seg_data_size, next_handle,
	    nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_network_device_debug_info_resp) + seg_data_size - 1);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = encode_get_network_device_debug_info_resp(
	    instance_id, cc, reason_code, nullptr, seg_data_size, next_handle,
	    response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(GetNetworkDeviceDebugInfo, goodTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    NSM_SUCCESS,		       // completion code
	    0x00,
	    0x00,
	    0x08, // data size
	    0x00,
	    0x21, // next_record_handle (little-endian)
	    0x43,
	    0x65,
	    0x87,
	    0xAA, // segment data
	    0xBB,
	    0xCC,
	    0xDD};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint16_t seg_data_size = 0;
	std::vector<uint8_t> seg_data(65535, 0);
	uint32_t next_handle = 0;

	auto rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reason_code, &seg_data_size,
	    seg_data.data(), &next_handle);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(4, seg_data_size);
	EXPECT_EQ(0x87654321, next_handle);
	EXPECT_EQ(0xAA, seg_data[0]);
	EXPECT_EQ(0xBB, seg_data[1]);
	EXPECT_EQ(0xCC, seg_data[2]);
	EXPECT_EQ(0xDD, seg_data[3]);
}

TEST(GetNetworkDeviceDebugInfo, badTestDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_DEBUG_INFO, // command
	    NSM_SUCCESS,		       // completion code
	    0x00,
	    0x00,
	    0x08, // data size
	    0x00,
	    0x21, // next_record_handle
	    0x43,
	    0x65,
	    0x87,
	    0xAA, // segment data
	    0xBB,
	    0xCC,
	    0xDD};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;
	uint16_t seg_data_size = 0;
	std::vector<uint8_t> seg_data(65535, 0);
	uint32_t next_handle = 0;

	auto rc = decode_get_network_device_debug_info_resp(
	    nullptr, msg_len, &cc, &reason_code, &seg_data_size,
	    seg_data.data(), &next_handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, nullptr, &reason_code, &seg_data_size,
	    seg_data.data(), &next_handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reason_code, nullptr, seg_data.data(),
	    &next_handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reason_code, &seg_data_size, nullptr,
	    &next_handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len, &cc, &reason_code, &seg_data_size,
	    seg_data.data(), nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_debug_info_resp(
	    response, msg_len - 8, &cc, &reason_code, &seg_data_size,
	    seg_data.data(), &next_handle);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// ===================================================================
// Tests for Get Network Device Log Info
// ===================================================================

TEST(GetNetworkDeviceLogInfo, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t instance_id = 0x18;
	uint32_t record_handle = 0xABCDEF01;

	auto rc = encode_get_network_device_log_info_req(instance_id,
							 record_handle, msg);

	auto req = reinterpret_cast<nsm_get_network_device_log_info_req *>(
	    msg->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(instance_id, msg->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_LOG_INFO, req->hdr.command);
	EXPECT_EQ(record_handle, le32toh(req->record_handle));
}

TEST(GetNetworkDeviceLogInfo, badTestEncodeRequest)
{
	uint8_t instance_id = 0x18;
	uint32_t record_handle = 0xABCDEF01;

	auto rc = encode_get_network_device_log_info_req(
	    instance_id, record_handle, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(GetNetworkDeviceLogInfo, goodTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_LOG_INFO, // command
	    4,				     // data size
	    0x01,			     // record handle (little-endian)
	    0xEF,
	    0xCD,
	    0xAB};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint32_t record_handle = 0;

	auto rc = decode_get_network_device_log_info_req(request, msg_len,
							 &record_handle);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0xABCDEF01, record_handle);
}

TEST(GetNetworkDeviceLogInfo, badTestDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,	     // NVIDIA_MSG_TYPE
	    NSM_GET_NETWORK_DEVICE_LOG_INFO, // command
	    4,				     // data size
	    0x01,			     // record handle
	    0xEF,
	    0xCD,
	    0xAB};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint32_t record_handle = 0;

	auto rc = decode_get_network_device_log_info_req(nullptr, msg_len,
							 &record_handle);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_req(request, msg_len, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_get_network_device_log_info_req(request, msg_len - 3,
						    &record_handle);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(GetNetworkDeviceLogInfo, goodTestEncodeResponse)
{
	uint8_t log_data[] = {0x11, 0x22, 0x33, 0x44};
	uint16_t log_data_size = 4;
	uint32_t next_handle = 0xFEDCBA98;
	struct nsm_device_log_info_breakdown log_info = {};
	log_info.lost_events = 2;
	log_info.synced_time = 1;
	log_info.time_low = 0x12345678;
	log_info.time_high = 0x9ABCDEF0;
	log_info.entry_prefix = 0xABCDEF;
	log_info.length = 20;
	log_info.entry_suffix = 0x1122334455667788;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_resp) +
	    log_data_size - 1);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x19;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;

	auto rc = encode_get_network_device_log_info_resp(
	    instance_id, cc, reason_code, next_handle, log_info, log_data,
	    log_data_size, response);

	auto resp = reinterpret_cast<nsm_get_network_device_log_info_resp *>(
	    response->payload);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_NETWORK_DEVICE_LOG_INFO, resp->hdr.command);
	EXPECT_EQ(next_handle, le32toh(resp->next_record_handle));
}

TEST(GetNetworkDeviceLogInfo, badTestEncodeResponse)
{
	uint8_t log_data[] = {0x11, 0x22, 0x33, 0x44};
	uint16_t log_data_size = 4;
	uint32_t next_handle = 0xFEDCBA98;
	struct nsm_device_log_info_breakdown log_info = {};
	uint8_t instance_id = 0x19;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;

	auto rc = encode_get_network_device_log_info_resp(
	    instance_id, cc, reason_code, next_handle, log_info, log_data,
	    log_data_size, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_resp) +
	    log_data_size - 1);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = encode_get_network_device_log_info_resp(
	    instance_id, cc, reason_code, next_handle, log_info, nullptr,
	    log_data_size, response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Simple tests for uncovered functions
TEST(EraseTrace, EncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = encode_erase_trace_req(0, msg);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(EraseTrace, DecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = decode_erase_trace_req(msg, msgBuf.size());
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}
