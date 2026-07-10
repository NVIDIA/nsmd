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

#include "device-capability-discovery.h"
#include "base.h"
#include "firmware-utils.h"
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int encode_nsm_get_event_source_req(uint8_t command, uint8_t instance_id,
					   uint8_t nvidia_message_type,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_REQUEST, instance_id,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_event_source_req *request =
	    (struct nsm_get_event_source_req *)msg->payload;

	request->hdr.command = command;
	request->hdr.data_size = NSM_GET_EVENT_SOURCES_REQ_DATA_SIZE;
	request->nvidia_message_type = nvidia_message_type;

	return NSM_SW_SUCCESS;
}

int encode_nsm_get_supported_event_source_req(uint8_t instance_id,
					      uint8_t nvidia_message_type,
					      struct nsm_msg *msg)
{
	return encode_nsm_get_event_source_req(NSM_GET_SUPPORTED_EVENT_SOURCES,
					       instance_id, nvidia_message_type,
					       msg);
}

int encode_nsm_get_current_event_source_req(uint8_t instance_id,
					    uint8_t nvidia_message_type,
					    struct nsm_msg *msg)
{
	return encode_nsm_get_event_source_req(NSM_GET_CURRENT_EVENT_SOURCES,
					       instance_id, nvidia_message_type,
					       msg);
}

int decode_nsm_get_event_source_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *nvidia_message_type)
{
	if (msg == NULL || nvidia_message_type == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	uint8_t rc = unpack_nsm_header(&msg->hdr, &header);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_event_source_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_event_source_req *request =
	    (struct nsm_get_event_source_req *)msg->payload;

	*nvidia_message_type = request->nvidia_message_type;

	return NSM_SW_SUCCESS;
}

static int encode_nsm_get_event_source_resp(
    uint8_t command, uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const bitfield8_t event_sources[EVENT_SOURCES_LENGTH], struct nsm_msg *msg)
{
	if (msg == NULL || event_sources == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, command, msg);
	}

	struct nsm_get_event_source_resp *response =
	    (struct nsm_get_event_source_resp *)msg->payload;

	response->hdr.command = command;
	response->hdr.completion_code = cc;
	response->hdr.data_size = EVENT_SOURCES_LENGTH;
	memcpy(response->event_sources, event_sources, EVENT_SOURCES_LENGTH);

	return NSM_SW_SUCCESS;
}

int encode_nsm_get_supported_event_source_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const bitfield8_t event_sources[EVENT_SOURCES_LENGTH], struct nsm_msg *msg)
{
	return encode_nsm_get_event_source_resp(NSM_GET_SUPPORTED_EVENT_SOURCES,
						instance_id, cc, reason_code,
						event_sources, msg);
}

int encode_nsm_get_current_event_source_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const bitfield8_t event_sources[EVENT_SOURCES_LENGTH], struct nsm_msg *msg)
{
	return encode_nsm_get_event_source_resp(NSM_GET_CURRENT_EVENT_SOURCES,
						instance_id, cc, reason_code,
						event_sources, msg);
}

int decode_nsm_get_event_source_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, bitfield8_t event_sources[EVENT_SOURCES_LENGTH])
{
	if (msg == NULL || cc == NULL || reason_code == NULL ||
	    event_sources == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_event_source_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_event_source_resp *response =
	    (struct nsm_get_event_source_resp *)msg->payload;

	memcpy(event_sources, response->event_sources, EVENT_SOURCES_LENGTH);

	return NSM_SW_SUCCESS;
}

int encode_nsm_get_event_subscription_req(uint8_t instance_id,
					  struct nsm_msg *msg)
{
	return encode_common_req(instance_id,
				 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				 NSM_GET_EVENT_SUBSCRIPTION, msg);
}

int decode_nsm_get_event_subscription_req(const struct nsm_msg *msg,
					  size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_nsm_get_event_subscription_resp(uint8_t instance_id, uint8_t cc,
					   uint16_t reason_code,
					   uint8_t receiver_eid,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_EVENT_SUBSCRIPTION, msg);
	}

	struct nsm_get_event_subscription_resp *response =
	    (struct nsm_get_event_subscription_resp *)msg->payload;

	response->hdr.command = NSM_GET_EVENT_SUBSCRIPTION;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(1);
	response->receiver_endpoint_id = receiver_eid;

	return NSM_SUCCESS;
}

