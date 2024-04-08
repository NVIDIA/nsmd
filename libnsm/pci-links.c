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

int decode_query_scalar_group_telemetry_v1_req(const struct nsm_msg *msg, size_t msg_len, uint8_t* device_id, uint8_t* group_index)
{
	if (msg == NULL || device_id == NULL || group_index == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
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


static void htoleScalarTelemetryGroup1(struct nsm_query_scalar_group_telemetry_group_1 *data)
{
	data->negotiated_link_speed = htole32(data->negotiated_link_speed);
	data->negotiated_link_width = htole32(data->negotiated_link_width);
	data->target_link_speed = htole32(data->target_link_speed);
	data->max_link_speed = htole32(data->max_link_speed);
	data->max_link_width = htole32(data->max_link_width);
}

static void
letohScalarTelemetryGroup1(struct nsm_query_scalar_group_telemetry_group_1 *data)
{
	data->negotiated_link_speed = le32toh(data->negotiated_link_speed);
	data->negotiated_link_width = le32toh(data->negotiated_link_width);
	data->target_link_speed = le32toh(data->target_link_speed);
	data->max_link_speed = le32toh(data->max_link_speed);
	data->max_link_width = le32toh(data->max_link_width);
}

int encode_query_scalar_group_telemetry_v1_group1_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_query_scalar_group_telemetry_group_1* data,
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
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, msg);
	}

	struct nsm_query_scalar_group_telemetry_v1_group_1_resp* resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_1_resp*)msg->payload;

	resp->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_query_scalar_group_telemetry_group_1);
	resp->hdr.data_size = htole16(data_size);
    htoleScalarTelemetryGroup1(data);
    resp->data = *data;
	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group1_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, struct nsm_query_scalar_group_telemetry_group_1* data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
    
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_scalar_group_telemetry_v1_group_1_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_group_1_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_1_resp*)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);

    if ((*data_size) < sizeof(struct nsm_query_scalar_group_telemetry_group_1)) {
		return NSM_SW_ERROR_DATA;
	}

    letohScalarTelemetryGroup1(&(resp->data));
	*data = resp->data;
	return NSM_SW_SUCCESS;
}

static void htoleScalarTelemetryGroup2(
    struct nsm_query_scalar_group_telemetry_group_2 *data)
{
	data->non_fatal_errors = htole32(data->non_fatal_errors);
	data->fatal_errors = htole32(data->fatal_errors);
	data->unsupported_request_count =
	    htole32(data->unsupported_request_count);
	data->correctable_errors = htole32(data->correctable_errors);
}

static void letohScalarTelemetryGroup2(
    struct nsm_query_scalar_group_telemetry_group_2 *data)
{
	data->non_fatal_errors = le32toh(data->non_fatal_errors);
	data->fatal_errors = le32toh(data->fatal_errors);
	data->unsupported_request_count =
	    le32toh(data->unsupported_request_count);
	data->correctable_errors = le32toh(data->correctable_errors);
}

int encode_query_scalar_group_telemetry_v1_group2_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_query_scalar_group_telemetry_group_2* data,
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
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, msg);
	}

	struct nsm_query_scalar_group_telemetry_v1_group_2_resp* resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_2_resp*)msg->payload;

	resp->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_query_scalar_group_telemetry_group_2);
	resp->hdr.data_size = htole16(data_size);
    htoleScalarTelemetryGroup2(data);
    resp->data = *data;
	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group2_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, struct nsm_query_scalar_group_telemetry_group_2* data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
    
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_scalar_group_telemetry_v1_group_2_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_group_2_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_2_resp*)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
    
	if ((*data_size) < sizeof(struct nsm_query_scalar_group_telemetry_group_2)) {
		return NSM_SW_ERROR_DATA;
	}
    letohScalarTelemetryGroup2(&(resp->data));
	*data = resp->data;
	return NSM_SW_SUCCESS;
}

static void htoleScalarTelemetryGroup3(
    struct nsm_query_scalar_group_telemetry_group_3 *data)
{
	data->L0ToRecoveryCount = htole32(data->L0ToRecoveryCount);
}
static void
letohScalarTelemetryGroup3(struct nsm_query_scalar_group_telemetry_group_3 *data)
{
	data->L0ToRecoveryCount = le32toh(data->L0ToRecoveryCount);
}

