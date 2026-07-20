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
#include "device-configuration.h"
#include <cstring>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <types.hpp>
#include <vector>

TEST(setErrorInjectionMode, testRequest)
{
	const uint8_t mode = 1;
	nsm_set_error_injection_mode_v1_req req;
	auto encodeSetErrorInjectionModeV1Req =
	    [&mode](uint8_t instanceId, const uint8_t *data, nsm_msg *msg) {
		    if (data == nullptr) {
			    return (int)NSM_SW_ERROR_NULL;
		    }
		    return encode_set_error_injection_mode_v1_req(instanceId,
								  *data, msg);
	    };
	testEncodeRequest<uint8_t>(
	    encodeSetErrorInjectionModeV1Req, NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_SET_ERROR_INJECTION_MODE_V1, mode, req.mode);
	EXPECT_EQ(mode, req.mode);

	testDecodeRequest<uint8_t>(&decode_set_error_injection_mode_v1_req,
				   NSM_TYPE_DEVICE_CONFIGURATION,
				   NSM_SET_ERROR_INJECTION_MODE_V1, mode,
				   req.mode);
	EXPECT_EQ(mode, req.mode);
}
TEST(setErrorInjectionMode, testResponse)
{
	testEncodeCommonResponse(encode_set_error_injection_mode_v1_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_SET_ERROR_INJECTION_MODE_V1);

	testDecodeCommonResponse(&decode_set_error_injection_mode_v1_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_SET_ERROR_INJECTION_MODE_V1);
}
TEST(getErrorInjectionMode, testRequest)
{
	testEncodeCommonRequest(&encode_get_error_injection_mode_v1_req,
				NSM_TYPE_DEVICE_CONFIGURATION,
				NSM_GET_ERROR_INJECTION_MODE_V1);
	testDecodeCommonRequest(&decode_get_error_injection_mode_v1_req,
				NSM_TYPE_DEVICE_CONFIGURATION,
				NSM_GET_ERROR_INJECTION_MODE_V1);
}
TEST(getErrorInjectionMode, testResponse)
{
	const nsm_error_injection_mode_v1 data = {1, 1};
	nsm_get_error_injection_mode_v1_resp resp;
	testEncodeResponse<nsm_error_injection_mode_v1>(
	    &encode_get_error_injection_mode_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION, NSM_GET_ERROR_INJECTION_MODE_V1,
	    data, resp.data);
	EXPECT_EQ(data.mode, resp.data.mode);
	EXPECT_EQ(data.flags.byte, resp.data.flags.byte);

	testDecodeResponse<nsm_error_injection_mode_v1>(
	    &decode_get_error_injection_mode_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION, NSM_GET_ERROR_INJECTION_MODE_V1,
	    data, resp.data);
	EXPECT_EQ(data.mode, resp.data.mode);
	EXPECT_EQ(data.flags.byte, resp.data.flags.byte);
}

TEST(getSupportedErrorInjection, testRequest)
{
	testEncodeCommonRequest(
	    &encode_get_supported_error_injection_types_v1_req,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1);
	testDecodeCommonRequest(&decode_get_error_injection_mode_v1_req,
				NSM_TYPE_DEVICE_CONFIGURATION,
				NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1);
}
TEST(getSupportedErrorInjection, testResponse)
{
	const nsm_error_injection_types_mask data = {0xF, 0, 0, 0, 0, 0, 0, 0};
	nsm_get_error_injection_types_mask_resp resp;
	testEncodeResponse<nsm_error_injection_types_mask>(
	    &encode_get_supported_error_injection_types_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1, data, resp.data);
	for (size_t i = 0; i < 8; i++) {

		EXPECT_EQ(data.mask[i], resp.data.mask[i]);
	}

	testDecodeResponse<nsm_error_injection_types_mask>(
	    &decode_get_error_injection_types_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1, data, resp.data);
	for (size_t i = 0; i < 8; i++) {

		EXPECT_EQ(data.mask[i], resp.data.mask[i]);
	}
}

TEST(setCurrentErrorInjection, testRequest)
{

	const nsm_error_injection_types_mask data = {0xF, 0, 0, 0, 0, 0, 0, 0};
	nsm_set_error_injection_types_mask_req req;
	testEncodeRequest<nsm_error_injection_types_mask>(
	    &encode_set_current_error_injection_types_v1_req,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1, data, req.data);
	for (size_t i = 0; i < 8; i++) {

		EXPECT_EQ(data.mask[i], req.data.mask[i]);
	}

	testDecodeRequest<nsm_error_injection_types_mask>(
	    &decode_set_current_error_injection_types_v1_req,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1, data, req.data);
	for (size_t i = 0; i < 8; i++) {

		EXPECT_EQ(data.mask[i], req.data.mask[i]);
	}
}
TEST(setCurrentErrorInjection, testResponse)
{
	testEncodeCommonResponse(
	    &encode_set_current_error_injection_types_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1);
	testDecodeCommonResponse(&decode_set_error_injection_mode_v1_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1);
}
TEST(getCurrentErrorInjection, testRequest)
{
	testEncodeCommonRequest(
	    &encode_get_current_error_injection_types_v1_req,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1);
	testDecodeCommonRequest(&decode_get_error_injection_mode_v1_req,
				NSM_TYPE_DEVICE_CONFIGURATION,
				NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1);
}
TEST(getCurrentErrorInjection, testResponse)
{
	const nsm_error_injection_types_mask data = {0xF, 0, 0, 0, 0, 0, 0, 0};
	nsm_get_error_injection_types_mask_resp resp;
	testEncodeResponse<nsm_error_injection_types_mask>(
	    &encode_get_current_error_injection_types_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1, data, resp.data);
	for (size_t i = 0; i < 8; i++) {

		EXPECT_EQ(data.mask[i], resp.data.mask[i]);
	}

	testDecodeResponse<nsm_error_injection_types_mask>(
	    &decode_get_error_injection_types_v1_resp,
	    NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1, data, resp.data);
	for (size_t i = 0; i < 8; i++) {

		EXPECT_EQ(data.mask[i], resp.data.mask[i]);
	}
}

TEST(setErrorInjectionPayload, testEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_payload_req) +
	    sizeof(nsm_error_injection_leak_payload) + 10);

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	// Use leak detect payload with 0 sensors (minimum valid payload)
	nsm_error_injection_leak_payload leakPayload = {};
	leakPayload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	leakPayload.number_of_sensors = 0;
	// Size is struct without the flexible array member
	size_t dataSize =
	    sizeof(nsm_error_injection_leak_payload) - sizeof(uint16_t);
	uint16_t errorInjectionType = EI_DEVICE_ERRORS;
	uint16_t errorInjectionSubtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;

	// good test
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&leakPayload), dataSize,
	    errorInjectionType, errorInjectionSubtype, request);
	struct nsm_set_error_injection_payload_req *req =
	    reinterpret_cast<struct nsm_set_error_injection_payload_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_ERROR_INJECTION_PAYLOAD, req->hdr.command);
	EXPECT_EQ(errorInjectionType, le16toh(req->error_injection_type));

	// Bad tests
	auto rec = encode_set_error_injection_payload_req(
	    0, nullptr, dataSize, errorInjectionType, errorInjectionSubtype,
	    request);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rec);
	rec = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&leakPayload), dataSize,
	    errorInjectionType, errorInjectionSubtype, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rec);
	rec = encode_set_error_injection_payload_req(
	    NSM_INSTANCE_MAX + 1,
	    reinterpret_cast<const uint8_t *>(&leakPayload), dataSize,
	    errorInjectionType, errorInjectionSubtype, request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rec);
}

TEST(setErrorInjectionPayload, testDecodeRequest)
{
	// Use encode to create a valid request, then decode it
	const uint8_t instanceId = 0;
	nsm_error_injection_leak_payload leakPayload = {};
	leakPayload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	leakPayload.number_of_sensors = 0;
	const size_t payloadSize =
	    sizeof(nsm_error_injection_leak_payload) - sizeof(uint16_t);
	uint16_t errorInjectionType = EI_DEVICE_ERRORS;
	uint16_t errorInjectionSubtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_payload_req) +
	    payloadSize + 10);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	// Encode a valid request first
	auto rc = encode_set_error_injection_payload_req(
	    instanceId, reinterpret_cast<const uint8_t *>(&leakPayload),
	    payloadSize, errorInjectionType, errorInjectionSubtype, request);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	auto data = reinterpret_cast<nsm_set_error_injection_payload_req *>(
	    request->payload);

	// Fix hdr.data_size: decode expects it to include offset +
	// error_injection_type + fault_payload The formula in decode:
	// fault_payload_size = data_size - (sizeof(req) - 1) So we need:
	// data_size = fault_payload_size + sizeof(req) - 1 = payloadSize + 12
	uint16_t correctedDataSize =
	    payloadSize +
	    (sizeof(nsm_set_error_injection_payload_req) - sizeof(uint8_t));
	data->hdr.data_size = htole16(correctedDataSize);

	// Calculate actual message length
	auto len = sizeof(nsm_msg_hdr) +
		   sizeof(nsm_set_error_injection_payload_req) + payloadSize -
		   1; // -1 for fault_payload[1] already in struct

	// Good test - decode the encoded request
	uint16_t decodedType = 0;
	uint16_t decodedSubtype = 0;
	uint8_t decodedPayload[64] = {0};
	size_t decodedSize = 0;
	rc = decode_set_error_injection_payload_req(
	    request, len, &decodedType, &decodedSubtype, decodedPayload,
	    &decodedSize);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(EI_DEVICE_ERRORS, decodedType);
	EXPECT_EQ(EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, decodedSubtype);

	nsm_header_info header;
	rc = unpack_nsm_header(&request->hdr, &header);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, header.nvidia_msg_type);
	EXPECT_EQ(NSM_SET_ERROR_INJECTION_PAYLOAD, data->hdr.command);
	EXPECT_EQ(instanceId, header.instance_id);

	// Bad tests
	rc = decode_set_error_injection_payload_req(
	    nullptr, len, &decodedType, &decodedSubtype, decodedPayload,
	    &decodedSize);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = decode_set_error_injection_payload_req(
	    request, len, nullptr, &decodedSubtype, decodedPayload,
	    &decodedSize);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = decode_set_error_injection_payload_req(
	    request, len, &decodedType, nullptr, decodedPayload, &decodedSize);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	// Test with too short message
	rc = decode_set_error_injection_payload_req(
	    request,
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_payload_req) -
		1,
	    &decodedType, &decodedSubtype, decodedPayload, &decodedSize);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
	requestMsg[0] = 0;
	rc = decode_set_error_injection_payload_req(
	    request, len, &decodedType, &decodedSubtype, decodedPayload,
	    &decodedSize);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(setErrorInjectionPayload, testResponse)
{
	testEncodeCommonResponse(encode_set_error_injection_payload_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_SET_ERROR_INJECTION_PAYLOAD);

	testDecodeCommonResponse(&decode_set_error_injection_payload_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_SET_ERROR_INJECTION_PAYLOAD);
}

