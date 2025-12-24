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
#include "dot_test_fixtures.hpp"
#include "firmware-utils.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

TEST(GetRotInformation, testGoodEncodeRequest)
{
	uint16_t classification = 0x1234;
	uint8_t classification_index = 0x56;
	uint16_t component_identifier = 0xABCD;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_get_erot_state_info_req));
	nsm_firmware_erot_state_info_req nsm_req;
	nsm_req.component_classification = classification;
	nsm_req.component_classification_index = classification_index;
	nsm_req.component_identifier = component_identifier;
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_query_get_erot_state_parameters_req(0, &nsm_req,
								 request);

	struct nsm_firmware_get_erot_state_info_req *requestTest =
	    (struct nsm_firmware_get_erot_state_info_req *)(request->payload);

	struct nsm_firmware_erot_state_info_req *req =
	    (struct nsm_firmware_erot_state_info_req *)&(requestTest->fq_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_GET_EROT_STATE_INFORMATION, requestTest->hdr.command);
	EXPECT_EQ(5, requestTest->hdr.data_size);

	EXPECT_EQ(classification, req->component_classification);
	EXPECT_EQ(classification_index, req->component_classification_index);
	EXPECT_EQ(component_identifier, req->component_identifier);
}

TEST(GetRotInformation, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    5,				       // data size
	    0x12, // component classification 0x3412
	    0x34, //
	    0x56, // component identifier 0x7856
	    0x78, //
	    0x9A, // classification index 0x9A
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	struct nsm_firmware_erot_state_info_req fw_req;
	auto rc = decode_nsm_query_get_erot_state_parameters_req(
	    request, msg_len, &fw_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0x3412, fw_req.component_classification);
	EXPECT_EQ(0x9A, fw_req.component_classification_index);
	EXPECT_EQ(0x7856, fw_req.component_identifier);
}

TEST(GetRotInformation, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    2,				       // data size
	    0x12, // component classification 0x3412
	    0x34, //
	    0x56, // component identifier 0x7856
	    0x78, //
	    0x9A, // classification index 0x9A
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	struct nsm_firmware_erot_state_info_req fw_req;
	auto rc = decode_nsm_query_get_erot_state_parameters_req(
	    request, msg_len, &fw_req);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(GetRotInformation, testTooShortDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    5,				       // data size
	    0x12, // component classification 0x3412s
	    0x34, //
	    0x56, // component identifier 0x7856
	    0x78, //
	    0x9A, // classification index 0x9A
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size() - 1;

	struct nsm_firmware_erot_state_info_req fw_req;
	auto rc = decode_nsm_query_get_erot_state_parameters_req(
	    request, msg_len, &fw_req);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(GetRotInformation, testNullDecodeRequest)
{
	auto rc = decode_nsm_query_get_erot_state_parameters_req(NULL, 0, NULL);

	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(GetRotInformation, testGoodEncodeResponse)
{
	const char *firmware_version1 = "Version ABCDE";
	const char *firmware_version2 = "Version 12345";

	/* Exact message size will be counted in the encode function,
	    just, make sure this buffer is big enough to cover
	    the number of slots */
	uint16_t msg_size = sizeof(struct nsm_msg_hdr) + 250;
	std::vector<uint8_t> response(msg_size, 0);
	auto responseMsg = reinterpret_cast<nsm_msg *>(response.data());
	uint16_t reason_code = ERR_NULL;

	// Example response with firmware state
	struct nsm_firmware_erot_state_info_resp fq_resp = {};

	/* Emulate an answer with all possible fields,
		but random content */
	fq_resp.fq_resp_hdr.background_copy_policy = 0x30;
	fq_resp.fq_resp_hdr.active_slot = 0x1;
	fq_resp.fq_resp_hdr.active_keyset = 0x32;
	fq_resp.fq_resp_hdr.minimum_security_version = 0x3334;
	fq_resp.fq_resp_hdr.inband_update_policy = 0x35;
	fq_resp.fq_resp_hdr.boot_status_code = 0x0102030405060708;
	fq_resp.fq_resp_hdr.firmware_slot_count = 2;

	fq_resp.slot_info = (struct nsm_firmware_slot_info *)malloc(
	    fq_resp.fq_resp_hdr.firmware_slot_count *
	    sizeof(struct nsm_firmware_slot_info));
	memset((char *)(fq_resp.slot_info), 0,
	       fq_resp.fq_resp_hdr.firmware_slot_count *
		   sizeof(struct nsm_firmware_slot_info));

	fq_resp.slot_info[0].slot_id = 0x40;
	strcpy((char *)(&(fq_resp.slot_info[0].firmware_version_string[0])),
	       firmware_version1);
	fq_resp.slot_info[0].version_comparison_stamp = 0x09ABCDEF;
	fq_resp.slot_info[0].build_type = 0x1;
	fq_resp.slot_info[0].signing_type = 0x42;
	fq_resp.slot_info[0].write_protect_state = 0x43;
	fq_resp.slot_info[0].firmware_state = 0x44;
	fq_resp.slot_info[0].security_version_number = 0x4546;
	fq_resp.slot_info[0].signing_key_index = 0x4748;

	fq_resp.slot_info[1].slot_id = 0x50;
	strcpy((char *)(&(fq_resp.slot_info[1].firmware_version_string[0])),
	       firmware_version2);
	fq_resp.slot_info[1].version_comparison_stamp = 0x23456789;
	fq_resp.slot_info[1].build_type = 0x1;
	fq_resp.slot_info[1].signing_type = 0x52;
	fq_resp.slot_info[1].write_protect_state = 0x53;
	fq_resp.slot_info[1].firmware_state = 0x54;
	fq_resp.slot_info[1].security_version_number = 0x5556;
	fq_resp.slot_info[1].signing_key_index = 0x5758;

	reason_code = encode_nsm_query_get_erot_state_parameters_resp(
	    0, NSM_SUCCESS, NSM_SW_SUCCESS, &fq_resp, responseMsg);

	free(fq_resp.slot_info);

	EXPECT_EQ(reason_code, NSM_SW_SUCCESS);

	struct nsm_firmware_get_erot_state_info_resp *responseTest =
	    (struct nsm_firmware_get_erot_state_info_resp *)
		responseMsg->payload;

	EXPECT_EQ(0, responseMsg->hdr.request);
	EXPECT_EQ(0, responseMsg->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, responseMsg->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_GET_EROT_STATE_INFORMATION, responseTest->hdr.command);
	EXPECT_EQ(26, responseTest->hdr.telemetry_count);
}

TEST(GetRotInformation, testGoodEncodeResponse2)
{
	const char *firmware_version1 = "Version ABCDE";
	const char *firmware_version2 = "Version 12345";

	/* Exact message size will be counted in the encode function,
	    just, make sure this buffer is big enough to cover
	    the number of slots */
	uint16_t msg_size = sizeof(struct nsm_msg_hdr) + 250;
	std::vector<uint8_t> response(msg_size, 0);
	nsm_msg *responseMsg = reinterpret_cast<nsm_msg *>(response.data());
	uint16_t reason_code = ERR_NULL;

	// Example response with firmware state
	struct nsm_firmware_erot_state_info_resp fq_resp = {};

	/* Emulate an answer with all possible fields,
		but random content */
	fq_resp.fq_resp_hdr.background_copy_policy = 0x30;
	fq_resp.fq_resp_hdr.active_slot = 0x1;
	fq_resp.fq_resp_hdr.active_keyset = 0x32;
	fq_resp.fq_resp_hdr.minimum_security_version = 0x3334;
	fq_resp.fq_resp_hdr.inband_update_policy = 0x35;
	fq_resp.fq_resp_hdr.boot_status_code = 0x0102030405060708;
	fq_resp.fq_resp_hdr.firmware_slot_count = 2;

	fq_resp.slot_info = (struct nsm_firmware_slot_info *)malloc(
	    fq_resp.fq_resp_hdr.firmware_slot_count *
	    sizeof(struct nsm_firmware_slot_info));
	memset((char *)(fq_resp.slot_info), 0,
	       fq_resp.fq_resp_hdr.firmware_slot_count *
		   sizeof(struct nsm_firmware_slot_info));

	fq_resp.slot_info[0].slot_id = 0x40;
	strcpy((char *)(&(fq_resp.slot_info[0].firmware_version_string[0])),
	       firmware_version1);
	fq_resp.slot_info[0].version_comparison_stamp = 0x09ABCDEF;
	fq_resp.slot_info[0].build_type = 0x1;
	fq_resp.slot_info[0].signing_type = 0x42;
	fq_resp.slot_info[0].write_protect_state = 0x43;
	fq_resp.slot_info[0].firmware_state = 0x44;
	fq_resp.slot_info[0].security_version_number = 0x4546;
	fq_resp.slot_info[0].signing_key_index = 0x4748;

	fq_resp.slot_info[1].slot_id = 0x50;
	strcpy((char *)(&(fq_resp.slot_info[1].firmware_version_string[0])),
	       firmware_version2);
	fq_resp.slot_info[1].version_comparison_stamp = 0x23456789;
	fq_resp.slot_info[1].build_type = 0x1;
	fq_resp.slot_info[1].signing_type = 0x52;
	fq_resp.slot_info[1].write_protect_state = 0x53;
	fq_resp.slot_info[1].firmware_state = 0x54;
	fq_resp.slot_info[1].security_version_number = 0x5556;
	fq_resp.slot_info[1].signing_key_index = 0x5758;

	reason_code = encode_nsm_query_get_erot_state_parameters_resp(
	    0, NSM_SUCCESS, NSM_SW_SUCCESS, &fq_resp, responseMsg);

	free(fq_resp.slot_info);

	EXPECT_EQ(reason_code, NSM_SW_SUCCESS);

	struct nsm_firmware_get_erot_state_info_resp *responseTest =
	    (struct nsm_firmware_get_erot_state_info_resp *)
		responseMsg->payload;

	EXPECT_EQ(0, responseMsg->hdr.request);
	EXPECT_EQ(0, responseMsg->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, responseMsg->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_GET_EROT_STATE_INFORMATION, responseTest->hdr.command);
	EXPECT_EQ(26, responseTest->hdr.telemetry_count);
}

TEST(GetRotInformation, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    0,
	    10,
	    0, // number of tags: 11
	    NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT,
	    1,
	    1, // active slot: 1
	    NSM_FIRMWARE_FIRMWARE_SLOT_COUNT,
	    1,
	    2, // number of slots: 2
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    0, // slot 0 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    1, // build type: 1
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    1, // firmware state: 1
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    1, // slot 1 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    2, // build type: 2
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    2 // firmware state: 2
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	struct nsm_firmware_erot_state_info_resp erot_info = {};

	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len, &cc, &reason_code, &erot_info);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);

	EXPECT_EQ(2, erot_info.fq_resp_hdr.firmware_slot_count);
	EXPECT_EQ(1, erot_info.fq_resp_hdr.active_slot);
	EXPECT_NE(nullptr, erot_info.slot_info);
	EXPECT_EQ(1, erot_info.slot_info[0].build_type);
	EXPECT_EQ(2, erot_info.slot_info[1].build_type);

	free(erot_info.slot_info);
}

TEST(GetRotInformation, testGoodDecodeResponseRealErot213v)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x81,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    0,
	    11,
	    0, // number of tags: 11
	    NSM_FIRMWARE_BOOT_STATUS_CODE,
	    7,
	    0x00,
	    0x05,
	    0x01,
	    0xFD,
	    0x00,
	    0x40,
	    0x11,
	    0x20,
	    NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT,
	    1,
	    0, // active slot: 1
	    NSM_FIRMWARE_FIRMWARE_SLOT_COUNT,
	    1,
	    2, // number of slots: 2
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    0, // slot 0 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    1, // build type: 1
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    1, // firmware state: 1
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    1, // slot 1 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    2, // build type: 2
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    2 // firmware state: 2
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	struct nsm_firmware_erot_state_info_resp erot_info = {};

	auto rc = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len, &cc, &reason_code, &erot_info);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);

	EXPECT_EQ(2, erot_info.fq_resp_hdr.firmware_slot_count);
	EXPECT_EQ(0, erot_info.fq_resp_hdr.active_slot);
	EXPECT_NE(nullptr, erot_info.slot_info);
	EXPECT_EQ(1, erot_info.slot_info[0].build_type);
	EXPECT_EQ(2, erot_info.slot_info[1].build_type);

	free(erot_info.slot_info);
}

