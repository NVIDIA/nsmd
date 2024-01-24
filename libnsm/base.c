
#include "base.h"

#include <endian.h>
#include <string.h>

uint8_t pack_nsm_header(const struct nsm_header_info *hdr,
			struct nsm_msg_hdr *msg)
{
	if (msg == NULL || hdr == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (hdr->nsm_msg_type != NSM_RESPONSE &&
	    hdr->nsm_msg_type != NSM_REQUEST &&
	    hdr->nsm_msg_type != NSM_EVENT) {
		return NSM_ERR_INVALID_DATA;
	}

	if (hdr->instance_id > NSM_INSTANCE_MAX) {
		return NSM_ERR_INVALID_DATA;
	}

	uint8_t datagram = (hdr->nsm_msg_type == NSM_EVENT) ? 1 : 0;

	if (hdr->nsm_msg_type == NSM_RESPONSE) {
		msg->request = 0;
	} else if (hdr->nsm_msg_type == NSM_REQUEST ||
		   hdr->nsm_msg_type == NSM_EVENT) {
		msg->request = 1;
	}

	msg->pci_vendor_id = htobe16(PCI_VENDOR_ID);
	msg->datagram = datagram;
	msg->reserved = 0;
	msg->instance_id = hdr->instance_id;
	msg->ocp_type = OCP_TYPE;
	msg->ocp_version = OCP_VERSION;
	msg->nvidia_msg_type = hdr->nvidia_msg_type;

	return NSM_SUCCESS;
}

uint8_t unpack_nsm_header(const struct nsm_msg_hdr *msg,
			  struct nsm_header_info *hdr)
{
	if (msg == NULL || hdr == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (be16toh(msg->pci_vendor_id) != PCI_VENDOR_ID) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg->ocp_type != OCP_TYPE) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg->ocp_version != OCP_VERSION) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg->request == NSM_RESPONSE) {
		hdr->nsm_msg_type = NSM_RESPONSE;
	} else {
		hdr->nsm_msg_type = msg->datagram ? NSM_EVENT : NSM_REQUEST;
	}

	hdr->instance_id = msg->instance_id;
	hdr->nvidia_msg_type = msg->nvidia_msg_type;

	return NSM_SUCCESS;
}

int encode_cc_only_resp(uint8_t instance_id, uint8_t type, uint8_t command,
			uint8_t cc, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = type;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_common_resp *response =
	    (struct nsm_common_resp *)msg->payload;

	response->command = command;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SUCCESS;
}

int encode_ping_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_common_req *request = (struct nsm_common_req *)msg->payload;

	request->command = NSM_PING;
	request->data_size = 0;

	return NSM_SUCCESS;
}

int encode_ping_resp(uint8_t instance_id, struct nsm_msg *msg)
{
	return encode_cc_only_resp(instance_id,
				   NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
				   NSM_PING, NSM_SUCCESS, msg);
}

int decode_ping_resp(const struct nsm_msg *msg, size_t msgLen, uint8_t *cc)
{
	if (msg == NULL || msgLen < 1) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	*cc = resp->completion_code;

	if (resp->data_size != 0) {
		return NSM_ERR_INVALID_DATA_LENGTH;
	}

	return NSM_SUCCESS;
}

int encode_get_supported_nvidia_message_types_req(uint8_t instance_id,
						  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_supported_nvidia_message_types_req *request =
	    (struct nsm_get_supported_nvidia_message_types_req *)msg->payload;

	request->command = NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES;
	request->data_size = 0;

	return NSM_SUCCESS;
}

int encode_get_supported_nvidia_message_types_resp(uint8_t instance_id,
						   const bitfield8_t *types,
						   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_supported_nvidia_message_types_resp *response =
	    (struct nsm_get_supported_nvidia_message_types_resp *)msg->payload;

	response->command = NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES;
	response->completion_code = NSM_SUCCESS;
	response->data_size = 32;

	memcpy(response->supported_nvidia_message_types, types, 32);

	return NSM_SUCCESS;
}

int decode_get_supported_nvidia_message_types_resp(const struct nsm_msg *msg,
						   size_t msg_len, uint8_t *cc,
						   bitfield8_t *types)
{
	if (msg == NULL || types == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len <
	    sizeof(struct nsm_get_supported_nvidia_message_types_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_supported_nvidia_message_types_resp *resp =
	    (struct nsm_get_supported_nvidia_message_types_resp *)msg->payload;

	*cc = resp->completion_code;
	if (NSM_SUCCESS != *cc) {
		return NSM_SUCCESS;
	}

	memcpy(&(types->byte), resp->supported_nvidia_message_types, 32);

	return NSM_SUCCESS;
}

int encode_get_supported_command_codes_req(uint8_t instance_id,
					   uint8_t nvidia_message_type,
					   struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_supported_command_codes_req *request =
	    (struct nsm_get_supported_command_codes_req *)msg->payload;

	request->command = NSM_SUPPORTED_COMMAND_CODES;
	request->data_size = 1;
	request->nvidia_message_type = nvidia_message_type;

	return NSM_SUCCESS;
}

int encode_get_supported_command_codes_resp(uint8_t instance_id,
					    const bitfield8_t *command_codes,
					    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_get_supported_command_codes_resp *response =
	    (struct nsm_get_supported_command_codes_resp *)msg->payload;

	response->command = NSM_SUPPORTED_COMMAND_CODES;
	response->completion_code = NSM_SUCCESS;
	response->data_size = htole16(32);

	if (command_codes == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	memcpy(response->supported_command_codes, command_codes, 32);

	return NSM_SUCCESS;
}

int decode_get_supported_command_codes_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    bitfield8_t *command_codes)
{
	if (msg == NULL || command_codes == NULL || cc == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_supported_command_codes_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_get_supported_command_codes_resp *resp =
	    (struct nsm_get_supported_command_codes_resp *)msg->payload;

	*cc = resp->completion_code;
	if (NSM_SUCCESS != *cc) {
		return NSM_SUCCESS;
	}

	memcpy(&(command_codes->byte), resp->supported_command_codes, 32);

	return NSM_SUCCESS;
}

int encode_nsm_query_device_identification_req(uint8_t instance_id,
					       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_query_device_identification_req *request =
	    (struct nsm_query_device_identification_req *)msg->payload;

	request->command = NSM_QUERY_DEVICE_IDENTIFICATION;
	request->data_size = 0;

	return NSM_SUCCESS;
}

int encode_query_device_identification_resp(uint8_t instance,
					    const uint8_t device_identification,
					    const uint8_t device_instance,
					    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance & 0x1f;
	header.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}

	struct nsm_query_device_identification_resp *response =
	    (struct nsm_query_device_identification_resp *)msg->payload;

	response->command = NSM_QUERY_DEVICE_IDENTIFICATION;
	response->completion_code = NSM_SUCCESS;
	response->data_size = htole16(2);

	response->device_identification = device_identification;
	response->instance_id = device_instance;

	return NSM_SUCCESS;
}

int decode_query_device_identification_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint8_t *device_identification,
					    uint8_t *device_instance)
{
	if (msg == NULL || cc == NULL || device_identification == NULL ||
	    device_instance == NULL) {
		return NSM_ERR_INVALID_DATA;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_device_identification_resp)) {
		return NSM_ERR_INVALID_DATA;
	}

	struct nsm_query_device_identification_resp *response =
	    (struct nsm_query_device_identification_resp *)msg->payload;

	*cc = response->completion_code;
	*device_identification = response->device_identification;
	*device_instance = response->instance_id;

	return NSM_SUCCESS;
}