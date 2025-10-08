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
#include "debug-token.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(provideToken, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_provide_token_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t token[NSM_DEBUG_TOKEN_DATA_MAX_SIZE];
	for (uint32_t i = 0; i < NSM_DEBUG_TOKEN_DATA_MAX_SIZE; ++i) {
		token[i] = i;
	}

	auto rc = encode_nsm_provide_token_req(
	    0, token, NSM_DEBUG_TOKEN_DATA_MAX_SIZE, request);

	nsm_provide_token_req *req = (nsm_provide_token_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_PROVIDE_TOKEN, req->hdr.command);
	EXPECT_EQ(NSM_DEBUG_TOKEN_DATA_MAX_SIZE, req->hdr.data_size);
	EXPECT_EQ(
	    0, memcmp(token, req->token_data, NSM_DEBUG_TOKEN_DATA_MAX_SIZE));
}

TEST(provideToken, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_provide_token_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t token[NSM_DEBUG_TOKEN_DATA_MAX_SIZE];
	for (uint32_t i = 0; i < NSM_DEBUG_TOKEN_DATA_MAX_SIZE; ++i) {
		token[i] = i;
	}

	auto rc = encode_nsm_provide_token_req(0, token, 0, request);

	EXPECT_EQ(NSM_ERR_INVALID_DATA, rc);
}

