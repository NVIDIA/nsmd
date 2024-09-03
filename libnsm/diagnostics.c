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

	memcpy(seg_data, resp->segment_data, *seg_data_size);

	return NSM_SW_SUCCESS;
}

int encode_erase_trace_req(uint8_t instance_id, uint8_t info_type,
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

	struct nsm_erase_trace_req *request =
	    (struct nsm_erase_trace_req *)msg->payload;

	request->hdr.command = NSM_ERASE_TRACE;
	request->hdr.data_size =
	    sizeof(struct nsm_erase_trace_req) - sizeof(struct nsm_common_req);
	request->info_type = info_type;
	request->reserved = 0x00;

	return NSM_SW_SUCCESS;
}

int decode_erase_trace_req(const struct nsm_msg *msg, size_t msg_len,
			   uint8_t *info_type)
{
	if (msg == NULL || info_type == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_erase_trace_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_erase_trace_req *request =
	    (struct nsm_erase_trace_req *)msg->payload;

	if (request->hdr.data_size != sizeof(struct nsm_erase_trace_req) -
					  sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*info_type = request->info_type;

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

	memcpy(log_data, resp->log_data, *log_data_size);

	return NSM_SW_SUCCESS;
}