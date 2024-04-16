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

#include <endian.h>
#include <string.h>

uint8_t pack_nsm_header(const struct nsm_header_info *hdr,
			struct nsm_msg_hdr *msg)
{
	if (msg == NULL || hdr == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (hdr->nsm_msg_type != NSM_RESPONSE &&
	    hdr->nsm_msg_type != NSM_REQUEST &&
	    hdr->nsm_msg_type != NSM_EVENT &&
	    hdr->nsm_msg_type != NSM_EVENT_ACKNOWLEDGMENT) {
		return NSM_SW_ERROR_DATA;
	}

	if (hdr->instance_id > NSM_INSTANCE_MAX) {
		return NSM_SW_ERROR_DATA;
	}

	msg->datagram = 0;
	if (hdr->nsm_msg_type == NSM_EVENT_ACKNOWLEDGMENT ||
	    hdr->nsm_msg_type == NSM_EVENT) {
		msg->datagram = 1;
	}

	msg->request = 0;
	if (hdr->nsm_msg_type == NSM_REQUEST ||
	    hdr->nsm_msg_type == NSM_EVENT) {
		msg->request = 1;
	}

	msg->pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->reserved = 0;
	msg->instance_id = hdr->instance_id;
	msg->ocp_type = OCP_TYPE;
	msg->ocp_version = OCP_VERSION;
	msg->nvidia_msg_type = hdr->nvidia_msg_type;

	return NSM_SW_SUCCESS;
}

uint8_t unpack_nsm_header(const struct nsm_msg_hdr *msg,
			  struct nsm_header_info *hdr)
{
	if (msg == NULL || hdr == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (be16toh(msg->pci_vendor_id) != PCI_VENDOR_ID) {
		return NSM_SW_ERROR_DATA;
	}

	if (msg->ocp_type != OCP_TYPE) {
		return NSM_SW_ERROR_DATA;
	}

	if (msg->ocp_version != OCP_VERSION) {
		return NSM_SW_ERROR_DATA;
	}

	if (msg->request == 0) {
		hdr->nsm_msg_type =
		    msg->datagram ? NSM_EVENT_ACKNOWLEDGMENT : NSM_RESPONSE;
	} else {
		hdr->nsm_msg_type = msg->datagram ? NSM_EVENT : NSM_REQUEST;
	}

	hdr->instance_id = msg->instance_id;
	hdr->nvidia_msg_type = msg->nvidia_msg_type;

	return NSM_SW_SUCCESS;
}

int encode_cc_only_resp(uint8_t instance_id, uint8_t type, uint8_t command,
			uint8_t cc, uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = type;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, command, msg);
	}

	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = command;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int encode_ping_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_PING;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int encode_ping_resp(uint8_t instance_id, uint16_t reason_code,
		     struct nsm_msg *msg)
{
	return encode_cc_only_resp(instance_id,
				   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				   NSM_PING, NSM_SUCCESS, reason_code, msg);
}

int decode_ping_resp(const struct nsm_msg *msg, size_t msgLen, uint8_t *cc,
		     uint16_t *reason_code)
{
	int rc = decode_reason_code_and_cc(msg, msgLen, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msgLen <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	if (resp->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_supported_nvidia_message_types_req(uint8_t instance_id,
						  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_supported_nvidia_message_types_req *request =
	    (struct nsm_get_supported_nvidia_message_types_req *)msg->payload;

	request->hdr.command = NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES;
	request->hdr.data_size = 0;

	return NSM_SW_SUCCESS;
}

int encode_get_supported_nvidia_message_types_resp(uint8_t instance_id,
						   uint8_t cc,
						   uint16_t reason_code,
						   const bitfield8_t *types,
						   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES, msg);
	}

	struct nsm_get_supported_nvidia_message_types_resp *response =
	    (struct nsm_get_supported_nvidia_message_types_resp *)msg->payload;

	response->hdr.command = NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(32);

	memcpy(response->supported_nvidia_message_types, types, 32);

	return NSM_SW_SUCCESS;
}

int decode_get_supported_nvidia_message_types_resp(const struct nsm_msg *msg,
						   size_t msg_len, uint8_t *cc,
						   uint16_t *reason_code,
						   bitfield8_t *types)
{
	if (types == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_supported_nvidia_message_types_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_supported_nvidia_message_types_resp *resp =
	    (struct nsm_get_supported_nvidia_message_types_resp *)msg->payload;

	memcpy(&(types->byte), resp->supported_nvidia_message_types, 32);

	return NSM_SW_SUCCESS;
}

int encode_get_supported_command_codes_req(uint8_t instance_id,
					   uint8_t nvidia_message_type,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_supported_command_codes_req *request =
	    (struct nsm_get_supported_command_codes_req *)msg->payload;

	request->hdr.command = NSM_SUPPORTED_COMMAND_CODES;
	request->hdr.data_size = 1;
	request->nvidia_message_type = nvidia_message_type;

	return NSM_SW_SUCCESS;
}

int encode_get_supported_command_codes_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    const bitfield8_t *command_codes,
					    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_SUPPORTED_COMMAND_CODES, msg);
	}

