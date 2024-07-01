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