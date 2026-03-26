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

// Test to cover line 208 in debug-token.c (message length validation)
TEST(provideToken, DISABLED_testDecodeResponseShortMessage)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 NSM_RESPONSE_ERROR_LEN); // Too short
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_nsm_provide_token_resp(response, responseMsg.size(),
						&cc, &reason_code);

	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Test to cover line 203 in debug-token.c (error completion code path)
TEST(provideToken, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_PROVIDE_TOKEN;
	resp->completion_code = NSM_ERR_INVALID_DATA;
	resp->reason_code = htole16(0xCCCC);

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	auto rc = decode_nsm_provide_token_resp(msg, msgBuf.size(), &cc,
						&reason_code);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0xCCCC, reason_code);
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

// Test to cover line 296 in debug-token.c (message length validation)
TEST(disableTokens, DISABLED_testDecodeResponseShortMessage)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 NSM_RESPONSE_ERROR_LEN); // Too short
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_nsm_disable_tokens_resp(response, responseMsg.size(),
						 &cc, &reason_code);

	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Test to cover line 291 in debug-token.c (error completion code path)
TEST(disableTokens, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_DISABLE_TOKENS;
	resp->completion_code = NSM_ERR_INVALID_DATA;
	resp->reason_code = htole16(0xDDDD);

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	auto rc = decode_nsm_disable_tokens_resp(msg, msgBuf.size(), &cc,
						 &reason_code);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0xDDDD, reason_code);
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

// Test to cover line 400 in debug-token.c (error completion code path)
TEST(queryTokenStatus, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_QUERY_TOKEN_STATUS;
	resp->completion_code = NSM_ERR_INVALID_DATA;
	resp->reason_code = htole16(0xBBBB);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	nsm_debug_token_status status;
	nsm_debug_token_status_additional_info additional_info;
	nsm_debug_token_type token_type;
	uint32_t time_left;

	auto rc = decode_nsm_query_token_status_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &status, &additional_info,
	    &token_type, &time_left);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0xBBBB, reason_code);
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

// Test to cover line 89 in debug-token.c (error completion code path)
TEST(queryTokenParameters, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_QUERY_TOKEN_PARAMETERS;
	resp->completion_code = NSM_ERR_INVALID_DATA;
	resp->reason_code = htole16(0xEEEE);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	nsm_debug_token_request token_request;
	memset(&token_request, 0, sizeof(nsm_debug_token_request));

	auto rc = decode_nsm_query_token_parameters_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &token_request);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0xEEEE, reason_code);
}

// Test to cover line 101 in debug-token.c (token_request_size validation)
TEST(queryTokenParameters, decodeResponseInvalidTokenRequestSize)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_parameters_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	auto resp =
	    reinterpret_cast<nsm_query_token_parameters_resp *>(msg->payload);
	resp->hdr.command = NSM_QUERY_TOKEN_PARAMETERS;
	resp->hdr.completion_code = NSM_SUCCESS;
	resp->hdr.data_size = htole16(sizeof(nsm_query_token_parameters_resp) -
				      sizeof(nsm_common_resp));

	// Set invalid token_request_size (should be
	// sizeof(nsm_debug_token_request))
	resp->token_request.token_request_size = 0;

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	nsm_debug_token_request token_request;
	memset(&token_request, 0, sizeof(nsm_debug_token_request));

	auto rc = decode_nsm_query_token_parameters_resp(
	    msg, msgBuf.size(), &cc, &reason_code, &token_request);

	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
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