TEST(provideToken, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		 // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_PROVIDE_TOKEN,	 // command
	    NSM_SUCCESS,	 // completion code
	    0,
	    0, // reserved
	    0,
	    0, // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	auto rc =
	    decode_nsm_provide_token_resp(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
}

TEST(disableTokens, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_disable_tokens_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_disable_tokens_req(0, request);

	nsm_disable_tokens_req *req =
	    (nsm_disable_tokens_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_DISABLE_TOKENS, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(disableTokens, testBadEncodeRequest)
{
	auto rc = encode_nsm_disable_tokens_req(0, nullptr);

	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(disableTokens, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_DISABLE_TOKENS,	 // command
	    NSM_SUCCESS,	 // completion code
	    0,
	    0, // reserved
	    0,
	    0, // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_nsm_disable_tokens_resp(response, msg_len, &cc,
						 &reason_code);

	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
}

TEST(queryTokenStatus, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_query_token_status_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	nsm_debug_token_type token_type = NSM_DEBUG_TOKEN_TYPE_FRC;

	auto rc = encode_nsm_query_token_status_req(0, token_type, request);

	nsm_query_token_status_req *req =
	    (nsm_query_token_status_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_TOKEN_STATUS, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(NSM_DEBUG_TOKEN_TYPE_FRC, req->token_type);
}

TEST(queryTokenStatus, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_query_token_status_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t token_type = 0xFF;

	auto rc = encode_nsm_query_token_status_req(
	    0, static_cast<nsm_debug_token_type>(token_type), request);

	EXPECT_EQ(NSM_ERR_INVALID_DATA, rc);
}

TEST(queryTokenStatus, testGoodDecodeResponse)
{
	uint16_t data_size =
	    sizeof(nsm_query_token_status_resp) - sizeof(nsm_common_resp);
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		    // PCI VID: NVIDIA 0x10DE
	    0x00,		    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		    // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,    // NVIDIA_MSG_TYPE
	    NSM_QUERY_TOKEN_STATUS, // command
	    NSM_SUCCESS,	    // completion code
	    0,
	    0,						 // reserved
	    (uint8_t)(data_size & 0x00FF),		 // data size
	    (uint8_t)((data_size & 0xFF00) >> 8),	 // data size
	    NSM_DEBUG_TOKEN_TYPE_CRCS,			 // token type
	    0,						 // reserved
	    NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE, // additional info
	    NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,	 // status
	    0x12,					 // time left
	    0x34,					 // time left
	    0x56,					 // time left
	    0x78					 // time left
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	nsm_debug_token_status status;
	nsm_debug_token_status_additional_info additional_info;
	nsm_debug_token_type token_type;
	uint32_t time_left;

	auto rc = decode_nsm_query_token_status_resp(
	    response, msg_len, &cc, &reason_code, &status, &additional_info,
	    &token_type, &time_left);

	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED, status);
	EXPECT_EQ(NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE, additional_info);
	EXPECT_EQ(NSM_DEBUG_TOKEN_TYPE_CRCS, token_type);
	EXPECT_EQ(0x78563412, time_left);
}

TEST(queryTokenParameters, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_query_token_parameters_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	nsm_debug_token_opcode token_opcode = NSM_DEBUG_TOKEN_OPCODE_CRCS;

	auto rc =
	    encode_nsm_query_token_parameters_req(0, token_opcode, request);

	nsm_query_token_parameters_req *req =
	    (nsm_query_token_parameters_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_TOKEN_PARAMETERS, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(NSM_DEBUG_TOKEN_OPCODE_CRCS, req->token_opcode);
}

TEST(queryTokenParameters, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_query_token_parameters_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t token_opcode = 0xFF;

	auto rc = encode_nsm_query_token_parameters_req(
	    0, static_cast<nsm_debug_token_opcode>(token_opcode), request);

	EXPECT_EQ(NSM_ERR_INVALID_DATA, rc);
}

TEST(queryTokenParameters, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10, 0xde, 0x00, 0x89, 0x04, 0x54, 0x00, 0x00, 0x00, 0x8c, 0x00,
	    0x00, 0x00, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x1c, 0x76, 0xc0, 0xc4, 0xfc, 0xaf, 0x17, 0x24, 0x03,
	    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x59, 0xbf, 0x4a, 0x04,
	    0x3d, 0xdd, 0x11, 0xef, 0xb9, 0x4f, 0xac, 0x1f, 0x6b, 0x01, 0xe5,
	    0xae, 0x1c, 0x76, 0xc0, 0xc4, 0xfc, 0xaf, 0x17, 0x24, 0x4e, 0x56,
	    0x44, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x35, 0x30,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x23, 0x07, 0xde, 0x04, 0x48,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc7, 0xfe,
	    0x66, 0xd4, 0xb4, 0x5c, 0x4e, 0xae, 0xdc, 0x42, 0xdc, 0x25, 0xc7,
	    0xc6, 0x8c, 0xcf, 0x7c, 0x1d, 0x85, 0x7d, 0x6f, 0x63, 0x66, 0x7b,
	    0xaa, 0xdf, 0xb3, 0xcb, 0x4b, 0x37, 0x8d, 0x38};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	nsm_debug_token_request token_request;
	memset(&token_request, 0, sizeof(nsm_debug_token_request));

	auto rc = decode_nsm_query_token_parameters_resp(
	    response, msg_len, &cc, &reason_code, &token_request);

	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(sizeof(nsm_debug_token_request),
		  token_request.token_request_size);
	EXPECT_EQ(0, token_request.token_request_version);
	EXPECT_EQ(0x1C, token_request.device_uuid[0]);
	EXPECT_EQ(0x76, token_request.device_uuid[1]);
	EXPECT_EQ(0xC0, token_request.device_uuid[2]);
	EXPECT_EQ(0xC4, token_request.device_uuid[3]);
	EXPECT_EQ(0xFC, token_request.device_uuid[4]);
	EXPECT_EQ(0xAF, token_request.device_uuid[5]);
	EXPECT_EQ(0x17, token_request.device_uuid[6]);
	EXPECT_EQ(0x24, token_request.device_uuid[7]);
	EXPECT_EQ(NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_NVSWITCH,
		  token_request.device_type);
	EXPECT_EQ(0, token_request.device_index);
	EXPECT_EQ(NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_OK,
		  token_request.status);
	EXPECT_EQ(NSM_DEBUG_TOKEN_OPCODE_CRDT, token_request.token_opcode);
	EXPECT_EQ(0x23, token_request.fw_version[0]);
	EXPECT_EQ(0x07, token_request.fw_version[1]);
	EXPECT_EQ(0xDE, token_request.fw_version[2]);
	EXPECT_EQ(0x04, token_request.fw_version[3]);
	EXPECT_EQ(0x48, token_request.fw_version[4]);
	EXPECT_EQ(0, token_request.session_id);
	EXPECT_EQ(0, token_request.challenge_version);
}

TEST(queryDeviceIDs, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_query_device_ids_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_query_device_ids_req(0, request);

	nsm_query_device_ids_req *req =
	    (nsm_query_device_ids_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_QUERY_DEVICE_IDS, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(queryDeviceIDs, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_device_ids_resp) + 8);

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_id[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	auto rc = encode_nsm_query_device_ids_resp(0, cc, reason_code,
						   device_id, 8, response);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);

	nsm_query_device_ids_resp *resp =
	    (nsm_query_device_ids_resp *)response->payload;
	EXPECT_EQ(NSM_QUERY_DEVICE_IDS, resp->hdr.command);
	EXPECT_EQ(cc, resp->hdr.completion_code);
	EXPECT_EQ(8, le16toh(resp->hdr.data_size));
	EXPECT_EQ(0, memcmp(device_id, resp->data, 8));
}

TEST(queryDeviceIDs, testBadEncodeResponse)
{
	uint8_t device_id[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	// null message
	auto rc = encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL,
						   device_id, 8, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// null device_id
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_device_ids_resp) + 8);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	rc = encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL, nullptr,
					      8, response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// zero device_id_len
	rc = encode_nsm_query_device_ids_resp(0, NSM_SUCCESS, ERR_NULL,
					      device_id, 0, response);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(queryDeviceIDs, testEncodeResponseWithError)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_device_ids_resp) + 8);

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = NSM_ERR_INVALID_DATA;
	uint8_t device_id[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

	auto rc = encode_nsm_query_device_ids_resp(0, cc, reason_code,
						   device_id, 8, response);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
}

TEST(queryDeviceIDs, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x00,		  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDS, // command
	    NSM_SUCCESS,	  // completion code
	    0,
	    0,	  // reserved
	    8,	  // data size (little endian)
	    0,	  // data size
	    0x01, // ID
	    0x02, // ID
	    0x03, // ID
	    0x04, // ID
	    0x05, // ID
	    0x06, // ID
	    0x07, // ID
	    0x08, // ID
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_id[8];
	size_t device_id_len = 0;

	auto rc = decode_nsm_query_device_ids_resp(
	    response, msg_len, &cc, &reason_code, device_id, &device_id_len);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(8, device_id_len);
	EXPECT_EQ(0x01, device_id[0]);
	EXPECT_EQ(0x02, device_id[1]);
	EXPECT_EQ(0x03, device_id[2]);
	EXPECT_EQ(0x04, device_id[3]);
	EXPECT_EQ(0x05, device_id[4]);
	EXPECT_EQ(0x06, device_id[5]);
	EXPECT_EQ(0x07, device_id[6]);
	EXPECT_EQ(0x08, device_id[7]);
}

TEST(queryDeviceIDs, testDecodeResponseWithNullDeviceId)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x00,		  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDS, // command
	    NSM_SUCCESS,	  // completion code
	    0,
	    0,	  // reserved
	    8,	  // data size (little endian)
	    0,	  // data size
	    0x01, // ID
	    0x02, // ID
	    0x03, // ID
	    0x04, // ID
	    0x05, // ID
	    0x06, // ID
	    0x07, // ID
	    0x08, // ID
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	size_t device_id_len = 0;

	// Test with null device_id pointer (should still work)
	auto rc = decode_nsm_query_device_ids_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &device_id_len);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(8, device_id_len);
}

