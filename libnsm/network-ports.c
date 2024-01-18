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
	portData->symbol_error = htole64(portData->symbol_error);
	portData->link_error_recovery_counter =
	    htole64(portData->link_error_recovery_counter);
	portData->link_downed_counter = htole64(portData->link_downed_counter);
	portData->port_rcv_remote_physical_errors =
	    htole64(portData->port_rcv_remote_physical_errors);
	portData->port_rcv_switch_relay_errors =
	    htole64(portData->port_rcv_switch_relay_errors);
	portData->QP1_dropped = htole64(portData->QP1_dropped);
	portData->xmit_wait = htole64(portData->xmit_wait);
}

static void letohPortCounterData(struct nsm_port_counter_data *portData)
{
	uint32_t le32toh_supported_counter_mask;
	memcpy(&le32toh_supported_counter_mask, &(portData->supported_counter),
	       sizeof(struct nsm_supported_port_counter));
	le32toh_supported_counter_mask = le32toh(le32toh_supported_counter_mask);
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
	portData->symbol_error = le64toh(portData->symbol_error);
	portData->link_error_recovery_counter =
	    le64toh(portData->link_error_recovery_counter);
	portData->link_downed_counter = le64toh(portData->link_downed_counter);
	portData->port_rcv_remote_physical_errors =
	    le64toh(portData->port_rcv_remote_physical_errors);
	portData->port_rcv_switch_relay_errors =
	    le64toh(portData->port_rcv_switch_relay_errors);
	portData->QP1_dropped = le64toh(portData->QP1_dropped);
	portData->xmit_wait = le64toh(portData->xmit_wait);
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

	struct nsm_get_port_telemetry_counter_req *request =
	    (struct nsm_get_port_telemetry_counter_req *)msg->payload;

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
			  sizeof(struct nsm_get_port_telemetry_counter_req)) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_port_telemetry_counter_req *request =
	    (struct nsm_get_port_telemetry_counter_req *)msg->payload;

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

	if (msg_len != (sizeof(struct nsm_msg_hdr) +
			sizeof(struct nsm_get_port_telemetry_counter_resp))) {
		return NSM_SW_ERROR_LENGTH;
	}

	struct nsm_get_port_telemetry_counter_resp *resp =
	    (struct nsm_get_port_telemetry_counter_resp *)msg->payload;
	size_t counter_data_len = sizeof(struct nsm_port_counter_data);

	*data_size = le16toh(resp->hdr.data_size);
	if (*data_size < counter_data_len) {
		return NSM_SW_ERROR_DATA;
	}

	memcpy(data, &(resp->data), counter_data_len);

	// conversion le64toh for each counter data
	letohPortCounterData(data);

	return NSM_SW_SUCCESS;
}