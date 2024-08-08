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

#include "firmware-utils.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

#define DBG1(x)
#define DBG2(x)
#define DBGW(x)
#define DBGE(x) x

struct nsm_firmware_aggregate_tag {
	uint8_t tag;
	uint8_t valid : 1;
	uint8_t length : 3;
	uint8_t reserved : 4;
	uint8_t data[1];
} __attribute__((packed));

void printArrayAsHex(const uint8_t *array, size_t size)
{
	for (size_t i = 0; i < size; ++i) {
		printf("%02X ", array[i]);
	}
	printf("\n");
}

void encode_nsm_firmware_aggregate_tag_uint8(uint8_t **buffer, uint8_t tag,
					     uint8_t value,
					     uint16_t *buffer_size)
{
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	field->tag = tag;
	field->valid = 1;
	field->length = 0;
	field->reserved = 0;
	field->data[0] = value;
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint8_t) - 1;
	*buffer_size +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint8_t) - 1;
}

void encode_nsm_firmware_aggregate_tag_uint16(uint8_t **buffer, uint8_t tag,
					      uint16_t value,
					      uint16_t *buffer_size)
{
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	field->tag = tag;
	field->valid = 1;
	field->length = 1;
	field->reserved = 0;
	value = htole16(value);
	memcpy(field->data, &value, sizeof(uint16_t));
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint16_t) - 1;
	*buffer_size +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint16_t) - 1;
}

void encode_nsm_firmware_aggregate_tag_uint32(uint8_t **buffer, uint8_t tag,
					      uint32_t value,
					      uint16_t *buffer_size)
{
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	field->tag = tag;
	field->valid = 1;
	field->length = 2;
	field->reserved = 0;
	value = htole32(value);
	memcpy(field->data, &value, sizeof(uint32_t));
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint32_t) - 1;
	*buffer_size +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint32_t) - 1;
}

void encode_nsm_firmware_aggregate_tag_uint64(uint8_t **buffer, uint8_t tag,
					      uint64_t value,
					      uint16_t *buffer_size)
{
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	field->tag = tag;
	field->valid = 1;
	field->length = 3;
	field->reserved = 0;
	value = htole64(value);
	memcpy(field->data, &value, sizeof(uint64_t));
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint64_t) - 1;
	*buffer_size +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint64_t) - 1;
}

void encode_nsm_firmware_aggregate_tag_uint8_array(uint8_t **buffer,
						   uint8_t tag, uint8_t *value,
						   uint16_t *buffer_size)
{
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	field->tag = tag;
	field->valid = 1;
	field->length = 4;
	field->reserved = 0;
	memcpy(field->data, value, 16);
	*buffer += sizeof(struct nsm_firmware_aggregate_tag) + 16 - 1;
	*buffer_size += sizeof(struct nsm_firmware_aggregate_tag) + 16 - 1;
}

bool decode_nsm_firmware_aggregate_tag_uint8(uint8_t **buffer, uint8_t *tag,
					     uint8_t *valid, uint8_t *value,
					     uint16_t *buffer_size)
{
	if (*buffer_size <
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint8_t) - 1) {
		return false;
	}
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	*tag = field->tag;
	*valid = field->valid;
	if (field->valid) {
		*value = field->data[0];
	}
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint8_t) - 1;
	*buffer_size -=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint8_t) - 1;
	return true;
}

bool decode_nsm_firmware_aggregate_tag_uint16(uint8_t **buffer, uint8_t *tag,
					      uint8_t *valid, uint16_t *value,
					      uint16_t *buffer_size)
{
	if (*buffer_size <
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint16_t) - 1) {
		return false;
	}
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	*tag = field->tag;
	*valid = field->valid;
	if (field->valid) {
		memcpy(value, field->data, sizeof(uint16_t));
		*value = le16toh(*value);
	}
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint16_t) - 1;
	*buffer_size -=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint16_t) - 1;
	return true;
}

bool decode_nsm_firmware_aggregate_tag_uint32(uint8_t **buffer, uint8_t *tag,
					      uint8_t *valid, uint32_t *value,
					      uint16_t *buffer_size)
{
	if (*buffer_size <
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint32_t) - 1) {
		return false;
	}
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	*tag = field->tag;
	*valid = field->valid;
	if (field->valid) {
		memcpy(value, field->data, sizeof(uint32_t));
		*value = le32toh(*value);
	}
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint32_t) - 1;
	*buffer_size -=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint32_t) - 1;
	return true;
}