	struct nsm_get_supported_command_codes_resp *response =
	    (struct nsm_get_supported_command_codes_resp *)msg->payload;

	response->hdr.command = NSM_SUPPORTED_COMMAND_CODES;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(32);

	if (command_codes == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	memcpy(response->supported_command_codes, command_codes, 32);

	return NSM_SW_SUCCESS;
}

int decode_get_supported_command_codes_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code,
					    bitfield8_t *command_codes)
{
	if (command_codes == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_supported_command_codes_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_supported_command_codes_resp *resp =
	    (struct nsm_get_supported_command_codes_resp *)msg->payload;

	memcpy(&(command_codes->byte), resp->supported_command_codes, 32);

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_device_identification_req(uint8_t instance_id,
					       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_device_identification_req *request =
	    (struct nsm_query_device_identification_req *)msg->payload;

	request->hdr.command = NSM_QUERY_DEVICE_IDENTIFICATION;
	request->hdr.data_size = 0;

	return NSM_SW_SUCCESS;
}

int encode_query_device_identification_resp(uint8_t instance, uint8_t cc,
					    uint16_t reason_code,
					    const uint8_t device_identification,
					    const uint8_t device_instance,
					    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_DEVICE_IDENTIFICATION, msg);
	}

	struct nsm_query_device_identification_resp *response =
	    (struct nsm_query_device_identification_resp *)msg->payload;

	response->hdr.command = NSM_QUERY_DEVICE_IDENTIFICATION;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(2);

	response->device_identification = device_identification;
	response->instance_id = device_instance;

	return NSM_SW_SUCCESS;
}

int decode_query_device_identification_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code,
					    uint8_t *device_identification,
					    uint8_t *device_instance)
{
	if (device_identification == NULL || device_instance == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_device_identification_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_device_identification_resp *response =
	    (struct nsm_query_device_identification_resp *)msg->payload;

	*device_identification = response->device_identification;
	*device_instance = response->instance_id;

	return NSM_SW_SUCCESS;
}

int encode_reason_code(uint8_t cc, uint16_t reason_code, uint8_t command_code,
		       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_common_non_success_resp *response =
	    (struct nsm_common_non_success_resp *)msg->payload;

	response->command = command_code;
	response->completion_code = cc;
	reason_code = htole16(reason_code);
	response->reason_code = reason_code;

	return NSM_SW_SUCCESS;
}

int decode_reason_code_and_cc(const struct nsm_msg *msg, size_t msg_len,
			      uint8_t *cc, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL || reason_code == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*cc = ((struct nsm_common_resp *)msg->payload)->completion_code;
	if (*cc == NSM_SUCCESS) {
		return NSM_SW_SUCCESS;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_common_non_success_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_non_success_resp *response =
	    (struct nsm_common_non_success_resp *)msg->payload;

	// reason code is expected to be present if CC != NSM_SUCCESS
	*reason_code = le16toh(response->reason_code);

	return NSM_SW_SUCCESS;
}

int encode_nsm_event_acknowledgement(uint8_t instance_id, uint8_t nsm_type,
				     uint8_t event_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_EVENT_ACKNOWLEDGMENT;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = nsm_type;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_event_ack *event = (struct nsm_event_ack *)msg->payload;
	event->event_id = event_id;

	return NSM_SUCCESS;
}

int decode_nsm_event_acknowledgement(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *instance_id, uint8_t *nsm_type,
				     uint8_t *event_id)
{
	if (msg == NULL || instance_id == NULL || nsm_type == NULL ||
	    event_id == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_event_ack)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_event_ack *event = (struct nsm_event_ack *)msg->payload;

	*event_id = event->event_id;
	*instance_id = msg->hdr.instance_id;
	*nsm_type = msg->hdr.nvidia_msg_type;

	return NSM_SUCCESS;
}

int encode_nsm_event(uint8_t instance_id, uint8_t nsm_type, bool ackr,
		     uint8_t version, uint8_t event_id, uint8_t event_class,
		     uint16_t event_state, uint8_t data_size, uint8_t *data,
		     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_EVENT;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = nsm_type;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;

	event->version = version;
	event->ackr = ackr;
	event->event_id = event_id;
	event->event_class = event_class;
	event->event_state = htole16(event_state);
	event->data_size = data_size;
	if (data_size > 0) {
		memcpy(event->data, data, data_size);
	}

	return NSM_SUCCESS;
}

int decode_common_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_LENGTH;
	}
	return NSM_SW_SUCCESS;
}