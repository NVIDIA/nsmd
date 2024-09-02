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

#ifndef NETWORK_PORT_H
#define NETWORK_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"
#define PORT_COUNTER_TELEMETRY_DATA_SIZE 204
#define PORT_MASK_DATA_SIZE 32
// defined in MBps
#define MAXLINKBANDWIDTH 50000

/** @brief NSM Type1 network port telemetry commands
 */
enum nsm_port_telemetry_commands {
	NSM_GET_PORT_TELEMETRY_COUNTER = 0x01,
	NSM_GET_PORT_HEALTH_THRESHOLDS = 0x02,
	NSM_SET_SYSTEM_GUID = 0x03,
	NSM_GET_SYSTEM_GUID = 0x04,
	NSM_SET_LINK_DISABLE_STICKY = 0x05,
	NSM_GET_LINK_DISABLE_STICKY = 0x06,
	NSM_SET_PORT_HEALTH_THRESHOLDS = 0x07,
	NSM_SET_SWITCH_ISOLATION_MODE = 0x08,
	NSM_GET_SWITCH_ISOLATION_MODE = 0x09,
	NSM_SET_POWER_MODE = 0x0a,
	NSM_GET_POWER_MODE = 0x0b,
	NSM_SET_POWER_PROFILE = 0x0c,
	NSM_GET_POWER_PROFILE = 0x0d,
	NSM_QUERY_PORTS_AVAILABLE = 0x41,
	NSM_QUERY_PORT_CHARACTERISTICS = 0x42,
	NSM_QUERY_PORT_STATUS = 0x43,
	NSM_SET_PORT_DISABLE_FUTURE = 0x44,
	NSM_GET_PORT_DISABLE_FUTURE = 0x45
};

/** @brief Port state
 */
enum nsm_port_state {
	NSM_PORTSTATE_DOWN = 0x01,
	NSM_PORTSTATE_UP = 0x02,
	NSM_PORTSTATE_RESERVED = 0x03,
	NSM_PORTSTATE_SLEEP = 0x04,
	NSM_PORTSTATE_DOWN_LOCK = 0x05,
	NSM_PORTSTATE_POLLING = 0x06,
	NSM_PORTSTATE_TRAINING = 0x07,
	NSM_PORTSTATE_TRAINING_FAILURE = 0x08
};

/** @brief Port status
 */
enum nsm_port_status {
	NSM_PORTSTATUS_DISABLED = 0x01,
	NSM_PORTSTATUS_ENABLED = 0x02
};

struct nsm_supported_port_counter {
	uint8_t port_rcv_pkts : 1;
	uint8_t port_rcv_data : 1;
	uint8_t port_multicast_rcv_pkts : 1;
	uint8_t port_unicast_rcv_pkts : 1;
	uint8_t port_malformed_pkts : 1;
	uint8_t vl15_dropped : 1;
	uint8_t port_rcv_errors : 1;
	uint8_t port_xmit_pkts : 1;

	uint8_t port_xmit_pkts_vl15 : 1;
	uint8_t port_xmit_data : 1;
	uint8_t port_xmit_data_vl15 : 1;
	uint8_t port_unicast_xmit_pkts : 1;
	uint8_t port_multicast_xmit_pkts : 1;
	uint8_t port_bcast_xmit_pkts : 1;
	uint8_t port_xmit_discard : 1;
	uint8_t port_neighbor_mtu_discards : 1;

	uint8_t port_rcv_ibg2_pkts : 1;
	uint8_t port_xmit_ibg2_pkts : 1;
	uint8_t symbol_error : 1;
	uint8_t link_error_recovery_counter : 1;
	uint8_t link_downed_counter : 1;
	uint8_t port_rcv_remote_physical_errors : 1;
	uint8_t port_rcv_switch_relay_errors : 1;
	uint8_t QP1_dropped : 1;

	uint8_t xmit_wait : 1;
	uint8_t unused : 7;
} __attribute__((packed));