bool decode_nsm_firmware_aggregate_tag_uint64(uint8_t **buffer, uint8_t *tag,
					      uint8_t *valid, uint64_t *value,
					      uint16_t *buffer_size)
{
	if (*buffer_size <
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint64_t) - 1) {
		return false;
	}
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	*tag = field->tag;
	*valid = field->valid;
	if (field->valid) {
		memcpy(value, field->data, sizeof(uint64_t));
		*value = le64toh(*value);
	}
	*buffer +=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint64_t) - 1;
	*buffer_size -=
	    sizeof(struct nsm_firmware_aggregate_tag) + sizeof(uint64_t) - 1;
	return true;
}

bool decode_nsm_firmware_aggregate_tag_uint8_array(uint8_t **buffer,
						   uint8_t *tag, uint8_t *valid,
						   uint8_t *value,
						   uint16_t *buffer_size)
{
	uint16_t length;
	if (*buffer_size < sizeof(struct nsm_firmware_aggregate_tag) + 4 - 1) {
		return false;
	}

	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*buffer;
	*tag = field->tag;
	*valid = field->valid;
	length = 1 << field->length;
	if (*buffer_size <
	    sizeof(struct nsm_firmware_aggregate_tag) + length - 1) {
		return false;
	}

	if (field->valid) {
		memcpy(value, field->data, length);
	}

	*buffer += sizeof(struct nsm_firmware_aggregate_tag) + length - 1;
	*buffer_size -= sizeof(struct nsm_firmware_aggregate_tag) + length - 1;
	return true;
}

