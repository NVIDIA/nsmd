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

int encode_get_power_supply_status_req(uint8_t instance_id, struct nsm_msg *msg)
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

	request->command = NSM_GET_POWER_SUPPLY_STATUS;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_power_supply_status_req(const struct nsm_msg *msg,
				       size_t msg_len)
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

int encode_get_power_supply_status_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint8_t power_supply_status,
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
					  NSM_GET_POWER_SUPPLY_STATUS, msg);
	}

	struct nsm_get_power_supply_status_resp *response =
	    (struct nsm_get_power_supply_status_resp *)msg->payload;

	response->hdr.command = NSM_GET_POWER_SUPPLY_STATUS;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(uint32_t));
	response->power_supply_status = power_supply_status;
	response->reserved1 = 0;
	response->reserved2 = 0;
	response->reserved3 = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_power_supply_status_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					uint8_t *power_supply_status)
{
	if (power_supply_status == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_power_supply_status_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_power_supply_status_resp *response =
	    (struct nsm_get_power_supply_status_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);

	if (data_size != sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*power_supply_status = response->power_supply_status;

	return NSM_SW_SUCCESS;
}

int encode_get_gpu_presence_and_power_status_req(uint8_t instance_id,
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

	request->command = NSM_GET_GPU_PRESENCE_POWER_STATUS;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_gpu_presence_and_power_status_req(const struct nsm_msg *msg,
						 size_t msg_len)
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

int encode_get_gpu_presence_and_power_status_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code, uint8_t presence,
    uint8_t power_status, struct nsm_msg *msg)
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
		    cc, reason_code, NSM_GET_GPU_PRESENCE_POWER_STATUS, msg);
	}

	struct nsm_get_gpu_presence_and_power_status_resp *response =
	    (struct nsm_get_gpu_presence_and_power_status_resp *)msg->payload;

	response->hdr.command = NSM_GET_GPU_PRESENCE_POWER_STATUS;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(uint32_t));

	response->presence = presence;
	response->power_status = power_status;
	response->reserved1 = 0;
	response->reserved2 = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_gpu_presence_and_power_status_resp(const struct nsm_msg *msg,
						  size_t msg_len, uint8_t *cc,
						  uint16_t *reason_code,
						  uint8_t *presence,
						  uint8_t *power_status)
{
	if (presence == NULL || power_status == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_gpu_presence_and_power_status_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_gpu_presence_and_power_status_resp *response =
	    (struct nsm_get_gpu_presence_and_power_status_resp *)msg->payload;

	uint16_t data_size = le16toh(response->hdr.data_size);

	if (data_size != sizeof(uint32_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*presence = response->presence;
	*power_status = response->power_status;

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
