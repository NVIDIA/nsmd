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

#include "platform-environmental.h"
#include "base.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_get_inventory_information_req(uint8_t instance_id,
					 uint8_t property_identifier,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_inventory_information_req *request =
	    (struct nsm_get_inventory_information_req *)msg->payload;

	request->hdr.command = NSM_GET_INVENTORY_INFORMATION;
	request->hdr.data_size = 1;
	request->property_identifier = property_identifier;

	return NSM_SW_SUCCESS;
}

int decode_get_inventory_information_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint8_t *property_identifier)
{
	if (msg == NULL || property_identifier == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_inventory_information_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_inventory_information_req *request =
	    (struct nsm_get_inventory_information_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(struct nsm_get_inventory_information_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*property_identifier = request->property_identifier;

	return NSM_SW_SUCCESS;
}

int encode_get_inventory_information_resp(uint8_t instance_id, uint8_t cc,
					  uint16_t reason_code,
					  const uint16_t data_size,
					  const uint8_t *inventory_information,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_INVENTORY_INFORMATION, msg);
	}

	struct nsm_get_inventory_information_resp *resp =
	    (struct nsm_get_inventory_information_resp *)msg->payload;

	resp->hdr.command = NSM_GET_INVENTORY_INFORMATION;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(data_size);

	if (cc == NSM_SUCCESS) {
		{
			if (inventory_information == NULL) {
				return NSM_SW_ERROR_NULL;
			}
		}
		memcpy(resp->inventory_information, inventory_information,
		       data_size);
	}

	return NSM_SW_SUCCESS;
}

int decode_get_inventory_information_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code,
					  uint16_t *data_size,
					  uint8_t *inventory_information)
{
	if (data_size == NULL || inventory_information == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_inventory_information_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_inventory_information_resp *resp =
	    (struct nsm_get_inventory_information_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	memcpy(inventory_information, resp->inventory_information, *data_size);

	return NSM_SW_SUCCESS;
}

int encode_get_platform_env_command_no_payload_req(
    uint8_t instance_id, struct nsm_msg *msg,
    enum nsm_platform_environmental_commands command)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = command;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

int decode_get_platform_env_command_no_payload_req(
    const struct nsm_msg *msg, size_t msg_len,
    __attribute__((unused)) enum nsm_platform_environmental_commands command)
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

uint32_t
decode_inventory_information_as_uint32(const uint8_t *inventory_information,
				       const uint16_t data_size)
{
	if (data_size < sizeof(uint32_t))
		return UINT32_MAX;
	return le32toh(*(uint32_t *)inventory_information);
}

int encode_get_temperature_reading_req(uint8_t instance_id, uint8_t sensor_id,
				       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_temperature_reading_req *request =
	    (nsm_get_temperature_reading_req *)msg->payload;

	request->hdr.command = NSM_GET_TEMPERATURE_READING;
	request->hdr.data_size = sizeof(sensor_id);
	request->sensor_id = sensor_id;

	return NSM_SW_SUCCESS;
}

int decode_get_temperature_reading_req(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *sensor_id)
{
	if (msg == NULL || sensor_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_temperature_reading_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_temperature_reading_req *request =
	    (nsm_get_temperature_reading_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->sensor_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*sensor_id = request->sensor_id;

	return NSM_SW_SUCCESS;
}

int encode_get_temperature_reading_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					double temperature_reading,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_TEMPERATURE_READING, msg);
	}

	struct nsm_get_temperature_reading_resp *response =
	    (struct nsm_get_temperature_reading_resp *)msg->payload;

	response->hdr.command = NSM_GET_TEMPERATURE_READING;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(uint32_t));

	int32_t reading = temperature_reading * (1 << 8);
	response->reading = htole32(reading);

	return NSM_SW_SUCCESS;
}

int decode_get_temperature_reading_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					double *temperature_reading)
{
	if (temperature_reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_temperature_reading_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_temperature_reading_resp *response =
	    (struct nsm_get_temperature_reading_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(int32_t)) {
		return NSM_SW_ERROR_DATA;
	}

	int32_t reading = le32toh(response->reading);
	*temperature_reading = reading / (double)(1 << 8);

	return NSM_SW_SUCCESS;
}

int encode_read_thermal_parameter_req(uint8_t instance, uint8_t parameter_id,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_read_thermal_parameter_req *request =
	    (struct nsm_read_thermal_parameter_req *)msg->payload;

	request->hdr.command = NSM_READ_THERMAL_PARAMETER;
	request->hdr.data_size = sizeof(parameter_id);
	request->parameter_id = parameter_id;

	return NSM_SW_SUCCESS;
}

int decode_read_thermal_parameter_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *parameter_id)
{
	if (msg == NULL || parameter_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_read_thermal_parameter_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_read_thermal_parameter_req *request =
	    (struct nsm_read_thermal_parameter_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->parameter_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*parameter_id = request->parameter_id;

	return NSM_SW_SUCCESS;
}

int encode_read_thermal_parameter_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code, int32_t threshold,
				       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_READ_THERMAL_PARAMETER, msg);
	}

	struct nsm_read_thermal_parameter_resp *response =
	    (struct nsm_read_thermal_parameter_resp *)msg->payload;

	response->hdr.command = NSM_READ_THERMAL_PARAMETER;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(threshold));
	response->threshold = htole32(threshold);

	return NSM_SW_SUCCESS;
}

int decode_read_thermal_parameter_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code,
				       int32_t *threshold)
{
	if (threshold == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_read_thermal_parameter_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_read_thermal_parameter_resp *response =
	    (struct nsm_read_thermal_parameter_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(*threshold)) {
		return NSM_SW_ERROR_DATA;
	}

	*threshold = le32toh(response->threshold);
	return NSM_SW_SUCCESS;
}

int encode_aggregate_thermal_parameter_data(int32_t threshold, uint8_t *data,
					    size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int32_t le_reading = htole32(threshold);

	memcpy(data, &le_reading, sizeof(int32_t));
	*data_len = sizeof(int32_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_thermal_parameter_data(const uint8_t *data,
					    size_t data_len, int32_t *threshold)
{
	if (data == NULL || threshold == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(int32_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	int32_t le_reading;
	memcpy(&le_reading, data, sizeof(int32_t));

	*threshold = le32toh(le_reading);

	return NSM_SW_SUCCESS;
}

int encode_get_current_power_draw_req(uint8_t instance_id, uint8_t sensor_id,
				      uint8_t averaging_interval,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_current_power_draw_req *request =
	    (struct nsm_get_current_power_draw_req *)msg->payload;

	request->hdr.command = NSM_GET_POWER;
	request->hdr.data_size = sizeof(sensor_id) + sizeof(averaging_interval);
	request->sensor_id = sensor_id;
	request->averaging_interval = averaging_interval;

	return NSM_SW_SUCCESS;
}

int decode_get_current_power_draw_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *sensor_id,
				      uint8_t *averaging_interval)
{
	if (msg == NULL || sensor_id == NULL || averaging_interval == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_current_power_draw_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_current_power_draw_req *request =
	    (struct nsm_get_current_power_draw_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->sensor_id) + sizeof(request->averaging_interval)) {
		return NSM_SW_ERROR_DATA;
	}

	*sensor_id = request->sensor_id;
	*averaging_interval = request->averaging_interval;

	return NSM_SW_SUCCESS;
}

int encode_get_current_power_draw_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code, uint32_t reading,
				       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_POWER, msg);
	}

	nsm_get_current_power_draw_resp *response =
	    (nsm_get_current_power_draw_resp *)msg->payload;

	response->hdr.command = NSM_GET_POWER;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(reading));
	response->reading = htole32(reading);

	return NSM_SW_SUCCESS;
}

int decode_get_current_power_draw_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code, uint32_t *reading)
{
	if (reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_current_power_draw_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_current_power_draw_resp *response =
	    (nsm_get_current_power_draw_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(*reading)) {
		return NSM_SW_ERROR_DATA;
	}

	*reading = le32toh(response->reading);
	return NSM_SW_SUCCESS;
}

int encode_get_max_observed_power_req(uint8_t instance_id, uint8_t sensor_id,
				      uint8_t averaging_interval,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_max_observed_power_req *request =
	    (nsm_get_max_observed_power_req *)msg->payload;

	request->hdr.command = NSM_GET_MAX_OBSERVED_POWER;
	request->hdr.data_size = sizeof(sensor_id) + sizeof(averaging_interval);
	request->sensor_id = sensor_id;
	request->averaging_interval = averaging_interval;

	return NSM_SW_SUCCESS;
}

int decode_get_max_observed_power_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *sensor_id,
				      uint8_t *averaging_interval)
{
	if (msg == NULL || sensor_id == NULL || averaging_interval == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_max_observed_power_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_max_observed_power_req *request =
	    (nsm_get_max_observed_power_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->sensor_id) + sizeof(request->averaging_interval)) {
		return NSM_SW_ERROR_DATA;
	}

	*sensor_id = request->sensor_id;
	*averaging_interval = request->averaging_interval;

	return NSM_SW_SUCCESS;
}

int encode_get_max_observed_power_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code, uint32_t reading,
				       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_POWER, msg);
	}

	nsm_get_max_observed_power_resp *response =
	    (nsm_get_max_observed_power_resp *)msg->payload;

	response->hdr.command = NSM_GET_MAX_OBSERVED_POWER;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(reading));
	response->reading = htole32(reading);

	return NSM_SW_SUCCESS;
}

int decode_get_max_observed_power_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code, uint32_t *reading)
{
	if (reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_max_observed_power_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_max_observed_power_resp *response =
	    (nsm_get_max_observed_power_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(*reading)) {
		return NSM_SW_ERROR_DATA;
	}

	*reading = le32toh(response->reading);
	return NSM_SW_SUCCESS;
}

int encode_get_driver_info_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	// Since there's no payload, we just set the command and data size
	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_DRIVER_INFO;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

int decode_get_driver_info_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	if (request->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}
	return NSM_SW_SUCCESS;
}

int encode_get_driver_info_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code, const uint16_t data_size,
				const uint8_t *driver_info_data,
				struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_DRIVER_INFO,
					  msg);
	}

	struct nsm_get_driver_info_resp *response =
	    (struct nsm_get_driver_info_resp *)msg->payload;

	response->hdr.command = NSM_GET_DRIVER_INFO;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(data_size);

	if (driver_info_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	response->driver_state = driver_info_data[0];
	memcpy(response->driver_version,
	       driver_info_data + sizeof(response->driver_state),
	       data_size - sizeof(response->driver_state));

	return NSM_SUCCESS;
}

