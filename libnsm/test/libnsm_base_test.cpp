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
#include "common-tests.hpp"
#include "device-capability-discovery.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

TEST(PackNSMMessage, BadPathTest)
{
	struct nsm_header_info hdr;
	struct nsm_header_info *hdr_ptr = NULL;

	struct nsm_msg_hdr msg{};

	// header information pointer is NULL
	auto rc = pack_nsm_header(hdr_ptr, &msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// message pointer is NULL
	rc = pack_nsm_header(&hdr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// header information pointer and message pointer is NULL
	rc = pack_nsm_header(hdr_ptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Instance ID out of range
	hdr.nsm_msg_type = NSM_REQUEST;
	hdr.instance_id = 32;
	rc = pack_nsm_header(&hdr, &msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(UnpackNSMMessage, BadPathTest)
{
	struct nsm_header_info hdr;

	// message pointer is NULL
	auto rc = unpack_nsm_header(nullptr, &hdr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	struct nsm_msg_hdr msg{};

	// hdr pointer is NULL
	rc = unpack_nsm_header(&msg, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(ping, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_ping_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_PING, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(ping, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint16_t reason_code = 0;
	auto rc = encode_ping_resp(instance_id, reason_code, response);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_PING, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(ping, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_PING,				  // command
	    0,					  // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_ping_resp(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(getSupportedNvidiaMessageTypes, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_common_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_get_supported_nvidia_message_types_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getSupportedNvidiaMessageTypes, testGoodEncodeResponse)
{

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_supported_nvidia_message_types_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> types{0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				   0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				   0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				   0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

	auto rc = encode_get_supported_nvidia_message_types_resp(
	    instance_id, cc, reason_code, (bitfield8_t *)types.data(),
	    response);

	struct nsm_get_supported_nvidia_message_types_resp *resp =
	    reinterpret_cast<
		struct nsm_get_supported_nvidia_message_types_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES, resp->hdr.command);
	EXPECT_EQ(32, le16toh(resp->hdr.data_size));

	uint8_t responseTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];
	memcpy(responseTypes, resp->supported_nvidia_message_types,
	       SUPPORTED_MSG_TYPE_DATA_SIZE);
	EXPECT_THAT(responseTypes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(getSupportedNvidiaMessageTypes, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES,	  // command
	    0,					  // completion code
	    0,
	    0,
	    32,
	    0, // data size
	    0xf,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0 // types
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t responseTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];

	auto rc = decode_get_supported_nvidia_message_types_resp(
	    response, msg_len, &cc, &reason_code, (bitfield8_t *)responseTypes);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_THAT(responseTypes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(getSupportedNvidiaMessageTypes, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES,	  // command
	    0,					  // completion code
	    0,
	    0,
	    32,
	    0, // data size
	    0xf,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0 // types
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t responseTypes[SUPPORTED_MSG_TYPE_DATA_SIZE];

	auto rc = decode_get_supported_nvidia_message_types_resp(
	    nullptr, msg_len, &cc, &reason_code, (bitfield8_t *)responseTypes);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_nvidia_message_types_resp(
	    response, msg_len, nullptr, &reason_code,
	    (bitfield8_t *)responseTypes);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_nvidia_message_types_resp(
	    response, msg_len, &cc, nullptr, (bitfield8_t *)responseTypes);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_nvidia_message_types_resp(
	    response, msg_len, &cc, &reason_code, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_nvidia_message_types_resp(
	    response, msg_len - 4, &cc, &reason_code,
	    (bitfield8_t *)responseTypes);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getSupportedCommandCodes, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x12;
	uint8_t msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
	auto rc = encode_get_supported_command_codes_req(instance_id, msg_type,
							 request);

	struct nsm_get_supported_command_codes_req *req =
	    reinterpret_cast<struct nsm_get_supported_command_codes_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(instance_id, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_COMMAND_CODES, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(msg_type, req->nvidia_message_type);
}

TEST(getSupportedCommandCodes, testGoodEncodeResponse)
{

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> command_codes{
	    0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

	auto rc = encode_get_supported_command_codes_resp(
	    instance_id, cc, reason_code, (bitfield8_t *)command_codes.data(),
	    response);

	struct nsm_get_supported_command_codes_resp *resp =
	    reinterpret_cast<struct nsm_get_supported_command_codes_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SUPPORTED_COMMAND_CODES, resp->hdr.command);
	EXPECT_EQ(32, le16toh(resp->hdr.data_size));

	uint8_t responseCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];
	memcpy(responseCodes, resp->supported_command_codes,
	       SUPPORTED_COMMAND_CODE_DATA_SIZE);
	EXPECT_THAT(responseCodes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(getSupportedCommandCodes, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SUPPORTED_COMMAND_CODES,	  // command
	    0,					  // completion code
	    0,
	    0,
	    32,
	    0, // data size
	    0xf,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0,
	    0x0 // codes
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t responseCodes[SUPPORTED_COMMAND_CODE_DATA_SIZE];

	auto rc = decode_get_supported_command_codes_resp(
	    response, msg_len, &cc, &reason_code, (bitfield8_t *)responseCodes);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_THAT(responseCodes,
		    ElementsAre(0xf, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0));
}

TEST(queryDeviceIdentification, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t instance_id = 0x12;
	auto rc =
	    encode_nsm_query_device_identification_req(instance_id, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(instance_id, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_DEVICE_IDENTIFICATION, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(queryDeviceIdentification, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 0x12;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_identification = NSM_DEV_ID_GPU;
	uint8_t device_instance = 1;

	auto rc = encode_query_device_identification_resp(
	    instance_id, cc, reason_code, device_identification,
	    device_instance, response);

	struct nsm_query_device_identification_resp *resp =
	    reinterpret_cast<struct nsm_query_device_identification_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instance_id, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_DEVICE_IDENTIFICATION, resp->hdr.command);
	EXPECT_EQ(2, le16toh(resp->hdr.data_size));
	EXPECT_EQ(device_identification, resp->device_identification);
	EXPECT_EQ(device_instance, resp->instance_id);
}

TEST(queryDeviceIdentification, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0,					  // completion code
	    0,
	    0,
	    2,
	    0,		    // data size
	    NSM_DEV_ID_GPU, // device _identification
	    1		    // instance_id
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_identification = 0;
	uint8_t device_instance = 0;

	auto rc = decode_query_device_identification_resp(
	    response, msg_len, &cc, &reason_code, &device_identification,
	    &device_instance);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(NSM_DEV_ID_GPU, device_identification);
	EXPECT_EQ(1, device_instance);
}

TEST(encodeReasonCode, testGoodEncodeReasonCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_reason_code(cc, reasonCode, NSM_PING, response);

	struct nsm_common_non_success_resp *resp =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, resp->completion_code);
	EXPECT_EQ(NSM_PING, resp->command);
	EXPECT_EQ(ERR_NULL, le16toh(resp->reason_code));
}

TEST(encodeReasonCode, testBadEncodeReasonCode)
{
	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_reason_code(cc, reasonCode, NSM_PING, nullptr);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(EncodeCCOnlyResp, testEncodeCCOnlyRespWithError)
{
	// Test encode_cc_only_resp with non-success completion code
	// This covers line 337 in base.c (encode_reason_code path)
	std::vector<uint8_t> responseMsg(256, 0);
	auto response = reinterpret_cast<struct nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 5;
	uint8_t type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
	uint8_t command = NSM_PING;
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x1234;

	auto rc = encode_cc_only_resp(instance_id, type, command, cc,
				      reason_code, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header was populated correctly (response bit should be 0)
	EXPECT_EQ(response->hdr.request, 0);
	EXPECT_EQ(response->hdr.instance_id, instance_id & INSTANCEID_MASK);
	EXPECT_EQ(response->hdr.nvidia_msg_type, type);

	// Verify response payload contains reason code (line 337 path)
	auto resp_payload =
	    reinterpret_cast<struct nsm_common_non_success_resp *>(
		response->payload);
	EXPECT_EQ(resp_payload->command, command);
	EXPECT_EQ(resp_payload->completion_code, cc);
	EXPECT_EQ(le16toh(resp_payload->reason_code), reason_code);
}

TEST(EncodeCCOnlyResp, testEncodeCCOnlyRespSuccess)
{
	// Test encode_cc_only_resp with success completion code
	// This covers the normal success path (lines 340-347)
	std::vector<uint8_t> responseMsg(256, 0);
	auto response = reinterpret_cast<struct nsm_msg *>(responseMsg.data());

	uint8_t instance_id = 3;
	uint8_t type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
	uint8_t command = NSM_PING;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0; // Unused when cc == NSM_SUCCESS

	auto rc = encode_cc_only_resp(instance_id, type, command, cc,
				      reason_code, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header was populated correctly (response bit should be 0)
	EXPECT_EQ(response->hdr.request, 0);
	EXPECT_EQ(response->hdr.instance_id, instance_id & INSTANCEID_MASK);
	EXPECT_EQ(response->hdr.nvidia_msg_type, type);

	// Verify success response payload
	auto resp_payload =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);
	EXPECT_EQ(resp_payload->command, command);
	EXPECT_EQ(resp_payload->completion_code, cc);
	EXPECT_EQ(resp_payload->data_size, 0);
}

TEST(decodeReasonCodeCC, testGoodDecodeReasonCode)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0x01, // completion code != NSM_SUCCESS
	    0x00, // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_reason_code_and_cc(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERROR);
	EXPECT_EQ(reason_code, 0x0000);
}

TEST(decodeReasonCodeCC, testGoodDecodeCompletionCode)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0x00, // completion code = NSM_SUCCESS
	    0x00, // reason code
	    0x02};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_reason_code_and_cc(response, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
}

TEST(decodeReasonCode, testBadDecodeReasonCode)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDENTIFICATION,	  // command
	    0x01,				  // completion code
	    0x00,				  // reason code
	    0x00};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc =
	    decode_reason_code_and_cc(nullptr, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc =
	    decode_reason_code_and_cc(response, msg_len, nullptr, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_reason_code_and_cc(response, msg_len, &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(commonReq, testCommonRequest)
{
	testEncodeCommonRequest(
	    [](uint8_t instanceId, struct nsm_msg *msg) {
		    return encode_common_req(instanceId, 0, 0, msg);
	    },
	    0, 0);
	testDecodeCommonRequest(&decode_common_req, 0, 0);
}

TEST(commonResp, testCommonResponse)
{
	testEncodeCommonResponse(
	    [](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
	       nsm_msg *msg) {
		    return encode_common_resp(instanceId, cc, reasonCode, 0, 0,
					      msg);
	    },
	    0, 0);
	testDecodeCommonResponse(
	    [](const nsm_msg *msg, size_t len, uint8_t *cc,
	       uint16_t *reasonCode) {
		    uint16_t size = 0;
		    auto rc =
			decode_common_resp(msg, len, cc, &size, reasonCode);
		    EXPECT_EQ(0, size);
		    return rc;
	    },
	    0, 0);
}

TEST(commonReq, testEncodeRawMsg)
{
	auto encodePingRawCmdReq = [](uint8_t instanceId, nsm_msg *msg) {
		return encode_raw_cmd_req(instanceId,
					  NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
					  NSM_PING, nullptr, 0, msg);
	};
	testEncodeCommonRequest(encodePingRawCmdReq,
				NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_PING);

	Request responseMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(responseMsg.data());
	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  encode_raw_cmd_req(0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				     NSM_PING, nullptr, 1, msg));
}

TEST(LongRunning, testDecodeEventState)
{
	const uint8_t instanceId = 0;
	const uint8_t commandCode = 0x4d;
	Response responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    (instanceId & 0x1f), // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    0x08,			    // NSM_EVENT_VERSION=0, ACK=1, RES=0
	    NSM_LONG_RUNNING_EVENT,	    // EVENT_ID
	    NSM_NVIDIA_GENERAL_EVENT_CLASS, // EVENT_CLASS
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL,	  // EVENT_STATE >> 8
	    commandCode,			  // EVENT_STATE & 0xFF
	    sizeof(struct nsm_long_running_resp), // data size
	    instanceId,				  // instance id
	    0,					  // completion code
	    0,					  // reserved
	    0,					  // reserved
	    0,					  // data
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto len = responseMsg.size();
	uint8_t dataSize;
	uint16_t eventState;
	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  decode_nsm_event(response, len, NSM_LONG_RUNNING_EVENT,
				   NSM_NVIDIA_GENERAL_EVENT_CLASS, nullptr,
				   &dataSize));
	EXPECT_EQ(NSM_SW_ERROR_LENGTH,
		  decode_nsm_event_with_data(
		      response, sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN,
		      NSM_LONG_RUNNING_EVENT, NSM_NVIDIA_GENERAL_EVENT_CLASS,
		      &eventState, &dataSize, nullptr));
	EXPECT_EQ(NSM_SW_SUCCESS,
		  decode_nsm_event(response, len, NSM_LONG_RUNNING_EVENT,
				   NSM_NVIDIA_GENERAL_EVENT_CLASS, &eventState,
				   &dataSize));
	nsm_long_running_event_state state;
	memcpy(&state, &eventState, sizeof(uint16_t));
	EXPECT_EQ(0x4d, state.command);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL, state.nvidia_message_type);
}

TEST(LongRunning, testBadEncodeSize)
{
	Response responseMsg(sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			     sizeof(struct nsm_long_running_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto rc = encode_long_running_resp(0, NSM_SUCCESS, ERR_NULL,
					   NSM_TYPE_PLATFORM_ENVIRONMENTAL, 0,
					   nullptr, 0xFC, response);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
	auto event = reinterpret_cast<nsm_event *>(response->payload);
	EXPECT_EQ(0, event->data_size);
}

TEST(LongRunning, testGoodEncodeNullData)
{
	Response responseMsg(sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			     sizeof(struct nsm_long_running_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto rc = encode_long_running_resp(0, NSM_SUCCESS, ERR_NULL,
					   NSM_TYPE_PLATFORM_ENVIRONMENTAL,
					   0xFC, nullptr, 0, response);
	EXPECT_EQ(0, rc);
	auto event = reinterpret_cast<nsm_event *>(response->payload);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_EVENT_VERSION, event->version);
	EXPECT_EQ(false, event->ackr);
	EXPECT_EQ(0, event->resvd);
	EXPECT_EQ(NSM_LONG_RUNNING_EVENT, event->event_id);
	EXPECT_EQ(NSM_NVIDIA_GENERAL_EVENT_CLASS, event->event_class);
	EXPECT_EQ(NSM_TYPE_PLATFORM_ENVIRONMENTAL, event->event_state & 0xFF);
	EXPECT_EQ(0xFC, event->event_state >> 8);
	EXPECT_EQ(sizeof(nsm_long_running_resp), event->data_size);
}

TEST(LongRunning, testBadDecode)
{
	const uint8_t instanceId = 0;
	const uint8_t commandCode = 0x4d;
	Response responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    (instanceId & 0x1f), // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    0x08,			    // NSM_EVENT_VERSION=0, ACK=1, RES=0
	    NSM_LONG_RUNNING_EVENT,	    // EVENT_ID
	    NSM_NVIDIA_GENERAL_EVENT_CLASS, // EVENT_CLASS
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL,	  // EVENT_STATE >> 8
	    commandCode,			  // EVENT_STATE & 0xFF
	    sizeof(struct nsm_long_running_resp), // data size
	    instanceId,				  // instance id
	    0,					  // completion code
	    0,					  // reserved
	    0,					  // reserved
	    0,					  // data
	};
	uint8_t data;
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto event = reinterpret_cast<nsm_event *>(response->payload);
	auto longRunning =
	    reinterpret_cast<nsm_long_running_resp *>(event->data);
	auto len = responseMsg.size();
	auto badLen = sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
		      sizeof(struct nsm_long_running_resp) - 1;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;

	auto rc = decode_long_running_resp_with_data(
	    response, badLen, NSM_TYPE_PLATFORM_ENVIRONMENTAL, commandCode, &cc,
	    &reasonCode, &data, sizeof(bitfield8_t));
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	const auto offset = [response](uint8_t *data) {
		return size_t(data) - size_t(response);
	};

	responseMsg[offset(&event->data_size)] =
	    sizeof(struct nsm_long_running_resp) - 1;
	rc = decode_long_running_resp_with_data(
	    response, len, NSM_TYPE_PLATFORM_ENVIRONMENTAL, commandCode, &cc,
	    &reasonCode, &data, sizeof(bitfield8_t));
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	responseMsg[offset(&event->data_size)] =
	    sizeof(struct nsm_long_running_resp);

	responseMsg[offset(&event->data_size)] =
	    sizeof(struct nsm_long_running_resp) + 1;
	rc = decode_long_running_resp_with_data(
	    response, badLen, NSM_TYPE_PLATFORM_ENVIRONMENTAL, commandCode, &cc,
	    &reasonCode, &data, sizeof(bitfield8_t));
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	responseMsg[offset(&longRunning->completion_code)] = NSM_ERROR;
	rc = decode_long_running_resp_with_data(
	    response, len, NSM_TYPE_PLATFORM_ENVIRONMENTAL, commandCode, &cc,
	    &reasonCode, &data, sizeof(bitfield8_t));
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(cc, NSM_ERROR);
	responseMsg[offset(&longRunning->completion_code)] = NSM_SUCCESS;
}

// Tests for uncovered functions - Functional 90% coverage target

TEST(decodeNsmEventAcknowledgement, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_event_ack));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up header for event acknowledgement
	msg->hdr.request = 0;
	msg->hdr.datagram = 1;
	msg->hdr.instance_id = 5;
	msg->hdr.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	// Set up event ack payload
	auto event_ack = reinterpret_cast<nsm_event_ack *>(msg->payload);
	event_ack->event_id = 0x42;

	uint8_t instance_id = 0;
	uint8_t nsm_type = 0;
	uint8_t event_id = 0;

	auto rc = decode_nsm_event_acknowledgement(
	    msg, msgBuf.size(), &instance_id, &nsm_type, &event_id);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(instance_id, 5);
	EXPECT_EQ(nsm_type, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
	EXPECT_EQ(event_id, 0x42);
}

TEST(decodeNsmEventAcknowledgement, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_event_ack));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t instance_id = 0;
	uint8_t nsm_type = 0;
	uint8_t event_id = 0;

	// Null message pointer
	auto rc = decode_nsm_event_acknowledgement(
	    nullptr, msgBuf.size(), &instance_id, &nsm_type, &event_id);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);

	// Null output pointers
	rc = decode_nsm_event_acknowledgement(msg, msgBuf.size(), nullptr,
					      &nsm_type, &event_id);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);

	// Invalid message length
	rc = decode_nsm_event_acknowledgement(msg, 1, &instance_id, &nsm_type,
					      &event_id);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(decodeCommonReqV2, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req_v2));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION_V2;
	msg->hdr.request = 1;
	msg->hdr.datagram = 0;

	auto rc = decode_common_req_v2(msg, msgBuf.size());

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(decodeCommonReqV2, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req_v2));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Null message pointer
	auto rc = decode_common_req_v2(nullptr, msgBuf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Invalid message length
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION_V2;
	rc = decode_common_req_v2(msg, sizeof(nsm_msg_hdr) - 1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(decodeGetHistogramFormatReq, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_histogram_format_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up request payload
	auto req =
	    reinterpret_cast<nsm_get_histogram_format_req *>(msg->payload);
	req->hdr.command = NSM_GET_HISTOGRAM_FORMAT;
	req->hdr.data_size = sizeof(req->histogram_id) + sizeof(req->parameter);
	req->histogram_id = htole32(0x12345678);
	req->parameter = htole16(0xABCD);

	uint32_t histogram_id = 0;
	uint16_t parameter = 0;

	auto rc = decode_get_histogram_format_req(msg, msgBuf.size(),
						  &histogram_id, &parameter);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(histogram_id, 0x12345678);
	EXPECT_EQ(parameter, 0xABCD);
}

TEST(decodeGetHistogramFormatReq, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_histogram_format_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint32_t histogram_id = 0;
	uint16_t parameter = 0;

	// Null message pointer
	auto rc = decode_get_histogram_format_req(nullptr, msgBuf.size(),
						  &histogram_id, &parameter);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Null output pointers
	rc = decode_get_histogram_format_req(msg, msgBuf.size(), nullptr,
					     &parameter);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Invalid message length
	rc = decode_get_histogram_format_req(msg, 1, &histogram_id, &parameter);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(decodeGetHistogramDataReq, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_histogram_data_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up request payload
	auto req = reinterpret_cast<nsm_get_histogram_data_req *>(msg->payload);
	req->hdr.command = NSM_GET_HISTOGRAM_DATA;
	req->hdr.data_size = sizeof(req->histogram_id) + sizeof(req->parameter);
	req->histogram_id = htole32(0x87654321);
	req->parameter = htole16(0xDCBA);

	uint32_t histogram_id = 0;
	uint16_t parameter = 0;

	auto rc = decode_get_histogram_data_req(msg, msgBuf.size(),
						&histogram_id, &parameter);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(histogram_id, 0x87654321);
	EXPECT_EQ(parameter, 0xDCBA);
}

TEST(decodeGetHistogramDataReq, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_histogram_data_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint32_t histogram_id = 0;
	uint16_t parameter = 0;

	// Null message pointer
	auto rc = decode_get_histogram_data_req(nullptr, msgBuf.size(),
						&histogram_id, &parameter);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Null output pointers
	rc = decode_get_histogram_data_req(msg, msgBuf.size(), nullptr,
					   &parameter);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Invalid message length
	rc = decode_get_histogram_data_req(msg, 1, &histogram_id, &parameter);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(decodeGetGpioStateReq, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_gpio_state_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up request payload
	auto req = reinterpret_cast<nsm_get_gpio_state_req *>(msg->payload);
	req->hdr.command = NSM_GET_GPIO_STATE;
	req->hdr.data_size = htole16(sizeof(req->offset) + sizeof(req->length));
	req->offset = htole16(0x1234);
	req->length = htole16(0x0008);

	uint16_t offset = 0;
	uint16_t length = 0;

	auto rc =
	    decode_get_gpio_state_req(msg, msgBuf.size(), &offset, &length);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(offset, 0x1234);
	EXPECT_EQ(length, 0x0008);
}

TEST(decodeGetGpioStateReq, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_get_gpio_state_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint16_t offset = 0;
	uint16_t length = 0;

	// Null message pointer
	auto rc =
	    decode_get_gpio_state_req(nullptr, msgBuf.size(), &offset, &length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Null output pointers
	rc = decode_get_gpio_state_req(msg, msgBuf.size(), nullptr, &length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpio_state_req(msg, msgBuf.size(), &offset, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Invalid message length
	rc = decode_get_gpio_state_req(msg, 1, &offset, &length);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(encodeSetGpioStateReq, testGoodEncode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_gpio_state_req) + 8);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t instance_id = 7;
	uint16_t offset = 0x5678;
	uint16_t length = 0x0008;
	uint8_t gpio_values[8] = {0x01, 0x02, 0x03, 0x04,
				  0x05, 0x06, 0x07, 0x08};
	uint32_t gpio_values_size = 8;

	auto rc = encode_set_gpio_state_req(instance_id, offset, length,
					    gpio_values, gpio_values_size, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.request, 1);
	EXPECT_EQ(msg->hdr.datagram, 0);

	auto req = reinterpret_cast<nsm_set_gpio_state_req *>(msg->payload);
	EXPECT_EQ(req->hdr.command, NSM_SET_GPIO_STATE);
	EXPECT_EQ(le16toh(req->offset), offset);
	EXPECT_EQ(le16toh(req->length), length);
	EXPECT_EQ(req->gpio_values[0], 0x01);
}

TEST(encodeSetGpioStateReq, testBadEncode)
{
	uint8_t gpio_values[8] = {0};

	// Null message pointer
	auto rc = encode_set_gpio_state_req(0, 0, 0, gpio_values, 8, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decodeSetGpioStateReq, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_gpio_state_req) + 8);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up request payload
	auto req = reinterpret_cast<nsm_set_gpio_state_req *>(msg->payload);
	req->hdr.command = NSM_SET_GPIO_STATE;
	req->hdr.data_size =
	    htole16(sizeof(req->offset) + sizeof(req->length) + 8);
	req->offset = htole16(0x9ABC);
	req->length = htole16(0x0008);
	req->gpio_values[0] = 0x01;

	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t gpio_values[8] = {0};
	uint32_t gpio_values_size = 0;

	auto rc =
	    decode_set_gpio_state_req(msg, msgBuf.size(), &offset, &length,
				      gpio_values, &gpio_values_size);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(offset, 0x9ABC);
	EXPECT_EQ(length, 0x0008);
	EXPECT_EQ(gpio_values[0], 0x01);
	EXPECT_EQ(gpio_values_size, 8);
}

TEST(decodeSetGpioStateReq, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_gpio_state_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t gpio_values[8] = {0};
	uint32_t gpio_values_size = 0;

	// Null message pointer
	auto rc =
	    decode_set_gpio_state_req(nullptr, msgBuf.size(), &offset, &length,
				      gpio_values, &gpio_values_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Null output pointers
	rc = decode_set_gpio_state_req(msg, msgBuf.size(), nullptr, &length,
				       gpio_values, &gpio_values_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Invalid message length
	rc = decode_set_gpio_state_req(msg, 1, &offset, &length, gpio_values,
				       &gpio_values_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(encodeSetGpioStateResp, testGoodEncode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_gpio_state_resp) + 8);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t instance_id = 9;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = 0;
	uint16_t offset = 0x1000;
	uint16_t length = 0x0008;
	uint8_t gpio_values[8] = {0x01, 0x02, 0x03, 0x04,
				  0x05, 0x06, 0x07, 0x08};
	uint32_t gpio_values_size = 8;

	auto rc = encode_set_gpio_state_resp(instance_id, cc, reason_code,
					     offset, length, gpio_values,
					     gpio_values_size, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.request, 0);
	EXPECT_EQ(msg->hdr.datagram, 0);

	auto resp = reinterpret_cast<nsm_set_gpio_state_resp *>(msg->payload);
	EXPECT_EQ(resp->hdr.command, NSM_SET_GPIO_STATE);
	EXPECT_EQ(resp->hdr.completion_code, cc);
	EXPECT_EQ(le16toh(resp->offset), offset);
	EXPECT_EQ(le16toh(resp->length), length);
}

TEST(encodeSetGpioStateResp, testBadEncode)
{
	uint8_t gpio_values[8] = {0};

	// Null message pointer
	auto rc = encode_set_gpio_state_resp(0, NSM_SUCCESS, 0, 0, 0,
					     gpio_values, 8, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decodeSetGpioStateResp, testGoodDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_gpio_state_resp) + 8);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up response payload
	auto resp = reinterpret_cast<nsm_set_gpio_state_resp *>(msg->payload);
	resp->hdr.command = NSM_SET_GPIO_STATE;
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->hdr.data_size =
	    htole16(sizeof(resp->offset) + sizeof(resp->length) + 8);
	resp->offset = htole16(0x1000);
	resp->length = htole16(0x0008);
	resp->gpio_values[0] = 0x01;

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t gpio_values[8] = {0};
	uint32_t gpio_values_size = 0;

	auto rc = decode_set_gpio_state_resp(msg, msgBuf.size(), &cc,
					     &reason_code, &offset, &length,
					     gpio_values, &gpio_values_size);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(offset, 0x1000);
	EXPECT_EQ(length, 0x0008);
	EXPECT_EQ(gpio_values[0], 0x01);
}

TEST(decodeSetGpioStateResp, testBadDecode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_set_gpio_state_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t offset = 0;
	uint16_t length = 0;
	uint8_t gpio_values[8] = {0};
	uint32_t gpio_values_size = 0;

	// Null message pointer
	auto rc = decode_set_gpio_state_resp(nullptr, msgBuf.size(), &cc,
					     &reason_code, &offset, &length,
					     gpio_values, &gpio_values_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Null output pointers
	rc = decode_set_gpio_state_resp(msg, msgBuf.size(), nullptr,
					&reason_code, &offset, &length,
					gpio_values, &gpio_values_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
// Simple decode test
TEST(CommonRequest, DecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	// First encode a valid message
	auto rc = encode_common_req(0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				    NSM_PING, msg);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	// Now decode it
	rc = decode_common_req(msg, msgBuf.size());
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}