struct nsm_port_counter_data {
	struct nsm_supported_port_counter supported_counter;
	uint64_t port_rcv_pkts;
	uint64_t port_rcv_data;
	uint64_t port_multicast_rcv_pkts;
	uint64_t port_unicast_rcv_pkts;
	uint64_t port_malformed_pkts;
	uint64_t vl15_dropped;
	uint64_t port_rcv_errors;
	uint64_t port_xmit_pkts;
	uint64_t port_xmit_pkts_vl15;
	uint64_t port_xmit_data;
	uint64_t port_xmit_data_vl15;
	uint64_t port_unicast_xmit_pkts;
	uint64_t port_multicast_xmit_pkts;
	uint64_t port_bcast_xmit_pkts;
	uint64_t port_xmit_discard;
	uint64_t port_neighbor_mtu_discards;
	uint64_t port_rcv_ibg2_pkts;
	uint64_t port_xmit_ibg2_pkts;
	uint64_t symbol_error;
	uint64_t link_error_recovery_counter;
	uint64_t link_downed_counter;
	uint64_t port_rcv_remote_physical_errors;
	uint64_t port_rcv_switch_relay_errors;
	uint64_t QP1_dropped;
	uint64_t xmit_wait;
} __attribute__((packed));

struct nsm_port_characteristics_data {
	uint32_t status;
	uint32_t nv_port_line_rate_mbps;
	uint32_t nv_port_data_rate_kbps;
	uint32_t status_lane_info;
} __attribute__((packed));

struct nsm_power_mode_data {
	uint8_t l1_hw_mode_control;
	uint32_t l1_hw_mode_threshold;
	uint8_t l1_fw_throttling_mode;
	uint8_t l1_prediction_mode;
	uint16_t l1_hw_active_time;
	uint16_t l1_hw_inactive_time;
	uint16_t l1_prediction_inactive_time;
} __attribute__((packed));

/** @struct nsm_common_port_req
 *
 *  Structure representing NSM common port request.
 */
struct nsm_common_port_req {
	struct nsm_common_req hdr;
	uint8_t port_number;
} __attribute__((packed));

/** @struct nsm_get_port_telemetry_counter_req
 *
 *  Structure representing NSM get port telemetry counter request.
 */
typedef struct nsm_common_port_req nsm_get_port_telemetry_counter_req;

/** @struct nsm_get_port_telemetry_counter_resp
 *
 *  Structure representing NSM get port telemetry counter response.
 */
struct nsm_get_port_telemetry_counter_resp {
	struct nsm_common_resp hdr;
	struct nsm_port_counter_data data;
} __attribute__((packed));

/** @struct nsm_query_port_status_req
 *
 *  Structure representing NSM query port status request.
 */
typedef struct nsm_common_port_req nsm_query_port_status_req;

/** @struct nsm_query_port_status_resp
 *
 *  Structure representing NSM query port status response.
 */
struct nsm_query_port_status_resp {
	struct nsm_common_resp hdr;
	uint8_t port_state;
	uint8_t port_status;
} __attribute__((packed));

/** @struct nsm_query_port_characteristics_req
 *
 *  Structure representing NSM query port characteristics request.
 */
typedef struct nsm_common_port_req nsm_query_port_characteristics_req;

/** @struct nsm_query_port_characteristics_resp
 *
 *  Structure representing NSM query port characteristics response.
 */
struct nsm_query_port_characteristics_resp {
	struct nsm_common_resp hdr;
	struct nsm_port_characteristics_data data;
} __attribute__((packed));

/** @struct nsm_query_ports_available_req
 *
 *  Structure representing NSM query ports available request.
 */
typedef struct nsm_common_req nsm_query_ports_available_req;

/** @struct nsm_query_ports_available_resp
 *
 *  Structure representing NSM query ports available response.
 */
struct nsm_query_ports_available_resp {
	struct nsm_common_resp hdr;
	uint8_t number_of_ports;
} __attribute__((packed));

/** @struct nsm_set_port_disable_future_req
 *
 *  Structure representing NSM set port disable future request.
 */
struct nsm_set_port_disable_future_req {
	struct nsm_common_req hdr;
	bitfield8_t port_mask[PORT_MASK_DATA_SIZE];
} __attribute__((packed));

/** @struct nsm_set_port_disable_future_resp
 *
 *  Structure representing NSM set port disable future response.
 */
typedef struct nsm_common_resp nsm_set_port_disable_future_resp;

/** @struct nsm_get_port_disable_future_req
 *
 *  Structure representing NSM get port disable future request.
 */
typedef struct nsm_common_req nsm_get_port_disable_future_req;

/** @struct nsm_get_port_disable_future_resp
 *
 *  Structure representing NSM get port disable future response.
 */