int decode_get_driver_info_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *reason_code,
				enum8 *driver_state, char *driver_version)
{
	if (driver_state == NULL || driver_version == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_driver_info_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_driver_info_resp *response =
	    (struct nsm_get_driver_info_resp *)msg->payload;
	size_t data_size = le16toh(response->hdr.data_size);
	*driver_state = response->driver_state;
	size_t driver_version_length =
	    data_size - sizeof(response->driver_state);

	if (driver_version_length > MAX_VERSION_STRING_SIZE ||
	    response->driver_version[driver_version_length - 1] != '\0') {
		return NSM_SW_ERROR_LENGTH;
	}

	memcpy(driver_version, response->driver_version, driver_version_length);

	return NSM_SUCCESS;
}

int encode_aggregate_get_current_power_draw_reading(uint32_t reading,
						    uint8_t *data,
						    size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint32_t le_reading = htole32(reading);

	memcpy(data, &le_reading, sizeof(uint32_t));
	*data_len = sizeof(uint32_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_get_current_power_draw_reading(const uint8_t *data,
						    size_t data_len,
						    uint32_t *reading)
{
	if (data == NULL || reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint32_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint32_t le_reading;
	memcpy(&le_reading, data, sizeof(uint32_t));

	*reading = le32toh(le_reading);

	return NSM_SW_SUCCESS;
}

int encode_get_current_energy_count_req(uint8_t instance, uint8_t sensor_id,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_current_energy_count_req *request =
	    (nsm_get_current_energy_count_req *)msg->payload;

	request->hdr.command = NSM_GET_ENERGY_COUNT;
	request->hdr.data_size = sizeof(sensor_id);
	request->sensor_id = sensor_id;

	return NSM_SW_SUCCESS;
}

int decode_get_current_energy_count_req(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *sensor_id)
{
	if (msg == NULL || sensor_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_current_energy_count_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_current_energy_count_req *request =
	    (nsm_get_current_energy_count_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->sensor_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*sensor_id = request->sensor_id;

	return NSM_SW_SUCCESS;
}

int encode_get_current_energy_count_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 uint64_t energy_reading,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_ENERGY_COUNT,
					  msg);
	}

	struct nsm_get_current_energy_count_resp *response =
	    (struct nsm_get_current_energy_count_resp *)msg->payload;

	response->hdr.command = NSM_GET_ENERGY_COUNT;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(energy_reading));
	response->reading = htole64(energy_reading);

	return NSM_SW_SUCCESS;
}

int decode_get_current_energy_count_resp(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *cc,
					 uint16_t *reason_code,
					 uint64_t *energy_reading)
{
	if (energy_reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_current_energy_count_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_current_energy_count_resp *response =
	    (struct nsm_get_current_energy_count_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(*energy_reading)) {
		return NSM_SW_ERROR_DATA;
	}

	*energy_reading = le64toh(response->reading);

	return NSM_SW_SUCCESS;
}

int encode_get_voltage_req(uint8_t instance, uint8_t sensor_id,
			   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_voltage_req *request = (nsm_get_voltage_req *)msg->payload;

	request->hdr.command = NSM_GET_VOLTAGE;
	request->hdr.data_size = sizeof(sensor_id);
	request->sensor_id = sensor_id;

	return NSM_SW_SUCCESS;
}

int decode_get_voltage_req(const struct nsm_msg *msg, size_t msg_len,
			   uint8_t *sensor_id)
{
	if (msg == NULL || sensor_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_get_voltage_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_voltage_req *request = (nsm_get_voltage_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->sensor_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*sensor_id = request->sensor_id;

	return NSM_SW_SUCCESS;
}

int encode_get_voltage_resp(uint8_t instance_id, uint8_t cc,
			    uint16_t reason_code, uint32_t voltage,
			    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_VOLTAGE,
					  msg);
	}

	nsm_get_voltage_resp *response = (nsm_get_voltage_resp *)msg->payload;

	response->hdr.command = NSM_GET_VOLTAGE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(voltage));
	response->reading = htole32(voltage);

	return NSM_SW_SUCCESS;
}

int decode_get_voltage_resp(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *cc, uint16_t *reason_code,
			    uint32_t *voltage)
{
	if (voltage == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_get_voltage_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_voltage_resp *response = (nsm_get_voltage_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(*voltage)) {
		return NSM_SW_ERROR_DATA;
	}

	*voltage = le64toh(response->reading);

	return NSM_SW_SUCCESS;
}

int encode_get_altitude_pressure_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ALTITUDE_PRESSURE;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int encode_get_altitude_pressure_resp(uint8_t instance_id, uint8_t cc,
				      uint16_t reason_code, uint32_t reading,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_ALTITUDE_PRESSURE, msg);
	}

	nsm_get_altitude_pressure_resp *response =
	    (nsm_get_altitude_pressure_resp *)msg->payload;

	response->hdr.command = NSM_GET_ALTITUDE_PRESSURE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(reading));
	response->reading = htole32(reading);

	return NSM_SW_SUCCESS;
}

