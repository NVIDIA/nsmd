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

#include "diagnostics.h"
#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_get_device_diagnostics_req(uint8_t instance_id, uint8_t segment_id,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_device_diagnostics_req *request =
	    (struct nsm_get_device_diagnostics_req *)msg->payload;

	request->hdr.command = NSM_GET_DEVICE_DIAGNOSTICS;
	request->hdr.data_size = sizeof(struct nsm_get_device_diagnostics_req) -
				 sizeof(struct nsm_common_req);
	request->segment_id = segment_id;
	return NSM_SW_SUCCESS;
}

int decode_get_device_diagnostics_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *segment_id)
{
	if (msg == NULL || segment_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_get_device_diagnostics_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_device_diagnostics_req *request =
	    (struct nsm_get_device_diagnostics_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_get_device_diagnostics_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*segment_id = request->segment_id;

	return NSM_SW_SUCCESS;
}

int encode_get_device_diagnostics_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code,
				       const uint8_t *seg_data,
				       const uint16_t seg_data_size,
				       const uint8_t next_segment_id,
				       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_DEVICE_DIAGNOSTICS, msg);
	}

	struct nsm_get_device_diagnostics_resp *response =
	    (struct nsm_get_device_diagnostics_resp *)msg->payload;

	response->hdr.command = NSM_GET_DEVICE_DIAGNOSTICS;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(seg_data_size + sizeof(next_segment_id));
	response->next_segment_id = next_segment_id;
	memcpy(response->segment_data, seg_data, seg_data_size);
	return NSM_SW_SUCCESS;
}