int decode_nsm_get_event_subscription_resp(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *cc,
					   uint16_t *reason_code,
					   uint8_t *receiver_eid)
{
	if (msg == NULL || receiver_eid == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	uint8_t rc = unpack_nsm_header(&msg->hdr, &header);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_get_event_subscription_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_event_subscription_resp *response =
	    (struct nsm_get_event_subscription_resp *)msg->payload;

	response->hdr.data_size = le16toh(response->hdr.data_size);
	if (response->hdr.data_size != 1) {
		return NSM_SW_ERROR_LENGTH;
	}

	*receiver_eid = response->receiver_endpoint_id;

	return NSM_SUCCESS;
}

int encode_nsm_set_event_subscription_req(uint8_t instance_id,
					  uint8_t global_setting,
					  uint8_t receiver_eid,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_set_event_subscription_req *request =
	    (struct nsm_set_event_subscription_req *)msg->payload;

	request->hdr.command = NSM_SET_EVENT_SUBSCRIPTION;
	request->hdr.data_size = NSM_SET_EVENT_SUBSCRIPTION_REQ_DATA_SIZE;
	request->global_event_generation_setting = global_setting;
	request->receiver_endpoint_id = receiver_eid;

	return NSM_SUCCESS;
}

int decode_nsm_set_event_subscription_req(const struct nsm_msg *msg,
					  size_t msg_len,
					  uint8_t *global_setting,
					  uint8_t *receiver_eid)
{
	if (msg == NULL || global_setting == NULL || receiver_eid == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_event_subscription_req)) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_set_event_subscription_req *request =
	    (struct nsm_set_event_subscription_req *)msg->payload;

	if (request->hdr.data_size < NSM_SET_EVENT_SUBSCRIPTION_REQ_DATA_SIZE) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	*global_setting = request->global_event_generation_setting;
	*receiver_eid = request->receiver_endpoint_id;

	return NSM_SUCCESS;
}

int decode_nsm_set_event_subscription_resp(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *cc)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_event_subscription_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_set_event_subscription_resp *response =
	    (struct nsm_set_event_subscription_resp *)msg->payload;

	*cc = response->hdr.completion_code;

	return NSM_SUCCESS;
}

int encode_nsm_configure_event_acknowledgement_req(
    uint8_t instance_id, uint8_t nvidia_message_type,
    bitfield8_t *current_event_sources_acknowledgement_mask,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_configure_event_acknowledgement_req *request =
	    (struct nsm_configure_event_acknowledgement_req *)msg->payload;

	request->hdr.command = NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT;
	request->hdr.data_size =
	    NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE;
	request->nvidia_message_type = nvidia_message_type;
	memcpy(request->current_event_sources_acknowledgement_mask,
	       current_event_sources_acknowledgement_mask,
	       EVENT_ACKNOWLEDGEMENT_MASK_LENGTH);

	return NSM_SUCCESS;
}

int decode_nsm_configure_event_acknowledgement_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *nvidia_message_type,
    bitfield8_t **current_event_sources_acknowledgement_mask)
{
	if (msg == NULL || nvidia_message_type == NULL ||
	    current_event_sources_acknowledgement_mask == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_configure_event_acknowledgement_req)) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_configure_event_acknowledgement_req *request =
	    (struct nsm_configure_event_acknowledgement_req *)msg->payload;

	if (request->hdr.data_size <
	    NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	*nvidia_message_type = request->nvidia_message_type;
	*current_event_sources_acknowledgement_mask =
	    request->current_event_sources_acknowledgement_mask;

	return NSM_SUCCESS;
}

int encode_nsm_configure_event_acknowledgement_resp(
    uint8_t instance_id, uint8_t cc,
    bitfield8_t *new_event_sources_acknowledgement_mask, struct nsm_msg *msg)
{
	if (new_event_sources_acknowledgement_mask == NULL || msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_configure_event_acknowledgement_resp *response =
	    (struct nsm_configure_event_acknowledgement_resp *)msg->payload;

	response->hdr.command = NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT;
	response->hdr.completion_code = cc;
	response->hdr.reserved = 0;
	response->hdr.data_size = htole16(EVENT_ACKNOWLEDGEMENT_MASK_LENGTH);
	memcpy(response->new_event_sources_acknowledgement_mask,
	       new_event_sources_acknowledgement_mask,
	       EVENT_ACKNOWLEDGEMENT_MASK_LENGTH);
	return NSM_SUCCESS;
}

int decode_nsm_configure_event_acknowledgement_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    bitfield8_t **new_event_sources_acknowledgement_mask)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_configure_event_acknowledgement_resp)) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_configure_event_acknowledgement_resp *response =
	    (struct nsm_configure_event_acknowledgement_resp *)msg->payload;

	*cc = response->hdr.completion_code;
	*new_event_sources_acknowledgement_mask =
	    response->new_event_sources_acknowledgement_mask;

	return NSM_SUCCESS;
}