int decode_get_altitude_pressure_resp(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *cc, uint16_t *reason_code,
				      uint32_t *reading)
{
	if (reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_altitude_pressure_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_altitude_pressure_resp *response =
	    (nsm_get_altitude_pressure_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);
	if (data_size != sizeof(*reading)) {
		return NSM_SW_ERROR_DATA;
	}

	*reading = le32toh(response->reading);
	return NSM_SW_SUCCESS;
}

int encode_aggregate_timestamp_data(uint64_t timestamp, uint8_t *data,
				    size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint64_t le_reading = htole64(timestamp);

	memcpy(data, &le_reading, sizeof(uint64_t));
	*data_len = sizeof(uint64_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_timestamp_data(const uint8_t *data, size_t data_len,
				    uint64_t *timestamp)
{
	if (data == NULL || timestamp == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint64_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint64_t le_timestamp;
	memcpy(&le_timestamp, data, sizeof(uint64_t));

	*timestamp = le64toh(le_timestamp);

	return NSM_SW_SUCCESS;
}

int encode_aggregate_resp(uint8_t instance_id, uint8_t command, uint8_t cc,
			  uint16_t telemetry_count, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_aggregate_resp *response =
	    (struct nsm_aggregate_resp *)msg->payload;

	response->command = command;
	response->telemetry_count = htole16(telemetry_count);
	response->completion_code = cc;

	return NSM_SW_SUCCESS;
}

int decode_aggregate_resp(const struct nsm_msg *msg, size_t msg_len,
			  size_t *consumed_len, uint8_t *cc,
			  uint16_t *telemetry_count)
{
	if (msg == NULL || cc == NULL || telemetry_count == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_aggregate_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_aggregate_resp *response =
	    (struct nsm_aggregate_resp *)msg->payload;

	*consumed_len =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_aggregate_resp);
	*cc = response->completion_code;
	*telemetry_count = le16toh(response->telemetry_count);

	return NSM_SW_SUCCESS;
}

int encode_aggregate_resp_sample(uint8_t tag, bool valid, const uint8_t *data,
				 size_t data_len,
				 struct nsm_aggregate_resp_sample *sample,
				 size_t *sample_len)
{
	if (data == NULL || sample == NULL || sample_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	sample->tag = tag;
	sample->reserved = 0;
	sample->valid = valid;

	for (int i = 0; i <= NSM_AGGREGATE_MAX_SAMPLE_SIZE_AS_POWER_OF_2; ++i) {
		size_t valid_size = 1 << i;
		if (data_len == valid_size) {
			sample->length = i;

			memcpy(sample->data, data, data_len);
			*sample_len = data_len +
				      sizeof(struct nsm_aggregate_resp_sample) -
				      1;

			return NSM_SW_SUCCESS;
		}
	}

	return NSM_SW_ERROR_DATA;
}

int decode_aggregate_resp_sample(const struct nsm_aggregate_resp_sample *sample,
				 size_t msg_len, size_t *consumed_len,
				 uint8_t *tag, bool *valid,
				 const uint8_t **data, size_t *data_len)
{
	if (sample == NULL || consumed_len == NULL || tag == NULL ||
	    data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_aggregate_resp_sample)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*data_len = 1 << sample->length;
	*consumed_len =
	    *data_len + sizeof(struct nsm_aggregate_resp_sample) - 1;

	*valid = sample->valid;
	*tag = sample->tag;
	*data = sample->data;

	if (msg_len < *consumed_len) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_aggregate_temperature_reading_data(double temperature_reading,
					      uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int32_t reading = temperature_reading * (1 << 8);
	reading = htole32(reading);

	memcpy(data, &reading, sizeof(int32_t));
	*data_len = sizeof(int32_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_temperature_reading_data(const uint8_t *data,
					      size_t data_len,
					      double *temperature_reading)
{
	if (data == NULL || temperature_reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint32_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint32_t reading = 0;
	memcpy(&reading, data, sizeof(uint32_t));
	*temperature_reading = (int32_t)le32toh(reading) / (double)(1 << 8);

	return NSM_SW_SUCCESS;
}

int encode_aggregate_energy_count_data(uint64_t energy_count, uint8_t *data,
				       size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint64_t le_reading = htole64(energy_count);

	memcpy(data, &le_reading, sizeof(uint64_t));
	*data_len = sizeof(uint64_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_energy_count_data(const uint8_t *data, size_t data_len,
				       uint64_t *energy_count)
{
	if (data == NULL || energy_count == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint64_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint64_t le_reading;
	memcpy(&le_reading, data, sizeof(uint64_t));

	*energy_count = le64toh(le_reading);

	return NSM_SW_SUCCESS;
}

int encode_aggregate_voltage_data(uint32_t voltage, uint8_t *data,
				  size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint32_t le_reading = htole32(voltage);

	memcpy(data, &le_reading, sizeof(uint32_t));
	*data_len = sizeof(uint32_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_voltage_data(const uint8_t *data, size_t data_len,
				  uint32_t *voltage)
{
	if (data == NULL || voltage == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint32_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint32_t le_reading;
	memcpy(&le_reading, data, sizeof(uint32_t));

	*voltage = le32toh(le_reading);

	return NSM_SW_SUCCESS;
}

// Get Mig Mode Command, NSM T3 spec
int encode_get_MIG_mode_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_MIG_MODE;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

// Get Mig Mode Command, NSM T3 spec
int encode_get_MIG_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, bitfield8_t *flags,
			     struct nsm_msg *msg)
{
	if (msg == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_MIG_MODE,
					  msg);
	}

	struct nsm_get_MIG_mode_resp *resp =
	    (struct nsm_get_MIG_mode_resp *)msg->payload;
	resp->hdr.command = NSM_GET_MIG_MODE;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(bitfield8_t);
	resp->hdr.data_size = htole16(data_size);
	memcpy(&(resp->flags), flags, data_size);

	return NSM_SW_SUCCESS;
}

// Get Mig Mode Command, NSM T3 spec
int decode_get_MIG_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code, bitfield8_t *flags)
{
	if (msg == NULL || cc == NULL || data_size == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_MIG_mode_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_MIG_mode_resp *resp =
	    (struct nsm_get_MIG_mode_resp *)msg->payload;

	if (resp->hdr.command != NSM_GET_MIG_MODE) {
		return NSM_SW_ERROR_DATA;
	}
	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < sizeof(bitfield8_t)) {
		return NSM_SW_ERROR_DATA;
	}
	memcpy(flags, &(resp->flags), sizeof(bitfield8_t));

	return NSM_SW_SUCCESS;
}

// Set Mig Mode Command, NSM T3 spec
int encode_set_MIG_mode_req(uint8_t instance_id, uint8_t requested_mode,
			    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_MIG_mode_req *request =
	    (struct nsm_set_MIG_mode_req *)msg->payload;

	request->hdr.command = NSM_SET_MIG_MODE;
	request->hdr.data_size = sizeof(uint8_t);
	request->requested_mode = requested_mode;
	return NSM_SW_SUCCESS;
}

// Set Mig Mode Command, NSM T3 spec
int decode_set_MIG_mode_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *requested_mode)
{
	if (msg == NULL || requested_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_set_MIG_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_MIG_mode_req *request =
	    (struct nsm_set_MIG_mode_req *)msg->payload;

	if (request->hdr.data_size != sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*requested_mode = request->requested_mode;
	return NSM_SW_SUCCESS;
}

int encode_set_MIG_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_SET_MIG_MODE,
					  msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_SET_MIG_MODE;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_MIG_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

// Get ECC Mode Command, NSM T3 spec
int encode_get_ECC_mode_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ECC_MODE;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

// Get ECC Mode Command, NSM T3 spec
int encode_get_ECC_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, bitfield8_t *flags,
			     struct nsm_msg *msg)
{
	if (msg == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_ECC_MODE,
					  msg);
	}
	struct nsm_get_ECC_mode_resp *resp =
	    (struct nsm_get_ECC_mode_resp *)msg->payload;
	resp->hdr.command = NSM_GET_ECC_MODE;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(bitfield8_t);
	resp->hdr.data_size = htole16(data_size);
	memcpy(&(resp->flags), flags, data_size);

	return NSM_SW_SUCCESS;
}

// Get ECC Mode Command, NSM T3 spec
int decode_get_ECC_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code, bitfield8_t *flags)
{
	if (msg == NULL || cc == NULL || data_size == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_ECC_mode_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_ECC_mode_resp *resp =
	    (struct nsm_get_ECC_mode_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < sizeof(bitfield8_t)) {
		return NSM_SW_ERROR_DATA;
	}
	memcpy(flags, &(resp->flags), sizeof(bitfield8_t));

	return NSM_SW_SUCCESS;
}

// Set ECC Mode Command, NSM T3 spec
int encode_set_ECC_mode_req(uint8_t instance_id, uint8_t requested_mode,
			    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_ECC_mode_req *request =
	    (struct nsm_set_ECC_mode_req *)msg->payload;

	request->hdr.command = NSM_SET_ECC_MODE;
	request->hdr.data_size = sizeof(uint8_t);
	request->requested_mode = requested_mode;
	return NSM_SW_SUCCESS;
}

// Set ECC Mode Command, NSM T3 spec
int decode_set_ECC_mode_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *requested_mode)
{
	if (msg == NULL || requested_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_set_ECC_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_ECC_mode_req *request =
	    (struct nsm_set_ECC_mode_req *)msg->payload;

	if (request->hdr.data_size != sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*requested_mode = request->requested_mode;
	return NSM_SW_SUCCESS;
}

int encode_set_ECC_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_SET_ECC_MODE,
					  msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_SET_ECC_MODE;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_ECC_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

static void htoleEccErrorCounts(struct nsm_ECC_error_counts *errorCounts)
{
	errorCounts->flags.byte = htole16(errorCounts->flags.byte);
	errorCounts->dram_corrected = htole32(errorCounts->dram_corrected);
	errorCounts->dram_uncorrected = htole32(errorCounts->dram_uncorrected);
	errorCounts->sram_corrected = htole32(errorCounts->sram_corrected);
	errorCounts->sram_uncorrected_parity =
	    htole32(errorCounts->sram_uncorrected_parity);
	errorCounts->sram_uncorrected_secded =
	    htole32(errorCounts->sram_uncorrected_secded);
}

static void letohEccErrorCounts(struct nsm_ECC_error_counts *errorCounts)
{
	errorCounts->flags.byte = le16toh(errorCounts->flags.byte);
	errorCounts->dram_corrected = le32toh(errorCounts->dram_corrected);
	errorCounts->dram_uncorrected = le32toh(errorCounts->dram_uncorrected);
	errorCounts->sram_corrected = le32toh(errorCounts->sram_corrected);
	errorCounts->sram_uncorrected_parity =
	    le32toh(errorCounts->sram_uncorrected_parity);
	errorCounts->sram_uncorrected_secded =
	    le32toh(errorCounts->sram_uncorrected_secded);
}

// Get ECC Error Counts command, NSM T3 spec
int encode_get_ECC_error_counts_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ECC_ERROR_COUNTS;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

// Get ECC Error Counts command, NSM T3 spec
int encode_get_ECC_error_counts_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code,
				     struct nsm_ECC_error_counts *errorCounts,
				     struct nsm_msg *msg)
{
	if (msg == NULL || errorCounts == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_ECC_ERROR_COUNTS, msg);
	}
	struct nsm_get_ECC_error_counts_resp *resp =
	    (struct nsm_get_ECC_error_counts_resp *)msg->payload;
	resp->hdr.command = NSM_GET_ECC_ERROR_COUNTS;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_ECC_error_counts);
	resp->hdr.data_size = htole16(data_size);

	htoleEccErrorCounts(errorCounts);
	memcpy(&(resp->errorCounts), errorCounts,
	       sizeof(struct nsm_ECC_error_counts));

	return NSM_SW_SUCCESS;
}

// Get ECC Error Counts command, NSM T3 spec
int decode_get_ECC_error_counts_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *data_size,
				     uint16_t *reason_code,
				     struct nsm_ECC_error_counts *errorCounts)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    errorCounts == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_ECC_error_counts_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_ECC_error_counts_resp *resp =
	    (struct nsm_get_ECC_error_counts_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	letohEccErrorCounts(&(resp->errorCounts));

	if ((*data_size) < sizeof(struct nsm_ECC_error_counts)) {
		return NSM_SW_ERROR_DATA;
	}
	memcpy(errorCounts, &(resp->errorCounts),
	       sizeof(struct nsm_ECC_error_counts));

	return NSM_SW_SUCCESS;
}

// Get Programmable EDPp Scaling factor, NSM T3 spec
int encode_get_programmable_EDPp_scaling_factor_req(uint8_t instance_id,
						    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

// Get Programmable EDPp Scaling factor, NSM T3 spec
int encode_get_programmable_EDPp_scaling_factor_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_EDPp_scaling_factors *scaling_factors, struct nsm_msg *msg)
{
	if (msg == NULL || scaling_factors == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR,
		    msg);
	}

	struct nsm_get_programmable_EDPp_scaling_factor_resp *resp =
	    (struct nsm_get_programmable_EDPp_scaling_factor_resp *)
		msg->payload;
	resp->hdr.command = NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_EDPp_scaling_factors);
	resp->hdr.data_size = htole16(data_size);
	resp->scaling_factors = *scaling_factors;

	return NSM_SW_SUCCESS;
}

// Get Programmable EDPp Scaling factor, NSM T3 spec
int decode_get_programmable_EDPp_scaling_factor_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, struct nsm_EDPp_scaling_factors *scaling_factors)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    scaling_factors == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    (sizeof(struct nsm_msg_hdr)) +
		sizeof(struct nsm_get_programmable_EDPp_scaling_factor_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_programmable_EDPp_scaling_factor_resp *resp =
	    (struct nsm_get_programmable_EDPp_scaling_factor_resp *)
		msg->payload;
	*cc = resp->hdr.completion_code;
	if (NSM_SUCCESS != *cc) {
		return NSM_SUCCESS;
	}
	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < sizeof(struct nsm_EDPp_scaling_factors)) {
		return NSM_SW_ERROR_DATA;
	}
	*scaling_factors = resp->scaling_factors;

	return NSM_SW_SUCCESS;
}

int encode_set_programmable_EDPp_scaling_factor_req(uint8_t instance_id,
						    uint8_t action,
						    uint8_t persistence,
						    uint8_t scaling_factor,
						    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_programmable_EDPp_scaling_factor_req *request =
	    (struct nsm_set_programmable_EDPp_scaling_factor_req *)msg->payload;

	request->hdr.command = NSM_SET_PROGRAMMABLE_EDPP_SCALING_FACTOR;
	request->hdr.data_size =
	    sizeof(struct nsm_set_programmable_EDPp_scaling_factor_req) -
	    sizeof(struct nsm_common_req);
	request->action = action;
	request->persistence = persistence;
	request->scaling_factor = scaling_factor;
	return NSM_SW_SUCCESS;
}

int decode_set_programmable_EDPp_scaling_factor_req(const struct nsm_msg *msg,
						    size_t msg_len,
						    uint8_t *action,
						    uint8_t *persistence,
						    uint8_t *scaling_factor)
{
	if (msg == NULL || action == NULL || persistence == NULL ||
	    scaling_factor == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_set_programmable_EDPp_scaling_factor_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_programmable_EDPp_scaling_factor_req *request =
	    (struct nsm_set_programmable_EDPp_scaling_factor_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_set_programmable_EDPp_scaling_factor_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*action = request->action;
	*persistence = request->persistence;
	*scaling_factor = request->scaling_factor;

	return NSM_SW_SUCCESS;
}

int encode_set_programmable_EDPp_scaling_factor_resp(uint8_t instance_id,
						     uint8_t cc,
						     uint16_t reason_code,
						     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_SET_PROGRAMMABLE_EDPP_SCALING_FACTOR,
		    msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_SET_PROGRAMMABLE_EDPP_SCALING_FACTOR;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_programmable_EDPp_scaling_factor_resp(const struct nsm_msg *msg,
						     size_t msg_len,
						     uint8_t *cc,
						     uint16_t *data_size,
						     uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

int encode_set_power_limit_req(uint8_t instance_id, uint32_t id, uint8_t action,
			       uint8_t persistence, uint32_t power_limit,
			       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_power_limit_req *request =
	    (struct nsm_set_power_limit_req *)msg->payload;

	request->hdr.command = NSM_SET_POWER_LIMITS;
	request->hdr.data_size = 2 * sizeof(uint8_t) + 2 * sizeof(uint32_t);
	request->action = action;
	request->persistance = persistence;
	request->power_limit = htole32(power_limit);
	request->id = htole32(id);
	return NSM_SW_SUCCESS;
}

int encode_set_device_power_limit_req(uint8_t instance, uint8_t action,
				      uint8_t persistence, uint32_t power_limit,
				      struct nsm_msg *msg)
{
	return encode_set_power_limit_req(instance, DEVICE, action, persistence,
					  power_limit, msg);
}

int encode_set_module_power_limit_req(uint8_t instance, uint8_t action,
				      uint8_t persistence, uint32_t power_limit,
				      struct nsm_msg *msg)
{
	return encode_set_power_limit_req(instance, MODULE, action, persistence,
					  power_limit, msg);
}

int decode_set_power_limit_req(const struct nsm_msg *msg, size_t msg_len,
			       uint32_t *id, uint8_t *action,
			       uint8_t *persistence, uint32_t *power_limit)
{
	if (msg == NULL || id == NULL || action == NULL ||
	    persistence == NULL || power_limit == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_set_power_limit_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_power_limit_req *request =
	    (struct nsm_set_power_limit_req *)msg->payload;

	if (request->hdr.data_size !=
	    2 * sizeof(uint8_t) + 2 * sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*action = request->action;
	*persistence = request->persistance;
	*power_limit = le32toh(request->power_limit);
	*id = le32toh(request->id);
	return NSM_SW_SUCCESS;
}

int encode_set_power_limit_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_SET_POWER_LIMITS,
					  msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_SET_POWER_LIMITS;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_power_limit_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *data_size,
				uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

int encode_get_power_limit_req(uint8_t instance_id, uint32_t id,
			       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_power_limit_req *request =
	    (struct nsm_get_power_limit_req *)msg->payload;

	request->hdr.command = NSM_GET_POWER_LIMITS;
	request->hdr.data_size = sizeof(id);
	request->id = htole32(id);
	return NSM_SW_SUCCESS;
}

int encode_get_device_power_limit_req(uint8_t instance, struct nsm_msg *msg)
{
	return encode_get_power_limit_req(instance, DEVICE, msg);
}

int encode_get_module_power_limit_req(uint8_t instance, struct nsm_msg *msg)
{
	return encode_get_power_limit_req(instance, MODULE, msg);
}

int decode_get_power_limit_req(const struct nsm_msg *msg, size_t msg_len,
			       uint32_t *id)
{
	if (msg == NULL || id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_get_power_limit_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_power_limit_req *request =
	    (struct nsm_get_power_limit_req *)msg->payload;

	if (request->hdr.data_size != sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}
	*id = le32toh(request->id);
	return NSM_SW_SUCCESS;
}

int encode_get_power_limit_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code,
				uint32_t requested_persistent_limit,
				uint32_t requested_oneshot_limit,
				uint32_t enforced_limit, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_POWER_LIMITS,
					  msg);
	}

	struct nsm_get_power_limit_resp *resp =
	    (struct nsm_get_power_limit_resp *)msg->payload;

	resp->hdr.command = NSM_GET_POWER_LIMITS;
	resp->hdr.completion_code = cc;
	uint16_t data_size = 3 * sizeof(uint32_t);
	resp->hdr.data_size = htole16(data_size);
	resp->requested_persistent_limit = htole32(requested_persistent_limit);
	resp->requested_oneshot_limit = htole32(requested_oneshot_limit);
	resp->enforced_limit = htole32(enforced_limit);

	return NSM_SW_SUCCESS;
}

int decode_get_power_limit_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *data_size,
				uint16_t *reason_code,
				uint32_t *requested_persistent_limit,
				uint32_t *requested_oneshot_limit,
				uint32_t *enforced_limit)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    requested_persistent_limit == NULL ||
	    requested_oneshot_limit == NULL || enforced_limit == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_power_limit_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_power_limit_resp *resp =
	    (struct nsm_get_power_limit_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) != 3 * sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}
	*requested_persistent_limit = le32toh(resp->requested_persistent_limit);
	*requested_oneshot_limit = le32toh(resp->requested_oneshot_limit);
	*enforced_limit = le32toh(resp->enforced_limit);

	return NSM_SW_SUCCESS;
}

static void htoleClockLimit(struct nsm_clock_limit *clock_limit)
{
	clock_limit->present_limit_max =
	    htole32(clock_limit->present_limit_max);
	clock_limit->present_limit_min =
	    htole32(clock_limit->present_limit_min);
	clock_limit->requested_limit_max =
	    htole32(clock_limit->requested_limit_max);
	clock_limit->requested_limit_min =
	    htole32(clock_limit->requested_limit_min);
}

static void letohClockLimit(struct nsm_clock_limit *clock_limit)
{
	clock_limit->present_limit_max =
	    le32toh(clock_limit->present_limit_max);
	clock_limit->present_limit_min =
	    le32toh(clock_limit->present_limit_min);
	clock_limit->requested_limit_max =
	    le32toh(clock_limit->requested_limit_max);
	clock_limit->requested_limit_min =
	    le32toh(clock_limit->requested_limit_min);
}

// Get Clock Limit, NSM T3 spec
int encode_get_clock_limit_req(uint8_t instance_id, uint8_t clock_id,
			       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_clock_limit_req *request =
	    (struct nsm_get_clock_limit_req *)msg->payload;

	request->hdr.command = NSM_GET_CLOCK_LIMIT;
	request->hdr.data_size = sizeof(clock_id);
	request->clock_id = clock_id;

	return NSM_SW_SUCCESS;
}

// Get Clock Limit, NSM T3 spec
int decode_get_clock_limit_req(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t *clock_id)
{
	if (msg == NULL || clock_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_clock_limit_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_clock_limit_req *request =
	    (struct nsm_get_clock_limit_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->clock_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*clock_id = request->clock_id;

	return NSM_SW_SUCCESS;
}

// Get Clock Limit, NSM T3 spec
int encode_get_clock_limit_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code,
				struct nsm_clock_limit *clock_limit,
				struct nsm_msg *msg)
{
	if (msg == NULL || clock_limit == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_CLOCK_LIMIT,
					  msg);
	}

	struct nsm_get_clock_limit_resp *resp =
	    (struct nsm_get_clock_limit_resp *)msg->payload;
	resp->hdr.command = NSM_GET_CLOCK_LIMIT;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_clock_limit);
	resp->hdr.data_size = htole16(data_size);

	htoleClockLimit(clock_limit);
	memcpy(&(resp->clock_limit), clock_limit,
	       sizeof(struct nsm_clock_limit));

	return NSM_SW_SUCCESS;
}

// Get Clock Limit, NSM T3 spec
int decode_get_clock_limit_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *data_size,
				uint16_t *reason_code,
				struct nsm_clock_limit *clock_limit)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    clock_limit == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_clock_limit_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_clock_limit_resp *resp =
	    (struct nsm_get_clock_limit_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	letohClockLimit(&(resp->clock_limit));

	if ((*data_size) < sizeof(struct nsm_clock_limit)) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(clock_limit, &(resp->clock_limit),
	       sizeof(struct nsm_clock_limit));

	return NSM_SW_SUCCESS;
}

// Get Current Clock Frequency command, NSM T3 spec
int encode_get_curr_clock_freq_req(uint8_t instance_id, uint8_t clock_id,
				   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_curr_clock_freq_req *request =
	    (struct nsm_get_curr_clock_freq_req *)msg->payload;

	request->hdr.command = NSM_GET_CURRENT_CLOCK_FREQUENCY;
	request->hdr.data_size = sizeof(clock_id);
	request->clock_id = clock_id;

	return NSM_SW_SUCCESS;
}

int decode_get_curr_clock_freq_req(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *clock_id)
{
	if (msg == NULL || clock_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_curr_clock_freq_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_curr_clock_freq_req *request =
	    (struct nsm_get_curr_clock_freq_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->clock_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*clock_id = request->clock_id;

	return NSM_SW_SUCCESS;
}

// Get Current Clock Frequency command, NSM T3 spec
int encode_get_curr_clock_freq_resp(uint8_t instance_id, uint8_t cc,
				    uint16_t reason_code, uint32_t *clockFreq,
				    struct nsm_msg *msg)
{
	if (msg == NULL || clockFreq == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_CURRENT_CLOCK_FREQUENCY, msg);
	}

	struct nsm_get_curr_clock_freq_resp *resp =
	    (struct nsm_get_curr_clock_freq_resp *)msg->payload;
	resp->hdr.command = NSM_GET_CURRENT_CLOCK_FREQUENCY;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(uint32_t);
	resp->hdr.data_size = htole16(data_size);
	*clockFreq = htole32(*clockFreq);
	memcpy(&(resp->clockFreq), clockFreq, data_size);

	return NSM_SW_SUCCESS;
}

// Get Current Clock Frequency command, NSM T3 spec
int decode_get_curr_clock_freq_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, uint32_t *clockFreq)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    clockFreq == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_curr_clock_freq_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_curr_clock_freq_resp *resp =
	    (struct nsm_get_curr_clock_freq_resp *)msg->payload;

	if (resp->hdr.command != NSM_GET_CURRENT_CLOCK_FREQUENCY) {
		return NSM_SW_ERROR_DATA;
	}

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}
	*clockFreq = le32toh(resp->clockFreq);

	return NSM_SW_SUCCESS;
}

int encode_get_current_clock_event_reason_code_req(uint8_t instance_id,
						   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_CLOCK_EVENT_REASON_CODES;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_current_clock_event_reason_code_req(const struct nsm_msg *msg,
						   size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_current_clock_event_reason_code_resp(uint8_t instance_id,
						    uint8_t cc,
						    uint16_t reason_code,
						    bitfield32_t *flags,
						    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_CLOCK_EVENT_REASON_CODES, msg);
	}

	struct nsm_get_current_clock_event_reason_code_resp *resp =
	    (struct nsm_get_current_clock_event_reason_code_resp *)msg->payload;

	resp->hdr.command = NSM_GET_CLOCK_EVENT_REASON_CODES;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(sizeof(bitfield32_t));

	resp->flags.byte = htole32(flags->byte);
	return NSM_SW_SUCCESS;
}

int decode_get_current_clock_event_reason_code_resp(const struct nsm_msg *msg,
						    size_t msg_len, uint8_t *cc,
						    uint16_t *data_size,
						    uint16_t *reason_code,
						    bitfield32_t *flags)
{
	if (data_size == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_current_clock_event_reason_code_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_current_clock_event_reason_code_resp *resp =
	    (struct nsm_get_current_clock_event_reason_code_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if ((*data_size) < sizeof(bitfield32_t)) {
		return NSM_SW_ERROR_DATA;
	}
	flags->byte = le32toh(resp->flags.byte);
	return NSM_SW_SUCCESS;
}

// Get Accumulated GPU Utilization Command, NSM T3 spec
int encode_get_accum_GPU_util_time_req(uint8_t instance, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

// Get Accumulated GPU Utilization Command, NSM T3 spec
int encode_get_accum_GPU_util_time_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint32_t *context_util_time,
					uint32_t *SM_util_time,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME,
		    msg);
	}

	struct nsm_get_accum_GPU_util_time_resp *resp =
	    (struct nsm_get_accum_GPU_util_time_resp *)msg->payload;
	resp->hdr.command = NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME;
	resp->hdr.completion_code = cc;
	uint16_t data_size = 2 * sizeof(uint32_t);
	resp->hdr.data_size = htole16(data_size);

	resp->context_util_time = htole32(*context_util_time);
	resp->SM_util_time = htole32(*SM_util_time);

	return NSM_SW_SUCCESS;
}

// Get Accumulated GPU Utilization Command, NSM T3 spec
int decode_get_accum_GPU_util_time_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint32_t *context_util_time, uint32_t *SM_util_time)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    context_util_time == NULL || SM_util_time == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_accum_GPU_util_time_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_accum_GPU_util_time_resp *resp =
	    (struct nsm_get_accum_GPU_util_time_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < 2 * sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}
	*context_util_time = le32toh(resp->context_util_time);
	*SM_util_time = le32toh(resp->SM_util_time);

	return NSM_SW_SUCCESS;
}

