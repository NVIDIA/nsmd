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
#include "device-capability-discovery.h"

#include <endian.h>
#include <limits.h>
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

uint8_t pack_nsm_header_v2(const struct nsm_header_info *hdr,
			   struct nsm_msg_hdr *msg)
{
	uint8_t rc = pack_nsm_header(hdr, msg);
	if (rc == NSM_SW_SUCCESS) {
		msg->ocp_version = OCP_VERSION_V2;
	}
	return rc;
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

	if (msg->ocp_version != OCP_VERSION &&
	    msg->ocp_version != OCP_VERSION_V2) {
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

static void htoleArrayData(uint8_t *data, uint16_t num_of_element,
			   uint8_t data_type)
{
	switch (data_type) {
	case NvU8:
	case NvS8:
		// No operation for 8-bit types, just return
		break;

	case NvU16: {
		uint16_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(uint16_t)),
			       sizeof(uint16_t));
			tmp = htole16(tmp);
			memcpy(data + (i * sizeof(uint16_t)), &tmp,
			       sizeof(uint16_t));
		}
	} break;
	case NvS16: {
		int16_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(int16_t)),
			       sizeof(int16_t));
			tmp = htole16(tmp);
			memcpy(data + (i * sizeof(int16_t)), &tmp,
			       sizeof(int16_t));
		}
	} break;

	case NvU32: {
		uint32_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(uint32_t)),
			       sizeof(uint32_t));
			tmp = htole32(tmp);
			memcpy(data + (i * sizeof(uint32_t)), &tmp,
			       sizeof(uint32_t));
		}
	} break;
	case NvS32: {
		int32_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(int32_t)),
			       sizeof(int32_t));
			tmp = htole32(tmp);
			memcpy(data + (i * sizeof(int32_t)), &tmp,
			       sizeof(int32_t));
		}
	} break;

	case NvS24_8: {
		float tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(float)), sizeof(float));
			tmp = htole32(tmp);
			memcpy(data + (i * sizeof(float)), &tmp, sizeof(float));
		}
	} break;

	case NvU64: {
		uint64_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(uint64_t)),
			       sizeof(uint64_t));
			tmp = htole64(tmp);
			memcpy(data + (i * sizeof(uint64_t)), &tmp,
			       sizeof(uint64_t));
		}
	} break;
	case NvS64: {
		int64_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(int64_t)),
			       sizeof(int64_t));
			tmp = htole64(tmp);
			memcpy(data + (i * sizeof(int64_t)), &tmp,
			       sizeof(int64_t));
		}
	} break;

	default:
		// No operation for unknown types
		break;
	}
}

static void letohArrayData(uint8_t *data, uint16_t num_of_element,
			   uint8_t data_type)
{
	switch (data_type) {
	case NvU8:
	case NvS8:
		// No operation for 8-bit types, just return
		break;

	case NvU16: {
		uint16_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(uint16_t)),
			       sizeof(uint16_t));
			tmp = le16toh(tmp);
			memcpy(data + (i * sizeof(uint16_t)), &tmp,
			       sizeof(uint16_t));
		}
	} break;
	case NvS16: {
		int16_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(int16_t)),
			       sizeof(int16_t));
			tmp = le16toh(tmp);
			memcpy(data + (i * sizeof(int16_t)), &tmp,
			       sizeof(int16_t));
		}
	} break;

	case NvU32: {
		uint32_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(uint32_t)),
			       sizeof(uint32_t));
			tmp = le32toh(tmp);
			memcpy(data + (i * sizeof(uint32_t)), &tmp,
			       sizeof(uint32_t));
		}
	} break;
	case NvS32: {
		int32_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(int32_t)),
			       sizeof(int32_t));
			tmp = le32toh(tmp);
			memcpy(data + (i * sizeof(int32_t)), &tmp,
			       sizeof(int32_t));
		}
	} break;

	case NvS24_8: {
		float tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(float)), sizeof(float));
			tmp = le32toh(tmp);
			memcpy(data + (i * sizeof(float)), &tmp, sizeof(float));
		}
	} break;

	case NvU64: {
		uint64_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(uint64_t)),
			       sizeof(uint64_t));
			tmp = le64toh(tmp);
			memcpy(data + (i * sizeof(uint64_t)), &tmp,
			       sizeof(uint64_t));
		}
	} break;
	case NvS64: {
		int64_t tmp = 0;
		for (size_t i = 0; i < num_of_element; ++i) {
			memcpy(&tmp, data + (i * sizeof(int64_t)),
			       sizeof(int64_t));
			tmp = le64toh(tmp);
			memcpy(data + (i * sizeof(int64_t)), &tmp,
			       sizeof(int64_t));
		}
	} break;

	default:
		// No operation for unknown types
		break;
	}
}

