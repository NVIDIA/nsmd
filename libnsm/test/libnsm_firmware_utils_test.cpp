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
	EXPECT_EQ(25, responseTest->hdr.telemetry_count);
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
	EXPECT_EQ(25, responseTest->hdr.telemetry_count);
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
	    NSM_FIRMWARE_BACKGROUND_COPY_POLICY,
	    1,
	    1,
	    NSM_FIRMWARE_ACTIVE_KEY_SET,
	    1,
	    2,
	    NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER,
	    3,
	    0xC0,
	    0xC1,
	    NSM_FIRMWARE_INBAND_UPDATE_POLICY,
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
	EXPECT_EQ(reason_code, NSM_SW_ERROR_DATA);
	EXPECT_NE(nullptr, erot_info.slot_info);
	free(erot_info.slot_info);
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
	struct nsm_firmware_security_version_number_resp sec_resp {
	};
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