int encode_get_current_utilization_req(uint8_t instance, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_CURRENT_UTILIZATION;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int encode_get_current_utilization_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint32_t gpu_utilization,
					uint32_t memory_utilization,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_CURRENT_UTILIZATION, msg);
	}

	struct nsm_get_current_utilization_resp *resp =
	    (struct nsm_get_current_utilization_resp *)msg->payload;
	resp->hdr.command = NSM_GET_CURRENT_UTILIZATION;
	resp->hdr.completion_code = cc;
	uint16_t data_size = 2 * sizeof(uint32_t);
	resp->hdr.data_size = htole16(data_size);

	resp->gpu_utilization = htole32(gpu_utilization);
	resp->memory_utilization = htole32(memory_utilization);

	return NSM_SW_SUCCESS;
}

int decode_get_current_utilization_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *data_size,
					uint16_t *reason_code,
					uint32_t *gpu_utilization,
					uint32_t *memory_utilization)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    gpu_utilization == NULL || memory_utilization == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < (sizeof(struct nsm_msg_hdr)) +
			  sizeof(struct nsm_get_current_utilization_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_current_utilization_resp *resp =
	    (struct nsm_get_current_utilization_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < 2 * sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}
	*gpu_utilization = le32toh(resp->gpu_utilization);
	*memory_utilization = le32toh(resp->memory_utilization);

	return NSM_SW_SUCCESS;
}

// Get Row Remap State command, NSM T3 spec
int encode_get_row_remap_state_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ROW_REMAP_STATE_FLAGS;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

// Get Row Remap State command, NSM T3 spec
int decode_get_row_remap_state_req(const struct nsm_msg *msg, size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

// Get Row Remap State command, NSM T3 spec
int encode_get_row_remap_state_resp(uint8_t instance_id, uint8_t cc,
				    uint16_t reason_code, bitfield8_t *flags,
				    struct nsm_msg *msg)
{
	if (msg == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_ROW_REMAP_STATE_FLAGS, msg);
	}

	struct nsm_get_row_remap_state_resp *resp =
	    (struct nsm_get_row_remap_state_resp *)msg->payload;
	resp->hdr.command = NSM_GET_ROW_REMAP_STATE_FLAGS;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(bitfield8_t);
	resp->hdr.data_size = htole16(data_size);

	memcpy(&(resp->flags), flags, data_size);

	return NSM_SW_SUCCESS;
}

// Get Row Remap State command, NSM T3 spec
int decode_get_row_remap_state_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, bitfield8_t *flags)
{
	if (msg == NULL || cc == NULL || data_size == NULL || flags == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_row_remap_state_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_row_remap_state_resp *resp =
	    (struct nsm_get_row_remap_state_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < sizeof(bitfield8_t)) {
		return NSM_SW_ERROR_DATA;
	}
	memcpy(flags, &(resp->flags), sizeof(bitfield8_t));

	return NSM_SW_SUCCESS;
}

// Get Row Remapping Counts, NSM T3 spec
int encode_get_row_remapping_counts_req(uint8_t instance_id,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ROW_REMAPPING_COUNTS;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

// Get Row Remapping Counts, NSM T3 spec
int decode_get_row_remapping_counts_req(const struct nsm_msg *msg,
					size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

// Get Row Remapping Counts, NSM T3 spec
int encode_get_row_remapping_counts_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 uint32_t correctable_error,
					 uint32_t uncorrectable_error,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_ROW_REMAPPING_COUNTS, msg);
	}

	struct nsm_get_row_remapping_counts_resp *resp =
	    (struct nsm_get_row_remapping_counts_resp *)msg->payload;
	resp->hdr.command = NSM_GET_ROW_REMAPPING_COUNTS;
	resp->hdr.completion_code = cc;
	uint16_t data_size = 2 * sizeof(uint32_t);
	resp->hdr.data_size = htole16(data_size);

	resp->correctable_error = htole32(correctable_error);
	resp->uncorrectable_error = htole32(uncorrectable_error);

	return NSM_SW_SUCCESS;
}

// Get Row Remapping Counts, NSM T3 spec
int decode_get_row_remapping_counts_resp(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *cc,
					 uint16_t *data_size,
					 uint16_t *reason_code,
					 uint32_t *correctable_error,
					 uint32_t *uncorrectable_error)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    correctable_error == NULL || uncorrectable_error == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_row_remapping_counts_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_row_remapping_counts_resp *resp =
	    (struct nsm_get_row_remapping_counts_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < 2 * sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*correctable_error = le16toh(resp->correctable_error);
	*uncorrectable_error = le16toh(resp->uncorrectable_error);

	return NSM_SW_SUCCESS;
}

int encode_get_row_remap_availability_req(uint8_t instance_id,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_ROW_REMAP_AVAILABILITY;
	request->data_size = 0;
	return NSM_SW_SUCCESS;
}

int decode_get_row_remap_availability_req(const struct nsm_msg *msg,
					  size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_row_remap_availability_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_row_remap_availability *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_ROW_REMAP_AVAILABILITY, msg);
	}

	struct nsm_get_row_remap_availability_resp *resp =
	    (struct nsm_get_row_remap_availability_resp *)msg->payload;
	resp->hdr.command = NSM_GET_ROW_REMAP_AVAILABILITY;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_row_remap_availability);
	resp->hdr.data_size = htole16(data_size);
	resp->data.high_remapping = htole16(data->high_remapping);
	resp->data.low_remapping = htole16(data->low_remapping);
	resp->data.max_remapping = htole16(data->max_remapping);
	resp->data.no_remapping = htole16(data->no_remapping);
	resp->data.partial_remapping = htole16(data->partial_remapping);
	return NSM_SW_SUCCESS;
}

