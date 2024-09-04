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
	    0,				    // reserved
	    NSM_DEBUG_TOKEN_DEVICE_ID_SIZE, // data size
	    0,				    // data size
	    0x01,			    // ID
	    0x02,			    // ID
	    0x03,			    // ID
	    0x04,			    // ID
	    0x05,			    // ID
	    0x06,			    // ID
	    0x07,			    // ID
	    0x08,			    // ID
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint8_t device_id[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE];

	auto rc = decode_nsm_query_device_ids_resp(response, msg_len, &cc,
						   &reason_code, device_id);

	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(0x01, device_id[0]);
	EXPECT_EQ(0x02, device_id[1]);
	EXPECT_EQ(0x03, device_id[2]);
	EXPECT_EQ(0x04, device_id[3]);
	EXPECT_EQ(0x05, device_id[4]);
	EXPECT_EQ(0x06, device_id[5]);
	EXPECT_EQ(0x07, device_id[6]);
	EXPECT_EQ(0x08, device_id[7]);
}
