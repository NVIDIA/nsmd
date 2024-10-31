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

int encode_set_error_injection_mode_v1_req(uint8_t instance_id,
					   const uint8_t mode,
					   struct nsm_msg *msg)
{
	int rc = encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				   NSM_SET_ERROR_INJECTION_MODE_V1, msg);
	if (rc == NSM_SW_SUCCESS) {
		struct nsm_set_error_injection_mode_v1_req *req =
		    (struct nsm_set_error_injection_mode_v1_req *)msg->payload;
		req->hdr.data_size = htole16(sizeof(uint8_t));
		req->mode = mode;
	}
	return rc;
}

int decode_set_error_injection_mode_v1_req(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *mode)
{
	int rc = decode_common_req(msg, msg_len);
	if (rc == NSM_SW_SUCCESS) {
		if (mode == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		if (msg_len <
		    sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_set_error_injection_mode_v1_req)) {
			return NSM_SW_ERROR_LENGTH;
		}
		struct nsm_set_error_injection_mode_v1_req *req =
		    (struct nsm_set_error_injection_mode_v1_req *)msg->payload;

		if (req->hdr.data_size != sizeof(uint8_t)) {
			return NSM_SW_ERROR_LENGTH;
		}
		*mode = req->mode;
	}
	return rc;
}

int encode_set_error_injection_mode_v1_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg)
{
	return encode_common_resp(instance_id, cc, reason_code,
				  NSM_TYPE_DEVICE_CONFIGURATION,
				  NSM_SET_ERROR_INJECTION_MODE_V1, msg);
}

int decode_set_error_injection_mode_v1_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (data_size != 0) {
		return NSM_SW_ERROR_LENGTH;
	}
	return rc;
}

int encode_get_error_injection_mode_v1_req(uint8_t instance_id,
					   struct nsm_msg *msg)
{
	return encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_GET_ERROR_INJECTION_MODE_V1, msg);
}

int decode_get_error_injection_mode_v1_req(const struct nsm_msg *msg,
					   size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_error_injection_mode_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_error_injection_mode_v1 *data, struct nsm_msg *msg)
{
	int rc = encode_common_resp(instance_id, cc, reason_code,
				    NSM_TYPE_DEVICE_CONFIGURATION,
				    NSM_GET_ERROR_INJECTION_MODE_V1, msg);
	if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		struct nsm_get_error_injection_mode_v1_resp *resp =
		    (struct nsm_get_error_injection_mode_v1_resp *)msg->payload;
		resp->hdr.data_size =
		    htole16(sizeof(struct nsm_error_injection_mode_v1));
		resp->data.mode = data->mode;
		resp->data.flags.byte = htole32(data->flags.byte);
	}
	return rc;
}

int decode_get_error_injection_mode_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_error_injection_mode_v1 *data)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (rc == NSM_SW_SUCCESS && *cc == NSM_SUCCESS) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		if (msg_len <
		    sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_get_error_injection_mode_v1_resp)) {
			return NSM_SW_ERROR_LENGTH;
		}
		if (data_size != sizeof(struct nsm_error_injection_mode_v1)) {
			return NSM_SW_ERROR_LENGTH;
		}
		struct nsm_get_error_injection_mode_v1_resp *resp =
		    (struct nsm_get_error_injection_mode_v1_resp *)msg->payload;
		data->mode = resp->data.mode;
		data->flags.byte = le32toh(resp->data.flags.byte);
	}
	return rc;
}

int encode_set_current_error_injection_types_v1_req(
    uint8_t instance_id, const struct nsm_error_injection_types_mask *data,
    struct nsm_msg *msg)
{
	int rc =
	    encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
			      NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1, msg);
	if (rc == NSM_SW_SUCCESS) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		struct nsm_set_error_injection_types_mask_req *req =
		    (struct nsm_set_error_injection_types_mask_req *)
			msg->payload;
		req->hdr.data_size =
		    htole16(sizeof(struct nsm_error_injection_types_mask));
		req->data = *data;
	}
	return rc;
}