TEST(queryDeviceIDs, testBadDecodeResponse)
{
	uint8_t cc;
	uint16_t reason_code;
	uint8_t device_id[8];
	size_t device_id_len;

	// null message
	auto rc = decode_nsm_query_device_ids_resp(
	    nullptr, 100, &cc, &reason_code, device_id, &device_id_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// null parameters
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_device_ids_resp) + 8);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = decode_nsm_query_device_ids_resp(response, responseMsg.size(),
					      nullptr, &reason_code, device_id,
					      &device_id_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_query_device_ids_resp(response, responseMsg.size(), &cc,
					      nullptr, device_id,
					      &device_id_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_query_device_ids_resp(response, responseMsg.size(), &cc,
					      &reason_code, device_id, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// insufficient message length
	rc = decode_nsm_query_device_ids_resp(response, 10, &cc, &reason_code,
					      device_id, &device_id_len);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(queryDeviceIDs, testDecodeResponseWithError)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		  // PCI VID: NVIDIA 0x10DE
	    0x00,		  // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		  // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC,  // NVIDIA_MSG_TYPE
	    NSM_QUERY_DEVICE_IDS, // command
	    NSM_ERROR,		  // completion code
	    ERR_TIMEOUT,	  // reason code
	    0,
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_id[8];
	size_t device_id_len = 0;

	auto rc = decode_nsm_query_device_ids_resp(
	    response, msg_len, &cc, &reason_code, device_id, &device_id_len);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(ERR_TIMEOUT, reason_code);
}

TEST(installToken, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_install_token_req) + 100);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint32_t chunk_offset = 0x12345678;
	uint32_t chunk_length = 64;
	uint32_t length_remaining = 1024;
	uint8_t data[64];
	for (uint32_t i = 0; i < chunk_length; ++i) {
		data[i] = i & 0xFF;
	}

	auto rc = encode_nsm_install_token_req(0, chunk_offset, chunk_length,
					       length_remaining, data, request);
	nsm_install_token_req *req = (nsm_install_token_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_INSTALL_TOKEN, req->hdr.command);
	EXPECT_EQ(sizeof(chunk_offset) + sizeof(chunk_length) +
		      sizeof(length_remaining) + chunk_length,
		  le16toh(req->hdr.data_size));
	EXPECT_EQ(chunk_offset, le32toh(req->chunk_offset));
	EXPECT_EQ(chunk_length, le32toh(req->chunk_length));
	EXPECT_EQ(length_remaining, le32toh(req->length_remaining));
	EXPECT_EQ(0, memcmp(data, req->data, chunk_length));
}

