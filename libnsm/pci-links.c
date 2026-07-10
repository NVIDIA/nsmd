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

	struct nsm_header_info header = {0};
	uint8_t rc = unpack_nsm_header(&msg->hdr, &header);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
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

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_PCI_LINK};
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

/**
 * @brief Validate scalar group telemetry response length before decoding.
 *
 * @param[in] msg - Response message
 * @param[in] msg_len - Length of response message
 * @param[in] max_group_struct_size - Maximum size of the group data struct
 * @return NSM_SW_SUCCESS if valid or msg too short to validate,
 *         NSM_SW_ERROR_LENGTH if validation fails
 */
static int validate_query_scalar_group_telemetry_v1_resp_length(
    const struct nsm_msg *msg, size_t msg_len, size_t max_group_struct_size)
{
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_query_scalar_group_telemetry_v1_resp)) {
		return NSM_SW_SUCCESS;
	}

	struct nsm_query_scalar_group_telemetry_v1_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_resp *)msg->payload;
	uint16_t resp_data_size = le16toh(resp->hdr.data_size);

	if (resp_data_size == 0 || resp_data_size > max_group_struct_size ||
	    resp_data_size % sizeof(uint32_t) != 0) {
		return NSM_SW_ERROR_LENGTH;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_common_resp) + resp_data_size) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group0_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_0 *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_0));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_0));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
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
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_1));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_1));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
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
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_2));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_2));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
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
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_3));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_3));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
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
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_4));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_4));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);

	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
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
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_5));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_5));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
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
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_6));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_6));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
}

int encode_query_scalar_group_telemetry_v1_group7_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_7 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_7),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group7_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_7 *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_7));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_7));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
}

int encode_query_scalar_group_telemetry_v1_group8_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_8 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_8),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group8_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_8 *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_8));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_8));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
}

int encode_query_scalar_group_telemetry_v1_group9_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_9 *data, struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_9),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group9_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_9 *data)
{
	if (msg == NULL || cc == NULL || data_size == NULL ||
	    reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_9));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_9));

	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
}

int encode_query_scalar_group_telemetry_v1_group10_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_query_scalar_group_telemetry_group_10 *data,
    struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_10),
	    (uint8_t *)data, msg);
}

int encode_query_scalar_group_telemetry_v1_group10_extended_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_query_scalar_group_telemetry_group_10_extended *data,
    struct nsm_msg *msg)
{
	return encode_query_scalar_group_telemetry_v1_resp(
	    instance_id, cc, reason_code,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_10_extended),
	    (uint8_t *)data, msg);
}

int decode_query_scalar_group_telemetry_v1_group10_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_10 *data)
{
	if (msg == NULL || cc == NULL || reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_10));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(data, 0,
	       sizeof(struct nsm_query_scalar_group_telemetry_group_10));

	uint16_t data_size = 0;
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group10_extended_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_10_extended *data)
{
	if (msg == NULL || cc == NULL || reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = validate_query_scalar_group_telemetry_v1_resp_length(
	    msg, msg_len,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_10_extended));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	memset(
	    data, 0,
	    sizeof(struct nsm_query_scalar_group_telemetry_group_10_extended));

	uint16_t data_size = 0;
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return ret;
	}

	return NSM_SW_SUCCESS;
}

int encode_assert_pcie_fundamental_reset_req(uint8_t instance_id,
					     uint8_t device_index,
					     uint8_t action,
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

	struct nsm_assert_pcie_fundamental_reset_req *request =
	    (struct nsm_assert_pcie_fundamental_reset_req *)msg->payload;

	request->hdr.command = NSM_ASSERT_PCIE_FUNDAMENTAL_RESET;
	request->hdr.data_size = 2 * sizeof(uint8_t);
	request->device_index = device_index;
	request->action = action;
	return NSM_SW_SUCCESS;
}