int decode_set_current_error_injection_types_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_error_injection_types_mask *data)
{
	int rc = decode_common_req(msg, msg_len);
	if (rc == NSM_SW_SUCCESS) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		if (msg_len <
		    sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_set_error_injection_types_mask_req)) {
			return NSM_SW_ERROR_LENGTH;
		}
		struct nsm_set_error_injection_types_mask_req *req =
		    (struct nsm_set_error_injection_types_mask_req *)
			msg->payload;
		if (req->hdr.data_size !=
		    sizeof(struct nsm_error_injection_types_mask)) {
			return NSM_SW_ERROR_LENGTH;
		}
		*data = req->data;
	}
	return rc;
}

int encode_set_current_error_injection_types_v1_resp(uint8_t instance_id,
						     uint8_t cc,
						     uint16_t reason_code,
						     struct nsm_msg *msg)
{
	return encode_common_resp(
	    instance_id, cc, reason_code, NSM_TYPE_DEVICE_CONFIGURATION,
	    NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1, msg);
}

int decode_set_current_error_injection_types_v1_resp(const struct nsm_msg *msg,
						     size_t msg_len,
						     uint8_t *cc,
						     uint16_t *reason_code)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (data_size != 0) {
		return NSM_SW_ERROR_NULL;
	}
	return rc;
}

int encode_get_supported_error_injection_types_v1_req(uint8_t instance_id,
						      struct nsm_msg *msg)
{
	return encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1,
				 msg);
}

int encode_get_current_error_injection_types_v1_req(uint8_t instance_id,
						    struct nsm_msg *msg)
{
	return encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1, msg);
}

int decode_get_error_injection_types_v1_req(const struct nsm_msg *msg,
					    size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

static int encode_get_error_injection_types_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code, uint8_t command,
    const struct nsm_error_injection_types_mask *data, struct nsm_msg *msg)
{
	int rc =
	    encode_common_resp(instance_id, cc, reason_code,
			       NSM_TYPE_DEVICE_CONFIGURATION, command, msg);
	if (rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		struct nsm_get_error_injection_types_mask_resp *resp =
		    (struct nsm_get_error_injection_types_mask_resp *)
			msg->payload;
		resp->hdr.data_size =
		    htole16(sizeof(struct nsm_error_injection_types_mask));
		resp->data = *data;
	}
	return rc;
}

int encode_get_supported_error_injection_types_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_error_injection_types_mask *data, struct nsm_msg *msg)
{
	return encode_get_error_injection_types_v1_resp(
	    instance_id, cc, reason_code,
	    NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1, data, msg);
}

int encode_get_current_error_injection_types_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_error_injection_types_mask *data, struct nsm_msg *msg)
{
	return encode_get_error_injection_types_v1_resp(
	    instance_id, cc, reason_code,
	    NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1, data, msg);
}

int decode_get_error_injection_types_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_error_injection_types_mask *data)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (rc == NSM_SW_SUCCESS && *cc == NSM_SUCCESS) {
		if (data == NULL) {
			return NSM_SW_ERROR_NULL;
		}
		if (msg_len <
		    sizeof(struct nsm_msg_hdr) +
			sizeof(
			    struct nsm_get_error_injection_types_mask_resp)) {
			return NSM_SW_ERROR_LENGTH;
		}
		if (data_size !=
		    sizeof(struct nsm_error_injection_types_mask)) {
			return NSM_SW_ERROR_LENGTH;
		}
		struct nsm_get_error_injection_types_mask_resp *resp =
		    (struct nsm_get_error_injection_types_mask_resp *)
			msg->payload;
		*data = resp->data;
	}
	return rc;
}

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

int encode_get_power_supply_status_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint8_t power_supply_status,
					struct nsm_msg *msg)
{
	return encode_get_fpga_diagnostics_settings_resp(
	    instance_id, cc, reason_code, sizeof(uint8_t), &power_supply_status,
	    msg);
}

int decode_get_power_supply_status_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					uint8_t *power_supply_status)
{
	uint16_t data_size = 0;
	int ret = decode_get_fpga_diagnostics_settings_resp(
	    msg, msg_len, cc, &data_size, reason_code,
	    (uint8_t *)power_supply_status);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size < sizeof(uint8_t))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_get_gpu_presence_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint8_t presence,
				 struct nsm_msg *msg)
{
	return encode_get_fpga_diagnostics_settings_resp(
	    instance_id, cc, reason_code, sizeof(uint8_t), &presence, msg);
}

