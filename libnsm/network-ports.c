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

#include "network-ports.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

static void htolePortCounterData(struct nsm_port_counter_data *portData)
{
	uint32_t htole32_supported_counter_mask;
	memcpy(&htole32_supported_counter_mask, &(portData->supported_counter),
	       sizeof(struct nsm_supported_port_counter));
	htole32_supported_counter_mask =
	    htole32(htole32_supported_counter_mask);
	memcpy(&(portData->supported_counter), &htole32_supported_counter_mask,
	       sizeof(struct nsm_supported_port_counter));

	portData->port_rcv_pkts = htole64(portData->port_rcv_pkts);
	portData->port_rcv_data = htole64(portData->port_rcv_data);
	portData->port_multicast_rcv_pkts =
	    htole64(portData->port_multicast_rcv_pkts);
	portData->port_unicast_rcv_pkts =
	    htole64(portData->port_unicast_rcv_pkts);
	portData->port_malformed_pkts = htole64(portData->port_malformed_pkts);
	portData->vl15_dropped = htole64(portData->vl15_dropped);
	portData->port_rcv_errors = htole64(portData->port_rcv_errors);
	portData->port_xmit_pkts = htole64(portData->port_xmit_pkts);
	portData->port_xmit_pkts_vl15 = htole64(portData->port_xmit_pkts_vl15);
	portData->port_xmit_data = htole64(portData->port_xmit_data);
	portData->port_xmit_data_vl15 = htole64(portData->port_xmit_data_vl15);
	portData->port_unicast_xmit_pkts =
	    htole64(portData->port_unicast_xmit_pkts);
	portData->port_multicast_xmit_pkts =
	    htole64(portData->port_multicast_xmit_pkts);
	portData->port_bcast_xmit_pkts =
	    htole64(portData->port_bcast_xmit_pkts);
	portData->port_xmit_discard = htole64(portData->port_xmit_discard);
	portData->port_neighbor_mtu_discards =
	    htole64(portData->port_neighbor_mtu_discards);
	portData->port_rcv_ibg2_pkts = htole64(portData->port_rcv_ibg2_pkts);
	portData->port_xmit_ibg2_pkts = htole64(portData->port_xmit_ibg2_pkts);
	portData->symbol_ber = htole64(portData->symbol_ber);
	portData->link_error_recovery_counter =
	    htole64(portData->link_error_recovery_counter);
	portData->link_downed_counter = htole64(portData->link_downed_counter);
	portData->port_rcv_remote_physical_errors =
	    htole64(portData->port_rcv_remote_physical_errors);
	portData->port_rcv_switch_relay_errors =
	    htole64(portData->port_rcv_switch_relay_errors);
	portData->QP1_dropped = htole64(portData->QP1_dropped);
	portData->xmit_wait = htole64(portData->xmit_wait);
	portData->effective_ber = htole64(portData->effective_ber);
	portData->total_raw_error = htole64(portData->total_raw_error);
	portData->effective_error = htole64(portData->effective_error);
	portData->symbol_error = htole64(portData->symbol_error);
	portData->total_raw_ber = htole64(portData->total_raw_ber);
	portData->unintentional_link_down_count =
	    htole64(portData->unintentional_link_down_count);
	portData->intentional_link_down_count =
	    htole64(portData->intentional_link_down_count);
}

static void letohPortCounterData(struct nsm_port_counter_data *portData)
{
	uint32_t le32toh_supported_counter_mask;
	memcpy(&le32toh_supported_counter_mask, &(portData->supported_counter),
	       sizeof(struct nsm_supported_port_counter));
	le32toh_supported_counter_mask =
	    le32toh(le32toh_supported_counter_mask);
	memcpy(&(portData->supported_counter), &le32toh_supported_counter_mask,
	       sizeof(struct nsm_supported_port_counter));

	portData->port_rcv_pkts = le64toh(portData->port_rcv_pkts);
	portData->port_rcv_data = le64toh(portData->port_rcv_data);
	portData->port_multicast_rcv_pkts =
	    le64toh(portData->port_multicast_rcv_pkts);
	portData->port_unicast_rcv_pkts =
	    le64toh(portData->port_unicast_rcv_pkts);
	portData->port_malformed_pkts = le64toh(portData->port_malformed_pkts);
	portData->vl15_dropped = le64toh(portData->vl15_dropped);
	portData->port_rcv_errors = le64toh(portData->port_rcv_errors);
	portData->port_xmit_pkts = le64toh(portData->port_xmit_pkts);
	portData->port_xmit_pkts_vl15 = le64toh(portData->port_xmit_pkts_vl15);
	portData->port_xmit_data = le64toh(portData->port_xmit_data);
	portData->port_xmit_data_vl15 = le64toh(portData->port_xmit_data_vl15);
	portData->port_unicast_xmit_pkts =
	    le64toh(portData->port_unicast_xmit_pkts);
	portData->port_multicast_xmit_pkts =
	    le64toh(portData->port_multicast_xmit_pkts);
	portData->port_bcast_xmit_pkts =
	    le64toh(portData->port_bcast_xmit_pkts);
	portData->port_xmit_discard = le64toh(portData->port_xmit_discard);
	portData->port_neighbor_mtu_discards =
	    le64toh(portData->port_neighbor_mtu_discards);
	portData->port_rcv_ibg2_pkts = le64toh(portData->port_rcv_ibg2_pkts);
	portData->port_xmit_ibg2_pkts = le64toh(portData->port_xmit_ibg2_pkts);
	portData->symbol_ber = le64toh(portData->symbol_ber);
	portData->link_error_recovery_counter =
	    le64toh(portData->link_error_recovery_counter);
	portData->link_downed_counter = le64toh(portData->link_downed_counter);
	portData->port_rcv_remote_physical_errors =
	    le64toh(portData->port_rcv_remote_physical_errors);
	portData->port_rcv_switch_relay_errors =
	    le64toh(portData->port_rcv_switch_relay_errors);
	portData->QP1_dropped = le64toh(portData->QP1_dropped);
	portData->xmit_wait = le64toh(portData->xmit_wait);
	portData->effective_ber = le64toh(portData->effective_ber);
	portData->total_raw_error = le64toh(portData->total_raw_error);
	portData->effective_error = le64toh(portData->effective_error);
	portData->symbol_error = le64toh(portData->symbol_error);
	portData->total_raw_ber = le64toh(portData->total_raw_ber);
	portData->unintentional_link_down_count =
	    le64toh(portData->unintentional_link_down_count);
	portData->intentional_link_down_count =
	    le64toh(portData->intentional_link_down_count);
}