int decode_assert_pcie_fundamental_reset_req(const struct nsm_msg *msg,
					     size_t msg_len,
					     uint8_t *device_index,
					     uint8_t *action)
{
	if (msg == NULL || device_index == NULL || action == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_assert_pcie_fundamental_reset_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_assert_pcie_fundamental_reset_req *request =
	    (struct nsm_assert_pcie_fundamental_reset_req *)msg->payload;

	if (request->hdr.data_size != 2 * sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
	}

	*device_index = request->device_index;
	*action = request->action;
	return NSM_SW_SUCCESS;
}

int encode_assert_pcie_fundamental_reset_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PCI_LINK;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_ASSERT_PCIE_FUNDAMENTAL_RESET;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_assert_pcie_fundamental_reset_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *data_size,
					      uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

int encode_query_available_clearable_scalar_data_sources_v1_req(
    uint8_t instance_id, uint8_t device_index, uint8_t group_id,
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

	request->hdr.command =
	    NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES;
	request->hdr.data_size =
	    sizeof(struct
		   nsm_query_available_clearable_scalar_data_sources_v1_req) -
	    sizeof(struct nsm_common_req);
	request->device_id = device_index;
	request->group_index = group_id;

	return NSM_SW_SUCCESS;
}

int decode_query_available_clearable_scalar_data_sources_v1_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *device_index,
    uint8_t *group_id)
{
	if (msg == NULL || device_index == NULL || group_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(
		    struct
		    nsm_query_available_clearable_scalar_data_sources_v1_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_available_clearable_scalar_data_sources_v1_req
	    *request =
		(struct nsm_query_available_clearable_scalar_data_sources_v1_req
		     *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct
		   nsm_query_available_clearable_scalar_data_sources_v1_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*device_index = request->device_index;
	*group_id = request->group_id;
	return NSM_SW_SUCCESS;
}

int encode_query_available_clearable_scalar_data_sources_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const uint16_t data_size, uint8_t mask_length,
    uint8_t *available_data_source_mask, uint8_t *clearable_data_source_mask,
    struct nsm_msg *msg)
{
	if (msg == NULL || available_data_source_mask == NULL ||
	    clearable_data_source_mask == NULL) {
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
		    cc, reason_code,
		    NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES, msg);
	}

	struct nsm_query_available_clearable_scalar_data_sources_v1_resp *resp =
	    (struct nsm_query_available_clearable_scalar_data_sources_v1_resp *)
		msg->payload;

	resp->hdr.command = NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size = htole16(data_size);
	resp->mask_length = mask_length;
	uint8_t val;
	for (int idx = 0; idx < mask_length; idx++) {
		memcpy(&val, available_data_source_mask + idx * sizeof(uint8_t),
		       sizeof(uint8_t));
		memcpy(resp->data + idx * sizeof(uint8_t), &val,
		       sizeof(uint8_t));
	}

	for (int idx = 0; idx < mask_length; idx++) {
		memcpy(&val, clearable_data_source_mask + idx * sizeof(uint8_t),
		       sizeof(uint8_t));
		memcpy(resp->data + (mask_length) * sizeof(uint8_t) +
			   idx * sizeof(uint8_t),
		       &val, sizeof(uint8_t));
	}

	return NSM_SW_SUCCESS;
}

int decode_query_available_clearable_scalar_data_sources_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint8_t *mask_length,
    uint8_t *available_data_source_mask, uint8_t *clearable_data_source_mask)
{
	if (msg == NULL || cc == NULL || mask_length == NULL ||
	    available_data_source_mask == NULL ||
	    clearable_data_source_mask == NULL || reason_code == NULL ||
	    data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(
		    struct
		    nsm_query_available_clearable_scalar_data_sources_v1_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_available_clearable_scalar_data_sources_v1_resp *resp =
	    (struct nsm_query_available_clearable_scalar_data_sources_v1_resp *)
		msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	*mask_length = resp->mask_length;

	/* Bound the wire mask_length against the caller's fixed destination
	 * buffers before the copies below, and against the bytes actually
	 * received so the source reads stay within the message. */
	if (*mask_length > NSM_MAX_SCALAR_DATA_SOURCE_MASK_SIZE) {
		return NSM_SW_ERROR_DATA;
	}

	if (*data_size !=
	    sizeof(uint8_t) + 2 * (*mask_length) * sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
	}

	if (sizeof(struct nsm_msg_hdr) +
		offsetof(
		    struct
		    nsm_query_available_clearable_scalar_data_sources_v1_resp,
		    data) +
		2u * (size_t)(*mask_length) >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint8_t val;
	for (int idx = 0; idx < *mask_length; idx++) {
		memcpy(&val, resp->data + idx * sizeof(uint8_t),
		       sizeof(uint8_t));
		memcpy(available_data_source_mask + idx * sizeof(uint8_t), &val,
		       sizeof(uint8_t));
	}

	for (int idx = 0; idx < *mask_length; idx++) {
		memcpy(&val,
		       resp->data + (*mask_length) * sizeof(uint8_t) +
			   idx * sizeof(uint8_t),
		       sizeof(uint8_t));
		memcpy(clearable_data_source_mask + idx * sizeof(uint8_t), &val,
		       sizeof(uint8_t));
	}

	return NSM_SW_SUCCESS;
}

int encode_clear_data_source_v1_req(uint8_t instance_id, uint8_t device_index,
				    uint8_t groupId, uint8_t dsId,
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

	struct nsm_clear_data_source_v1_req *request =
	    (struct nsm_clear_data_source_v1_req *)msg->payload;

	request->hdr.command = NSM_CLEAR_DATA_SOURCE_V1;
	request->hdr.data_size = sizeof(struct nsm_clear_data_source_v1_req) -
				 sizeof(struct nsm_common_req);
	request->device_index = device_index;
	request->groupId = groupId;
	request->dsId = dsId;
	return NSM_SW_SUCCESS;
}

int decode_clear_data_source_v1_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *device_index, uint8_t *groupId,
				    uint8_t *dsId)
{
	if (msg == NULL || device_index == NULL || groupId == NULL ||
	    dsId == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_clear_data_source_v1_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_clear_data_source_v1_req *request =
	    (struct nsm_clear_data_source_v1_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_clear_data_source_v1_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*device_index = request->device_index;
	*groupId = request->groupId;
	*dsId = request->dsId;
	return NSM_SW_SUCCESS;
}

int encode_clear_data_source_v1_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_PCI_LINK;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_CLEAR_DATA_SOURCE_V1, msg);
	}

	struct nsm_common_resp *resp = (struct nsm_common_resp *)msg->payload;
	resp->command = NSM_CLEAR_DATA_SOURCE_V1;
	resp->completion_code = cc;
	resp->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_clear_data_source_v1_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *data_size,
				     uint16_t *reason_code)
{
	return decode_common_resp(msg, msg_len, cc, data_size, reason_code);
}

int encode_list_available_pcie_ports_req(uint8_t instance_id,
					 struct nsm_msg *msg)
{
	return encode_common_req(instance_id, NSM_TYPE_PCI_LINK,
				 NSM_LIST_AVAILABLE_PCIE_PORTS, msg);
}

int decode_list_available_pcie_ports_req(const struct nsm_msg *msg,
					 size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_list_available_pcie_ports_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_list_available_pcie_ports_info *info, struct nsm_msg *msg)
{
	if (msg == NULL || info == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_PCI_LINK};

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}
	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_LIST_AVAILABLE_PCIE_PORTS, msg);
	}

	struct nsm_list_available_pcie_ports_resp *resp =
	    (struct nsm_list_available_pcie_ports_resp *)msg->payload;

	resp->hdr.command = NSM_LIST_AVAILABLE_PCIE_PORTS;
	resp->hdr.completion_code = cc;
	resp->hdr.data_size =
	    sizeof(uint16_t) +
	    info->ports_count * sizeof(struct nsm_pcie_upstream_port_info);
	resp->hdr.data_size = htole16(resp->hdr.data_size);
	resp->port_info.ports_count = htole16(info->ports_count);
	memcpy(resp->port_info.ports, info->ports,
	       info->ports_count * sizeof(struct nsm_pcie_upstream_port_info));
	return rc;
}

