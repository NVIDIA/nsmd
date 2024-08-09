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

#include "debug-token.h"

#include <endian.h>
#include <string.h>

int decode_nsm_query_token_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum nsm_debug_token_opcode *token_opcode)
{
	if (msg == NULL || token_opcode == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_token_parameters_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_token_parameters_req *request =
	    (struct nsm_query_token_parameters_req *)msg->payload;
	if (request->hdr.data_size < sizeof(request->token_opcode)) {
		return NSM_SW_ERROR_DATA;
	}
	*token_opcode = request->token_opcode;

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_token_parameters_req(
    uint8_t instance_id, enum nsm_debug_token_opcode token_opcode,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (token_opcode != NSM_DEBUG_TOKEN_OPCODE_RMCS &&
	    token_opcode != NSM_DEBUG_TOKEN_OPCODE_RMDT &&
	    token_opcode != NSM_DEBUG_TOKEN_OPCODE_CRCS &&
	    token_opcode != NSM_DEBUG_TOKEN_OPCODE_CRDT &&
	    token_opcode != NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_token_parameters_req *request =
	    (struct nsm_query_token_parameters_req *)msg->payload;
	request->hdr.command = NSM_QUERY_TOKEN_PARAMETERS;
	request->hdr.data_size = 1;
	request->token_opcode = token_opcode;

	return NSM_SW_SUCCESS;
}

int decode_nsm_query_token_parameters_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_debug_token_request *token_request)
{
	if (msg == NULL || token_request == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_token_parameters_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_token_parameters_resp *resp =
	    (struct nsm_query_token_parameters_resp *)msg->payload;
	if (resp->token_request.token_request_size !=
	    sizeof(struct nsm_debug_token_request)) {
		return NSM_SW_ERROR_DATA;
	}
	*token_request = resp->token_request;

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_token_parameters_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_debug_token_request *token_request, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_TOKEN_PARAMETERS, msg);
	}

	struct nsm_query_token_parameters_resp *response =
	    (struct nsm_query_token_parameters_resp *)msg->payload;
	response->hdr.command = NSM_QUERY_TOKEN_PARAMETERS;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_debug_token_request));
	response->token_request = *token_request;

	return NSM_SW_SUCCESS;
}