TEST(activateErrorInjectionPayload, testRequestResponse)
{
	uint8_t instanceId = 0;
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_activate_error_injection_payload_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	nsm_activate_error_injection_payload_req *data =
	    reinterpret_cast<nsm_activate_error_injection_payload_req *>(
		request->payload);

	// Bad tests
	uint16_t error_injection_type = EI_DEVICE_ERRORS;
	uint16_t error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	auto rc = encode_activate_error_injection_payload_req(
	    instanceId, error_injection_type, error_injection_subtype, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = encode_activate_error_injection_payload_req(
	    NSM_INSTANCE_MAX + 1, error_injection_type, error_injection_subtype,
	    request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// Good test
	rc = encode_activate_error_injection_payload_req(
	    instanceId, error_injection_type, error_injection_subtype, request);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(OCP_VERSION_V2, request->hdr.ocp_version);
	EXPECT_EQ(OCP_TYPE, request->hdr.ocp_type);
	EXPECT_EQ(instanceId, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_ACTIVATE_ERROR_INJECTION, data->hdr.command);
	EXPECT_EQ(sizeof(nsm_error_injection_id), le16toh(data->hdr.data_size));

	// Test decode request - decode the request we just encoded
	uint16_t decoded_type = 0, decoded_subtype = 0;
	rc = decode_activate_error_injection_payload_req(
	    request,
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_activate_error_injection_payload_req),
	    &decoded_type, &decoded_subtype);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(error_injection_type, decoded_type);
	EXPECT_EQ(error_injection_subtype, decoded_subtype);

	// Bad decode tests
	rc = decode_activate_error_injection_payload_req(
	    nullptr,
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_activate_error_injection_payload_req),
	    &decoded_type, &decoded_subtype);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = decode_activate_error_injection_payload_req(
	    request,
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_activate_error_injection_payload_req),
	    nullptr, &decoded_subtype);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);

	testEncodeCommonResponse(&encode_activate_error_injection_payload_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_ACTIVATE_ERROR_INJECTION);
	testDecodeCommonResponse(&decode_activate_error_injection_payload_resp,
				 NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_ACTIVATE_ERROR_INJECTION);
}

TEST(getErrorInjectionPayload, testEncodeRequest)
{
	const uint16_t error_injection_type = 0x04;
	const uint16_t error_injection_subtype = 0x00;
	uint8_t instanceId = 0;
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	nsm_get_error_injection_payload_req *data =
	    reinterpret_cast<nsm_get_error_injection_payload_req *>(
		request->payload);

	// Bad tests
	auto rc = encode_get_error_injection_payload_req(
	    instanceId, error_injection_type, error_injection_subtype, nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = encode_get_error_injection_payload_req(
	    NSM_INSTANCE_MAX + 1, error_injection_type, error_injection_subtype,
	    request);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);

	// Good test
	rc = encode_get_error_injection_payload_req(
	    instanceId, error_injection_type, error_injection_subtype, request);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(OCP_VERSION_V2, request->hdr.ocp_version);
	EXPECT_EQ(OCP_TYPE, request->hdr.ocp_type);
	EXPECT_EQ(instanceId, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_ERROR_INJECTION_PAYLOAD, data->hdr.command);
	EXPECT_EQ(sizeof(uint32_t), data->hdr.data_size);
	nsm_error_injection_id *error_injection_id_ptr =
	    reinterpret_cast<nsm_error_injection_id *>(
		&data->error_injection_id);
	EXPECT_EQ(error_injection_type,
		  error_injection_id_ptr->error_injection_type);
	EXPECT_EQ(error_injection_subtype,
		  error_injection_id_ptr->error_injection_subtype);
}

TEST(getErrorInjectionPayload, testDecodeRequest)
{
	const uint8_t instanceId = 0;
	Request requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80 | (instanceId & 0x1f),	     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x8A,			     // OCP_TYPE=8, OCP_VER=10
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_GET_ERROR_INJECTION_PAYLOAD, // command
	    0x00,			     // reserved1
	    sizeof(uint32_t),		     // data size LSB
	    0x00,			     // data size MSB
	    0x00,			     // reserved2
	    0x00,			     // reserved2
	};
	const uint16_t error_injection_type = 0x00;
	const uint16_t error_injection_subtype = 0x00;
	nsm_error_injection_id errorInjId = {error_injection_type,
					     error_injection_subtype};
	requestMsg.insert(
	    requestMsg.end(), reinterpret_cast<const uint8_t *>(&errorInjId),
	    reinterpret_cast<const uint8_t *>(&errorInjId) + sizeof(uint32_t));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto data = reinterpret_cast<nsm_get_error_injection_payload_req *>(
	    request->payload);
	auto len = requestMsg.size();

	// Good test
	uint16_t temp_type = 0;
	uint16_t temp_subtype = 0;
	auto rc = decode_get_error_injection_payload_req(
	    request, len, &temp_type, &temp_subtype);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	nsm_header_info header;
	rc = unpack_nsm_header(&request->hdr, &header);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, header.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_ERROR_INJECTION_PAYLOAD, data->hdr.command);
	EXPECT_EQ(sizeof(uint32_t), data->hdr.data_size);
	EXPECT_EQ(instanceId, header.instance_id);
	EXPECT_EQ(error_injection_type, temp_type);
	EXPECT_EQ(error_injection_subtype, temp_subtype);

	// Bad tests
	rc = decode_get_error_injection_payload_req(nullptr, len, &temp_type,
						    &temp_subtype);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = decode_get_error_injection_payload_req(request, len, nullptr,
						    &temp_subtype);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = decode_get_error_injection_payload_req(request, len, &temp_type,
						    nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = decode_get_error_injection_payload_req(request, len - 1,
						    &temp_type, &temp_subtype);
	EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
	requestMsg[0] = 0;
	rc = decode_get_error_injection_payload_req(request, len, &temp_type,
						    &temp_subtype);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

TEST(getErrorInjectionPayload, testResponse)
{
	// Test encode/decode response - just verify basic flow works
	const uint16_t errorInjectionType = EI_DEVICE_ERRORS;
	const uint16_t errorInjectionSubtype =
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;

	// Use the actual nsm_error_injection_leak_payload struct with 0 sensors
	nsm_error_injection_leak_payload leakPayload = {};
	leakPayload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	leakPayload.number_of_sensors = 0;
	// For 0 sensors, size is struct minus the flexible array member
	const size_t payloadSize =
	    sizeof(nsm_error_injection_leak_payload) - sizeof(uint16_t);

	// Calculate the exact message length for decode
	// msg_hdr + get_resp struct (minus fault_payload[1]) + actual payload
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(nsm_get_error_injection_payload_resp) -
			      sizeof(uint8_t) + payloadSize;

	// Test encode response
	std::vector<uint8_t> responseMsg(msgLen + 32);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_error_injection_payload_resp(
	    0, NSM_SUCCESS, ERR_NULL, errorInjectionType, errorInjectionSubtype,
	    reinterpret_cast<const uint8_t *>(&leakPayload), payloadSize,
	    response);
	EXPECT_EQ(NSM_SW_SUCCESS, rc);

	if (rc == NSM_SW_SUCCESS) {
		EXPECT_EQ(0, response->hdr.request);
		EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION,
			  response->hdr.nvidia_msg_type);

		auto resp =
		    reinterpret_cast<nsm_get_error_injection_payload_resp *>(
			response->payload);
		EXPECT_EQ(NSM_GET_ERROR_INJECTION_PAYLOAD, resp->hdr.command);

		// Test decode response - pass exact message length
		uint8_t cc = 0;
		uint16_t reasonCode = 0;
		uint8_t decodedPayload[64] = {0};
		size_t decodedSize = 0;
		rc = decode_get_error_injection_payload_resp(
		    response, msgLen, errorInjectionType, errorInjectionSubtype,
		    &cc, &reasonCode, decodedPayload, &decodedSize);
		EXPECT_EQ(NSM_SW_SUCCESS, rc);
		EXPECT_EQ(NSM_SUCCESS, cc);
	}

	// Bad tests
	rc = encode_get_error_injection_payload_resp(
	    0, NSM_SUCCESS, ERR_NULL, errorInjectionType, errorInjectionSubtype,
	    nullptr, payloadSize, response);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = encode_get_error_injection_payload_resp(
	    0, NSM_SUCCESS, ERR_NULL, errorInjectionType, errorInjectionSubtype,
	    reinterpret_cast<const uint8_t *>(&leakPayload), payloadSize,
	    nullptr);
	EXPECT_EQ(NSM_SW_ERROR_NULL, rc);
	rc = encode_get_error_injection_payload_resp(
	    NSM_INSTANCE_MAX + 1, NSM_SUCCESS, ERR_NULL, errorInjectionType,
	    errorInjectionSubtype,
	    reinterpret_cast<const uint8_t *>(&leakPayload), payloadSize,
	    response);
	EXPECT_EQ(NSM_SW_ERROR_DATA, rc);
}

void testGetFpgaDiagnosticSettingsEncodeRequest(
    fpga_diagnostics_settings_data_index dataIndex)
{

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_fpga_diagnostics_settings_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc =
	    encode_get_fpga_diagnostics_settings_req(0, dataIndex, request);

	struct nsm_get_fpga_diagnostics_settings_req *req =
	    (struct nsm_get_fpga_diagnostics_settings_req *)request->payload;

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(dataIndex, req->data_index);
}
void testGetFpgaDiagnosticSettingsEncodeResponse(
    fpga_diagnostics_settings_data_index expectedDataIndex)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    1,				       // data size
	    (uint8_t)expectedDataIndex	       // data_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	fpga_diagnostics_settings_data_index dataIndex;
	auto rc = decode_get_fpga_diagnostics_settings_req(request, msg_len,
							   &dataIndex);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(expectedDataIndex, dataIndex);
}

TEST(getFpgaDiagnosticsSettings, testRequests)
{
	for (auto di = (uint8_t)GET_WP_SETTINGS;
	     di <= (uint8_t)GET_GPU_POWER_STATUS; di++) {
		auto dataIndex = fpga_diagnostics_settings_data_index(di);
		testGetFpgaDiagnosticSettingsEncodeRequest(dataIndex);
		testGetFpgaDiagnosticSettingsEncodeResponse(dataIndex);
	}
	testGetFpgaDiagnosticSettingsEncodeRequest(GET_AGGREGATE_TELEMETRY);
	testGetFpgaDiagnosticSettingsEncodeResponse(GET_AGGREGATE_TELEMETRY);
}

TEST(getFpgaDiagnosticsSettingsWPSettings, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_fpga_diagnostics_settings_wp_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_fpga_diagnostics_settings_wp data = {};
	data.gpu1_4 = 1;
	struct nsm_fpga_diagnostics_settings_wp data_test = data;

	auto rc = encode_get_fpga_diagnostics_settings_wp_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_fpga_diagnostics_settings_wp_resp *resp =
	    reinterpret_cast<struct nsm_fpga_diagnostics_settings_wp_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_fpga_diagnostics_settings_wp),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.gpu1_4, data.gpu1_4);
}

TEST(getFpgaDiagnosticsSettingsWPSettings, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{0b10000000, 0x00, 0b00000100, 0x00,
				       0x00,	   0x00, 0x00,	     0x00};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    8,
	    0 // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp *>(
	    data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len, &cc, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(1, data->gpu1_4);
	EXPECT_EQ(1, data->retimer3);
}

TEST(getFpgaDiagnosticsSettingsWPSettings, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    7, // incorrect data size
	    0  // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp *>(
	    data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    NULL, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_resp(
	    response, msg_len, &cc, NULL, &reason_code, (uint8_t *)data,
	    sizeof(struct nsm_fpga_diagnostics_settings_wp));
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len - data_byte.size(), &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    response, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getFpgaDiagnosticsSettingsWPJumper, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	struct nsm_fpga_diagnostics_settings_wp_jumper data = {};
	data.presence = 1;
	struct nsm_fpga_diagnostics_settings_wp_jumper data_test = data;

	auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	struct nsm_fpga_diagnostics_settings_wp_jumper_resp *resp =
	    reinterpret_cast<
		struct nsm_fpga_diagnostics_settings_wp_jumper_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_fpga_diagnostics_settings_wp_jumper),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(data_test.presence, data.presence);
}

TEST(getFpgaDiagnosticsSettingsWPJumper, testGoodDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0 // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp_jumper *>(
	    data_byte.data());
	auto data_test = data;
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len, &cc, &reason_code, data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_test->presence, data->presence);
}