int encode_nsm_set_current_event_sources_req(
    uint8_t instance_id, uint8_t nvidia_message_type,
    bitfield8_t event_sources[EVENT_SOURCES_LENGTH], struct nsm_msg *msg)
{
	if (event_sources == NULL || msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_set_current_event_source_req *request =
	    (struct nsm_set_current_event_source_req *)msg->payload;

	request->hdr.command = NSM_SET_CURRENT_EVENT_SOURCES;
	request->hdr.data_size = NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE;
	request->nvidia_message_type = nvidia_message_type;
	memcpy(request->event_sources, event_sources, EVENT_SOURCES_LENGTH);

	return NSM_SUCCESS;
}

int decode_nsm_set_current_event_sources_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *nvidia_message_type,
    bitfield8_t event_sources[EVENT_SOURCES_LENGTH])
{
	if (msg == NULL || nvidia_message_type == NULL ||
	    event_sources == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_current_event_source_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_current_event_source_req *request =
	    (struct nsm_set_current_event_source_req *)msg->payload;

	if (request->hdr.data_size !=
	    NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	*nvidia_message_type = request->nvidia_message_type;
	memcpy(event_sources, request->event_sources, EVENT_SOURCES_LENGTH);

	return NSM_SUCCESS;
}

int decode_nsm_set_current_event_sources_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_current_event_source_resp)) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_set_current_event_source_resp *response =
	    (struct nsm_set_current_event_source_resp *)msg->payload;

	*cc = response->hdr.completion_code;

	return NSM_SUCCESS;
}

int encode_nsm_get_event_log_record_req(uint8_t instance_id,
					uint8_t selector_type,
					uint32_t selector, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_event_log_record_req *request =
	    (struct nsm_get_event_log_record_req *)msg->payload;

	request->hdr.command = NSM_GET_EVENT_LOG_RECORD;
	request->hdr.data_size = 5;
	request->selector_type = selector_type;
	request->selector = htole32(selector);

	return NSM_SUCCESS;
}

int decode_nsm_get_event_log_record_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint8_t *nvidia_message_type, uint8_t *event_id, uint32_t *event_handle,
    uint64_t *timestamp, uint8_t **payload, uint16_t *payload_len)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_event_log_record_resp) - 1) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_get_event_log_record_resp *response =
	    (struct nsm_get_event_log_record_resp *)msg->payload;

	*cc = response->hdr.completion_code;
	*nvidia_message_type = response->nvidia_message_type;
	*event_id = response->event_id;
	*event_handle = le32toh(response->event_handle);
	*timestamp = le64toh(response->timestamp);
	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size > NSM_GET_EVENT_LOG_RECORD_RESP_MIN_DATA_SIZE) {
		*payload = response->payload;
		*payload_len =
		    data_size - NSM_GET_EVENT_LOG_RECORD_RESP_MIN_DATA_SIZE;
	}

	return NSM_SUCCESS;
}

int encode_nsm_rediscovery_event(uint8_t instance_id, bool ackr,
				 struct nsm_msg *msg)
{
	return encode_nsm_event(instance_id,
				NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, ackr,
				NSM_EVENT_VERSION, NSM_REDISCOVERY_EVENT,
				NSM_GENERAL_EVENT_CLASS, 0, 0, NULL, msg);
}