int decode_get_row_remap_availability_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, struct nsm_row_remap_availability *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_row_remap_availability_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_row_remap_availability_resp *resp =
	    (struct nsm_get_row_remap_availability_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if ((*data_size) < sizeof(struct nsm_row_remap_availability)) {
		return NSM_SW_ERROR_DATA;
	}

	data->high_remapping = le16toh(resp->data.high_remapping);
	data->low_remapping = le16toh(resp->data.low_remapping);
	data->max_remapping = le16toh(resp->data.max_remapping);
	data->no_remapping = le16toh(resp->data.no_remapping);
	data->partial_remapping = le16toh(resp->data.partial_remapping);

	return NSM_SW_SUCCESS;
}

int encode_get_memory_capacity_util_req(uint8_t instance_id,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_MEMORY_CAPACITY_UTILIZATION;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_memory_capacity_util_req(const struct nsm_msg *msg,
					size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_memory_capacity_util_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_memory_capacity_utilization *data, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_MEMORY_CAPACITY_UTILIZATION, msg);
	}

	struct nsm_get_memory_capacity_util_resp *resp =
	    (struct nsm_get_memory_capacity_util_resp *)msg->payload;

	resp->hdr.command = NSM_GET_MEMORY_CAPACITY_UTILIZATION;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size =
	    htole16(sizeof(struct nsm_memory_capacity_utilization));

	resp->data.reserved_memory = htole32(data->reserved_memory);
	resp->data.used_memory = htole32(data->used_memory);

	return NSM_SW_SUCCESS;
}

int decode_get_memory_capacity_util_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, struct nsm_memory_capacity_utilization *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_memory_capacity_util_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_memory_capacity_util_resp *resp =
	    (struct nsm_get_memory_capacity_util_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if (*data_size != sizeof(struct nsm_memory_capacity_utilization)) {
		return NSM_SW_ERROR_DATA;
	}
	data->reserved_memory = le32toh(resp->data.reserved_memory);
	data->used_memory = le32toh(resp->data.used_memory);

	return NSM_SW_SUCCESS;
}

int encode_get_clock_output_enable_state_req(uint8_t instance_id, uint8_t index,
					     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_clock_output_enabled_state_req *request =
	    (struct nsm_get_clock_output_enabled_state_req *)msg->payload;

	request->hdr.command = NSM_GET_CLOCK_OUTPUT_ENABLE_STATE;
	request->hdr.data_size = sizeof(index);
	request->index = index;

	return NSM_SW_SUCCESS;
}

int decode_get_clock_output_enable_state_req(const struct nsm_msg *msg,
					     size_t msg_len, uint8_t *index)
{
	if (msg == NULL || index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_clock_output_enabled_state_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_clock_output_enabled_state_req *request =
	    (struct nsm_get_clock_output_enabled_state_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->index)) {
		return NSM_SW_ERROR_DATA;
	}

	*index = request->index;

	return NSM_SW_SUCCESS;
}

int encode_get_clock_output_enable_state_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      uint32_t clk_buf,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_CLOCK_OUTPUT_ENABLE_STATE, msg);
	}

	struct nsm_get_clock_output_enabled_state_resp *response =
	    (struct nsm_get_clock_output_enabled_state_resp *)msg->payload;

	response->hdr.command = NSM_GET_CLOCK_OUTPUT_ENABLE_STATE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(clk_buf));
	response->clk_buf_data = htole32(clk_buf);

	return NSM_SW_SUCCESS;
}

int decode_get_clock_output_enable_state_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *reason_code,
					      uint16_t *data_size,
					      uint32_t *clk_buf)
{
	if (data_size == NULL || clk_buf == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    (sizeof(struct nsm_msg_hdr) +
	     sizeof(struct nsm_get_clock_output_enabled_state_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_clock_output_enabled_state_resp *resp =
	    (struct nsm_get_clock_output_enabled_state_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < sizeof(*clk_buf)) {
		return NSM_SW_ERROR_DATA;
	}

	*clk_buf = le32toh(resp->clk_buf_data);

	return NSM_SW_SUCCESS;
}

// Set Clock Limit Command, NSM T3 spec
int encode_set_clock_limit_req(uint8_t instance_id, uint8_t clock_id,
			       uint8_t flags, uint32_t limit_min,
			       uint32_t limit_max, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_clock_limit_req *request =
	    (struct nsm_set_clock_limit_req *)msg->payload;

	request->hdr.command = NSM_SET_CLOCK_LIMIT;
	request->hdr.data_size = sizeof(struct nsm_set_clock_limit_req) -
				 sizeof(struct nsm_common_req);
	request->flags = flags;
	request->clock_id = clock_id;
	request->limit_max = htole32(limit_max);
	request->limit_min = htole32(limit_min);
	return NSM_SW_SUCCESS;
}

// Set Clock Limit Command, NSM T3 spec
int decode_set_clock_limit_req(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t *clock_id, uint8_t *flags,
			       uint32_t *limit_min, uint32_t *limit_max)
{
	if (msg == NULL || clock_id == NULL || flags == NULL ||
	    limit_min == NULL || limit_max == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_set_clock_limit_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_clock_limit_req *request =
	    (struct nsm_set_clock_limit_req *)msg->payload;

	if (request->hdr.data_size != sizeof(struct nsm_set_clock_limit_req) -
					  sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*clock_id = request->clock_id;
	*flags = request->flags;
	*limit_max = le32toh(request->limit_max);
	*limit_min = le32toh(request->limit_min);
	return NSM_SW_SUCCESS;
}

int encode_set_clock_limit_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_SET_CLOCK_LIMIT,
					  msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_SET_CLOCK_LIMIT;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_clock_limit_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *data_size,
				uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

int encode_nsm_xid_event(uint8_t instance_id, bool ackr,
			 struct nsm_xid_event_payload payload,
			 const char *message_text, size_t message_text_size,
			 struct nsm_msg *msg)
{
	payload.reason = htole32(payload.reason);
	payload.sequence_number = htole32(payload.sequence_number);
	payload.timestamp = htole64(payload.timestamp);

	uint8_t event_data[NSM_EVENT_DATA_MAX_LEN];
	memcpy(event_data, &payload, sizeof(payload));
	memcpy(&event_data[sizeof(payload)], message_text, message_text_size);

	return encode_nsm_event(
	    instance_id, NSM_TYPE_PLATFORM_ENVIRONMENTAL, ackr,
	    NSM_EVENT_VERSION, NSM_XID_EVENT, NSM_GENERAL_EVENT_CLASS, 0,
	    sizeof(payload) + message_text_size, event_data, msg);
}

int decode_nsm_xid_event(const struct nsm_msg *msg, size_t msg_len,
			 uint8_t *event_class, uint16_t *event_state,
			 struct nsm_xid_event_payload *payload,
			 char *message_text, size_t *message_text_size)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    payload == NULL || message_text == NULL ||
	    message_text_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_event *event = (struct nsm_event *)msg->payload;

	if (event->data_size >
	    msg_len - sizeof(struct nsm_msg_hdr) - NSM_EVENT_MIN_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*event_class = event->event_class;
	*event_state = le16toh(event->event_state);

	if (event->data_size < sizeof(*payload)) {
		return NSM_SW_ERROR_DATA;
	} else if (event->data_size == sizeof(*payload)) {
		*message_text_size = 0;
	} else {
		*message_text_size = event->data_size - sizeof(*payload);

		const uint8_t *text = &event->data[0] + sizeof(*payload);
		memcpy(message_text, text, *message_text_size);
	}

	memcpy(payload, event->data, sizeof(*payload));

	payload->reason = le32toh(payload->reason);
	payload->sequence_number = le32toh(payload->sequence_number);
	payload->timestamp = le64toh(payload->timestamp);

	return NSM_SW_SUCCESS;
}

int encode_query_aggregate_gpm_metrics_req(
    uint8_t instance, uint8_t retrieval_source, uint8_t gpu_instance,
    uint8_t compute_instance, const uint8_t *metrics_bitfield,
    size_t metrics_bitfield_length, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_aggregate_gpm_metrics_req *request =
	    (struct nsm_query_aggregate_gpm_metrics_req *)msg->payload;

	request->hdr.command = NSM_QUERY_AGGREGATE_GPM_METRICS;
	request->hdr.data_size =
	    sizeof(struct nsm_query_aggregate_gpm_metrics_req) -
	    sizeof(struct nsm_common_req) - 1 + metrics_bitfield_length;
	request->retrieval_source = retrieval_source;
	request->gpu_instance = gpu_instance;
	request->compute_instance = compute_instance;
	memcpy(request->metrics_bitfield, metrics_bitfield,
	       metrics_bitfield_length);

	return NSM_SW_SUCCESS;
}

int encode_nsm_reset_required_event(uint8_t instance_id, bool ackr,
				    struct nsm_msg *msg)
{
	return encode_nsm_event(instance_id, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
				ackr, NSM_EVENT_VERSION,
				NSM_RESET_REQUIRED_EVENT,
				NSM_GENERAL_EVENT_CLASS, 0, 0, NULL, msg);
}

int decode_nsm_reset_required_event(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *event_class, uint16_t *event_state)
{
	uint8_t data_size = 0;
	int result = decode_nsm_event(msg, msg_len, NSM_RESET_REQUIRED_EVENT,
				      NSM_GENERAL_EVENT_CLASS, event_state,
				      &data_size, NULL);
	if (data_size > 0) {
		return NSM_SW_ERROR_DATA;
	}
	if (event_class != NULL) {
		*event_class = NSM_GENERAL_EVENT_CLASS;
	}
	return result;
}

int decode_query_aggregate_gpm_metrics_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *retrieval_source,
    uint8_t *gpu_instance, uint8_t *compute_instance,
    const uint8_t **metrics_bitfield, size_t *metrics_bitfield_length)
{
	if (msg == NULL || retrieval_source == NULL || gpu_instance == NULL ||
	    compute_instance == NULL || metrics_bitfield == NULL ||
	    metrics_bitfield_length == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_aggregate_gpm_metrics_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_aggregate_gpm_metrics_req *request =
	    (struct nsm_query_aggregate_gpm_metrics_req *)msg->payload;

	if (request->hdr.data_size <
	    (sizeof(struct nsm_query_aggregate_gpm_metrics_req) -
	     sizeof(struct nsm_common_req))) {
		return NSM_SW_ERROR_DATA;
	}

	*retrieval_source = request->retrieval_source;
	*gpu_instance = request->gpu_instance;
	*compute_instance = request->compute_instance;
	*metrics_bitfield = request->metrics_bitfield;
	*metrics_bitfield_length =
	    request->hdr.data_size -
	    (sizeof(struct nsm_query_aggregate_gpm_metrics_req) -
	     sizeof(struct nsm_common_req) - 1);

	return NSM_SW_SUCCESS;
}

int encode_query_per_instance_gpm_metrics_req(
    uint8_t instance, uint8_t retrieval_source, uint8_t gpu_instance,
    uint8_t compute_instance, uint8_t metric_id, uint32_t instance_bitmask,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_per_instance_gpm_metrics_req *request =
	    (struct nsm_query_per_instance_gpm_metrics_req *)msg->payload;

	request->hdr.command = NSM_QUERY_PER_INSTANCE_GPM_METRICS;
	request->hdr.data_size =
	    sizeof(struct nsm_query_per_instance_gpm_metrics_req) -
	    sizeof(struct nsm_common_req);
	request->retrieval_source = retrieval_source;
	request->gpu_instance = gpu_instance;
	request->compute_instance = compute_instance;
	request->metric_id = metric_id;
	request->instance_bitmask = htole32(instance_bitmask);

	return NSM_SW_SUCCESS;
}

int decode_query_per_instance_gpm_metrics_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *retrieval_source,
    uint8_t *gpu_instance, uint8_t *compute_instance, uint8_t *metric_id,
    uint32_t *instance_bitmask)
{
	if (msg == NULL || retrieval_source == NULL || gpu_instance == NULL ||
	    compute_instance == NULL || metric_id == NULL ||
	    instance_bitmask == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_query_per_instance_gpm_metrics_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_per_instance_gpm_metrics_req *request =
	    (struct nsm_query_per_instance_gpm_metrics_req *)msg->payload;

	if (request->hdr.data_size <
	    (sizeof(struct nsm_query_per_instance_gpm_metrics_req) -
	     sizeof(struct nsm_common_req))) {
		return NSM_SW_ERROR_DATA;
	}

	*retrieval_source = request->retrieval_source;
	*gpu_instance = request->gpu_instance;
	*compute_instance = request->compute_instance;
	*metric_id = request->metric_id;
	*instance_bitmask = le32toh(request->instance_bitmask);

	return NSM_SW_SUCCESS;
}

int encode_aggregate_gpm_metric_percentage_data(double percentage,
						uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (percentage < 0) {
		return NSM_SW_ERROR_DATA;
	}

	// Data type of the percentage in NSM Response is NvS24.8 which is 32
	// bit fixed point signed number with 8 fraction bits. And hence the
	// floating point value value is multiplied by 256 (1 << 8 fraction
	// bits) and converted into integer value.
	uint32_t reading = percentage * (1 << 8);
	reading = htole32(reading);

	memcpy(data, &reading, sizeof(uint32_t));
	*data_len = sizeof(uint32_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_gpm_metric_percentage_data(const uint8_t *data,
						size_t data_len,
						double *percentage)
{
	if (data == NULL || percentage == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint32_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint32_t reading = 0;
	memcpy(&reading, data, sizeof(uint32_t));

	// Data type of the percentage in NSM Response is NvS24.8 which is 32
	// bit fixed point signed number with 8 fraction bits. And hence the
	// value is divided by 256 (1 << 8 fraction bits) in its integer form to
	// obtain a floating point value.
	*percentage = (uint32_t)le32toh(reading) / (double)(1 << 8);

	return NSM_SW_SUCCESS;
}

int encode_aggregate_gpm_metric_bandwidth_data(uint64_t bandwidth,
					       uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint64_t le_reading = htole64(bandwidth);

	memcpy(data, &le_reading, sizeof(uint64_t));
	*data_len = sizeof(uint64_t);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_gpm_metric_bandwidth_data(const uint8_t *data,
					       size_t data_len,
					       uint64_t *bandwidth)
{
	if (data == NULL || bandwidth == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint64_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint64_t le_reading;
	memcpy(&le_reading, data, sizeof(uint64_t));

	*bandwidth = le64toh(le_reading);

	return NSM_SW_SUCCESS;
}

int encode_get_violation_duration_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_GET_VIOLATION_DURATION;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_violation_duration_req(const struct nsm_msg *msg, size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_violation_duration_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code,
				       struct nsm_violation_duration *data,
				       struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_VIOLATION_DURATION, msg);
	}

	struct nsm_get_violation_duration_resp *resp =
	    (struct nsm_get_violation_duration_resp *)msg->payload;

	resp->hdr.command = NSM_GET_VIOLATION_DURATION;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(sizeof(struct nsm_violation_duration));

	resp->data.supported_counter.byte =
	    htole64(data->supported_counter.byte);
	resp->data.hw_violation_duration = htole64(data->hw_violation_duration);
	resp->data.global_sw_violation_duration =
	    htole64(data->global_sw_violation_duration);
	resp->data.power_violation_duration =
	    htole64(data->power_violation_duration);
	resp->data.thermal_violation_duration =
	    htole64(data->thermal_violation_duration);
	resp->data.counter4 = htole64(data->counter4);
	resp->data.counter5 = htole64(data->counter5);
	resp->data.counter6 = htole64(data->counter6);
	resp->data.counter7 = htole64(data->counter7);

	return NSM_SW_SUCCESS;
}

int decode_get_violation_duration_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *data_size,
				       uint16_t *reason_code,
				       struct nsm_violation_duration *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_get_violation_duration_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_violation_duration_resp *resp =
	    (struct nsm_get_violation_duration_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

	if (*data_size != sizeof(struct nsm_violation_duration)) {
		return NSM_SW_ERROR_DATA;
	}
	data->supported_counter.byte =
	    le64toh(resp->data.supported_counter.byte);
	data->hw_violation_duration = le64toh(resp->data.hw_violation_duration);
	data->global_sw_violation_duration =
	    le64toh(resp->data.global_sw_violation_duration);
	data->power_violation_duration =
	    le64toh(resp->data.power_violation_duration);
	data->thermal_violation_duration =
	    le64toh(resp->data.thermal_violation_duration);
	data->counter4 = le64toh(resp->data.counter4);
	data->counter5 = le64toh(resp->data.counter5);
	data->counter6 = le64toh(resp->data.counter6);
	data->counter7 = le64toh(resp->data.counter7);

	return NSM_SW_SUCCESS;
}

static void
htolePowerSmoothingFeat(struct nsm_pwr_smoothing_featureinfo_data *data)
{
	data->feature_flag = htole32(data->feature_flag);
	data->currentTmpSetting = htole32(data->currentTmpSetting);
	data->currentTmpFloorSetting = htole32(data->currentTmpFloorSetting);
	data->maxTmpFloorSettingInPercent =
	    htole16(data->maxTmpFloorSettingInPercent);
	data->minTmpFloorSettingInPercent =
	    htole16(data->minTmpFloorSettingInPercent);
}

static void
letohPowerSmoothingFeat(struct nsm_pwr_smoothing_featureinfo_data *data)
{
	data->feature_flag = le32toh(data->feature_flag);
	data->currentTmpSetting = le32toh(data->currentTmpSetting);
	data->currentTmpFloorSetting = le32toh(data->currentTmpFloorSetting);
	data->maxTmpFloorSettingInPercent =
	    le16toh(data->maxTmpFloorSettingInPercent);
	data->minTmpFloorSettingInPercent =
	    le16toh(data->minTmpFloorSettingInPercent);
}

int encode_get_powersmoothing_featinfo_req(uint8_t instance_id,
					   struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg, NSM_PWR_SMOOTHING_GET_FEATURE_INFO);
}

int decode_get_powersmoothing_featinfo_req(const struct nsm_msg *msg,
					   size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_GET_FEATURE_INFO);
}

int encode_get_powersmoothing_featinfo_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_pwr_smoothing_featureinfo_data *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_PWR_SMOOTHING_GET_FEATURE_INFO, msg);
	}

	struct nsm_get_power_smoothing_feat_resp *response =
	    (struct nsm_get_power_smoothing_feat_resp *)msg->payload;
	response->hdr.command = NSM_PWR_SMOOTHING_GET_FEATURE_INFO;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_pwr_smoothing_featureinfo_data));

	htolePowerSmoothingFeat(data);
	memcpy(&(response->data), data,
	       sizeof(struct nsm_pwr_smoothing_featureinfo_data));

	return NSM_SW_SUCCESS;
}

int decode_get_powersmoothing_featinfo_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_pwr_smoothing_featureinfo_data *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL || data == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr)) +
			   sizeof(struct nsm_get_power_smoothing_feat_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_power_smoothing_feat_resp *resp =
	    (struct nsm_get_power_smoothing_feat_resp *)msg->payload;
	*data_size = le16toh(resp->hdr.data_size);

	if (resp->hdr.command != NSM_PWR_SMOOTHING_GET_FEATURE_INFO) {
		return NSM_ERR_INVALID_DATA;
	}

	size_t nsm_featureinfo_data_length =
	    sizeof(struct nsm_pwr_smoothing_featureinfo_data);
	if (*data_size < nsm_featureinfo_data_length) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	memcpy(data, &(resp->data), nsm_featureinfo_data_length);
	*cc = resp->hdr.completion_code;

	letohPowerSmoothingFeat(data);
	return NSM_SUCCESS;
}