int decode_get_device_diagnostics_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code, uint8_t *seg_data,
				       uint16_t *seg_data_size,
				       uint8_t *next_segment_id)
{
	if (seg_data == NULL || seg_data_size == NULL ||
	    next_segment_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_common_resp) +
			  sizeof(*next_segment_id)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_device_diagnostics_resp *response =
	    (struct nsm_get_device_diagnostics_resp *)msg->payload;

	if (response->hdr.data_size < sizeof(*next_segment_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*seg_data_size =
	    response->hdr.data_size - sizeof(response->next_segment_id);
	*next_segment_id = response->next_segment_id;
	if (sizeof(struct nsm_msg_hdr) +
		offsetof(struct nsm_get_device_diagnostics_resp, segment_data) +
		(size_t)*seg_data_size >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}
	memcpy(seg_data, response->segment_data, *seg_data_size);
	return NSM_SW_SUCCESS;
}

int encode_reset_enum_data(uint8_t resetType, uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*data = resetType; // Enum is 1 byte
	*data_len = sizeof(uint8_t);

	return NSM_SW_SUCCESS;
}

int encode_reset_count_data(uint16_t count, uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint16_t le_count = htole16(count);
	memcpy(data, &le_count, sizeof(uint16_t));
	*data_len = sizeof(uint16_t);

	return NSM_SW_SUCCESS;
}

int encode_reset_count_256data(const uint64_t *counter, uint8_t *data,
			       size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	for (size_t i = 0; i < 4; i++) {
		uint64_t le_count = htole64(counter[i]);
		memcpy(data + i * sizeof(uint64_t), &le_count,
		       sizeof(uint64_t));
	}
	*data_len = sizeof(uint64_t) * 4;

	return NSM_SW_SUCCESS;
}

int decode_reset_enum_data(const uint8_t *data, size_t data_len,
			   uint8_t *resetType)
{
	if (data == NULL || resetType == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*resetType = *data; // Enum is 1 byte
	return NSM_SW_SUCCESS;
}

int decode_reset_count_data(const uint8_t *data, size_t data_len,
			    uint16_t *count)
{
	if (data == NULL || count == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint16_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint16_t le_count;
	memcpy(&le_count, data, sizeof(uint16_t));
	*count = le16toh(le_count);

	return NSM_SW_SUCCESS;
}

int decode_reset_count_256data(const uint8_t *data, size_t data_len,
			       uint64_t *counter, size_t counter_len)
{
	if (data == NULL || counter == NULL || counter_len != 4) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint64_t) * counter_len) {
		return NSM_SW_ERROR_LENGTH;
	}

	for (size_t i = 0; i < counter_len; i++) {
		uint64_t le_count;
		memcpy(&le_count, data + i * sizeof(uint64_t),
		       sizeof(uint64_t));
		counter[i] = le64toh(le_count);
	}

	return NSM_SW_SUCCESS;
}

int encode_get_device_reset_statistics_req(uint8_t instance_id,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;
	request->command = NSM_GET_DEVICE_RESET_STATISTICS;
	request->data_size = 0; // No additional payload for the request

	return NSM_SW_SUCCESS;
}

int decode_get_device_reset_statistics_req(const struct nsm_msg *msg,
					   size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	// Validate message length
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	// Cast the payload to nsm_common_req structure
	const struct nsm_common_req *request =
	    (const struct nsm_common_req *)msg->payload;

	// Validate that the data_size field in the common request is 0
	// (indicating no extra data)
	if (request->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_reset_network_device_req(uint8_t instance_id, uint8_t mode,
				    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_reset_network_device_req *request =
	    (struct nsm_reset_network_device_req *)msg->payload;

	request->hdr.command = NSM_RESET_NETWORK_DEVICE;
	request->hdr.data_size = sizeof(mode);
	request->mode = mode;

	return NSM_SW_SUCCESS;
}

int decode_reset_network_device_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *mode)
{
	if (msg == NULL || mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_reset_network_device_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_reset_network_device_req *request =
	    (struct nsm_reset_network_device_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->mode)) {
		return NSM_SW_ERROR_DATA;
	}

	*mode = request->mode;

	return NSM_SW_SUCCESS;
}

int encode_reset_network_device_resp(uint8_t instance_id, uint16_t reason_code,
				     struct nsm_msg *msg)
{
	return encode_cc_only_resp(instance_id, NSM_TYPE_DIAGNOSTIC,
				   NSM_RESET_NETWORK_DEVICE, NSM_SUCCESS,
				   reason_code, msg);
}

int decode_reset_network_device_resp(const struct nsm_msg *msg, size_t msgLen,
				     uint8_t *cc, uint16_t *reason_code)
{
	int rc = decode_reason_code_and_cc(msg, msgLen, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msgLen < sizeof(struct nsm_msg_hdr) +
			 sizeof(nsm_reset_network_device_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_reset_network_device_resp *resp =
	    (nsm_reset_network_device_resp *)msg->payload;
	if (resp->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_device_reset_req(uint8_t instance_id, uint8_t reset_target,
			    uint8_t trigger, uint32_t port_index,
			    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_device_reset_req *request =
	    (struct nsm_device_reset_req *)msg->payload;

	request->hdr.command = NSM_DEVICE_RESET;
	request->hdr.data_size =
	    sizeof(request->reset_target) + sizeof(request->trigger) +
	    sizeof(request->reserved) + sizeof(request->port_index);
	request->reset_target = reset_target;
	request->trigger = trigger;
	request->reserved = 0;
	request->port_index = htole32(port_index);

	return NSM_SW_SUCCESS;
}

int decode_device_reset_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *reset_target, uint8_t *trigger,
			    uint32_t *port_index)
{
	if (msg == NULL || reset_target == NULL || trigger == NULL ||
	    port_index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_device_reset_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_device_reset_req *request =
	    (struct nsm_device_reset_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(request->reset_target) + sizeof(request->trigger) +
		sizeof(request->reserved) + sizeof(request->port_index)) {
		return NSM_SW_ERROR_DATA;
	}

	*reset_target = request->reset_target;
	*trigger = request->trigger;
	*port_index = le32toh(request->port_index);

	return NSM_SW_SUCCESS;
}

int encode_device_reset_resp(uint8_t instance_id, uint16_t reason_code,
			     struct nsm_msg *msg)
{
	return encode_cc_only_resp(instance_id, NSM_TYPE_DIAGNOSTIC,
				   NSM_DEVICE_RESET, NSM_SUCCESS, reason_code,
				   msg);
}

int decode_device_reset_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *reason_code)
{
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_device_reset_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_device_reset_resp *resp = (nsm_device_reset_resp *)msg->payload;
	if (resp->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_enable_disable_wp_req(
    uint8_t instance_id,
    enum diagnostics_enable_disable_wp_data_index data_index, uint8_t value,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_enable_disable_wp_req *request =
	    (struct nsm_enable_disable_wp_req *)msg->payload;

	request->hdr.command = NSM_ENABLE_DISABLE_WP;
	request->hdr.data_size = 2;
	request->data_index = data_index;
	request->value = value;

	return NSM_SW_SUCCESS;
}

int decode_enable_disable_wp_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum diagnostics_enable_disable_wp_data_index *data_index, uint8_t *value)
{
	if (msg == NULL || data_index == NULL || value == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_enable_disable_wp_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_enable_disable_wp_req *request =
	    (struct nsm_enable_disable_wp_req *)msg->payload;

	if (request->hdr.data_size < sizeof(struct nsm_enable_disable_wp_req) -
					 NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*data_index = request->data_index;
	*value = request->value;
	return NSM_SW_SUCCESS;
}

int encode_enable_disable_wp_resp(uint8_t instance_id, uint8_t cc,
				  uint16_t reason_code, struct nsm_msg *msg)
{
	return encode_common_resp(instance_id, cc, reason_code,
				  NSM_TYPE_DIAGNOSTIC, NSM_ENABLE_DISABLE_WP,
				  msg);
}

int decode_enable_disable_wp_resp(const struct nsm_msg *msg, size_t msg_len,
				  uint8_t *cc, uint16_t *reason_code)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (data_size != 0) {
		return NSM_SW_ERROR_LENGTH;
	}
	return rc;
}

int encode_get_network_device_debug_info_req(uint8_t instance_id,
					     uint8_t debug_type,
					     uint32_t handle,
					     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_network_device_debug_info_req *request =
	    (struct nsm_get_network_device_debug_info_req *)msg->payload;

	request->hdr.command = NSM_GET_NETWORK_DEVICE_DEBUG_INFO;
	request->hdr.data_size =
	    sizeof(struct nsm_get_network_device_debug_info_req) -
	    sizeof(struct nsm_common_req);
	request->debug_info_type = debug_type;
	request->reserved = 0x00;
	request->record_handle = htole32(handle);

	return NSM_SW_SUCCESS;
}

int decode_get_network_device_debug_info_req(const struct nsm_msg *msg,
					     size_t msg_len,
					     uint8_t *debug_type,
					     uint32_t *handle)
{
	if (msg == NULL || debug_type == NULL || handle == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_network_device_debug_info_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_network_device_debug_info_req *request =
	    (struct nsm_get_network_device_debug_info_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_get_network_device_debug_info_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*debug_type = request->debug_info_type;
	*handle = le32toh(request->record_handle);

	return NSM_SW_SUCCESS;
}

int encode_get_network_device_debug_info_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      const uint8_t *seg_data,
					      const uint16_t seg_data_size,
					      const uint32_t next_handle,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_NETWORK_DEVICE_DEBUG_INFO, msg);
	}

	struct nsm_get_network_device_debug_info_resp *resp =
	    (struct nsm_get_network_device_debug_info_resp *)msg->payload;

	resp->hdr.command = NSM_GET_NETWORK_DEVICE_DEBUG_INFO;
	resp->hdr.completion_code = cc;

	uint16_t total_data_size = seg_data_size + sizeof(next_handle);
	resp->hdr.data_size = htole16(total_data_size);
	resp->next_record_handle = htole32(next_handle);

	if (cc == NSM_SUCCESS) {
		{
			if (seg_data == NULL) {
				return NSM_SW_ERROR_NULL;
			}
		}
		memcpy(resp->segment_data, seg_data, seg_data_size);
	}

	return NSM_SW_SUCCESS;
}

int decode_get_network_device_debug_info_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *reason_code,
					      uint16_t *seg_data_size,
					      uint8_t *seg_data,
					      uint32_t *next_handle)
{
	if (seg_data == NULL || seg_data_size == NULL || next_handle == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_network_device_debug_info_resp) -
		sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_network_device_debug_info_resp *resp =
	    (struct nsm_get_network_device_debug_info_resp *)msg->payload;

	uint16_t total_data_size = le16toh(resp->hdr.data_size);
	*next_handle = le32toh(resp->next_record_handle);
	*seg_data_size = total_data_size - sizeof(resp->next_record_handle);

	if (sizeof(struct nsm_msg_hdr) +
		offsetof(struct nsm_get_network_device_debug_info_resp,
			 segment_data) +
		(size_t)*seg_data_size >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}
	memcpy(seg_data, resp->segment_data, *seg_data_size);

	return NSM_SW_SUCCESS;
}

int encode_erase_trace_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_erase_trace_req *request =
	    (struct nsm_erase_trace_req *)msg->payload;

	request->hdr.command = NSM_ERASE_TRACE;
	request->hdr.data_size = 0x00;

	return NSM_SW_SUCCESS;
}

int decode_erase_trace_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_erase_trace_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_erase_trace_req *request =
	    (struct nsm_erase_trace_req *)msg->payload;

	if (request->hdr.data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_erase_trace_resp(uint8_t instance_id, uint8_t cc,
			    uint16_t reason_code, uint8_t result_status,
			    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_ERASE_TRACE,
					  msg);
	}

	struct nsm_erase_trace_resp *response =
	    (struct nsm_erase_trace_resp *)msg->payload;

	response->hdr.command = NSM_ERASE_TRACE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(uint8_t));
	response->result_status = result_status;

	return NSM_SW_SUCCESS;
}

int decode_erase_trace_resp(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *cc, uint16_t *reason_code,
			    uint8_t *result_status)
{
	if (result_status == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_erase_trace_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_erase_trace_resp *response =
	    (struct nsm_erase_trace_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*result_status = response->result_status;

	return NSM_SW_SUCCESS;
}

int encode_get_network_device_log_info_req(uint8_t instance_id,
					   uint32_t record_handle,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_network_device_log_info_req *request =
	    (struct nsm_get_network_device_log_info_req *)msg->payload;

	request->hdr.command = NSM_GET_NETWORK_DEVICE_LOG_INFO;
	request->hdr.data_size =
	    sizeof(struct nsm_get_network_device_log_info_req) -
	    sizeof(struct nsm_common_req);
	request->record_handle = htole32(record_handle);

	return NSM_SW_SUCCESS;
}

int decode_get_network_device_log_info_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint32_t *record_handle)
{
	if (msg == NULL || record_handle == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_network_device_log_info_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_network_device_log_info_req *request =
	    (struct nsm_get_network_device_log_info_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_get_network_device_log_info_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*record_handle = le32toh(request->record_handle);

	return NSM_SW_SUCCESS;
}

int encode_get_network_device_log_info_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const uint32_t next_handle,
    struct nsm_device_log_info_breakdown log_info_breakdown,
    const uint8_t *log_data, const uint16_t log_data_size, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_NETWORK_DEVICE_LOG_INFO, msg);
	}

	struct nsm_get_network_device_log_info_resp *resp =
	    (struct nsm_get_network_device_log_info_resp *)msg->payload;

	resp->hdr.command = NSM_GET_NETWORK_DEVICE_LOG_INFO;
	resp->hdr.completion_code = cc;

	uint16_t total_data_size = log_data_size + sizeof(next_handle) +
				   sizeof(struct nsm_device_log_info);
	resp->hdr.data_size = htole16(total_data_size);
	resp->next_record_handle = htole32(next_handle);

	struct nsm_device_log_info *log_info =
	    (struct nsm_device_log_info *)(&log_info_breakdown);
	resp->log_info.lost_events_and_synced_time =
	    log_info->lost_events_and_synced_time;
	resp->log_info.reserved1 = log_info->reserved1;
	resp->log_info.reserved2 = log_info->reserved2;
	resp->log_info.time_low = htole32(log_info->time_low);
	resp->log_info.time_high = htole32(log_info->time_high);
	resp->log_info.entry_prefix_and_length =
	    htole32(log_info->entry_prefix_and_length);
	resp->log_info.entry_suffix = htole64(log_info->entry_suffix);

	if (cc == NSM_SUCCESS) {
		{
			if (log_data == NULL) {
				return NSM_SW_ERROR_NULL;
			}
		}
		memcpy(resp->log_data, log_data, log_data_size);
	}

	return NSM_SW_SUCCESS;
}

int decode_get_network_device_log_info_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint32_t *next_handle,
    struct nsm_device_log_info_breakdown *log_info, uint8_t *log_data,
    uint16_t *log_data_size)
{
	if (log_info == NULL || log_data == NULL || log_data_size == NULL ||
	    next_handle == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_network_device_log_info_resp) -
			  sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_network_device_log_info_resp *resp =
	    (struct nsm_get_network_device_log_info_resp *)msg->payload;

	uint16_t total_data_size = le16toh(resp->hdr.data_size);
	*next_handle = le32toh(resp->next_record_handle);
	*log_data_size = total_data_size - sizeof(resp->next_record_handle) -
			 sizeof(struct nsm_device_log_info);

	struct nsm_device_log_info info = {0};
	info.lost_events_and_synced_time =
	    resp->log_info.lost_events_and_synced_time;
	info.reserved1 = 0;
	info.reserved2 = 0;
	info.time_low = le32toh(resp->log_info.time_low);
	info.time_high = le32toh(resp->log_info.time_high);
	info.entry_prefix_and_length =
	    le32toh(resp->log_info.entry_prefix_and_length);
	info.entry_suffix = le64toh(resp->log_info.entry_suffix);
	memcpy(log_info, &info, sizeof(struct nsm_device_log_info));

	if (sizeof(struct nsm_msg_hdr) +
		offsetof(struct nsm_get_network_device_log_info_resp,
			 log_data) +
		(size_t)*log_data_size >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}
	memcpy(log_data, resp->log_data, *log_data_size);

	return NSM_SW_SUCCESS;
}

int encode_erase_debug_info_req(uint8_t instance_id, uint8_t info_type,
				struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_erase_debug_info_req *request =
	    (struct nsm_erase_debug_info_req *)msg->payload;

	request->hdr.command = NSM_ERASE_DEBUG_INFO;
	request->hdr.data_size = sizeof(struct nsm_erase_debug_info_req) -
				 sizeof(struct nsm_common_req);
	request->debug_info_type = info_type;
	request->reserved = 0;

	return NSM_SW_SUCCESS;
}

int decode_erase_debug_info_req(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *info_type)
{
	if (msg == NULL || info_type == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_erase_debug_info_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_erase_debug_info_req *request =
	    (struct nsm_erase_debug_info_req *)msg->payload;

	if (request->hdr.data_size != sizeof(struct nsm_erase_debug_info_req) -
					  sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*info_type = request->debug_info_type;

	return NSM_SW_SUCCESS;
}

int encode_erase_debug_info_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint8_t result_status,
				 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_ERASE_TRACE,
					  msg);
	}

	struct nsm_erase_debug_info_resp *response =
	    (struct nsm_erase_debug_info_resp *)msg->payload;

	response->hdr.command = NSM_ERASE_DEBUG_INFO;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_erase_debug_info_resp) -
		    sizeof(struct nsm_common_resp));
	response->result_status = result_status;
	response->reserved = 0;

	return NSM_SW_SUCCESS;
}