// Test to cover line 862 in debug-token.c (error completion code path)
TEST(queryToken, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x8899;
	uint8_t tlv_payload[64] = {0};

	auto rc = encode_nsm_query_token_resp(0, cc, reason_code, tlv_payload,
					      32, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_QUERY_TOKEN, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
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

// Test to cover line 821 in debug-token.c (error completion code path)
TEST(queryToken, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_QUERY_TOKEN;
	resp->completion_code = NSM_ERR_INVALID_DATA;
	resp->reason_code = htole16(0x9AAA);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t tlv_payload[64];
	size_t tlv_payload_len = sizeof(tlv_payload);

	auto rc =
	    decode_nsm_query_token_resp(msg, msgBuf.size(), &cc, &reason_code,
					tlv_payload, &tlv_payload_len);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0x9AAA, reason_code);
}

// Tests for uncovered functions - Functional 90% coverage target

TEST(queryTokenParameters, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_parameters_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req =
	    reinterpret_cast<nsm_query_token_parameters_req *>(msg->payload);
	req->hdr.command = NSM_QUERY_TOKEN_PARAMETERS;
	req->hdr.data_size = sizeof(req->token_opcode);
	req->token_opcode = NSM_DEBUG_TOKEN_OPCODE_RMCS;

	enum nsm_debug_token_opcode token_opcode =
	    NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC;

	auto rc = decode_nsm_query_token_parameters_req(msg, msgBuf.size(),
							&token_opcode);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_DEBUG_TOKEN_OPCODE_RMCS, token_opcode);
}