static void
htolePortCharacteristicsData(struct nsm_port_characteristics_data *data)
{
	data->status_lane_info = htole32(data->status_lane_info);
	data->nv_port_line_rate_mbps = htole32(data->nv_port_line_rate_mbps);
	data->nv_port_data_rate_kbps = htole32(data->nv_port_data_rate_kbps);
	uint32_t htole32_port_status;
	memcpy(&htole32_port_status, &(data->port_status),
	       sizeof(struct status));
	htole32_port_status = htole32(htole32_port_status);
	memcpy(&(data->port_status), &htole32_port_status,
	       sizeof(struct status));
}

static void
letohPortCharacteristicsData(struct nsm_port_characteristics_data *data)
{
	data->status_lane_info = le32toh(data->status_lane_info);
	data->nv_port_line_rate_mbps = le32toh(data->nv_port_line_rate_mbps);
	data->nv_port_data_rate_kbps = le32toh(data->nv_port_data_rate_kbps);
	uint32_t le32toh_port_status;
	memcpy(&le32toh_port_status, &(data->port_status),
	       sizeof(struct status));
	le32toh_port_status = le32toh(le32toh_port_status);
	memcpy(&(data->port_status), &le32toh_port_status,
	       sizeof(struct status));
}

#ifdef ENABLE_SYSTEM_GUID
int encode_set_system_guid_req(uint8_t instance_id, struct nsm_msg *msg,
			       uint8_t *sys_guid, size_t sys_guid_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_system_guid_req *request =
	    (struct nsm_set_system_guid_req *)msg->payload;

	request->hdr.command = NSM_SET_SYSTEM_GUID;
	request->hdr.data_size = 0x08;

	if (sys_guid_len != 8) {
		return NSM_SW_ERROR_DATA;
	}
	request->SysGUID_0 = sys_guid[0];
	request->SysGUID_1 = sys_guid[1];
	request->SysGUID_2 = sys_guid[2];
	request->SysGUID_3 = sys_guid[3];
	request->SysGUID_4 = sys_guid[4];
	request->SysGUID_5 = sys_guid[5];
	request->SysGUID_6 = sys_guid[6];
	request->SysGUID_7 = sys_guid[7];

	return NSM_SW_SUCCESS;
}

int decode_set_system_guid_resp(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_system_guid_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_system_guid_resp *response =
	    (struct nsm_set_system_guid_resp *)msg->payload;

	if (response->hdr.data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_system_guid_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_system_guid_req *request =
	    (struct nsm_get_system_guid_req *)msg->payload;

	request->hdr.command = NSM_GET_SYSTEM_GUID;
	request->hdr.data_size = 0x00;

	return NSM_SW_SUCCESS;
}

int decode_get_system_guid_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *reason_code,
				uint8_t *sys_guid, size_t sys_guid_len)
{
	if (msg == NULL || sys_guid == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_system_guid_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_system_guid_resp *response =
	    (struct nsm_get_system_guid_resp *)msg->payload;

	if (response->hdr.data_size < 8 || sys_guid_len != 8) {
		return NSM_SW_ERROR_DATA;
	}

	sys_guid[0] = response->SysGUID_0;
	sys_guid[1] = response->SysGUID_1;
	sys_guid[2] = response->SysGUID_2;
	sys_guid[3] = response->SysGUID_3;
	sys_guid[4] = response->SysGUID_4;
	sys_guid[5] = response->SysGUID_5;
	sys_guid[6] = response->SysGUID_6;
	sys_guid[7] = response->SysGUID_7;

	return NSM_SW_SUCCESS;
}
#endif

int encode_get_nvlink_agg_led_status_req(uint8_t instance_id,
					 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_nvlink_agg_led_status_req *request =
	    (struct nsm_get_nvlink_agg_led_status_req *)msg->payload;

	request->hdr.command = NSM_GET_NVLINK_LED_STATUS;
	request->hdr.data_size = 0x00;

	return NSM_SW_SUCCESS;
}

int decode_get_nvlink_agg_led_status_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code,
					  enum nsm_nvlink_led_state *led_state)
{
	// NOTE: Decode reason code function checks pointers passed in
	// (msg, msg_len, cc, and reason_code)
	// so they are not checked here before use
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (led_state == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_nvlink_agg_led_status_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_nvlink_agg_led_status_resp *response =
	    (struct nsm_get_nvlink_agg_led_status_resp *)msg->payload;

	if (response->hdr.data_size < 9) {
		return NSM_SW_ERROR_DATA;
	}

	uint8_t led_status_byte = response->LED_State_Aggregate;

	// The top bit of the led state byte indicates an invalid state
	// so we will check it before reading the value
	if ((led_status_byte >> 7) == 1) {
		// First 3 bits provide LED state, so we will mask them off
		switch (led_status_byte & 0x07) {
		case 0:
			*led_state = NSM_NVLINK_LED_GREEN;
			break;
		case 1: // Blinking at 0.5 Hz
		case 2: // Blinking at 1 Hz
		case 3: // Blinking at 2 Hz
		case 4: // Blinking at 4 Hz
			*led_state = NSM_NVLINK_LED_GREEN_BLINK;
			break;
		case 5:
			*led_state = NSM_NVLINK_LED_AMBER;
			break;
		case 6:
			*led_state = NSM_NVLINK_LED_OFF;
			break;
		case 7: // Blinking at 1 Hz
			*led_state = NSM_NVLINK_LED_AMBER_BLINK;
			break;
		default:
			*led_state = NSM_NVLINK_LED_ERROR;
			break;
		}
	} else {
		*led_state = NSM_NVLINK_LED_ERROR;

		return NSM_ERR_INVALID_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_port_telemetry_counter_req(uint8_t instance_id,
					  uint8_t port_number,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_port_telemetry_counter_req *request =
	    (nsm_get_port_telemetry_counter_req *)msg->payload;

	request->hdr.command = NSM_GET_PORT_TELEMETRY_COUNTER;
	request->hdr.data_size = sizeof(port_number);
	request->port_number = port_number;

	return NSM_SW_SUCCESS;
}

int decode_get_port_telemetry_counter_req(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *port_number)
{
	if (msg == NULL || port_number == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_port_telemetry_counter_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_port_telemetry_counter_req *request =
	    (nsm_get_port_telemetry_counter_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->port_number)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = request->port_number;

	return NSM_SW_SUCCESS;
}

int encode_get_port_telemetry_counter_resp(uint8_t instance_id, uint8_t cc,
					   uint16_t reason_code,
					   struct nsm_port_counter_data *data,
					   struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_PORT_TELEMETRY_COUNTER, msg);
	}

	struct nsm_get_port_telemetry_counter_resp *response =
	    (struct nsm_get_port_telemetry_counter_resp *)msg->payload;

	response->hdr.command = NSM_GET_PORT_TELEMETRY_COUNTER;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(struct nsm_port_counter_data));

	// conversion htole64 for each counter data
	htolePortCounterData(data);

	memcpy(&(response->data), data, sizeof(struct nsm_port_counter_data));

	return NSM_SW_SUCCESS;
}

int decode_get_port_telemetry_counter_resp(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *cc,
					   uint16_t *reason_code,
					   uint16_t *data_size,
					   struct nsm_port_counter_data *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len <
	    (sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp) +
	     PORT_COUNTER_TELEMETRY_MIN_DATA_SIZE)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_port_telemetry_counter_resp *resp =
	    (struct nsm_get_port_telemetry_counter_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < PORT_COUNTER_TELEMETRY_MIN_DATA_SIZE) {
		return NSM_SW_ERROR_DATA;
	}