int decode_erase_debug_info_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint8_t *result_status)
{
	if (result_status == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_erase_debug_info_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_erase_debug_info_resp *response =
	    (struct nsm_erase_debug_info_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(struct nsm_erase_debug_info_resp) -
			     sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_DATA;
	}

	*result_status = response->result_status;

	return NSM_SW_SUCCESS;
}

int encode_get_device_debug_parameters_req(
    uint8_t instance_id, uint8_t debug_configuration_type,
    struct nsm_debug_parameter_id parameter_id,
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint8_t rc = encode_common_req_v2(instance_id, NSM_TYPE_DIAGNOSTIC,
					  NSM_GET_DEVICE_DEBUG_PARAMETERS, msg);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	struct nsm_get_device_debug_parameters_req *request =
	    (struct nsm_get_device_debug_parameters_req *)msg->payload;
	request->hdr.data_size =
	    sizeof(struct nsm_get_device_debug_parameters_req) -
	    sizeof(struct nsm_common_req_v2);
	request->debug_configuration_type = debug_configuration_type;
	request->reserved[0] = 0;
	request->reserved[1] = 0;
	request->reserved[2] = 0;
	request->parameter_id.reserved = 0;
	request->parameter_id.port_number = htole16(parameter_id.port_number);
	request->parameter_id.index = parameter_id.index;
	request->parameter_sub_id.value = htole32(parameter_sub_id.value);
	return NSM_SW_SUCCESS;
}

int decode_get_device_debug_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    uint8_t *debug_configuration_type,
    struct nsm_debug_parameter_id *parameter_id,
    nsm_debug_parameter_sub_id_bitfield *parameter_sub_id)
{
	if (msg == NULL || debug_configuration_type == NULL ||
	    parameter_id == NULL || parameter_sub_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_common_req_v2(msg, msg_len);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	const struct nsm_get_device_debug_parameters_req *request =
	    (const struct nsm_get_device_debug_parameters_req *)msg->payload;
	if (request->reserved[0] != 0 || request->reserved[1] != 0 ||
	    request->reserved[2] != 0 || request->parameter_id.reserved != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*debug_configuration_type = request->debug_configuration_type;
	parameter_id->port_number = le16toh(request->parameter_id.port_number);
	parameter_id->reserved = request->parameter_id.reserved;
	parameter_id->index = request->parameter_id.index;
	parameter_sub_id->value = le32toh(request->parameter_sub_id.value);
	return NSM_SW_SUCCESS;
}

int encode_get_device_debug_parameters_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    uint16_t *data_size, uint8_t *data,
					    struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL || data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_DEVICE_DEBUG_PARAMETERS, msg);
	}

	struct nsm_get_device_debug_parameters_resp *resp =
	    (struct nsm_get_device_debug_parameters_resp *)msg->payload;
	resp->hdr.command = NSM_GET_DEVICE_DEBUG_PARAMETERS;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(*data_size);
	memcpy(resp->data, data, *data_size);
	return NSM_SW_SUCCESS;
}