TEST(GetRotInformation, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    0,				       // completion code
	    26,
	    0, // number of tags
	    NSM_FIRMWARE_BACKGROUND_COPY_POLICY_PERSISTENT,
	    1,
	    1,
	    NSM_FIRMWARE_ACTIVE_KEY_SET,
	    1,
	    2,
	    NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER,
	    3,
	    0xC0,
	    0xC1,
	    NSM_FIRMWARE_INBAND_UPDATE_POLICY_PERSISTENT,
	    1,
	    99,
	    NSM_FIRMWARE_BOOT_STATUS_CODE,
	    7,
	    0x08,
	    0x07,
	    0x06,
	    0x05,
	    0x04,
	    0x03,
	    0x02,
	    0x01,
	    NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT,
	    1,
	    1, // active slot: 1
	    NSM_FIRMWARE_FIRMWARE_SLOT_COUNT,
	    1,
	    2, // number of slots: 2
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    0, // slot 0 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    1, // build type: 1
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    1, // firmware state: 1
	    NSM_FIRMWARE_VERSION_COMPARISON_STAMP,
	    5,
	    0xD0,
	    0xD1,
	    0xD2,
	    0xD3,
	    NSM_FIRMWARE_SIGNING_TYPE,
	    1,
	    0xA1,
	    NSM_FIRMWARE_WRITE_PROTECT_STATE,
	    1,
	    0xA2,
	    NSM_FIRMWARE_SECURITY_VERSION_NUMBER,
	    3,
	    0xA3,
	    0xA4,
	    NSM_FIRMWARE_SIGNING_KEY_INDEX,
	    3,
	    0xA5,
	    0xA6,
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    1, // slot 1 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    2, // build type: 2
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    2, // firmware state: 2
	    NSM_FIRMWARE_VERSION_COMPARISON_STAMP,
	    5,
	    0xE0,
	    0xE1,
	    0xE2,
	    0xE3,
	    NSM_FIRMWARE_SIGNING_TYPE,
	    1,
	    0xB1,
	    NSM_FIRMWARE_WRITE_PROTECT_STATE,
	    1,
	    0xB2,
	    NSM_FIRMWARE_SECURITY_VERSION_NUMBER,
	    3,
	    0xB3,
	    0xB4,
	    NSM_FIRMWARE_SIGNING_KEY_INDEX,
	    3,
	    0xB5,
	    0xB6,
	    23,
	    1,
	    1 // unsupported tag number
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	struct nsm_firmware_erot_state_info_resp erot_info = {};

	reason_code = decode_nsm_query_get_erot_state_parameters_resp(
	    NULL, msg_len, &cc, &reason_code, &erot_info);
	EXPECT_EQ(reason_code, NSM_SW_ERROR_NULL);

	reason_code = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len, &cc, NULL, &erot_info);
	EXPECT_EQ(reason_code, NSM_SW_ERROR_NULL);

	reason_code = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len, &cc, &reason_code, NULL);
	EXPECT_EQ(reason_code, NSM_SW_ERROR_NULL);

	cc = NSM_SUCCESS;
	reason_code = ERR_NULL;
	reason_code = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len - 20, &cc, &reason_code, &erot_info);
	/* In this case there is not enough data to decode one of the tags
	 * properly */
	EXPECT_EQ(reason_code, NSM_SW_ERROR_LENGTH);
	/* Though, some tags should be decoded properly */
	EXPECT_EQ(0x0102030405060708, erot_info.fq_resp_hdr.boot_status_code);
	EXPECT_NE(nullptr, erot_info.slot_info);
	free(erot_info.slot_info);
	erot_info.slot_info = nullptr;

	cc = NSM_SUCCESS;
	reason_code = ERR_NULL;
	reason_code = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len, &cc, &reason_code, &erot_info);
	/* The last tag has an unsupported id */
	EXPECT_EQ(reason_code, NSM_SW_SUCCESS);
	EXPECT_NE(nullptr, erot_info.slot_info);
	free(erot_info.slot_info);
}

TEST(GetRotInformation, testDecodeResponseWithUnsupportedTag)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_GET_EROT_STATE_INFORMATION, // command
	    0,				       // completion code
	    27,
	    0, // number of tags
	    NSM_FIRMWARE_BACKGROUND_COPY_POLICY_PERSISTENT,
	    1,
	    1,
	    NSM_FIRMWARE_ACTIVE_KEY_SET,
	    1,
	    2,
	    NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER,
	    3,
	    0xC0,
	    0xC1,
	    NSM_FIRMWARE_INBAND_UPDATE_POLICY_PERSISTENT,
	    1,
	    99,
	    NSM_FIRMWARE_BOOT_STATUS_CODE,
	    7,
	    0x08,
	    0x07,
	    0x06,
	    0x05,
	    0x04,
	    0x03,
	    0x02,
	    0x01,
	    NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT,
	    1,
	    1,	// active slot: 1
	    99, // unsupported tag number
	    1,	// value
	    1,	// value
	    NSM_FIRMWARE_FIRMWARE_SLOT_COUNT,
	    1,
	    2, // number of slots: 2
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    0, // slot 0 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    1, // build type: 1
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    1, // firmware state: 1
	    NSM_FIRMWARE_VERSION_COMPARISON_STAMP,
	    5,
	    0xD0,
	    0xD1,
	    0xD2,
	    0xD3,
	    NSM_FIRMWARE_SIGNING_TYPE,
	    1,
	    0xA1,
	    NSM_FIRMWARE_WRITE_PROTECT_STATE,
	    1,
	    0xA2,
	    NSM_FIRMWARE_SECURITY_VERSION_NUMBER,
	    3,
	    0xA3,
	    0xA4,
	    NSM_FIRMWARE_SIGNING_KEY_INDEX,
	    3,
	    0xA5,
	    0xA6,
	    NSM_FIRMWARE_FIRMWARE_SLOT_ID,
	    1,
	    1, // slot 1 tag
	    NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
	    0x0B,
	    0x30,
	    0x31,
	    0x2E,
	    0x30,
	    0x33,
	    0x2E,
	    0x30,
	    0x32,
	    0x31,
	    0x30,
	    0x2E,
	    0x30,
	    0x30,
	    0x30,
	    0x33,
	    0x5F,
	    0x6E,
	    0x30,
	    0x33,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    0,
	    NSM_FIRMWARE_BUILD_TYPE,
	    1,
	    2, // build type: 2
	    NSM_FIRMWARE_FIRMWARE_STATE,
	    1,
	    2, // firmware state: 2
	    NSM_FIRMWARE_VERSION_COMPARISON_STAMP,
	    5,
	    0xE0,
	    0xE1,
	    0xE2,
	    0xE3,
	    NSM_FIRMWARE_SIGNING_TYPE,
	    1,
	    0xB1,
	    NSM_FIRMWARE_WRITE_PROTECT_STATE,
	    1,
	    0xB2,
	    NSM_FIRMWARE_SECURITY_VERSION_NUMBER,
	    3,
	    0xB3,
	    0xB4,
	    NSM_FIRMWARE_SIGNING_KEY_INDEX,
	    3,
	    0xB5,
	    0xB6,
	    23,
	    1,
	    1 // unsupported tag number
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	struct nsm_firmware_erot_state_info_resp erot_info = {};

	reason_code = decode_nsm_query_get_erot_state_parameters_resp(
	    response, msg_len, &cc, &reason_code, &erot_info);
	/* The last tag has an unsupported id */
	EXPECT_EQ(reason_code, NSM_SW_SUCCESS);
	EXPECT_NE(nullptr, erot_info.slot_info);
	free(erot_info.slot_info);
}

TEST(codeAuthKeyPermQuery, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint16_t component_classification = 0x0001;
	uint16_t component_identifier = 0x0002;
	uint8_t component_classification_index = 0x03;
	auto rc = encode_nsm_code_auth_key_perm_query_req(
	    0, component_classification, component_identifier,
	    component_classification_index, request);

	nsm_code_auth_key_perm_query_req *req =
	    (nsm_code_auth_key_perm_query_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_QUERY_CODE_AUTH_KEY_PERM, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_code_auth_key_perm_query_req) -
		      sizeof(nsm_common_req),
		  req->hdr.data_size);
	EXPECT_EQ(req->component_classification, component_classification);
	EXPECT_EQ(req->component_identifier, component_identifier);
	EXPECT_EQ(req->component_classification_index,
		  component_classification_index);
}

TEST(codeAuthKeyPermQuery, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE,		     // NVIDIA_MSG_TYPE
	    NSM_FW_QUERY_CODE_AUTH_KEY_PERM, // command
	    NSM_SUCCESS,		     // completion code
	    0,
	    0,	  // reserved
	    13,	  // data size
	    0,	  // data size
	    0x12, // active_component_key_index
	    0x34, // active_component_key_index
	    0x56, // pending_component_key_index
	    0x78, // pending_component_key_index
	    2,	  // permission_bitmap_length
	    0x01, // active_component_key_perm_bitmap
	    0x02, // active_component_key_perm_bitmap
	    0x03, // pending_component_key_perm_bitmap
	    0x04, // pending_component_key_perm_bitmap
	    0x05, // efuse_key_perm_bitmap
	    0x06, // efuse_key_perm_bitmap
	    0x07, // pending_efuse_key_perm_bitmap
	    0x08, // pending_efuse_key_perm_bitmap
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint16_t active_component_key_index;
	uint16_t pending_component_key_index;
	uint8_t permission_bitmap_length;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    response, msg_len, &cc, &reason_code, &active_component_key_index,
	    &pending_component_key_index, &permission_bitmap_length, NULL, NULL,
	    NULL, NULL);
	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(0x3412, active_component_key_index);
	EXPECT_EQ(0x7856, pending_component_key_index);
	EXPECT_EQ(2, permission_bitmap_length);

	std::unique_ptr<uint8_t[]> active_component_key_perm_bitmap(
	    new uint8_t[permission_bitmap_length]);
	std::unique_ptr<uint8_t[]> pending_component_key_perm_bitmap(
	    new uint8_t[permission_bitmap_length]);
	std::unique_ptr<uint8_t[]> efuse_key_perm_bitmap(
	    new uint8_t[permission_bitmap_length]);
	std::unique_ptr<uint8_t[]> pending_efuse_key_perm_bitmap(
	    new uint8_t[permission_bitmap_length]);

	rc = decode_nsm_code_auth_key_perm_query_resp(
	    response, msg_len, &cc, &reason_code, &active_component_key_index,
	    &pending_component_key_index, &permission_bitmap_length,
	    active_component_key_perm_bitmap.get(),
	    pending_component_key_perm_bitmap.get(),
	    efuse_key_perm_bitmap.get(), pending_efuse_key_perm_bitmap.get());
	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(0x3412, active_component_key_index);
	EXPECT_EQ(0x7856, pending_component_key_index);
	EXPECT_EQ(2, permission_bitmap_length);
	EXPECT_EQ(0x01, active_component_key_perm_bitmap.get()[0]);
	EXPECT_EQ(0x02, active_component_key_perm_bitmap.get()[1]);
	EXPECT_EQ(0x03, pending_component_key_perm_bitmap.get()[0]);
	EXPECT_EQ(0x04, pending_component_key_perm_bitmap.get()[1]);
	EXPECT_EQ(0x05, efuse_key_perm_bitmap.get()[0]);
	EXPECT_EQ(0x06, efuse_key_perm_bitmap.get()[1]);
	EXPECT_EQ(0x07, pending_efuse_key_perm_bitmap.get()[0]);
	EXPECT_EQ(0x08, pending_efuse_key_perm_bitmap.get()[1]);
}

TEST(codeAuthKeyPermQuery, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE,		     // NVIDIA_MSG_TYPE
	    NSM_FW_QUERY_CODE_AUTH_KEY_PERM, // command
	    NSM_SUCCESS,		     // completion code
	    0,
	    0,	  // reserved
	    13,	  // data size
	    0,	  // data size
	    0x12, // active_component_key_index
	    0x34, // active_component_key_index
	    0x56, // pending_component_key_index
	    0x78, // pending_component_key_index
	    8,	  // permission_bitmap_length (incorrect)
	    0x01, // active_component_key_perm_bitmap
	    0x02, // active_component_key_perm_bitmap
	    0x03, // pending_component_key_perm_bitmap
	    0x04, // pending_component_key_perm_bitmap
	    0x05, // efuse_key_perm_bitmap
	    0x06, // efuse_key_perm_bitmap
	    0x07, // pending_efuse_key_perm_bitmap
	    0x08, // pending_efuse_key_perm_bitmap
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint16_t active_component_key_index;
	uint16_t pending_component_key_index;
	uint8_t permission_bitmap_length;
	auto rc = decode_nsm_code_auth_key_perm_query_resp(
	    response, msg_len, &cc, &reason_code, &active_component_key_index,
	    &pending_component_key_index, &permission_bitmap_length, NULL, NULL,
	    NULL, NULL);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(codeAuthKeyPermUpdate, testGoodEncodeRequest)
{
	size_t dataLength = 16;
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_req) +
	    dataLength);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_code_auth_key_perm_request_type request_type =
	    NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE;
	uint16_t component_classification = 0x0001;
	uint16_t component_identifier = 0x0002;
	uint8_t component_classification_index = 0x03;
	uint64_t nonce = 0x0123456789abcdef;
	uint8_t permission_bitmap_length = dataLength;
	std::unique_ptr<uint8_t[]> permission_bitmap(
	    new uint8_t[permission_bitmap_length]);
	for (size_t i = 0; i < permission_bitmap_length; ++i) {
		permission_bitmap.get()[i] = i;
	}
	auto rc = encode_nsm_code_auth_key_perm_update_req(
	    0, request_type, component_classification, component_identifier,
	    component_classification_index, nonce, permission_bitmap_length,
	    permission_bitmap.get(), request);

	nsm_code_auth_key_perm_update_req *req =
	    (nsm_code_auth_key_perm_update_req *)request->payload;

	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_UPDATE_CODE_AUTH_KEY_PERM, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_code_auth_key_perm_update_req) -
		      sizeof(nsm_common_req) + permission_bitmap_length,
		  req->hdr.data_size);
	EXPECT_EQ(req->request_type, request_type);
	EXPECT_EQ(req->component_classification, component_classification);
	EXPECT_EQ(req->component_identifier, component_identifier);
	EXPECT_EQ(req->component_classification_index,
		  component_classification_index);
	EXPECT_EQ(req->nonce, nonce);
	EXPECT_EQ(req->permission_bitmap_length, permission_bitmap_length);
	uint8_t *bitmap_ptr =
	    request->payload + sizeof(struct nsm_code_auth_key_perm_update_req);
	EXPECT_EQ(0, memcmp(permission_bitmap.get(), bitmap_ptr,
			    permission_bitmap_length));
}

