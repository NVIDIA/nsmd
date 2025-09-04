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

	EXPECT_EQ(NSM_GET_CURRENT_EVENT_SOURCES, req->hdr.command);
	EXPECT_EQ(NSM_GET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE,
		  req->hdr.data_size);
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
	    0,
	    0, // reserved
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
	uint16_t reason_code = 0;
	bitfield8_t event_sources[EVENT_SOURCES_LENGTH];
	auto rc = decode_nsm_get_supported_event_source_resp(
	    response, msg_len, &cc, &reason_code, &event_sources[0]);

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

TEST(nsm_get_event_subscription, testRequest)
{
	testEncodeCommonRequest(encode_nsm_get_event_subscription_req,
				NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				NSM_GET_EVENT_SUBSCRIPTION);
	testDecodeCommonRequest(decode_nsm_get_event_subscription_req,
				NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				NSM_GET_EVENT_SUBSCRIPTION);
}

TEST(nsm_get_event_subscription, testResponse)
{
	auto encodeNsmGetEventSubscriptionResp =
	    [](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
	       const uint8_t *data, nsm_msg *msg) {
		    if (data == nullptr) {
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return encode_nsm_get_event_subscription_resp(
			instanceId, cc, reasonCode, *data, msg);
	    };
	uint8_t receiverEid = 8;
	uint8_t expectedReceiverEid = 8;

	testEncodeResponse<uint8_t>(encodeNsmGetEventSubscriptionResp,
				    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				    NSM_GET_EVENT_SUBSCRIPTION,
				    expectedReceiverEid, receiverEid);
	testDecodeResponse<uint8_t>(&decode_nsm_get_event_subscription_resp,
				    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				    NSM_GET_EVENT_SUBSCRIPTION,
				    expectedReceiverEid, receiverEid);
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

	EXPECT_EQ(NSM_SET_EVENT_SUBSCRIPTION, req->hdr.command);
	EXPECT_EQ(NSM_SET_EVENT_SUBSCRIPTION_REQ_DATA_SIZE, req->hdr.data_size);
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
	    0, // reserved
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

	EXPECT_EQ(NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT, req->hdr.command);
	EXPECT_EQ(NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE,
		  req->hdr.data_size);
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

	EXPECT_EQ(NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT, resp->hdr.command);
	EXPECT_EQ(EVENT_ACKNOWLEDGEMENT_MASK_LENGTH, resp->hdr.data_size);
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
	    0, // reserved
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

	EXPECT_EQ(NSM_SET_CURRENT_EVENT_SOURCES, req->hdr.command);
	EXPECT_EQ(NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE,
		  req->hdr.data_size);
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
	    0, // reserved
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
	    0,
	    0, // reserved
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

TEST(encode_nsm_get_device_capabilities_v2_req, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_nsm_get_device_capabilities_v2_req(0, request);

	struct nsm_get_device_capabilities_v2_req *req =
	    reinterpret_cast<struct nsm_get_device_capabilities_v2_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_DEVICE_CAPABILITIES_V2, req->hdr.command);
	EXPECT_EQ(0, req->hdr.data_size);
}

TEST(decode_nsm_get_device_capabilities_v2_req, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x80, // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_CAPABILITIES_V2,	  // command
	    0x00				  // data_size
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();

	auto rc = decode_nsm_get_device_capabilities_v2_req(request, msg_len);

	EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(encode_nsm_get_device_capabilities_v2_resp, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_resp) -
	    1 + NSM_GET_DEVICE_CAPABILITIES_V2_DATA_SIZE);

	enum8 timestamp_generation = 1;
	uint32_t maximum_input_buffer_size = 0x12345678;

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto rc = encode_nsm_get_device_capabilities_v2_resp(
	    0, NSM_SUCCESS, 0, timestamp_generation, maximum_input_buffer_size,
	    response);

	struct nsm_get_device_capabilities_v2_resp *resp =
	    reinterpret_cast<struct nsm_get_device_capabilities_v2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_DEVICE_CAPABILITIES_V2, resp->hdr.command);
	EXPECT_EQ(NSM_SUCCESS, resp->hdr.completion_code);
	EXPECT_EQ(2, resp->hdr.telemetry_count);

	// Verify the tagged data format
	uint8_t *data = &(resp->payload[0]);

	// First tag: NSM_TAG_TIMESTAMP_GENERATION (0)
	EXPECT_EQ(0, data[0]);	  // tag
	EXPECT_EQ(0x01, data[1]); // valid=1, length=0, reserved=0
	EXPECT_EQ(timestamp_generation, data[2]); // value

	// Second tag: NSM_TAG_MAXIMUM_INPUT_BUFFER_SIZE (1)
	EXPECT_EQ(1, data[3]);	  // tag
	EXPECT_EQ(0x05, data[4]); // valid=1, length=2, reserved=0
	EXPECT_EQ(0x78, data[5]); // value (little endian)
	EXPECT_EQ(0x56, data[6]);
	EXPECT_EQ(0x34, data[7]);
	EXPECT_EQ(0x12, data[8]);
}

