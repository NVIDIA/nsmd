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



#include "device-capability-discovery.h"
#include "base.h"
#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_nsm_get_supported_event_source_req(uint8_t instance_id,
					      uint8_t nvidia_message_type,
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

	struct nsm_get_supported_event_source_req *request =
	    (struct nsm_get_supported_event_source_req *)msg->payload;

	request->hdr.command = NSM_GET_CURRENT_EVENT_SOURCES;
	request->hdr.data_size = NSM_GET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE;
	request->nvidia_message_type = nvidia_message_type;

	return NSM_SUCCESS;
}

int decode_nsm_get_supported_event_source_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    bitfield8_t **supported_event_sources)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_supported_event_source_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_supported_event_source_resp *response =
	    (struct nsm_get_supported_event_source_resp *)msg->payload;

	*cc = response->hdr.completion_code;
	if (NSM_SUCCESS != *cc) {
		return NSM_SUCCESS;
	}

	*supported_event_sources = response->supported_event_sources;

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
	request->hdr.data_size = NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE;
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

int encode_nsm_set_current_event_sources_req(uint8_t instance_id,
					     uint8_t nvidia_message_type,
					     bitfield8_t *event_sources,
					     struct nsm_msg *msg)
{
	if (event_sources == NULL || msg == NULL) {
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

	struct nsm_set_current_event_source_req *request =
	    (struct nsm_set_current_event_source_req *)msg->payload;

	request->hdr.command = NSM_SET_CURRENT_EVENT_SOURCES;
	request->hdr.data_size = NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE;
	request->nvidia_message_type = nvidia_message_type;
	memcpy(request->event_sources, event_sources, EVENT_SOURCES_LENGTH);

	return NSM_SUCCESS;
}

int decode_nsm_set_current_event_source_req(const struct nsm_msg *msg,
					    size_t msg_len,
					    uint8_t *nvidia_message_type,
					    bitfield8_t **event_sources)
{
	if (msg == NULL || nvidia_message_type == NULL ||
	    event_sources == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_current_event_source_req)) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	struct nsm_set_current_event_source_req *request =
	    (struct nsm_set_current_event_source_req *)msg->payload;

	if (request->hdr.data_size != NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	*nvidia_message_type = request->nvidia_message_type;
	*event_sources = request->event_sources;

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
