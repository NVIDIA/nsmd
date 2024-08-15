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