int decode_nsm_rediscovery_event(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *event_class, uint16_t *event_state)
{
	if (msg == NULL || event_class == NULL || event_state == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	if (event->data_size > 0) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	*event_class = event->event_class;
	*event_state = le16toh(event->event_state);

	return NSM_SUCCESS;
}

int encode_nsm_get_device_capabilities_v2_req(uint8_t instance_id,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_device_capabilities_v2_req *request =
	    (struct nsm_get_device_capabilities_v2_req *)msg->payload;
	request->hdr.command = NSM_GET_DEVICE_CAPABILITIES_V2;
	request->hdr.data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_nsm_get_device_capabilities_v2_req(const struct nsm_msg *msg,
					      size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_device_capabilities_v2_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_get_device_capabilities_v2_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    uint8_t timestamp_generation, uint32_t maximum_input_buffer_size,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_DEVICE_CAPABILITIES_V2, msg);
	}

	struct nsm_get_device_capabilities_v2_resp *response =
	    (struct nsm_get_device_capabilities_v2_resp *)msg->payload;
	response->hdr.command = NSM_GET_DEVICE_CAPABILITIES_V2;
	response->hdr.completion_code = cc;

	uint16_t telemetry_count = 0;
	uint16_t msg_size = sizeof(struct nsm_common_telemetry_resp);

	uint8_t *data = &(response->payload[0]);
	encode_nsm_firmware_aggregate_tag_uint8(
	    &data, NSM_TAG_TIMESTAMP_GENERATION, timestamp_generation,
	    &msg_size);
	++telemetry_count;
	encode_nsm_firmware_aggregate_tag_uint32(
	    &data, NSM_TAG_MAXIMUM_INPUT_BUFFER_SIZE, maximum_input_buffer_size,
	    &msg_size);
	++telemetry_count;
	response->hdr.telemetry_count = htole16(telemetry_count);

	return NSM_SW_SUCCESS;
}