	if (*data_size > sizeof(*data)) {
		return NSM_SW_ERROR_LENGTH;
	}

	memcpy(data, &(resp->data), *data_size);

	// conversion le64toh for each counter data
	letohPortCounterData(data);

	return NSM_SW_SUCCESS;
}

int encode_query_port_status_req(uint8_t instance_id, uint8_t port_number,
				 struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_query_port_status_req *request =
	    (nsm_query_port_status_req *)msg->payload;

	request->hdr.command = NSM_QUERY_PORT_STATUS;
	request->hdr.data_size = sizeof(port_number);
	request->port_number = port_number;

	return NSM_SW_SUCCESS;
}

int decode_query_port_status_req(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *port_number)
{
	if (msg == NULL || port_number == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_query_port_status_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_query_port_status_req *request =
	    (nsm_query_port_status_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->port_number)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = request->port_number;

	return NSM_SW_SUCCESS;
}

int encode_query_port_status_resp(uint8_t instance_id, uint8_t cc,
				  uint16_t reason_code, uint8_t port_state,
				  uint8_t port_status, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_PORT_STATUS, msg);
	}

	struct nsm_query_port_status_resp *response =
	    (struct nsm_query_port_status_resp *)msg->payload;

	response->hdr.command = NSM_QUERY_PORT_STATUS;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(port_state) + sizeof(port_status));
	response->port_state = port_state;
	response->port_status = port_status;

	return NSM_SW_SUCCESS;
}

int decode_query_port_status_resp(const struct nsm_msg *msg, size_t msg_len,
				  uint8_t *cc, uint16_t *reason_code,
				  uint16_t *data_size, uint8_t *port_state,
				  uint8_t *port_status)
{
	if (data_size == NULL || port_state == NULL || port_status == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_query_port_status_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_port_status_resp *resp =
	    (struct nsm_query_port_status_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < (sizeof(*port_state) + sizeof(*port_status))) {
		return NSM_SW_ERROR_DATA;
	}

	*port_state = resp->port_state;
	*port_status = resp->port_status;

	return NSM_SW_SUCCESS;
}

int encode_query_port_characteristics_req(uint8_t instance_id,
					  uint8_t port_number,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_query_port_characteristics_req *request =
	    (nsm_query_port_characteristics_req *)msg->payload;

	request->hdr.command = NSM_QUERY_PORT_CHARACTERISTICS;
	request->hdr.data_size = sizeof(port_number);
	request->port_number = port_number;

	return NSM_SW_SUCCESS;
}

int decode_query_port_characteristics_req(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *port_number)
{
	if (msg == NULL || port_number == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_query_port_characteristics_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_query_port_characteristics_req *request =
	    (nsm_query_port_characteristics_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->port_number)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = request->port_number;

	return NSM_SW_SUCCESS;
}

int encode_query_port_characteristics_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_port_characteristics_data *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_PORT_CHARACTERISTICS, msg);
	}

	struct nsm_query_port_characteristics_resp *response =
	    (struct nsm_query_port_characteristics_resp *)msg->payload;

	response->hdr.command = NSM_QUERY_PORT_CHARACTERISTICS;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_port_characteristics_data));

	// conversion htole32 for each characteristics data
	htolePortCharacteristicsData(data);

	memcpy(&(response->data), data,
	       sizeof(struct nsm_port_characteristics_data));

	return NSM_SW_SUCCESS;
}

int decode_query_port_characteristics_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_port_characteristics_data *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_query_port_characteristics_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_port_characteristics_resp *resp =
	    (struct nsm_query_port_characteristics_resp *)msg->payload;
	size_t characteristics_data_len =
	    sizeof(struct nsm_port_characteristics_data);

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < characteristics_data_len) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(data, &(resp->data), characteristics_data_len);

	// conversion le32toh for each characteristics data
	letohPortCharacteristicsData(data);

	return NSM_SW_SUCCESS;
}

int encode_query_ports_available_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_query_ports_available_req *request =
	    (nsm_query_ports_available_req *)msg->payload;

	request->command = NSM_QUERY_PORTS_AVAILABLE;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_query_ports_available_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_query_ports_available_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_query_ports_available_req *request =
	    (nsm_query_ports_available_req *)msg->payload;

	if (request->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_query_ports_available_resp(uint8_t instance_id, uint8_t cc,
				      uint16_t reason_code,
				      uint8_t number_of_ports,
				      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_QUERY_PORTS_AVAILABLE, msg);
	}

	struct nsm_query_ports_available_resp *response =
	    (struct nsm_query_ports_available_resp *)msg->payload;

	response->hdr.command = NSM_QUERY_PORTS_AVAILABLE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(number_of_ports));
	response->number_of_ports = number_of_ports;

	return NSM_SW_SUCCESS;
}