static void dataCopy(uint8_t *srcData, uint8_t *destData, uint16_t numOfElement,
		     uint8_t dataType)
{
	size_t dataSize = 0;
	switch (dataType) {
	case NvU8:
		dataSize = sizeof(uint8_t) * numOfElement;
		break;
	case NvS8:
		dataSize = sizeof(int8_t) * numOfElement;
		break;
	case NvU16:
		dataSize = sizeof(uint16_t) * numOfElement;
		break;
	case NvS16:
		dataSize = sizeof(int16_t) * numOfElement;
		break;
	case NvU32:
		dataSize = sizeof(uint32_t) * numOfElement;
		break;
	case NvS32:
		dataSize = sizeof(int32_t) * numOfElement;
		break;
	case NvS24_8:
		dataSize = sizeof(float) * numOfElement;
		break;
	case NvU64:
		dataSize = sizeof(uint64_t) * numOfElement;
		break;
	case NvS64:
		dataSize = sizeof(int64_t) * numOfElement;
		break;
	default:
		// No operation for 8-bit types
		break;
	}
	memcpy(destData, srcData, dataSize);
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

int decode_get_supported_nvidia_message_types_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, bitfield8_t types[SUPPORTED_MSG_TYPE_DATA_SIZE])
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

	memcpy(types, resp->supported_nvidia_message_types,
	       SUPPORTED_MSG_TYPE_DATA_SIZE);

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

int decode_get_supported_command_codes_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    bitfield8_t command_codes[SUPPORTED_COMMAND_CODE_DATA_SIZE])
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

	memcpy(command_codes, resp->supported_command_codes,
	       SUPPORTED_COMMAND_CODE_DATA_SIZE);

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
	if (*cc == NSM_SUCCESS || *cc == NSM_ACCEPTED) {
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
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_EVENT, instance_id, nsm_type};
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

int decode_nsm_event(const struct nsm_msg *msg, size_t msg_len,
		     uint8_t event_id, uint8_t event_class,
		     uint16_t *event_state, uint8_t *data_size)
{
	if (msg == NULL || event_state == NULL || data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;

	if (event_id != event->event_id || event_class != event->event_class) {
		return NSM_SW_ERROR_DATA;
	}
	*event_state = le16toh(event->event_state);
	*data_size = event->data_size;

	if (msg_len < (sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
		       event->data_size)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int decode_nsm_event_with_data(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t event_id, uint8_t event_class,
			       uint16_t *event_state, uint8_t *data_size,
			       uint8_t *data)
{
	int rc = decode_nsm_event(msg, msg_len, event_id, event_class,
				  event_state, data_size);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	if (event->data_size > 0) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		memcpy(data, event->data, event->data_size);
	}

	return NSM_SW_SUCCESS;
}

int encode_common_req(uint8_t instance_id, uint8_t nvidia_msg_type,
		      uint8_t command, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_REQUEST, instance_id,
					 nvidia_msg_type};
	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = command;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_common_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	uint8_t rc = unpack_nsm_header(&msg->hdr, &header);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_LENGTH;
	}
	return NSM_SW_SUCCESS;
}

