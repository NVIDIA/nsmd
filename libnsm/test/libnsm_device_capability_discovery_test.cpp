/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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
#include "device-capability-discovery.h"
#include <gtest/gtest.h>

TEST(encode_nsm_get_supported_event_source_req, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_event_source_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_get_supported_event_source_req(
	    0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, request);

	struct nsm_get_supported_event_source_req *req =
	    reinterpret_cast<struct nsm_get_supported_event_source_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CURRENT_EVENT_SOURCES, req->command);
	EXPECT_EQ(NSM_GET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE, req->data_size);
}

TEST(decode_nsm_get_supported_event_source_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_GET_CURRENT_EVENT_SOURCES,	  // command
	    NSM_SUCCESS,			  // completion code
	    8,
	    0, // data size
	    1,
	    2,
	    3,
	    4,
	    5,
	    6,
	    7,
	    8 // event_sources
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	bitfield8_t *event_sources;
	auto rc = decode_nsm_get_supported_event_source_resp(
	    response, msg_len, &cc, &event_sources);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_TRUE(event_sources != NULL);
	EXPECT_EQ(event_sources[0].byte, 1);
	EXPECT_EQ(event_sources[1].byte, 2);
	EXPECT_EQ(event_sources[2].byte, 3);
	EXPECT_EQ(event_sources[3].byte, 4);
	EXPECT_EQ(event_sources[4].byte, 5);
	EXPECT_EQ(event_sources[5].byte, 6);
	EXPECT_EQ(event_sources[6].byte, 7);
	EXPECT_EQ(event_sources[7].byte, 8);
}


TEST(encode_nsm_set_event_subscription_req, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_event_subscription_req));

	uint8_t globalSetting = GLOBAL_EVENT_GENERATION_ENABLE_PUSH;
	uint8_t receiverEid = 8;

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_set_event_subscription_req(0, globalSetting,
							receiverEid, request);

	struct nsm_set_event_subscription_req *req =
	    reinterpret_cast<struct nsm_set_event_subscription_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_EVENT_SUBSCRIPTION, req->command);
	EXPECT_EQ(NSM_SET_EVENT_SUBSCRIPTION_REQ_DATA_SIZE, req->data_size);
	EXPECT_EQ(globalSetting, req->global_event_generation_setting);
	EXPECT_EQ(receiverEid, req->receiver_endpoint_id);
}

TEST(decode_nsm_set_event_subscription_req, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x80, // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SET_CURRENT_EVENT_SOURCES,	  // command
	    0x02,				  // data_size
	    GLOBAL_EVENT_GENERATION_ENABLE_PUSH,  // Global Setting
	    0x8					  // Receiver EID
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t globalSetting = 0;
	uint8_t receiverEid = 0;

	auto rc = decode_nsm_set_event_subscription_req(
	    request, msg_len, &globalSetting, &receiverEid);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(globalSetting, GLOBAL_EVENT_GENERATION_ENABLE_PUSH);
	EXPECT_EQ(receiverEid, 0x8);
}

TEST(decode_nsm_set_event_subscription_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SET_EVENT_SUBSCRIPTION,		  // command
	    NSM_SUCCESS,			  // completion code
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	auto rc =
	    decode_nsm_set_event_subscription_resp(response, msg_len, &cc);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}


TEST(encode_nsm_configure_event_acknowledgement_req, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_configure_event_acknowledgement_req));
	std::vector<uint8_t> event_sources{1, 2, 3, 4, 5, 6, 7, 8};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_configure_event_acknowledgement_req(
	    0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    (bitfield8_t *)event_sources.data(), request);

	struct nsm_configure_event_acknowledgement_req *req =
	    reinterpret_cast<struct nsm_configure_event_acknowledgement_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT, req->command);
	EXPECT_EQ(NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE,
		  req->data_size);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  req->nvidia_message_type);
	EXPECT_EQ(event_sources[0],
		  req->current_event_sources_acknowledgement_mask[0].byte);
	EXPECT_EQ(event_sources[1],
		  req->current_event_sources_acknowledgement_mask[1].byte);
	EXPECT_EQ(event_sources[2],
		  req->current_event_sources_acknowledgement_mask[2].byte);
	EXPECT_EQ(event_sources[3],
		  req->current_event_sources_acknowledgement_mask[3].byte);
}