TEST(getFpgaDiagnosticsSettingsWPJumper, testBadDecodeResponse)
{
	std::vector<uint8_t> data_byte{
	    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0, // incorrect data size
	    0  // data size
	};
	auto data = reinterpret_cast<nsm_fpga_diagnostics_settings_wp_jumper *>(
	    data_byte.data());
	responseMsg.insert(responseMsg.end(), data_byte.begin(),
			   data_byte.end());
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    NULL, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len, NULL, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_resp(
	    response, msg_len, &cc, NULL, &reason_code, (uint8_t *)data,
	    sizeof(struct nsm_fpga_diagnostics_settings_wp_jumper));
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len - data_byte.size(), &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    response, msg_len, &cc, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getPowerSupplyStatus, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t status = 0x02;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_get_power_supply_status_resp(
	    0, NSM_SUCCESS, reasonCode, status, response);

	struct nsm_get_power_supply_status_resp *resp =
	    reinterpret_cast<struct nsm_get_power_supply_status_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(status, resp->power_supply_status);
}

TEST(getPowerSupplyStatus, testGoodDecodeResponse)
{
#define EXPECTED_POWER_SUPPLY_STATUS_LSB 0x02
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0,				      // data size
	    EXPECTED_POWER_SUPPLY_STATUS_LSB, // status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t status = 0;

	auto rc = decode_get_power_supply_status_resp(response, msg_len, &cc,
						      &reasonCode, &status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(status, EXPECTED_POWER_SUPPLY_STATUS_LSB);
}

TEST(getPowerSupplyStatus, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0,
	    0, // incorrect data size
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t status = 0;

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_power_supply_status_resp(NULL, msg_len, &cc,
						      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_supply_status_resp(response, msg_len, NULL,
						 &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_supply_status_resp(response, msg_len, &cc,
						 &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_power_supply_status_resp(response, msg_len - 1, &cc,
						 &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_power_supply_status_resp(response, msg_len, &cc,
						 &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getGpusPresence, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_gpu_presence_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t presence = 0b00111001;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_get_gpu_presence_resp(0, NSM_SUCCESS, reasonCode,
					       presence, response);

	struct nsm_get_gpu_presence_resp *resp =
	    reinterpret_cast<struct nsm_get_gpu_presence_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(presence, resp->presence);
}

TEST(getGpusPresence, testGoodDecodeResponse)
{
#define EXPECTED_PRESENCE_LSB 0b00111001
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0,			   // data size
	    EXPECTED_PRESENCE_LSB, // status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t presence = 0;

	auto rc = decode_get_gpu_presence_resp(response, msg_len, &cc,
					       &reasonCode, &presence);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(presence, EXPECTED_PRESENCE_LSB);
}

TEST(getGpusPresence, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0,
	    0, // incorrect data size
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t presence = 0;

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_gpu_presence_resp(NULL, msg_len, &cc, &reason_code,
					       &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_presence_resp(response, msg_len, NULL, &reason_code,
					  &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_presence_resp(response, msg_len, &cc, &reason_code,
					  NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_presence_resp(response, msg_len - 1, &cc,
					  &reason_code, &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_gpu_presence_resp(response, msg_len, &cc, &reason_code,
					  &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getGpusPowerStatus, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_get_gpu_power_status_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t status = 0x02;
	uint16_t reasonCode = ERR_NULL;

	auto rc = encode_get_gpu_power_status_resp(0, NSM_SUCCESS, reasonCode,
						   status, response);

	struct nsm_get_gpu_power_status_resp *resp =
	    reinterpret_cast<struct nsm_get_gpu_power_status_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->hdr.data_size));
	EXPECT_EQ(status, resp->power_status);
}

TEST(getGpusPowerStatus, testGoodDecodeResponse)
{
#define EXPECTED_STATUS_LSB 0b11001011
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0,			 // data size
	    EXPECTED_STATUS_LSB, // status
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	uint8_t status = 0;

	auto rc = decode_get_gpu_power_status_resp(response, msg_len, &cc,
						   &reasonCode, &status);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(status, EXPECTED_STATUS_LSB);
}

TEST(getGpusPowerStatus, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0,
	    0, // incorrect data size
	    0};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t status = 0;

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_gpu_power_status_resp(NULL, msg_len, &cc,
						   &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_power_status_resp(response, msg_len, NULL,
					      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_power_status_resp(response, msg_len, &cc,
					      &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_power_status_resp(response, msg_len - 1, &cc,
					      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_gpu_power_status_resp(response, msg_len, &cc,
					      &reason_code, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getFpgaDiagnosticsSettingsGpuIstMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_ist_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	uint8_t data = 0b01111001;
	uint8_t data_test = data;

	auto rc = encode_get_gpu_ist_mode_resp(0, NSM_SUCCESS, reason_code,
					       data, response);

	auto resp = reinterpret_cast<nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->command);
	EXPECT_EQ(sizeof(uint8_t), le16toh(resp->data_size));
	EXPECT_EQ(data_test, data);
}

TEST(getFpgaDiagnosticsSettingsGpuIstMode, testGoodDecodeResponse)
{
	uint8_t data = 0x01;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    1,
	    0, // data size
	    data,
	};
	auto data_test = data;
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_gpu_ist_mode_resp(response, msg_len, &cc,
					       &reason_code, &data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(data_test, data);
}

TEST(getFpgaDiagnosticsSettingsGpuIstMode, testBadDecodeResponse)
{
	uint8_t data = 0;
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, // command
	    0,				       // completion code
	    0,
	    0,
	    0, // incorrect data size
	    0, // data size
	    data,
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	auto rc = decode_get_gpu_ist_mode_resp(NULL, msg_len, &cc, &reason_code,
					       &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len, NULL, &reason_code,
					  &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len, &cc, &reason_code,
					  NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len - 1, &cc,
					  &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_gpu_ist_mode_resp(response, msg_len, &cc, &reason_code,
					  &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(enableDisableGpuIstMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_index = ALL_GPUS_DEVICE_INDEX;
	uint8_t value = 0;

	auto rc = encode_enable_disable_gpu_ist_mode_req(0, device_index, value,
							 request);

	auto req = reinterpret_cast<nsm_enable_disable_gpu_ist_mode_req *>(
	    request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ENABLE_DISABLE_GPU_IST_MODE, req->hdr.command);
	EXPECT_EQ(2, req->hdr.data_size);
	EXPECT_EQ(device_index, req->device_index);
	EXPECT_EQ(value, req->value);
}

TEST(enableDisableGpuIstMode, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x80,			     // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
	    2,				     // data size
	    0,				     // device_index
	    1,				     // set
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	uint8_t device_index;
	uint8_t value;
	auto rc = decode_enable_disable_gpu_ist_mode_req(request, msg_len,
							 &device_index, &value);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, device_index);
	EXPECT_EQ(1, value);
}

TEST(enableDisableGpuIstMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_enable_disable_gpu_ist_mode_resp(
	    0, NSM_SUCCESS, reason_code, response);

	auto resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_ENABLE_DISABLE_GPU_IST_MODE, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(enableDisableGpuIstMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
	    0,				     // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len,
							  &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(enableDisableGpuIstMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_ENABLE_DISABLE_GPU_IST_MODE, // command
	    0,				     // completion code
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

	auto rc = decode_enable_disable_gpu_ist_mode_resp(NULL, msg_len, &cc,
							  &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len, NULL,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len, &cc,
						     NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len, &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_enable_disable_gpu_ist_mode_resp(response, msg_len - 1, &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

void testGetReconfigurationPermissionsV1EncodeRequest(
    reconfiguration_permissions_v1_index settingIndex)
{

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_get_reconfiguration_permissions_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_reconfiguration_permissions_v1_req(0, settingIndex,
								request);

	auto req =
	    reinterpret_cast<nsm_get_reconfiguration_permissions_v1_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_RECONFIGURATION_PERMISSIONS_V1, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(settingIndex, req->setting_index);
}
void testGetReconfigurationPermissionsV1EncodeResponse(
    reconfiguration_permissions_v1_index expectedSettingIndex)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_RECONFIGURATION_PERMISSIONS_V1, // command
	    1,					    // data size
	    (uint8_t)expectedSettingIndex	    // data_index
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	size_t msg_len = requestMsg.size();

	reconfiguration_permissions_v1_index settingIndex;
	auto rc = decode_get_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(expectedSettingIndex, settingIndex);
}

TEST(getReconfigurationPermissionsV1, testRequests)
{
	for (auto di = (uint8_t)RP_IN_SYSTEM_TEST;
	     di <= (uint8_t)RP_RUNTIME_IN_SYSTEM_TEST; di++) {
		auto settingIndex = reconfiguration_permissions_v1_index(di);
		testGetReconfigurationPermissionsV1EncodeRequest(settingIndex);
		testGetReconfigurationPermissionsV1EncodeResponse(settingIndex);
	}
}

TEST(getReconfigurationPermissionsV1, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_reconfiguration_permissions_v1_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	nsm_reconfiguration_permissions_v1 data = {};
	data.host_persistent = 1;

	auto rc = encode_get_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, reason_code, &data, response);

	auto resp =
	    reinterpret_cast<nsm_get_reconfiguration_permissions_v1_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_RECONFIGURATION_PERMISSIONS_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(nsm_reconfiguration_permissions_v1),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(1, data.host_persistent);
}

TEST(getReconfigurationPermissionsV1, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    1,
	    0,		// data size
	    0b00000110, // data
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	nsm_reconfiguration_permissions_v1 data;
	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code, &data);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data.host_oneshot);
	EXPECT_EQ(1, data.host_persistent);
	EXPECT_EQ(1, data.host_flr_persistent);
}

TEST(getReconfigurationPermissionsV1, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    0, // incorrect data size
	    0  // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;

	nsm_reconfiguration_permissions_v1 data;
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    NULL, msg_len, &cc, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, NULL, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(response, msg_len,
							    &cc, NULL, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len - 1, &cc, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

void testSetReconfigurationPermissionsV1EncodeRequest(
    reconfiguration_permissions_v1_index settingIndex,
    reconfiguration_permissions_v1_setting configuration, uint8_t permission)
{
	Request requestMsg(sizeof(nsm_msg_hdr) +
			   sizeof(nsm_set_reconfiguration_permissions_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_set_reconfiguration_permissions_v1_req(
	    0, settingIndex, configuration, permission, request);

	auto req =
	    reinterpret_cast<nsm_set_reconfiguration_permissions_v1_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_RECONFIGURATION_PERMISSIONS_V1, req->hdr.command);
	EXPECT_EQ(3, req->hdr.data_size);
	EXPECT_EQ(settingIndex, req->setting_index);
	EXPECT_EQ(configuration, req->configuration);
	EXPECT_EQ(permission, req->permission);
}

TEST(setReconfigurationPermissionsV1, testGoodEncodeRequest)
{
	for (auto si = 0; si <= int(RP_RUNTIME_IN_SYSTEM_TEST); si++) {
		for (auto ci = 0; ci <= int(RP_ONESHOT_FLR); ci++) {
			auto settingIndex =
			    reconfiguration_permissions_v1_index(si);
			auto configuration =
			    reconfiguration_permissions_v1_setting(ci);
			testSetReconfigurationPermissionsV1EncodeRequest(
			    settingIndex, configuration, 1);
			testSetReconfigurationPermissionsV1EncodeRequest(
			    settingIndex, configuration, 0);
		}
	}
}

TEST(setReconfigurationPermissionsV1, testGoodDecodeRequest)
{
	Request requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    3,					    // data size
	    3,					    // settingIndex
	    1,					    // configuration
	    1,					    // set
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto msg_len = requestMsg.size();

	auto settingIndex = RP_IN_SYSTEM_TEST;
	auto configuration = RP_ONESHOOT_HOT_RESET;
	uint8_t permission = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, &configuration, &permission);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(RP_BAR0_FIREWALL, settingIndex);
	EXPECT_EQ(RP_PERSISTENT, configuration);
	EXPECT_EQ(1, permission);
}

TEST(setReconfigurationPermissionsV1, testBadDecodeRequest)
{
	Request requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // incorect data size
	};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto msg_len = requestMsg.size();

	auto settingIndex = RP_IN_SYSTEM_TEST;
	auto configuration = RP_ONESHOOT_HOT_RESET;
	uint8_t permission = 0;

	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    NULL, msg_len, &settingIndex, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, NULL, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, NULL, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, &configuration, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len, &settingIndex, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_set_reconfiguration_permissions_v1_req(
	    request, msg_len - 1, &settingIndex, &configuration, &permission);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(setReconfigurationPermissionsV1, testGoodEncodeResponse)
{
	Response responseMsg(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_set_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, reason_code, response);

	auto resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_RECONFIGURATION_PERMISSIONS_V1, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setReconfigurationPermissionsV1, testGoodDecodeResponse)
{
	Response responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
	    0,
	    0,
	    0,
	    0 // data size
	};
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	auto rc = decode_set_reconfiguration_permissions_v1_resp(
	    response, msg_len, &cc, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(setReconfigurationPermissionsV1, testBadDecodeResponse)
{
	Response responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_RECONFIGURATION_PERMISSIONS_V1, // command
	    0,					    // completion code
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

	auto rc = decode_set_reconfiguration_permissions_v1_resp(
	    NULL, msg_len, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_resp(response, msg_len,
							    NULL, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_resp(response, msg_len,
							    &cc, NULL);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_reconfiguration_permissions_v1_resp(response, msg_len,
							    &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
	rc = decode_set_reconfiguration_permissions_v1_resp(
	    response, msg_len - 1, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getConfidentialComputeMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) +
		sizeof(struct nsm_get_confidential_compute_mode_v1_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t current_mode = 2;
	uint8_t pending_mode = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_confidential_compute_mode_v1_resp(
	    0, NSM_SUCCESS, reason_code, current_mode, pending_mode, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_confidential_compute_mode_v1_resp *resp =
	    reinterpret_cast<
		struct nsm_get_confidential_compute_mode_v1_resp *>(
		response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1, resp->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_get_confidential_compute_mode_v1_resp) -
		      sizeof(struct nsm_common_resp),
		  le16toh(resp->hdr.data_size));

	EXPECT_EQ(resp->current_mode, 2);
	EXPECT_EQ(resp->pending_mode, 1);
}

TEST(getConfidentialComputeMode, testEncodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x17;
	uint8_t current_mode = 3;
	uint8_t pending_mode = 2;

	// Test error response with reason code
	int rc = encode_get_confidential_compute_mode_v1_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0xBEEF, current_mode,
	    pending_mode, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DEVICE_CONFIGURATION);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0xBEEF);
}

TEST(getConfidentialComputeMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1, // command
	    0,					  // completion code
	    0,					  // reserved
	    0,					  // reserved
	    2,
	    0, // data size
	    1, // current_mode
	    0  // pending mode
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t current_mode;
	uint8_t pending_mode;

	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &current_mode,
	    &pending_mode);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(2, data_size);
	EXPECT_EQ(1, current_mode);
	EXPECT_EQ(0, pending_mode);
}

TEST(getConfidentialComputeMode, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1, // command
	    0,					  // completion code
	    0,					  // reserved
	    0,					  // reserved
	    3,
	    0, // wrong data size
	    2, // current data
	    1  // pending data
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	uint8_t current_mode;
	uint8_t pending_mode;

	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    NULL, msg_len, &cc, &data_size, &reason_code, &current_mode,
	    &pending_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_confidential_compute_mode_v1_resp(
	    response, msg_len, NULL, &data_size, &reason_code, &current_mode,
	    &pending_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_confidential_compute_mode_v1_resp(
	    response, msg_len, &cc, NULL, &reason_code, &current_mode,
	    &pending_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_confidential_compute_mode_v1_resp(
	    response, msg_len - 1, &cc, &data_size, &reason_code, &current_mode,
	    &pending_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_confidential_compute_mode_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code, &current_mode,
	    &pending_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(setConfidentialComputeMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_set_confidential_compute_mode_v1_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t mode = 1;
	auto rc = encode_set_confidential_compute_mode_v1_req(0, mode, request);
	struct nsm_set_confidential_compute_mode_v1_req *req =
	    reinterpret_cast<struct nsm_set_confidential_compute_mode_v1_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(mode, req->mode);
}

TEST(setConfidentialComputeMode, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1, // command
	    1,					  // data size
	    1					  // mode
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t mode;
	auto rc = decode_set_confidential_compute_mode_v1_req(request, msg_len,
							      &mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(mode, 1);
}

TEST(setConfidentialComputeMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_set_confidential_compute_mode_v1_resp(
	    0, NSM_SUCCESS, reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setConfidentialComputeMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1, // command
	    0,					  // completion code
	    0,					  // reserved
	    0,					  // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_confidential_compute_mode_v1_resp(
	    response, msg_len, &cc, &data_size, &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(setEGMMode, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					sizeof(nsm_set_EGM_mode_req));

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint8_t mode = 1;
	auto rc = encode_set_EGM_mode_req(0, mode, request);
	struct nsm_set_EGM_mode_req *req =
	    reinterpret_cast<struct nsm_set_EGM_mode_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_EGM_MODE, req->hdr.command);
	EXPECT_EQ(1, req->hdr.data_size);
	EXPECT_EQ(mode, req->requested_mode);
}

TEST(setEGMMode, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x80,			   // RQ=1, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_EGM_MODE,		   // command
	    1,				   // data size
	    1				   // mode
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t mode;
	auto rc = decode_set_EGM_mode_req(request, msg_len, &mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(mode, 1);
}

TEST(setEGMMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc =
	    encode_set_EGM_mode_resp(0, NSM_SUCCESS, reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_EGM_MODE, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setEGMMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_EGM_MODE,		   // command
	    0,				   // completion code
	    0,				   // reserved
	    0,				   // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;

	auto rc = decode_set_EGM_mode_resp(response, msg_len, &cc, &data_size,
					   &reason_code);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(0, data_size);
}

TEST(getEGMMode, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_get_EGM_mode_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	bitfield8_t flags;
	flags.byte = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_EGM_mode_resp(0, NSM_SUCCESS, reason_code, &flags,
					   response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_get_EGM_mode_resp *resp =
	    reinterpret_cast<struct nsm_get_EGM_mode_resp *>(response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_GET_EGM_MODE, resp->hdr.command);
	EXPECT_EQ(sizeof(struct nsm_get_EGM_mode_resp) -
		      sizeof(struct nsm_common_resp),
		  le16toh(resp->hdr.data_size));
	EXPECT_EQ(1, resp->flags.byte);
}

TEST(getEGMMode, testEncodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x18;
	bitfield8_t flags;
	flags.byte = 3;

	// Test error response with reason code
	int rc = encode_get_EGM_mode_resp(instance_id, NSM_ERR_INVALID_DATA,
					  0xCAFE, &flags, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DEVICE_CONFIGURATION);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_EGM_MODE);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0xCAFE);
}

TEST(getEGMMode, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_EGM_MODE,		   // command
	    0,				   // completion code
	    0,				   // reserved
	    0,				   // reserved
	    1,
	    0, // data size
	    1  // current mode
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	uint16_t data_size = 0;
	bitfield8_t flags;

	auto rc = decode_get_EGM_mode_resp(response, msg_len, &cc, &data_size,
					   &reason_code, &flags);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(1, data_size);
	EXPECT_EQ(1, flags.byte);
}

TEST(getDeviceModeSettings, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req), 0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_device_mode_setting_req(0, 0, request);

	struct nsm_get_device_mode_setting_req *req =
	    reinterpret_cast<struct nsm_get_device_mode_setting_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_MODE_SETTING, req->hdr.command);
	EXPECT_EQ(sizeof(uint8_t), req->hdr.data_size);
	EXPECT_EQ(0, req->device_mode_index);
}

TEST(getDeviceModeSettings, testBadEncodeRequest)
{
	auto rc = encode_get_device_mode_setting_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDeviceModeSettings, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{0x10,
					0xDE,
					0x80,
					0x89,
					NSM_TYPE_DEVICE_CONFIGURATION,
					NSM_GET_DEVICE_MODE_SETTING,
					1,
					0};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t device_mode_index = 0;

	auto rc = decode_get_device_mode_setting_req(request, msg_len,
						     &device_mode_index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(device_mode_index, 0);
}

TEST(getDeviceModeSettings, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{0x10,
					0xDE,
					0x80,
					0x89,
					NSM_TYPE_DEVICE_CONFIGURATION,
					NSM_GET_DEVICE_MODE_SETTING,
					0,
					0};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_mode_index = 0;
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req);

	auto rc =
	    decode_get_device_mode_setting_req(nullptr, 0, &device_mode_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_setting_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_setting_req(request, msg_len - 1,
						&device_mode_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_device_mode_setting_req(request, msg_len,
						&device_mode_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getDeviceModeSettings, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint8_t device_mode = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_device_mode_settings_resp(
	    0, NSM_SUCCESS, reason_code, device_mode, response);

	struct nsm_get_device_mode_setting_resp *resp =
	    reinterpret_cast<struct nsm_get_device_mode_setting_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(response->hdr.nvidia_msg_type, NSM_TYPE_DEVICE_CONFIGURATION);
	EXPECT_EQ(resp->hdr.command, NSM_GET_DEVICE_MODE_SETTING);
	EXPECT_EQ(resp->device_mode, device_mode);
}

TEST(getDeviceModeSettings, testBadEncodeResponse)
{
	uint8_t device_mode = 1;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_device_mode_settings_resp(
	    0, NSM_SUCCESS, reason_code, device_mode, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDeviceModeSettings, testEncodeResponseErrorCompletionCode)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto *msg = reinterpret_cast<struct nsm_msg *>(responseMsg.data());
	uint8_t instance_id = 0x19;
	uint8_t device_mode = 5;

	// Test error response with reason code
	int rc = encode_get_device_mode_settings_resp(
	    instance_id, NSM_ERR_INVALID_DATA, 0xFACE, device_mode, msg);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Verify header fields
	EXPECT_EQ(msg->hdr.request, 0); // Response message
	EXPECT_EQ(msg->hdr.datagram, 0);
	EXPECT_EQ(msg->hdr.instance_id, instance_id);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DEVICE_CONFIGURATION);

	// Error responses use nsm_common_non_success_resp structure
	auto *resp =
	    reinterpret_cast<nsm_common_non_success_resp *>(msg->payload);
	EXPECT_EQ(resp->command, NSM_GET_DEVICE_MODE_SETTING);
	EXPECT_EQ(resp->completion_code, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(le16toh(resp->reason_code), 0xFACE);
}

TEST(getDeviceModeSettings, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_MODE_SETTING,   // command
	    0,				   // completion code
	    0,				   // reserved
	    0,				   // reserved
	    1,
	    0, // data size
	    1  // Device mode
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	enum nsm_l1_prediction_mode_config device_mode =
	    nsm_l1_prediction_mode_config::DISABLED;

	auto rc = decode_get_device_mode_setting_resp(
	    response, msg_len, &cc, &reason_code, &device_mode);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(device_mode, nsm_l1_prediction_mode_config::ENABLED);
}

TEST(getDeviceModeSettings, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_GET_DEVICE_MODE_SETTING,   // command
	    0,				   // completion code
	    0,				   // reserved
	    0,				   // reserved
	    1,
	    0, // data size
	    0  // Device mode
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp);

	uint8_t cc = NSM_SUCCESS;
	uint16_t reason_code = ERR_NULL;
	enum nsm_l1_prediction_mode_config device_mode =
	    nsm_l1_prediction_mode_config::DISABLED;

	auto rc = decode_get_device_mode_setting_resp(
	    nullptr, 0, &cc, &reason_code, &device_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_setting_resp(response, 0, nullptr,
						 &reason_code, &device_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_setting_resp(response, msg_len - 1, &cc,
						 &reason_code, &device_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(setDeviceModeSettings, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_setting_req), 0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	uint8_t device_mode_index = 0;
	enum nsm_l1_prediction_mode_config device_mode =
	    nsm_l1_prediction_mode_config::ENABLED;

	auto rc = encode_set_device_mode_setting_req(0, device_mode_index,
						     device_mode, request);
	struct nsm_set_device_mode_setting_req *req =
	    reinterpret_cast<struct nsm_set_device_mode_setting_req *>(
		request->payload);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_SET_DEVICE_MODE_SETTING, req->hdr.command);
	EXPECT_EQ(sizeof(uint16_t), req->hdr.data_size);
	EXPECT_EQ(0, req->device_mode_index);
	EXPECT_EQ(nsm_l1_prediction_mode_config::ENABLED, req->device_mode);
}

TEST(setDeviceModeSettings, testBadEncodeRequest)
{
	auto rc = encode_set_device_mode_setting_req(
	    0, 0, nsm_l1_prediction_mode_config::ENABLED, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setDeviceModeSettings, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_MODE_SETTING,   // command
	    2,				   // data size
	    0,				   // device mode index
	    1,				   // device mode
	};

	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msg_len = requestMsg.size();
	uint8_t device_mode_index = 0;
	enum nsm_l1_prediction_mode_config device_mode =
	    nsm_l1_prediction_mode_config::DISABLED;

	auto rc = decode_set_device_mode_settings_req(
	    request, msg_len, &device_mode_index, &device_mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(device_mode_index, 0);
	EXPECT_EQ(device_mode, nsm_l1_prediction_mode_config::ENABLED);
}

TEST(setDeviceModeSettings, testBadDecodeRequest)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_MODE_SETTING,   // command
	    2,				   // data size
	    0,				   // device mode index
	    0,				   // device mode
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t device_mode_index = 0;
	enum nsm_l1_prediction_mode_config device_mode =
	    nsm_l1_prediction_mode_config::DISABLED;
	size_t msg_len = responseMsg.size();

	auto rc = decode_set_device_mode_settings_req(
	    nullptr, 0, &device_mode_index, &device_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_req(response, 0, nullptr,
						 &device_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_req(
	    response, msg_len - 1, &device_mode_index, &device_mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(setDeviceModeSettings, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_set_device_mode_settings_resp(0, NSM_SUCCESS,
						       reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);

	EXPECT_EQ(NSM_SET_DEVICE_MODE_SETTING, resp->command);
}

TEST(setDeviceModeSettings, testBadEncodeResponse)
{
	auto rc = encode_set_device_mode_settings_resp(0, NSM_SUCCESS, ERR_NULL,
						       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setDeviceModeSettings, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_MODE_SETTING,   // command
	    0,				   // completion code
	    0,				   // reserved
	    0,				   // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	auto rc = decode_set_device_mode_setting_resp(response, msg_len, &cc,
						      &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
}

TEST(setDeviceModeSettings, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			   // PCI VID: NVIDIA 0x10DE
	    0x00,			   // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			   // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION, // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_MODE_SETTING,   // command
	    1,				   // completion code
	    0,				   // reserved
	    0,				   // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	size_t msg_len = responseMsg.size();

	auto rc =
	    decode_set_device_mode_setting_resp(nullptr, 0, &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_setting_resp(response, 0, nullptr,
						 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_setting_resp(response, msg_len - 1, &cc,
						 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_set_device_mode_setting_resp(response, msg_len, &cc,
						 &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getDeviceModeSettingsV2, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint32_t device_mode_index = DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT;
	auto rc = encode_get_device_mode_settings_v2_req(0, device_mode_index,
							 request);

	struct nsm_get_device_mode_settings_v2_req *req =
	    reinterpret_cast<struct nsm_get_device_mode_settings_v2_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_MODE_SETTINGS_V2, req->hdr.command);
	EXPECT_EQ(sizeof(req->device_mode_index), req->hdr.data_size);
	EXPECT_EQ(device_mode_index, le32toh(req->device_mode_index));
}

TEST(getDeviceModeSettingsV2, testBadEncodeRequest)
{
	auto rc = encode_get_device_mode_settings_v2_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDeviceModeSettingsV2, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint32_t device_mode_index =
	    DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT;
	encode_get_device_mode_settings_v2_req(0, device_mode_index, request);

	uint32_t decoded_index = 0;
	auto rc = decode_get_device_mode_settings_v2_req(
	    request, requestMsg.size(), &decoded_index);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(device_mode_index, decoded_index);
}

TEST(getDeviceModeSettingsV2, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint32_t device_mode_index = 0;
	size_t msg_len = requestMsg.size();

	auto rc = decode_get_device_mode_settings_v2_req(nullptr, 0,
							 &device_mode_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_settings_v2_req(request, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_settings_v2_req(request, msg_len - 1,
						    &device_mode_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	struct nsm_get_device_mode_settings_v2_req *req =
	    reinterpret_cast<struct nsm_get_device_mode_settings_v2_req *>(
		request->payload);
	req->hdr.data_size = 0;
	rc = decode_get_device_mode_settings_v2_req(request, msg_len,
						    &device_mode_index);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getDeviceModeSettingsV2, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
		sizeof(nsm_get_device_mode_settings_v2_req::device_mode_index) *
		    2,
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t current_power_limit = 500000;
	uint32_t pending_power_limit = 600000;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code,
	    reinterpret_cast<uint8_t *>(&current_power_limit),
	    sizeof(current_power_limit),
	    reinterpret_cast<uint8_t *>(&pending_power_limit),
	    sizeof(pending_power_limit), response);

	struct nsm_get_device_mode_settings_v2_resp *resp =
	    reinterpret_cast<struct nsm_get_device_mode_settings_v2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_DEVICE_MODE_SETTINGS_V2, resp->hdr.command);
	EXPECT_EQ(sizeof(current_power_limit),
		  le16toh(resp->current_mode_length));
	EXPECT_EQ(sizeof(pending_power_limit),
		  le16toh(resp->pending_mode_length));
}

TEST(getDeviceModeSettingsV2, testGoodEncodeResponseNoPending)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
		sizeof(nsm_get_device_mode_settings_v2_req::device_mode_index),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t current_power_limit = 500000;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code,
	    reinterpret_cast<uint8_t *>(&current_power_limit),
	    sizeof(current_power_limit), nullptr, 0, response);

	struct nsm_get_device_mode_settings_v2_resp *resp =
	    reinterpret_cast<struct nsm_get_device_mode_settings_v2_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(sizeof(current_power_limit),
		  le16toh(resp->current_mode_length));
	EXPECT_EQ(0, le16toh(resp->pending_mode_length));
}

TEST(getDeviceModeSettingsV2, testBadEncodeResponse)
{
	uint32_t current_power_limit = 500000;
	uint32_t pending_power_limit = 0;
	uint16_t reason_code = ERR_NULL;

	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code,
	    reinterpret_cast<uint8_t *>(&current_power_limit),
	    sizeof(current_power_limit), nullptr, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
		sizeof(nsm_get_device_mode_settings_v2_req::device_mode_index),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code, nullptr, sizeof(current_power_limit),
	    nullptr, 0, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code,
	    reinterpret_cast<uint8_t *>(&current_power_limit),
	    sizeof(current_power_limit), nullptr, sizeof(pending_power_limit),
	    response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getDeviceModeSettingsV2, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
		sizeof(nsm_get_device_mode_settings_v2_req::device_mode_index) *
		    2,
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint32_t current_power_limit = 500000;
	uint32_t pending_power_limit = 600000;
	uint16_t reason_code = ERR_NULL;

	encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code,
	    reinterpret_cast<uint8_t *>(&current_power_limit),
	    sizeof(current_power_limit),
	    reinterpret_cast<uint8_t *>(&pending_power_limit),
	    sizeof(pending_power_limit), response);

	uint8_t cc = NSM_ERROR;
	uint16_t decoded_reason_code = 0;
	uint8_t current_mode_data[sizeof(current_power_limit)] = {0};
	uint16_t current_mode_length = 0;
	uint8_t pending_mode_data[sizeof(pending_power_limit)] = {0};
	uint16_t pending_mode_length = 0;

	auto rc = decode_get_device_mode_settings_v2_resp(
	    response, responseMsg.size(), &cc, &decoded_reason_code,
	    current_mode_data, &current_mode_length, pending_mode_data,
	    &pending_mode_length);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(sizeof(current_power_limit), current_mode_length);
	EXPECT_EQ(sizeof(pending_power_limit), pending_mode_length);

	uint32_t decoded_current = 0;
	uint32_t decoded_pending = 0;
	memcpy(&decoded_current, current_mode_data, sizeof(decoded_current));
	memcpy(&decoded_pending, pending_mode_data, sizeof(decoded_pending));
	EXPECT_EQ(current_power_limit, decoded_current);
	EXPECT_EQ(pending_power_limit, decoded_pending);
}

TEST(getDeviceModeSettingsV2, testBadDecodeResponse)
{
	uint32_t current_power_limit = 500000;

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) +
		sizeof(current_power_limit),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, ERR_NULL,
	    reinterpret_cast<uint8_t *>(&current_power_limit),
	    sizeof(current_power_limit), nullptr, 0, response);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t current_mode_data[sizeof(current_power_limit)] = {0};
	uint16_t current_mode_length = 0;
	uint8_t pending_mode_data[sizeof(current_power_limit)] = {0};
	uint16_t pending_mode_length = 0;
	size_t msg_len = responseMsg.size();

	auto rc = decode_get_device_mode_settings_v2_resp(
	    nullptr, msg_len, &cc, &reason_code, current_mode_data,
	    &current_mode_length, pending_mode_data, &pending_mode_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_settings_v2_resp(
	    response, msg_len, nullptr, &reason_code, current_mode_data,
	    &current_mode_length, pending_mode_data, &pending_mode_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_settings_v2_resp(
	    response, msg_len, &cc, nullptr, current_mode_data,
	    &current_mode_length, pending_mode_data, &pending_mode_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_settings_v2_resp(
	    response, msg_len, &cc, &reason_code, current_mode_data, nullptr,
	    pending_mode_data, &pending_mode_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_mode_settings_v2_resp(
	    response, msg_len, &cc, &reason_code, current_mode_data,
	    &current_mode_length, pending_mode_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	struct nsm_get_device_mode_settings_v2_resp *resp =
	    reinterpret_cast<struct nsm_get_device_mode_settings_v2_resp *>(
		response->payload);
	rc = decode_get_device_mode_settings_v2_resp(
	    response,
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp) -
		sizeof(resp->mode_data) - 1,
	    &cc, &reason_code, current_mode_data, &current_mode_length,
	    pending_mode_data, &pending_mode_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(setDeviceModeSettingsV2, testGoodEncodeRequest)
{
	uint32_t device_mode_index = DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT;
	uint32_t power_limit = 500000;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		sizeof(power_limit),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_set_device_mode_settings_v2_req(
	    0, device_mode_index, reinterpret_cast<uint8_t *>(&power_limit),
	    sizeof(power_limit), request);

	struct nsm_set_device_mode_settings_v2_req *req =
	    reinterpret_cast<struct nsm_set_device_mode_settings_v2_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_SET_DEVICE_MODE_SETTINGS_V2, req->hdr.command);
	EXPECT_EQ(sizeof(req->device_mode_index) + sizeof(power_limit),
		  req->hdr.data_size);
	EXPECT_EQ(device_mode_index, le32toh(req->device_mode_index));
}

TEST(setDeviceModeSettingsV2, testBadEncodeRequest)
{
	uint32_t power_limit = 500000;

	auto rc = encode_set_device_mode_settings_v2_req(
	    0, 0, reinterpret_cast<uint8_t *>(&power_limit),
	    sizeof(power_limit), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		sizeof(power_limit),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	rc = encode_set_device_mode_settings_v2_req(
	    0, 0, nullptr, sizeof(power_limit), request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setDeviceModeSettingsV2, testGoodDecodeRequest)
{
	uint32_t device_mode_index =
	    DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY;
	uint32_t power_limit = 750000;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		sizeof(power_limit),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	encode_set_device_mode_settings_v2_req(
	    0, device_mode_index, reinterpret_cast<uint8_t *>(&power_limit),
	    sizeof(power_limit), request);

	uint32_t decoded_index = 0;
	uint8_t decoded_data[sizeof(power_limit)] = {0};
	uint16_t decoded_data_length = 0;

	auto rc = decode_set_device_mode_settings_v2_req(
	    request, requestMsg.size(), &decoded_index, decoded_data,
	    &decoded_data_length);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(device_mode_index, decoded_index);
	EXPECT_EQ(sizeof(power_limit), decoded_data_length);

	uint32_t decoded_power_limit = 0;
	memcpy(&decoded_power_limit, decoded_data, sizeof(decoded_power_limit));
	EXPECT_EQ(power_limit, decoded_power_limit);
}

TEST(setDeviceModeSettingsV2, testBadDecodeRequest)
{
	struct nsm_set_device_mode_settings_v2_req dummy_req;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		sizeof(dummy_req.device_mode_index),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint32_t device_mode_index = 0;
	uint8_t device_mode_data[sizeof(dummy_req.device_mode_index)] = {0};
	uint16_t device_mode_data_length = 0;
	size_t msg_len = requestMsg.size();

	auto rc = decode_set_device_mode_settings_v2_req(
	    nullptr, msg_len, &device_mode_index, device_mode_data,
	    &device_mode_data_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_v2_req(request, msg_len, nullptr,
						    device_mode_data,
						    &device_mode_data_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_v2_req(
	    request, msg_len, &device_mode_index, device_mode_data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	struct nsm_set_device_mode_settings_v2_req *req =
	    reinterpret_cast<struct nsm_set_device_mode_settings_v2_req *>(
		request->payload);
	rc = decode_set_device_mode_settings_v2_req(
	    request,
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) -
		sizeof(req->device_mode_data) - 1,
	    &device_mode_index, device_mode_data, &device_mode_data_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	req->hdr.data_size = sizeof(req->device_mode_index) - 1;
	rc = decode_set_device_mode_settings_v2_req(
	    request, msg_len, &device_mode_index, device_mode_data,
	    &device_mode_data_length);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(setDeviceModeSettingsV2, testGoodEncodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(struct nsm_common_resp), 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	uint16_t reason_code = ERR_NULL;

	auto rc = encode_set_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, reason_code, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	struct nsm_common_resp *resp =
	    reinterpret_cast<struct nsm_common_resp *>(response->payload);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(0, response->hdr.datagram);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_SET_DEVICE_MODE_SETTINGS_V2, resp->command);
	EXPECT_EQ(0, le16toh(resp->data_size));
}

TEST(setDeviceModeSettingsV2, testBadEncodeResponse)
{
	auto rc = encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS,
							  ERR_NULL, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(setDeviceModeSettingsV2, testGoodDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_MODE_SETTINGS_V2, // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    0,
	    0 // data size
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();
	uint8_t cc = 0;
	uint16_t reason_code = 0;

	auto rc = decode_set_device_mode_settings_v2_resp(response, msg_len,
							  &cc, &reason_code);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reason_code, ERR_NULL);
}

TEST(setDeviceModeSettingsV2, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg{
	    0x10,
	    0xDE,			     // PCI VID: NVIDIA 0x10DE
	    0x00,			     // RQ=0, D=0, RSVD=0, INSTANCE_ID=0
	    0x89,			     // OCP_TYPE=8, OCP_VER=9
	    NSM_TYPE_DEVICE_CONFIGURATION,   // NVIDIA_MSG_TYPE
	    NSM_SET_DEVICE_MODE_SETTINGS_V2, // command
	    0,				     // completion code
	    0,				     // reserved
	    0,				     // reserved
	    1,				     // incorrect data size
	    0,				     // data size MSB
	    0				     // invalid data byte
	};

	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	size_t msg_len = responseMsg.size();

	auto rc = decode_set_device_mode_settings_v2_resp(nullptr, msg_len, &cc,
							  &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_v2_resp(response, msg_len, nullptr,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_v2_resp(response, msg_len, &cc,
						     nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_mode_settings_v2_resp(response, msg_len - 1, &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_set_device_mode_settings_v2_resp(response, msg_len, &cc,
						     &reason_code);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(deviceModeSettingsV2, testAllDeviceModeIndices)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	std::vector<device_mode_index> indices = {
	    DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY,
	    DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY};

	for (auto idx : indices) {
		auto rc = encode_get_device_mode_settings_v2_req(
		    0, static_cast<uint32_t>(idx), request);
		EXPECT_EQ(rc, NSM_SW_SUCCESS);

		uint32_t decoded_index = 0;
		rc = decode_get_device_mode_settings_v2_req(
		    request, requestMsg.size(), &decoded_index);
		EXPECT_EQ(rc, NSM_SW_SUCCESS);
		EXPECT_EQ(static_cast<uint32_t>(idx), decoded_index);
	}
}

TEST(deviceConfigV2, setEncodeDecodeRoundTrip)
{
	const uint32_t cfgType = 0x12345678;
	const std::vector<uint8_t> data = {0x01, 0x02, 0xab, 0xcd};
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					    sizeof(nsm_common_req_v2) +
					    sizeof(uint32_t) + data.size(),
					0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_set_device_config_v2_req(
	    1, cfgType, data.data(), static_cast<uint16_t>(data.size()),
	    request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint32_t outType = 0;
	std::vector<uint8_t> outBuf(data.size());
	uint16_t outLen = 0;
	rc = decode_set_device_config_v2_req(request, requestMsg.size(),
					     &outType, outBuf.data(), &outLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(outType, cfgType);
	EXPECT_EQ(outLen, data.size());
	EXPECT_EQ(memcmp(outBuf.data(), data.data(), data.size()), 0);
}

TEST(deviceConfigV2, getEncodeDecodeRoundTrip)
{
	const uint32_t cfgType = 0xdeadbeef;
	const std::vector<uint8_t> query = {0x11, 0x22};
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					    sizeof(nsm_common_req_v2) +
					    sizeof(uint32_t) + query.size(),
					0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_get_device_config_v2_req(
	    2, cfgType, query.data(), static_cast<uint16_t>(query.size()),
	    request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint32_t outType = 0;
	uint8_t outQuery[64];
	uint16_t outQLen = 0;
	rc = decode_get_device_config_v2_req(request, requestMsg.size(),
					     &outType, outQuery, &outQLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(outType, cfgType);
	EXPECT_EQ(outQLen, query.size());
	EXPECT_EQ(memcmp(outQuery, query.data(), query.size()), 0);
}

TEST(deviceConfigV2, getResponseEncodeDecode)
{
	const uint8_t cur[] = {0xaa, 0xbb};
	const uint8_t pend[] = {0xcc};
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_config_v2_resp) - 1 +
		sizeof(cur) + sizeof(pend),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	int rc = encode_get_device_config_v2_resp(0, NSM_SUCCESS, ERR_NULL, cur,
						  sizeof(cur), pend,
						  sizeof(pend), response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0xffff;
	uint8_t curOut[16];
	uint16_t curLen = 0;
	uint8_t pendOut[16];
	uint16_t pendLen = 0;
	rc = decode_get_device_config_v2_resp(response, responseMsg.size(), &cc,
					      &reason, curOut, &curLen, pendOut,
					      &pendLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(curLen, sizeof(cur));
	EXPECT_EQ(pendLen, sizeof(pend));
	EXPECT_EQ(memcmp(curOut, cur, curLen), 0);
	EXPECT_EQ(memcmp(pendOut, pend, pendLen), 0);
}

TEST(deviceConfigV2, setEncodeErrors)
{
	const uint32_t cfgType = 1;
	uint8_t data[] = {0x01};
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					    sizeof(nsm_common_req_v2) +
					    sizeof(uint32_t) + sizeof(data),
					0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  encode_set_device_config_v2_req(0, cfgType, data,
						  sizeof(data), nullptr));

	EXPECT_EQ(NSM_SW_ERROR_NULL, encode_set_device_config_v2_req(
					 0, cfgType, nullptr, 1, request));
}

TEST(deviceConfigV2, setDecodeErrors)
{
	const uint32_t cfgType = 0x11223344;
	const std::vector<uint8_t> data = {0x55, 0x66};
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					    sizeof(nsm_common_req_v2) +
					    sizeof(uint32_t) + data.size(),
					0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_set_device_config_v2_req(
	    0, cfgType, data.data(), static_cast<uint16_t>(data.size()),
	    request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint32_t outType = 0;
	uint8_t outBuf[8];
	uint16_t outLen = 0;

	rc = decode_set_device_config_v2_req(nullptr, requestMsg.size(),
					     &outType, outBuf, &outLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_config_v2_req(request, requestMsg.size(),
					     nullptr, outBuf, &outLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_config_v2_req(request, requestMsg.size(),
					     &outType, outBuf, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_set_device_config_v2_req(request, requestMsg.size() - 1,
					     &outType, outBuf, &outLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	auto *req =
	    reinterpret_cast<nsm_set_device_config_v2_req *>(request->payload);
	uint8_t savedCmd = req->hdr.command;
	req->hdr.command = 0xff;
	rc = decode_set_device_config_v2_req(request, requestMsg.size(),
					     &outType, outBuf, &outLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	req->hdr.command = savedCmd;

	rc = decode_set_device_config_v2_req(request, requestMsg.size(),
					     &outType, nullptr, &outLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(deviceConfigV2, getEncodeErrors)
{
	const uint32_t cfgType = 2;
	uint8_t q[] = {0xab};

	EXPECT_EQ(NSM_SW_ERROR_NULL, encode_get_device_config_v2_req(
					 0, cfgType, q, sizeof(q), nullptr));
}

TEST(deviceConfigV2, getDecodeErrors)
{
	const uint32_t cfgType = 3;
	const std::vector<uint8_t> query = {0x01};
	std::vector<uint8_t> requestMsg(sizeof(nsm_msg_hdr) +
					    sizeof(nsm_common_req_v2) +
					    sizeof(uint32_t) + query.size(),
					0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	int rc = encode_get_device_config_v2_req(
	    0, cfgType, query.data(), static_cast<uint16_t>(query.size()),
	    request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint32_t outType = 0;
	uint8_t outQ[8];
	uint16_t outQLen = 0;

	rc = decode_get_device_config_v2_req(nullptr, requestMsg.size(),
					     &outType, outQ, &outQLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_req(request, requestMsg.size(),
					     nullptr, outQ, &outQLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_req(request, requestMsg.size(),
					     &outType, outQ, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_req(request, requestMsg.size() - 1,
					     &outType, outQ, &outQLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	auto *req =
	    reinterpret_cast<nsm_get_device_config_v2_req *>(request->payload);
	uint8_t savedCmd = req->hdr.command;
	req->hdr.command = 0xfe;
	rc = decode_get_device_config_v2_req(request, requestMsg.size(),
					     &outType, outQ, &outQLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
	req->hdr.command = savedCmd;

	rc = decode_get_device_config_v2_req(request, requestMsg.size(),
					     &outType, nullptr, &outQLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(deviceConfigV2, getResponseEncodeErrors)
{
	const uint8_t cur[] = {0x01};
	const uint8_t pend[] = {0x02};

	EXPECT_EQ(NSM_SW_ERROR_NULL,
		  encode_get_device_config_v2_resp(0, NSM_SUCCESS, ERR_NULL,
						   cur, sizeof(cur), pend,
						   sizeof(pend), nullptr));
}

TEST(deviceConfigV2, getResponseDecodeErrors)
{
	const uint8_t cur[] = {0x11};
	const uint8_t pend[] = {0x22};
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_config_v2_resp) - 1 +
		sizeof(cur) + sizeof(pend),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	int rc = encode_get_device_config_v2_resp(0, NSM_SUCCESS, ERR_NULL, cur,
						  sizeof(cur), pend,
						  sizeof(pend), response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = NSM_ERROR;
	uint16_t reason = 0;
	uint8_t curOut[8];
	uint16_t curLen = 0;
	uint8_t pendOut[8];
	uint16_t pendLen = 0;

	rc = decode_get_device_config_v2_resp(nullptr, responseMsg.size(), &cc,
					      &reason, curOut, &curLen, pendOut,
					      &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_resp(response, responseMsg.size(),
					      nullptr, &reason, curOut, &curLen,
					      pendOut, &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_resp(response, responseMsg.size(), &cc,
					      nullptr, curOut, &curLen, pendOut,
					      &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_resp(response, responseMsg.size() - 1,
					      &cc, &reason, curOut, &curLen,
					      pendOut, &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	rc = decode_get_device_config_v2_resp(response, responseMsg.size(), &cc,
					      &reason, nullptr, &curLen,
					      pendOut, &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_device_config_v2_resp(response, responseMsg.size(), &cc,
					      &reason, curOut, &curLen, pendOut,
					      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> bad = responseMsg;
	auto badMsg = reinterpret_cast<nsm_msg *>(bad.data());
	auto *resp =
	    reinterpret_cast<nsm_get_device_config_v2_resp *>(badMsg->payload);
	resp->current_config_length = htole16(200);
	resp->pending_config_length = htole16(200);
	resp->hdr.data_size =
	    htole16(sizeof(resp->current_config_length) +
		    sizeof(resp->pending_config_length) + 200 + 200);
	rc = decode_get_device_config_v2_resp(badMsg, bad.size(), &cc, &reason,
					      curOut, &curLen, pendOut,
					      &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(deviceConfigV2, deviceConfigurationRequestEventV1EncodeDecode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
	auto msg = reinterpret_cast<nsm_msg *>(buf.data());
	int rc = encode_nsm_device_config_request_event_v1(7, true, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DEVICE_CONFIGURATION);

	uint8_t evClass = 0xff;
	uint16_t evState = 0xffff;
	rc = decode_nsm_device_config_request_event_v1(msg, buf.size(),
						       &evClass, &evState);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(evClass, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(evState, 0u);

	std::vector<uint8_t> wrongType(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN,
				       0);
	auto wmsg = reinterpret_cast<nsm_msg *>(wrongType.data());
	rc = encode_nsm_event(1, NSM_TYPE_PLATFORM_ENVIRONMENTAL, false,
			      NSM_EVENT_VERSION,
			      NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1,
			      NSM_GENERAL_EVENT_CLASS, 0, 0, NULL, wmsg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	rc = decode_nsm_device_config_request_event_v1(wmsg, wrongType.size(),
						       &evClass, &evState);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);

	uint8_t extra = 0x55;
	std::vector<uint8_t> withData(
	    sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 1, 0);
	auto dmsg = reinterpret_cast<nsm_msg *>(withData.data());
	rc = encode_nsm_event(1, NSM_TYPE_DEVICE_CONFIGURATION, false,
			      NSM_EVENT_VERSION,
			      NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1,
			      NSM_GENERAL_EVENT_CLASS, 0, 1, &extra, dmsg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	rc = decode_nsm_device_config_request_event_v1(dmsg, withData.size(),
						       &evClass, &evState);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(getSupportedDeviceModesV2, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_supported_device_modes_req(0, request);

	struct nsm_common_req *req =
	    reinterpret_cast<struct nsm_common_req *>(request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, request->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_SUPPORTED_DEVICE_MODES_V2, req->command);
	EXPECT_EQ(0, req->data_size);
}

TEST(getSupportedDeviceModesV2, testBadEncodeRequest)
{
	auto rc = encode_get_supported_device_modes_req(0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getSupportedDeviceModesV2, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	encode_get_supported_device_modes_req(0, request);

	auto rc =
	    decode_get_supported_device_modes_req(request, requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(getSupportedDeviceModesV2, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	encode_get_supported_device_modes_req(0, request);

	auto rc =
	    decode_get_supported_device_modes_req(nullptr, requestMsg.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_device_modes_req(request,
						   requestMsg.size() - 1);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(getSupportedDeviceModesV2, testGoodEncodeResponse)
{
	const uint16_t handle = 0x0001;
	const uint16_t mode_count = 3;
	const uint32_t mode_list[] = {
	    DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE};

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_modes_resp) +
		(mode_count - 1) * sizeof(uint32_t),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_get_supported_device_modes_resp(
	    0, NSM_SUCCESS, ERR_NULL, handle, mode_count, mode_list, response);

	struct nsm_get_supported_device_modes_resp *resp =
	    reinterpret_cast<struct nsm_get_supported_device_modes_resp *>(
		response->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(NSM_TYPE_DEVICE_CONFIGURATION, response->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_GET_SUPPORTED_DEVICE_MODES_V2, resp->hdr.command);
	EXPECT_EQ(handle, le16toh(resp->handle));
	EXPECT_EQ(mode_count, le16toh(resp->mode_count));
	uint16_t expected_data_size =
	    sizeof(resp->handle) + sizeof(resp->mode_count) +
	    mode_count * sizeof(resp->supported_mode_list[0]);
	EXPECT_EQ(expected_data_size, le16toh(resp->hdr.data_size));
	for (uint16_t i = 0; i < mode_count; i++) {
		EXPECT_EQ(mode_list[i], le32toh(resp->supported_mode_list[i]));
	}
}

TEST(getSupportedDeviceModesV2, testBadEncodeResponse)
{
	const uint32_t mode_list[] = {
	    DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT};

	auto rc = encode_get_supported_device_modes_resp(
	    0, NSM_SUCCESS, ERR_NULL, 0, 1, mode_list, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_modes_resp),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	rc = encode_get_supported_device_modes_resp(0, NSM_SUCCESS, ERR_NULL, 0,
						    1, nullptr, response);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(getSupportedDeviceModesV2, testGoodDecodeResponse)
{
	const uint16_t handle = 0x0001;
	const uint16_t mode_count = 3;
	const uint32_t mode_list[] = {
	    DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE};

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_modes_resp) +
		(mode_count - 1) * sizeof(uint32_t),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	encode_get_supported_device_modes_resp(0, NSM_SUCCESS, ERR_NULL, handle,
					       mode_count, mode_list, response);

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0;
	uint16_t decoded_handle = 0;
	uint16_t decoded_mode_count = 0;
	uint32_t decoded_mode_list[mode_count] = {};

	auto rc = decode_get_supported_device_modes_resp(
	    response, responseMsg.size(), &cc, &reason_code, &decoded_handle,
	    &decoded_mode_count, decoded_mode_list);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(ERR_NULL, reason_code);
	EXPECT_EQ(handle, decoded_handle);
	EXPECT_EQ(mode_count, decoded_mode_count);
	for (uint16_t i = 0; i < mode_count; i++) {
		EXPECT_EQ(mode_list[i], decoded_mode_list[i]);
	}
}

TEST(getSupportedDeviceModesV2, testBadDecodeResponse)
{
	const uint16_t handle = 0x0001;
	const uint16_t mode_count = 2;
	const uint32_t mode_list[] = {
	    DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
	    DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT};

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_modes_resp) +
		(mode_count - 1) * sizeof(uint32_t),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	size_t msg_len = responseMsg.size();

	encode_get_supported_device_modes_resp(0, NSM_SUCCESS, ERR_NULL, handle,
					       mode_count, mode_list, response);

	uint8_t cc = NSM_ERROR;
	uint16_t reason_code = 0;
	uint16_t decoded_handle = 0;
	uint16_t decoded_mode_count = 0;
	uint32_t decoded_mode_list[mode_count] = {};

	auto rc = decode_get_supported_device_modes_resp(
	    nullptr, msg_len, &cc, &reason_code, &decoded_handle,
	    &decoded_mode_count, decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_device_modes_resp(
	    response, msg_len, nullptr, &reason_code, &decoded_handle,
	    &decoded_mode_count, decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_device_modes_resp(
	    response, msg_len, &cc, nullptr, &decoded_handle,
	    &decoded_mode_count, decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_device_modes_resp(
	    response, msg_len, &cc, &reason_code, nullptr, &decoded_mode_count,
	    decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	rc = decode_get_supported_device_modes_resp(
	    response, msg_len, &cc, &reason_code, &decoded_handle, nullptr,
	    decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

	size_t short_len = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
			   sizeof(uint16_t) + sizeof(uint16_t) - 1;
	rc = decode_get_supported_device_modes_resp(
	    response, short_len, &cc, &reason_code, &decoded_handle,
	    &decoded_mode_count, decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);

	struct nsm_get_supported_device_modes_resp *resp =
	    reinterpret_cast<struct nsm_get_supported_device_modes_resp *>(
		response->payload);
	resp->hdr.data_size = htole16(0);
	rc = decode_get_supported_device_modes_resp(
	    response, msg_len, &cc, &reason_code, &decoded_handle,
	    &decoded_mode_count, decoded_mode_list);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ---------------------------------------------------------------------------
// Buffer-overflow guard regression (nvbug 6232725): each decoder must reject
// a message whose on-wire payload length exceeds the bytes actually present
// in msg_len, returning NSM_SW_ERROR_LENGTH instead of an out-of-bounds copy.
// Power-capping Mode (DEVICE_MODE_POWER_CAPPING / POWER_CAPPING_MODE_DATA_SIZE)
// uses these same v2 codecs.
// ---------------------------------------------------------------------------

namespace
{
// Payload offset of the data_size field inside struct nsm_common_resp
// (command:1, completion_code:1, reserved:2, data_size:2).
constexpr size_t kRespDataSizeOff = sizeof(nsm_msg_hdr) + 4;

void putU16(std::vector<uint8_t> &buf, size_t off, uint16_t v)
{
	buf[off] = static_cast<uint8_t>(v & 0xFF);
	buf[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

// Success-CC response buffer (completion_code byte stays 0 == NSM_SUCCESS)
// sized to the struct, with hdr.data_size set far larger than the bytes the
// buffer actually carries.
std::vector<uint8_t> oversizedResp(size_t structSize, uint16_t dataSize)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + structSize, 0);
	putU16(buf, kRespDataSizeOff, dataSize);
	return buf;
}

const nsm_msg *asMsg(const std::vector<uint8_t> &buf)
{
	return reinterpret_cast<const nsm_msg *>(buf.data());
}
} // namespace

TEST(LibnsmOverreadGuard, FpgaDiagnosticsSettingsResp)
{
	auto buf = oversizedResp(sizeof(nsm_get_fpga_diagnostics_settings_resp),
				 0xFFFF);
	uint8_t cc = 0xFF;
	uint16_t ds = 0;
	uint16_t rc = 0;
	uint8_t dst[8] = {0};
	EXPECT_EQ(decode_get_fpga_diagnostics_settings_resp(
		      asMsg(buf), buf.size(), &cc, &ds, &rc, dst, sizeof(dst)),
		  NSM_SW_ERROR_LENGTH);
}

TEST(LibnsmOverreadGuard, GetDeviceModeSettingsV2Resp)
{
	// current_mode_length=1000, pending=0; hdr.data_size kept
	// consistent (2 + 2 + 1000) so the internal consistency check
	// passes and the new length-vs-msg_len guard is what fires.
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_resp),
	    0);
	putU16(buf, kRespDataSizeOff, 1004);
	putU16(buf, sizeof(nsm_msg_hdr) + 6, 1000); // current_mode_length
	putU16(buf, sizeof(nsm_msg_hdr) + 8, 0);    // pending_mode_length
	uint8_t cc = 0xFF;
	uint16_t rc = 0;
	uint8_t cur[8] = {0};
	uint8_t pend[8] = {0};
	uint16_t curLen = 0;
	uint16_t pendLen = 0;
	EXPECT_EQ(
	    decode_get_device_mode_settings_v2_resp(
		asMsg(buf), buf.size(), &cc, &rc, cur, &curLen, pend, &pendLen),
	    NSM_SW_ERROR_LENGTH);
}

TEST(LibnsmOverreadGuard, SetDeviceModeSettingsV2Req)
{
	// nsm_common_req: command:1, data_size:1. Oversized data_size at
	// payload offset 1. Exercises the same encode/decode path used by
	// DEVICE_MODE_POWER_CAPPING (POWER_CAPPING_MODE_DATA_SIZE).
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) -
		1,
	    0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // hdr.data_size = 255
	uint32_t idx = 0;
	uint8_t data[8] = {0};
	uint16_t dataLen = 0;
	EXPECT_EQ(decode_set_device_mode_settings_v2_req(asMsg(buf), buf.size(),
							 &idx, data, &dataLen),
		  NSM_SW_ERROR_LENGTH);
}

TEST(lldpModeBitfield, wireLayout)
{
	EXPECT_EQ(sizeof(nsm_lldp_mode_bitfield), 1U);

	auto toByte = [](const nsm_lldp_mode_bitfield &view) {
		uint8_t byte = 0;
		memcpy(&byte, &view, sizeof(byte));
		return byte;
	};

	auto fromByte = [](uint8_t byte) {
		nsm_lldp_mode_bitfield view = {};
		memcpy(&view, &byte, sizeof(byte));
		return view;
	};

	/* bits 0:1 – TX mode */
	nsm_lldp_mode_bitfield txOnly = {};
	txOnly.tx_mode = 3;
	EXPECT_EQ(toByte(txOnly), 0x03U);

	/* bits 2:3 – RX mode */
	nsm_lldp_mode_bitfield rxOnly = {};
	rxOnly.rx_mode = 3;
	EXPECT_EQ(toByte(rxOnly), 0x0CU);

	/* bit 4 – DCBX mode */
	nsm_lldp_mode_bitfield dcbxOnly = {};
	dcbxOnly.dcbx_mode = 1;
	EXPECT_EQ(toByte(dcbxOnly), 0x10U);

	/* bits 5:7 – reserved */
	nsm_lldp_mode_bitfield reservedOnly = {};
	reservedOnly.reserved = 7;
	EXPECT_EQ(toByte(reservedOnly), 0xE0U);

	/* combined: TX=All, RX=All, DCBX=Disabled */
	nsm_lldp_mode_bitfield allModes = {};
	allModes.tx_mode = NSM_LLDP_DIR_MODE_ALL;
	allModes.rx_mode = NSM_LLDP_DIR_MODE_ALL;
	allModes.dcbx_mode = NSM_LLDP_DCBX_DISABLED;
	EXPECT_EQ(toByte(allModes), 0x0AU);

	/* round-trip decode */
	auto decoded = fromByte(0x0AU);
	EXPECT_EQ(decoded.tx_mode, NSM_LLDP_DIR_MODE_ALL);
	EXPECT_EQ(decoded.rx_mode, NSM_LLDP_DIR_MODE_ALL);
	EXPECT_EQ(decoded.dcbx_mode, NSM_LLDP_DCBX_DISABLED);
	EXPECT_EQ(decoded.reserved, 0U);
}

TEST(lldpModeBitfield, EncodeDecodeRoundTripMemcpy)
{
	/* The bitfield struct maps directly to the wire byte via memcpy —
	 * verify all valid field combinations round-trip correctly. */
	struct {
		uint8_t tx;
		uint8_t rx;
		uint8_t dcbx;
		uint8_t expected_byte;
	} cases[] = {
	    {NSM_LLDP_DIR_MODE_OFF, NSM_LLDP_DIR_MODE_OFF,
	     NSM_LLDP_DCBX_DISABLED, 0x00},
	    {NSM_LLDP_DIR_MODE_MANDATORY, NSM_LLDP_DIR_MODE_OFF,
	     NSM_LLDP_DCBX_DISABLED, 0x01},
	    {NSM_LLDP_DIR_MODE_OFF, NSM_LLDP_DIR_MODE_MANDATORY,
	     NSM_LLDP_DCBX_DISABLED, 0x04},
	    {NSM_LLDP_DIR_MODE_ALL, NSM_LLDP_DIR_MODE_ALL,
	     NSM_LLDP_DCBX_DISABLED, 0x0A},
	    {NSM_LLDP_DIR_MODE_ALL, NSM_LLDP_DIR_MODE_ALL,
	     NSM_LLDP_DCBX_ENABLED, 0x1A},
	};

	for (const auto &c : cases) {
		nsm_lldp_mode_bitfield view = {};
		view.tx_mode = c.tx;
		view.rx_mode = c.rx;
		view.dcbx_mode = c.dcbx;

		uint8_t byte = 0xFF;
		memcpy(&byte, &view, sizeof(byte));
		EXPECT_EQ(byte, c.expected_byte);

		nsm_lldp_mode_bitfield decoded = {};
		memcpy(&decoded, &byte, sizeof(byte));
		EXPECT_EQ(decoded.tx_mode, c.tx);
		EXPECT_EQ(decoded.rx_mode, c.rx);
		EXPECT_EQ(decoded.dcbx_mode, c.dcbx);
		EXPECT_EQ(decoded.reserved, 0U);
	}
}

TEST(lldpModeBitfield, ReservedBitsRoundTrip)
{
	/* Bytes with bits 5:7 set must survive a memcpy round-trip: the
	 * reserved field captures them and they don't corrupt tx/rx/dcbx. */
	uint8_t byte = 0xEA; /* 0x0A | 0xE0 (reserved bits 5:7 all set) */
	nsm_lldp_mode_bitfield view = {};
	memcpy(&view, &byte, sizeof(byte));
	EXPECT_EQ(view.tx_mode, NSM_LLDP_DIR_MODE_ALL);
	EXPECT_EQ(view.rx_mode, NSM_LLDP_DIR_MODE_ALL);
	EXPECT_EQ(view.dcbx_mode, NSM_LLDP_DCBX_DISABLED);
	EXPECT_EQ(view.reserved, 7U);
}

// ---------------------------------------------------------------------------
// Power Capping Mode (NSM Type 5 Device Mode Index 27, enum8).
// enum8 {0 Default, 1 Enabled, 2 Disabled}; 1-byte data via the generic v2
// codec. DGXOPENBMC-27169 / NVBug 6320086.
// ---------------------------------------------------------------------------

TEST(powerCappingModeV2, enumValues)
{
	EXPECT_EQ(DEVICE_MODE_POWER_CAPPING, 27);
	EXPECT_EQ(NSM_POWER_CAPPING_MODE_DEFAULT, 0);
	EXPECT_EQ(NSM_POWER_CAPPING_MODE_ENABLED, 1);
	EXPECT_EQ(NSM_POWER_CAPPING_MODE_DISABLED, 2);
	EXPECT_EQ(POWER_CAPPING_MODE_DATA_SIZE, 1);
}

TEST(powerCappingModeV2, setEncodeRequestEnabled)
{
	uint32_t device_mode_index = DEVICE_MODE_POWER_CAPPING;
	uint8_t mode = NSM_POWER_CAPPING_MODE_ENABLED;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		POWER_CAPPING_MODE_DATA_SIZE - 1,
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_set_device_mode_settings_v2_req(
	    0, device_mode_index, &mode, POWER_CAPPING_MODE_DATA_SIZE, request);

	struct nsm_set_device_mode_settings_v2_req *req =
	    reinterpret_cast<struct nsm_set_device_mode_settings_v2_req *>(
		request->payload);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SET_DEVICE_MODE_SETTINGS_V2, req->hdr.command);
	EXPECT_EQ(sizeof(req->device_mode_index) + POWER_CAPPING_MODE_DATA_SIZE,
		  req->hdr.data_size);
	EXPECT_EQ(device_mode_index, le32toh(req->device_mode_index));
}

TEST(powerCappingModeV2, setDecodeRequestDisabled)
{
	uint32_t device_mode_index = DEVICE_MODE_POWER_CAPPING;
	uint8_t mode = NSM_POWER_CAPPING_MODE_DISABLED;

	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		POWER_CAPPING_MODE_DATA_SIZE - 1,
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	encode_set_device_mode_settings_v2_req(
	    0, device_mode_index, &mode, POWER_CAPPING_MODE_DATA_SIZE, request);

	uint32_t decoded_index = 0;
	uint8_t decoded_data[POWER_CAPPING_MODE_DATA_SIZE] = {0};
	uint16_t decoded_data_length = 0;

	auto rc = decode_set_device_mode_settings_v2_req(
	    request, requestMsg.size(), &decoded_index, decoded_data,
	    &decoded_data_length);

	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(device_mode_index, decoded_index);
	EXPECT_EQ(POWER_CAPPING_MODE_DATA_SIZE, decoded_data_length);
	EXPECT_EQ(NSM_POWER_CAPPING_MODE_DISABLED, decoded_data[0]);
}

TEST(powerCappingModeV2, getEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req),
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_get_device_mode_settings_v2_req(
	    0, DEVICE_MODE_POWER_CAPPING, request);

	uint32_t decoded_index = 0;
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(decode_get_device_mode_settings_v2_req(
		      request, requestMsg.size(), &decoded_index),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(DEVICE_MODE_POWER_CAPPING, decoded_index);
}

TEST(powerCappingModeV2, supportedListIncludesIndex27)
{
	const uint16_t handle = 0x0000;
	const uint16_t mode_count = 2;
	const uint32_t mode_list[] = {DEVICE_MODE_LLDP,
				      DEVICE_MODE_POWER_CAPPING};

	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_device_modes_resp) +
		(mode_count - 1) * sizeof(uint32_t),
	    0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	ASSERT_EQ(encode_get_supported_device_modes_resp(
		      0, NSM_SUCCESS, ERR_NULL, handle, mode_count, mode_list,
		      response),
		  NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint16_t decoded_handle = 0;
	uint16_t decoded_mode_count = 0;
	uint32_t decoded_mode_list[2] = {0};

	ASSERT_EQ(decode_get_supported_device_modes_resp(
		      response, responseMsg.size(), &cc, &reason_code,
		      &decoded_handle, &decoded_mode_count, decoded_mode_list),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_SUCCESS, cc);
	EXPECT_EQ(mode_count, decoded_mode_count);

	bool found = false;
	for (uint16_t i = 0; i < decoded_mode_count; i++) {
		if (decoded_mode_list[i] == DEVICE_MODE_POWER_CAPPING) {
			found = true;
		}
	}
	EXPECT_TRUE(found);
}

TEST(powerCappingModeV2, setEncodeRejectsNullDataWithLength)
{
	/* Bounds guard: encode must fail when length > 0 but data pointer is
	 * null. */
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) +
		POWER_CAPPING_MODE_DATA_SIZE - 1,
	    0);
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto rc = encode_set_device_mode_settings_v2_req(
	    0, DEVICE_MODE_POWER_CAPPING, nullptr, POWER_CAPPING_MODE_DATA_SIZE,
	    request);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(powerCappingModeV2, getDecodeRejectsTruncatedResponse)
{
	/* Oversized-buffer / truncated-response regression for the shared v2
	 * codec used by index 27: msg_len shorter than the fixed header must
	 * fail. */
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 4, 0);
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	uint8_t cc = 0;
	uint16_t reason_code = 0;
	uint8_t current_mode = 0;
	uint8_t pending_mode = 0;
	uint16_t current_len = 0;
	uint16_t pending_len = 0;
	auto rc = decode_get_device_mode_settings_v2_resp(
	    response, responseMsg.size(), &cc, &reason_code, &current_mode,
	    &current_len, &pending_mode, &pending_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