int decode_get_gpu_presence_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint8_t *presence)
{
	uint16_t data_size = 0;
	int ret = decode_get_fpga_diagnostics_settings_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)presence);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size < sizeof(uint8_t))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_get_gpu_power_status_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, uint8_t power_status,
				     struct nsm_msg *msg)
{
	return encode_get_fpga_diagnostics_settings_resp(
	    instance_id, cc, reason_code, sizeof(uint8_t), &power_status, msg);
}

int decode_get_gpu_power_status_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code,
				     uint8_t *power_status)
{
	uint16_t data_size = 0;
	int ret = decode_get_fpga_diagnostics_settings_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)power_status);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size < sizeof(uint8_t))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_get_gpu_ist_mode_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint8_t mode,
				 struct nsm_msg *msg)
{
	return encode_get_fpga_diagnostics_settings_resp(
	    instance_id, cc, reason_code, sizeof(uint8_t), &mode, msg);
}

int decode_get_gpu_ist_mode_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint8_t *mode)
{
	uint16_t data_size = 0;
	int ret = decode_get_fpga_diagnostics_settings_resp(
	    msg, msg_len, cc, &data_size, reason_code, mode);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size < sizeof(uint8_t))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_enable_disable_gpu_ist_mode_req(uint8_t instance_id,
					   uint8_t device_index, uint8_t value,
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

	struct nsm_enable_disable_gpu_ist_mode_req *request =
	    (struct nsm_enable_disable_gpu_ist_mode_req *)msg->payload;

	request->hdr.command = NSM_ENABLE_DISABLE_GPU_IST_MODE;
	request->hdr.data_size = 2;
	request->device_index = device_index;
	request->value = value;

	return NSM_SW_SUCCESS;
}

int decode_enable_disable_gpu_ist_mode_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint8_t *device_index,
					   uint8_t *value)
{
	if (msg == NULL || device_index == NULL || value == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_enable_disable_gpu_ist_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_enable_disable_gpu_ist_mode_req *request =
	    (struct nsm_enable_disable_gpu_ist_mode_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(struct nsm_enable_disable_gpu_ist_mode_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*device_index = request->device_index;
	*value = request->value;
	return NSM_SW_SUCCESS;
}

int encode_enable_disable_gpu_ist_mode_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg)
{
	return encode_common_resp(instance_id, cc, reason_code,
				  NSM_TYPE_DEVICE_CONFIGURATION,
				  NSM_ENABLE_DISABLE_GPU_IST_MODE, msg);
}

int decode_enable_disable_gpu_ist_mode_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (data_size != 0) {
		return NSM_SW_ERROR_LENGTH;
	}
	return rc;
}

int encode_get_reconfiguration_permissions_v1_req(
    uint8_t instance_id,
    enum reconfiguration_permissions_v1_index setting_index,
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

	struct nsm_get_reconfiguration_permissions_v1_req *request =
	    (struct nsm_get_reconfiguration_permissions_v1_req *)msg->payload;

	request->hdr.command = NSM_GET_RECONFIGURATION_PERMISSIONS_V1;
	request->hdr.data_size = 1;
	request->setting_index = setting_index;

	return NSM_SW_SUCCESS;
}
int decode_get_reconfiguration_permissions_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum reconfiguration_permissions_v1_index *setting_index)
{
	if (msg == NULL || setting_index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_reconfiguration_permissions_v1_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_reconfiguration_permissions_v1_req *request =
	    (struct nsm_get_reconfiguration_permissions_v1_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(struct nsm_get_reconfiguration_permissions_v1_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*setting_index = request->setting_index;
	return NSM_SW_SUCCESS;
}

int encode_get_reconfiguration_permissions_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_reconfiguration_permissions_v1 *data, struct nsm_msg *msg)
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
		    cc, reason_code, NSM_GET_RECONFIGURATION_PERMISSIONS_V1,
		    msg);
	}

	struct nsm_get_reconfiguration_permissions_v1_resp *resp =
	    (struct nsm_get_reconfiguration_permissions_v1_resp *)msg->payload;

	uint16_t data_size = sizeof(struct nsm_reconfiguration_permissions_v1);
	resp->hdr.command = NSM_GET_RECONFIGURATION_PERMISSIONS_V1;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(data_size);
	resp->data = *data;
	return NSM_SW_SUCCESS;
}