int encode_common_resp(uint8_t instance_id, uint8_t cc, uint16_t reason_code,
		       uint8_t nvidia_msg_type, uint8_t command,
		       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 nvidia_msg_type};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS && cc != NSM_ACCEPTED) {
		return encode_reason_code(cc, reason_code, command, msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	resp->command = command;
	resp->completion_code = cc;
	resp->data_size = htole16(0);
	return NSM_SW_SUCCESS;
}

int decode_common_resp(const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
		       uint16_t *data_size, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL || data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    (sizeof(struct nsm_msg_hdr)) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	*data_size = le16toh(resp->data_size);

	return NSM_SW_SUCCESS;
}

/** @brief Encode a long running event state
 *
 *  @param[in] event_state - long running event state
 *  @return uint16_t - Encoded long running event state
 */
static uint16_t encode_long_running_event_state(
    const struct nsm_long_running_event_state *event_state)
{
	if (event_state == NULL) {
		return 0;
	}
	return *(uint16_t *)event_state;
}

int encode_long_running_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, uint8_t nvidia_msg_type,
			     uint8_t command, const uint8_t *data,
			     uint8_t data_size, struct nsm_msg *msg)
{
	struct nsm_long_running_event_state event_state = {
	    .nvidia_message_type = nvidia_msg_type, .command = command};
	int rc = encode_nsm_event(
	    instance_id, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, false,
	    NSM_EVENT_VERSION, NSM_LONG_RUNNING_EVENT,
	    NSM_NVIDIA_GENERAL_EVENT_CLASS,
	    encode_long_running_event_state(&event_state), 0, NULL, msg);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	if (cc != NSM_SUCCESS) {
		struct nsm_long_running_non_success_resp *resp =
		    (struct nsm_long_running_non_success_resp *)event->data;

		event->data_size =
		    sizeof(struct nsm_long_running_non_success_resp);
		resp->completion_code = cc;
		resp->reason_code = htole16(reason_code);
		resp->instance_id = instance_id;

		rc = NSM_SW_SUCCESS;
	} else {
		struct nsm_long_running_resp *resp =
		    (struct nsm_long_running_resp *)event->data;
		resp->completion_code = cc;
		resp->reserved = 0;
		resp->instance_id = instance_id;

		if (data_size >
		    (UCHAR_MAX - sizeof(struct nsm_long_running_resp))) {
			rc = NSM_SW_ERROR_LENGTH;
		} else if (data_size > 0 && data == NULL) {
			rc = NSM_SW_ERROR_NULL;
		} else {
			event->data_size =
			    data_size + sizeof(struct nsm_long_running_resp);
			if (data != NULL) {
				memcpy(event->data +
					   sizeof(struct nsm_long_running_resp),
				       data, data_size);
			}
			rc = NSM_SW_SUCCESS;
		}
	}
	return rc;
}

int decode_long_running_event(const struct nsm_msg *msg, size_t msg_len,
			      uint8_t *instance_id, uint8_t *cc,
			      uint16_t *reason_code)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			  sizeof(struct nsm_long_running_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	event->event_state = le16toh(event->event_state);

	if (instance_id != NULL) {
		*instance_id =
		    ((struct nsm_long_running_resp *)event->data)->instance_id;
	}
	if (cc != NULL && reason_code != NULL) {
		*cc = ((struct nsm_long_running_resp *)event->data)
			  ->completion_code;
		if (*cc != NSM_SUCCESS) {
			if (msg_len !=
			    (sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			     sizeof(
				 struct nsm_long_running_non_success_resp))) {
				return NSM_SW_ERROR_LENGTH;
			}

			struct nsm_long_running_non_success_resp *response =
			    (struct nsm_long_running_non_success_resp *)
				event->data;

			// reason code is expected to be present if CC !=
			// NSM_SUCCESS
			*reason_code = le16toh(response->reason_code);
		}
	}
	return NSM_SW_SUCCESS;
}

