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

#include "common-tests.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "device-capability-discovery.h"

using ::testing::ElementsAre;

void testEncodeRequest(
    std::function<int(uint8_t, const uint8_t *, nsm_msg *)> function,
    uint8_t nvidiaMsgType, uint8_t command, uint8_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload)
{
	uint8_t instanceId = 0;
	Request requestMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
	requestMsg.insert(requestMsg.end(), expectedPayload,
			  expectedPayload + payloadSize);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	nsm_common_req *data =
	    reinterpret_cast<nsm_common_req *>(request->payload);

	// Bad tests
	auto rc = function(instanceId, nullptr, request);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(instanceId, expectedPayload, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(NSM_INSTANCE_MAX + 1, expectedPayload, request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// Good test
	rc = function(instanceId, expectedPayload, request);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(instanceId, request->hdr.instance_id);
	EXPECT_EQ(nvidiaMsgType, request->hdr.nvidia_msg_type);
	EXPECT_EQ(command, data->command);
	EXPECT_EQ(payloadSize, data->data_size);
	if (payload != nullptr) {
		memcpy(payload, request->payload + sizeof(nsm_common_req),
		       payloadSize);
	}
}

void testEncodeCommonRequest(std::function<int(uint8_t, nsm_msg *)> function,
			     uint8_t nvidiaMsgType, uint8_t command)
{
	uint8_t payload = 0;
	testEncodeRequest(
	    [function](uint8_t instanceId, const uint8_t *data, nsm_msg *msg) {
		    if (data == nullptr) {
			    // special case for function with one parameter less
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return function(instanceId, msg);
	    },
	    nvidiaMsgType, command, 0, &payload, nullptr);
}

void testDecodeRequest(
    std::function<int(nsm_msg *, uint16_t, uint8_t *)> function,
    uint8_t nvidiaMsgType, uint8_t command, uint8_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload)
{
	const uint8_t instanceId = 0;
	Request requestMsg{
	    0x10,
	    0xDE,			// PCI VID: NVIDIA 0x10DE
	    0x80 | (instanceId & 0x1f), // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			// OCP_TYPE=8, OCP_VER=9
	    nvidiaMsgType,		// NVIDIA_MSG_TYPE
	    command,			// command
	    payloadSize			// data size
	};
	requestMsg.insert(requestMsg.end(), expectedPayload,
			  expectedPayload + payloadSize);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto data = reinterpret_cast<nsm_common_req *>(request->payload);
	auto len = requestMsg.size();

	// Good test
	auto rc = function(request, len, payload);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	nsm_header_info header;
	rc = unpack_nsm_header(&request->hdr, &header);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(nvidiaMsgType, header.nvidia_msg_type);
	EXPECT_EQ(command, data->command);
	EXPECT_EQ(payloadSize, data->data_size);
	EXPECT_EQ(instanceId, header.instance_id);

	// Bad tests
	rc = function(nullptr, len, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(request, len, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(request, len - 1, payload);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
	requestMsg[0] = 0;
	rc = function(request, len, payload);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

void testDecodeCommonRequest(std::function<int(nsm_msg *, uint16_t)> function,
			     uint8_t nvidiaMsgType, uint8_t command)
{
	uint8_t payload = 0;
	testDecodeRequest(
	    [function](nsm_msg *msg, uint16_t len, uint8_t *data) {
		    if (data == nullptr) {
			    // special case for function with one parameter less
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return function(msg, len);
	    },
	    nvidiaMsgType, command, 0, &payload, &payload);
}

void testEncodeResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, const uint8_t *, nsm_msg *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command, size_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload)
{
	// Test common response
	const uint8_t instanceId = 0;
	Response responseMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
	responseMsg.insert(responseMsg.end(), expectedPayload,
			   expectedPayload + payloadSize);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto common = reinterpret_cast<nsm_common_resp *>(response->payload);

	// Good test
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	auto rc =
	    function(instanceId, cc, reasonCode, expectedPayload, response);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(instanceId, response->hdr.instance_id);
	EXPECT_EQ(nvidiaMsgType, response->hdr.nvidia_msg_type);
	EXPECT_EQ(command, common->command);
	EXPECT_EQ(cc, common->completion_code);
	EXPECT_EQ(payloadSize, le16toh(common->data_size));
	if (payload != nullptr) {
		memcpy(payload, response->payload + sizeof(nsm_common_resp),
		       payloadSize);
	}

	// Bad tests
	Response badResponseMsg(sizeof(nsm_msg_hdr) +
				sizeof(nsm_common_non_success_resp));
	auto badResponse = reinterpret_cast<nsm_msg *>(badResponseMsg.data());
	auto badData = reinterpret_cast<nsm_common_non_success_resp *>(
	    badResponse->payload);
	rc = function(instanceId, cc, reasonCode, nullptr, response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(instanceId, cc, reasonCode, expectedPayload, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(NSM_INSTANCE_MAX + 1, cc, reasonCode, expectedPayload,
		      response);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
	cc = NSM_ERROR;
	reasonCode = ERR_TIMEOUT;
	rc = function(instanceId, cc, reasonCode, expectedPayload, badResponse);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(command, badData->command);
	EXPECT_EQ(cc, badData->completion_code);
	EXPECT_EQ(reasonCode, badData->reason_code);
}

void testEncodeCommonResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, nsm_msg *)> function,
    uint8_t nvidiaMsgType, uint8_t command)
{
	const uint8_t payload = 0;
	testEncodeResponse(
	    [function](uint8_t instanceId, uint8_t cc, uint16_t reasonCode,
		       const uint8_t *data, nsm_msg *msg) {
		    if (data == nullptr) {
			    // special case for function with one parameter less
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return function(instanceId, cc, reasonCode, msg);
	    },
	    nvidiaMsgType, command, 0, &payload, nullptr);
}

void testDecodeResponse(std::function<int(const nsm_msg *, size_t, uint8_t *,
					  uint16_t *, uint8_t *)>
			    function,
			uint8_t nvidiaMsgType, uint8_t command,
			size_t payloadSize, const uint8_t *expectedPayload,
			uint8_t *payload)
{
	const uint8_t instanceId = 0;
	Response responseMsg{
	    0x10,
	    0xDE,		      // PCI VID: NVIDIA 0x10DE
	    (instanceId & 0x1f),      // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		      // OCP_TYPE=8, OCP_VER=9
	    nvidiaMsgType,	      // NVIDIA_MSG_TYPE
	    command,		      // command
	    NSM_SUCCESS,	      // completion code
	    0,			      // reserved
	    0,			      // reserved
	    uint8_t(payloadSize),     // data size
	    uint8_t(payloadSize >> 8) // data size
	};
	responseMsg.insert(responseMsg.end(), expectedPayload,
			   expectedPayload + payloadSize);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto len = responseMsg.size();

	// Good test
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	auto rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reasonCode);

	// Bad tests
	rc = function(nullptr, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len, nullptr, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len, &cc, nullptr, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len, &cc, &reasonCode, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len - 1, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);

	Response badResponseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    (instanceId & 0x1f), // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    nvidiaMsgType,	 // NVIDIA_MSG_TYPE
	    command,		 // command
	    NSM_ERROR,		 // completion code
	    ERR_TIMEOUT,	 // reason code
	    0,			 // reason code
	};
	response = reinterpret_cast<nsm_msg *>(badResponseMsg.data());
	len = badResponseMsg.size();
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(ERR_TIMEOUT, reasonCode);
}

void testDecodeCommonResponse(
    std::function<int(const nsm_msg *, size_t, uint8_t *, uint16_t *)> function,
    uint8_t nvidiaMsgType, uint8_t command)
{
	uint8_t payload = 0;
	testDecodeResponse(
	    [function](const nsm_msg *msg, size_t len, uint8_t *cc,
		       uint16_t *reasonCode, uint8_t *data) {
		    if (data == nullptr) {
			    // special case for function with one parameter less
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return function(msg, len, cc, reasonCode);
	    },
	    nvidiaMsgType, command, 0, &payload, &payload);
}

void testEncodeLongRunningResponse(
    std::function<int(uint8_t, uint8_t, uint16_t, const uint8_t *, nsm_msg *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command, size_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload)
{
	const uint8_t instanceId = 0;
	Response responseMsg(sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			     sizeof(struct nsm_long_running_resp));
	responseMsg.insert(responseMsg.end(), expectedPayload,
			   expectedPayload + payloadSize);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto event = reinterpret_cast<nsm_event *>(response->payload);
	auto longRunning =
	    reinterpret_cast<nsm_long_running_resp *>(event->data);

	// Good test
	uint16_t eventStateData;
	nsm_long_running_event_state eventState{
	    .nvidia_message_type = nvidiaMsgType, .command = command};
	memcpy(&eventStateData, &eventState, sizeof(uint16_t));
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	auto rc =
	    function(instanceId, cc, reasonCode, expectedPayload, response);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, response->hdr.request);
	EXPECT_EQ(1, response->hdr.datagram);
	EXPECT_EQ(instanceId, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
		  response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_LONG_RUNNING_EVENT, event->event_id);
	EXPECT_EQ(NSM_NVIDIA_GENERAL_EVENT_CLASS, event->event_class);
	EXPECT_EQ(eventStateData, event->event_state);
	EXPECT_EQ(cc, longRunning->completion_code);
	EXPECT_EQ(instanceId, longRunning->instance_id);
	EXPECT_EQ(payloadSize + sizeof(nsm_long_running_resp),
		  event->data_size);
	if (payload != nullptr) {
		memcpy(payload,
		       event->data + sizeof(struct nsm_long_running_resp),
		       payloadSize);
	}

	// Bad tests
	Response badResponseMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
				sizeof(nsm_long_running_non_success_resp));
	auto badResponse = reinterpret_cast<nsm_msg *>(badResponseMsg.data());
	auto badEvent = reinterpret_cast<nsm_event *>(badResponse->payload);
	auto badData = reinterpret_cast<nsm_long_running_non_success_resp *>(
	    badEvent->data);
	rc = function(instanceId, cc, reasonCode, nullptr, response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(instanceId, cc, reasonCode, expectedPayload, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(NSM_INSTANCE_MAX + 1, cc, reasonCode, expectedPayload,
		      response);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
	cc = NSM_ERROR;
	reasonCode = ERR_TIMEOUT;
	rc = function(instanceId, cc, reasonCode, expectedPayload, badResponse);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(cc, badData->completion_code);
	EXPECT_EQ(reasonCode, badData->reason_code);
}

void testDecodeLongRunningResponse(
    std::function<int(const nsm_msg *, size_t, uint8_t *, uint16_t *,
		      uint8_t *)>
	function,
    uint8_t nvidiaMsgType, uint8_t command, size_t payloadSize,
    const uint8_t *expectedPayload, uint8_t *payload)
{
	const uint8_t instanceId = 0;
	Response responseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    (instanceId & 0x1f), // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // NVIDIA_MSG_TYPE
	    0x08,			    // NSM_EVENT_VERSION=0, ACK=1, RES=0
	    NSM_LONG_RUNNING_EVENT,	    // EVENT_ID
	    NSM_NVIDIA_GENERAL_EVENT_CLASS, // EVENT_CLASS
	    nvidiaMsgType,		    // EVENT_STATE >> 8
	    command,			    // EVENT_STATE & 0xFF
	    uint8_t(payloadSize +
		    sizeof(struct nsm_long_running_resp)), // data size
	    instanceId,					   // instance id
	    0,						   // completion code
	    0,						   // reserved
	    0,						   // reserved
	};
	responseMsg.insert(responseMsg.end(), expectedPayload,
			   expectedPayload + payloadSize);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	auto len = responseMsg.size();

	// Good test
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	auto rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reasonCode);
	rc = function(response, len, &cc, &reasonCode, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	// Bad tests
	rc = function(nullptr, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len, nullptr, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len, &cc, nullptr, payload);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = function(response, len - 1, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);

	Response badResponseMsg{
	    0x10,
	    0xDE,		 // PCI VID: NVIDIA 0x10DE
	    (instanceId & 0x1f), // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,		 // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, // EVENT_MSG_TYPE
	    0x08,			    // NSM_EVENT_VERSION=0, ACK=1, RES=0
	    NSM_LONG_RUNNING_EVENT,	    // EVENT_ID
	    NSM_NVIDIA_GENERAL_EVENT_CLASS, // EVENT_CLASS
	    command,			    // Bad order EVENT_STATE >> 8
	    nvidiaMsgType,		    // Bad order EVENT_STATE & 0xFF
	    sizeof(struct nsm_long_running_non_success_resp), // data size
	    instanceId,					      // instance id
	    NSM_ERROR,					      // completion code
	    ERR_TIMEOUT, // reason_code >> 8
	    0,		 // reason_code & 0xFF
	};
	response = reinterpret_cast<nsm_msg *>(badResponseMsg.data());
	len = badResponseMsg.size();
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
	EXPECT_EQ(NSM_ERROR, cc);
	EXPECT_EQ(ERR_TIMEOUT, reasonCode);

	auto event = reinterpret_cast<nsm_event *>(response->payload);
	nsm_long_running_event_state badEventState;
	memcpy(&badEventState, &event->event_state, sizeof(uint16_t));
	EXPECT_EQ(command, badEventState.nvidia_message_type);
	EXPECT_EQ(nvidiaMsgType, badEventState.command);

	// Changing to good message
	badResponseMsg = responseMsg;
	response = reinterpret_cast<nsm_msg *>(badResponseMsg.data());
	event = reinterpret_cast<nsm_event *>(response->payload);
	auto eventState = reinterpret_cast<nsm_long_running_event_state *>(
	    &event->event_state);
	len = badResponseMsg.size();
	const auto offset = [response](uint8_t *data) {
		return (size_t)data - (size_t)response;
	};
	badResponseMsg[offset(&response->hdr.nvidia_msg_type)] =
	    NSM_TYPE_PLATFORM_ENVIRONMENTAL; // Injecting Bad EVENT_MSG_TYPE
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
	badResponseMsg = responseMsg; // Reverting good msg
	badResponseMsg[offset(&event->event_id)] =
	    0x00; // Injecting Bad EVENT_ID
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
	badResponseMsg = responseMsg; // Reverting good msg
	badResponseMsg[offset(&event->event_class)] =
	    1; // Injecting Bad EVENT_CLASS
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
	badResponseMsg = responseMsg; // Reverting good msg
	badResponseMsg[offset(&eventState->nvidia_message_type)] =
	    NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY; // Injecting Bad
						  // NVIDIA_MSG_TYPE
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
	badResponseMsg = responseMsg; // Reverting good msg
	badResponseMsg[offset(&eventState->command)] =
	    command + 1; // Injecting Bad command
	rc = function(response, len, &cc, &reasonCode, payload);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}