int encode_query_scalar_group_telemetry_v1_group3_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_query_scalar_group_telemetry_group_3* data,
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
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, msg);
	}

	struct nsm_query_scalar_group_telemetry_v1_group_3_resp* resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_3_resp*)msg->payload;

	resp->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_query_scalar_group_telemetry_group_3);
	resp->hdr.data_size = htole16(data_size);
    htoleScalarTelemetryGroup3(data);
    resp->data = *data;
	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group3_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, struct nsm_query_scalar_group_telemetry_group_3* data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
    
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_scalar_group_telemetry_v1_group_3_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_group_3_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_3_resp*)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
    
	if ((*data_size) < sizeof(struct nsm_query_scalar_group_telemetry_group_3)) {
		return NSM_SW_ERROR_DATA;
	}
    letohScalarTelemetryGroup3(&(resp->data));
	*data = resp->data;
	return NSM_SW_SUCCESS;
}

static void htoleScalarTelemetryGroup4(struct nsm_query_scalar_group_telemetry_group_4 *data)
{
	data->bad_TLP_cnt = htole32(data->bad_TLP_cnt);
	data->FC_timeout_err_cnt = htole32(data->FC_timeout_err_cnt);
	data->NAK_recv_cnt = htole32(data->NAK_recv_cnt);
	data->NAK_sent_cnt = htole32(data->NAK_sent_cnt);
	data->recv_err_cnt = htole32(data->recv_err_cnt);
	data->replay_cnt = htole32(data->replay_cnt);
	data->replay_rollover_cnt = htole32(data->replay_rollover_cnt);
}

static void
letohScalarTelemetryGroup4(struct nsm_query_scalar_group_telemetry_group_4 *data)
{
	data->bad_TLP_cnt = le32toh(data->bad_TLP_cnt);
	data->FC_timeout_err_cnt = le32toh(data->FC_timeout_err_cnt);
	data->NAK_recv_cnt = le32toh(data->NAK_recv_cnt);
	data->NAK_sent_cnt = le32toh(data->NAK_sent_cnt);
	data->recv_err_cnt = le32toh(data->recv_err_cnt);
	data->replay_cnt = le32toh(data->replay_cnt);
	data->replay_rollover_cnt = le32toh(data->replay_rollover_cnt);
}

int encode_query_scalar_group_telemetry_v1_group4_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_query_scalar_group_telemetry_group_4* data,
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
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, msg);
	}

	struct nsm_query_scalar_group_telemetry_v1_group_4_resp* resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_4_resp*)msg->payload;

	resp->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_query_scalar_group_telemetry_group_4);
	resp->hdr.data_size = htole16(data_size);
    htoleScalarTelemetryGroup4(data);
    resp->data = *data;
	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group4_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, struct nsm_query_scalar_group_telemetry_group_4* data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
   
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_scalar_group_telemetry_v1_group_4_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_group_4_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_4_resp*)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
    
	if ((*data_size) < sizeof(struct nsm_query_scalar_group_telemetry_group_4)) {
		return NSM_SW_ERROR_DATA;
	}

    letohScalarTelemetryGroup4(&(resp->data));
	*data = resp->data;
	return NSM_SW_SUCCESS;
}

static void htoleScalarTelemetryGroup5(struct nsm_query_scalar_group_telemetry_group_5 *data)
{
	data->PCIeTXBytes = htole32(data->PCIeTXBytes);
	data->PCIeRXBytes = htole32(data->PCIeRXBytes);
}

static void
letohScalarTelemetryGroup5(struct nsm_query_scalar_group_telemetry_group_5 *data)
{
	data->PCIeTXBytes = le32toh(data->PCIeTXBytes);
	data->PCIeRXBytes = le32toh(data->PCIeRXBytes);
}

int encode_query_scalar_group_telemetry_v1_group5_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_query_scalar_group_telemetry_group_5* data,
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
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, msg);
	}

	struct nsm_query_scalar_group_telemetry_v1_group_5_resp* resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_5_resp*)msg->payload;

	resp->hdr.command = NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1;
	resp->hdr.completion_code = cc;
	uint16_t data_size = sizeof(struct nsm_query_scalar_group_telemetry_group_5);
	resp->hdr.data_size = htole16(data_size);
    htoleScalarTelemetryGroup5(data);
    resp->data = *data;
	return NSM_SW_SUCCESS;
}

int decode_query_scalar_group_telemetry_v1_group5_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *data_size,
				    uint16_t *reason_code, struct nsm_query_scalar_group_telemetry_group_5* data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}
 
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_query_scalar_group_telemetry_v1_group_5_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_scalar_group_telemetry_v1_group_5_resp *resp =
	    (struct nsm_query_scalar_group_telemetry_v1_group_5_resp*)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
    
	if ((*data_size) < sizeof(struct nsm_query_scalar_group_telemetry_group_5)) {
		return NSM_SW_ERROR_DATA;
	}

    letohScalarTelemetryGroup5(&(resp->data));
	*data = resp->data;
	return NSM_SW_SUCCESS;
}