struct nsm_get_port_disable_future_resp {
	struct nsm_common_resp hdr;
	bitfield8_t port_mask[PORT_MASK_DATA_SIZE];
} __attribute__((packed));

/** @struct nsm_get_power_mode_req
 *
 *  Structure representing NSM get power mode request.
 */
typedef struct nsm_common_req nsm_get_power_mode_req;

/** @struct nsm_get_power_mode_resp
 *
 *  Structure representing NSM get power mode response.
 */
struct nsm_get_power_mode_resp {
	struct nsm_common_resp hdr;
	struct nsm_power_mode_data data;
} __attribute__((packed));

/** @struct nsm_set_power_mode_req
 *
 *  Structure representing NSM set power mode request.
 */
struct nsm_set_power_mode_req {
	struct nsm_common_req hdr;
	uint8_t l1_hw_mode_control;
	uint8_t reserved;
	uint32_t l1_hw_mode_threshold;
	uint8_t l1_fw_throttling_mode;
	uint8_t l1_prediction_mode;
	uint16_t l1_hw_active_time;
	uint16_t l1_hw_inactive_time;
	uint16_t l1_prediction_inactive_time;
} __attribute__((packed));

/** @struct nsm_set_power_mode_resp
 *
 *  Structure representing NSM set power mode response.
 */
typedef struct nsm_common_resp nsm_set_power_mode_resp;

/** @brief Encode a Get port telemetry counter request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - port number
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_port_telemetry_counter_req(uint8_t instance_id,
					  uint8_t port_number,
					  struct nsm_msg *msg);

/** @brief Decode a Get port telemetry counter request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - port number
 *  @return nsm_completion_codes
 */
int decode_get_port_telemetry_counter_req(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *port_number);

/** @brief Encode a Get port telemetry counter response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] data - port telemetry counter data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_port_telemetry_counter_resp(uint8_t instance_id, uint8_t cc,
					   uint16_t reason_code,
					   struct nsm_port_counter_data *data,
					   struct nsm_msg *msg);

/** @brief Decode a Get port telemetry counter response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] data_size - data size in bytes
 *  @param[out] data - port telemetry counter data
 *  @return nsm_completion_codes
 */
int decode_get_port_telemetry_counter_resp(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *cc,
					   uint16_t *reason_code,
					   uint16_t *data_size,
					   struct nsm_port_counter_data *data);

/** @brief Encode a Query port status request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - port number
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_port_status_req(uint8_t instance_id, uint8_t port_number,
				 struct nsm_msg *msg);

/** @brief Decode a Query port status request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - port number
 *  @return nsm_completion_codes
 */
int decode_query_port_status_req(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *port_number);

/** @brief Encode a Query port status response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] port_state - port state data
 *  @param[in] port_status - port status data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_port_status_resp(uint8_t instance_id, uint8_t cc,
				  uint16_t reason_code, uint8_t port_state,
				  uint8_t port_status, struct nsm_msg *msg);

/** @brief Decode a Query port status response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] data_size - data size in bytes
 *  @param[out] port_state - port state data
 *  @param[out] port_status - port status data
 *  @return nsm_completion_codes
 */
int decode_query_port_status_resp(const struct nsm_msg *msg, size_t msg_len,
				  uint8_t *cc, uint16_t *reason_code,
				  uint16_t *data_size, uint8_t *port_state,
				  uint8_t *port_status);

/** @brief Encode a query port characteristics request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - port number
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_port_characteristics_req(uint8_t instance_id,
					  uint8_t port_number,
					  struct nsm_msg *msg);

/** @brief Decode a query port characteristics request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - port number
 *  @return nsm_completion_codes
 */
int decode_query_port_characteristics_req(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *port_number);

/** @brief Encode a query port characteristics response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] data - port characteristics data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_port_characteristics_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_port_characteristics_data *data, struct nsm_msg *msg);

/** @brief Decode a query port characteristics response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] data_size - data size in bytes
 *  @param[out] data - port characteristics data
 *  @return nsm_completion_codes
 */
int decode_query_port_characteristics_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_port_characteristics_data *data);