TEST(decode_nsm_get_device_capabilities_v2_resp, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE, // PCI VID: NVIDIA 0x10DE
	    0x00, // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89, // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_CAPABILITIES_V2,	  // command
	    NSM_SUCCESS,			  // completion code
	    2,
	    0,	  // telemetry_count (2 tags)
	    0,	  // NSM_TAG_TIMESTAMP_GENERATION tag
	    0x01, // valid=1, length=0, reserved=0
	    2,	  // timestamp_generation value
	    1,	  // NSM_TAG_MAXIMUM_INPUT_BUFFER_SIZE tag
	    0x05, // valid=1, length=2, reserved=0
	    0x78,
	    0x56,
	    0x34,
	    0x12 // maximum_input_buffer_size (little endian)
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum8 timestamp_generation = 0;
	uint32_t maximum_input_buffer_size = 0;

	auto rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, msg_len, &cc, &reason_code, &timestamp_generation,
	    &maximum_input_buffer_size);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(timestamp_generation,
		  NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_MONOTONIC_TIME);
	EXPECT_EQ(maximum_input_buffer_size, 0x12345678);
}

TEST(encode_nsm_get_device_capabilities_v2_req, testNullPointer)
{
	auto rc = encode_nsm_get_device_capabilities_v2_req(0, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decode_nsm_get_device_capabilities_v2_req, testNullPointer)
{
	auto rc = decode_nsm_get_device_capabilities_v2_req(NULL, 0);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decode_nsm_get_device_capabilities_v2_req, testInvalidLength)
{
	std::vector<uint8_t> requestMsg{0x10, 0xDE}; // Too short
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = decode_nsm_get_device_capabilities_v2_req(request,
							    requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(encode_nsm_get_device_capabilities_v2_resp, testNullPointer)
{
	auto rc = encode_nsm_get_device_capabilities_v2_resp(0, NSM_SUCCESS, 0,
							     1, 1024, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decode_nsm_get_device_capabilities_v2_resp, testNullPointers)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,
	    0x00,
	    0x89,
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    NSM_GET_DEVICE_CAPABILITIES_V2,
	    NSM_SUCCESS,
	    0,
	    0,
	    2,
	    0,	  // telemetry_count (2 tags)
	    0,	  // NSM_TAG_TIMESTAMP_GENERATION tag
	    0x01, // valid=1, length=0, reserved=0
	    2,	  // timestamp_generation value
	    1,	  // NSM_TAG_MAXIMUM_INPUT_BUFFER_SIZE tag
	    0x05, // valid=1, length=2, reserved=0
	    0x78,
	    0x56,
	    0x34,
	    0x12};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Test NULL cc
	uint16_t reason_code = 0;
	enum8 timestamp_generation = 0;
	uint32_t maximum_input_buffer_size = 0;
	auto rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), NULL, &reason_code,
	    &timestamp_generation, &maximum_input_buffer_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test NULL reason_code
	uint8_t cc = 0;
	rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, NULL, &timestamp_generation,
	    &maximum_input_buffer_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test NULL timestamp_generation
	rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code, NULL,
	    &maximum_input_buffer_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	// Test NULL maximum_input_buffer_size
	rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code,
	    &timestamp_generation, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(decode_nsm_get_device_capabilities_v2_resp, testInvalidLength)
{
	std::vector<uint8_t> responseMsg{0x10, 0xDE}; // Too short
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum8 timestamp_generation = 0;
	uint32_t maximum_input_buffer_size = 0;

	auto rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code,
	    &timestamp_generation, &maximum_input_buffer_size);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(encode_decode_nsm_get_device_capabilities_v2_resp, testRoundTrip)
{
	// Test that encode and decode work together correctly
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_resp) +
	    -1 + NSM_GET_DEVICE_CAPABILITIES_V2_DATA_SIZE);

	enum8 original_timestamp_generation =
	    NSM_DEVICE_CAPABILITY_TIMESTAMP_GENERATION_MONOTONIC_TIME;
	uint32_t original_maximum_input_buffer_size = 0x12345678;

	// Encode the response
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto encode_rc = encode_nsm_get_device_capabilities_v2_resp(
	    0, NSM_SUCCESS, 0, original_timestamp_generation,
	    original_maximum_input_buffer_size, response);

	EXPECT_EQ(encode_rc, NSM_SUCCESS);

	// Decode the response
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum8 decoded_timestamp_generation = 0;
	uint32_t decoded_maximum_input_buffer_size = 0;

	auto decode_rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code,
	    &decoded_timestamp_generation, &decoded_maximum_input_buffer_size);

	EXPECT_EQ(decode_rc, NSM_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(decoded_timestamp_generation, original_timestamp_generation);
	EXPECT_EQ(decoded_maximum_input_buffer_size,
		  original_maximum_input_buffer_size);
}