TEST(codeAuthKeyPermUpdate, testBadEncodeRequest)
{
	size_t dataLength = 16;
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_req) +
	    dataLength);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint16_t component_classification = 0x0001;
	uint16_t component_identifier = 0x0002;
	uint8_t component_classification_index = 0x03;
	uint64_t nonce = 0x0123456789abcdef;
	uint8_t permission_bitmap_length = dataLength;
	std::unique_ptr<uint8_t[]> permission_bitmap(
	    new uint8_t[permission_bitmap_length]);
	for (size_t i = 0; i < permission_bitmap_length; ++i) {
		permission_bitmap.get()[i] = i;
	}
	int rc;

	// most restrictive value requested but bitmap is specified
	rc = encode_nsm_code_auth_key_perm_update_req(
	    0, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
	    component_classification, component_identifier,
	    component_classification_index, nonce, permission_bitmap_length,
	    permission_bitmap.get(), request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// specified value requested but bitmap is of zero length
	rc = encode_nsm_code_auth_key_perm_update_req(
	    0, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE,
	    component_classification, component_identifier,
	    component_classification_index, nonce, 0, permission_bitmap.get(),
	    request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// specified value requested but bitmap is nullptr
	rc = encode_nsm_code_auth_key_perm_update_req(
	    0, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE,
	    component_classification, component_identifier,
	    component_classification_index, nonce, permission_bitmap_length,
	    nullptr, request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// specified value requested but bitmap is not specified
	rc = encode_nsm_code_auth_key_perm_update_req(
	    0, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE,
	    component_classification, component_identifier,
	    component_classification_index, nonce, 0, nullptr, request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// incorrect request type
	rc = encode_nsm_code_auth_key_perm_update_req(
	    0, static_cast<nsm_code_auth_key_perm_request_type>(0xFF),
	    component_classification, component_identifier,
	    component_classification_index, nonce, permission_bitmap_length,
	    permission_bitmap.get(), request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(codeAuthKeyPermUpdate, testGoodDecodeResponse)
{
	uint32_t method = NSM_EFUSE_UPDATE_METHOD_SYSTEM_REBOOT |
			  NSM_EFUSE_UPDATE_METHOD_FUNCTION_LEVEL_RESET;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_UPDATE_CODE_AUTH_KEY_PERM, // command
	    NSM_SUCCESS,		      // completion code
	    0,
	    0,						 // reserved
	    4,						 // data size
	    0,						 // data size
	    static_cast<uint8_t>(method & 0xFF),	 // update_method
	    static_cast<uint8_t>((method >> 8) & 0xFF),	 // update_method
	    static_cast<uint8_t>((method >> 16) & 0xFF), // update_method
	    static_cast<uint8_t>((method >> 24) & 0xFF), // update_method
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint32_t update_method = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_resp(
	    response, msg_len, &cc, &reason_code, &update_method);
	EXPECT_EQ(NSM_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(0, reason_code);
	EXPECT_EQ(NSM_EFUSE_UPDATE_METHOD_SYSTEM_REBOOT |
		      NSM_EFUSE_UPDATE_METHOD_FUNCTION_LEVEL_RESET,
		  update_method);
}

TEST(codeAuthKeyPermUpdate, testBadDecodeResponse)
{
	uint32_t method = NSM_EFUSE_UPDATE_METHOD_SYSTEM_REBOOT |
			  NSM_EFUSE_UPDATE_METHOD_FUNCTION_LEVEL_RESET;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x00,	       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_UPDATE_CODE_AUTH_KEY_PERM, // command
	    NSM_SUCCESS,		      // completion code
	    0,
	    0,						// reserved
	    2,						// data size
	    0,						// data size
	    static_cast<uint8_t>(method & 0xFF),	// update_method
	    static_cast<uint8_t>((method >> 8) & 0xFF), // update_method
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = ERR_NULL;
	uint32_t update_method = 0;
	auto rc = decode_nsm_code_auth_key_perm_update_resp(
	    response, msg_len, &cc, &reason_code, &update_method);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST(QueryFirmwareSecurityVersion, testEncodeRequest)
{
	uint16_t classification = 0xA;
	uint8_t index = 0x0;
	uint16_t identifier = 0x10;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_firmware_security_version_number_req_command));
	nsm_firmware_security_version_number_req nsm_req;
	nsm_req.component_classification = classification;
	nsm_req.component_classification_index = index;
	nsm_req.component_identifier = identifier;
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_query_firmware_security_version_number_req(
	    0, &nsm_req, request);

	struct nsm_firmware_security_version_number_req_command *requestTest =
	    (struct nsm_firmware_security_version_number_req_command
		 *)(request->payload);

	struct nsm_firmware_security_version_number_req *req =
	    (struct nsm_firmware_security_version_number_req *)&(
		requestTest->fq_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER,
		  requestTest->hdr.command);
	EXPECT_EQ(5, requestTest->hdr.data_size);

	EXPECT_EQ(classification, req->component_classification);
	EXPECT_EQ(index, req->component_classification_index);
	EXPECT_EQ(identifier, req->component_identifier);

	// Negative test case
	rc = encode_nsm_query_firmware_security_version_number_req(0, &nsm_req,
								   NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(QueryFirmwareSecurityVersion, testDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER, // command
	    0x5,				      // data size
	    0x0A, // component classification 0x000A
	    0x00, //
	    0x00, // component identifier 0xFF00
	    0xFF, //
	    0x0,  // classification index 0x00
	};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	struct nsm_firmware_security_version_number_req fw_req;
	auto rc = decode_nsm_query_firmware_security_version_number_req(
	    request, msg_len, &fw_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0x0A, fw_req.component_classification);
	EXPECT_EQ(0xFF00, fw_req.component_identifier);
	EXPECT_EQ(0x0, fw_req.component_classification_index);

	// Negative test cases
	rc = decode_nsm_query_firmware_security_version_number_req(
	    NULL, msg_len, &fw_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	rc = decode_nsm_query_firmware_security_version_number_req(
	    request, msg_len, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
	rc = decode_nsm_query_firmware_security_version_number_req(
	    request, msg_len - 2, &fw_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(QueryFirmwareSecurityVersion, testEncodeResponse)
{
	struct nsm_firmware_security_version_number_resp sec_resp{};
	sec_resp.active_component_security_version = 3;
	sec_resp.pending_component_security_version = 4;
	sec_resp.minimum_security_version = 1;
	sec_resp.pending_minimum_security_version = 2;
	uint16_t msg_size =
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_firmware_security_version_number_resp_command);

	std::vector<uint8_t> response(msg_size, 0);
	auto responseMsg = reinterpret_cast<nsm_msg *>(response.data());
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_nsm_query_firmware_security_version_number_resp(
	    0, NSM_SUCCESS, reason_code, &sec_resp, responseMsg);

	struct nsm_firmware_security_version_number_resp_command *responseTest =
	    (struct nsm_firmware_security_version_number_resp_command
		 *)(responseMsg->payload);

	struct nsm_firmware_security_version_number_resp *resp =
	    (struct nsm_firmware_security_version_number_resp *)&(
		responseTest->sec_ver_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, responseMsg->hdr.request);
	EXPECT_EQ(0, responseMsg->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, responseMsg->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER,
		  responseTest->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_common_resp) +
		      sizeof(struct nsm_firmware_security_version_number_resp),
		  responseTest->hdr.data_size);
	EXPECT_EQ(resp->active_component_security_version, 3);
	EXPECT_EQ(resp->pending_component_security_version, 4);
	EXPECT_EQ(resp->minimum_security_version, 1);
	EXPECT_EQ(resp->pending_minimum_security_version, 2);

	// Negative test case
	rc = encode_nsm_query_firmware_security_version_number_resp(
	    0, NSM_SUCCESS, reason_code, &sec_resp, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	reason_code = NSM_SW_ERROR_COMMAND_FAIL;
	rc = encode_nsm_query_firmware_security_version_number_resp(
	    0, NSM_ERROR, reason_code, &sec_resp, responseMsg);
	struct nsm_common_non_success_resp *responseFail =
	    (struct nsm_common_non_success_resp *)responseMsg->payload;
	EXPECT_EQ(reason_code, responseFail->reason_code);
}

TEST(QueryFirmwareSecurityVersion, testDecodeResponse)
{
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	std::vector<uint8_t> response{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER, // command
	    0x00,				      // completion_code
	    0x00,
	    0x00, // reserved
	    0x08,
	    0x00, // data size
	    0x03,
	    0x00, // active_component_security_version
	    0x04,
	    0x00, // pending_component_security_version
	    0x01,
	    0x00, // minimum_security_version
	    0x02,
	    0x00 // pending_minimum_security_version
	};
	auto responseMsg = reinterpret_cast<nsm_msg *>(response.data());
	size_t msg_len = response.size();

	struct nsm_firmware_security_version_number_resp sec_resp;
	auto rc = decode_nsm_query_firmware_security_version_number_resp(
	    responseMsg, msg_len, &cc, &reason_code, &sec_resp);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
	EXPECT_EQ(sec_resp.active_component_security_version, 3);
	EXPECT_EQ(sec_resp.pending_component_security_version, 4);
	EXPECT_EQ(sec_resp.minimum_security_version, 1);
	EXPECT_EQ(sec_resp.pending_minimum_security_version, 2);

	// Negative test cases
	rc = decode_nsm_query_firmware_security_version_number_resp(
	    NULL, msg_len, &cc, &reason_code, &sec_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> response1{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER, // command
	    NSM_ERROR,				      // completion_code
	    0x00,
	    0x00, // reserved
	    0x08,
	    0x00, // data size
	    0x03,
	    0x00, // active_component_security_version
	    0x04,
	    0x00, // pending_component_security_version
	    0x01,
	    0x00, // minimum_security_version
	    0x02,
	    0x00 // pending_minimum_security_version
	};
	auto responseMsg1 = reinterpret_cast<nsm_msg *>(response1.data());
	size_t msg_len1 = response1.size();
	rc = decode_nsm_query_firmware_security_version_number_resp(
	    responseMsg1, msg_len1, &cc, &reason_code, &sec_resp);
	EXPECT_EQ(cc, NSM_ERROR);

	std::vector<uint8_t> response2{
	    0x10,
	    0xDE,	       // PCI VID: NVIDIA 0x10DE
	    0x80,	       // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,	       // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_FIRMWARE, // NVIDIA_MSG_TYPE
	    NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER, // command
	    0x00,				      // completion_code
	    0x00,
	    0x00, // reserved
	    0x02,
	    0x00, // data size
	    0x03,
	    0x00, // active_component_security_version --> Truncated response
	};
	auto responseMsg2 = reinterpret_cast<nsm_msg *>(response2.data());
	size_t msg_len2 = response2.size();
	rc = decode_nsm_query_firmware_security_version_number_resp(
	    responseMsg2, msg_len2, &cc, &reason_code, &sec_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(UpdateFirmwareSecurityVersion, testEncodeRequest)
{
	uint8_t requestType = 0x1;
	uint16_t classification = 0xA;
	uint8_t index = 0x0;
	uint16_t identifier = 0x10;
	uint64_t nonce = 0x12345678;
	uint16_t reqMinSecVersion = 0x3;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_firmware_update_min_sec_ver_req_command));
	nsm_firmware_update_min_sec_ver_req nsm_req;
	nsm_req.request_type = requestType;
	nsm_req.component_classification = classification;
	nsm_req.component_classification_index = index;
	nsm_req.component_identifier = identifier;
	nsm_req.nonce = nonce;
	nsm_req.req_min_security_version = reqMinSecVersion;
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_firmware_update_sec_ver_req(0, &nsm_req, request);

	struct nsm_firmware_update_min_sec_ver_req_command *requestTest =
	    (struct nsm_firmware_update_min_sec_ver_req_command
		 *)(request->payload);

	struct nsm_firmware_update_min_sec_ver_req *req =
	    (struct nsm_firmware_update_min_sec_ver_req *)&(
		requestTest->ver_update_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER,
		  requestTest->hdr.command);
	EXPECT_EQ(sizeof(nsm_firmware_update_min_sec_ver_req),
		  requestTest->hdr.data_size);

	EXPECT_EQ(requestType, req->request_type);
	EXPECT_EQ(classification, req->component_classification);
	EXPECT_EQ(index, req->component_classification_index);
	EXPECT_EQ(identifier, req->component_identifier);
	EXPECT_EQ(nonce, req->nonce);
	EXPECT_EQ(reqMinSecVersion, req->req_min_security_version);

	// Negative test case
	rc = encode_nsm_firmware_update_sec_ver_req(0, &nsm_req, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKInstall, testGoodEncodeRequest)
{
	nsm_dot_cak_install_req dot_req;
	// CAK: 148 bytes pattern 0x00-0x93, LAK: 148 bytes pattern 0xA0-0xFF
	// cycling
	dot_test::createDotCAKInstallRequest(dot_req, 0, 0xA0, 0, 0x12345678);

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_install_req(0, &dot_req, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_cak_install_req_command *req =
	    (nsm_dot_cak_install_req_command *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_INSTALL, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_dot_cak_install_req), le16toh(req->hdr.data_size));

	for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
		EXPECT_EQ((i % 256), req->dot_cak_install_req.cak_pub[i]);
	}
	for (size_t i = 0; i < dot_test::LAK_KEY_SIZE; i++) {
		EXPECT_EQ(((i + 0xA0) % 256),
			  req->dot_cak_install_req.lak_pub[i]);
	}
	EXPECT_EQ(0, req->dot_cak_install_req.lock_disable);
	uint32_t expectedMinSvn = (0x56 & 0xFF) | ((0x78 & 0xFF) << 8);
	EXPECT_EQ(expectedMinSvn, le32toh(req->dot_cak_install_req.min_svn));
}

TEST(DotCAKInstall, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.ocp_type = OCP_TYPE;
	request->hdr.ocp_version = OCP_VERSION_V2;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	nsm_dot_cak_install_req_command *req_cmd =
	    (nsm_dot_cak_install_req_command *)request->payload;
	req_cmd->hdr.command = NSM_FW_DOT_CAK_INSTALL;
	req_cmd->hdr.data_size = htole16(sizeof(nsm_dot_cak_install_req));

	dot_test::fillPatternSequential(req_cmd->dot_cak_install_req.cak_pub,
					dot_test::CAK_KEY_SIZE, 0);
	dot_test::fillPatternCycling(req_cmd->dot_cak_install_req.lak_pub,
				     dot_test::LAK_KEY_SIZE, 0xA0);
	req_cmd->dot_cak_install_req.lock_disable = 1;
	uint32_t testMinSvn = (0xDD & 0xFF) | ((0xCC & 0xFF) << 8);
	req_cmd->dot_cak_install_req.min_svn = htole32(testMinSvn);

	nsm_dot_cak_install_req dot_req;
	auto rc = decode_nsm_dot_cak_install_req(request, requestMsg.size(),
						 &dot_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
		EXPECT_EQ((i % 256), dot_req.cak_pub[i]);
		EXPECT_EQ(((i + 0xA0) % 256), dot_req.lak_pub[i]);
	}
	EXPECT_EQ(1, dot_req.lock_disable);
	EXPECT_EQ(testMinSvn, dot_req.min_svn);
}

TEST(DotCAKInstall, testShortDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) + 10);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_dot_cak_install_req dot_req;
	auto rc = decode_nsm_dot_cak_install_req(request, requestMsg.size(),
						 &dot_req);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotCAKInstall, testNullDecodeRequest)
{
	nsm_dot_cak_install_req dot_req;

	auto rc = decode_nsm_dot_cak_install_req(NULL, 100, &dot_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = decode_nsm_dot_cak_install_req(request, requestMsg.size(), NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKInstall, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;
	auto rc = encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, reason_code,
						  response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, response->hdr.nvidia_msg_type);

	nsm_common_resp *resp = (nsm_common_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_INSTALL, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);
	EXPECT_EQ(0, resp->data_size); // No additional data for DOT CAK Install
}

TEST(DotCAKInstall, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		    // PCI VID: NVIDIA 0x10DE
	    0x00,		    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		    // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	    // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_CAK_INSTALL, // command
	    NSM_SUCCESS,	    // completion code
	    0x00,
	    0x00, // reserved (uint16_t)
	    0x04,
	    0x00,		    // data size (4 bytes for inner response)
	    NSM_FW_DOT_CAK_INSTALL, // response command_code
	    NSM_SUCCESS,	    // response completion_code
	    0x00,
	    0x00 // reserved (uint16_t)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = 0xFFFF;
	auto rc = decode_nsm_dot_cak_install_resp(response, msg_len, &cc,
						  &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
}

TEST(DotCAKInstall, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		    // PCI VID: NVIDIA 0x10DE
	    0x00,		    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		    // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	    // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_CAK_INSTALL, // command
	    NSM_ERROR,		    // completion code (error)
	    0x12,
	    0x34 // reason code 0x3412 (little endian)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_nsm_dot_cak_install_resp(response, msg_len, &cc,
						  &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x3412, reason_code);
}

TEST(DotCAKInstall, testEncodeDecodeRoundTrip)
{
	nsm_dot_cak_install_req original_req;
	dot_test::createDotCAKInstallRequest(original_req, 0, 100, 1,
					     0xDEADBEEF);

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_dot_cak_install_req(5, &original_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_cak_install_req decoded_req;
	rc = decode_nsm_dot_cak_install_req(request, requestMsg.size(),
					    &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
		EXPECT_EQ(original_req.cak_pub[i], decoded_req.cak_pub[i]);
		EXPECT_EQ(original_req.lak_pub[i], decoded_req.lak_pub[i]);
	}
	EXPECT_EQ(original_req.lock_disable, decoded_req.lock_disable);
	EXPECT_EQ(original_req.min_svn, decoded_req.min_svn);
}

TEST(DotCAKInstall, testEncodeDecodeResponseRoundTrip)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t original_reason_code = ERR_NULL;
	uint8_t original_cc = NSM_SUCCESS;

	auto rc = encode_nsm_dot_cak_install_resp(
	    7, original_cc, original_reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0xFFFF;

	rc = decode_nsm_dot_cak_install_resp(response, responseMsg.size(),
					     &decoded_cc, &decoded_reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(original_cc, decoded_cc);
	EXPECT_EQ(original_reason_code, decoded_reason_code);

	nsm_common_resp *resp = (nsm_common_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_INSTALL, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);

	std::vector<uint8_t> errorResponseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
	auto error_response =
	    reinterpret_cast<nsm_msg *>(errorResponseMsg.data());

	uint16_t error_reason_code = 0xABCD;
	uint8_t error_cc = NSM_ERROR;

	rc = encode_nsm_dot_cak_install_resp(3, error_cc, error_reason_code,
					     error_response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_error_cc = 0;
	uint16_t decoded_error_reason_code = 0;

	rc = decode_nsm_dot_cak_install_resp(
	    error_response, errorResponseMsg.size(), &decoded_error_cc,
	    &decoded_error_reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(error_cc, decoded_error_cc);
	EXPECT_EQ(error_reason_code, decoded_error_reason_code);
}

TEST(DotCAKInstall, testNullPointerHandling)
{
	nsm_dot_cak_install_req dot_req;
	dot_test::createDotCAKInstallRequest(dot_req, 0, 100, 0, 0x12345678);

	auto rc = encode_nsm_dot_cak_install_req(0, &dot_req, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = encode_nsm_dot_cak_install_req(0, NULL, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_nsm_dot_cak_install_resp(NULL, 100, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// First encode a valid response for testing
	rc =
	    encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Now test with null cc pointer
	rc = decode_nsm_dot_cak_install_resp(response, responseMsg.size(), NULL,
					     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_install_resp(response, responseMsg.size(), &cc,
					     NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKInstall, testDataIntegrity)
{
	const struct {
		const char *name;
		uint8_t cak_pattern_start;
		uint8_t lak_pattern_start;
		uint8_t lock_disable;
		uint8_t vendor_min_svn;
		uint8_t owner_min_svn;
	} test_cases[] = {
	    {"all_zeros", 0x00, 0x00, 0, 0x00, 0x00},
	    {"all_ones", 0xFF, 0xFF, 1, 0xFF, 0xFF},
	    {"alternating", 0xAA, 0x55, 1, 0xAA, 0x55},
	    {"sequential", 0x00, 0x00, 0, 0x78, 0x56},
	    {"random", 0x42, 0xDE, 1, 0xEF, 0xBE},
	};

	for (const auto &test : test_cases) {
		nsm_dot_cak_install_req original_req;
		for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
			original_req.cak_pub[i] =
			    (test.cak_pattern_start + i) & 0xFF;
			original_req.lak_pub[i] =
			    (test.lak_pattern_start + i) & 0xFF;
		}
		original_req.lock_disable = test.lock_disable;
		uint32_t minSvnBitmap = (test.owner_min_svn & 0xFF) |
					((test.vendor_min_svn & 0xFF) << 8);
		original_req.min_svn = minSvnBitmap;

		std::vector<uint8_t> requestMsg(
		    sizeof(nsm_msg_hdr) +
		    sizeof(nsm_dot_cak_install_req_command));
		auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
		auto rc =
		    encode_nsm_dot_cak_install_req(0, &original_req, request);
		ASSERT_EQ(rc, NSM_SW_SUCCESS)
		    << "Encoding failed for test case: " << test.name;

		nsm_dot_cak_install_req decoded_req;
		rc = decode_nsm_dot_cak_install_req(request, requestMsg.size(),
						    &decoded_req);
		ASSERT_EQ(rc, NSM_SW_SUCCESS)
		    << "Decoding failed for test case: " << test.name;

		for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
			EXPECT_EQ(original_req.cak_pub[i],
				  decoded_req.cak_pub[i])
			    << "CAK mismatch at byte " << i
			    << " for test case: " << test.name;
			EXPECT_EQ(original_req.lak_pub[i],
				  decoded_req.lak_pub[i])
			    << "LAK mismatch at byte " << i
			    << " for test case: " << test.name;
		}
		EXPECT_EQ(original_req.lock_disable, decoded_req.lock_disable)
		    << "lock_disable mismatch for test case: " << test.name;
		EXPECT_EQ(original_req.min_svn, decoded_req.min_svn)
		    << "min_svn mismatch for test case: " << test.name;
	}
}

TEST(DotCAKInstall, testBoundaryConditions)
{
	nsm_dot_cak_install_req dot_req;
	memset(&dot_req, 0, sizeof(dot_req));

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_install_req(0, &dot_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_cak_install_req decoded_req;
	rc = decode_nsm_dot_cak_install_req(request, requestMsg.size(),
					    &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_install_req(request, requestMsg.size() - 1,
					    &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_install_req(request, sizeof(nsm_msg_hdr),
					    &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc =
	    encode_nsm_dot_cak_install_resp(0, NSM_SUCCESS, ERR_NULL, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;

	// Valid size
	rc = decode_nsm_dot_cak_install_resp(response, responseMsg.size(), &cc,
					     &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Error response - manually create to test decode with proper V2 format
	std::vector<uint8_t> errorMsg{
	    0x10,
	    0xDE,		    // PCI VID: NVIDIA 0x10DE
	    0x00,		    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		    // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	    // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_CAK_INSTALL, // command
	    NSM_ERROR,		    // completion code (error)
	    0x34,
	    0x12 // reason code 0x1234 (little endian)
	};
	auto error_response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	rc = decode_nsm_dot_cak_install_resp(error_response, errorMsg.size(),
					     &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x1234, reason_code);

	// Test minimum valid size (11 bytes = 5 header + 6 common_resp)
	std::vector<uint8_t> minimalMsg{
	    0x10,
	    0xDE,		    // PCI VID: NVIDIA 0x10DE
	    0x00,		    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		    // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	    // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_CAK_INSTALL, // command
	    NSM_SUCCESS,	    // completion code
	    0x00,
	    0x00, // reserved
	    0x00,
	    0x00 // data_size = 0
	};
	auto minimal_response = reinterpret_cast<nsm_msg *>(minimalMsg.data());

	rc = decode_nsm_dot_cak_install_resp(
	    minimal_response, minimalMsg.size(), &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);

	rc = decode_nsm_dot_cak_install_resp(
	    minimal_response, minimalMsg.size() - 1, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_install_resp(
	    minimal_response, sizeof(nsm_msg_hdr), &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotCAKInstall, testDecodeResponseNegativeCases)
{
	uint8_t cc;
	uint16_t reason_code;

	auto rc = decode_nsm_dot_cak_install_resp(NULL, 11, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> validMsg{
	    0x10,
	    0xDE,		    // PCI VID: NVIDIA 0x10DE
	    0x00,		    // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		    // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	    // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_CAK_INSTALL, // command
	    NSM_SUCCESS,	    // completion code
	    0x00,
	    0x00, // reserved
	    0x00,
	    0x00 // data_size = 0
	};
	auto valid_response = reinterpret_cast<nsm_msg *>(validMsg.data());

	rc = decode_nsm_dot_cak_install_resp(valid_response, validMsg.size(),
					     NULL, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_install_resp(valid_response, validMsg.size(),
					     &cc, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_install_resp(valid_response, 0, &cc,
					     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_install_resp(valid_response, 5, &cc,
					     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	// payload
	rc = decode_nsm_dot_cak_install_resp(
	    valid_response, sizeof(nsm_msg_hdr), &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_install_resp(valid_response, 10, &cc,
					     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_install_resp(valid_response, validMsg.size(),
					     &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
}

TEST(DotCAKBypass, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_bypass_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_bypass_req(0, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_cak_bypass_req *req =
	    (nsm_dot_cak_bypass_req *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_BYPASS, req->command);
	EXPECT_EQ(0, le16toh(req->data_size)); // No data
}

TEST(DotCAKBypass, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_bypass_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	// Fill header
	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;
	request->hdr.ocp_version = OCP_VERSION_V2;

	nsm_dot_cak_bypass_req *req =
	    (nsm_dot_cak_bypass_req *)request->payload;
	req->command = NSM_FW_DOT_CAK_BYPASS;
	req->data_size = 0;

	auto rc = decode_nsm_dot_cak_bypass_req(request, requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DotCAKBypass, testShortDecodeRequest)
{
	// Test with too short message
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) + 2);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = decode_nsm_dot_cak_bypass_req(request, requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotCAKBypass, testNullDecodeRequest)
{
	auto rc = decode_nsm_dot_cak_bypass_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKBypass, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc =
	    encode_nsm_dot_cak_bypass_resp(0, NSM_SUCCESS, ERR_NULL, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, response->hdr.nvidia_msg_type);
	EXPECT_EQ(OCP_VERSION_V2, response->hdr.ocp_version);

	nsm_common_resp *resp = (nsm_common_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_BYPASS, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(DotCAKBypass, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg = {
	    0x15,
	    0x68,		   // PCI Vendor ID (little endian)
	    0x00,		   // Request=0, Datagram=0, Instance=0
	    0x8A,		   // OCP_VER=10 (V2), Nvidia msg type=6
	    0x06,		   // Nvidia msg type (firmware)
	    NSM_FW_DOT_CAK_BYPASS, // command
	    NSM_SUCCESS,	   // completion code
	    0x04,
	    0x00,		   // data_size = 4 (sizeof response fields)
	    NSM_FW_DOT_CAK_BYPASS, // response command_code
	    NSM_SUCCESS,	   // response completion_code
	    0x00,
	    0x00 // reserved = 0
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0xFFFF;

	auto rc = decode_nsm_dot_cak_bypass_resp(response, responseMsg.size(),
						 &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
}

TEST(DotCAKBypass, testBadDecodeResponse)
{
	std::vector<uint8_t> errorMsg = {
	    0x15,
	    0x68,		   // PCI Vendor ID (little endian)
	    0x00,		   // Request=0, Datagram=0, Instance=0
	    0x8A,		   // OCP_VER=10 (V2), Nvidia msg type=6
	    0x06,		   // Nvidia msg type (firmware)
	    NSM_FW_DOT_CAK_BYPASS, // command
	    NSM_ERROR,		   // completion code (error)
	    0x34,
	    0x12 // reason code 0x1234 (little endian)
	};
	auto error_response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_nsm_dot_cak_bypass_resp(
	    error_response, errorMsg.size(), &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x1234, reason_code);
}

TEST(DotCAKBypass, testEncodeDecodeRoundTrip)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_bypass_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_bypass_req(5, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_bypass_req(request, requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(5, request->hdr.instance_id);
}

TEST(DotCAKBypass, testEncodeDecodeResponseRoundTrip)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t original_instance_id = 7;
	uint8_t original_cc = NSM_SUCCESS;
	uint16_t original_reason_code = ERR_NULL;

	auto rc = encode_nsm_dot_cak_bypass_resp(
	    original_instance_id, original_cc, original_reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_cc = NSM_ERROR;
	uint16_t decoded_reason_code = 0xFFFF;

	rc = decode_nsm_dot_cak_bypass_resp(response, responseMsg.size(),
					    &decoded_cc, &decoded_reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(original_cc, decoded_cc);
	EXPECT_EQ(original_reason_code, decoded_reason_code);

	std::vector<uint8_t> errorMsg(sizeof(nsm_msg_hdr) +
				      sizeof(nsm_common_non_success_resp));
	auto error_response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	uint16_t error_reason_code = 0xABCD;
	rc = encode_nsm_dot_cak_bypass_resp(original_instance_id, NSM_ERROR,
					    error_reason_code, error_response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_error_cc = NSM_SUCCESS;
	uint16_t decoded_error_reason_code = ERR_NULL;

	rc = decode_nsm_dot_cak_bypass_resp(error_response, errorMsg.size(),
					    &decoded_error_cc,
					    &decoded_error_reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, decoded_error_cc);
	EXPECT_EQ(error_reason_code, decoded_error_reason_code);
}

TEST(DotCAKBypass, testNullPointerHandling)
{
	auto rc = encode_nsm_dot_cak_bypass_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_bypass_req(nullptr, 100);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_nsm_dot_cak_bypass_resp(0, NSM_SUCCESS, ERR_NULL, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc;
	uint16_t reason_code;

	rc = decode_nsm_dot_cak_bypass_resp(nullptr, responseMsg.size(), &cc,
					    &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_bypass_resp(response, responseMsg.size(),
					    nullptr, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_bypass_resp(response, responseMsg.size(), &cc,
					    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKBypass, testBoundaryConditions)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_bypass_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_bypass_req(0, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_bypass_req(request, requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_bypass_req(request, requestMsg.size() - 1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	std::vector<uint8_t> errorMsg = {
	    0x15,
	    0x68,		   // PCI Vendor ID (little endian)
	    0x00,		   // Request=0, Datagram=0, Instance=0
	    0x8A,		   // OCP_VER=10 (V2), Nvidia msg type=6
	    0x06,		   // Nvidia msg type (firmware)
	    NSM_FW_DOT_CAK_BYPASS, // command
	    NSM_ERROR,		   // completion code (error)
	    0x34,
	    0x12 // reason code 0x1234 (little endian)
	};
	auto error_response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	uint8_t cc;
	uint16_t reason_code;

	rc = decode_nsm_dot_cak_bypass_resp(error_response, errorMsg.size(),
					    &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0x1234, reason_code);
}

TEST(DotOverride, testGoodEncodeRequest)
{
	nsm_dot_override_req dot_req;
	// Fill signature with sequential pattern: 0x00-0xFF cycling
	dot_test::fillPatternSequential(dot_req.signature, DOT_SIGNATURE_SIZE,
					0);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_override_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_override_req(0, &dot_req, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_override_req_command *req =
	    (nsm_dot_override_req_command *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_OVERRIDE, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_dot_override_req), le16toh(req->hdr.data_size));

	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		EXPECT_EQ((i % 256), req->dot_override_req.signature[i]);
	}
}

TEST(DotOverride, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_override_req_command));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.ocp_type = OCP_TYPE;
	request->hdr.ocp_version = OCP_VERSION_V2;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	nsm_dot_override_req_command *req_cmd =
	    (nsm_dot_override_req_command *)request->payload;
	req_cmd->hdr.command = NSM_FW_DOT_OVERRIDE;
	req_cmd->hdr.data_size = htole16(sizeof(nsm_dot_override_req));

	dot_test::fillPatternSequential(req_cmd->dot_override_req.signature,
					DOT_SIGNATURE_SIZE, 0x42);

	nsm_dot_override_req dot_req;
	auto rc =
	    decode_nsm_dot_override_req(request, requestMsg.size(), &dot_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		EXPECT_EQ(((i + 0x42) % 256), dot_req.signature[i]);
	}
}

TEST(DotOverride, testShortDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) + 10);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_dot_override_req dot_req;
	auto rc =
	    decode_nsm_dot_override_req(request, requestMsg.size(), &dot_req);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotOverride, testNullDecodeRequest)
{
	nsm_dot_override_req dot_req;

	auto rc = decode_nsm_dot_override_req(NULL, 100, &dot_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_override_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = decode_nsm_dot_override_req(request, requestMsg.size(), NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotOverride, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_override_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;
	auto rc =
	    encode_nsm_dot_override_resp(0, NSM_SUCCESS, reason_code, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, response->hdr.nvidia_msg_type);

	nsm_dot_override_resp *resp =
	    (nsm_dot_override_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_OVERRIDE, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);
	EXPECT_EQ(0, resp->data_size);
}

TEST(DotOverride, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		 // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	 // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_OVERRIDE, // command
	    NSM_SUCCESS,	 // completion code
	    0x00,
	    0x00, // reserved (uint16_t)
	    0x04,
	    0x00,		 // data size (4 bytes for inner response)
	    NSM_FW_DOT_OVERRIDE, // response command_code
	    NSM_SUCCESS,	 // response completion_code
	    0x00,
	    0x00 // reserved (uint16_t)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = 0xFFFF;
	auto rc =
	    decode_nsm_dot_override_resp(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
}

TEST(DotOverride, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    0x00,		 // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		 // OCP_TYPE=8, OCP_VER=10 (V2)
	    NSM_TYPE_FIRMWARE,	 // NVIDIA_MSG_TYPE
	    NSM_FW_DOT_OVERRIDE, // command
	    NSM_ERROR,		 // completion code (error)
	    0x12,
	    0x34 // reason code 0x3412 (little endian)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc =
	    decode_nsm_dot_override_resp(response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x3412, reason_code);
}

TEST(DotOverride, testEncodeDecodeRoundTrip)
{
	nsm_dot_override_req original_req;
	dot_test::fillPatternMultiplied(original_req.signature,
					DOT_SIGNATURE_SIZE, 7);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_override_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_dot_override_req(5, &original_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_override_req decoded_req;
	rc = decode_nsm_dot_override_req(request, requestMsg.size(),
					 &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		EXPECT_EQ(original_req.signature[i], decoded_req.signature[i]);
	}
}

TEST(DotOverride, testEncodeDecodeResponseRoundTrip)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_override_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t original_reason_code = ERR_NULL;
	uint8_t original_cc = NSM_SUCCESS;

	auto rc = encode_nsm_dot_override_resp(7, original_cc,
					       original_reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0xFFFF;

	rc = decode_nsm_dot_override_resp(response, responseMsg.size(),
					  &decoded_cc, &decoded_reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(original_cc, decoded_cc);
	EXPECT_EQ(original_reason_code, decoded_reason_code);

	nsm_dot_override_resp *resp =
	    (nsm_dot_override_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_OVERRIDE, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);

	std::vector<uint8_t> errorResponseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
	auto error_response =
	    reinterpret_cast<nsm_msg *>(errorResponseMsg.data());

	uint16_t error_reason_code = 0xABCD;
	uint8_t error_cc = NSM_ERROR;

	rc = encode_nsm_dot_override_resp(3, error_cc, error_reason_code,
					  error_response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_error_cc = 0;
	uint16_t decoded_error_reason_code = 0;

	rc = decode_nsm_dot_override_resp(
	    error_response, errorResponseMsg.size(), &decoded_error_cc,
	    &decoded_error_reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(error_cc, decoded_error_cc);
	EXPECT_EQ(error_reason_code, decoded_error_reason_code);
}

TEST(DotOverride, testNullPointerHandling)
{
	nsm_dot_override_req dot_req;
	dot_test::fillPatternSequential(dot_req.signature, DOT_SIGNATURE_SIZE,
					0);

	auto rc = encode_nsm_dot_override_req(0, &dot_req, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_override_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = encode_nsm_dot_override_req(0, NULL, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_nsm_dot_override_resp(0, NSM_SUCCESS, ERR_NULL, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_nsm_dot_override_resp(NULL, 100, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_override_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// First encode a valid response for testing
	rc = encode_nsm_dot_override_resp(0, NSM_SUCCESS, ERR_NULL, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Now test with null cc pointer
	rc = decode_nsm_dot_override_resp(response, responseMsg.size(), NULL,
					  &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_override_resp(response, responseMsg.size(), &cc,
					  NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotOverride, testDataIntegrity)
{
	const struct {
		const char *name;
		uint8_t signaturePatternStart;
		uint8_t signatureMultiplier;
	} test_cases[] = {
	    {"all_zeros", 0x00, 0},   {"all_ones", 0xFF, 0},
	    {"alternating", 0xAA, 0}, {"sequential", 0x00, 0},
	    {"multiplied", 0x00, 7},
	};

	for (const auto &test : test_cases) {
		nsm_dot_override_req original_req;

		if (test.signatureMultiplier == 0) {
			if (test.signaturePatternStart == 0x00 &&
			    strcmp(test.name, "all_zeros") == 0) {
				dot_test::fillPatternConstant(
				    original_req.signature, DOT_SIGNATURE_SIZE,
				    0x00);
			} else if (test.signaturePatternStart == 0xFF) {
				dot_test::fillPatternConstant(
				    original_req.signature, DOT_SIGNATURE_SIZE,
				    0xFF);
			} else if (test.signaturePatternStart == 0xAA) {
				dot_test::fillPatternConstant(
				    original_req.signature, DOT_SIGNATURE_SIZE,
				    0xAA);
			} else {
				dot_test::fillPatternSequential(
				    original_req.signature, DOT_SIGNATURE_SIZE,
				    test.signaturePatternStart);
			}
		} else {
			dot_test::fillPatternMultiplied(
			    original_req.signature, DOT_SIGNATURE_SIZE,
			    test.signatureMultiplier);
		}

		std::vector<uint8_t> requestMsg(
		    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command));
		auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

		auto rc =
		    encode_nsm_dot_override_req(0, &original_req, request);
		ASSERT_EQ(rc, NSM_SW_SUCCESS)
		    << "Encoding failed for test case: " << test.name;

		nsm_dot_override_req decoded_req;
		rc = decode_nsm_dot_override_req(request, requestMsg.size(),
						 &decoded_req);
		ASSERT_EQ(rc, NSM_SW_SUCCESS)
		    << "Decoding failed for test case: " << test.name;

		for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
			EXPECT_EQ(original_req.signature[i],
				  decoded_req.signature[i])
			    << "Signature mismatch at byte " << i
			    << " for test case: " << test.name;
		}
	}
}

TEST(DotOverride, testBoundaryConditions)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_override_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_dot_override_req dot_req;
	memset(&dot_req, 0, sizeof(dot_req));

	auto rc = encode_nsm_dot_override_req(0, &dot_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_override_req decoded_req;
	rc = decode_nsm_dot_override_req(request, requestMsg.size(),
					 &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_override_req(request, requestMsg.size() - 1,
					 &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_override_req(request, sizeof(nsm_msg_hdr),
					 &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_override_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = encode_nsm_dot_override_resp(0, NSM_SUCCESS, ERR_NULL, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;

	// Valid size
	rc = decode_nsm_dot_override_resp(response, responseMsg.size(), &cc,
					  &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Error response - manually create to test decode with proper V2 format
	std::vector<uint8_t> errorMsg = {
	    0x10,
	    0xDE,		 // PCI Vendor ID (little endian)
	    0x00,		 // Request=0, Datagram=0, Instance=0
	    0x8A,		 // OCP_VER=10 (V2), Nvidia msg type=6
	    NSM_TYPE_FIRMWARE,	 // Nvidia msg type (firmware)
	    NSM_FW_DOT_OVERRIDE, // command
	    NSM_ERROR,		 // completion code (error)
	    0x34,
	    0x12 // reason code 0x1234 (little endian)
	};
	auto error_response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	rc = decode_nsm_dot_override_resp(error_response, errorMsg.size(), &cc,
					  &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x1234, reason_code);
}

TEST(ImageCopyControl, testGoodEncodeRequestNoComponents)
{
	// Test encoding Image Copy Control request with no components (Query
	// Progress)
	uint8_t requestType = 0; // Query Image Copy Progress
	uint8_t componentCount = 0;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
	    sizeof(nsm_firmware_image_copy_control_req));

	nsm_firmware_image_copy_control_req nsm_req;
	nsm_req.request_type = requestType;
	nsm_req.component_count = componentCount;

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_firmware_image_copy_control_req(0, &nsm_req,
							     nullptr, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify message header
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	// Verify common request header
	auto common_req = reinterpret_cast<nsm_common_req *>(request->payload);
	EXPECT_EQ(NSM_FW_IMAGE_COPY_CONTROL, common_req->command);

	// Verify request data
	auto req_data = reinterpret_cast<nsm_firmware_image_copy_control_req *>(
	    request->payload + sizeof(nsm_common_req));
	EXPECT_EQ(requestType, req_data->request_type);
	EXPECT_EQ(componentCount, req_data->component_count);
}

TEST(ImageCopyControl, testGoodEncodeRequestWithSingleComponent)
{
	// Test encoding Image Copy Control request with one component
	uint8_t requestType = 1; // Initiate Image Copy
	uint8_t componentCount = 1;

	nsm_firmware_image_copy_component_entry component;
	component.component_classification = 0x1234;
	component.component_identifier = 0x5678;
	component.component_classification_index = 0xAB;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
	    sizeof(nsm_firmware_image_copy_control_req) +
	    (componentCount * sizeof(nsm_firmware_image_copy_component_entry)));

	nsm_firmware_image_copy_control_req nsm_req;
	nsm_req.request_type = requestType;
	nsm_req.component_count = componentCount;

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_firmware_image_copy_control_req(
	    0, &nsm_req, &component, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify message header
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, request->hdr.nvidia_msg_type);

	// Verify common request header
	auto common_req = reinterpret_cast<nsm_common_req *>(request->payload);
	EXPECT_EQ(NSM_FW_IMAGE_COPY_CONTROL, common_req->command);

	// Verify request data
	auto req_data = reinterpret_cast<nsm_firmware_image_copy_control_req *>(
	    request->payload + sizeof(nsm_common_req));
	EXPECT_EQ(requestType, req_data->request_type);
	EXPECT_EQ(componentCount, req_data->component_count);

	// Verify component entry
	auto component_entry =
	    reinterpret_cast<nsm_firmware_image_copy_component_entry *>(
		request->payload + sizeof(nsm_common_req) +
		sizeof(nsm_firmware_image_copy_control_req));
	EXPECT_EQ(0x1234, component_entry->component_classification);
	EXPECT_EQ(0x5678, component_entry->component_identifier);
	EXPECT_EQ(0xAB, component_entry->component_classification_index);
}

TEST(ImageCopyControl, testGoodEncodeRequestWithMultipleComponents)
{
	// Test encoding Image Copy Control request with multiple components
	uint8_t requestType = 1; // Initiate Image Copy
	uint8_t componentCount = 3;

	std::vector<nsm_firmware_image_copy_component_entry> components(
	    componentCount);
	components[0].component_classification = 0x0001;
	components[0].component_identifier = 0x0002;
	components[0].component_classification_index = 0;

	components[1].component_classification = 0x0003;
	components[1].component_identifier = 0x0004;
	components[1].component_classification_index = 1;

	components[2].component_classification = 0x0005;
	components[2].component_identifier = 0x0006;
	components[2].component_classification_index = 2;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
	    sizeof(nsm_firmware_image_copy_control_req) +
	    (componentCount * sizeof(nsm_firmware_image_copy_component_entry)));

	nsm_firmware_image_copy_control_req nsm_req;
	nsm_req.request_type = requestType;
	nsm_req.component_count = componentCount;

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_firmware_image_copy_control_req(
	    0, &nsm_req, components.data(), request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify request data
	auto req_data = reinterpret_cast<nsm_firmware_image_copy_control_req *>(
	    request->payload + sizeof(nsm_common_req));
	EXPECT_EQ(requestType, req_data->request_type);
	EXPECT_EQ(componentCount, req_data->component_count);

	// Verify all component entries
	auto component_entries =
	    reinterpret_cast<nsm_firmware_image_copy_component_entry *>(
		request->payload + sizeof(nsm_common_req) +
		sizeof(nsm_firmware_image_copy_control_req));

	for (uint8_t i = 0; i < componentCount; ++i) {
		EXPECT_EQ(components[i].component_classification,
			  component_entries[i].component_classification);
		EXPECT_EQ(components[i].component_identifier,
			  component_entries[i].component_identifier);
		EXPECT_EQ(components[i].component_classification_index,
			  component_entries[i].component_classification_index);
	}
}

TEST(ImageCopyControl, testGoodDecodeResponseQueryProgress)
{
	// Test decoding response for Query Progress
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00,			// data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_IN_PROGRESS, // image_copy_status
	    0x64,			// image_copy_progress (100%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_IN_PROGRESS, query_resp.image_copy_status);
	EXPECT_EQ(100, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusComplete)
{
	// Test decoding response with Complete status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00,		     // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_COMPLETE, // image_copy_status
	    0x64,		     // image_copy_progress (100%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_COMPLETE, query_resp.image_copy_status);
	EXPECT_EQ(100, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusNotTriggered)
{
	// Test decoding response with Not Triggered status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00,			  // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_NOT_TRIGGERED, // image_copy_status
	    0x00,			  // image_copy_progress (0%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_NOT_TRIGGERED, query_resp.image_copy_status);
	EXPECT_EQ(0, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusUndefinedFailure)
{
	// Test decoding response with Undefined Failure status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00, // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_UNDEFINED_FAILURE, // image_copy_status
	    0x32,			      // image_copy_progress (50%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = 0;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_UNDEFINED_FAILURE,
		  query_resp.image_copy_status);
	EXPECT_EQ(50, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusNoValidImage)
{
	// Test decoding response with No Valid Image status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00,			   // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_NO_VALID_IMAGE, // image_copy_status
	    0x00,			   // image_copy_progress (0%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_NO_VALID_IMAGE, query_resp.image_copy_status);
	EXPECT_EQ(0, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusWriteProtected)
{
	// Test decoding response with Destination Write Protected status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00, // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_DESTINATION_WRITE_PROTECTED, // image_copy_status
	    0x00, // image_copy_progress (0%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_DESTINATION_WRITE_PROTECTED,
		  query_resp.image_copy_status);
	EXPECT_EQ(0, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusFlashAccessFailure)
{
	// Test decoding response with Flash Access Failure status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00, // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_FAIL_FLASH_ACCESS, // image_copy_status
	    0x00,			      // image_copy_progress (0%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_FAIL_FLASH_ACCESS,
		  query_resp.image_copy_status);
	EXPECT_EQ(0, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testDecodeResponseStatusFailedVerify)
{
	// Test decoding response with Failed Verify status
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x02,
	    0x00,			  // data_size (2 bytes, little-endian)
	    NSM_IMAGE_COPY_FAILED_VERIFY, // image_copy_status
	    0x64,			  // image_copy_progress (100%)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, msg_len, &cc, &reason_code, &query_resp);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(NSM_IMAGE_COPY_FAILED_VERIFY, query_resp.image_copy_status);
	EXPECT_EQ(100, query_resp.image_copy_progress);
}

TEST(ImageCopyControl, testGoodDecodeResponseInitiateCopy)
{
	// Test decoding response for Initiate Copy
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,		       // PCI VID: NVIDIA 0x10DE
	    0x00,		       // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,		       // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_FIRMWARE,	       // NVIDIA_MSG_TYPE
	    NSM_FW_IMAGE_COPY_CONTROL, // command
	    NSM_SUCCESS,	       // completion code
	    0x00,
	    0x00, // reserved (2 bytes)
	    0x00,
	    0x00, // data_size (2 bytes, little-endian)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
	    response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
}

TEST(ImageCopyControl, testNullEncodeRequest)
{
	// Test encoding with null parameters
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
	    sizeof(nsm_firmware_image_copy_control_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_firmware_image_copy_control_req nsm_req;
	nsm_req.request_type = 0;
	nsm_req.component_count = 0;

	// Test with null request structure
	auto rc = encode_nsm_firmware_image_copy_control_req(0, nullptr,
							     nullptr, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test with null message buffer
	rc = encode_nsm_firmware_image_copy_control_req(0, &nsm_req, nullptr,
							nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(ImageCopyControl, testNullDecodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp) + 2);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	// Test null response message
	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    nullptr, responseMsg.size(), &cc, &reason_code, &query_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test null response data
	rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    response, responseMsg.size(), &cc, &reason_code, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test null parameters for Initiate Copy decode
	rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
	    nullptr, responseMsg.size(), &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(ImageCopyControl, testInvalidResponseSize)
{
	uint8_t cc;
	uint16_t reason_code = ERR_NULL;
	nsm_firmware_image_copy_control_query_progress_resp query_resp = {};

	// Test query progress response with size too small
	std::vector<uint8_t> smallResponseMsg(sizeof(nsm_msg_hdr) +
					      sizeof(nsm_common_resp) - 1);
	auto smallResponse =
	    reinterpret_cast<nsm_msg *>(smallResponseMsg.data());

	auto rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    smallResponse, smallResponseMsg.size(), &cc, &reason_code,
	    &query_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	// Test query progress response with size smaller than minimum required
	std::vector<uint8_t> tinyResponseMsg(sizeof(nsm_msg_hdr) +
					     sizeof(nsm_common_resp));
	auto tinyResponse = reinterpret_cast<nsm_msg *>(tinyResponseMsg.data());

	rc = decode_nsm_firmware_image_copy_control_query_progress_resp(
	    tinyResponse, tinyResponseMsg.size(), &cc, &reason_code,
	    &query_resp);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	/*	// Test initiate copy response with size too small
		rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
		    smallResponse, smallResponseMsg.size(), &cc, &reason_code);
		EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

		// Test initiate copy response with zero size
		rc = decode_nsm_firmware_image_copy_control_initiate_copy_resp(
		    smallResponse, 0, &cc, &reason_code);
		EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH); */
}

// ================ DOT LOCK Tests ================

TEST(DotLock, testGoodEncodeRequest)
{
	nsm_dot_lock_req dot_req;
	dot_test::createDotLockRequest(dot_req, 0, 0xA0, 2, 0, 7);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_lock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_lock_req(0, &dot_req, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_lock_req_command *req =
	    (nsm_dot_lock_req_command *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_LOCK, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_dot_lock_req), le16toh(req->hdr.data_size));

	// Verify CAK and LAK public keys
	for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
		EXPECT_EQ((i % 256), req->dot_lock_req.cak_pub[i]);
		EXPECT_EQ(((i + 0xA0) % 256), req->dot_lock_req.lak_pub[i]);
	}

	// Verify unlock method
	EXPECT_EQ(2, le32toh(req->dot_lock_req.unlock_method));

	// Verify static challenge
	for (size_t i = 0; i < dot_test::CHALLENGE_SIZE; i++) {
		EXPECT_EQ((i % 256), req->dot_lock_req.s_challenge[i]);
	}

	// Verify signature
	for (size_t i = 0; i < dot_test::SIGNATURE_SIZE; i++) {
		EXPECT_EQ(((i * 7) % 256), req->dot_lock_req.signature[i]);
	}
}

TEST(DotLock, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_lock_req_command));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	// Fill header
	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.ocp_type = OCP_TYPE;
	request->hdr.ocp_version = OCP_VERSION_V2;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	// Fill payload
	nsm_dot_lock_req_command *req_cmd =
	    (nsm_dot_lock_req_command *)request->payload;
	req_cmd->hdr.command = NSM_FW_DOT_LOCK;
	req_cmd->hdr.data_size = htole16(sizeof(nsm_dot_lock_req));

	dot_test::fillPatternSequential(req_cmd->dot_lock_req.cak_pub,
					dot_test::CAK_KEY_SIZE, 0);
	dot_test::fillPatternCycling(req_cmd->dot_lock_req.lak_pub,
				     dot_test::LAK_KEY_SIZE, 0xA0);
	req_cmd->dot_lock_req.unlock_method = htole32(2);
	dot_test::fillPatternSequential(req_cmd->dot_lock_req.s_challenge,
					dot_test::CHALLENGE_SIZE, 0);
	dot_test::fillPatternMultiplied(req_cmd->dot_lock_req.signature,
					dot_test::SIGNATURE_SIZE, 7);

	// Decode the request
	nsm_dot_lock_req dot_req;
	auto rc = decode_nsm_dot_lock_req(request, requestMsg.size(), &dot_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify decoded data
	for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
		EXPECT_EQ((i % 256), dot_req.cak_pub[i]);
		EXPECT_EQ(((i + 0xA0) % 256), dot_req.lak_pub[i]);
	}
	EXPECT_EQ(2, dot_req.unlock_method);
	for (size_t i = 0; i < dot_test::CHALLENGE_SIZE; i++) {
		EXPECT_EQ((i % 256), dot_req.s_challenge[i]);
	}
	for (size_t i = 0; i < dot_test::SIGNATURE_SIZE; i++) {
		EXPECT_EQ(((i * 7) % 256), dot_req.signature[i]);
	}
}

TEST(DotLock, testShortDecodeRequest)
{
	// Create a message that's too short
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) + 100);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_dot_lock_req dot_req;
	auto rc = decode_nsm_dot_lock_req(request, requestMsg.size(), &dot_req);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotLock, testNullDecodeRequest)
{
	nsm_dot_lock_req dot_req;

	// Null message pointer
	auto rc = decode_nsm_dot_lock_req(NULL, 100, &dot_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Null output pointer
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_lock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = decode_nsm_dot_lock_req(request, requestMsg.size(), NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotLock, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_lock_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Create a test DOT blob
	uint8_t dot_blob[DOT_BLOB_SIZE];
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		dot_blob[i] = (i % 256);
	}

	uint16_t reason_code = ERR_NULL;
	auto rc = encode_nsm_dot_lock_resp(0, NSM_SUCCESS, reason_code,
					   dot_blob, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, response->hdr.nvidia_msg_type);

	nsm_dot_lock_resp *resp = (nsm_dot_lock_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_LOCK, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);
	EXPECT_EQ(DOT_BLOB_SIZE, le16toh(resp->data_size));

	// Verify DOT blob
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ((i % 256), resp->dot_blob[i]);
	}
}

TEST(DotLock, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_lock_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Fill header
	response->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	response->hdr.request = 0;
	response->hdr.datagram = 0;
	response->hdr.instance_id = 0;
	response->hdr.ocp_type = OCP_TYPE;
	response->hdr.ocp_version = OCP_VERSION_V2;
	response->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	// Fill response
	nsm_dot_lock_resp *resp = (nsm_dot_lock_resp *)response->payload;
	resp->command = NSM_FW_DOT_LOCK;
	resp->completion_code = NSM_SUCCESS;
	resp->reserved = 0;
	resp->data_size = htole16(DOT_BLOB_SIZE);

	// Fill DOT blob with test pattern
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		resp->dot_blob[i] = ((i * 3) % 256);
	}

	// Decode response
	uint8_t cc = 0;
	uint16_t reason_code = 0xFFFF;
	uint8_t decoded_blob[DOT_BLOB_SIZE];

	auto rc = decode_nsm_dot_lock_resp(response, responseMsg.size(), &cc,
					   &reason_code, decoded_blob);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);

	// Verify DOT blob
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ(((i * 3) % 256), decoded_blob[i]);
	}
}

TEST(DotLock, testErrorResponse)
{
	std::vector<uint8_t> errorMsg(sizeof(nsm_msg_hdr) +
				      sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	// Encode error response
	uint16_t reason_code = 0x3412;
	auto rc =
	    encode_nsm_dot_lock_resp(0, NSM_ERROR, reason_code, NULL, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode error response
	uint8_t cc = 0;
	uint16_t decoded_reason_code = 0;
	rc = decode_nsm_dot_lock_resp(response, errorMsg.size(), &cc,
				      &decoded_reason_code, NULL);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x3412, decoded_reason_code);
}

TEST(DotLock, testEncodeDecodeRoundTrip)
{
	nsm_dot_lock_req original_req;
	dot_test::createDotLockRequestCustom(original_req, 0, 100, 2, 2, 3);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_lock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_dot_lock_req(5, &original_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_lock_req decoded_req;
	rc = decode_nsm_dot_lock_req(request, requestMsg.size(), &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < dot_test::CAK_KEY_SIZE; i++) {
		EXPECT_EQ(original_req.cak_pub[i], decoded_req.cak_pub[i]);
		EXPECT_EQ(original_req.lak_pub[i], decoded_req.lak_pub[i]);
	}
	EXPECT_EQ(original_req.unlock_method, decoded_req.unlock_method);
	for (size_t i = 0; i < dot_test::CHALLENGE_SIZE; i++) {
		EXPECT_EQ(original_req.s_challenge[i],
			  decoded_req.s_challenge[i]);
	}
	for (size_t i = 0; i < dot_test::SIGNATURE_SIZE; i++) {
		EXPECT_EQ(original_req.signature[i], decoded_req.signature[i]);
	}
}

TEST(DotLock, testEncodeDecodeResponseRoundTrip)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_lock_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Create original DOT blob
	uint8_t original_blob[DOT_BLOB_SIZE];
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		original_blob[i] = (i * 5) & 0xFF;
	}

	uint16_t original_reason_code = ERR_NULL;
	uint8_t original_cc = NSM_SUCCESS;

	auto rc = encode_nsm_dot_lock_resp(7, original_cc, original_reason_code,
					   original_blob, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0xFFFF;
	uint8_t decoded_blob[DOT_BLOB_SIZE];

	rc = decode_nsm_dot_lock_resp(response, responseMsg.size(), &decoded_cc,
				      &decoded_reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(original_cc, decoded_cc);
	EXPECT_EQ(original_reason_code, decoded_reason_code);
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ(original_blob[i], decoded_blob[i]);
	}

	// Verify response fields
	nsm_dot_lock_resp *resp = (nsm_dot_lock_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_LOCK, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
}

TEST(DotLock, testNullPointerHandling)
{
	nsm_dot_lock_req dot_req;
	memset(&dot_req, 0, sizeof(dot_req));

	auto rc = encode_nsm_dot_lock_req(0, &dot_req, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_lock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = encode_nsm_dot_lock_req(0, NULL, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t blob[DOT_BLOB_SIZE] = {0};
	rc = encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, blob, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_nsm_dot_lock_resp(NULL, 100, &cc, &reason_code, blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_lock_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	rc = encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, blob, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_lock_resp(response, responseMsg.size(), NULL,
				      &reason_code, blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_lock_resp(response, responseMsg.size(), &cc, NULL,
				      blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotLock, testBufferLengthValidation)
{
	// Test request encoding and decoding with exact buffer size
	nsm_dot_lock_req dot_req;
	memset(&dot_req, 0, sizeof(dot_req));
	dot_req.unlock_method = 2;

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_lock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_lock_req(0, &dot_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_lock_req decoded_req;
	rc = decode_nsm_dot_lock_req(request, requestMsg.size(), &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Test decode with too short buffer
	rc = decode_nsm_dot_lock_req(request, requestMsg.size() - 1,
				     &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc =
	    decode_nsm_dot_lock_req(request, sizeof(nsm_msg_hdr), &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	// Test response decoding with too short buffer
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_lock_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t blob[DOT_BLOB_SIZE] = {0};
	rc = encode_nsm_dot_lock_resp(0, NSM_SUCCESS, ERR_NULL, blob, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;
	uint8_t decoded_blob[DOT_BLOB_SIZE];

	rc = decode_nsm_dot_lock_resp(response, responseMsg.size(), &cc,
				      &reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Too short for full response
	rc = decode_nsm_dot_lock_resp(response, responseMsg.size() - 1, &cc,
				      &reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	// Only header size
	rc = decode_nsm_dot_lock_resp(response, sizeof(nsm_msg_hdr), &cc,
				      &reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// DOT CAK ROTATE Tests
// ============================================================================

TEST(DotCAKRotate, testGoodEncodeRequest)
{
	nsm_dot_cak_rotate_req cak_rotate_req;
	dot_test::createDotCAKRotateRequest(cak_rotate_req, 0, 3);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_rotate_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_rotate_req(0, &cak_rotate_req, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_cak_rotate_req_command *req =
	    (nsm_dot_cak_rotate_req_command *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_ROTATE, req->hdr.command);
	EXPECT_EQ(sizeof(nsm_dot_cak_rotate_req), le16toh(req->hdr.data_size));

	for (size_t i = 0; i < DOT_KEY_AUTH_DATA_SIZE; i++) {
		EXPECT_EQ((i % 256), req->dot_cak_rotate_req.new_cak[i]);
	}

	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		EXPECT_EQ(((i * 3) % 256),
			  req->dot_cak_rotate_req.signature[i]);
	}
}

TEST(DotCAKRotate, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_rotate_req_command));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.ocp_type = OCP_TYPE;
	request->hdr.ocp_version = OCP_VERSION_V2;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	nsm_dot_cak_rotate_req_command *req_cmd =
	    (nsm_dot_cak_rotate_req_command *)request->payload;
	req_cmd->hdr.command = NSM_FW_DOT_CAK_ROTATE;
	req_cmd->hdr.data_size = htole16(sizeof(nsm_dot_cak_rotate_req));

	nsm_dot_cak_rotate_req temp_req;
	dot_test::createDotCAKRotateRequest(temp_req, 0, 5);
	std::memcpy(&req_cmd->dot_cak_rotate_req, &temp_req,
		    sizeof(nsm_dot_cak_rotate_req));

	nsm_dot_cak_rotate_req cak_rotate_req;
	auto rc = decode_nsm_dot_cak_rotate_req(request, requestMsg.size(),
						&cak_rotate_req);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < DOT_KEY_AUTH_DATA_SIZE; i++) {
		EXPECT_EQ((i % 256), cak_rotate_req.new_cak[i]);
	}
	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		EXPECT_EQ(((i * 5) % 256), cak_rotate_req.signature[i]);
	}
}

TEST(DotCAKRotate, testShortDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) + 100);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	nsm_dot_cak_rotate_req cak_rotate_req;
	auto rc = decode_nsm_dot_cak_rotate_req(request, requestMsg.size(),
						&cak_rotate_req);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotCAKRotate, testNullDecodeRequest)
{
	nsm_dot_cak_rotate_req cak_rotate_req;

	auto rc = decode_nsm_dot_cak_rotate_req(NULL, 100, &cak_rotate_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_rotate_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = decode_nsm_dot_cak_rotate_req(request, requestMsg.size(), NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKRotate, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_cak_rotate_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t dot_blob[DOT_BLOB_SIZE];
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		dot_blob[i] = (i % 256);
	}

	uint16_t reason_code = ERR_NULL;
	auto rc = encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, reason_code,
						 dot_blob, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_FIRMWARE, response->hdr.nvidia_msg_type);

	nsm_dot_cak_rotate_resp *resp =
	    (nsm_dot_cak_rotate_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_ROTATE, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(0, resp->reserved);
	EXPECT_EQ(DOT_BLOB_SIZE, le16toh(resp->data_size));

	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ((i % 256), resp->dot_blob[i]);
	}
}

TEST(DotCAKRotate, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_cak_rotate_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	response->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	response->hdr.request = 0;
	response->hdr.datagram = 0;
	response->hdr.instance_id = 0;
	response->hdr.ocp_type = OCP_TYPE;
	response->hdr.ocp_version = OCP_VERSION_V2;
	response->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	nsm_dot_cak_rotate_resp *resp =
	    (nsm_dot_cak_rotate_resp *)response->payload;
	resp->command = NSM_FW_DOT_CAK_ROTATE;
	resp->completion_code = NSM_SUCCESS;
	resp->reserved = 0;
	resp->data_size = htole16(DOT_BLOB_SIZE);

	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		resp->dot_blob[i] = ((i * 7) % 256);
	}

	uint8_t cc = 0;
	uint16_t reason_code = 0xFFFF;
	uint8_t decoded_blob[DOT_BLOB_SIZE];

	auto rc = decode_nsm_dot_cak_rotate_resp(
	    response, responseMsg.size(), &cc, &reason_code, decoded_blob);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);

	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ(((i * 7) % 256), decoded_blob[i]);
	}
}

TEST(DotCAKRotate, testErrorResponse)
{
	std::vector<uint8_t> errorMsg(sizeof(nsm_msg_hdr) +
				      sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(errorMsg.data());

	uint16_t reason_code = 0x0042;
	auto rc = encode_nsm_dot_cak_rotate_resp(0, NSM_ERROR, reason_code,
						 NULL, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t decoded_reason_code = 0;
	rc = decode_nsm_dot_cak_rotate_resp(response, errorMsg.size(), &cc,
					    &decoded_reason_code, NULL);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(0x0042, decoded_reason_code);
}

TEST(DotCAKRotate, testEncodeDecodeRoundTrip)
{
	nsm_dot_cak_rotate_req original_req;
	dot_test::createDotCAKRotateRequest(original_req, 0, 11);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_rotate_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_dot_cak_rotate_req(7, &original_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_cak_rotate_req decoded_req;
	rc = decode_nsm_dot_cak_rotate_req(request, requestMsg.size(),
					   &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < DOT_KEY_AUTH_DATA_SIZE; i++) {
		EXPECT_EQ(original_req.new_cak[i], decoded_req.new_cak[i]);
	}
	for (size_t i = 0; i < DOT_SIGNATURE_SIZE; i++) {
		EXPECT_EQ(original_req.signature[i], decoded_req.signature[i]);
	}
}

TEST(DotCAKRotate, testEncodeDecodeResponseRoundTrip)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_cak_rotate_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t original_blob[DOT_BLOB_SIZE];
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		original_blob[i] = (i * 13) & 0xFF;
	}

	uint16_t original_reason_code = ERR_NULL;
	uint8_t original_cc = NSM_SUCCESS;

	auto rc = encode_nsm_dot_cak_rotate_resp(
	    9, original_cc, original_reason_code, original_blob, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t decoded_cc = 0;
	uint16_t decoded_reason_code = 0xFFFF;
	uint8_t decoded_blob[DOT_BLOB_SIZE];

	rc = decode_nsm_dot_cak_rotate_resp(response, responseMsg.size(),
					    &decoded_cc, &decoded_reason_code,
					    decoded_blob);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(original_cc, decoded_cc);
	EXPECT_EQ(original_reason_code, decoded_reason_code);
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ(original_blob[i], decoded_blob[i]);
	}

	nsm_dot_cak_rotate_resp *resp =
	    (nsm_dot_cak_rotate_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_CAK_ROTATE, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
}

TEST(DotCAKRotate, testNullPointerHandling)
{
	nsm_dot_cak_rotate_req cak_rotate_req;
	memset(&cak_rotate_req, 0, sizeof(cak_rotate_req));

	auto rc = encode_nsm_dot_cak_rotate_req(0, &cak_rotate_req, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_rotate_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	rc = encode_nsm_dot_cak_rotate_req(0, NULL, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	uint8_t blob[DOT_BLOB_SIZE] = {0};
	rc = encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, blob,
					    NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_cak_rotate_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	rc = decode_nsm_dot_cak_rotate_resp(NULL, 100, &cc, &reason_code, blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, blob,
					    response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_rotate_resp(response, responseMsg.size(), NULL,
					    &reason_code, blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_cak_rotate_resp(response, responseMsg.size(), &cc,
					    NULL, blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotCAKRotate, testBufferLengthValidation)
{
	nsm_dot_cak_rotate_req cak_rotate_req;
	memset(&cak_rotate_req, 0, sizeof(cak_rotate_req));

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_cak_rotate_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_cak_rotate_req(0, &cak_rotate_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_cak_rotate_req decoded_req;
	rc = decode_nsm_dot_cak_rotate_req(request, requestMsg.size(),
					   &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_rotate_req(request, requestMsg.size() - 1,
					   &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_rotate_req(request, sizeof(nsm_msg_hdr),
					   &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_cak_rotate_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t blob[DOT_BLOB_SIZE] = {0};
	rc = encode_nsm_dot_cak_rotate_resp(0, NSM_SUCCESS, ERR_NULL, blob,
					    response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;
	uint8_t decoded_blob[DOT_BLOB_SIZE];

	rc = decode_nsm_dot_cak_rotate_resp(response, responseMsg.size(), &cc,
					    &reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	rc = decode_nsm_dot_cak_rotate_resp(response, responseMsg.size() - 1,
					    &cc, &reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_cak_rotate_resp(response, sizeof(nsm_msg_hdr), &cc,
					    &reason_code, decoded_blob);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ============================================================================
// DOT UNLOCK CHALLENGE Tests
// ============================================================================

TEST(DotUnlockChallenge, testGoodEncodeRequest)
{
	nsm_dot_unlock_challenge_req unlock_challenge_req;
	unlock_challenge_req.unlock_type = 1; // Owner_Unlock

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_challenge_req(0, &unlock_challenge_req,
						      request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto req_cmd = reinterpret_cast<nsm_dot_unlock_challenge_req_command *>(
	    request->payload);
	EXPECT_EQ(req_cmd->hdr.command, NSM_FW_DOT_UNLOCK_CHALLENGE);
	EXPECT_EQ(le32toh(req_cmd->unlock_challenge_req.unlock_type), 1);
}

TEST(DotUnlockChallenge, testGoodDecodeRequest)
{
	nsm_dot_unlock_challenge_req unlock_challenge_req;
	unlock_challenge_req.unlock_type = 2; // Vendor_Unlock

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_challenge_req(0, &unlock_challenge_req,
						      request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_unlock_challenge_req decoded_req;
	rc = decode_nsm_dot_unlock_challenge_req(request, requestMsg.size(),
						 &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decoded_req.unlock_type, 2);
}

TEST(DotUnlockChallenge, testGoodEncodeResponse)
{
	uint8_t challenge[32];
	for (size_t i = 0; i < dot_test::CHALLENGE_SIZE; i++) {
		challenge[i] = static_cast<uint8_t>(i);
	}

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_unlock_challenge_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, ERR_NULL,
						       challenge, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto resp = reinterpret_cast<nsm_dot_unlock_challenge_resp *>(
	    response->payload);
	EXPECT_EQ(resp->command, NSM_FW_DOT_UNLOCK_CHALLENGE);
	EXPECT_EQ(resp->completion_code, NSM_SUCCESS);
	EXPECT_EQ(le16toh(resp->data_size), 32);
}

TEST(DotUnlockChallenge, testGoodDecodeResponse)
{
	uint8_t challenge[32];
	for (size_t i = 0; i < dot_test::CHALLENGE_SIZE; i++) {
		challenge[i] = static_cast<uint8_t>(i * 2);
	}

	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_unlock_challenge_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_nsm_dot_unlock_challenge_resp(0, NSM_SUCCESS, ERR_NULL,
						       challenge, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;
	uint8_t decoded_challenge[32];

	rc = decode_nsm_dot_unlock_challenge_resp(
	    response, responseMsg.size(), &cc, &reason_code, decoded_challenge);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);

	for (size_t i = 0; i < dot_test::CHALLENGE_SIZE; i++) {
		EXPECT_EQ(decoded_challenge[i], i * 2);
	}
}

TEST(DotUnlockChallenge, testErrorResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_nsm_dot_unlock_challenge_resp(
	    0, NSM_ERR_INVALID_DATA, 0x0003, nullptr, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;
	uint8_t decoded_challenge[32];

	rc = decode_nsm_dot_unlock_challenge_resp(
	    response, responseMsg.size(), &cc, &reason_code, decoded_challenge);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(reason_code, 0x0003);
}

TEST(DotUnlockChallenge, testNullPointerHandling)
{
	nsm_dot_unlock_challenge_req unlock_challenge_req;
	unlock_challenge_req.unlock_type = 1;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_challenge_req(0, nullptr, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_nsm_dot_unlock_challenge_req(0, &unlock_challenge_req,
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_unlock_challenge_req(nullptr, requestMsg.size(),
						 &unlock_challenge_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_unlock_challenge_req(request, requestMsg.size(),
						 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ============================================================================
// DOT UNLOCK Tests
// ============================================================================

TEST(DotUnlock, testGoodEncodeRequest)
{
	nsm_dot_unlock_req dot_unlock_req;
	memset(&dot_unlock_req, 0, sizeof(dot_unlock_req));

	for (size_t i = 0; i < dot_test::SIGNATURE_SIZE; i++) {
		dot_unlock_req.signature[i] = static_cast<uint8_t>(i % 256);
	}

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_unlock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_req(0, &dot_unlock_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto req_cmd =
	    reinterpret_cast<nsm_dot_unlock_req_command *>(request->payload);
	EXPECT_EQ(req_cmd->hdr.command, NSM_FW_DOT_UNLOCK);
}

TEST(DotUnlock, testGoodDecodeRequest)
{
	nsm_dot_unlock_req dot_unlock_req;
	memset(&dot_unlock_req, 0, sizeof(dot_unlock_req));

	for (size_t i = 0; i < dot_test::SIGNATURE_SIZE; i++) {
		dot_unlock_req.signature[i] = static_cast<uint8_t>(i + 10);
	}

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_unlock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_req(0, &dot_unlock_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_unlock_req decoded_req;
	rc =
	    decode_nsm_dot_unlock_req(request, requestMsg.size(), &decoded_req);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	for (size_t i = 0; i < dot_test::SIGNATURE_SIZE; i++) {
		EXPECT_EQ(decoded_req.signature[i], (i + 10) % 256);
	}
}

TEST(DotUnlock, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc =
	    encode_nsm_dot_unlock_resp(0, NSM_SUCCESS, ERR_NULL, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto resp = reinterpret_cast<nsm_common_resp *>(response->payload);
	EXPECT_EQ(resp->command, NSM_FW_DOT_UNLOCK);
	EXPECT_EQ(resp->completion_code, NSM_SUCCESS);
	EXPECT_EQ(le16toh(resp->data_size), 0);
}

TEST(DotUnlock, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc =
	    encode_nsm_dot_unlock_resp(0, NSM_SUCCESS, ERR_NULL, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;

	rc = decode_nsm_dot_unlock_resp(response, responseMsg.size(), &cc,
					&reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
}

TEST(DotUnlock, testErrorResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_nsm_dot_unlock_resp(0, NSM_ERR_INVALID_DATA, 0x0005,
					     response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc;
	uint16_t reason_code;

	rc = decode_nsm_dot_unlock_resp(response, responseMsg.size(), &cc,
					&reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(reason_code, 0x0005);
}

TEST(DotUnlock, testNullPointerHandling)
{
	nsm_dot_unlock_req dot_unlock_req;
	memset(&dot_unlock_req, 0, sizeof(dot_unlock_req));

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_unlock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_req(0, nullptr, request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_nsm_dot_unlock_req(0, &dot_unlock_req, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_unlock_req(nullptr, requestMsg.size(),
				       &dot_unlock_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_nsm_dot_unlock_req(request, requestMsg.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotUnlock, testBufferLengthValidation)
{
	nsm_dot_unlock_req dot_unlock_req;
	memset(&dot_unlock_req, 0, sizeof(dot_unlock_req));

	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_unlock_req_command));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_unlock_req(0, &dot_unlock_req, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_unlock_req decoded_req;
	rc = decode_nsm_dot_unlock_req(request, requestMsg.size() - 1,
				       &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_nsm_dot_unlock_req(request, sizeof(nsm_msg_hdr),
				       &decoded_req);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
// Unit tests for DOT GET INFO and GET STATUS

TEST(DotGetInfo, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_get_info_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_get_info_req(0, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_get_info_req *req = (nsm_dot_get_info_req *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_GET_INFO, req->command);
	EXPECT_EQ(0, le16toh(req->data_size));
}

TEST(DotGetInfo, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_get_info_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	// Fill header
	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.ocp_type = OCP_TYPE;
	request->hdr.ocp_version = OCP_VERSION_V2;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	// Fill payload
	nsm_dot_get_info_req *req_cmd =
	    (nsm_dot_get_info_req *)request->payload;
	req_cmd->command = NSM_FW_DOT_GET_INFO;
	req_cmd->data_size = 0;

	size_t msg_len = requestMsg.size();
	auto rc = decode_nsm_dot_get_info_req(request, msg_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DotGetInfo, testBadDecodeRequest)
{
	auto rc = decode_nsm_dot_get_info_req(NULL, 0);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotGetInfo, testTooShortDecodeRequest)
{
	std::vector<uint8_t> requestMsg(5);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_nsm_dot_get_info_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotGetInfo, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_get_info_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t version = 1;
	uint8_t fuseChangeState = 0x02;
	uint8_t transfersRemaining = 128;
	std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);
	for (size_t i = 0; i < DOT_BLOB_SIZE; i++) {
		dotBlob[i] = (i % 256);
	}

	auto rc = encode_nsm_dot_get_info_resp(
	    0, NSM_SUCCESS, 0, version, fuseChangeState, transfersRemaining,
	    dotBlob.data(), response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_get_info_resp *resp =
	    (nsm_dot_get_info_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_GET_INFO, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(1028, le16toh(resp->data_size));
	EXPECT_EQ(1, le16toh(resp->version));
	EXPECT_EQ(0x02, resp->fuse_change_state);
	EXPECT_EQ(128, resp->transfers_remaining);

	for (size_t i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ((i % 256), resp->dot_blob[i]);
	}
}

TEST(DotGetInfo, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_get_info_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Fill response
	nsm_dot_get_info_resp *resp =
	    (nsm_dot_get_info_resp *)response->payload;
	resp->command = NSM_FW_DOT_GET_INFO;
	resp->completion_code = NSM_SUCCESS;
	resp->reserved = 0;
	resp->data_size = htole16(1027);
	resp->version = htole16(1);
	resp->fuse_change_state = 0x02;
	resp->transfers_remaining = 128;
	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		resp->dot_blob[i] = (i % 256);
	}

	uint8_t cc;
	uint16_t reasonCode;
	uint16_t version;
	uint8_t fuseChangeState;
	uint8_t transfersRemaining;
	std::vector<uint8_t> dotBlob(DOT_BLOB_SIZE);

	auto rc = decode_nsm_dot_get_info_resp(
	    response, responseMsg.size(), &cc, &reasonCode, &version,
	    &fuseChangeState, &transfersRemaining, dotBlob.data());

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(1, version);
	EXPECT_EQ(0x02, fuseChangeState);
	EXPECT_EQ(128, transfersRemaining);

	for (int i = 0; i < DOT_BLOB_SIZE; i++) {
		EXPECT_EQ((i % 256), dotBlob[i]);
	}
}

TEST(DotGetInfo, testBadDecodeResponse)
{
	uint8_t cc;
	auto rc = decode_nsm_dot_get_info_resp(NULL, 0, &cc, NULL, NULL, NULL,
					       NULL, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotGetStatus, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_get_status_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_nsm_dot_get_status_req(0, request);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_TRUE(dot_test::validateNSMRequestHeader(request));

	nsm_dot_get_status_req *req =
	    (nsm_dot_get_status_req *)request->payload;
	EXPECT_EQ(NSM_FW_DOT_GET_STATUS, req->command);
	EXPECT_EQ(0, le16toh(req->data_size));
}

TEST(DotGetStatus, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_dot_get_status_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	// Fill header
	request->hdr.pci_vendor_id = htobe16(PCI_VENDOR_ID);
	request->hdr.request = 1;
	request->hdr.datagram = 0;
	request->hdr.instance_id = 0;
	request->hdr.ocp_type = OCP_TYPE;
	request->hdr.ocp_version = OCP_VERSION_V2;
	request->hdr.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	// Fill payload
	nsm_dot_get_status_req *req_cmd =
	    (nsm_dot_get_status_req *)request->payload;
	req_cmd->command = NSM_FW_DOT_GET_STATUS;
	req_cmd->data_size = 0;

	size_t msg_len = requestMsg.size();
	auto rc = decode_nsm_dot_get_status_req(request, msg_len);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DotGetStatus, testBadDecodeRequest)
{
	auto rc = decode_nsm_dot_get_status_req(NULL, 0);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotGetStatus, testTooShortDecodeRequest)
{
	std::vector<uint8_t> requestMsg(5);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_nsm_dot_get_status_req(request, msg_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(DotGetStatus, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_get_status_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t status = 1; // Volatile

	auto rc =
	    encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, 0, status, response);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	nsm_dot_get_status_resp *resp =
	    (nsm_dot_get_status_resp *)response->payload;
	EXPECT_EQ(NSM_FW_DOT_GET_STATUS, resp->command);
	EXPECT_EQ(NSM_SUCCESS, resp->completion_code);
	EXPECT_EQ(1, le16toh(resp->data_size));
	EXPECT_EQ(1, resp->status);
}

TEST(DotGetStatus, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_dot_get_status_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Fill response
	nsm_dot_get_status_resp *resp =
	    (nsm_dot_get_status_resp *)response->payload;
	resp->command = NSM_FW_DOT_GET_STATUS;
	resp->completion_code = NSM_SUCCESS;
	resp->reserved = 0;
	resp->data_size = htole16(1);
	resp->status = 2; // Locked

	uint8_t cc;
	uint16_t reasonCode;
	uint8_t status;

	auto rc = decode_nsm_dot_get_status_resp(response, responseMsg.size(),
						 &cc, &reasonCode, &status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(2, status);
}

TEST(DotGetStatus, testBadDecodeResponse)
{
	uint8_t cc;
	auto rc = decode_nsm_dot_get_status_resp(NULL, 0, &cc, NULL, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DotGetStatus, testAllStatusValues)
{
	for (uint8_t status = 0; status <= 3; status++) {
		std::vector<uint8_t> responseMsg(
		    sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp));
		auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

		auto rc = encode_nsm_dot_get_status_resp(0, NSM_SUCCESS, 0,
							 status, response);
		EXPECT_EQ(rc, NSM_SW_SUCCESS);

		uint8_t cc;
		uint16_t reasonCode;
		uint8_t decodedStatus;

		rc = decode_nsm_dot_get_status_resp(
		    response, responseMsg.size(), &cc, &reasonCode,
		    &decodedStatus);
		EXPECT_EQ(rc, NSM_SW_SUCCESS);
		EXPECT_EQ(NSM_SUCCESS, cc);
		EXPECT_EQ(status, decodedStatus);
	}
}