int decode_long_running_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t nvidia_msg_type, uint8_t command,
			     uint8_t *cc, uint16_t *reason_code)
{
	if (cc == NULL || reason_code == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_long_running_event(msg, msg_len, NULL, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	struct nsm_long_running_event_state event_state = {
	    .nvidia_message_type = nvidia_msg_type, .command = command};

	if (msg->hdr.nvidia_msg_type != NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY ||
	    event->event_class != NSM_NVIDIA_GENERAL_EVENT_CLASS ||
	    event->event_id != NSM_LONG_RUNNING_EVENT ||
	    event->event_state !=
		encode_long_running_event_state(&event_state)) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int decode_long_running_resp_with_data(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t nvidia_msg_type,
				       uint8_t command, uint8_t *cc,
				       uint16_t *reason_code, uint8_t *data,
				       uint8_t data_size)
{
	int rc = decode_long_running_resp(msg, msg_len, nvidia_msg_type,
					  command, cc, reason_code);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	if (msg_len < (sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
		       event->data_size) ||
	    event->data_size < sizeof(struct nsm_long_running_resp) ||
	    (event->data_size - sizeof(struct nsm_long_running_resp)) <
		data_size) {
		return NSM_SW_ERROR_LENGTH;
	}
	memcpy(data, event->data + sizeof(struct nsm_long_running_resp),
	       event->data_size - sizeof(struct nsm_long_running_resp));

	return NSM_SW_SUCCESS;
}

int encode_raw_cmd_req(uint8_t instanceId, uint8_t messageType,
		       uint8_t commandCode, const uint8_t *payload,
		       size_t dataSize, struct nsm_msg *msg)
{
	if (!msg || (dataSize > 0 && !payload)) {
		return NSM_SW_ERROR_NULL;
	}

	// Reuse encode_common_req for setting up the header and command
	// structure
	uint8_t rc =
	    encode_common_req(instanceId, messageType, commandCode, msg);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	// Copy the command data into the payload after the common request
	if (dataSize > 0) {
		memcpy(msg->payload + sizeof(struct nsm_common_req), payload,
		       dataSize);
	}

	// Set the data_size to the size of the command data
	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;
	request->data_size = dataSize;

	return NSM_SW_SUCCESS;
}

int encode_get_histogram_format_req(uint8_t instance_id, uint32_t histogram_id,
				    uint16_t parameter, struct nsm_msg *msg)
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

	struct nsm_get_histogram_format_req *request =
	    (struct nsm_get_histogram_format_req *)msg->payload;

	request->hdr.command = NSM_GET_HISTOGRAM_FORMAT;
	request->hdr.data_size = sizeof(histogram_id) + sizeof(parameter);
	request->histogram_id = htole32(histogram_id);
	request->parameter = htole16(parameter);

	return NSM_SW_SUCCESS;
}

int decode_get_histogram_format_req(const struct nsm_msg *msg, size_t msg_len,
				    uint32_t *histogram_id, uint16_t *parameter)
{
	if (msg == NULL || histogram_id == NULL || parameter == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_histogram_format_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_histogram_format_req *request =
	    (struct nsm_get_histogram_format_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->histogram_id) + sizeof(request->parameter)) {
		return NSM_SW_ERROR_DATA;
	}

	*histogram_id = le32toh(request->histogram_id);
	*parameter = le16toh(request->parameter);

	return NSM_SW_SUCCESS;
}

int encode_get_histogram_format_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_histogram_format_metadata *meta_data, uint8_t *bucket_offsets,
    uint32_t bucket_offsets_size, struct nsm_msg *msg)
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
		return encode_reason_code(cc, reason_code,
					  NSM_GET_HISTOGRAM_FORMAT, msg);
	}

	struct nsm_get_histogram_format_resp *resp =
	    (struct nsm_get_histogram_format_resp *)msg->payload;

	resp->hdr.command = NSM_GET_HISTOGRAM_FORMAT;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(
	    sizeof(struct nsm_histogram_format_metadata) + bucket_offsets_size);
	resp->metadata.num_of_buckets = htole16(meta_data->num_of_buckets);
	resp->metadata.min_sampling_time =
	    htole32(meta_data->min_sampling_time);
	resp->metadata.accumulation_cycle = meta_data->accumulation_cycle;
	resp->metadata.reserved0 = 0;
	resp->metadata.increment_duration =
	    htole32(meta_data->increment_duration);
	resp->metadata.bucket_unit_of_measure =
	    meta_data->bucket_unit_of_measure;
	resp->metadata.reserved1 = 0;
	resp->metadata.bucket_data_type = meta_data->bucket_data_type;
	resp->metadata.reserved2 = 0;

	if (cc == NSM_SUCCESS) {
		if (bucket_offsets == NULL) {
			return NSM_SW_ERROR_NULL;
		}

		htoleArrayData(bucket_offsets, meta_data->num_of_buckets,
			       meta_data->bucket_data_type);
		dataCopy(bucket_offsets, resp->bucket_offsets,
			 meta_data->num_of_buckets,
			 meta_data->bucket_data_type);
	}

	return NSM_SW_SUCCESS;
}

