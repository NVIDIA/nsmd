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

	uint8_t type = 0;
	uint8_t data_size =
	    sizeof(struct nsm_erase_trace_req) - sizeof(struct nsm_common_req);

	auto rc = encode_erase_trace_req(0, type, request);

	struct nsm_erase_trace_req *req =
	    reinterpret_cast<struct nsm_erase_trace_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_TRACE, req->hdr.command);
	EXPECT_EQ(data_size, req->hdr.data_size);
	EXPECT_EQ(type, req->info_type);
}

TEST(eraseTrace, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_trace_req));

	auto rc = encode_erase_trace_req(0, 0, nullptr);

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
	    0x02,		 // data size
	    0x01,		 // debug_info_type
	    0x00};		 // reserved

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t type;
	auto rc = decode_erase_trace_req(request, msg_len, &type);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(01, type);
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
	    0x04,		 // data size [shouldn't be 4]
	    0x01,		 // debug_info_type
	    0x00};		 // reserved

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t type;
	auto rc = decode_erase_trace_req(nullptr, msg_len, &type);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_trace_req(request, msg_len, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_erase_trace_req(request, msg_len - 2, &type);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_erase_trace_req(request, msg_len, &type);
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
	auto rc = decode_get_network_device_log_info_req(request, msg_len,
							&handle);

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
	log_info.reserved = 00;
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