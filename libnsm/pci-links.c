#include "pci-links.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_query_scalar_group_telemetry_v1_req(uint8_t instance_id,
					       uint8_t device_id,
					       uint8_t group_index,
					       struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_PCI_LINK;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_scalar_group_telemetry_v1_req *request =
	    (struct nsm_query_scalar_group_telemetry_v1_req *)msg->payload;

	request->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	request->hdr.data_size = 2;
	request->device_id = device_id;
	request->group_index = group_index;

	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_req(const struct nsm_msg *msg,
					       size_t msg_len,
					       uint8_t *device_id,
					       uint8_t *group_index)
{
	if (msg == NULL || device_id == NULL || group_index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_query_scalar_group_telemetry_v1_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_req *request =
	    (struct nsm_query_scalar_group_telemetry_v1_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_v1_req) -
		NSM_REQUEST_CONVENTION_LEN) {
		return NSM_SW_ERROR_DATA;
	}

	*device_id = request->device_id;
	*group_index = request->group_index;
	return NSM_SW_SUCCESS;
}

int encode_query_scalar_group_telemetry_v1_resp(uint8_t instance_id, uint8_t cc,
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
	header.nvidia_msg_type = NSM_TYPE_PCI_LINK;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, msg);
	}

	struct nsm_query_scalar_group_telemetry_v1_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_resp *)msg->payload;

	resp->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(data_size);
	int cnt = data_size / sizeof(uint32_t);
	uint32_t val;
	for (int idx = 0; idx < cnt; idx++) {
		memcpy(&val, data + idx * sizeof(uint32_t), sizeof(uint32_t));
		val = htole32(val);
		memcpy(resp->data + idx * sizeof(uint32_t), &val,
		       sizeof(uint32_t));
	}
	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_resp(const struct nsm_msg *msg,
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
		sizeof(struct nsm_query_scalar_group_telemetry_v1_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	int cnt = *data_size / sizeof(uint32_t);
	uint32_t val;
	for (int idx = 0; idx < cnt; idx++) {
		memcpy(&val, resp->data + idx * sizeof(uint32_t),
		       sizeof(uint32_t));
		val = le32toh(val);
		memcpy(data + idx * sizeof(uint32_t), &val, sizeof(uint32_t));
	}
	return NSM_SW_SUCCESS;
}

int encode_query_scalar_group_telemetry_v1_group0_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_0 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_0),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group0_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_0 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_0))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_query_scalar_group_telemetry_v1_group1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_1 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_1),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_1 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_1))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_query_scalar_group_telemetry_v1_group2_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_2 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_2),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group2_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_2 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_2))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_query_scalar_group_telemetry_v1_group3_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_3 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_3),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group3_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_3 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_3))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_query_scalar_group_telemetry_v1_group4_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_4 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_4),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group4_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_4 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);

	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_4))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_query_scalar_group_telemetry_v1_group5_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_5 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_5),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group5_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_5 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_5))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}

int encode_query_scalar_group_telemetry_v1_group6_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_6 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_6),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group6_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_6 *data)
{
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size <
	    sizeof(struct nsm_query_scalar_group_telemetry_group_6))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}