TEST(decode_nsm_configure_event_acknowledgement_req, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x80, // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT,  // command
	    0x09,				  // data_size
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // message type
	    0x1,
	    0x2,
	    0x3,
	    0x4,
	    0x5,
	    0x6,
	    0x7,
	    0x8 // event_sources
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t nvidia_message_type = 0;
	bitfield8_t *acknowledgement_mask = NULL;

	auto rc = decode_nsm_configure_event_acknowledgement_req(
	    request, msg_len, &nvidia_message_type, &acknowledgement_mask);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(nvidia_message_type, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
	EXPECT_TRUE(acknowledgement_mask != NULL);
	EXPECT_EQ(acknowledgement_mask[0].byte, 0x1);
	EXPECT_EQ(acknowledgement_mask[1].byte, 0x2);
	EXPECT_EQ(acknowledgement_mask[2].byte, 0x3);
	EXPECT_EQ(acknowledgement_mask[3].byte, 0x4);
	EXPECT_EQ(acknowledgement_mask[4].byte, 0x5);
	EXPECT_EQ(acknowledgement_mask[5].byte, 0x6);
	EXPECT_EQ(acknowledgement_mask[6].byte, 0x7);
	EXPECT_EQ(acknowledgement_mask[7].byte, 0x8);
}

TEST(encode_nsm_configure_event_acknowledgement_resp, testGoodEncoderesponse)
{

	std::vector<uint8_t> acknowledgement_mask{1, 2, 3, 4, 5, 6, 7, 8};
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_configure_event_acknowledgement_resp));

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto rc = encode_nsm_configure_event_acknowledgement_resp(
	    0, NSM_SUCCESS, (bitfield8_t *)acknowledgement_mask.data(),
	    response);

	struct nsm_configure_event_acknowledgement_resp *resp =
	    reinterpret_cast<struct nsm_configure_event_acknowledgement_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT, resp->command);
	EXPECT_EQ(EVENT_ACKNOWLEDGEMENT_MASK_LENGTH, resp->data_size);
	EXPECT_EQ(acknowledgement_mask[0],
		  resp->new_event_sources_acknowledgement_mask[0].byte);
	EXPECT_EQ(acknowledgement_mask[1],
		  resp->new_event_sources_acknowledgement_mask[1].byte);
	EXPECT_EQ(acknowledgement_mask[2],
		  resp->new_event_sources_acknowledgement_mask[2].byte);
	EXPECT_EQ(acknowledgement_mask[3],
		  resp->new_event_sources_acknowledgement_mask[3].byte);
	EXPECT_EQ(acknowledgement_mask[4],
		  resp->new_event_sources_acknowledgement_mask[4].byte);
	EXPECT_EQ(acknowledgement_mask[5],
		  resp->new_event_sources_acknowledgement_mask[5].byte);
	EXPECT_EQ(acknowledgement_mask[6],
		  resp->new_event_sources_acknowledgement_mask[6].byte);
	EXPECT_EQ(acknowledgement_mask[7],
		  resp->new_event_sources_acknowledgement_mask[7].byte);
}