int encode_get_hardware_lifetime_cricuitry_req(uint8_t instance_id,
					       struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg,
	    NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE);
}

int decode_get_hardware_lifetime_cricuitry_req(const struct nsm_msg *msg,
					       size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len,
	    NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE);
}

int encode_get_hardware_lifetime_cricuitry_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_hardwarecircuitry_data *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code,
		    NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE,
		    msg);
	}

	struct nsm_hardwareciruitry_resp *response =
	    (struct nsm_hardwareciruitry_resp *)msg->payload;
	response->hdr.command =
	    NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE;
	response->hdr.completion_code = cc;
	uint16_t data_size = htole16(sizeof(struct nsm_hardwarecircuitry_data));
	response->hdr.data_size = htole16(data_size);
	memcpy(&(response->data), data,
	       sizeof(struct nsm_hardwarecircuitry_data));
	response->data.reading = htole32(response->data.reading);
	return NSM_SW_SUCCESS;
}

int decode_get_hardware_lifetime_cricuitry_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_hardwarecircuitry_data *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL || data == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_hardwareciruitry_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_hardwareciruitry_resp *resp =
	    (struct nsm_hardwareciruitry_resp *)msg->payload;
	*data_size = le16toh(resp->hdr.data_size);

	if (resp->hdr.command !=
	    NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE) {
		return NSM_ERR_INVALID_DATA;
	}

	size_t nsm_hardwarecircuitry_data_length =
	    sizeof(struct nsm_hardwarecircuitry_data);
	if (*data_size < nsm_hardwarecircuitry_data_length) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	memcpy(data, &(resp->data), nsm_hardwarecircuitry_data_length);
	data->reading = le32toh(data->reading);
	*cc = resp->hdr.completion_code;

	return NSM_SUCCESS;
}

int encode_get_current_profile_info_req(uint8_t instance_id,
					struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg,
	    NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION);
}

int decode_get_current_profile_info_req(const struct nsm_msg *msg,
					size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION);
}

static void
htole32CurrentProfileInfoResp(struct nsm_get_current_profile_data *profileInfo)
{
	profileInfo->current_percent_tmp_floor =
	    htole16(profileInfo->current_percent_tmp_floor);
	profileInfo->current_rampup_rate_in_miliwatts_per_second =
	    htole32(profileInfo->current_rampup_rate_in_miliwatts_per_second);
	profileInfo->current_rampdown_rate_in_miliwatts_per_second =
	    htole32(profileInfo->current_rampdown_rate_in_miliwatts_per_second);
	profileInfo->current_rampdown_hysteresis_value_in_milisec =
	    htole32(profileInfo->current_rampdown_hysteresis_value_in_milisec);
}

static void
le32tohCurrentProfileInfoResp(struct nsm_get_current_profile_data *profileInfo)
{
	profileInfo->current_percent_tmp_floor =
	    le16toh(profileInfo->current_percent_tmp_floor);
	profileInfo->current_rampup_rate_in_miliwatts_per_second =
	    le32toh(profileInfo->current_rampup_rate_in_miliwatts_per_second);
	profileInfo->current_rampdown_rate_in_miliwatts_per_second =
	    le32toh(profileInfo->current_rampdown_rate_in_miliwatts_per_second);
	profileInfo->current_rampdown_hysteresis_value_in_milisec =
	    le32toh(profileInfo->current_rampdown_hysteresis_value_in_milisec);
}

int encode_get_current_profile_info_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_get_current_profile_data *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code,
		    NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION, msg);
	}

	struct nsm_get_current_profile_info_resp *response =
	    (struct nsm_get_current_profile_info_resp *)msg->payload;

	response->hdr.command =
	    NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_pwr_smoothing_featureinfo_data));

	htole32CurrentProfileInfoResp(data);

	memcpy(&(response->data), data,
	       sizeof(struct nsm_get_current_profile_data));

	return NSM_SW_SUCCESS;
}

int decode_get_current_profile_info_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_get_current_profile_data *data)
{
	if (data == NULL || data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_get_current_profile_info_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}
	struct nsm_get_current_profile_info_resp *resp_payload =
	    (struct nsm_get_current_profile_info_resp *)msg->payload;

	*data_size = le16toh(resp_payload->hdr.data_size);
	size_t response_data_len = sizeof(struct nsm_get_current_profile_data);
	memcpy(data, &(resp_payload->data), response_data_len);

	// conversion le32toh for each counter data
	le32tohCurrentProfileInfoResp(data);

	return NSM_SW_SUCCESS;
}

int encode_query_admin_override_req(uint8_t instance_id, struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg, NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE);
}

int decode_query_admin_override_req(const struct nsm_msg *msg, size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE);
}

