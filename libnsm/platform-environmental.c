#include "platform-environmental.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_get_inventory_information_req(uint8_t instance_id,
					 uint32_t transfer_handle,
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
	request->data_size = 6;
	request->arg1 = 0;
	request->arg2 = 0;
	request->transfer_handle = htole32(transfer_handle);

	return NSM_SUCCESS;
}

int decode_get_inventory_information_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint32_t *transfer_handle)
{
	if (msg == NULL || transfer_handle == NULL) {
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

	*transfer_handle = request->transfer_handle;

	return NSM_SUCCESS;
}

int encode_get_inventory_information_resp(uint8_t instance_id, uint8_t cc,
					  uint32_t next_transfer_handle,
					  const uint8_t *inventory_information,
					  uint32_t inventory_information_len,
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

	struct nsm_get_inventory_information_resp *response =
	    (struct nsm_get_inventory_information_resp *)msg->payload;

	response->command = NSM_GET_INVENTORY_INFORMATION;
	response->completion_code = cc;

	uint16_t data_size =
	    sizeof(next_transfer_handle) + inventory_information_len;
	response->next_transfer_handle = next_transfer_handle;
	response->data_size = htole16(data_size);

	if (cc == NSM_SUCCESS) {
		{
			if (inventory_information == NULL) {
				return NSM_ERR_INVALID_DATA;
			}
		}
		memcpy(response->inventory_information, inventory_information,
		       inventory_information_len);
	}

	return NSM_SUCCESS;
}

int decode_get_inventory_information_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *data_size,
					  uint32_t *next_transfer_handle,
					  uint8_t *inventory_information)
{
	if (msg == NULL || cc == NULL || data_size == NULL) {
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

	if (*data_size < (sizeof(struct nsm_get_inventory_information_resp) -
			  NSM_RESPONSE_CONVENTION_LEN - 1)) {
		return NSM_ERR_INVALID_DATA;
	}

	*next_transfer_handle = resp->next_transfer_handle;

	size_t inventory_information_length = *data_size - sizeof(uint32_t);
	// exclude next_transfer_handle(uint32_t)

	memcpy(inventory_information, resp->inventory_information,
	       inventory_information_length);

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
	request->data_size = 2;
	request->arg1 = sensor_id;
	request->arg2 = 0;

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

	if (request->data_size < 2) {
		return NSM_ERR_INVALID_DATA;
	}

	*sensor_id = request->arg1;

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
	printf("reading=0x%08x\n", reading);
	response->reading = htole32(reading);

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
		*((uint32_t*)temperature_reading) = le32toh(reading);
	}

	return NSM_SUCCESS;
}