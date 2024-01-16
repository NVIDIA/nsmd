#include "platform-environmental.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_get_inventory_information_req(uint8_t instance_id,
					 uint8_t property_identifier,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_inventory_information_req *request =
	    (struct nsm_get_inventory_information_req *)msg->payload;

	request->command = NSM_GET_INVENTORY_INFORMATION;
	request->data_size = 1;
	request->property_identifier = property_identifier;

	return NSM_SUCCESS;
}

int decode_get_inventory_information_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint8_t *property_identifier)
{
	if (msg == NULL || property_identifier == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_inventory_information_req)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_inventory_information_req *request =
	    (struct nsm_get_inventory_information_req *)msg->payload;

	if (request->data_size <
	    sizeof(struct nsm_get_inventory_information_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_ERR_INVALID_DATA;
	}

	*property_identifier = request->property_identifier;

	return NSM_SUCCESS;
}

int encode_get_inventory_information_resp(uint8_t instance_id, uint8_t cc,
					  const uint16_t data_size,
					  const uint8_t *inventory_information,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_inventory_information_resp *resp =
	    (struct nsm_get_inventory_information_resp *)msg->payload;

	resp->command = NSM_GET_INVENTORY_INFORMATION;
	resp->completion_code = cc;
	resp->data_size = htole16(data_size);

	if (cc == NSM_SUCCESS) {
		{
			if (inventory_information == NULL) {
				return NSM_ERR_INVALID_DATA;
			}
		}
		memcpy(resp->inventory_information, inventory_information,
		       data_size);
	}

	return NSM_SUCCESS;
}

int decode_get_inventory_information_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *data_size,
					  uint8_t *inventory_information)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    inventory_information == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_inventory_information_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_inventory_information_resp *resp =
	    (struct nsm_get_inventory_information_resp *)msg->payload;

	*cc = resp->completion_code;
	if (NSM_SUCCESS != *cc) {
		return NSM_SUCCESS;
	}

	*data_size = le16toh(resp->data_size);
	memcpy(inventory_information, resp->inventory_information, *data_size);

	return NSM_SUCCESS;
}

int encode_get_temperature_reading_req(uint8_t instance_id, uint8_t sensor_id,
				       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_temperature_reading_req *request =
	    (struct nsm_get_temperature_reading_req *)msg->payload;

	request->command = NSM_GET_TEMPERATURE_READING;
	request->data_size = sizeof(sensor_id);
	request->sensor_id = sensor_id;

	return NSM_SUCCESS;
}

int decode_get_temperature_reading_req(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *sensor_id)
{
	if (msg == NULL || sensor_id == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_temperature_reading_req)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_temperature_reading_req *request =
	    (struct nsm_get_temperature_reading_req *)msg->payload;

	if (request->data_size < sizeof(request->sensor_id)) {
		return NSM_ERR_INVALID_DATA;
	}

	*sensor_id = request->sensor_id;

	return NSM_SUCCESS;
}

int encode_get_temperature_reading_resp(uint8_t instance_id, uint8_t cc,
					real32_t temperature_reading,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_temperature_reading_resp *response =
	    (struct nsm_get_temperature_reading_resp *)msg->payload;

	response->command = NSM_GET_TEMPERATURE_READING;
	response->completion_code = cc;
	response->data_size = htole16(sizeof(uint32_t));

	uint32_t reading = 0;
	memcpy(&reading, &temperature_reading, sizeof(uint32_t));
	reading = htole32(reading);
	memcpy(&response->reading, &reading, sizeof(uint32_t));

	return NSM_SUCCESS;
}

int decode_get_temperature_reading_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					real32_t *temperature_reading)
{
	if (msg == NULL || cc == NULL || temperature_reading == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_temperature_reading_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_temperature_reading_resp *response =
	    (struct nsm_get_temperature_reading_resp *)msg->payload;

	*cc = response->completion_code;
	uint16_t data_size = le16toh(response->data_size);
	if (data_size == sizeof(uint32_t)) {
		uint32_t reading = 0;
		memcpy(&reading, &response->reading, sizeof(uint32_t));
		reading = le32toh(reading);
		memcpy(temperature_reading, &reading, sizeof(uint32_t));
	}

	return NSM_SUCCESS;
}