static void htoleAdminOverrideData(struct nsm_admin_override_data *adminData)
{
	adminData->admin_override_percent_tmp_floor =
	    htole16(adminData->admin_override_percent_tmp_floor);
	adminData->admin_override_ramup_rate_in_miliwatts_per_second = htole32(
	    adminData->admin_override_ramup_rate_in_miliwatts_per_second);
	adminData->admin_override_rampdown_rate_in_miliwatts_per_second =
	    htole32(adminData
			->admin_override_rampdown_rate_in_miliwatts_per_second);
	adminData->admin_override_rampdown_hysteresis_value_in_milisec =
	    htole32(
		adminData->admin_override_rampdown_hysteresis_value_in_milisec);
}

static void letohAdminOverrideData(struct nsm_admin_override_data *adminData)
{
	adminData->admin_override_percent_tmp_floor =
	    le16toh(adminData->admin_override_percent_tmp_floor);
	adminData->admin_override_ramup_rate_in_miliwatts_per_second = le32toh(
	    adminData->admin_override_ramup_rate_in_miliwatts_per_second);
	adminData->admin_override_rampdown_rate_in_miliwatts_per_second =
	    le32toh(adminData
			->admin_override_rampdown_rate_in_miliwatts_per_second);
	adminData->admin_override_rampdown_hysteresis_value_in_milisec =
	    le32toh(
		adminData->admin_override_rampdown_hysteresis_value_in_milisec);
}

int encode_query_admin_override_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code,
				     struct nsm_admin_override_data *data,
				     struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE,
		    msg);
	}

	struct nsm_query_admin_override_resp *response =
	    (struct nsm_query_admin_override_resp *)msg->payload;

	response->hdr.command = NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_admin_override_data));

	// conversion htole32 for each admin override data
	htoleAdminOverrideData(data);

	memcpy(&(response->data), data, sizeof(struct nsm_admin_override_data));

	return NSM_SW_SUCCESS;
}

int decode_query_admin_override_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code,
				     uint16_t *data_size,
				     struct nsm_admin_override_data *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_query_admin_override_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_admin_override_resp *resp =
	    (struct nsm_query_admin_override_resp *)msg->payload;
	size_t admin_data_len = sizeof(struct nsm_admin_override_data);

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < admin_data_len) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(data, &(resp->data), admin_data_len);

	// conversion le32toh for each admin override data
	letohAdminOverrideData(data);

	return NSM_SW_SUCCESS;
}

int encode_set_active_preset_profile_req(uint8_t instance_id,
					 uint8_t profile_id,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_active_preset_profile_req *request =
	    (struct nsm_set_active_preset_profile_req *)msg->payload;

	request->hdr.command = NSM_PWR_SMOOTHING_SET_ACTIVE_PRESET_PROFILE;
	request->hdr.data_size = sizeof(profile_id);
	request->profile_id = profile_id;
	return NSM_SW_SUCCESS;
}

int decode_set_active_preset_profile_req(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *profile_id)
{
	if (msg == NULL || profile_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_active_preset_profile_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_active_preset_profile_req *request =
	    (struct nsm_set_active_preset_profile_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->profile_id)) {
		return NSM_SW_ERROR_DATA;
	}

	*profile_id = request->profile_id;

	return NSM_SW_SUCCESS;
}

int encode_set_active_preset_profile_resp(uint8_t instance_id, uint8_t cc,
					  uint16_t reason_code,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code,
		    NSM_PWR_SMOOTHING_SET_ACTIVE_PRESET_PROFILE, msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_PWR_SMOOTHING_SET_ACTIVE_PRESET_PROFILE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_active_preset_profile_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

int encode_setup_admin_override_req(uint8_t instance_id, uint8_t parameter_id,
				    uint32_t param_value, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_setup_admin_override_req *request =
	    (struct nsm_setup_admin_override_req *)msg->payload;

	request->hdr.command = NSM_PWR_SMOOTHING_SETUP_ADMIN_OVERRIDE;
	request->hdr.data_size = sizeof(parameter_id) + sizeof(param_value);
	request->parameter_id = parameter_id;
	request->param_value = htole32(param_value);
	return NSM_SW_SUCCESS;
}

int decode_setup_admin_override_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *parameter_id,
				    uint32_t *param_value)
{
	if (msg == NULL || parameter_id == NULL || param_value == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_setup_admin_override_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_setup_admin_override_req *request =
	    (struct nsm_setup_admin_override_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->parameter_id) + sizeof(request->param_value)) {
		return NSM_SW_ERROR_DATA;
	}

	*parameter_id = request->parameter_id;
	*param_value = le32toh(request->param_value);

	return NSM_SW_SUCCESS;
}

int encode_setup_admin_override_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_PWR_SMOOTHING_SETUP_ADMIN_OVERRIDE,
		    msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_PWR_SMOOTHING_SETUP_ADMIN_OVERRIDE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_setup_admin_override_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

int encode_apply_admin_override_req(uint8_t instance_id, struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg, NSM_PWR_SMOOTHING_APPLY_ADMIN_OVERRIDE);
}

int decode_apply_admin_override_req(const struct nsm_msg *msg, size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_APPLY_ADMIN_OVERRIDE);
}

int encode_apply_admin_override_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_PWR_SMOOTHING_APPLY_ADMIN_OVERRIDE,
		    msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_PWR_SMOOTHING_APPLY_ADMIN_OVERRIDE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_apply_admin_override_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

int encode_toggle_immediate_rampdown_req(uint8_t instance_id,
					 uint8_t ramp_down_toggle,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_toggle_immediate_rampdown_req *request =
	    (struct nsm_toggle_immediate_rampdown_req *)msg->payload;

	request->hdr.command = NSM_PWR_SMOOTHING_TOGGLE_IMMEDIATE_RAMP_DOWN;
	request->hdr.data_size = sizeof(ramp_down_toggle);
	request->ramp_down_toggle = ramp_down_toggle;
	return NSM_SW_SUCCESS;
}

int decode_toggle_immediate_rampdown_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint8_t *ramp_down_toggle)
{
	if (msg == NULL || ramp_down_toggle == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_toggle_immediate_rampdown_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_toggle_immediate_rampdown_req *request =
	    (struct nsm_toggle_immediate_rampdown_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->ramp_down_toggle)) {
		return NSM_SW_ERROR_DATA;
	}

	*ramp_down_toggle = request->ramp_down_toggle;

	return NSM_SW_SUCCESS;
}

int encode_toggle_immediate_rampdown_resp(uint8_t instance_id, uint8_t cc,
					  uint16_t reason_code,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code,
		    NSM_PWR_SMOOTHING_TOGGLE_IMMEDIATE_RAMP_DOWN, msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_PWR_SMOOTHING_TOGGLE_IMMEDIATE_RAMP_DOWN;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_toggle_immediate_rampdown_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

int encode_toggle_feature_state_req(uint8_t instance_id, uint8_t feature_state,
				    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_toggle_feature_state_req *request =
	    (struct nsm_toggle_feature_state_req *)msg->payload;

	request->hdr.command = NSM_PWR_SMOOTHING_TOGGLE_FEATURESTATE;
	request->hdr.data_size = sizeof(feature_state);
	request->feature_state = feature_state;
	return NSM_SW_SUCCESS;
}

int decode_toggle_feature_state_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *feature_state)
{
	if (msg == NULL || feature_state == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_toggle_feature_state_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_toggle_feature_state_req *request =
	    (struct nsm_toggle_feature_state_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->feature_state)) {
		return NSM_SW_ERROR_DATA;
	}

	*feature_state = request->feature_state;

	return NSM_SW_SUCCESS;
}

int encode_toggle_feature_state_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_PWR_SMOOTHING_TOGGLE_FEATURESTATE,
					  msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_PWR_SMOOTHING_TOGGLE_FEATURESTATE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_toggle_feature_state_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

static void letohgetPresetProfiledata(struct nsm_preset_profile_data *data)
{
	data->tmp_floor_setting_in_percent =
	    le16toh(data->tmp_floor_setting_in_percent);
	data->ramp_up_rate_in_miliwattspersec =
	    le32toh(data->ramp_up_rate_in_miliwattspersec);
	data->ramp_down_rate_in_miliwattspersec =
	    le32toh(data->ramp_down_rate_in_miliwattspersec);
	data->ramp_hysterisis_rate_in_miliwattspersec =
	    le32toh(data->ramp_hysterisis_rate_in_miliwattspersec);
}

static void htolegetPresetProfiledata(struct nsm_preset_profile_data *data)
{
	data->tmp_floor_setting_in_percent =
	    htole16(data->tmp_floor_setting_in_percent);
	data->ramp_up_rate_in_miliwattspersec =
	    htole32(data->ramp_up_rate_in_miliwattspersec);
	data->ramp_down_rate_in_miliwattspersec =
	    htole32(data->ramp_down_rate_in_miliwattspersec);
	data->ramp_hysterisis_rate_in_miliwattspersec =
	    htole32(data->ramp_hysterisis_rate_in_miliwattspersec);
}

int encode_get_preset_profile_req(uint8_t instance_id, struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg, NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION);
}

int decode_get_preset_profile_req(const struct nsm_msg *msg, size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION);
}

int encode_get_preset_profile_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_get_all_preset_profile_meta_data *meta_data,
    struct nsm_preset_profile_data *profile_data,
    uint8_t max_number_of_profiles, struct nsm_msg *msg)
{
	if (msg == NULL || profile_data == NULL || meta_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code,
		    NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION, msg);
	}

	struct nsm_get_all_preset_profile_resp *response =
	    (struct nsm_get_all_preset_profile_resp *)msg->payload;

	uint16_t meta_data_size =
	    sizeof(struct nsm_get_all_preset_profile_meta_data);
	uint16_t profile_data_size = sizeof(struct nsm_preset_profile_data);
	// data size is sum of metadata + number of profiles * size of one
	// profile
	uint16_t data_size =
	    meta_data_size + max_number_of_profiles * profile_data_size;

	response->hdr.command =
	    NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(data_size);

	response->data.max_profiles_supported = max_number_of_profiles;
	for (int i = 0; i < max_number_of_profiles; i++) {
		htolegetPresetProfiledata(profile_data + i);
	}
	memcpy(&(response->data.profiles), profile_data,
	       profile_data_size * max_number_of_profiles);
	return NSM_SW_SUCCESS;
}