TEST(installToken, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_install_token_req) + 100);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t data[64];

	// null message
	auto rc = encode_nsm_install_token_req(0, 0, 64, 1024, data, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// null data
	rc = encode_nsm_install_token_req(0, 0, 64, 1024, nullptr, request);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(installToken, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_install_token_req) + 100);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint32_t expected_chunk_offset = 0x12345678;
	uint32_t expected_chunk_length = 64;
	uint32_t expected_length_remaining = 1024;
	uint8_t expected_data[64];
	for (uint32_t i = 0; i < expected_chunk_length; ++i) {
		expected_data[i] = i & 0xFF;
	}
	auto rc = encode_nsm_install_token_req(
	    0, expected_chunk_offset, expected_chunk_length,
	    expected_length_remaining, expected_data, request);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	uint32_t chunk_offset;
	uint32_t chunk_length;
	uint32_t length_remaining;
	uint8_t data[64];
	rc = decode_nsm_install_token_req(request, requestMsg.size(),
					  &chunk_offset, &chunk_length,
					  &length_remaining, data);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(expected_chunk_offset, chunk_offset);
	EXPECT_EQ(expected_chunk_length, chunk_length);
	EXPECT_EQ(expected_length_remaining, length_remaining);
	EXPECT_EQ(0, memcmp(expected_data, data, expected_chunk_length));
}

