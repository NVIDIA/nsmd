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