/** @brief Encode a query ports available request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_ports_available_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a query ports available request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_query_ports_available_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a query ports available response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - response message completion code
 *  @param[in] reason_code - reason code
 *  @param[in] number_of_ports - number of ports
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_ports_available_resp(uint8_t instance_id, uint8_t cc,
				      uint16_t reason_code,
				      uint8_t number_of_ports,
				      struct nsm_msg *msg);

/** @brief Decode a query ports available response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] data_size - data size in bytes
 *  @param[out] number_of_ports - number of ports
 *  @return nsm_completion_codes
 */
int decode_query_ports_available_resp(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *cc, uint16_t *reason_code,
				      uint16_t *data_size,
				      uint8_t *number_of_ports);

/** @brief Encode a set port disable future request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] mask - pointer to array bitfield8_t[32] containing mask
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_port_disable_future_req(uint8_t instance,
				       const bitfield8_t *mask,
				       struct nsm_msg *msg);

/** @brief Decode a set port disable future request message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] mask  - pointer to array bitfield8_t[32] for receiving mask
 *  @return nsm_completion_codes
 */
int decode_set_port_disable_future_req(const struct nsm_msg *msg,
				       size_t msg_len,
				       bitfield8_t mask[PORT_MASK_DATA_SIZE]);

/** @brief Encode a set port disable future response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - completion code
 *  @param[in] reason_code - reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_port_disable_future_resp(uint8_t instance, uint8_t cc,
					uint16_t reason_code,
					struct nsm_msg *msg);

/** @brief Decode a set port disable future response message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code  - pointer to reason code
 *  @return nsm_completion_codes
 */
int decode_set_port_disable_future_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code);

/** @brief Encode a get port disable future request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_port_disable_future_req(uint8_t instance, struct nsm_msg *msg);

/** @brief Decode a get port disable future request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_port_disable_future_req(const struct nsm_msg *msg,
				       size_t msg_len);

/** @brief Encode a get port disable future response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] mask - pointer to array bitfield8_t[32] containing mask
 *  @param[in] cc - completion code
 *  @param[in] reason_code - reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_port_disable_future_resp(uint8_t instance, uint8_t cc,
					uint16_t reason_code,
					const bitfield8_t *mask,
					struct nsm_msg *msg);

/** @brief Decode a get port disable future response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code  - pointer to reason code
 *  @param[out] mask  - pointer to array bitfield8_t[32] for receiving mask
 *  @return nsm_completion_codes
 */
int decode_get_port_disable_future_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					bitfield8_t mask[PORT_MASK_DATA_SIZE]);

/** @brief Create a Get power mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_power_mode_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode Get power mode request message
 *
 *  @param[in] msg    - Request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_power_mode_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a get power mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - Response message completion code
 *  @param[in] reason_code - Reason code
 *  @param[in] data - Power mode data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_power_mode_resp(uint8_t instance_id, uint8_t cc,
			       uint16_t reason_code,
			       struct nsm_power_mode_data *data,
			       struct nsm_msg *msg);

/** @brief Decode a get power mode response message
 *
 *  @param[in] msg    - Response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - Pointer to response message completion code
 *  @param[out] reason_code     - Pointer to reason code
 *  @param[out] data_size - Data size in bytes
 *  @param[out] data - Power mode data
 *  @return nsm_completion_codes
 */
int decode_get_power_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t *cc, uint16_t *reason_code,
			       uint16_t *data_size,
			       struct nsm_power_mode_data *data);

/** @brief Encode Set Power Mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] data - Power Mode data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_power_mode_req(uint8_t instance, struct nsm_msg *msg,
			      struct nsm_power_mode_data data);

/** @brief Decode a Set Power Mode request request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] data - Power Mode data
 *  @return nsm_completion_codes
 */
int decode_set_power_mode_req(const struct nsm_msg *msg, size_t msg_len,
			      struct nsm_power_mode_data *data);

/** @brief Encode Set Power Mode response message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[in] reason_code - Reason Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_power_mode_resp(uint8_t instance, uint16_t reason_code,
			       struct nsm_msg *req);

/** @brief Decode Set Power Mode response message
 *
 *  @param[in] resp    - response message
 *  @param[in] respLen - Length of response message
 *  @param[out] cc     - Completion Code
 *  @param[out] reason_code  - Reason Code
 *  @return nsm_completion_codes
 */
int decode_set_power_mode_resp(const struct nsm_msg *msg, size_t msgLen,
			       uint8_t *cc, uint16_t *reason_code);
#ifdef __cplusplus
}
#endif
#endif