int decode_list_available_pcie_ports_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_list_available_pcie_ports_info *info)
{
	if (msg == NULL || cc == NULL || reason_code == NULL || info == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_list_available_pcie_ports_resp *resp =
	    (struct nsm_list_available_pcie_ports_resp *)msg->payload;

	info->ports_count = le16toh(resp->port_info.ports_count);

	uint16_t data_size = le16toh(resp->hdr.data_size);
	uint16_t expected_data_size =
	    (sizeof(uint16_t) +
	     info->ports_count * sizeof(struct nsm_pcie_upstream_port_info));
	if (msg_len < (sizeof(struct nsm_msg_hdr) +
		       sizeof(struct nsm_common_resp) + expected_data_size) ||
	    data_size != expected_data_size) {
		return NSM_SW_ERROR_LENGTH;
	}

	memcpy(info->ports, resp->port_info.ports,
	       info->ports_count * sizeof(struct nsm_pcie_upstream_port_info));

	for (int i = 0; i < info->ports_count; i++) {
		if (info->ports[i].type > NSM_PORT_TYPE_DOWNSTREAM) {
			return NSM_SW_ERROR_DATA;
		}
	}

	return NSM_SW_SUCCESS;
}

int encode_multiport_query_scalar_group_telemetry_v2_req(
    uint8_t instance_id,
    const struct nsm_multiport_query_scalar_group_telemetry_v2_req_data *data,
    struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_REQUEST, instance_id,
					 NSM_TYPE_PCI_LINK};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_multiport_query_scalar_group_telemetry_v2_req *request =
	    (struct nsm_multiport_query_scalar_group_telemetry_v2_req *)
		msg->payload;

	request->hdr.command = NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2;
	request->hdr.data_size = sizeof(
	    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data);
	request->data = *data;

	return NSM_SW_SUCCESS;
}