int decode_nsm_get_device_capabilities_v2_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint8_t *timestamp_generation,
    uint32_t *maximum_input_buffer_size)
{
	if (msg == NULL || cc == NULL || reason_code == NULL ||
	    timestamp_generation == NULL || maximum_input_buffer_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_firmware_aggregate_tag)) {
		return NSM_SW_ERROR_LENGTH;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	/* Require the full telemetry response header before dereferencing any
	 * field inside it (telemetry_count is a uint16_t at offset 2 of the
	 * struct, so a message shorter than
	 * hdr+sizeof(nsm_common_telemetry_resp) would cause an OOB read without
	 * this guard). */
	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_common_telemetry_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_device_capabilities_v2_resp *response =
	    (struct nsm_get_device_capabilities_v2_resp *)msg->payload;
	if (response->hdr.telemetry_count < 2) {
		return NSM_SW_ERROR_LENGTH;
	}
	uint16_t telemetry_count = le16toh(response->hdr.telemetry_count);
	uint8_t *data = &(response->payload[0]);
	uint16_t data_len = (uint16_t)msg_len - sizeof(struct nsm_msg_hdr) -
			    sizeof(struct nsm_common_telemetry_resp);
	uint8_t tag = 0;
	uint8_t valid = 0;
	bool rc_ok = false;

	while ((data_len >= sizeof(struct nsm_firmware_aggregate_tag)) &&
	       (telemetry_count > 0)) {
		struct nsm_firmware_aggregate_tag *field =
		    (struct nsm_firmware_aggregate_tag *)data;
		tag = field->tag;
		if (tag == NSM_TAG_TIMESTAMP_GENERATION) {
			uint8_t value8 = 0;
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    &data, &tag, &valid, &value8, &data_len);
			if (rc_ok) {
				--telemetry_count;
				*timestamp_generation = value8;
			}
		} else if (tag == NSM_TAG_MAXIMUM_INPUT_BUFFER_SIZE) {
			uint32_t value32 = 0;
			rc_ok = decode_nsm_firmware_aggregate_tag_uint32(
			    &data, &tag, &valid, &value32, &data_len);
			if (rc_ok) {
				--telemetry_count;
				*maximum_input_buffer_size = value32;
			}
		} else {
			rc_ok = decode_nsm_firmware_aggregate_tag_skip(
			    &data, &data_len);
			if (rc_ok) {
				--telemetry_count;
			}
		}
		if (!rc_ok || !valid) {
			return NSM_SW_ERROR_DATA;
		}
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_gpio_state_change_event(
    uint8_t instance_id, bool ackr,
    const struct nsm_gpio_state_change_event_payload *payload,
    struct nsm_msg *msg)
{
	if (msg == NULL || payload == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	size_t payload_size =
	    sizeof(struct nsm_gpio_state_change_event_payload) -
	    sizeof(struct nsm_gpio_event) +
	    (payload->num_gpio_events * sizeof(struct nsm_gpio_event));

	uint8_t event_data[NSM_EVENT_DATA_MAX_LEN];
	struct nsm_gpio_state_change_event_payload temp_payload = *payload;
	temp_payload.timestamp_low = htole32(temp_payload.timestamp_low);
	temp_payload.timestamp_high = htole32(temp_payload.timestamp_high);
	temp_payload.num_gpio_events = htole16(temp_payload.num_gpio_events);

	// Copy the fixed part (timestamps and num_gpio_events)
	size_t cpySize = sizeof(struct nsm_gpio_state_change_event_payload) -
			 sizeof(struct nsm_gpio_event);
	memcpy(event_data, &temp_payload, cpySize);

	// Copy and convert each gpio_event
	for (uint16_t i = 0; i < payload->num_gpio_events; i++) {
		uint16_t gpio_event_raw;
		memcpy(&gpio_event_raw, &payload->gpio_events[i],
		       sizeof(uint16_t));
		gpio_event_raw = htole16(gpio_event_raw);
		memcpy(event_data + cpySize + i * sizeof(struct nsm_gpio_event),
		       &gpio_event_raw, sizeof(uint16_t));
	}

	return encode_nsm_event(
	    instance_id, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, ackr,
	    NSM_EVENT_VERSION, NSM_GPIO_STATE_CHANGE_EVENT,
	    NSM_GENERAL_EVENT_CLASS, 0, payload_size, event_data, msg);
}

int decode_nsm_gpio_state_change_event(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *event_class,
    uint16_t *event_state, struct nsm_gpio_state_change_event_payload **payload)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    payload == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;

	/* Minimum payload size: timestamp + num_gpio_events */
	if (event->data_size <
	    (sizeof(struct nsm_gpio_state_change_event_payload) -
	     sizeof(struct nsm_gpio_event))) {
		return NSM_SW_ERROR_DATA;
	}

	/* The on-wire data_size must fit within the bytes actually received
	 * before any payload field (incl. the trailing gpio_events[] array)
	 * is dereferenced. */
	if (sizeof(struct nsm_msg_hdr) + offsetof(struct nsm_event, data) +
		(size_t)event->data_size >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}

	*event_class = event->event_class;
	*event_state = le16toh(event->event_state);

	*payload = (struct nsm_gpio_state_change_event_payload *)event->data;

	/* Convert from little endian */
	(*payload)->timestamp_low = le32toh((*payload)->timestamp_low);
	(*payload)->timestamp_high = le32toh((*payload)->timestamp_high);
	(*payload)->num_gpio_events = le16toh((*payload)->num_gpio_events);

	/* Verify the payload size matches the expected size */
	size_t expected_size =
	    sizeof(struct nsm_gpio_state_change_event_payload) -
	    sizeof(struct nsm_gpio_event) +
	    ((*payload)->num_gpio_events * sizeof(struct nsm_gpio_event));
	if (event->data_size != expected_size) {
		return NSM_SW_ERROR_DATA;
	}

	/* Convert GPIO events from little endian */
	for (uint16_t i = 0; i < (*payload)->num_gpio_events; i++) {
		/* Convert the 16-bit GPIO event data from little endian */
		uint16_t gpio_data;
		memcpy(&gpio_data, &(*payload)->gpio_events[i],
		       sizeof(uint16_t));
		gpio_data = le16toh(gpio_data);
		memcpy(&(*payload)->gpio_events[i], &gpio_data,
		       sizeof(uint16_t));
	}

	return NSM_SUCCESS;
}

int encode_nsm_get_event_log_record_v2_req(uint8_t instance_id, uint8_t mode,
					   uint16_t event_handle,
					   uint16_t transfer_handle,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_event_log_record_v2_req *request =
	    (struct nsm_get_event_log_record_v2_req *)msg->payload;

	request->hdr.command = NSM_GET_EVENT_LOG_RECORD_V2;
	request->hdr.data_size =
	    sizeof(struct nsm_get_event_log_record_v2_req) -
	    sizeof(struct nsm_common_req);
	request->mode = mode;
	request->event_handle = htole16(event_handle);
	request->transfer_handle = htole16(transfer_handle);

	return NSM_SUCCESS;
}

int decode_nsm_get_event_log_record_v2_req(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *mode,
					   uint16_t *event_handle,
					   uint16_t *transfer_handle)
{
	if (msg == NULL || mode == NULL || event_handle == NULL ||
	    transfer_handle == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_get_event_log_record_v2_req)) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_get_event_log_record_v2_req *request =
	    (struct nsm_get_event_log_record_v2_req *)msg->payload;

	*mode = request->mode;
	*event_handle = le16toh(request->event_handle);
	*transfer_handle = le16toh(request->transfer_handle);

	return NSM_SUCCESS;
}