TEST(encode_nsm_get_device_capabilities_v2_resp, testErrorResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_resp) -
	    1 + NSM_GET_DEVICE_CAPABILITIES_V2_DATA_SIZE);

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	// Test with error completion code
	auto rc = encode_nsm_get_device_capabilities_v2_resp(
	    0, NSM_ERR_INVALID_DATA, 0x1234, 1, 1024, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify the response contains the error code
	struct nsm_get_device_capabilities_v2_resp *resp =
	    reinterpret_cast<struct nsm_get_device_capabilities_v2_resp *>(
		response->payload);
	EXPECT_EQ(resp->hdr.completion_code, NSM_ERR_INVALID_DATA);
}

TEST(decode_nsm_get_device_capabilities_v2_resp, testInvalidTagValue)
{
	// Test with invalid tag value
	std::vector<uint8_t> responseMsg{0x10,
					 0xDE,
					 0x00,
					 0x89,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
					 NSM_GET_DEVICE_CAPABILITIES_V2,
					 NSM_SUCCESS,
					 2,
					 0, // telemetry_count = 2
					 0xFF,
					 0x01,
					 2, // Invalid tag 0xFF
					 1,
					 0x05,
					 0x78,
					 0x56,
					 0x34,
					 0x12};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum8 timestamp_generation = 0;
	uint32_t maximum_input_buffer_size = 0;

	auto rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code,
	    &timestamp_generation, &maximum_input_buffer_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(decode_nsm_get_device_capabilities_v2_resp, testInvalidValidFlag)
{
	// Test with invalid valid flag (should be 1)
	std::vector<uint8_t> responseMsg{0x10,
					 0xDE,
					 0x00,
					 0x89,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
					 NSM_GET_DEVICE_CAPABILITIES_V2,
					 NSM_SUCCESS,
					 2,
					 0, // telemetry_count = 2
					 0,
					 0x00,
					 2, // valid=0 (invalid)
					 1,
					 0x05,
					 0x78,
					 0x56,
					 0x34,
					 0x12};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum8 timestamp_generation = 0;
	uint32_t maximum_input_buffer_size = 0;

	auto rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code,
	    &timestamp_generation, &maximum_input_buffer_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(decode_nsm_get_device_capabilities_v2_resp, testInsufficientTelemetryCount)
{
	// Test with telemetry_count < 2
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,
	    0x00,
	    0x89,
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    NSM_GET_DEVICE_CAPABILITIES_V2,
	    NSM_SUCCESS,
	    1,
	    0, // telemetry_count = 1 (should be >= 2)
	    3,
	    0, // data_size = 3 (only one tag)
	    0,
	    0x01,
	    2 // Only one tag
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	enum8 timestamp_generation = 0;
	uint32_t maximum_input_buffer_size = 0;

	auto rc = decode_nsm_get_device_capabilities_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code,
	    &timestamp_generation, &maximum_input_buffer_size);

	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