int decode_multiport_query_scalar_group_telemetry_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data *data)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	uint8_t rc = unpack_nsm_header(&msg->hdr, &header);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(
		    struct nsm_multiport_query_scalar_group_telemetry_v2_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_multiport_query_scalar_group_telemetry_v2_req *request =
	    (struct nsm_multiport_query_scalar_group_telemetry_v2_req *)
		msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct
		   nsm_multiport_query_scalar_group_telemetry_v2_req_data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*data = request->data;
	return NSM_SW_SUCCESS;
}

int encode_get_pcie_port_config_req(uint8_t instance_id, uint8_t port_number,
				    uint8_t port_type, uint8_t port_index,
				    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_REQUEST, instance_id,
					 NSM_TYPE_PCI_LINK};

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (port_type > NSM_PORT_TYPE_DOWNSTREAM) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_get_port_config_req *request =
	    (struct nsm_get_port_config_req *)msg->payload;

	request->hdr.command = NSM_GET_PORT_CONFIGURATION;
	request->hdr.data_size = sizeof(struct nsm_get_port_config_req) -
				 sizeof(struct nsm_common_req);
	request->port_number = port_number & 0x7f;
	request->port_type = port_type & 0x1;
	request->port_index = port_index;

	return NSM_SW_SUCCESS;
}

