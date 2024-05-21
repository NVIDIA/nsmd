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

#include "device-configuration.h"
#include "base.h"
#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_get_fpga_diagnostics_settings_req(
    uint8_t instance_id, enum fpga_diagnostics_settings_data_index data_index,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CONFIGURATION;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_fpga_diagnostics_settings_req *request =
	    (struct nsm_get_fpga_diagnostics_settings_req *)msg->payload;

	request->hdr.command = NSM_GET_FPGA_DIAGNOSTICS_SETTINGS;
	request->hdr.data_size = 1;
	request->data_index = data_index;

	return NSM_SW_SUCCESS;
}

int decode_get_fpga_diagnostics_settings_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum fpga_diagnostics_settings_data_index *data_index)
{
	if (msg == NULL || data_index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_fpga_diagnostics_settings_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_fpga_diagnostics_settings_req *request =
	    (struct nsm_get_fpga_diagnostics_settings_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(struct nsm_get_fpga_diagnostics_settings_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*data_index = request->data_index;
	return NSM_SW_SUCCESS;
}

int encode_get_fpga_diagnostics_settings_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      const uint16_t data_size,
					      uint8_t *data,
					      struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CONFIGURATION;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, msg);
	}

	struct nsm_get_fpga_diagnostics_settings_resp *resp =
	    (struct nsm_get_fpga_diagnostics_settings_resp *)msg->payload;

	resp->hdr.command = NSM_GET_FPGA_DIAGNOSTICS_SETTINGS;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(data_size);
	memcpy(resp->data, data, data_size);
	return NSM_SW_SUCCESS;
}

int decode_get_fpga_diagnostics_settings_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *data_size,
					      uint16_t *reason_code,
					      uint8_t *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_fpga_diagnostics_settings_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_fpga_diagnostics_settings_resp *resp =
	    (struct nsm_get_fpga_diagnostics_settings_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	memcpy(data, resp->data, *data_size);
	return NSM_SW_SUCCESS;
}

int encode_get_fpga_diagnostics_settings_wp_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_fpga_diagnostics_settings_wp *data, struct nsm_msg *msg)
{
	return encode_get_fpga_diagnostics_settings_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_fpga_diagnostics_settings_wp), (uint8_t *)data,
	    msg);
}

int decode_get_fpga_diagnostics_settings_wp_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_fpga_diagnostics_settings_wp *data)
{
	uint16_t data_size = 0;
	int ret = decode_get_fpga_diagnostics_settings_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size < sizeof(struct nsm_fpga_diagnostics_settings_wp))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_get_fpga_diagnostics_settings_wp_jumper_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_fpga_diagnostics_settings_wp_jumper *data, struct nsm_msg *msg)
{
	return encode_get_fpga_diagnostics_settings_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_fpga_diagnostics_settings_wp_jumper),
	    (uint8_t *)data, msg);
}

int decode_get_fpga_diagnostics_settings_wp_jumper_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_fpga_diagnostics_settings_wp_jumper *data)
{
	uint16_t data_size = 0;
	int ret = decode_get_fpga_diagnostics_settings_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size < sizeof(struct nsm_fpga_diagnostics_settings_wp_jumper))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}