int decode_get_reconfiguration_permissions_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_reconfiguration_permissions_v1 *data)
{
	if (msg == NULL || cc == NULL || reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_reconfiguration_permissions_v1_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_reconfiguration_permissions_v1_resp *resp =
	    (struct nsm_get_reconfiguration_permissions_v1_resp *)msg->payload;

	*data = resp->data;
	return NSM_SW_SUCCESS;
}

int encode_set_reconfiguration_permissions_v1_req(
    uint8_t instance_id,
    enum reconfiguration_permissions_v1_index setting_index,
    enum reconfiguration_permissions_v1_setting configuration,
    uint8_t permission, struct nsm_msg *msg)
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

	struct nsm_set_reconfiguration_permissions_v1_req *request =
	    (struct nsm_set_reconfiguration_permissions_v1_req *)msg->payload;

	request->hdr.command = NSM_SET_RECONFIGURATION_PERMISSIONS_V1;
	request->hdr.data_size = 3;
	request->setting_index = setting_index;
	request->configuration = configuration;
	request->permission = permission;

	return NSM_SW_SUCCESS;
}

int decode_set_reconfiguration_permissions_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum reconfiguration_permissions_v1_index *setting_index,
    enum reconfiguration_permissions_v1_setting *configuration,
    uint8_t *permission)
{
	if (msg == NULL || setting_index == NULL || configuration == NULL ||
	    permission == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_set_reconfiguration_permissions_v1_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_reconfiguration_permissions_v1_req *request =
	    (struct nsm_set_reconfiguration_permissions_v1_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(struct nsm_set_reconfiguration_permissions_v1_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*setting_index = request->setting_index;
	*configuration = request->configuration;
	*permission = request->permission;
	return NSM_SW_SUCCESS;
}

int encode_set_reconfiguration_permissions_v1_resp(uint8_t instance_id,
						   uint8_t cc,
						   uint16_t reason_code,
						   struct nsm_msg *msg)
{

	return encode_common_resp(instance_id, cc, reason_code,
				  NSM_TYPE_DEVICE_CONFIGURATION,
				  NSM_SET_RECONFIGURATION_PERMISSIONS_V1, msg);
}

int decode_set_reconfiguration_permissions_v1_resp(const struct nsm_msg *msg,
						   size_t msg_len, uint8_t *cc,
						   uint16_t *reason_code)
{
	uint16_t data_size = 0;
	int rc = decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
	if (data_size != 0) {
		return NSM_SW_ERROR_LENGTH;
	}
	return rc;
}

int encode_get_confidential_compute_mode_v1_req(uint8_t instance_id,
						struct nsm_msg *msg)
{
	return encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1, msg);
}

int decode_get_confidential_compute_mode_v1_req(const struct nsm_msg *msg,
						size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_confidential_compute_mode_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code, uint8_t current_mode,
    uint8_t pending_mode, struct nsm_msg *msg)
{
	if (msg == NULL) {
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
		    cc, reason_code, NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1, msg);
	}

	struct nsm_get_confidential_compute_mode_v1_resp *resp =
	    (struct nsm_get_confidential_compute_mode_v1_resp *)msg->payload;

	resp->hdr.command = NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size =
	    htole16(sizeof(struct nsm_get_confidential_compute_mode_v1_resp) -
		    sizeof(struct nsm_common_resp));
	resp->current_mode = current_mode;
	resp->pending_mode = pending_mode;
	return NSM_SW_SUCCESS;
}

int decode_get_confidential_compute_mode_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint8_t *current_mode, uint8_t *pending_mode)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    current_mode == NULL || pending_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    (sizeof(struct nsm_msg_hdr)) +
		sizeof(struct nsm_get_confidential_compute_mode_v1_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_confidential_compute_mode_v1_resp *resp =
	    (struct nsm_get_confidential_compute_mode_v1_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) !=
	    (sizeof(struct nsm_get_confidential_compute_mode_v1_resp) -
	     sizeof(struct nsm_common_resp))) {
		return NSM_SW_ERROR_DATA;
	}
	*current_mode = resp->current_mode;
	*pending_mode = resp->pending_mode;
	return NSM_SW_SUCCESS;
}

int encode_set_confidential_compute_mode_v1_req(uint8_t instance_id,
						uint8_t mode,
						struct nsm_msg *msg)
{
	int rc = encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				   NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1, msg);
	if (rc == NSM_SW_SUCCESS) {
		struct nsm_set_confidential_compute_mode_v1_req *req =
		    (struct nsm_set_confidential_compute_mode_v1_req *)
			msg->payload;
		req->hdr.data_size =
		    sizeof(struct nsm_set_confidential_compute_mode_v1_req) -
		    sizeof(struct nsm_common_req);
		req->mode = mode;
	}
	return rc;
}