int decode_nsm_query_get_erot_state_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_firmware_erot_state_info_req *fw_req)
{
	if (msg == NULL || fw_req == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_firmware_get_erot_state_info_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_firmware_get_erot_state_info_req *request =
	    (struct nsm_firmware_get_erot_state_info_req *)msg->payload;
	if (request->hdr.data_size < sizeof(*fw_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*fw_req = request->fq_req;

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_get_erot_state_parameters_req(
    uint8_t instance_id, const struct nsm_firmware_erot_state_info_req *fw_req,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_firmware_get_erot_state_info_req *request =
	    (struct nsm_firmware_get_erot_state_info_req *)msg->payload;
	request->hdr.command = NSM_FW_GET_EROT_STATE_INFORMATION;
	request->hdr.data_size =
	    htole16(sizeof(struct nsm_firmware_erot_state_info_req));
	memcpy(&request->fq_req, fw_req,
	       sizeof(struct nsm_firmware_erot_state_info_req));

	return NSM_SW_SUCCESS;
}

int encode_nsm_query_get_erot_state_parameters_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_firmware_erot_state_info_resp *fw_info, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_FIRMWARE;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_FW_GET_EROT_STATE_INFORMATION, msg);
	}

	struct nsm_firmware_get_erot_state_info_resp *response =
	    (struct nsm_firmware_get_erot_state_info_resp *)msg->payload;
	response->hdr.command = NSM_FW_GET_EROT_STATE_INFORMATION;
	response->hdr.completion_code = cc;

	uint16_t telemetry_count = 0;
	uint16_t msg_size = sizeof(struct nsm_common_telemetry_resp);

	uint8_t *ptr = &(response->payload[0]);

	encode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, NSM_FIRMWARE_BACKGROUND_COPY_POLICY,
	    fw_info->fq_resp_hdr.background_copy_policy, &msg_size);
	telemetry_count++;
	encode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT,
	    fw_info->fq_resp_hdr.active_slot, &msg_size);
	telemetry_count++;
	encode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, NSM_FIRMWARE_ACTIVE_KEY_SET,
	    fw_info->fq_resp_hdr.active_keyset, &msg_size);
	telemetry_count++;
	encode_nsm_firmware_aggregate_tag_uint16(
	    &ptr, NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER,
	    fw_info->fq_resp_hdr.minimum_security_version, &msg_size);
	telemetry_count++;
	encode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, NSM_FIRMWARE_INBAND_UPDATE_POLICY,
	    fw_info->fq_resp_hdr.inband_update_policy, &msg_size);
	telemetry_count++;
	encode_nsm_firmware_aggregate_tag_uint64(
	    &ptr, NSM_FIRMWARE_BOOT_STATUS_CODE,
	    fw_info->fq_resp_hdr.boot_status_code, &msg_size);
	telemetry_count++;
	encode_nsm_firmware_aggregate_tag_uint8(
	    &ptr, NSM_FIRMWARE_FIRMWARE_SLOT_COUNT,
	    fw_info->fq_resp_hdr.firmware_slot_count, &msg_size);
	telemetry_count++;

	for (int i = 0; i < fw_info->fq_resp_hdr.firmware_slot_count; i++) {
		encode_nsm_firmware_aggregate_tag_uint8(
		    &ptr, NSM_FIRMWARE_FIRMWARE_SLOT_ID,
		    fw_info->slot_info[i].slot_id, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint8_array(
		    &ptr, NSM_FIRMWARE_FIRMWARE_VERSION_STRING,
		    (uint8_t *)(&(
			fw_info->slot_info[i].firmware_version_string[0])),
		    &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint32(
		    &ptr, NSM_FIRMWARE_VERSION_COMPARISON_STAMP,
		    fw_info->slot_info[i].version_comparison_stamp, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint8(
		    &ptr, NSM_FIRMWARE_BUILD_TYPE,
		    fw_info->slot_info[i].build_type, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint8(
		    &ptr, NSM_FIRMWARE_SIGNING_TYPE,
		    fw_info->slot_info[i].signing_type, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint8(
		    &ptr, NSM_FIRMWARE_WRITE_PROTECT_STATE,
		    fw_info->slot_info[i].write_protect_state, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint8(
		    &ptr, NSM_FIRMWARE_FIRMWARE_STATE,
		    fw_info->slot_info[i].firmware_state, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint16(
		    &ptr, NSM_FIRMWARE_SECURITY_VERSION_NUMBER,
		    fw_info->slot_info[i].security_version_number, &msg_size);
		telemetry_count++;
		encode_nsm_firmware_aggregate_tag_uint16(
		    &ptr, NSM_FIRMWARE_SIGNING_KEY_INDEX,
		    fw_info->slot_info[i].signing_key_index, &msg_size);
		telemetry_count++;
	}

	response->hdr.telemetry_count = htole16(telemetry_count);

	DBG1(printf("Telemetry count = %u, resp buffer (size = %u):\n",
		    (uint32_t)telemetry_count, (uint32_t)msg_size);)
	DBG1(printArrayAsHex(&(response->payload[0]), msg_size);)

	return NSM_SW_SUCCESS;
}

int decode_nsm_query_firmware_header_information(
    struct nsm_firmware_erot_state_info_hdr_resp *fw_info_hdr, uint8_t **ptr,
    uint16_t *payload_size, uint16_t *telemetry_count)
{
	uint8_t tag, valid;
	bool rc_ok;

	while ((*payload_size >= sizeof(struct nsm_firmware_aggregate_tag)) &&
	       (*telemetry_count > 0)) {
		struct nsm_firmware_aggregate_tag *field =
		    (struct nsm_firmware_aggregate_tag *)*ptr;
		tag = field->tag;

		if (tag == NSM_FIRMWARE_BACKGROUND_COPY_POLICY) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid,
			    &(fw_info_hdr->background_copy_policy),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf(
					 "Decoded "
					 "NSM_FIRMWARE_BACKGROUND_COPY_POLICY, "
					 "value = 0x%02x\n",
					 fw_info_hdr->background_copy_policy);)
			}
		} else if (tag == NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid, &(fw_info_hdr->active_slot),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT,"
					    " value = 0x%02x\n",
					    fw_info_hdr->active_slot);)
			}
		} else if (tag == NSM_FIRMWARE_ACTIVE_KEY_SET) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid, &(fw_info_hdr->active_keyset),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf(
					 "Decoded NSM_FIRMWARE_ACTIVE_KEY_SET, "
					 "value = 0x%02x\n",
					 fw_info_hdr->active_keyset);)
			}
		} else if (tag ==
			   NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint16(
			    ptr, &tag, &valid,
			    &(fw_info_hdr->minimum_security_version),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(
				    printf(
					"Decoded "
					"NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_"
					"NUMBER, value = 0x%04x\n",
					fw_info_hdr->minimum_security_version);)
			}
		} else if (tag == NSM_FIRMWARE_INBAND_UPDATE_POLICY) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid,
			    &(fw_info_hdr->inband_update_policy), payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_INBAND_UPDATE_POLICY,"
					    " value = 0x%02x\n",
					    fw_info_hdr->inband_update_policy);)
			}
		} else if (tag == NSM_FIRMWARE_BOOT_STATUS_CODE) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint64(
			    ptr, &tag, &valid,
			    &(fw_info_hdr->boot_status_code), payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_BOOT_STATUS_CODE,"
					    " value = 0x%016lx\n",
					    fw_info_hdr->boot_status_code);)
			}
		} else if (tag == NSM_FIRMWARE_FIRMWARE_SLOT_COUNT) {
			if (decode_nsm_firmware_aggregate_tag_uint8(
				ptr, &tag, &valid,
				&(fw_info_hdr->firmware_slot_count),
				payload_size)) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_FIRMWARE_SLOT_COUNT, "
					    "value = 0x%02x\n",
					    fw_info_hdr->firmware_slot_count);)
				break;
			}
			rc_ok = false;
		} else {
			/* Not expected field before firmware slot count */
			DBGE(printf("Error: Decoded unsupported tag in header: "
				    "0x%02x\n",
				    field->tag);)
			DBGE(printf("Left telemetries: %u, left payload: %u\n",
				    (uint32_t)(*telemetry_count),
				    (uint32_t)(*payload_size));)
			return NSM_SW_ERROR_DATA;
		}

		if (!rc_ok) {
			DBGE(printf("Error: The decoded message header is not "
				    "properly encoded\n");)
			DBGE(printf("Left telemetries: %u, left payload: %u\n",
				    (uint32_t)(*telemetry_count),
				    (uint32_t)(*payload_size));)
			return NSM_SW_ERROR_LENGTH;
		}
	}

	DBG2(printf("Success in decoding the response header\n");)
	return NSM_SW_SUCCESS;
}