int decode_get_device_debug_parameters_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *data_size, uint8_t *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_device_debug_parameters_resp) -
			  1) {
		return NSM_SW_ERROR_LENGTH;
	}

	const struct nsm_get_device_debug_parameters_resp *resp =
	    (const struct nsm_get_device_debug_parameters_resp *)msg->payload;

	*cc = resp->hdr.completion_code;
	*data_size = le16toh(resp->hdr.data_size);

	// Validate data_size against available message buffer
	size_t available_data_size = msg_len - sizeof(struct nsm_msg_hdr) -
				     sizeof(struct nsm_common_resp);
	if (*data_size > available_data_size) {
		return NSM_SW_ERROR_LENGTH;
	}

	memcpy(data, resp->data, *data_size);
	return NSM_SW_SUCCESS;
}

int encode_set_device_debug_parameters_req(
    uint8_t instance_id, uint8_t debug_configuration_type,
    struct nsm_debug_parameter_id parameter_id,
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id, uint8_t data_size,
    uint8_t *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint8_t rc = encode_common_req_v2(instance_id, NSM_TYPE_DIAGNOSTIC,
					  NSM_SET_DEVICE_DEBUG_PARAMETERS, msg);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_device_debug_parameters_req *request =
	    (struct nsm_set_device_debug_parameters_req *)msg->payload;
	request->hdr.data_size =
	    sizeof(struct nsm_set_device_debug_parameters_req) -
	    sizeof(struct nsm_common_req_v2) + data_size - sizeof(uint8_t);
	request->debug_configuration_type = debug_configuration_type;
	request->data_size = data_size;
	request->reserved[0] = 0;
	request->reserved[1] = 0;
	request->parameter_id.reserved = 0;
	request->parameter_id.port_number = htole16(parameter_id.port_number);
	request->parameter_id.index = parameter_id.index;
	request->parameter_sub_id.value = htole32(parameter_sub_id.value);
	memcpy(request->data, data, data_size);
	return NSM_SW_SUCCESS;
}

