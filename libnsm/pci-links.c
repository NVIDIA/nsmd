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
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size !=
	    sizeof(struct nsm_query_scalar_group_telemetry_group_8))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
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
	int ret = decode_query_scalar_group_telemetry_v1_resp(
	    msg, msg_len, cc, data_size, reason_code, (uint8_t *)data);
	if (ret != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)
		return ret;
	if (*data_size !=
	    sizeof(struct nsm_query_scalar_group_telemetry_group_9))
		ret = NSM_SW_ERROR_LENGTH;
	return ret;
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

	if (*data_size !=
	    sizeof(uint8_t) + 2 * (*mask_length) * sizeof(uint8_t)) {
		return NSM_SW_ERROR_DATA;
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