int decode_nsm_query_firmware_slot_information(
    struct nsm_firmware_slot_info *fw_slot_info, uint8_t **ptr,
    uint16_t *payload_size, uint16_t *telemetry_count)
{
	uint8_t tag, valid;
	bool rc_ok;

	if ((*payload_size < sizeof(struct nsm_firmware_aggregate_tag)) ||
	    (*telemetry_count == 0)) {
		DBG2(printf("Error: Expected NSM_FIRMWARE_FIRMWARE_SLOT_ID, "
			    "but there is no more tags in the message\n");)
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)*ptr;
	tag = field->tag;

	// Firmware slot ID tag must be always as the first one
	if (tag == NSM_FIRMWARE_FIRMWARE_SLOT_ID) {
		if (!decode_nsm_firmware_aggregate_tag_uint8(
			ptr, &tag, &valid, &(fw_slot_info->slot_id),
			payload_size)) {
			return NSM_SW_ERROR_LENGTH;
		}
		(*telemetry_count)--;
		DBG2(printf(
			 "Decoded NSM_FIRMWARE_FIRMWARE_SLOT_ID, value = %u\n",
			 fw_slot_info->slot_id);)
	} else {
		return NSM_SW_ERROR_DATA;
	}

	while ((*payload_size >= sizeof(struct nsm_firmware_aggregate_tag)) &&
	       (*telemetry_count > 0)) {
		field = (struct nsm_firmware_aggregate_tag *)*ptr;
		tag = field->tag;

		if (tag == NSM_FIRMWARE_FIRMWARE_VERSION_STRING) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8_array(
			    ptr, &tag, &valid,
			    (uint8_t *)(&(
				fw_slot_info->firmware_version_string[0])),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(
				    printf(
					"Decoded "
					"NSM_FIRMWARE_FIRMWARE_VERSION_STRING, "
					"value = %s\n",
					(char
					     *)(fw_slot_info
						    ->firmware_version_string));)
			}
		} else if (tag == NSM_FIRMWARE_VERSION_COMPARISON_STAMP) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint32(
			    ptr, &tag, &valid,
			    &(fw_slot_info->version_comparison_stamp),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_VERSION_COMPARISON_"
					    "STAMP, value = %u\n",
					    fw_slot_info
						->version_comparison_stamp);)
			}
		} else if (tag == NSM_FIRMWARE_BUILD_TYPE) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid, &(fw_slot_info->build_type),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded NSM_FIRMWARE_BUILD_TYPE, "
					    "value = %u\n",
					    fw_slot_info->build_type);)
			}
		} else if (tag == NSM_FIRMWARE_SIGNING_TYPE) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid, &(fw_slot_info->signing_type),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(
				    printf("Decoded NSM_FIRMWARE_SIGNING_TYPE, "
					   "value = %u\n",
					   fw_slot_info->signing_type);)
			}
		} else if (tag == NSM_FIRMWARE_WRITE_PROTECT_STATE) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid,
			    &(fw_slot_info->write_protect_state), payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_WRITE_PROTECT_STATE, "
					    "value = %u\n",
					    fw_slot_info->write_protect_state);)
			}
		} else if (tag == NSM_FIRMWARE_FIRMWARE_STATE) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint8(
			    ptr, &tag, &valid, &(fw_slot_info->firmware_state),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf(
					 "Decoded NSM_FIRMWARE_FIRMWARE_STATE, "
					 "value = %u\n",
					 fw_slot_info->firmware_state);)
			}
		} else if (tag == NSM_FIRMWARE_SECURITY_VERSION_NUMBER) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint16(
			    ptr, &tag, &valid,
			    &(fw_slot_info->security_version_number),
			    payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(
				    printf(
					"Decoded "
					"NSM_FIRMWARE_SECURITY_VERSION_NUMBER, "
					"value = %u\n",
					fw_slot_info->security_version_number);)
			}
		} else if (tag == NSM_FIRMWARE_SIGNING_KEY_INDEX) {
			rc_ok = decode_nsm_firmware_aggregate_tag_uint16(
			    ptr, &tag, &valid,
			    &(fw_slot_info->signing_key_index), payload_size);
			if (rc_ok) {
				(*telemetry_count)--;
				DBG2(printf("Decoded "
					    "NSM_FIRMWARE_SIGNING_KEY_INDEX, "
					    "value = %u\n",
					    fw_slot_info->signing_key_index);)
			}
		} else if (tag == NSM_FIRMWARE_FIRMWARE_SLOT_ID) {
			/* we are good, we reached new firmware slot id */
			return NSM_SW_SUCCESS;
		} else {
			/* Not expected field before firmware slot count */
			DBGE(printf("Error: Decoded unsupported tag in slot "
				    "information: 0x%02x\n",
				    field->tag);)
			DBGE(printf("Left telemetries: %u, left payload: %u\n",
				    (uint32_t)(*telemetry_count),
				    (uint32_t)(*payload_size));)
			return NSM_SW_ERROR_DATA;
		}

		if (!rc_ok) {
			DBGE(printf("Error: The decoded message slot "
				    "information is not properly encoded\n");)
			DBGE(printf("Left telemetries: %u, left payload: %u\n",
				    (uint32_t)(*telemetry_count),
				    (uint32_t)(*payload_size));)
			return NSM_SW_ERROR_LENGTH;
		}
	}

	DBG2(printf("Success in decoding the response slot information\n");)
	return NSM_SW_SUCCESS;
}

