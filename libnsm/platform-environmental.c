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

	struct nsm_get_temperature_reading_req *request =
	    (struct nsm_get_temperature_reading_req *)msg->payload;

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
			  sizeof(struct nsm_get_temperature_reading_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_temperature_reading_req *request =
	    (struct nsm_get_temperature_reading_req *)msg->payload;

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
		return encode_reason_code(cc, reason_code,
					  NSM_GET_TEMPERATURE_READING, msg);
	}

	struct nsm_get_current_power_draw_resp *response =
	    (struct nsm_get_current_power_draw_resp *)msg->payload;

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
			  sizeof(struct nsm_get_current_power_draw_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_current_power_draw_resp *response =
	    (struct nsm_get_current_power_draw_resp *)msg->payload;

	*cc = response->hdr.completion_code;

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