int decode_query_ports_available_resp(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *cc, uint16_t *reason_code,
				      uint16_t *data_size,
				      uint8_t *number_of_ports)
{
	if (data_size == NULL || number_of_ports == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_query_ports_available_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_query_ports_available_resp *resp =
	    (struct nsm_query_ports_available_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < sizeof(*number_of_ports)) {
		return NSM_SW_ERROR_DATA;
	}

	*number_of_ports = resp->number_of_ports;

	return NSM_SW_SUCCESS;
}

int encode_set_port_disable_future_req(uint8_t instance,
				       const bitfield8_t *mask,
				       struct nsm_msg *msg)
{
	if (mask == NULL || msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_port_disable_future_req *request =
	    (struct nsm_set_port_disable_future_req *)msg->payload;

	request->hdr.command = NSM_SET_PORT_DISABLE_FUTURE;
	request->hdr.data_size = PORT_MASK_DATA_SIZE;

	memcpy(request->port_mask, mask, PORT_MASK_DATA_SIZE);

	return NSM_SW_SUCCESS;
}

int decode_set_port_disable_future_req(const struct nsm_msg *msg,
				       size_t msg_len,
				       bitfield8_t mask[PORT_MASK_DATA_SIZE])
{
	if (msg == NULL || mask == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_set_port_disable_future_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_port_disable_future_req *request =
	    (struct nsm_set_port_disable_future_req *)msg->payload;

	if (request->hdr.data_size != PORT_MASK_DATA_SIZE) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(mask, request->port_mask, PORT_MASK_DATA_SIZE);

	return NSM_SW_SUCCESS;
}

int encode_set_port_disable_future_resp(uint8_t instance, uint8_t cc,
					uint16_t reason_code,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_SET_PORT_DISABLE_FUTURE, msg);
	}

	nsm_set_port_disable_future_resp *response =
	    (nsm_set_port_disable_future_resp *)msg->payload;

	response->command = NSM_SET_PORT_DISABLE_FUTURE;
	response->completion_code = cc;
	response->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_set_port_disable_future_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code)
{
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_set_port_disable_future_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_port_disable_future_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_port_disable_future_req *request =
	    (nsm_get_port_disable_future_req *)msg->payload;

	request->command = NSM_GET_PORT_DISABLE_FUTURE;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_port_disable_future_req(const struct nsm_msg *msg,
				       size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_port_disable_future_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_port_disable_future_req *request =
	    (nsm_get_port_disable_future_req *)msg->payload;

	if (request->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_port_disable_future_resp(uint8_t instance, uint8_t cc,
					uint16_t reason_code,
					const bitfield8_t *mask,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_PORT_DISABLE_FUTURE, msg);
	}

	struct nsm_get_port_disable_future_resp *response =
	    (struct nsm_get_port_disable_future_resp *)msg->payload;

	response->hdr.command = NSM_GET_PORT_DISABLE_FUTURE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(PORT_MASK_DATA_SIZE);

	memcpy(response->port_mask, mask, PORT_MASK_DATA_SIZE);

	return NSM_SW_SUCCESS;
}

int decode_get_port_disable_future_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					bitfield8_t mask[PORT_MASK_DATA_SIZE])
{
	if (mask == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_port_disable_future_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_port_disable_future_resp *resp =
	    (struct nsm_get_port_disable_future_resp *)msg->payload;

	memcpy(mask, resp->port_mask, PORT_MASK_DATA_SIZE);

	return NSM_SW_SUCCESS;
}

int encode_get_power_mode_req(uint8_t instance_id, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_power_mode_req *request =
	    (nsm_get_power_mode_req *)msg->payload;

	request->command = NSM_GET_POWER_MODE;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_power_mode_req(const struct nsm_msg *msg, size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_get_power_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_power_mode_req *request =
	    (nsm_get_power_mode_req *)msg->payload;

	if (request->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_power_mode_resp(uint8_t instance_id, uint8_t cc,
			       uint16_t reason_code,
			       struct nsm_power_mode_data *data,
			       struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_POWER_MODE,
					  msg);
	}

	struct nsm_get_power_mode_resp *response =
	    (struct nsm_get_power_mode_resp *)msg->payload;

	response->hdr.command = NSM_GET_POWER_MODE;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(sizeof(struct nsm_power_mode_data));

	response->data.l1_hw_mode_control = data->l1_hw_mode_control;
	response->data.l1_hw_mode_threshold =
	    htole32(data->l1_hw_mode_threshold);
	response->data.l1_fw_throttling_mode = data->l1_fw_throttling_mode;
	response->data.l1_prediction_mode = data->l1_prediction_mode;
	response->data.l1_hw_active_time = htole16(data->l1_hw_active_time);
	response->data.l1_hw_inactive_time = htole16(data->l1_hw_inactive_time);
	response->data.l1_prediction_inactive_time =
	    htole16(data->l1_prediction_inactive_time);

	return NSM_SW_SUCCESS;
}

int decode_get_power_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t *cc, uint16_t *reason_code,
			       uint16_t *data_size,
			       struct nsm_power_mode_data *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_get_power_mode_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_power_mode_resp *resp =
	    (struct nsm_get_power_mode_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < sizeof(struct nsm_power_mode_data)) {
		return NSM_SW_ERROR_DATA;
	}

	data->l1_hw_mode_control = resp->data.l1_hw_mode_control;
	data->l1_hw_mode_threshold = le32toh(resp->data.l1_hw_mode_threshold);
	data->l1_fw_throttling_mode = resp->data.l1_fw_throttling_mode;
	data->l1_prediction_mode = resp->data.l1_prediction_mode;
	data->l1_hw_active_time = le16toh(resp->data.l1_hw_active_time);
	data->l1_hw_inactive_time = le16toh(resp->data.l1_hw_inactive_time);
	data->l1_prediction_inactive_time =
	    le16toh(resp->data.l1_prediction_inactive_time);

	return NSM_SW_SUCCESS;
}

int encode_set_power_mode_req(uint8_t instance_id, struct nsm_msg *msg,
			      struct nsm_power_mode_data data)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_set_power_mode_req *request =
	    (struct nsm_set_power_mode_req *)msg->payload;

	request->hdr.command = NSM_SET_POWER_MODE;
	request->hdr.data_size =
	    sizeof(struct nsm_power_mode_data) + sizeof(uint8_t);

	request->l1_hw_mode_control = data.l1_hw_mode_control;
	request->reserved = 0x00; // reserved as per spec
	request->l1_hw_mode_threshold = htole32(data.l1_hw_mode_threshold);
	request->l1_fw_throttling_mode = data.l1_fw_throttling_mode;
	request->l1_prediction_mode = data.l1_prediction_mode;
	request->l1_hw_active_time = htole16(data.l1_hw_active_time);
	request->l1_hw_inactive_time = htole16(data.l1_hw_inactive_time);
	request->l1_prediction_inactive_time =
	    htole16(data.l1_prediction_inactive_time);

	return NSM_SW_SUCCESS;
}

int decode_set_power_mode_req(const struct nsm_msg *msg, size_t msg_len,
			      struct nsm_power_mode_data *data)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_set_power_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_power_mode_req *request =
	    (struct nsm_set_power_mode_req *)msg->payload;

	if (request->hdr.data_size !=
	    (sizeof(struct nsm_power_mode_data) + sizeof(uint8_t))) {
		return NSM_SW_ERROR_DATA;
	}

	data->l1_hw_mode_control = request->l1_hw_mode_control;
	data->l1_hw_mode_threshold = le32toh(request->l1_hw_mode_threshold);
	data->l1_fw_throttling_mode = request->l1_fw_throttling_mode;
	data->l1_prediction_mode = request->l1_prediction_mode;
	data->l1_hw_active_time = le16toh(request->l1_hw_active_time);
	data->l1_hw_inactive_time = le16toh(request->l1_hw_inactive_time);
	data->l1_prediction_inactive_time =
	    le16toh(request->l1_prediction_inactive_time);

	return NSM_SW_SUCCESS;
}