TEST(decode_nsm_configure_event_acknowledgement_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT,  // command
	    NSM_SUCCESS,			  // completion code
	    0,
	    8, // data size
	    1,
	    2,
	    3,
	    4,
	    5,
	    6,
	    7,
	    8 // acknowledgement mask
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	bitfield8_t *acknowledgement_mask;
	auto rc = decode_nsm_configure_event_acknowledgement_resp(
	    response, msg_len, &cc, &acknowledgement_mask);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(acknowledgement_mask[0].byte, 1);
	EXPECT_EQ(acknowledgement_mask[1].byte, 2);
	EXPECT_EQ(acknowledgement_mask[2].byte, 3);
	EXPECT_EQ(acknowledgement_mask[3].byte, 4);
	EXPECT_EQ(acknowledgement_mask[4].byte, 5);
	EXPECT_EQ(acknowledgement_mask[5].byte, 6);
	EXPECT_EQ(acknowledgement_mask[6].byte, 7);
	EXPECT_EQ(acknowledgement_mask[7].byte, 8);
}

TEST(encode_nsm_set_current_event_sources_req, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_current_event_source_req));
	std::vector<uint8_t> event_sources{0, 0, 0, 0, 0, 0, 0, 0};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_set_current_event_sources_req(
	    0, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    (bitfield8_t *)event_sources.data(), request);

	struct nsm_set_current_event_source_req *req =
	    reinterpret_cast<struct nsm_set_current_event_source_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_CURRENT_EVENT_SOURCES, req->command);
	EXPECT_EQ(NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE, req->data_size);
}

TEST(decode_nsm_set_current_event_source_req, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x80, // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SET_CURRENT_EVENT_SOURCES,	  // command
	    0x09,				  // data_size
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // message type
	    0x1,
	    0x2,
	    0x3,
	    0x4,
	    0x5,
	    0x6,
	    0x7,
	    0x8 // event_sources
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	uint8_t nvidia_message_type = 0;
	bitfield8_t *event_sources = NULL;

	auto rc = decode_nsm_set_current_event_source_req(
	    request, msg_len, &nvidia_message_type, &event_sources);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(nvidia_message_type, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
	EXPECT_TRUE(event_sources != NULL);
	EXPECT_EQ(event_sources[0].byte, 0x1);
	EXPECT_EQ(event_sources[1].byte, 0x2);
	EXPECT_EQ(event_sources[2].byte, 0x3);
	EXPECT_EQ(event_sources[3].byte, 0x4);
	EXPECT_EQ(event_sources[4].byte, 0x5);
	EXPECT_EQ(event_sources[5].byte, 0x6);
	EXPECT_EQ(event_sources[6].byte, 0x7);
	EXPECT_EQ(event_sources[7].byte, 0x8);
}

TEST(decode_nsm_set_current_event_sources_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_SET_CURRENT_EVENT_SOURCES,	  // command
	    NSM_SUCCESS,			  // completion code
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	auto rc =
	    decode_nsm_set_current_event_sources_resp(response, msg_len, &cc);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(decode_nsm_get_event_log_record_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_GET_EVENT_LOG_RECORD,		  // command
	    0,					  // completion code
	    14,
	    0,					  // data size
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // nvidia_message_type
	    0xaa,				  // event_id
	    0x78,
	    0x56,
	    0x34,
	    0x12, // event_handle
	    0x88,
	    0x77,
	    0x66,
	    0x55,
	    0x44,
	    0x33,
	    0x22,
	    0x11 // timestamp
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint8_t nvidia_message_type = 0;
	uint8_t event_id = 0;
	uint32_t event_handle = 0;
	uint64_t timestamp = 0;
	uint16_t payload_len = 0;
	uint8_t *payload = NULL;

	auto rc = decode_nsm_get_event_log_record_resp(
	    response, msg_len, &cc, &nvidia_message_type, &event_id,
	    &event_handle, &timestamp, &payload, &payload_len);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(nvidia_message_type, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
	EXPECT_EQ(event_id, 0xaa);
	EXPECT_EQ(event_handle, 0x12345678);
	EXPECT_EQ(timestamp, 0x1122334455667788);
	EXPECT_EQ(payload_len, 0);
	EXPECT_TRUE(payload == NULL);
}