TEST(installToken, testBadDecodeRequest)
{
	uint32_t chunk_offset;
	uint32_t chunk_length;
	uint32_t length_remaining;
	uint8_t data[64];

	// null message
	auto rc = decode_nsm_install_token_req(nullptr, 100, &chunk_offset,
					       &chunk_length, &length_remaining,
					       data);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// null parameters
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_install_token_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	rc = decode_nsm_install_token_req(request, requestMsg.size(), nullptr,
					  &chunk_length, &length_remaining,
					  data);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_install_token_req(request, requestMsg.size(),
					  &chunk_offset, nullptr,
					  &length_remaining, data);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_install_token_req(request, requestMsg.size(),
					  &chunk_offset, &chunk_length, nullptr,
					  data);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// insufficient message length
	rc = decode_nsm_install_token_req(
	    request, 10, &chunk_offset, &chunk_length, &length_remaining, data);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(eraseToken, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_token_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint32_t token_type = 0x12345678;
	auto rc = encode_nsm_erase_token_req(0, token_type, request);
	nsm_erase_token_req *req = (nsm_erase_token_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ERASE_TOKEN, req->hdr.command);
	EXPECT_EQ(sizeof(token_type), req->hdr.data_size);
	EXPECT_EQ(token_type, le32toh(req->token_type));
}

TEST(eraseToken, testBadEncodeRequest)
{
	auto rc = encode_nsm_erase_token_req(0, 0x12345678, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(eraseToken, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_token_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint32_t expected_token_type = 0x87654321;
	auto rc = encode_nsm_erase_token_req(0, expected_token_type, request);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	uint32_t token_type;
	rc =
	    decode_nsm_erase_token_req(request, requestMsg.size(), &token_type);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(expected_token_type, token_type);
}

TEST(eraseToken, testBadDecodeRequest)
{
	uint32_t token_type;

	// null message
	auto rc = decode_nsm_erase_token_req(nullptr, 100, &token_type);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// null parameter
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_erase_token_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	rc = decode_nsm_erase_token_req(request, requestMsg.size(), nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// insufficient message length
	rc = decode_nsm_erase_token_req(request, 10, &token_type);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);

	// invalid data size
	nsm_erase_token_req *req = (nsm_erase_token_req *)request->payload;
	req->hdr.data_size = 10; // Invalid size
	rc =
	    decode_nsm_erase_token_req(request, requestMsg.size(), &token_type);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(queryToken, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_token_resp) + 100);

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint8_t tlv_payload[64];
	size_t tlv_payload_len = 32;
	for (size_t i = 0; i < tlv_payload_len; ++i) {
		tlv_payload[i] = i & 0xFF;
	}

	auto rc = encode_nsm_query_token_resp(0, cc, reason_code, tlv_payload,
					      tlv_payload_len, response);
	nsm_query_token_resp *resp = (nsm_query_token_resp *)response->payload;
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_QUERY_TOKEN, resp->hdr.command);
	EXPECT_EQ(cc, resp->hdr.completion_code);
	EXPECT_EQ(tlv_payload_len, le16toh(resp->hdr.data_size));
	EXPECT_EQ(0, memcmp(tlv_payload, resp->tlv_payload, tlv_payload_len));
}

TEST(queryToken, testBadEncodeResponse)
{
	uint8_t tlv_payload[64] = {0};

	auto rc = encode_nsm_query_token_resp(0, NSM_SUCCESS, ERR_NULL,
					      tlv_payload, 32, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

TEST(queryToken, testGoodDecodeResponse)
{
	uint16_t data_size = 32;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DIAGNOSTIC, // NVIDIA_MSG_TYPE
	    NSM_QUERY_TOKEN,	 // command
	    NSM_SUCCESS,	 // completion code
	    0,
	    0,					 // reserved
	    (uint8_t)(data_size & 0x00FF),	 // data size
	    (uint8_t)((data_size & 0xFF00) >> 8) // data size
	};
	for (uint16_t i = 0; i < data_size; ++i) {
		responseMsg.push_back(i & 0xFF);
	}

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint8_t tlv_payload[64];
	size_t tlv_payload_len = sizeof(tlv_payload);
	auto rc =
	    decode_nsm_query_token_resp(response, msg_len, &cc, &reason_code,
					tlv_payload, &tlv_payload_len);
	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(data_size, tlv_payload_len);
	for (uint16_t i = 0; i < data_size; ++i) {
		EXPECT_EQ(i & 0xFF, tlv_payload[i]);
	}
}

TEST(queryToken, testBadDecodeResponse)
{
	uint8_t cc;
	uint16_t reason_code;
	uint8_t tlv_payload[64];
	size_t tlv_payload_len;

	// null message
	auto rc = decode_nsm_query_token_resp(nullptr, 100, &cc, &reason_code,
					      tlv_payload, &tlv_payload_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// null parameters
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_query_token_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = decode_nsm_query_token_resp(response, responseMsg.size(), nullptr,
					 &reason_code, tlv_payload,
					 &tlv_payload_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc =
	    decode_nsm_query_token_resp(response, responseMsg.size(), &cc,
					nullptr, tlv_payload, &tlv_payload_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_query_token_resp(response, responseMsg.size(), &cc,
					 &reason_code, tlv_payload, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// insufficient message length
	rc = decode_nsm_query_token_resp(response, 10, &cc, &reason_code,
					 tlv_payload, &tlv_payload_len);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}