int decode_nsm_query_get_erot_state_parameters_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_firmware_erot_state_info_resp *fw_resp)
{
	if (msg == NULL || fw_resp == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	DBG1(printf("Received Resp buffer (size = %zi):\n", msg_len);)
	DBG1(printArrayAsHex(&(msg->payload[0]), msg_len);)

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_firmware_aggregate_tag)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_firmware_get_erot_state_info_resp *resp =
	    (struct nsm_firmware_get_erot_state_info_resp *)msg->payload;

	uint8_t *ptr = &(resp->payload[0]);
	uint16_t telemetry_count = resp->hdr.telemetry_count;
	uint16_t payload_size = (uint16_t)msg_len - sizeof(struct nsm_msg_hdr) -
				sizeof(struct nsm_common_telemetry_resp);
	DBG2(printf("Telemetry count = %u, msg max size = %u:\n",
		    (uint32_t)telemetry_count, (uint32_t)payload_size);)

	int ret = decode_nsm_query_firmware_header_information(
	    &(fw_resp->fq_resp_hdr), &ptr, &payload_size, &telemetry_count);
	if (ret) {
		DBGE(printf("Full payload (len = %zi):\n", msg_len);)
		DBGE(printArrayAsHex((const uint8_t *)msg, msg_len);)
		return ret;
	}

	DBG2(printf("Payload remaining size = %u):\n", (uint32_t)payload_size);)
	if (fw_resp->fq_resp_hdr.firmware_slot_count > 0) {
		int slot_info_len = sizeof(struct nsm_firmware_slot_info) *
				    fw_resp->fq_resp_hdr.firmware_slot_count;
		fw_resp->slot_info = malloc(slot_info_len);
		memset((char *)(fw_resp->slot_info), 0, slot_info_len);

		for (int i = 0; i < fw_resp->fq_resp_hdr.firmware_slot_count;
		     i++) {
			int ret = decode_nsm_query_firmware_slot_information(
			    &(fw_resp->slot_info[i]), &ptr, &payload_size,
			    &telemetry_count);
			if (ret) {
				DBGE(printf("Full payload (len = %zi):\n",
					    msg_len);)
				DBGE(printArrayAsHex((const uint8_t *)msg,
						     msg_len);)
				return ret;
			}
		}
	} else {
		fw_resp->slot_info = NULL;
	}

	return NSM_SW_SUCCESS;
}