int encode_nsm_get_event_log_record_v2_resp_first_handle(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_event_log_record_v2_first_fields *in, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_EVENT_LOG_RECORD_V2, msg);
	}

	if (in == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_get_event_log_record_v2_resp_first_handle *response =
	    (struct nsm_get_event_log_record_v2_resp_first_handle *)
		msg->payload;

	response->hdr.command = NSM_GET_EVENT_LOG_RECORD_V2;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(
	    NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE +
	    in->event_data_len);
	response->next_transfer_handle = htole16(in->next_transfer_handle);
	response->event_handle = htole16(in->event_handle);
	response->nvidia_message_type = in->nvidia_message_type;
	response->event_version = in->event_version;
	response->event_id = in->event_id;
	response->event_class = in->event_class;
	response->event_state = htole16(in->event_state);
	if (in->event_data_len > 0 && in->event_data != NULL) {
		memcpy(response->event_data, in->event_data,
		       in->event_data_len);
	}

	return NSM_SW_SUCCESS;
}

int decode_nsm_get_event_log_record_v2_resp_first_handle(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    struct nsm_event_log_record_v2_first_fields *out)
{
	if (msg == NULL || cc == NULL || out == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint16_t reason_code = 0;
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, &reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp) +
		NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_event_log_record_v2_resp_first_handle *response =
	    (struct nsm_get_event_log_record_v2_resp_first_handle *)
		msg->payload;

	out->next_transfer_handle = le16toh(response->next_transfer_handle);
	out->event_handle = le16toh(response->event_handle);
	out->nvidia_message_type = response->nvidia_message_type;
	out->event_version = response->event_version;
	out->event_id = response->event_id;
	out->event_class = response->event_class;
	out->event_state = le16toh(response->event_state);

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size >
	    NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE) {
		out->event_data = response->event_data;
		out->event_data_len =
		    data_size -
		    NSM_GET_EVENT_LOG_RECORD_V2_RESP_FIRST_HANDLE_MIN_DATA_SIZE;
		if (sizeof(struct nsm_msg_hdr) +
			offsetof(struct
				 nsm_get_event_log_record_v2_resp_first_handle,
				 event_data) +
			(size_t)out->event_data_len >
		    msg_len) {
			return NSM_SW_ERROR_LENGTH;
		}
	} else {
		out->event_data = NULL;
		out->event_data_len = 0;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_get_event_log_record_v2_resp_next_handle(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_event_log_record_v2_next_fields *in, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_EVENT_LOG_RECORD_V2, msg);
	}

	if (in == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_get_event_log_record_v2_resp_next_handle *response =
	    (struct nsm_get_event_log_record_v2_resp_next_handle *)msg->payload;

	response->hdr.command = NSM_GET_EVENT_LOG_RECORD_V2;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE +
		    in->event_data_len);
	response->next_transfer_handle = htole16(in->next_transfer_handle);
	response->event_handle = htole16(in->event_handle);
	if (in->event_data_len > 0 && in->event_data != NULL) {
		memcpy(response->event_data, in->event_data,
		       in->event_data_len);
	}

	return NSM_SW_SUCCESS;
}

int decode_nsm_get_event_log_record_v2_resp_next_handle(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    struct nsm_event_log_record_v2_next_fields *out)
{
	if (msg == NULL || cc == NULL || out == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint16_t reason_code = 0;
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, &reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp) +
		NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_event_log_record_v2_resp_next_handle *response =
	    (struct nsm_get_event_log_record_v2_resp_next_handle *)msg->payload;

	out->next_transfer_handle = le16toh(response->next_transfer_handle);
	out->event_handle = le16toh(response->event_handle);

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size >
	    NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE) {
		out->event_data = response->event_data;
		out->event_data_len =
		    data_size -
		    NSM_GET_EVENT_LOG_RECORD_V2_RESP_NEXT_HANDLE_MIN_DATA_SIZE;
		if (sizeof(struct nsm_msg_hdr) +
			offsetof(
			    struct nsm_get_event_log_record_v2_resp_next_handle,
			    event_data) +
			(size_t)out->event_data_len >
		    msg_len) {
			return NSM_SW_ERROR_LENGTH;
		}
	} else {
		out->event_data = NULL;
		out->event_data_len = 0;
	}

	return NSM_SW_SUCCESS;
}