int encode_set_power_mode_resp(uint8_t instance_id, uint16_t reason_code,
			       struct nsm_msg *msg)
{
	return encode_cc_only_resp(instance_id, NSM_TYPE_NETWORK_PORT,
				   NSM_SET_POWER_MODE, NSM_SUCCESS, reason_code,
				   msg);
}

int decode_set_power_mode_resp(const struct nsm_msg *msg, size_t msgLen,
			       uint8_t *cc, uint16_t *reason_code)
{
	int rc = decode_reason_code_and_cc(msg, msgLen, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msgLen <
	    sizeof(struct nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_set_power_mode_resp *resp = (nsm_set_power_mode_resp *)msg->payload;
	if (resp->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_nsm_health_event(uint8_t instance_id, bool ackr,
			    const struct nsm_health_event_payload *payload,
			    struct nsm_msg *msg)
{
	return encode_nsm_event(instance_id, NSM_TYPE_NETWORK_PORT, ackr,
				NSM_EVENT_VERSION, NSM_THRESHOLD_EVENT,
				NSM_GENERAL_EVENT_CLASS, 0, sizeof(*payload),
				(uint8_t *)(payload), msg);
}

int decode_nsm_health_event(const struct nsm_msg *msg, size_t msg_len,
			    uint16_t *event_state,
			    struct nsm_health_event_payload *payload)
{
	if (payload == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	uint8_t data_size = 0;
	int result = decode_nsm_event_with_data(
	    msg, msg_len, NSM_THRESHOLD_EVENT, NSM_GENERAL_EVENT_CLASS,
	    event_state, &data_size, (uint8_t *)payload);
	if (result == NSM_SW_SUCCESS &&
	    data_size != sizeof(struct nsm_health_event_payload)) {
		return NSM_SW_ERROR_LENGTH;
	}
	return result;
}

int encode_get_switch_isolation_mode_req(uint8_t instance, struct nsm_msg *msg)
{
	return encode_common_req(instance, NSM_TYPE_NETWORK_PORT,
				 NSM_GET_SWITCH_ISOLATION_MODE, msg);
}

int decode_get_switch_isolation_mode_req(const struct nsm_msg *msg,
					 size_t msg_len)
{
	return decode_common_req(msg, msg_len);
}

int encode_get_switch_isolation_mode_resp(uint8_t instance, uint8_t cc,
					  uint16_t reason_code,
					  uint8_t isolation_mode,
					  struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_SWITCH_ISOLATION_MODE, msg);
	}

	struct nsm_get_switch_isolation_mode_resp *response =
	    (struct nsm_get_switch_isolation_mode_resp *)msg->payload;

	response->hdr.command = NSM_GET_SWITCH_ISOLATION_MODE;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_get_switch_isolation_mode_resp) -
		    sizeof(struct nsm_common_resp));

	response->isolation_mode = isolation_mode;

	return NSM_SW_SUCCESS;
}

int decode_get_switch_isolation_mode_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code,
					  uint8_t *isolation_mode)
{
	if (isolation_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_get_switch_isolation_mode_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_switch_isolation_mode_resp *resp =
	    (struct nsm_get_switch_isolation_mode_resp *)msg->payload;

	uint16_t data_size = le16toh(resp->hdr.data_size);
	if (data_size != sizeof(struct nsm_get_switch_isolation_mode_resp) -
			     sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_DATA;
	}

	*isolation_mode = resp->isolation_mode;

	return NSM_SW_SUCCESS;
}

int encode_set_switch_isolation_mode_req(uint8_t instance,
					 uint8_t isolation_mode,
					 struct nsm_msg *msg)
{
	int rc = encode_common_req(instance, NSM_TYPE_NETWORK_PORT,
				   NSM_SET_SWITCH_ISOLATION_MODE, msg);

	if (rc == NSM_SW_SUCCESS) {
		struct nsm_set_switch_isolation_mode_req *req =
		    (struct nsm_set_switch_isolation_mode_req *)msg->payload;
		req->hdr.data_size =
		    sizeof(struct nsm_set_switch_isolation_mode_req) -
		    sizeof(struct nsm_common_req);
		req->isolation_mode = isolation_mode;
	}
	return rc;
}

int decode_set_switch_isolation_mode_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint8_t *isolation_mode)
{
	if (msg == NULL || isolation_mode == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	uint8_t rc = unpack_nsm_header(&msg->hdr, &header);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (msg_len != sizeof(struct nsm_msg_hdr) +
			   sizeof(struct nsm_set_switch_isolation_mode_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_set_switch_isolation_mode_req *req =
	    (struct nsm_set_switch_isolation_mode_req *)msg->payload;

	if (req->hdr.data_size !=
	    sizeof(struct nsm_set_switch_isolation_mode_req) -
		sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}

	*isolation_mode = req->isolation_mode;
	return NSM_SW_SUCCESS;
}

int encode_set_switch_isolation_mode_resp(uint8_t instance, uint8_t cc,
					  uint16_t reason_code,
					  struct nsm_msg *msg)
{
	return encode_common_resp(instance, cc, reason_code,
				  NSM_TYPE_NETWORK_PORT,
				  NSM_SET_SWITCH_ISOLATION_MODE, msg);
}

int decode_set_switch_isolation_mode_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code)
{
	uint16_t data_size;
	return decode_common_resp(msg, msg_len, cc, &data_size, reason_code);
}

int encode_get_fabric_manager_state_req(uint8_t instance_id,
					struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	nsm_get_fabric_manager_state_req *request =
	    (nsm_get_fabric_manager_state_req *)msg->payload;

	request->command = NSM_GET_FABRIC_MANAGER_STATE;
	request->data_size = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_fabric_manager_state_req(const struct nsm_msg *msg,
					size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(nsm_get_fabric_manager_state_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	nsm_get_fabric_manager_state_req *request =
	    (nsm_get_fabric_manager_state_req *)msg->payload;

	if (request->data_size != 0) {
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_fabric_manager_state_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_fabric_manager_state_data *data, struct nsm_msg *msg)
{
	if (msg == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code,
					  NSM_GET_FABRIC_MANAGER_STATE, msg);
	}

	struct nsm_get_fabric_manager_state_resp *response =
	    (struct nsm_get_fabric_manager_state_resp *)msg->payload;

	response->hdr.command = NSM_GET_FABRIC_MANAGER_STATE;
	response->hdr.completion_code = cc;
	response->hdr.data_size =
	    htole16(sizeof(struct nsm_fabric_manager_state_data));

	response->data.fm_state = data->fm_state;
	response->data.report_status = data->report_status;
	response->data.last_restart_timestamp =
	    htole64(data->last_restart_timestamp);
	response->data.duration_since_last_restart_sec =
	    htole64(data->duration_since_last_restart_sec);

	return NSM_SW_SUCCESS;
}

int decode_get_fabric_manager_state_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_fabric_manager_state_data *data)
{
	if (data_size == NULL || data == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}

	if (msg_len < (sizeof(struct nsm_msg_hdr) +
		       sizeof(struct nsm_get_fabric_manager_state_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_fabric_manager_state_resp *resp =
	    (struct nsm_get_fabric_manager_state_resp *)msg->payload;

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < sizeof(struct nsm_fabric_manager_state_data)) {
		return NSM_SW_ERROR_DATA;
	}

	data->fm_state = resp->data.fm_state;
	data->report_status = resp->data.report_status;
	data->last_restart_timestamp =
	    le64toh(resp->data.last_restart_timestamp);
	data->duration_since_last_restart_sec =
	    le64toh(resp->data.duration_since_last_restart_sec);

	return NSM_SW_SUCCESS;
}

int encode_nsm_get_fabric_manager_state_event(
    uint8_t instance_id, bool ackr,
    nsm_get_fabric_manager_state_event_payload payload, struct nsm_msg *msg)
{
	payload.last_restart_timestamp =
	    htole64(payload.last_restart_timestamp);
	payload.duration_since_last_restart_sec =
	    htole64(payload.duration_since_last_restart_sec);

	uint8_t event_data[NSM_EVENT_DATA_MAX_LEN];
	memcpy(event_data, &payload, sizeof(payload));

	return encode_nsm_event(
	    instance_id, NSM_TYPE_NETWORK_PORT, ackr, NSM_EVENT_VERSION,
	    NSM_FABRIC_MANAGER_STATE_EVENT, NSM_GENERAL_EVENT_CLASS, 0,
	    sizeof(payload), event_data, msg);
}

int decode_nsm_get_fabric_manager_state_event(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *event_class,
    uint16_t *event_state, nsm_get_fabric_manager_state_event_payload *payload)
{
	if (msg == NULL || event_class == NULL || event_state == NULL ||
	    payload == NULL) {
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
	}

	memcpy(payload, event->data, sizeof(*payload));

	payload->last_restart_timestamp =
	    le64toh(payload->last_restart_timestamp);
	payload->duration_since_last_restart_sec =
	    le64toh(payload->duration_since_last_restart_sec);

	return NSM_SW_SUCCESS;
}

int encode_get_eth_port_telemetry_counter_req(uint8_t instance_id,
					      uint16_t port_number,
					      struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_ethernet_port_telemetry_counter_req *request =
	    (struct nsm_get_ethernet_port_telemetry_counter_req *)msg->payload;

	request->hdr.command = NSM_GET_ETH_PORT_TELEMETRY_COUNTER;
	request->hdr.data_size = sizeof(port_number);
	request->port_number = htole16(port_number);

	return NSM_SW_SUCCESS;
}

int decode_get_eth_port_telemetry_counter_req(const struct nsm_msg *msg,
					      size_t msg_len,
					      uint16_t *port_number)
{
	if (msg == NULL || port_number == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len <
	    sizeof(struct nsm_msg_hdr) +
		sizeof(struct nsm_get_ethernet_port_telemetry_counter_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_ethernet_port_telemetry_counter_req *request =
	    (struct nsm_get_ethernet_port_telemetry_counter_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->port_number)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = le16toh(request->port_number);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_eth_port_telemetry_data(
    const uint8_t *data, size_t *data_len, uint8_t tag,
    nsm_ethernet_port_counter_data *counter_reading)
{
	if (data == NULL || data_len == NULL || counter_reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	switch (tag) {
	case ETHERNET_PORT_COUNTER_TAG_RX_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_UNICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_MULTICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_BROADCAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_UNICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_MULTICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_BROADCAST_BYTES: {
		if (*data_len != sizeof(uint64_t)) {
			return NSM_SW_ERROR_LENGTH;
		}
		uint64_t le_reading;
		memcpy(&le_reading, data, sizeof(uint64_t));
		counter_reading->ethernet_port_counter_data_64bit =
		    le64toh(le_reading);
		break;
	}
	case ETHERNET_PORT_COUNTER_TAG_RX_FCS_ERRORS:
	case ETHERNET_PORT_COUNTER_TAG_RX_ALIGNMENT_ERRORS:
	case ETHERNET_PORT_COUNTER_TAG_RX_FALSE_CARRIER_DETECTIONS:
	case ETHERNET_PORT_COUNTER_TAG_RX_RUNT_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_JABBER_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_XON_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_XOFF_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_TX_XON_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_TX_XOFF_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_SINGLE_COLLISION_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_MULTIPLE_COLLISION_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_LATE_COLLISION_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_EXCESSIVE_COLLISION_FRAMES: {
		if (*data_len != sizeof(uint32_t)) {
			return NSM_SW_ERROR_LENGTH;
		}
		uint32_t le_reading32;
		memcpy(&le_reading32, data, sizeof(uint32_t));
		counter_reading->ethernet_port_counter_data_32bit =
		    le32toh(le_reading32);
		break;
	}
	default:
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_aggregate_eth_port_telemetry_data(
    uint8_t tag, nsm_ethernet_port_counter_data *counter_reading, uint8_t *data,
    size_t *data_len)
{
	if (data == NULL || data_len == NULL || counter_reading == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	switch (tag) {
	case ETHERNET_PORT_COUNTER_TAG_RX_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_UNICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_MULTICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_BROADCAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_UNICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_MULTICAST_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_TX_BROADCAST_BYTES: {
		if (*data_len != sizeof(uint64_t)) {
			return NSM_SW_ERROR_LENGTH;
		}
		uint64_t le_reading =
		    htole64(counter_reading->ethernet_port_counter_data_64bit);
		memcpy(data, &le_reading, sizeof(uint64_t));
		*data_len = sizeof(uint64_t);
		break;
	}
	case ETHERNET_PORT_COUNTER_TAG_RX_FCS_ERRORS:
	case ETHERNET_PORT_COUNTER_TAG_RX_ALIGNMENT_ERRORS:
	case ETHERNET_PORT_COUNTER_TAG_RX_FALSE_CARRIER_DETECTIONS:
	case ETHERNET_PORT_COUNTER_TAG_RX_RUNT_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_JABBER_BYTES:
	case ETHERNET_PORT_COUNTER_TAG_RX_XON_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_XOFF_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_TX_XON_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_TX_XOFF_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_SINGLE_COLLISION_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_MULTIPLE_COLLISION_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_LATE_COLLISION_FRAMES:
	case ETHERNET_PORT_COUNTER_TAG_RX_EXCESSIVE_COLLISION_FRAMES: {
		if (*data_len != sizeof(uint32_t)) {
			return NSM_SW_ERROR_LENGTH;
		}
		uint32_t le_reading32 =
		    htole32(counter_reading->ethernet_port_counter_data_32bit);
		memcpy(data, &le_reading32, sizeof(uint32_t));
		*data_len = sizeof(uint32_t);
		break;
	}
	default:
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_network_addresses_req(uint8_t instance_id, uint16_t port_number,
				     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_network_addresses_req *request =
	    (struct nsm_get_network_addresses_req *)msg->payload;

	request->hdr.command = NSM_GET_NETWORK_ADDRESSES;
	request->hdr.data_size = sizeof(port_number);
	request->port_number = htole16(port_number);

	return NSM_SW_SUCCESS;
}

int decode_get_network_addresses_req(const struct nsm_msg *msg, size_t msg_len,
				     uint16_t *port_number)
{
	if (msg == NULL || port_number == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_network_addresses_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_network_addresses_req *request =
	    (struct nsm_get_network_addresses_req *)msg->payload;

	if (request->hdr.data_size != sizeof(request->port_number)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = le16toh(request->port_number);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_network_address_data(uint8_t tag, const uint8_t *data,
					  size_t data_len,
					  network_address_sample_data *address)
{
	if (data == NULL || address == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	switch (tag) {
	case NSM_TAG_LINK_TYPE:
		if (data_len != sizeof(uint8_t)) {
			return NSM_SW_ERROR_LENGTH;
		}
		address->link_type = data[0];
		break;

	case NSM_TAG_MAC_ADDRESS:
	case NSM_TAG_PERMANENT_MAC_ADDRESS:
		if (data_len != MAC_ADDRESS_LENGTH) {
			return NSM_SW_ERROR_LENGTH;
		}
		memcpy(address->mac_address, data, MAC_ADDRESS_LENGTH);
		break;

	case NSM_TAG_NODE_GUID:
	case NSM_TAG_PORT_GUID:
		if (data_len != sizeof(uint64_t)) {
			return NSM_SW_ERROR_LENGTH;
		}

		uint64_t le_reading;
		memcpy(&le_reading, data, sizeof(uint64_t));
		address->network_identifier_64bit = le64toh(le_reading);
		break;
	default:
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_aggregate_network_address_data(
    uint8_t tag, const network_address_sample_data *address, uint8_t *data,
    size_t *data_len)
{
	if (address == NULL || data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	switch (tag) {
	case NSM_TAG_LINK_TYPE:
		*data_len = sizeof(uint8_t);
		data[0] = address->link_type;
		break;

	case NSM_TAG_MAC_ADDRESS:
	case NSM_TAG_PERMANENT_MAC_ADDRESS: {
		memcpy(data, address->mac_address, MAC_ADDRESS_LENGTH);
		*data_len = MAC_ADDRESS_LENGTH;
		break;
	}

	case NSM_TAG_NODE_GUID:
	case NSM_TAG_PORT_GUID: {
		uint64_t le_reading =
		    htole64(address->network_identifier_64bit);
		memcpy(data, &le_reading, sizeof(uint64_t));
		*data_len = sizeof(uint64_t);
		break;
	}

	default:
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_port_ecc_counters_req(uint8_t instance_id, uint16_t port_number,
				     struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_port_ecc_counters_req *request =
	    (struct nsm_get_port_ecc_counters_req *)msg->payload;

	request->hdr.command = NSM_GET_PORT_ECC_COUNTERS;
	request->hdr.data_size = sizeof(port_number);
	request->port_number = htole16(port_number);

	return NSM_SW_SUCCESS;
}

int decode_get_port_ecc_counters_req(const struct nsm_msg *msg, size_t msg_len,
				     uint16_t *port_number)
{
	if (msg == NULL || port_number == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_port_ecc_counters_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_port_ecc_counters_req *request =
	    (struct nsm_get_port_ecc_counters_req *)msg->payload;

	if (request->hdr.data_size < sizeof(request->port_number)) {
		return NSM_SW_ERROR_DATA;
	}

	*port_number = le16toh(request->port_number);

	return NSM_SW_SUCCESS;
}

int decode_aggregate_port_ecc_counter_data(uint8_t tag, const uint8_t *data,
					   size_t data_len,
					   uint64_t *counter_value)
{
	if (data == NULL || counter_value == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint64_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	switch (tag) {
	case NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES:
	case NSM_TAG_ECC_CORRECTED_BITS:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_0:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_1:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_2:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_3: {
		uint64_t le_reading;
		memcpy(&le_reading, data, sizeof(uint64_t));
		*counter_value = le64toh(le_reading);
		break;
	}
	default:
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_aggregate_port_ecc_counter_data(uint8_t tag, uint64_t counter_value,
					   uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	switch (tag) {
	case NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES:
	case NSM_TAG_ECC_CORRECTED_BITS:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_0:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_1:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_2:
	case NSM_TAG_ECC_RAW_ERRORS_LANE_3: {
		uint64_t le_value = htole64(counter_value);
		memcpy(data, &le_value, sizeof(uint64_t));
		*data_len = sizeof(uint64_t);
		break;
	}
	default:
		return NSM_SW_ERROR_DATA;
	}

	return NSM_SW_SUCCESS;
}

int encode_get_lldp_packet_req(uint8_t instance_id, uint16_t port_number,
			       uint8_t direction, struct nsm_msg *msg)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (direction != NSM_LLDP_DIRECTION_RX &&
	    direction != NSM_LLDP_DIRECTION_TX) {
		return NSM_SW_ERROR_DATA;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_REQUEST;
	header.instance_id = instance_id;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &(msg->hdr));
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	struct nsm_get_lldp_packet_req *request =
	    (struct nsm_get_lldp_packet_req *)msg->payload;

	request->hdr.command = NSM_GET_LLDP_PACKET;
	request->hdr.data_size = sizeof(struct nsm_get_lldp_packet_req) -
				 sizeof(struct nsm_common_req);
	request->port_number = htole16(port_number);
	request->reserved = 0;
	request->direction = direction;
	request->reserved2 = 0;
	request->reserved3 = 0;

	return NSM_SW_SUCCESS;
}

int decode_get_lldp_packet_req(const struct nsm_msg *msg, size_t msg_len,
			       uint16_t *port_number, uint8_t *direction)
{
	if (msg == NULL || port_number == NULL || direction == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (msg_len < sizeof(struct nsm_msg_hdr) +
			  sizeof(struct nsm_get_lldp_packet_req)) {
		return NSM_SW_ERROR_LENGTH;
	}
	struct nsm_get_lldp_packet_req *request =
	    (struct nsm_get_lldp_packet_req *)msg->payload;
	if (request->hdr.data_size < sizeof(struct nsm_get_lldp_packet_req) -
					 sizeof(struct nsm_common_req)) {
		return NSM_SW_ERROR_DATA;
	}
	*port_number = le16toh(request->port_number);
	*direction = request->direction;
	return NSM_SW_SUCCESS;
}

int encode_get_lldp_packet_resp(uint8_t instance_id, uint8_t cc,
				uint16_t reason_code, const uint8_t *data,
				uint16_t data_size, struct nsm_msg *msg,
				size_t msg_len)
{
	if (msg == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (data == NULL && data_size != 0) {
		return NSM_SW_ERROR_NULL;
	}
	if (data_size > NSM_LLDP_PACKET_MAX_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_header_info header = {0};
	header.nsm_msg_type = NSM_RESPONSE;
	header.instance_id = instance_id & INSTANCEID_MASK;
	header.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;

	uint8_t rc = pack_nsm_header(&header, &msg->hdr);
	if (rc != NSM_SW_SUCCESS) {
		return rc;
	}

	if (cc != NSM_SUCCESS) {
		return encode_reason_code(cc, reason_code, NSM_GET_LLDP_PACKET,
					  msg);
	}

	size_t header_bytes =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp);
	if (msg_len < header_bytes + data_size) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_lldp_packet_resp *response =
	    (struct nsm_get_lldp_packet_resp *)msg->payload;
	response->hdr.command = NSM_GET_LLDP_PACKET;
	response->hdr.completion_code = cc;
	response->hdr.data_size = htole16(data_size);
	if (data_size > 0) {
		memcpy(&(response->data[0]), data, data_size);
	}
	return NSM_SW_SUCCESS;
}

int decode_get_lldp_packet_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *reason_code,
				uint8_t *data, uint16_t *data_size)
{
	if (data_size == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	int rc = decode_reason_code_and_cc(msg, msg_len, cc, reason_code);
	if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) {
		return rc;
	}
	if (msg_len <
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp)) {
		return NSM_SW_ERROR_LENGTH;
	}
	struct nsm_get_lldp_packet_resp *resp =
	    (struct nsm_get_lldp_packet_resp *)msg->payload;
	uint16_t reported_size = le16toh(resp->hdr.data_size);
	if (reported_size > NSM_LLDP_PACKET_MAX_DATA_SIZE) {
		return NSM_SW_ERROR_LENGTH;
	}

	size_t header_bytes =
	    sizeof(struct nsm_msg_hdr) + sizeof(struct nsm_common_resp);
	if (msg_len < header_bytes + reported_size) {
		return NSM_SW_ERROR_LENGTH;
	}
	if (data == NULL && reported_size != 0) {
		return NSM_SW_ERROR_NULL;
	}
	if (data != NULL && reported_size > *data_size) {
		return NSM_SW_ERROR_LENGTH;
	}
	if (reported_size > 0) {
		memcpy(data, &(resp->data[0]), reported_size);
	}
	*data_size = reported_size;
	return NSM_SW_SUCCESS;
}