int decode_nsm_provide_token_req(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *token_data, uint8_t *token_data_len)
{
	if (msg == NULL || token_data == NULL || token_data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_provide_token_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_provide_token_req *request =
	    (struct nsm_provide_token_req *)msg->payload;
	if (request->hdr.data_size == 0) {
		return NSM_SW_ERROR_DATA;
	}
	memcpy(token_data, request->token_data, request->hdr.data_size);
	*token_data_len = request->hdr.data_size;

	return NSM_SW_SUCCESS;
}

int encode_nsm_provide_token_req(uint8_t instance_id, const uint8_t *token_data,
				 const uint16_t token_data_len,
				 struct nsm_msg *msg)
{
	if (msg == NULL || token_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (token_data_len == 0) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header_v2(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_provide_token_req *request =
	    (struct nsm_provide_token_req *)msg->payload;
	request->hdr.command = NSM_PROVIDE_TOKEN;
	request->hdr.data_size = htole16(token_data_len);
	memcpy(request->token_data, token_data, token_data_len);

	return NSM_SW_SUCCESS;
}

int decode_nsm_provide_token_resp(const struct nsm_msg *msg, size_t msg_len,
				  uint8_t *cc, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL || reason_code == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_provide_token_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_provide_token_resp(uint8_t instance_id, uint8_t cc,
				  uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_PROVIDE_TOKEN,
					  msg);
	}

	struct nsm_query_token_parameters_resp *response =
	    (struct nsm_query_token_parameters_resp *)msg->payload;
	response->hdr.command = NSM_PROVIDE_TOKEN;
	response->hdr.completion_code = cc;
	response->hdr.data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_nsm_disable_tokens_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_disable_tokens_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_disable_tokens_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_disable_tokens_req *request =
	    (nsm_disable_tokens_req *)msg->payload;
	request->command = NSM_DISABLE_TOKENS;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_nsm_disable_tokens_resp(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *cc, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL || reason_code == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_disable_tokens_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_disable_tokens_resp(uint8_t instance_id, uint8_t cc,
				   uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_DISABLE_TOKENS,
					  msg);
	}

	struct nsm_query_token_parameters_resp *response =
	    (struct nsm_query_token_parameters_resp *)msg->payload;
	response->hdr.command = NSM_DISABLE_TOKENS;
	response->hdr.completion_code = cc;
	response->hdr.data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_nsm_query_token_status_req(const struct nsm_msg *msg, size_t msg_len,
				      enum nsm_debug_token_type *token_type)
{
	if (msg == NULL || token_type == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_token_status_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_token_status_req *request =
	    (struct nsm_query_token_status_req *)msg->payload;
	if (request->hdr.data_size < sizeof(request->token_type)) {
		return NSM_SW_ERROR_DATA;
	}
	*token_type = request->token_type;

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_token_status_req(uint8_t instance_id,
				      enum nsm_debug_token_type token_type,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (token_type != NSM_DEBUG_TOKEN_TYPE_FRC &&
	    token_type != NSM_DEBUG_TOKEN_TYPE_CRCS &&
	    token_type != NSM_DEBUG_TOKEN_TYPE_CRDT &&
	    token_type != NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_token_status_req *request =
	    (struct nsm_query_token_status_req *)msg->payload;
	request->hdr.command = NSM_QUERY_TOKEN_STATUS;
	request->hdr.data_size = 1;
	request->token_type = token_type;

	return NSM_SW_SUCCESS;
}

int decode_nsm_query_token_status_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, enum nsm_debug_token_status *status,
    enum nsm_debug_token_status_additional_info *additional_info,
    enum nsm_debug_token_type *token_type, uint32_t *time_left)
{
	if (msg == NULL || status == NULL || additional_info == NULL ||
	    token_type == NULL || time_left == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_token_status_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_token_status_resp *resp =
	    (struct nsm_query_token_status_resp *)msg->payload;
	*status = resp->status;
	*additional_info = resp->additional_info;
	*token_type = resp->token_type;
	*time_left = resp->time_left;

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_token_status_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    enum nsm_debug_token_status status,
    enum nsm_debug_token_status_additional_info additional_info,
    enum nsm_debug_token_type token_type, uint32_t time_left,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_TOKEN_STATUS, msg);
	}

	struct nsm_query_token_status_resp *response =
	    (struct nsm_query_token_status_resp *)msg->payload;
	response->hdr.command = NSM_QUERY_TOKEN_STATUS;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(status) + sizeof(additional_info) +
		    sizeof(token_type) + sizeof(time_left));
	response->status = status;
	response->additional_info = additional_info;
	response->token_type = token_type;
	response->time_left = time_left;

	return NSM_SW_SUCCESS;
}

int decode_nsm_query_device_ids_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_query_device_ids_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_device_ids_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_query_device_ids_req *request =
	    (nsm_query_device_ids_req *)msg->payload;
	request->command = NSM_QUERY_DEVICE_IDS;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_nsm_query_device_ids_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint8_t device_id[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE])
{
	if (msg == NULL || cc == NULL || reason_code == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_device_ids_resp)) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_query_device_ids_resp *resp =
	    (struct nsm_query_device_ids_resp *)msg->payload;
	memcpy(device_id, resp->device_id, NSM_DEBUG_TOKEN_DEVICE_ID_SIZE);

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_device_ids_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const uint8_t device_id[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE],
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_QUERY_DEVICE_IDS,
					  msg);
	}

	struct nsm_query_device_ids_resp *response =
	    (struct nsm_query_device_ids_resp *)msg->payload;
	response->hdr.command = NSM_QUERY_DEVICE_IDS;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(NSM_DEBUG_TOKEN_DEVICE_ID_SIZE);
	memcpy(response->device_id, device_id, NSM_DEBUG_TOKEN_DEVICE_ID_SIZE);

	return NSM_SW_SUCCESS;
}