int decode_get_pcie_port_config_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *port_number, uint8_t *port_type,
				    uint8_t *port_index)
{
	if (msg == NULL || port_number == NULL || port_type == NULL ||
	    port_index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_get_port_config_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_port_config_req *request =
	    (struct nsm_get_port_config_req *)msg->payload;

	*port_number = request->port_number;
	*port_type = request->port_type;
	*port_index = request->port_index;

	return NSM_SW_SUCCESS;
}

int encode_preset_PCIe_data(uint8_t preset, uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*data = preset;
	*data_len = sizeof(uint8_t);

	return NSM_SW_SUCCESS;
}

int decode_preset_PCIe_data(const uint8_t *data, size_t data_len,
			    uint8_t *preset_0, uint8_t *preset_1)
{
	if (data == NULL || preset_0 == NULL || preset_1 == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*preset_0 = *data & 0x0f;
	*preset_1 = *data >> 4;

	return NSM_SW_SUCCESS;
}

int decode_PCIe_TxAmplitude_data(const uint8_t *data, size_t data_len,
				 uint8_t *tx_amplitude)
{
	if (data == NULL || tx_amplitude == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*tx_amplitude = *data & 0x07;

	return NSM_SW_SUCCESS;
}

int encode_PCIe_TxAmplitude_data(uint8_t tx_amplitude, uint8_t *data,
				 size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*data = tx_amplitude & 0x07;
	*data_len = sizeof(uint8_t);

	return NSM_SW_SUCCESS;
}

int encode_get_port_config_aggregate_resp(uint8_t instance_id, uint8_t command,
					  uint8_t cc, uint16_t telemetry_count,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
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

	struct nsm_get_port_config_aggregate_resp *response =
	    (struct nsm_get_port_config_aggregate_resp *)msg->payload;

	response->command = command;
	response->telemetry_count = htole16(telemetry_count);
	response->completion_code = cc;

	return NSM_SW_SUCCESS;
}

int encode_set_port_config_aggregate_req(uint8_t instance_id, uint8_t port_id,
					 uint8_t port_type, uint8_t port_index,
					 uint16_t sample_count,
					 const uint8_t *sample_data,
					 size_t sample_data_len,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (sample_data == NULL && sample_count > 0) {
		return NSM_SW_ERROR_NULL;
	}

	if (port_type > NSM_PORT_TYPE_DOWNSTREAM) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_header_info header = {NSM_REQUEST, instance_id,
					 NSM_TYPE_PCI_LINK};

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_port_config_aggregate_req *request =
	    (struct nsm_set_port_config_aggregate_req *)msg->payload;

	request->hdr.command = NSM_SET_PORT_CONFIGURATION;
	request->hdr.data_size = sizeof(request->port_index) +
				 sizeof(request->sample_count) +
				 sample_data_len + 1;
	request->port_number = port_id & 0x7f;
	request->port_type = port_type & 0x1;
	request->port_index = port_index;
	request->sample_count = sample_count;
	if (sample_data != NULL && sample_data_len > 0) {
		memcpy(request->sample_data, sample_data, sample_data_len);
	}

	return NSM_SW_SUCCESS;
}

int decode_set_port_config_aggregate_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *port_number,
    uint8_t *port_type, uint8_t *port_index, uint16_t *sample_count,
    const uint8_t **sample_data, size_t *sample_data_len)
{
	if (msg == NULL || port_number == NULL || port_type == NULL ||
	    port_index == NULL || sample_count == NULL || sample_data == NULL ||
	    sample_data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_port_config_aggregate_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_port_config_aggregate_req *request =
	    (struct nsm_set_port_config_aggregate_req *)msg->payload;

	if (request->hdr.data_size <
	    sizeof(request->port_index) + sizeof(request->sample_count) + 1) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = request->port_number;
	*port_type = request->port_type;
	*port_index = request->port_index;
	*sample_count = request->sample_count;
	*sample_data = request->sample_data;
	*sample_data_len = request->hdr.data_size -
			   sizeof(request->port_index) -
			   sizeof(request->sample_count) - 1;
	return NSM_SW_SUCCESS;
}

int encode_set_port_config_aggregate_resp(uint8_t instance_id, uint8_t cc,
					  uint16_t reason_code,
					  struct nsm_msg *msg)
{
	return encode_cc_only_resp(instance_id, NSM_TYPE_PCI_LINK,
				   NSM_SET_PORT_CONFIGURATION, cc, reason_code,
				   msg);
}

int decode_set_port_config_aggregate_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code)
{
	return decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
}

int encode_query_vector_group_telemetry_v2_req(
    uint8_t instance_id, uint8_t port_number, uint8_t port_type, uint8_t index,
    uint8_t group_id, uint8_t selector_0, uint8_t selector_1,
    struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_REQUEST, instance_id,
					 NSM_TYPE_PCI_LINK};

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_query_vector_group_telemetry_v2_req *request =
	    (struct nsm_query_vector_group_telemetry_v2_req *)msg->payload;

	request->hdr.command = NSM_QUERY_VECTOR_DATA_SOURCES_V2;
	request->hdr.data_size =
	    sizeof(struct nsm_query_vector_group_telemetry_v2_req) -
	    sizeof(struct nsm_common_req);
	request->port_number = port_number;
	request->port_type = port_type;
	request->index = index;
	request->group_id = group_id;
	request->selector_0 = selector_0;
	request->selector_1 = selector_1;

	return NSM_SW_SUCCESS;
}

int decode_query_vector_group_telemetry_v2_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *port_number,
    uint8_t *port_type, uint8_t *index, uint8_t *group_id, uint8_t *selector_0,
    uint8_t *selector_1)
{
	if (msg == NULL || port_number == NULL || port_type == NULL ||
	    index == NULL || group_id == NULL || selector_0 == NULL ||
	    selector_1 == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len !=
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_query_vector_group_telemetry_v2_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_vector_group_telemetry_v2_req *request =
	    (struct nsm_query_vector_group_telemetry_v2_req *)msg->payload;

	if (request->hdr.data_size !=
	    sizeof(struct nsm_query_vector_group_telemetry_v2_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = request->port_number;
	*port_type = request->port_type;
	*index = request->index;
	*group_id = request->group_id;
	*selector_0 = request->selector_0;
	*selector_1 = request->selector_1;

	return NSM_SW_SUCCESS;
}

int encode_query_vector_group_telemetry_v2_resp(uint8_t instance_id, uint8_t cc,
						uint16_t reason_code,
						uint16_t data_count,
						const uint32_t *data_values,
						struct nsm_msg *msg)
{
	if (msg == NULL || (data_count > 0 && data_values == NULL)) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {NSM_RESPONSE, instance_id,
					 NSM_TYPE_PCI_LINK};
	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(
		    cc, reason_code, NSM_QUERY_VECTOR_DATA_SOURCES_V2, msg);
	}

	struct nsm_query_vector_data_sources_v2_resp *resp =
	    (struct nsm_query_vector_data_sources_v2_resp *)msg->payload;

	resp->hdr.command = NSM_QUERY_VECTOR_DATA_SOURCES_V2;
	resp->hdr.completion_code = cc;
	uint16_t data_size = data_count * sizeof(uint32_t);
	resp->hdr.data_size = htole16(data_size);

	// Copy and convert data values to little endian
	for (int i = 0; i < data_count; i++) {
		uint32_t val = htole32(data_values[i]);
		memcpy(&resp->data_values[i], &val, sizeof(uint32_t));
	}

	return NSM_SW_SUCCESS;
}

int decode_query_vector_group_telemetry_v2_resp(const struct nsm_msg *msg,
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
		sizeof(struct nsm_query_vector_data_sources_v2_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_vector_data_sources_v2_resp *resp =
	    (struct nsm_query_vector_data_sources_v2_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	/* Bound the wire data_size against the bytes actually received so the
	 * source read below cannot run past the end of the message. The
	 * destination capacity is enforced by the typed group wrappers, which
	 * reject an unexpected data_size before this copy. */
	if (sizeof(struct nsm_msg_hdr) +
		offsetof(struct nsm_query_vector_data_sources_v2_resp,
			 data_values) +
		(size_t)*data_size >
	    msg_len) {
		return NSM_SW_ERROR_LENGTH;
	}
	int cnt = *data_size / sizeof(uint32_t);
	uint32_t val;
	for (int idx = 0; idx < cnt; idx++) {
		memcpy(&val, resp->data_values + idx, sizeof(uint32_t));
		val = le32toh(val);
		memcpy(data + idx * sizeof(uint32_t), &val, sizeof(uint32_t));
	}

	return NSM_SW_SUCCESS;
}

int encode_query_vector_group_telemetry_v2_group1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_query_vector_group_1_data *data, struct nsm_msg *msg)
{
	if (data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint32_t value = data->cdr_error_per_lane;
	return encode_query_vector_group_telemetry_v2_resp(
	    instance_id, cc, reason_code, 1, &value, msg);
}

int decode_query_vector_group_telemetry_v2_group1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_query_vector_group_1_data *data)
{
	if (msg == NULL || cc == NULL || reason_code == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	/* Reject a wire data_size that would overflow the fixed group-1 buffer
	 * BEFORE the generic decoder copies it; the post-call check below runs
	 * too late to prevent the overflow. */
	int prc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (prc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return prc;
	}
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_query_vector_data_sources_v2_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}
	const struct nsm_query_vector_data_sources_v2_resp *presp =
	    (const struct nsm_query_vector_data_sources_v2_resp *)msg->payload;
	if (le16toh(presp->hdr.data_size) !=
	    sizeof(struct nsm_query_vector_group_1_data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint16_t data_size = 0;
	int ret = decode_query_vector_group_telemetry_v2_resp(
	    msg, msg_len, cc, &data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (data_size != sizeof(struct nsm_query_vector_group_1_data))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
}