TEST(queryTokenParameters, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_parameters_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	enum nsm_debug_token_opcode token_opcode =
	    NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC;

	// Null message
	auto rc = decode_nsm_query_token_parameters_req(nullptr, msgBuf.size(),
							&token_opcode);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Null output parameter
	rc = decode_nsm_query_token_parameters_req(msg, msgBuf.size(), nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Invalid message length
	rc = decode_nsm_query_token_parameters_req(msg, 1, &token_opcode);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Test to cover line 38 in debug-token.c (data_size validation)
TEST(queryTokenParameters, decodeRequestInvalidDataSize)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_parameters_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req =
	    reinterpret_cast<nsm_query_token_parameters_req *>(msg->payload);
	req->hdr.command = NSM_QUERY_TOKEN_PARAMETERS;
	req->hdr.data_size =
	    0; // Invalid - should be at least sizeof(token_opcode)
	req->token_opcode = NSM_DEBUG_TOKEN_OPCODE_CRCS;

	enum nsm_debug_token_opcode token_opcode =
	    NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC;

	auto rc = decode_nsm_query_token_parameters_req(msg, msgBuf.size(),
							&token_opcode);

	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(queryTokenParameters, goodTestEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_parameters_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	struct nsm_debug_token_request token_request = {};
	token_request.token_request_size =
	    sizeof(struct nsm_debug_token_request);

	auto rc = encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, 0,
							 &token_request, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);

	auto resp =
	    reinterpret_cast<nsm_query_token_parameters_resp *>(msg->payload);
	EXPECT_EQ(NSM_QUERY_TOKEN_PARAMETERS, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
}

TEST(queryTokenParameters, badTestEncodeResponse)
{
	struct nsm_debug_token_request token_request = {};

	// Null message
	auto rc = encode_nsm_query_token_parameters_resp(
	    0, NSM_SUCCESS, 0, &token_request, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 127 in debug-token.c (error completion code path)
TEST(queryTokenParameters, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	struct nsm_debug_token_request token_request = {};
	token_request.token_request_size =
	    sizeof(struct nsm_debug_token_request);

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0xABCD;

	auto rc = encode_nsm_query_token_parameters_resp(0, cc, reason_code,
							 &token_request, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_QUERY_TOKEN_PARAMETERS, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
}

TEST(provideToken, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_provide_token_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req = reinterpret_cast<nsm_provide_token_req *>(msg->payload);
	req->hdr.command = NSM_PROVIDE_TOKEN;
	req->hdr.data_size = 32;
	memset(req->token_data, 0xAB, 32);

	uint8_t token_data[NSM_DEBUG_TOKEN_DATA_MAX_SIZE] = {0};
	uint8_t token_data_len = 0;

	auto rc = decode_nsm_provide_token_req(msg, msgBuf.size(), token_data,
					       &token_data_len);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(32, token_data_len);
	EXPECT_EQ(0xAB, token_data[0]);
}

TEST(provideToken, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_provide_token_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t token_data[NSM_DEBUG_TOKEN_DATA_MAX_SIZE] = {0};
	uint8_t token_data_len = 0;

	// Null message
	auto rc = decode_nsm_provide_token_req(nullptr, msgBuf.size(),
					       token_data, &token_data_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Null output parameters
	rc = decode_nsm_provide_token_req(msg, msgBuf.size(), nullptr,
					  &token_data_len);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_provide_token_req(msg, msgBuf.size(), token_data,
					  nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Invalid message length
	rc = decode_nsm_provide_token_req(msg, 1, token_data, &token_data_len);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Test to cover line 156 in debug-token.c (data_size = 0 validation)
TEST(provideToken, decodeRequestZeroDataSize)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_provide_token_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req = reinterpret_cast<nsm_provide_token_req *>(msg->payload);
	req->hdr.command = NSM_PROVIDE_TOKEN;
	req->hdr.data_size = 0; // Invalid - should be > 0

	uint8_t token_data[NSM_DEBUG_TOKEN_DATA_MAX_SIZE] = {0};
	uint8_t token_data_len = 0;

	auto rc = decode_nsm_provide_token_req(msg, msgBuf.size(), token_data,
					       &token_data_len);

	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(provideToken, goodTestEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_provide_token_resp(0, NSM_SUCCESS, 0, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
}

TEST(provideToken, badTestEncodeResponse)
{
	// Null message
	auto rc = encode_nsm_provide_token_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 232 in debug-token.c (error completion code path)
TEST(provideToken, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x9876;

	auto rc = encode_nsm_provide_token_resp(0, cc, reason_code, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_PROVIDE_TOKEN, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
}

TEST(disableTokens, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_disable_tokens_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = decode_nsm_disable_tokens_req(msg, msgBuf.size());

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(disableTokens, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_disable_tokens_req));

	// Null message
	auto rc = decode_nsm_disable_tokens_req(nullptr, msgBuf.size());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Invalid message length
	rc = decode_nsm_disable_tokens_req(msg, 1);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(disableTokens, goodTestEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_disable_tokens_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_disable_tokens_resp(0, NSM_SUCCESS, 0, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
}

TEST(disableTokens, badTestEncodeResponse)
{
	// Null message
	auto rc = encode_nsm_disable_tokens_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 320 in debug-token.c (error completion code path)
TEST(disableTokens, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x5432;

	auto rc = encode_nsm_disable_tokens_resp(0, cc, reason_code, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_DISABLE_TOKENS, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
}

TEST(queryTokenStatus, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_status_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req = reinterpret_cast<nsm_query_token_status_req *>(msg->payload);
	req->hdr.command = NSM_QUERY_TOKEN_STATUS;
	req->hdr.data_size = sizeof(req->token_type);
	req->token_type = NSM_DEBUG_TOKEN_TYPE_FRC;

	enum nsm_debug_token_type token_type = NSM_DEBUG_TOKEN_TYPE_CRCS;

	auto rc =
	    decode_nsm_query_token_status_req(msg, msgBuf.size(), &token_type);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_DEBUG_TOKEN_TYPE_FRC, token_type);
}

TEST(queryTokenStatus, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_status_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	enum nsm_debug_token_type token_type = NSM_DEBUG_TOKEN_TYPE_CRCS;

	// Null message
	auto rc = decode_nsm_query_token_status_req(nullptr, msgBuf.size(),
						    &token_type);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Null output parameter
	rc = decode_nsm_query_token_status_req(msg, msgBuf.size(), nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Invalid message length
	rc = decode_nsm_query_token_status_req(msg, 1, &token_type);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Test to cover line 347 in debug-token.c (data_size validation)
TEST(queryTokenStatus, decodeRequestInvalidDataSize)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_status_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req = reinterpret_cast<nsm_query_token_status_req *>(msg->payload);
	req->hdr.command = NSM_QUERY_TOKEN_STATUS;
	req->hdr.data_size =
	    0; // Invalid - should be at least sizeof(token_type)
	req->token_type = NSM_DEBUG_TOKEN_TYPE_CRCS;

	enum nsm_debug_token_type token_type = NSM_DEBUG_TOKEN_TYPE_FRC;

	auto rc =
	    decode_nsm_query_token_status_req(msg, msgBuf.size(), &token_type);

	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(queryTokenStatus, goodTestEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_status_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_query_token_status_resp(
	    0, NSM_SUCCESS, 0, NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
	    NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
	    NSM_DEBUG_TOKEN_TYPE_FRC, 3600, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);

	auto resp =
	    reinterpret_cast<nsm_query_token_status_resp *>(msg->payload);
	EXPECT_EQ(NSM_QUERY_TOKEN_STATUS, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
}

TEST(queryTokenStatus, badTestEncodeResponse)
{
	// Null message
	auto rc = encode_nsm_query_token_status_resp(
	    0, NSM_SUCCESS, 0, NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
	    NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
	    NSM_DEBUG_TOKEN_TYPE_FRC, 3600, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 440 in debug-token.c (error completion code path)
TEST(queryTokenStatus, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x1122;

	auto rc = encode_nsm_query_token_status_resp(
	    0, cc, reason_code, NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
	    NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
	    NSM_DEBUG_TOKEN_TYPE_FRC, 3600, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_QUERY_TOKEN_STATUS, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
}

TEST(queryDeviceIds, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_device_ids_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = decode_nsm_query_device_ids_req(msg, msgBuf.size());

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(queryDeviceIds, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_device_ids_req));

	// Null message
	auto rc = decode_nsm_query_device_ids_req(nullptr, msgBuf.size());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Invalid message length
	rc = decode_nsm_query_device_ids_req(msg, 1);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(installToken, goodTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_install_token_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	auto resp = reinterpret_cast<nsm_install_token_resp *>(msg->payload);
	resp->command = NSM_INSTALL_TOKEN;
	resp->completion_code = NSM_SUCCESS;

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	auto rc = decode_nsm_install_token_resp(msg, msgBuf.size(), &cc,
						&reason_code);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
}

TEST(installToken, badTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_install_token_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;

	// Null message
	auto rc = decode_nsm_install_token_resp(nullptr, msgBuf.size(), &cc,
						&reason_code);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Null output parameters
	rc = decode_nsm_install_token_resp(msg, msgBuf.size(), nullptr,
					   &reason_code);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_install_token_resp(msg, msgBuf.size(), &cc, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 640 in debug-token.c (error completion code path)
TEST(installToken, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_INSTALL_TOKEN;
	resp->completion_code = NSM_ERR_INVALID_DATA; // Error completion code
	resp->reason_code = htole16(0x3344);

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	// decode should succeed but return early due to cc != NSM_SUCCESS
	auto rc = decode_nsm_install_token_resp(msg, msgBuf.size(), &cc,
						&reason_code);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0x3344, reason_code);
}

TEST(installToken, goodTestEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_install_token_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_install_token_resp(0, NSM_SUCCESS, 0, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
}

TEST(installToken, badTestEncodeResponse)
{
	// Null message
	auto rc = encode_nsm_install_token_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 664 in debug-token.c (error completion code path)
TEST(installToken, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x6677;

	auto rc = encode_nsm_install_token_resp(0, cc, reason_code, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_INSTALL_TOKEN, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
}

TEST(eraseToken, goodTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_erase_token_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	auto resp = reinterpret_cast<nsm_erase_token_resp *>(msg->payload);
	resp->command = NSM_ERASE_TOKEN;
	resp->completion_code = NSM_SUCCESS;

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	auto rc =
	    decode_nsm_erase_token_resp(msg, msgBuf.size(), &cc, &reason_code);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
}

TEST(eraseToken, badTestDecodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_erase_token_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;

	// Null message
	auto rc = decode_nsm_erase_token_resp(nullptr, msgBuf.size(), &cc,
					      &reason_code);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Null output parameters
	rc = decode_nsm_erase_token_resp(msg, msgBuf.size(), nullptr,
					 &reason_code);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	rc = decode_nsm_erase_token_resp(msg, msgBuf.size(), &cc, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 733 in debug-token.c (error completion code path)
TEST(eraseToken, decodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_non_success_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Set up valid response header
	msg->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->hdr.ocp_type = OCP_TYPE;
	msg->hdr.ocp_version = OCP_VERSION;

	// Set up error response with reason code
	auto resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	resp->command = NSM_ERASE_TOKEN;
	resp->completion_code = NSM_ERR_INVALID_DATA;
	resp->reason_code = htole16(0x5566);

	uint8_t cc = 0;
	uint16_t reason_code = 0;

	auto rc =
	    decode_nsm_erase_token_resp(msg, msgBuf.size(), &cc, &reason_code);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERR_INVALID_DATA, cc);
	EXPECT_EQ(0x5566, reason_code);
}

TEST(eraseToken, goodTestEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_erase_token_resp));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_erase_token_resp(0, NSM_SUCCESS, 0, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, msg->hdr.request);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);
}

TEST(eraseToken, badTestEncodeResponse)
{
	// Null message
	auto rc = encode_nsm_erase_token_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Test to cover line 757 in debug-token.c (error completion code path)
TEST(eraseToken, errorCompletionCodeEncodeResponse)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + 256);
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Test with non-success completion code to trigger error path
	uint8_t cc = NSM_ERR_INVALID_DATA;
	uint16_t reason_code = 0x7788;

	auto rc = encode_nsm_erase_token_resp(0, cc, reason_code, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	// Verify error response structure
	auto resp_payload =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(NSM_ERASE_TOKEN, resp_payload->command);
	EXPECT_EQ(cc, resp_payload->completion_code);
	EXPECT_EQ(reason_code, le16toh(resp_payload->reason_code));
}

TEST(queryToken, goodTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req = reinterpret_cast<nsm_query_token_req *>(msg->payload);
	req->command = NSM_QUERY_TOKEN;
	req->data_size = 0;

	auto rc = decode_nsm_query_token_req(msg, msgBuf.size());

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(queryToken, badTestDecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_req));

	// Null message
	auto rc = decode_nsm_query_token_req(nullptr, msgBuf.size());
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	// Invalid message length
	rc = decode_nsm_query_token_req(msg, 1);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

// Test to cover line 781 in debug-token.c (non-zero data_size validation)
TEST(queryToken, decodeRequestInvalidDataSize)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto req = reinterpret_cast<nsm_query_token_req *>(msg->payload);
	req->command = NSM_QUERY_TOKEN;
	req->data_size = 1; // Invalid - should be 0

	auto rc = decode_nsm_query_token_req(msg, msgBuf.size());

	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(queryToken, goodTestEncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_query_token_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());

	auto rc = encode_nsm_query_token_req(0, msg);

	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, msg->hdr.request);
	EXPECT_EQ(0, msg->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DIAGNOSTIC, msg->hdr.nvidia_msg_type);

	auto req = reinterpret_cast<nsm_query_token_req *>(msg->payload);
	EXPECT_EQ(NSM_QUERY_TOKEN, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(queryToken, badTestEncodeRequest)
{
	// Null message
	auto rc = encode_nsm_query_token_req(0, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
}

// Simple decode tests
TEST(DisableTokens, DecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = decode_nsm_disable_tokens_req(msg, msgBuf.size());
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(QueryDeviceIds, DecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = decode_nsm_query_device_ids_req(msg, msgBuf.size());
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(QueryToken, DecodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = decode_nsm_query_token_req(msg, msgBuf.size());
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

// More simple encode tests
TEST(DisableTokens, EncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = encode_nsm_disable_tokens_req(0, msg);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(QueryDeviceIds, EncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = encode_nsm_query_device_ids_req(0, msg);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

TEST(QueryToken, EncodeRequest)
{
	std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) +
				    sizeof(nsm_common_req));
	auto msg = reinterpret_cast<nsm_msg *>(msgBuf.data());
	auto rc = encode_nsm_query_token_req(0, msg);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

// Branch coverage: encode_nsm_query_token_parameters_req L49
TEST(DebugTokenNullBranch, EncodeQueryTokenParamReq_NullMsg)
{
	auto rc = encode_nsm_query_token_parameters_req(
	    0, NSM_DEBUG_TOKEN_OPCODE_RMCS, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: encode_nsm_query_token_parameters_resp L112
TEST(DebugTokenNullBranch, EncodeQueryTokenParamResp_NullMsg)
{
	auto rc = encode_nsm_query_token_parameters_resp(0, NSM_SUCCESS, 0,
							 nullptr, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: decode_nsm_query_token_parameters_resp L83
// (msg == NULL || token_request == NULL)
TEST(DebugTokenNullBranch, DecodeQueryTokenParamResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	struct nsm_debug_token_request token_request = {};
	auto rc = decode_nsm_query_token_parameters_resp(
	    nullptr, 0, &cc, &reason_code, &token_request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenNullBranch, DecodeQueryTokenParamResp_NullTokenRequest)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_nsm_query_token_parameters_resp(msg, buf.size(), &cc,
							 &reason_code, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: encode_nsm_provide_token_req L168
// (msg == NULL || token_data == NULL)
TEST(DebugTokenNullBranch, EncodeProvideTokenReq_NullMsg)
{
	uint8_t token_data[4] = {};
	auto rc = encode_nsm_provide_token_req(0, token_data, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenNullBranch, EncodeProvideTokenReq_NullTokenData)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_nsm_provide_token_req(0, nullptr, 4, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: encode_nsm_query_token_status_req L358
TEST(DebugTokenNullBranch, EncodeQueryTokenStatusReq_NullMsg)
{
	auto rc = encode_nsm_query_token_status_req(0, NSM_DEBUG_TOKEN_TYPE_FRC,
						    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: decode_nsm_query_token_status_resp L393
// (msg == NULL || status == NULL || additional_info == NULL || ...)
TEST(DebugTokenNullBranch, DecodeQueryTokenStatusResp_NullMsg)
{
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info additional_info = {};
	enum nsm_debug_token_type token_type = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(
	    nullptr, 0, &cc, &reason_code, &status, &additional_info,
	    &token_type, &time_left);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DebugTokenNullBranch, DecodeQueryTokenStatusResp_NullStatus)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum nsm_debug_token_status_additional_info additional_info = {};
	enum nsm_debug_token_type token_type = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason_code, nullptr, &additional_info,
	    &token_type, &time_left);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: encode_nsm_query_device_ids_req L474
TEST(DebugTokenNullBranch, EncodeQueryDeviceIdsReq_NullMsg)
{
	auto rc = encode_nsm_query_device_ids_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ---------------------------------------------------------------------------
// Null checks for secondary conditions (cc/reason_code/time_left/etc == NULL)
// ---------------------------------------------------------------------------

// decode_nsm_provide_token_resp L197: cc == NULL
TEST(DebugTokenNullBranch, DecodeProvideTokenResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 8, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason_code = 0;
	auto rc = decode_nsm_provide_token_resp(msg, buf.size(), nullptr,
						&reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_disable_tokens_resp L285: cc == NULL
TEST(DebugTokenNullBranch, DecodeDisableTokensResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 8, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason_code = 0;
	auto rc = decode_nsm_disable_tokens_resp(msg, buf.size(), nullptr,
						 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_query_token_status_resp L394: token_type == NULL
TEST(DebugTokenNullBranch, DecodeQueryTokenStatusResp_NullTokenType)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 8, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info additional_info = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason_code, &status, &additional_info,
	    nullptr, &time_left);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// Pack-fail branches (instance_id > NSM_INSTANCE_MAX = 31)
// ---------------------------------------------------------------------------

static std::vector<uint8_t> g_dbg_buf(1024, 0);
static nsm_msg *g_dbg_msg()
{
	return reinterpret_cast<nsm_msg *>(g_dbg_buf.data());
}

// encode_nsm_query_token_parameters_req L66
TEST(DebugTokenPackFailBranch, EncodeQueryTokenParamReq_PackFail)
{
	auto rc = encode_nsm_query_token_parameters_req(
	    32, NSM_DEBUG_TOKEN_OPCODE_RMCS, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_token_parameters_resp L122
TEST(DebugTokenPackFailBranch, EncodeQueryTokenParamResp_PackFail)
{
	auto rc = encode_nsm_query_token_parameters_resp(32, NSM_SUCCESS, 0,
							 nullptr, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_provide_token_req L181
TEST(DebugTokenPackFailBranch, EncodeProvideTokenReq_PackFail)
{
	uint8_t token_data[1] = {0};
	auto rc = encode_nsm_provide_token_req(32, token_data, 1, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_provide_token_resp L227
TEST(DebugTokenPackFailBranch, EncodeProvideTokenResp_PackFail)
{
	auto rc =
	    encode_nsm_provide_token_resp(32, NSM_SUCCESS, 0, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_disable_tokens_req L270
TEST(DebugTokenPackFailBranch, EncodeDisableTokensReq_PackFail)
{
	auto rc = encode_nsm_disable_tokens_req(32, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_disable_tokens_resp L315
TEST(DebugTokenPackFailBranch, EncodeDisableTokensResp_PackFail)
{
	auto rc =
	    encode_nsm_disable_tokens_resp(32, NSM_SUCCESS, 0, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_token_status_req L374
TEST(DebugTokenPackFailBranch, EncodeQueryTokenStatusReq_PackFail)
{
	auto rc = encode_nsm_query_token_status_req(
	    32, NSM_DEBUG_TOKEN_TYPE_FRC, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_token_status_resp L435
TEST(DebugTokenPackFailBranch, EncodeQueryTokenStatusResp_PackFail)
{
	auto rc = encode_nsm_query_token_status_resp(
	    32, NSM_SUCCESS, 0, static_cast<nsm_debug_token_status>(0),
	    static_cast<nsm_debug_token_status_additional_info>(0),
	    static_cast<nsm_debug_token_type>(0), 0, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_device_ids_req L484
TEST(DebugTokenPackFailBranch, EncodeQueryDeviceIdsReq_PackFail)
{
	auto rc = encode_nsm_query_device_ids_req(32, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_device_ids_resp L543
TEST(DebugTokenPackFailBranch, EncodeQueryDeviceIdsResp_PackFail)
{
	uint8_t dev_id[1] = {0};
	auto rc = encode_nsm_query_device_ids_resp(32, NSM_SUCCESS, 0, dev_id,
						   1, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_install_token_req L613
TEST(DebugTokenPackFailBranch, EncodeInstallTokenReq_PackFail)
{
	uint8_t data[1] = {0};
	auto rc = encode_nsm_install_token_req(32, 0, 0, 0, data, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_install_token_resp L659
TEST(DebugTokenPackFailBranch, EncodeInstallTokenResp_PackFail)
{
	auto rc =
	    encode_nsm_install_token_resp(32, NSM_SUCCESS, 0, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_erase_token_req L711
TEST(DebugTokenPackFailBranch, EncodeEraseTokenReq_PackFail)
{
	auto rc = encode_nsm_erase_token_req(32, 0, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_erase_token_resp L752
TEST(DebugTokenPackFailBranch, EncodeEraseTokenResp_PackFail)
{
	auto rc = encode_nsm_erase_token_resp(32, NSM_SUCCESS, 0, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_token_req L799
TEST(DebugTokenPackFailBranch, EncodeQueryTokenReq_PackFail)
{
	auto rc = encode_nsm_query_token_req(32, g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// encode_nsm_query_token_resp L857
TEST(DebugTokenPackFailBranch, EncodeQueryTokenResp_PackFail)
{
	uint8_t payload[1] = {0};
	auto rc = encode_nsm_query_token_resp(32, NSM_SUCCESS, 0, payload, 1,
					      g_dbg_msg());
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// rc != NSM_SW_SUCCESS branches (B0 of || condition)
// Pass msg with error CC + wrong size → decode_reason_code_and_cc returns
// NSM_SW_ERROR_LENGTH, so rc != NSM_SW_SUCCESS → TRUE branch taken
// ===========================================================================
// Error-CC buffer helper: large buffer with completion_code = NSM_ERROR
// payload[1] (not payload[0]) is completion_code in nsm_common_resp
static std::vector<uint8_t> makeErrCcBuf(size_t size = sizeof(nsm_msg_hdr) +
						       100)
{
	std::vector<uint8_t> buf(size, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code != NSM_SUCCESS
	return buf;
}

// decode_nsm_query_token_parameters_resp L88 — rc != NSM_SW_SUCCESS
TEST(DebugTokenBranches, DecodeQueryTokenParamResp_RcFail)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_debug_token_request token_req = {};
	auto rc = decode_nsm_query_token_parameters_resp(msg, buf.size(), &cc,
							 &reason, &token_req);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// decode_nsm_provide_token_resp L197 — rc != NSM_SW_SUCCESS
TEST(DebugTokenBranches, DecodeProvideTokenResp_RcFail)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_provide_token_resp(msg, buf.size(), &cc, &reason);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// decode_nsm_disable_tokens_resp L285 — reason_code=NULL
TEST(DebugTokenBranches, DecodeDisableTokensResp_NullReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	auto rc = decode_nsm_disable_tokens_resp(msg, buf.size(), &cc, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_disable_tokens_resp L290 — rc != NSM_SW_SUCCESS
TEST(DebugTokenBranches, DecodeDisableTokensResp_RcFail)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_nsm_disable_tokens_resp(msg, buf.size(), &cc, &reason);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// decode_nsm_query_token_status_resp L394 — time_left=NULL
TEST(DebugTokenBranches, DecodeQueryTokenStatusResp_NullTimeLeft)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info additional_info = {};
	enum nsm_debug_token_type token_type = {};
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason, &status, &additional_info,
	    &token_type, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_query_token_status_resp L399 — rc != NSM_SW_SUCCESS
TEST(DebugTokenBranches, DecodeQueryTokenStatusResp_RcFail)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_debug_token_status status = {};
	enum nsm_debug_token_status_additional_info additional_info = {};
	enum nsm_debug_token_type token_type = {};
	uint32_t time_left = 0;
	auto rc = decode_nsm_query_token_status_resp(
	    msg, buf.size(), &cc, &reason, &status, &additional_info,
	    &token_type, &time_left);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// decode_nsm_query_token_resp L820 — rc != NSM_SW_SUCCESS
TEST(DebugTokenBranches, DecodeQueryTokenResp_RcFail)
{
	auto buf = makeErrCcBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	size_t tlv_payload_len = 0;
	auto rc = decode_nsm_query_token_resp(msg, buf.size(), &cc, &reason,
					      nullptr, &tlv_payload_len);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_nsm_install_token_req branch coverage
// L577: data_size too small, L588: size mismatch, L591: data=NULL
// ===========================================================================

// L577 TRUE: data_size < minimum (zero-fill → data_size=0)
TEST(DebugTokenBranches, DecodeInstallTokenReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_req), 0);
	// data_size = 0 (zero-fill) < 13 → L577 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t chunk_offset = 0, chunk_length = 0, length_remaining = 0;
	auto rc = decode_nsm_install_token_req(msg, buf.size(), &chunk_offset,
					       &chunk_length, &length_remaining,
					       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L588 TRUE: data_size >= 13 but != expected (data_size=13, chunk_length=5)
TEST(DebugTokenBranches, DecodeInstallTokenReq_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_req) + 10, 0);
	// data_size at payload offset 2 = 13 (LE16): passes L577
	buf[sizeof(nsm_msg_hdr) + 2] = 13;
	buf[sizeof(nsm_msg_hdr) + 3] = 0;
	// chunk_length at payload offset 10 = 5: expected_size = 12+5=17 ≠ 13
	buf[sizeof(nsm_msg_hdr) + 10] = 5;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t chunk_offset = 0, chunk_length = 0, length_remaining = 0;
	auto rc = decode_nsm_install_token_req(msg, buf.size(), &chunk_offset,
					       &chunk_length, &length_remaining,
					       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L591 FALSE: data=NULL, valid data_size=13, chunk_length=1 → skip copy
TEST(DebugTokenBranches, DecodeInstallTokenReq_NullData)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_req) + 10, 0);
	// data_size=13: passes L577 (>= 13)
	buf[sizeof(nsm_msg_hdr) + 2] = 13;
	buf[sizeof(nsm_msg_hdr) + 3] = 0;
	// chunk_length=1: expected_size = 4+4+4+1 = 13 = data_size → passes
	// L588
	buf[sizeof(nsm_msg_hdr) + 10] = 1;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t chunk_offset = 0, chunk_length = 0, length_remaining = 0;
	// data=NULL → L591 FALSE → skip copy → NSM_SW_SUCCESS
	auto rc = decode_nsm_install_token_req(msg, buf.size(), &chunk_offset,
					       &chunk_length, &length_remaining,
					       nullptr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