int decode_set_device_debug_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    uint8_t *debug_configuration_type,
    struct nsm_debug_parameter_id *parameter_id,
    nsm_debug_parameter_sub_id_bitfield *parameter_sub_id, uint8_t *data_size,
    uint8_t **data)
{
	if (msg == NULL || debug_configuration_type == NULL ||
	    parameter_id == NULL || parameter_sub_id == NULL ||
	    data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_common_req_v2(msg, msg_len);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		offsetof(struct nsm_set_device_debug_parameters_req, data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	const struct nsm_set_device_debug_parameters_req *request =
	    (const struct nsm_set_device_debug_parameters_req *)msg->payload;

	if (request->reserved[0] != 0 || request->reserved[1] != 0 ||
	    request->parameter_id.reserved != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*debug_configuration_type = request->debug_configuration_type;
	parameter_id->port_number = le16toh(request->parameter_id.port_number);
	parameter_id->reserved = request->parameter_id.reserved;
	parameter_id->index = request->parameter_id.index;
	parameter_sub_id->value = le32toh(request->parameter_sub_id.value);
	*data_size = request->data_size;
	if (sizeof(struct nsm_msg_hdr) +
		offsetof(struct nsm_set_device_debug_parameters_req, data) +
		(size_t)*data_size >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}
	*data = (uint8_t *)request->data;
	return NSM_SW_SUCCESS;
}

int decode_set_device_debug_parameters_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc)
{
	if (msg == NULL || cc == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	const struct nsm_common_resp *resp =
	    (const struct nsm_common_resp *)msg->payload;
	*cc = resp->completion_code;
	return NSM_SW_SUCCESS;
}

/*
 * Vera CPU Pre-Boot Diagnostics - Event encode/decode
 */

int encode_nsm_diag_get_system_config_event(uint8_t instance_id, bool ackr,
					    uint8_t config_type,
					    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint8_t event_data[1];
	event_data[0] = config_type;

	return encode_nsm_event(
	    instance_id, NSM_TYPE_DIAGNOSTIC, ackr,
	    NSM_DIAG_PREBOOT_EVENT_VERSION, NSM_DIAG_GET_SYSTEM_CONFIG_EVENT,
	    NSM_GENERAL_EVENT_CLASS, 0, sizeof(event_data), event_data, msg);
}

int decode_nsm_diag_get_system_config_event(const struct nsm_msg *msg,
					    size_t msg_len,
					    uint8_t *event_class,
					    uint16_t *event_state,
					    uint8_t *config_type)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    config_type == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint8_t data_size = 0;
	int rc =
	    decode_nsm_event(msg, msg_len, NSM_DIAG_GET_SYSTEM_CONFIG_EVENT,
			     NSM_GENERAL_EVENT_CLASS, event_state, &data_size);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (data_size < sizeof(struct nsm_diag_get_system_config_event_data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	*event_class = event->event_class;

	const struct nsm_diag_get_system_config_event_data *data =
	    (const struct nsm_diag_get_system_config_event_data *)event->data;
	*config_type = data->config_type;

	return NSM_SW_SUCCESS;
}

int encode_nsm_diag_get_tid_config_event(uint8_t instance_id, bool ackr,
					 uint8_t tid, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint8_t event_data[1];
	event_data[0] = tid;

	return encode_nsm_event(
	    instance_id, NSM_TYPE_DIAGNOSTIC, ackr,
	    NSM_DIAG_PREBOOT_EVENT_VERSION, NSM_DIAG_GET_TID_CONFIG_EVENT,
	    NSM_GENERAL_EVENT_CLASS, 0, sizeof(event_data), event_data, msg);
}

int decode_nsm_diag_get_tid_config_event(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *event_class,
					 uint16_t *event_state, uint8_t *tid)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    tid == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint8_t data_size = 0;
	int rc =
	    decode_nsm_event(msg, msg_len, NSM_DIAG_GET_TID_CONFIG_EVENT,
			     NSM_GENERAL_EVENT_CLASS, event_state, &data_size);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (data_size < sizeof(struct nsm_diag_get_tid_config_event_data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	*event_class = event->event_class;

	const struct nsm_diag_get_tid_config_event_data *data =
	    (const struct nsm_diag_get_tid_config_event_data *)event->data;
	*tid = data->tid;

	return NSM_SW_SUCCESS;
}

int encode_nsm_diag_set_test_result_event(uint8_t instance_id, bool ackr,
					  uint8_t tid, uint16_t test_error_code,
					  uint8_t dynamic_data_size,
					  const uint8_t *dynamic_data,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (dynamic_data_size > 0 && dynamic_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (dynamic_data_size > NSM_DIAG_MAX_DYNAMIC_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint8_t event_data[NSM_EVENT_DATA_MAX_LEN];
	size_t fixed_size =
	    sizeof(struct nsm_diag_set_test_result_event_data) - 1;
	size_t total_size = fixed_size + dynamic_data_size;

	if (total_size > NSM_EVENT_DATA_MAX_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_diag_set_test_result_event_data *data =
	    (struct nsm_diag_set_test_result_event_data *)event_data;
	data->tid = tid;
	data->test_error_code = htole16(test_error_code);
	data->dynamic_data_size = dynamic_data_size;

	if (dynamic_data_size > 0) {
		memcpy(data->dynamic_data, dynamic_data, dynamic_data_size);
	}

	return encode_nsm_event(
	    instance_id, NSM_TYPE_DIAGNOSTIC, ackr,
	    NSM_DIAG_PREBOOT_EVENT_VERSION, NSM_DIAG_SET_TEST_RESULT_EVENT,
	    NSM_GENERAL_EVENT_CLASS, 0, total_size, event_data, msg);
}

int decode_nsm_diag_set_test_result_event(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *event_class,
					  uint16_t *event_state, uint8_t *tid,
					  uint16_t *test_error_code,
					  uint8_t *dynamic_data_size,
					  uint8_t *dynamic_data)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    tid == NULL || test_error_code == NULL ||
	    dynamic_data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint8_t data_size = 0;
	int rc =
	    decode_nsm_event(msg, msg_len, NSM_DIAG_SET_TEST_RESULT_EVENT,
			     NSM_GENERAL_EVENT_CLASS, event_state, &data_size);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	size_t fixed_size =
	    sizeof(struct nsm_diag_set_test_result_event_data) - 1;
	if (data_size < fixed_size) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	*event_class = event->event_class;

	const struct nsm_diag_set_test_result_event_data *data =
	    (const struct nsm_diag_set_test_result_event_data *)event->data;

	*tid = data->tid;
	*test_error_code = le16toh(data->test_error_code);
	*dynamic_data_size = data->dynamic_data_size;

	/* Spec §5.3 — DynamicDataSize is bounded to [0, 251]. Reject any
	 * value above the protocol cap as a protocol violation, before
	 * any further processing. Without this check, a crafted event with
	 * dynamic_data_size up to 254 (still within the wire-level data_size
	 * upper bound of 255) could overflow a caller's stack buffer. */
	if (*dynamic_data_size > NSM_DIAG_MAX_DYNAMIC_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	if (*dynamic_data_size > data_size - fixed_size) {
		return NSM_SW_ERROR_DATA;
	}

	if (*dynamic_data_size > 0) {
		if (dynamic_data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		memcpy(dynamic_data, data->dynamic_data, *dynamic_data_size);
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_diag_set_flow_control_event(uint8_t instance_id, bool ackr,
					   uint8_t flow_ctrl_status,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint8_t event_data[1];
	event_data[0] = flow_ctrl_status;

	return encode_nsm_event(
	    instance_id, NSM_TYPE_DIAGNOSTIC, ackr,
	    NSM_DIAG_PREBOOT_EVENT_VERSION, NSM_DIAG_SET_FLOW_CONTROL_EVENT,
	    NSM_GENERAL_EVENT_CLASS, 0, sizeof(event_data), event_data, msg);
}

int decode_nsm_diag_set_flow_control_event(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *event_class,
					   uint16_t *event_state,
					   uint8_t *flow_ctrl_status)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    flow_ctrl_status == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint8_t data_size = 0;
	int rc =
	    decode_nsm_event(msg, msg_len, NSM_DIAG_SET_FLOW_CONTROL_EVENT,
			     NSM_GENERAL_EVENT_CLASS, event_state, &data_size);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (data_size < sizeof(struct nsm_diag_set_flow_control_event_data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;
	*event_class = event->event_class;

	const struct nsm_diag_set_flow_control_event_data *data =
	    (const struct nsm_diag_set_flow_control_event_data *)event->data;
	*flow_ctrl_status = data->flow_ctrl_status;

	return NSM_SW_SUCCESS;
}

/*
 * Vera CPU Pre-Boot Diagnostics - Command encode/decode
 */

int encode_nsm_diag_set_system_config_req(
    uint8_t instance_id, uint8_t config_type, uint8_t system_test_duration,
    const uint8_t *dynamic_data, uint8_t dynamic_data_size, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (dynamic_data_size > 0 && dynamic_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (dynamic_data_size > NSM_DIAG_MAX_DYNAMIC_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_diag_set_system_config_req *request =
	    (struct nsm_diag_set_system_config_req *)msg->payload;

	request->hdr.command = NSM_DIAG_SET_SYSTEM_CONFIG;
	request->hdr.data_size = sizeof(config_type) +
				 sizeof(system_test_duration) +
				 dynamic_data_size;
	request->config_type = config_type;
	request->system_test_duration = system_test_duration;

	if (dynamic_data_size > 0) {
		memcpy(request->dynamic_data, dynamic_data, dynamic_data_size);
	}

	return NSM_SW_SUCCESS;
}

int decode_nsm_diag_set_system_config_req(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *config_type,
					  uint8_t *system_test_duration,
					  uint8_t *dynamic_data_size,
					  uint8_t *dynamic_data)
{
	if (msg == NULL || config_type == NULL ||
	    system_test_duration == NULL || dynamic_data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	size_t min_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_diag_set_system_config_req) - 1;
	if (msg_len < min_len) {
		return NSM_SW_ERROR_LENGTH;
	}

	const struct nsm_diag_set_system_config_req *request =
	    (const struct nsm_diag_set_system_config_req *)msg->payload;

	*config_type = request->config_type;
	*system_test_duration = request->system_test_duration;

	uint8_t fixed_fields_size = sizeof(request->config_type) +
				    sizeof(request->system_test_duration);
	if (request->hdr.data_size < fixed_fields_size) {
		return NSM_SW_ERROR_DATA;
	}

	*dynamic_data_size = request->hdr.data_size - fixed_fields_size;

	/* Wire format carries no explicit dynamic size — it is derived from
	 * hdr.data_size, which can encode up to 253 dynamic bytes while
	 * callers provide NSM_DIAG_MAX_DYNAMIC_DATA_SIZE-byte buffers.
	 * Reject anything above the protocol cap before copying. */
	if (*dynamic_data_size > NSM_DIAG_MAX_DYNAMIC_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	if (*dynamic_data_size > 0) {
		if (dynamic_data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		size_t available = msg_len - min_len;
		if (*dynamic_data_size > available) {
			return NSM_SW_ERROR_LENGTH;
		}
		memcpy(dynamic_data, request->dynamic_data, *dynamic_data_size);
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_diag_set_tid_config_req(
    uint8_t instance_id, uint8_t tid, uint8_t tid_test_duration, uint16_t loops,
    uint8_t console_log_level, uint8_t dynamic_data_size,
    const uint8_t *dynamic_data, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (dynamic_data_size > 0 && dynamic_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	/* Design §5.2.3 — TID command total must fit in 256-byte budget.
	 * Tighter cap than the event-side 251 because the TID command also
	 * carries 6 fixed bytes (tid + duration + loops + log + dyn_size). */
	if (dynamic_data_size > NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_diag_set_tid_config_req *request =
	    (struct nsm_diag_set_tid_config_req *)msg->payload;

	request->hdr.command = NSM_DIAG_SET_TID_CONFIG;
	request->hdr.data_size =
	    sizeof(request->tid) + sizeof(request->tid_test_duration) +
	    sizeof(request->loops) + sizeof(request->console_log_level) +
	    sizeof(request->dynamic_data_size) + dynamic_data_size;
	request->tid = tid;
	request->tid_test_duration = tid_test_duration;
	request->loops = htole16(loops);
	request->console_log_level = console_log_level;
	request->dynamic_data_size = dynamic_data_size;

	if (dynamic_data_size > 0) {
		memcpy(request->dynamic_data, dynamic_data, dynamic_data_size);
	}

	return NSM_SW_SUCCESS;
}

int decode_nsm_diag_set_tid_config_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *tid,
    uint8_t *tid_test_duration, uint16_t *loops, uint8_t *console_log_level,
    uint8_t *dynamic_data_size, uint8_t *dynamic_data)
{
	if (msg == NULL || tid == NULL || tid_test_duration == NULL ||
	    loops == NULL || console_log_level == NULL ||
	    dynamic_data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	size_t min_len = sizeof(struct nsm_msg_hdr) +
			 sizeof(struct nsm_diag_set_tid_config_req) - 1;
	if (msg_len < min_len) {
		return NSM_SW_ERROR_LENGTH;
	}

	const struct nsm_diag_set_tid_config_req *request =
	    (const struct nsm_diag_set_tid_config_req *)msg->payload;

	*tid = request->tid;
	*tid_test_duration = request->tid_test_duration;
	*loops = le16toh(request->loops);
	*console_log_level = request->console_log_level;
	*dynamic_data_size = request->dynamic_data_size;

	/* Same protocol-cap guard as the system-config decoder: the wire
	 * byte can claim up to 255 dynamic bytes, above both the TID cap
	 * (244) and typical caller buffers. Reject before copying. */
	if (*dynamic_data_size > NSM_DIAG_MAX_TID_DYNAMIC_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	if (*dynamic_data_size > 0) {
		if (dynamic_data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		size_t available = msg_len - min_len;
		if (*dynamic_data_size > available) {
			return NSM_SW_ERROR_LENGTH;
		}
		memcpy(dynamic_data, request->dynamic_data, *dynamic_data_size);
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_runtime_ist_complete_event(
    uint8_t instance_id, bool ackr, uint16_t event_state,
    const struct nsm_runtime_ist_complete_event_payload *payload,
    struct nsm_msg *msg)
{
	if (payload == NULL || msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_runtime_ist_complete_event_payload wire = *payload;

	wire.timestamp = htole64(wire.timestamp);
	wire.status_code = htole64(wire.status_code);
	wire.max_temperature = (int32_t)htole32((uint32_t)wire.max_temperature);
	wire.avg_temperature = (int32_t)htole32((uint32_t)wire.avg_temperature);

	uint8_t event_data[NSM_EVENT_DATA_MAX_LEN];
	memcpy(event_data, &wire, sizeof(wire));

	return encode_nsm_event(
	    instance_id, NSM_TYPE_DIAGNOSTIC, ackr, NSM_EVENT_VERSION_V1,
	    NSM_RUNTIME_IST_COMPLETE_EVENT, NSM_GENERAL_EVENT_CLASS,
	    event_state, sizeof(wire), event_data, msg);
}

int decode_nsm_runtime_ist_complete_event(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *event_class,
    uint16_t *event_state,
    struct nsm_runtime_ist_complete_event_payload *payload)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    payload == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;

	/* RIST payload is fixed-length, so the wire frame's data_size must
	 * match the bytes actually present in msg_len exactly (no trailing
	 * bytes, no missing bytes), and that size must equal sizeof(*payload).
	 */
	if (event->data_size !=
	    msg_len - sizeof(struct nsm_msg_hdr) - NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	if (event->data_size != sizeof(*payload)) {
		return NSM_SW_ERROR_DATA;
	}

	*event_class = event->event_class;
	*event_state = le16toh(event->event_state);

	memcpy(payload, event->data, sizeof(*payload));

	payload->timestamp = le64toh(payload->timestamp);
	payload->status_code = le64toh(payload->status_code);
	payload->max_temperature =
	    (int32_t)le32toh((uint32_t)payload->max_temperature);
	payload->avg_temperature =
	    (int32_t)le32toh((uint32_t)payload->avg_temperature);

	return NSM_SW_SUCCESS;
}