int decode_set_confidential_compute_mode_v1_req(const struct nsm_msg *msg,
						size_t msg_len, uint8_t *mode)
{
	if (msg == NULL || mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_set_confidential_compute_mode_v1_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_confidential_compute_mode_v1_req *request =
	    (struct nsm_set_confidential_compute_mode_v1_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_set_confidential_compute_mode_v1_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*mode = request->mode;
	return NSM_SW_SUCCESS;
}

int encode_set_confidential_compute_mode_v1_resp(uint8_t instance_id,
						 uint8_t cc,
						 uint16_t reason_code,
						 struct nsm_msg *msg)
{
	return encode_common_resp(instance_id, cc, reason_code,
				  NSM_TYPE_DEVICE_CONFIGURATION,
				  NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1, msg);
}

int decode_set_confidential_compute_mode_v1_resp(const struct nsm_msg *msg,
						 size_t msg_len, uint8_t *cc,
						 uint16_t *data_size,
						 uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

// Get EGM Mode Command, NSM T5 spec
int encode_get_EGM_mode_req(uint8_t instance_id, struct nsm_msg *msg)
{
	return encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				 NSM_GET_EGM_MODE, msg);
}

int decode_get_EGM_mode_req(const struct nsm_msg *msg, size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

// Get EGM Mode Command, NSM T5 spec
int encode_get_EGM_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, uint8_t current_mode,
			     uint8_t pending_mode, struct nsm_msg *msg)
{
	if (msg == NULL || current_mode > 1) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CONFIGURATION;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_EGM_MODE,
					  msg);
	}

	struct nsm_get_EGM_mode_resp *resp =
	    (struct nsm_get_EGM_mode_resp *)msg->payload;
	resp->hdr.command = NSM_GET_EGM_MODE;
	resp->hdr.completion_code = cc;
	uint16_t data_size = htole16(sizeof(struct nsm_get_EGM_mode_resp) -
				     sizeof(struct nsm_common_resp));
	resp->hdr.data_size = htole16(data_size);
	resp->pending_mode = pending_mode;

	return NSM_SW_SUCCESS;
}

// Get EGM Mode Command, NSM T5 spec
int decode_get_EGM_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code, uint8_t *current_mode,
			     uint8_t *pending_mode)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    current_mode == NULL || pending_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_EGM_mode_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_EGM_mode_resp *resp =
	    (struct nsm_get_EGM_mode_resp *)msg->payload;

	if (resp->hdr.command != NSM_GET_EGM_MODE) {
		return NSM_SW_ERROR_DATA;
	}
	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) != (sizeof(struct nsm_get_EGM_mode_resp) -
			     sizeof(struct nsm_common_resp))) {
		return NSM_SW_ERROR_DATA;
	}
	*current_mode = 0;
	*pending_mode = resp->pending_mode;

	return NSM_SW_SUCCESS;
}

// Set EGM Mode Command, NSM T5 spec
int encode_set_EGM_mode_req(uint8_t instance_id, uint8_t requested_mode,
			    struct nsm_msg *msg)
{
	int rc = encode_common_req(instance_id, NSM_TYPE_DEVICE_CONFIGURATION,
				   NSM_SET_EGM_MODE, msg);

	if (rc == NSM_SW_SUCCESS) {
		struct nsm_set_EGM_mode_req *request =
		    (struct nsm_set_EGM_mode_req *)msg->payload;
		request->hdr.data_size = htole16(sizeof(uint8_t));
		request->requested_mode = requested_mode;
	}
	return rc;
}

// Set EGM Mode Command, NSM T5 spec
int decode_set_EGM_mode_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *requested_mode)
{
	if (msg == NULL || requested_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_set_EGM_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_EGM_mode_req *request =
	    (struct nsm_set_EGM_mode_req *)msg->payload;

	if (request->hdr.data_size != sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*requested_mode = request->requested_mode;
	return NSM_SW_SUCCESS;
}

int encode_set_EGM_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg)
{
	return encode_common_resp(instance_id, cc, reason_code,
				  NSM_TYPE_DEVICE_CONFIGURATION,
				  NSM_SET_EGM_MODE, msg);
}

int decode_set_EGM_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}