int decode_get_histogram_format_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_histogram_format_metadata *meta_data, uint8_t *bucket_offsets,
    uint32_t *bucket_offsets_size)
{
	if (data_size == NULL || meta_data == NULL || bucket_offsets == NULL ||
	    bucket_offsets_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_histogram_format_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_histogram_format_resp *resp =
	    (struct nsm_get_histogram_format_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	meta_data->num_of_buckets = le16toh(resp->metadata.num_of_buckets);
	meta_data->min_sampling_time =
	    le32toh(resp->metadata.min_sampling_time);
	meta_data->accumulation_cycle = resp->metadata.accumulation_cycle;
	meta_data->reserved0 = 0;
	meta_data->increment_duration =
	    le32toh(resp->metadata.increment_duration);
	meta_data->bucket_unit_of_measure =
	    resp->metadata.bucket_unit_of_measure;
	meta_data->reserved1 = 0;
	meta_data->bucket_data_type = resp->metadata.bucket_data_type;
	meta_data->reserved2 = 0;
	*bucket_offsets_size =
	    *data_size - sizeof(struct nsm_histogram_format_metadata);

	dataCopy(resp->bucket_offsets, bucket_offsets,
		 meta_data->num_of_buckets, meta_data->bucket_data_type);
	letohArrayData(bucket_offsets, meta_data->num_of_buckets,
		       meta_data->bucket_data_type);

	return NSM_SW_SUCCESS;
}

int encode_get_histogram_data_req(uint8_t instance_id, uint32_t histogram_id,
				  uint16_t parameter, struct nsm_msg *msg)
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

	nsm_get_histogram_data_req *request =
	    (nsm_get_histogram_data_req *)msg->payload;

	request->hdr.command = NSM_GET_HISTOGRAM_DATA;
	request->hdr.data_size = sizeof(histogram_id) + sizeof(parameter);
	request->histogram_id = htole32(histogram_id);
	request->parameter = htole16(parameter);

	return NSM_SW_SUCCESS;
}

int decode_get_histogram_data_req(const struct nsm_msg *msg, size_t msg_len,
				  uint32_t *histogram_id, uint16_t *parameter)
{
	if (msg == NULL || histogram_id == NULL || parameter == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_histogram_data_req *request =
	    (nsm_get_histogram_data_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->histogram_id) + sizeof(request->parameter)) {
		return NSM_SW_ERROR_DATA;
	}

	*histogram_id = le32toh(request->histogram_id);
	*parameter = le16toh(request->parameter);

	return NSM_SW_SUCCESS;
}

int encode_get_histogram_data_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    uint8_t bucket_data_type, uint16_t num_of_buckets, uint8_t *bucket_data,
    uint32_t bucket_data_size, struct nsm_msg *msg)
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
		return encode_reason_code(cc, reason_code,
					  NSM_GET_HISTOGRAM_DATA, msg);
	}

	struct nsm_get_histogram_data_resp *resp =
	    (struct nsm_get_histogram_data_resp *)msg->payload;

	resp->hdr.command = NSM_GET_HISTOGRAM_DATA;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size =
	    htole16(sizeof(num_of_buckets) + sizeof(bucket_data_type) +
		    bucket_data_size);
	resp->bucket_data_type = bucket_data_type;
	resp->num_of_buckets = htole16(num_of_buckets);

	if (cc == NSM_SUCCESS) {
		if (bucket_data == NULL) {
			return NSM_SW_ERROR_NULL;
		}

		htoleArrayData(bucket_data, num_of_buckets, bucket_data_type);
		dataCopy(bucket_data, resp->bucket_data, num_of_buckets,
			 bucket_data_type);
	}

	return NSM_SW_SUCCESS;
}

int decode_get_histogram_data_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size, uint8_t *bucket_data_type,
    uint16_t *num_of_buckets, uint8_t *bucket_data, uint32_t *bucket_data_size)
{
	if (data_size == NULL || bucket_data_type == NULL ||
	    num_of_buckets == NULL || bucket_data == NULL ||
	    bucket_data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_histogram_data_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_histogram_data_resp *resp =
	    (struct nsm_get_histogram_data_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	*bucket_data_type = resp->bucket_data_type;
	*num_of_buckets = le16toh(resp->num_of_buckets);
	*bucket_data_size = *data_size - sizeof(resp->num_of_buckets) -
			    sizeof(resp->bucket_data_type);

	dataCopy(resp->bucket_data, bucket_data, *num_of_buckets,
		 *bucket_data_type);
	letohArrayData(bucket_data, *num_of_buckets, *bucket_data_type);

	return NSM_SW_SUCCESS;
}