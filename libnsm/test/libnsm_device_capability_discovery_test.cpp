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
#include <cstddef>
#include <gtest/gtest.h>

TEST(nsm_get_supported_event, testRequest)
{
	const uint8_t expectedNvidiaMessageType = NSM_TYPE_NETWORK_PORT;
	uint8_t nvidiaMessageType;
	auto encodeNsmGetSupportedEventReq =
	    [](uint8_t instanceId, const uint8_t *nvidiaMessageType,
	       struct nsm_msg *msg) {
		    if (nvidiaMessageType == nullptr) {
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return encode_nsm_get_supported_event_source_req(
			instanceId, *nvidiaMessageType, msg);
	    };
	testEncodeRequest<uint8_t>(
	    encodeNsmGetSupportedEventReq, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    NSM_GET_SUPPORTED_EVENT_SOURCES, expectedNvidiaMessageType,
	    nvidiaMessageType);
	testDecodeRequest<uint8_t>(decode_nsm_get_event_source_req,
				   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				   NSM_GET_SUPPORTED_EVENT_SOURCES,
				   expectedNvidiaMessageType,
				   nvidiaMessageType);
	EXPECT_EQ(expectedNvidiaMessageType, nvidiaMessageType);
}

TEST(nsm_get_supported_event, testResponse)
{
	const bitfield8_t expectedEventSources[EVENT_SOURCES_LENGTH] = {
	    0, 1, 2, 3, 4, 5, 6, 7};
	bitfield8_t eventSources[EVENT_SOURCES_LENGTH];
	auto encodeNsmGetSupportedEventSourceResp =
	    [](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
	       bitfield8_t *const *data, nsm_msg *msg) {
		    return encode_nsm_get_supported_event_source_resp(
			instanceId, cc, reasonCode,
			reinterpret_cast<const bitfield8_t *>(data), msg);
	    };
	auto decodeNsmGetEventSourceResp = [](const nsm_msg *msg, size_t msgLen,
					      uint8_t *cc, uint16_t *reasonCode,
					      bitfield8_t **eventSources) {
		return decode_nsm_get_event_source_resp(
		    msg, msgLen, cc, reasonCode,
		    reinterpret_cast<bitfield8_t *>(eventSources));
	};
	testEncodeResponse<bitfield8_t *>(
	    encodeNsmGetSupportedEventSourceResp,
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    NSM_GET_SUPPORTED_EVENT_SOURCES,
	    reinterpret_cast<bitfield8_t *const &>(expectedEventSources),
	    reinterpret_cast<bitfield8_t *&>(eventSources),
	    sizeof(eventSources));
	testDecodeResponse<bitfield8_t *>(
	    decodeNsmGetEventSourceResp, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    NSM_GET_SUPPORTED_EVENT_SOURCES,
	    reinterpret_cast<bitfield8_t *const &>(expectedEventSources),
	    reinterpret_cast<bitfield8_t *&>(eventSources),
	    sizeof(eventSources));
	EXPECT_EQ(
	    memcmp(expectedEventSources, eventSources, sizeof(eventSources)),
	    0);
}

TEST(nsm_get_current_event_sources, testRequest)
{
	const uint8_t expectedNvidiaMessageType =
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
	uint8_t nvidiaMessageType;
	auto encodeNsmGetCurrentEventSourcesReq =
	    [](uint8_t instanceId, const uint8_t *nvidiaMessageType,
	       struct nsm_msg *msg) {
		    if (nvidiaMessageType == nullptr) {
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return encode_nsm_get_current_event_source_req(
			instanceId, *nvidiaMessageType, msg);
	    };
	testEncodeRequest<uint8_t>(
	    encodeNsmGetCurrentEventSourcesReq,
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_GET_CURRENT_EVENT_SOURCES,
	    expectedNvidiaMessageType, nvidiaMessageType);
	testDecodeRequest<uint8_t>(
	    decode_nsm_get_event_source_req,
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_GET_CURRENT_EVENT_SOURCES,
	    expectedNvidiaMessageType, nvidiaMessageType);
	EXPECT_EQ(expectedNvidiaMessageType, nvidiaMessageType);
}

TEST(nsm_get_current_event_sources, testResponse)
{
	const bitfield8_t expectedEventSources[EVENT_SOURCES_LENGTH] = {
	    0, 1, 2, 3, 4, 5, 6, 7};
	bitfield8_t eventSources[EVENT_SOURCES_LENGTH];
	auto encodeNsmGetCurrentEventSourceResp =
	    [](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
	       bitfield8_t *const *data, nsm_msg *msg) {
		    return encode_nsm_get_current_event_source_resp(
			instanceId, cc, reasonCode,
			reinterpret_cast<const bitfield8_t *>(data), msg);
	    };
	auto decodeNsmGetEventSourceResp = [](const nsm_msg *msg, size_t msgLen,
					      uint8_t *cc, uint16_t *reasonCode,
					      bitfield8_t **eventSources) {
		return decode_nsm_get_event_source_resp(
		    msg, msgLen, cc, reasonCode,
		    reinterpret_cast<bitfield8_t *>(eventSources));
	};
	testEncodeResponse<bitfield8_t *>(
	    encodeNsmGetCurrentEventSourceResp,
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_GET_CURRENT_EVENT_SOURCES,
	    reinterpret_cast<bitfield8_t *const &>(expectedEventSources),
	    reinterpret_cast<bitfield8_t *&>(eventSources),
	    sizeof(eventSources));
	testDecodeResponse<bitfield8_t *>(
	    decodeNsmGetEventSourceResp, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
	    NSM_GET_CURRENT_EVENT_SOURCES,
	    reinterpret_cast<bitfield8_t *const &>(expectedEventSources),
	    reinterpret_cast<bitfield8_t *&>(eventSources),
	    sizeof(eventSources));
	EXPECT_EQ(
	    memcmp(expectedEventSources, eventSources, sizeof(eventSources)),
	    0);
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

TEST(decode_nsm_set_current_event_sources_req, testGoodDecodeRequest)
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
	bitfield8_t event_sources[EVENT_SOURCES_LENGTH] = {0};

	auto rc = decode_nsm_set_current_event_sources_req(
	    request, msg_len, &nvidia_message_type, event_sources);

	EXPECT_EQ(rc, NSM_SUCCESS);
	EXPECT_EQ(nvidia_message_type, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
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

// Branch coverage: encode_nsm_set_event_subscription_req L259
TEST(DevCapDiscNullBranch, EncodeSetEventSubscriptionReq_NullMsg)
{
	auto rc = encode_nsm_set_event_subscription_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// Branch coverage: encode_nsm_configure_event_acknowledgement_req L336
TEST(DevCapDiscNullBranch, EncodeConfigureEventAckReq_NullMsg)
{
	bitfield8_t mask[8] = {};
	auto rc =
	    encode_nsm_configure_event_acknowledgement_req(0, 0, mask, nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// Branch coverage: encode_nsm_configure_event_acknowledgement_resp L398
// (new_event_sources_acknowledgement_mask == NULL || msg == NULL)
TEST(DevCapDiscNullBranch, EncodeConfigureEventAckResp_NullMask)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_nsm_configure_event_acknowledgement_resp(
	    0, NSM_SUCCESS, nullptr, msg);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(DevCapDiscNullBranch, EncodeConfigureEventAckResp_NullMsg)
{
	bitfield8_t mask[8] = {};
	auto rc = encode_nsm_configure_event_acknowledgement_resp(
	    0, NSM_SUCCESS, mask, nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// Branch coverage: encode_nsm_set_current_event_sources_req L453
// (event_sources == NULL || msg == NULL)
TEST(DevCapDiscNullBranch, EncodeSetCurrentEventSourcesReq_NullSources)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_nsm_set_current_event_sources_req(0, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevCapDiscNullBranch, EncodeSetCurrentEventSourcesReq_NullMsg)
{
	bitfield8_t sources[8] = {};
	auto rc =
	    encode_nsm_set_current_event_sources_req(0, 0, sources, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// Branch coverage: encode_nsm_get_event_log_record_req L530
TEST(DevCapDiscNullBranch, EncodeGetEventLogRecordReq_NullMsg)
{
	auto rc = encode_nsm_get_event_log_record_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// encode_nsm_set_event_subscription_req L259: msg == NULL
TEST(DevCapDiscNullBranch, EncodeSetEventSubscriptionReq_NullMsg2)
{
	auto rc = encode_nsm_set_event_subscription_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_set_event_subscription_req L289: global_setting == NULL
TEST(DevCapDiscNullBranch, DecodeSetEventSubscriptionReq_NullGlobalSetting)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t receiver_eid = 0;
	auto rc = decode_nsm_set_event_subscription_req(msg, buf.size(),
							nullptr, &receiver_eid);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_set_event_subscription_resp L314: msg == NULL
TEST(DevCapDiscNullBranch, DecodeSetEventSubscriptionResp_NullMsg)
{
	uint8_t cc = 0;
	auto rc = decode_nsm_set_event_subscription_resp(nullptr, 0, &cc);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(DevCapDiscNullBranch, DecodeSetEventSubscriptionResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 8, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_nsm_set_event_subscription_resp(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_configure_event_acknowledgement_req L368: msg == NULL
TEST(DevCapDiscNullBranch, DecodeConfigureEventAckReq_NullMsg)
{
	uint8_t nvidia_msg_type = 0;
	bitfield8_t *mask = nullptr;
	auto rc = decode_nsm_configure_event_acknowledgement_req(
	    nullptr, 0, &nvidia_msg_type, &mask);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_configure_event_acknowledgement_resp L429: msg == NULL
TEST(DevCapDiscNullBranch, DecodeConfigureEventAckResp_NullMsg)
{
	uint8_t cc = 0;
	bitfield8_t *mask = nullptr;
	auto rc = decode_nsm_configure_event_acknowledgement_resp(nullptr, 0,
								  &cc, &mask);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(DevCapDiscNullBranch, DecodeConfigureEventAckResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	bitfield8_t *mask = nullptr;
	auto rc = decode_nsm_configure_event_acknowledgement_resp(
	    msg, buf.size(), nullptr, &mask);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_set_current_event_sources_req L482: msg == NULL
TEST(DevCapDiscNullBranch, DecodeSetCurrentEventSourcesReq_NullMsg)
{
	uint8_t nvidia_msg_type = 0;
	bitfield8_t sources[EVENT_SOURCES_LENGTH] = {};
	auto rc = decode_nsm_set_current_event_sources_req(
	    nullptr, 0, &nvidia_msg_type, sources);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_set_current_event_sources_resp L509: msg == NULL
TEST(DevCapDiscNullBranch, DecodeSetCurrentEventSourcesResp_NullMsg)
{
	uint8_t cc = 0;
	auto rc = decode_nsm_set_current_event_sources_resp(nullptr, 0, &cc);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(DevCapDiscNullBranch, DecodeSetCurrentEventSourcesResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 8, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_nsm_set_current_event_sources_resp(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_get_event_log_record_resp L560: msg == NULL
TEST(DevCapDiscNullBranch, DecodeGetEventLogRecordResp_NullMsg)
{
	uint8_t cc = 0, nvidia_msg_type = 0, event_id = 0;
	uint32_t event_handle = 0;
	uint64_t timestamp = 0;
	uint8_t *payload = nullptr;
	uint16_t payload_len = 0;
	auto rc = decode_nsm_get_event_log_record_resp(
	    nullptr, 0, &cc, &nvidia_msg_type, &event_id, &event_handle,
	    &timestamp, &payload, &payload_len);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(DevCapDiscNullBranch, DecodeGetEventLogRecordResp_NullCc)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t nvidia_msg_type = 0, event_id = 0;
	uint32_t event_handle = 0;
	uint64_t timestamp = 0;
	uint8_t *payload = nullptr;
	uint16_t payload_len = 0;
	auto rc = decode_nsm_get_event_log_record_resp(
	    msg, buf.size(), nullptr, &nvidia_msg_type, &event_id,
	    &event_handle, &timestamp, &payload, &payload_len);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// decode_nsm_rediscovery_event L599: msg == NULL
TEST(DevCapDiscNullBranch, DecodeRediscoveryEvent_NullMsg)
{
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	auto rc = decode_nsm_rediscovery_event(nullptr, 0, &event_class,
					       &event_state);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(DevCapDiscNullBranch, DecodeRediscoveryEvent_NullEventClass)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 16, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t event_state = 0;
	auto rc = decode_nsm_rediscovery_event(msg, buf.size(), nullptr,
					       &event_state);
	EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

// encode_nsm_gpio_state_change_event L778: msg == NULL
TEST(DevCapDiscNullBranch, EncodeGpioStateChangeEvent_NullMsg)
{
	struct nsm_gpio_state_change_event_payload payload = {};
	auto rc =
	    encode_nsm_gpio_state_change_event(0, false, &payload, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevCapDiscNullBranch, EncodeGpioStateChangeEvent_NullPayload)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 64, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_nsm_gpio_state_change_event(0, false, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_nsm_gpio_state_change_event L819: event_state == NULL or payload ==
// NULL
TEST(DevCapDiscNullBranch, DecodeGpioStateChangeEvent_NullMsg)
{
	uint8_t event_class = 0;
	uint16_t event_state = 0;
	struct nsm_gpio_state_change_event_payload *payload = nullptr;
	auto rc = decode_nsm_gpio_state_change_event(nullptr, 0, &event_class,
						     &event_state, &payload);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevCapDiscNullBranch, DecodeGpioStateChangeEvent_NullEventClass)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 32, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t event_state = 0;
	struct nsm_gpio_state_change_event_payload *payload = nullptr;
	auto rc = decode_nsm_gpio_state_change_event(msg, buf.size(), nullptr,
						     &event_state, &payload);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ---------------------------------------------------------------------------
// Buffer-overread guard regression (Glasswing). Decoders that hand out a
// pointer/length into the received message must reject an on-wire data_size
// whose payload would extend past the bytes actually present (msg_len),
// returning an error instead of an out-of-bounds read. Each guard is covered
// with the malicious case, the exact boundary, and a well-formed happy path.
// ---------------------------------------------------------------------------
namespace
{
// data_size field offset: nsm_msg_hdr + nsm_common_resp{cmd:1,cc:1,rsvd:2}.
constexpr size_t kDataSizeOff = sizeof(nsm_msg_hdr) + 4;

void setU16(std::vector<uint8_t> &buf, size_t off, uint16_t v)
{
	buf[off] = static_cast<uint8_t>(v & 0xFF);
	buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}
const nsm_msg *asMsg(const std::vector<uint8_t> &b)
{
	return reinterpret_cast<const nsm_msg *>(b.data());
}
} // namespace

// ---- Get Event Log Record V2, first handle (09-HIGH) ----

TEST(DevCapDiscOverreadGuard, EventLogV2First_OversizedRejected)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_first_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE +
		   40);
	uint8_t cc = 0;
	nsm_event_log_record_v2_first_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DevCapDiscOverreadGuard, EventLogV2First_BoundaryPlusOneRejected)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_first_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4,
			       0); // holds 4 bytes
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE + 5);
	uint8_t cc = 0;
	nsm_event_log_record_v2_first_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DevCapDiscOverreadGuard, EventLogV2First_BoundaryExactAccepted)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_first_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE + 4);
	uint8_t cc = 0;
	nsm_event_log_record_v2_first_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(out.event_data_len, 4);
	EXPECT_NE(out.event_data, nullptr);
}

TEST(DevCapDiscOverreadGuard, EventLogV2First_EmptyPayload)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_first_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE);
	uint8_t cc = 0;
	nsm_event_log_record_v2_first_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(out.event_data_len, 0);
	EXPECT_EQ(out.event_data, nullptr);
}

TEST(DevCapDiscOverreadGuard, EventLogV2First_HappyPathFields)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_first_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 2, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE + 2);
	const size_t p = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp);
	setU16(b, p, 0x1234);	  // next_transfer_handle
	setU16(b, p + 2, 0x5678); // event_handle
	uint8_t cc = 0;
	nsm_event_log_record_v2_first_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(out.next_transfer_handle, 0x1234);
	EXPECT_EQ(out.event_handle, 0x5678);
	EXPECT_EQ(out.event_data_len, 2);
}

// ---- Get Event Log Record V2, next handle (09-HIGH) ----

TEST(DevCapDiscOverreadGuard, EventLogV2Next_OversizedRejected)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_next_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE + 40);
	uint8_t cc = 0;
	nsm_event_log_record_v2_next_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_next_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DevCapDiscOverreadGuard, EventLogV2Next_BoundaryExactAccepted)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_next_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE + 4);
	uint8_t cc = 0;
	nsm_event_log_record_v2_next_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_next_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(out.event_data_len, 4);
	EXPECT_NE(out.event_data, nullptr);
}

TEST(DevCapDiscOverreadGuard, EventLogV2Next_EmptyPayload)
{
	constexpr size_t off =
	    offsetof(nsm_get_event_log_record_v2_resp_next_handle, event_data);
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + off + 4, 0);
	setU16(b, kDataSizeOff,
	       NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE);
	uint8_t cc = 0;
	nsm_event_log_record_v2_next_fields out{};
	EXPECT_EQ(decode_nsm_get_event_log_record_v2_resp_next_handle(
		      asMsg(b), b.size(), &cc, &out),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(out.event_data_len, 0);
	EXPECT_EQ(out.event_data, nullptr);
}

// ---- GPIO state-change event (V16.2) ----

namespace
{
constexpr size_t kGpioFixed =
    sizeof(nsm_gpio_state_change_event_payload) - sizeof(nsm_gpio_event);
void setEventDataSize(std::vector<uint8_t> &b, uint8_t v)
{
	b[sizeof(nsm_msg_hdr) + offsetof(nsm_event, data_size)] = v;
}
void setNumGpioEvents(std::vector<uint8_t> &b, uint16_t n)
{
	setU16(b, sizeof(nsm_msg_hdr) + offsetof(nsm_event, data) + 8, n);
}
} // namespace

TEST(DevCapDiscOverreadGuard, Gpio_DataSizeBelowFixedPrefixRejected)
{
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 10, 0);
	setEventDataSize(b, 5); // < 10-byte fixed prefix
	uint8_t ec = 0;
	uint16_t es = 0;
	nsm_gpio_state_change_event_payload *p = nullptr;
	EXPECT_EQ(decode_nsm_gpio_state_change_event(asMsg(b), b.size(), &ec,
						     &es, &p),
		  NSM_SW_ERROR_DATA);
}

TEST(DevCapDiscOverreadGuard, Gpio_DataSizeExceedsMsgLenRejected)
{
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 10, 0);
	setEventDataSize(b, 110);
	uint8_t ec = 0;
	uint16_t es = 0;
	nsm_gpio_state_change_event_payload *p = nullptr;
	EXPECT_EQ(decode_nsm_gpio_state_change_event(asMsg(b), b.size(), &ec,
						     &es, &p),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DevCapDiscOverreadGuard, Gpio_LargeCountShortBufferRejected)
{
	// num_gpio_events and data_size are internally consistent, but the
	// buffer is far too short: the msg_len guard must fire before the loop.
	std::vector<uint8_t> b(
	    sizeof(nsm_msg_hdr) + offsetof(nsm_event, data) + kGpioFixed, 0);
	setEventDataSize(b, static_cast<uint8_t>(kGpioFixed + 50 * 2));
	setNumGpioEvents(b, 50);
	uint8_t ec = 0;
	uint16_t es = 0;
	nsm_gpio_state_change_event_payload *p = nullptr;
	EXPECT_EQ(decode_nsm_gpio_state_change_event(asMsg(b), b.size(), &ec,
						     &es, &p),
		  NSM_SW_ERROR_LENGTH);
}

TEST(DevCapDiscOverreadGuard, Gpio_ExpectedSizeMismatchRejected)
{
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + offsetof(nsm_event, data) +
				   kGpioFixed + 2,
			       0);
	setEventDataSize(b, static_cast<uint8_t>(kGpioFixed)); // says 0 events
	setNumGpioEvents(b, 1);				       // but claims 1
	uint8_t ec = 0;
	uint16_t es = 0;
	nsm_gpio_state_change_event_payload *p = nullptr;
	EXPECT_EQ(decode_nsm_gpio_state_change_event(asMsg(b), b.size(), &ec,
						     &es, &p),
		  NSM_SW_ERROR_DATA);
}

TEST(DevCapDiscOverreadGuard, Gpio_HappyPath)
{
	std::vector<uint8_t> b(sizeof(nsm_msg_hdr) + offsetof(nsm_event, data) +
				   kGpioFixed + 2 * 2,
			       0);
	setEventDataSize(b, static_cast<uint8_t>(kGpioFixed + 2 * 2));
	setNumGpioEvents(b, 2);
	uint8_t ec = 0;
	uint16_t es = 0;
	nsm_gpio_state_change_event_payload *p = nullptr;
	EXPECT_EQ(decode_nsm_gpio_state_change_event(asMsg(b), b.size(), &ec,
						     &es, &p),
		  NSM_SUCCESS);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(p->num_gpio_events, 2);
}

// Glasswing re-scan regression: device-capabilities-v2 data_len must not
// underflow at the minimum accepted msg_len (short message rejected).
TEST(DevCapDiscOverreadGuard, GetDeviceCapabilitiesV2_ShortMsgUnderflowRejected)
{
	std::vector<uint8_t> b(8,
			       0); // hdr(5)+3; below hdr+sizeof(telemetry_resp)
	b[sizeof(nsm_msg_hdr) +
	  offsetof(nsm_common_telemetry_resp, telemetry_count)] = 2;
	uint8_t cc = 0, tg = 0;
	uint16_t rc = 0;
	uint32_t mibs = 0;
	EXPECT_EQ(decode_nsm_get_device_capabilities_v2_resp(
		      reinterpret_cast<const nsm_msg *>(b.data()), b.size(),
		      &cc, &rc, &tg, &mibs),
		  NSM_SW_ERROR_LENGTH);
}