int decode_get_preset_profile_metadata_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_get_all_preset_profile_meta_data *data,
    uint8_t *number_of_profiles)
{
	if (data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < (sizeof(struct nsm_msg_hdr) +
		       sizeof(struct nsm_get_all_preset_profile_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_all_preset_profile_resp *resp =
	    (struct nsm_get_all_preset_profile_resp *)msg->payload;
	size_t meta_data_len =
	    sizeof(struct nsm_get_all_preset_profile_meta_data);

	uint16_t preset_profile_size = sizeof(struct nsm_preset_profile_data);
	uint16_t expected_data_size =
	    preset_profile_size * (data->max_profiles_supported);

	if (le16toh(resp->hdr.data_size) < expected_data_size) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(data, &(resp->data), meta_data_len);
	*number_of_profiles = data->max_profiles_supported;

	return NSM_SW_SUCCESS;
}

int decode_get_preset_profile_data_from_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint8_t max_profiles_supported, uint8_t profile_id,
    struct nsm_preset_profile_data *profile_data)
{
	if (profile_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < (sizeof(struct nsm_msg_hdr) +
		       sizeof(struct nsm_get_all_preset_profile_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_all_preset_profile_resp *resp =
	    (struct nsm_get_all_preset_profile_resp *)msg->payload;

	uint16_t preset_profile_size = sizeof(struct nsm_preset_profile_data);
	uint16_t expected_data_size =
	    preset_profile_size * (max_profiles_supported);

	if (le16toh(resp->hdr.data_size) < expected_data_size) {
		return NSM_SW_ERROR_DATA;
	}
	uint8_t profile_data_size = sizeof(struct nsm_preset_profile_data);

	memcpy(profile_data,
	       &(resp->data.profiles) + profile_id * profile_data_size,
	       profile_data_size);

	// conversion le to he
	letohgetPresetProfiledata(profile_data);

	return NSM_SW_SUCCESS;
}
int encode_update_preset_profile_param_req(uint8_t instance_id,
					   uint8_t profile_id,
					   uint8_t parameter_id,
					   uint32_t param_value,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_update_preset_profile_req *request =
	    (struct nsm_update_preset_profile_req *)msg->payload;

	request->hdr.command =
	    NSM_PWR_SMOOTHING_UPDATE_PRESET_PROFILE_PARAMETERS;
	request->hdr.data_size = sizeof(parameter_id) + sizeof(param_value);
	request->profile_id = profile_id;
	request->parameter_id = parameter_id;
	request->param_value = htole32(param_value);
	return NSM_SW_SUCCESS;
}

int decode_update_preset_profile_param_req(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *profile_id,
					   uint8_t *parameter_id,
					   uint32_t *param_value)
{
	if (msg == NULL || parameter_id == NULL || param_value == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_update_preset_profile_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_update_preset_profile_req *request =
	    (struct nsm_update_preset_profile_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->parameter_id) + sizeof(request->param_value)) {
		return NSM_SW_ERROR_DATA;
	}
	*profile_id = request->profile_id;
	*parameter_id = request->parameter_id;
	*param_value = le32toh(request->param_value);

	return NSM_SW_SUCCESS;
}

int encode_update_preset_profile_param_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code,
		    NSM_PWR_SMOOTHING_UPDATE_PRESET_PROFILE_PARAMETERS, msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_PWR_SMOOTHING_UPDATE_PRESET_PROFILE_PARAMETERS;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_update_preset_profile_param_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

double NvUFXP4_12ToDouble(uint16_t reading)
{
	double value = reading / (double)(1 << 12);
	return value;
}

uint16_t doubleToNvUFXP4_12(double reading)
{
	uint16_t value = reading * (1 << 12);
	return value;
}

double NvUFXP8_24ToDouble(uint32_t reading)
{
	double value = reading / (double)(1 << 24);
	return value;
}

uint32_t doubleToNvUFXP8_24(double reading)
{
	uint32_t value = reading * (1 << 24);
	return value;
}

//  Enable Workload Power Profiles
int encode_enable_workload_power_profile_req(uint8_t instance_id,
					     bitfield256_t profile_mask,
					     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_enable_workload_power_profile_req *request =
	    (struct nsm_enable_workload_power_profile_req *)msg->payload;

	request->hdr.command = NSM_ENABLE_WORKLOAD_POWER_PROFILE;
	request->hdr.data_size = sizeof(bitfield256_t);
	for (int i = 0; i < 8; i++) {
		request->profile_mask.fields[i].byte =
		    htole32(profile_mask.fields[i].byte);
	}

	return NSM_SW_SUCCESS;
}

int decode_enable_workload_power_profile_req(const struct nsm_msg *msg,
					     size_t msg_len,
					     bitfield256_t *profile_mask)
{
	if (msg == NULL || profile_mask == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + sizeof(bitfield256_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_enable_workload_power_profile_req *request =
	    (struct nsm_enable_workload_power_profile_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->profile_mask)) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(profile_mask, &(request->profile_mask), sizeof(bitfield256_t));
	for (int i = 0; i < 8; i++) {
		profile_mask->fields[i].byte =
		    le32toh(profile_mask->fields[i].byte);
	}
	return NSM_SW_SUCCESS;
}

int encode_enable_workload_power_profile_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_ENABLE_WORKLOAD_POWER_PROFILE, msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_ENABLE_WORKLOAD_POWER_PROFILE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_enable_workload_power_profile_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

//  Enable Workload Power Profiles
int encode_disable_workload_power_profile_req(uint8_t instance_id,
					      bitfield256_t profile_mask,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_disable_workload_power_profile_req *request =
	    (struct nsm_disable_workload_power_profile_req *)msg->payload;

	request->hdr.command = NSM_DISABLE_WORKLOAD_POWER_PROFILE;
	request->hdr.data_size = sizeof(bitfield256_t);
	for (int i = 0; i < 8; i++) {
		request->profile_mask.fields[i].byte =
		    htole32(profile_mask.fields[i].byte);
	}

	return NSM_SW_SUCCESS;
}

int decode_disable_workload_power_profile_req(const struct nsm_msg *msg,
					      size_t msg_len,
					      bitfield256_t *profile_mask)
{
	if (msg == NULL || profile_mask == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) + sizeof(bitfield256_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_disable_workload_power_profile_req *request =
	    (struct nsm_disable_workload_power_profile_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->profile_mask)) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(profile_mask, &(request->profile_mask), sizeof(bitfield32_t[8]));
	for (int i = 0; i < 8; i++) {
		profile_mask->fields[i].byte =
		    le32toh(profile_mask->fields[i].byte);
	}
	return NSM_SW_SUCCESS;
}

int encode_disable_workload_power_profile_resp(uint8_t instance_id, uint8_t cc,
					       uint16_t reason_code,
					       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_DISABLE_WORKLOAD_POWER_PROFILE, msg);
	}
	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = NSM_ENABLE_WORKLOAD_POWER_PROFILE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_disable_workload_power_profile_resp(const struct nsm_msg *msg,
					       size_t msg_len, uint8_t *cc,
					       uint16_t *reason_code)
{
	if (msg == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->data_size);
	if (data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	*cc = resp->completion_code;
	return NSM_SUCCESS;
}

int encode_get_workload_power_profile_status_req(uint8_t instance_id,
						 struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg, NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO);
}

int decode_get_workload_power_profile_status_req(const struct nsm_msg *msg,
						 size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO);
}

static void
htole32PresetProfileResp(struct workload_power_profile_status *profileInfo)
{
	for (int i = 0; i < 8; i++) {
		profileInfo->supported_profile_mask.fields[i].byte =
		    htole32(profileInfo->supported_profile_mask.fields[i].byte);
		profileInfo->requested_profile_maks.fields[i].byte =
		    htole32(profileInfo->requested_profile_maks.fields[i].byte);
		profileInfo->enforced_profile_mask.fields[i].byte =
		    htole32(profileInfo->enforced_profile_mask.fields[i].byte);
	}
}

static void
le32tohPresetProfileResp(struct workload_power_profile_status *profileInfo)
{
	for (int i = 0; i < 8; i++) {
		profileInfo->supported_profile_mask.fields[i].byte =
		    le32toh(profileInfo->supported_profile_mask.fields[i].byte);
		profileInfo->requested_profile_maks.fields[i].byte =
		    le32toh(profileInfo->requested_profile_maks.fields[i].byte);
		profileInfo->enforced_profile_mask.fields[i].byte =
		    le32toh(profileInfo->enforced_profile_mask.fields[i].byte);
	}
}

int encode_get_workload_power_profile_status_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct workload_power_profile_status *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO,
		    msg);
	}

	struct nsm_get_workload_power_profile_status_info_resp *response =
	    (struct nsm_get_workload_power_profile_status_info_resp *)
		msg->payload;

	response->hdr.command = NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct workload_power_profile_status));

	htole32PresetProfileResp(data);

	memcpy(&(response->data), data,
	       sizeof(struct workload_power_profile_status));

	return NSM_SW_SUCCESS;
}

int decode_get_workload_power_profile_status_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct workload_power_profile_status *data)
{
	if (data == NULL || data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    (sizeof(struct nsm_msg_hdr) +
	     sizeof(struct nsm_get_workload_power_profile_status_info_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}
	struct nsm_get_workload_power_profile_status_info_resp *resp_payload =
	    (struct nsm_get_workload_power_profile_status_info_resp *)
		msg->payload;

	*data_size = le16toh(resp_payload->hdr.data_size);
	size_t response_data_len = sizeof(struct workload_power_profile_status);
	memcpy(data, &(resp_payload->data), response_data_len);

	// conversion le32toh
	le32tohPresetProfileResp(data);

	return NSM_SW_SUCCESS;
}

static void
letohgetWorkloadPresetProfiledata(struct nsm_workload_power_profile_data *data)
{
	data->profile_id = le16toh(data->profile_id);
	data->priority = le16toh(data->priority);
	for (int i = 0; i < 8; i++) {
		data->conflict_mask.fields[i].byte =
		    le32toh(data->conflict_mask.fields[i].byte);
	}
}

static void
htolegetWorkloadPresetProfiledata(struct nsm_workload_power_profile_data *data)
{
	data->profile_id = htole16(data->profile_id);
	data->priority = htole16(data->priority);
	for (int i = 0; i < 8; i++) {
		data->conflict_mask.fields[i].byte =
		    htole32(data->conflict_mask.fields[i].byte);
	}
}

int encode_get_workload_power_profile_info_req(uint8_t instance_id,
					       uint16_t identifier,
					       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_workload_power_profile_info_req *request =
	    (struct nsm_get_workload_power_profile_info_req *)msg->payload;

	request->hdr.command = NSM_GET_WORKLOAD_POWER_PROFILE_INFO;
	request->hdr.data_size = sizeof(identifier);
	request->identifier = identifier;
	return NSM_SW_SUCCESS;
}

int decode_get_workload_power_profile_info_req(const struct nsm_msg *msg,
					       size_t msg_len,
					       uint16_t *identifier)
{
	if (msg == NULL || identifier == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_workload_power_profile_info_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_workload_power_profile_info_req *request =
	    (struct nsm_get_workload_power_profile_info_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->identifier)) {
		return NSM_SW_ERROR_DATA;
	}

	*identifier = request->identifier;

	return NSM_SW_SUCCESS;
}

int encode_get_workload_power_profile_info_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_all_workload_power_profile_meta_data *meta_data,
    struct nsm_workload_power_profile_data *profile_data,
    uint8_t number_of_profiles, struct nsm_msg *msg)
{
	if (msg == NULL || profile_data == NULL || meta_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_GET_WORKLOAD_POWER_PROFILE_INFO, msg);
	}

	struct nsm_workload_power_profile_get_all_preset_profile_resp
	    *response =
		(struct nsm_workload_power_profile_get_all_preset_profile_resp
		     *)msg->payload;

	uint16_t meta_data_size =
	    sizeof(struct nsm_all_workload_power_profile_meta_data);
	uint16_t profile_data_size =
	    sizeof(struct nsm_workload_power_profile_data);
	// data size is sum of metadata + number of profiles * size of one
	// profile
	uint16_t data_size =
	    meta_data_size + number_of_profiles * profile_data_size;

	response->hdr.command = NSM_GET_WORKLOAD_POWER_PROFILE_INFO;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(data_size);
	response->data.next_identifier = htole16(meta_data->next_identifier);
	response->data.number_of_profiles = number_of_profiles;
	for (int i = 0; i < number_of_profiles; i++) {
		htolegetWorkloadPresetProfiledata(profile_data + i);
	}
	memcpy(&(response->data.profiles), profile_data,
	       profile_data_size * number_of_profiles);
	return NSM_SW_SUCCESS;
}

int decode_get_workload_power_profile_info_metadata_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_all_workload_power_profile_meta_data *data,
    uint8_t *number_of_profiles)
{
	if (data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    (sizeof(struct nsm_msg_hdr) +
	     sizeof(struct nsm_all_workload_power_profile_resp_meta_data))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_workload_power_profile_get_all_preset_profile_resp *resp =
	    (struct nsm_workload_power_profile_get_all_preset_profile_resp *)
		msg->payload;
	size_t meta_data_len =
	    sizeof(struct nsm_all_workload_power_profile_meta_data);

	uint16_t preset_profile_size =
	    sizeof(struct nsm_workload_power_profile_data);
	uint16_t expected_data_size =
	    preset_profile_size * (resp->data.number_of_profiles);

	if (le16toh(resp->hdr.data_size) < expected_data_size) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(data, &(resp->data), meta_data_len);
	*number_of_profiles = data->number_of_profiles;
	data->next_identifier = le16toh(data->next_identifier);

	return NSM_SW_SUCCESS;
}

int decode_get_workload_power_profile_info_data_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint8_t max_profiles_supported, uint8_t offset,
    struct nsm_workload_power_profile_data *profile_data)
{
	if (profile_data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < (sizeof(struct nsm_msg_hdr) +
		       sizeof(struct nsm_get_all_preset_profile_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_workload_power_profile_get_all_preset_profile_resp *resp =
	    (struct nsm_workload_power_profile_get_all_preset_profile_resp *)
		msg->payload;

	uint16_t preset_profile_size =
	    sizeof(struct nsm_workload_power_profile_data);
	uint16_t expected_data_size =
	    preset_profile_size * (max_profiles_supported);

	if (le16toh(resp->hdr.data_size) < expected_data_size) {
		return NSM_SW_ERROR_DATA;
	}
	uint8_t profile_data_size =
	    sizeof(struct nsm_workload_power_profile_data);

	memcpy(profile_data,
	       &(resp->data.profiles) + offset * profile_data_size,
	       profile_data_size);

	// conversion le to he
	letohgetWorkloadPresetProfiledata(profile_data);

	return NSM_SW_SUCCESS;
}
