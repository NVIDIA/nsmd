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

// Min size is when no counter is sent in response
#define PORT_COUNTER_TELEMETRY_MIN_DATA_SIZE 4
// Max size is when all 27 counters are supported
#define PORT_COUNTER_TELEMETRY_MAX_DATA_SIZE 260
#define FABRIC_MANAGER_STATE_DATA_SIZE 18
#define PORT_MASK_DATA_SIZE 32
// defined in MBps
#define MAXLINKBANDWIDTH 50000
#define ETH_PORT_TELEMETRY_COUNTER_ENABLED_COUNT 21
// defined MAC address length
#define MAC_ADDRESS_LENGTH 8

/** @brief NSM Type1 network port telemetry commands
 */
enum nsm_network_port_commands {
	NSM_GET_PORT_TELEMETRY_COUNTER = 0x01,
	NSM_GET_PORT_HEALTH_THRESHOLDS = 0x02,
#ifdef ENABLE_SYSTEM_GUID
	NSM_SET_SYSTEM_GUID = 0x03,
	NSM_GET_SYSTEM_GUID = 0x04,
#endif
	NSM_SET_LINK_DISABLE_STICKY = 0x05,
	NSM_GET_LINK_DISABLE_STICKY = 0x06,
	NSM_SET_PORT_HEALTH_THRESHOLDS = 0x07,
	NSM_SET_SWITCH_ISOLATION_MODE = 0x08,
	NSM_GET_SWITCH_ISOLATION_MODE = 0x09,
	NSM_SET_POWER_MODE = 0x0a,
	NSM_GET_POWER_MODE = 0x0b,
	NSM_SET_POWER_PROFILE = 0x0c,
	NSM_GET_POWER_PROFILE = 0x0d,
	NSM_GET_FABRIC_MANAGER_STATE = 0x0e,
	NSM_GET_ETH_PORT_TELEMETRY_COUNTER = 0x0f,
	NSM_GET_PORT_ECC_COUNTERS = 0x10,
	NSM_GET_NETWORK_ADDRESSES = 0x11,
	NSM_QUERY_PORTS_AVAILABLE = 0x41,
	NSM_QUERY_PORT_CHARACTERISTICS = 0x42,
	NSM_QUERY_PORT_STATUS = 0x43,
	NSM_SET_PORT_DISABLE_FUTURE = 0x44,
	NSM_GET_PORT_DISABLE_FUTURE = 0x45,
	NSM_GET_NVLINK_LED_STATUS = 0x60
};

/** @brief NVLink LED status
 */
enum nsm_nvlink_led_state {
	NSM_NVLINK_LED_OFF = 0x01,
	NSM_NVLINK_LED_GREEN = 0x02,
	NSM_NVLINK_LED_GREEN_BLINK = 0x03,
	NSM_NVLINK_LED_AMBER = 0x04,
	NSM_NVLINK_LED_AMBER_BLINK = 0x05,
	NSM_NVLINK_LED_ERROR = 0x06
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
	NSM_PORTSTATE_TRAINING_FAILURE = 0x08,
	NSM_PORTSTATE_TRAINING_FAILURE_LOCKED = 0x09,
	NSM_PORTSTATE_PHYSICAL_UP = 0x0A
};

/** @brief Port status
 */
enum nsm_port_status {
	NSM_PORTSTATUS_DISABLED = 0x01,
	NSM_PORTSTATUS_ENABLED = 0x02
};

/** @brief NSM Type 1 network ports events
 */
enum nsm_network_ports_events {
	NSM_THRESHOLD_EVENT = 0x00,
	NSM_FABRIC_MANAGER_STATE_EVENT = 0x01,
};

/** @brief FM State
 */
enum nsm_fabric_manager_state {
	NSM_FM_STATE_RESERVED = 0x00,
	NSM_FM_STATE_OFFLINE = 0x01,
	NSM_FM_STATE_STANDBY = 0x02,
	NSM_FM_STATE_CONFIGURED = 0x03,
	NSM_FM_STATE_RESERVED_TIMEOUT = 0x04,
	NSM_FM_STATE_ERROR = 0x05
};

/** @brief FM Report Status
 */
enum nsm_fm_report_status {
	NSM_FM_REPORT_STATUS_RESERVED = 0x00,
	NSM_FM_REPORT_STATUS_NOT_RECEIVED = 0x01,
	NSM_FM_REPORT_STATUS_RECEIVED = 0x02,
	NSM_FM_REPORT_STATUS_TIMEOUT = 0x03
};

enum port_down_reason_code {
	NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN = 0x00,
	NSM_PORT_DOWN_REASON_CODE_UNKNOWN = 0x01,
	NSM_PORT_DOWN_REASON_CODE_HI_SER_BER = 0x02,
	NSM_PORT_DOWN_REASON_CODE_BLOCK_LOCK_LOSS = 0x03,
	NSM_PORT_DOWN_REASON_CODE_ALIGNMENT_LOSS = 0x04,
	NSM_PORT_DOWN_REASON_CODE_FEC_SYNC_LOSS = 0x05,
	NSM_PORT_DOWN_REASON_CODE_PLL_LOCK_LOSS = 0x06,
	NSM_PORT_DOWN_REASON_CODE_FIFO_OVERFLOW = 0x07,
	NSM_PORT_DOWN_REASON_CODE_FALSE_SKIP_CONDITION = 0x08,
	NSM_PORT_DOWN_REASON_CODE_MINOR_ERR_THRESHOLD = 0x09,
	NSM_PORT_DOWN_REASON_CODE_PHY_LAYER_RETRANSMIT_TIMEOUT = 0x0A,
	NSM_PORT_DOWN_REASON_CODE_HEARTBEAT_ERRORS = 0x0B,
	NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_CREDIT_MON_WD = 0x0C,
	NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_INTEGRITY_THRESHOLD = 0x0D,
	NSM_PORT_DOWN_REASON_CODE_LINK_LAYER_BUFFER_OVERRUN = 0x0E,
	NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HEALTHY = 0x0F,
	NSM_PORT_DOWN_REASON_CODE_OOB_CMD_LINK_HI_BER = 0x10,
	NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HEALTHY = 0x11,
	NSM_PORT_DOWN_REASON_CODE_INBAND_CMD_LINK_HI_BER = 0x12,
	NSM_PORT_DOWN_REASON_CODE_DOWN_BY_VERIFICATION_GW = 0x13,
	NSM_PORT_DOWN_REASON_CODE_RECEIVED_REMOTE_FAULT = 0x14,
	NSM_PORT_DOWN_REASON_CODE_RECEIEVED_TS1 = 0x15,
	NSM_PORT_DOWN_REASON_CODE_DOWN_BY_MGMT_CMD = 0x16,
	NSM_PORT_DOWN_REASON_CODE_CABLE_UNPLUGGED = 0x17,
	NSM_PORT_DOWN_REASON_CODE_CABLE_ACCESS_ISSUES = 0x18,
	NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN = 0x19,
	NSM_PORT_DOWN_REASON_CODE_CURRENT_ISSUE = 0x1A,
	NSM_PORT_DOWN_REASON_CODE_POWER_BUDGET = 0x1B,
	NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_RAW_BER = 0x1C,
	NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_EFFECTIVE_BER = 0x1D,
	NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_SYMBOL_BER = 0x1E,
	NSM_PORT_DOWN_REASON_CODE_FAST_RECOVERY_CREDIT_WATCHDOG = 0x1F,
	NSM_PORT_DOWN_REASON_CODE_PEER_SLEEP = 0x20,
	NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE = 0x21,
	NSM_PORT_DOWN_REASON_CODE_PEER_DISABLE_LOCK = 0x22,
	NSM_PORT_DOWN_REASON_CODE_PEER_THERMAL_EVENT = 0x23,
	NSM_PORT_DOWN_REASON_CODE_PEER_FORCE_EVENT = 0x24,
	NSM_PORT_DOWN_REASON_CODE_PEER_RESET_EVENT = 0x25
};

enum nsm_network_address_property_tag {
	NSM_TAG_LINK_TYPE = 0,
	NSM_TAG_MAC_ADDRESS = 1,
	NSM_TAG_PERMANENT_MAC_ADDRESS = 2,
	NSM_TAG_NODE_GUID = 3,
	NSM_TAG_PORT_GUID = 4
};

enum nsm_network_address_link_type {
	NSM_PORT_PROTOCOL_UNKNOWN = -1,
	NSM_PORT_PROTOCOL_ETHERNET = 0,
	NSM_PORT_PROTOCOL_INFINIBAND = 1
};

enum nsm_ecc_counters_tag {
	NSM_TAG_ECC_RX_SYMBOL_ERRORS_BYTES = 0,
	NSM_TAG_ECC_CORRECTED_BITS = 1,
	NSM_TAG_ECC_RAW_ERRORS_LANE_0 = 2,
	NSM_TAG_ECC_RAW_ERRORS_LANE_1 = 3,
	NSM_TAG_ECC_RAW_ERRORS_LANE_2 = 4,
	NSM_TAG_ECC_RAW_ERRORS_LANE_3 = 5
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
	uint8_t symbol_ber : 1;
	uint8_t link_error_recovery_counter : 1;
	uint8_t link_downed_counter : 1;
	uint8_t port_rcv_remote_physical_errors : 1;
	uint8_t port_rcv_switch_relay_errors : 1;
	uint8_t QP1_dropped : 1;

	uint8_t xmit_wait : 1;
	uint8_t effective_ber : 1;
	uint8_t total_raw_error : 1;
	uint8_t effective_error : 1;
	uint8_t symbol_error : 1;
	uint8_t total_raw_ber : 1;
	uint8_t unintentional_link_down_count : 1;
	uint8_t intentional_link_down_count : 1;
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
	uint64_t symbol_ber;
	uint64_t link_error_recovery_counter;
	uint64_t link_downed_counter;
	uint64_t port_rcv_remote_physical_errors;
	uint64_t port_rcv_switch_relay_errors;
	uint64_t QP1_dropped;
	uint64_t xmit_wait;
	uint64_t effective_ber;
	uint64_t total_raw_error;
	uint64_t effective_error;
	uint64_t symbol_error;
	uint64_t total_raw_ber;
	uint64_t unintentional_link_down_count;
	uint64_t intentional_link_down_count;
} __attribute__((packed));

enum ethernet_port_counter_data_tag {
	ETHERNET_PORT_COUNTER_TAG_RX_BYTES = 0,
	ETHERNET_PORT_COUNTER_TAG_TX_BYTES = 1,
	ETHERNET_PORT_COUNTER_TAG_RX_UNICAST_BYTES = 2,
	ETHERNET_PORT_COUNTER_TAG_RX_MULTICAST_BYTES = 3,
	ETHERNET_PORT_COUNTER_TAG_RX_BROADCAST_BYTES = 4,
	ETHERNET_PORT_COUNTER_TAG_TX_UNICAST_BYTES = 5,
	ETHERNET_PORT_COUNTER_TAG_TX_MULTICAST_BYTES = 6,
	ETHERNET_PORT_COUNTER_TAG_TX_BROADCAST_BYTES = 7,
	ETHERNET_PORT_COUNTER_TAG_RX_FCS_ERRORS = 8,
	ETHERNET_PORT_COUNTER_TAG_RX_ALIGNMENT_ERRORS = 9,
	ETHERNET_PORT_COUNTER_TAG_RX_FALSE_CARRIER_DETECTIONS = 10,
	ETHERNET_PORT_COUNTER_TAG_RX_RUNT_BYTES = 11,
	ETHERNET_PORT_COUNTER_TAG_RX_JABBER_BYTES = 12,
	ETHERNET_PORT_COUNTER_TAG_RX_XON_FRAMES = 13,
	ETHERNET_PORT_COUNTER_TAG_RX_XOFF_FRAMES = 14,
	ETHERNET_PORT_COUNTER_TAG_TX_XON_FRAMES = 15,
	ETHERNET_PORT_COUNTER_TAG_TX_XOFF_FRAMES = 16,
	ETHERNET_PORT_COUNTER_TAG_RX_SINGLE_COLLISION_FRAMES = 17,
	ETHERNET_PORT_COUNTER_TAG_RX_MULTIPLE_COLLISION_FRAMES = 18,
	ETHERNET_PORT_COUNTER_TAG_RX_LATE_COLLISION_FRAMES = 19,
	ETHERNET_PORT_COUNTER_TAG_RX_EXCESSIVE_COLLISION_FRAMES = 20,
};

typedef union {
	uint64_t ethernet_port_counter_data_64bit;
	uint32_t ethernet_port_counter_data_32bit;
} nsm_ethernet_port_counter_data;

/** @struct nsm_get_port_telemetry_counter_req
 *
 *  Structure representing NSM get port telemetry counter request.
 */
struct nsm_get_ethernet_port_telemetry_counter_req {
	struct nsm_common_req hdr;
	uint16_t port_number;
} __attribute__((packed));

struct status {
	uint32_t link_state : 3;
	uint32_t sub_link_state : 5;
	uint32_t rx_detect_state : 2;
	uint32_t port_down_reason_code : 8;
	uint32_t unused : 14;
} __attribute__((packed));

struct nsm_port_characteristics_data {
	struct status port_status;
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

struct nsm_fabric_manager_state_data {
	uint8_t fm_state;
	uint8_t report_status;
	uint64_t last_restart_timestamp;
	uint64_t duration_since_last_restart_sec;
} __attribute__((packed));

#ifdef ENABLE_SYSTEM_GUID
/** @struct nsm_set_system_guid_req
 *
 *  Structure representing NSM set system guid request.
 */
struct nsm_set_system_guid_req {
	struct nsm_common_req hdr;
	uint8_t SysGUID_0;
	uint8_t SysGUID_1;
	uint8_t SysGUID_2;
	uint8_t SysGUID_3;
	uint8_t SysGUID_4;
	uint8_t SysGUID_5;
	uint8_t SysGUID_6;
	uint8_t SysGUID_7;
} __attribute__((packed));

/** @struct nsm_set_system_guid_resp
 *
 *  Structure representing NSM set system guid response.
 */
struct nsm_set_system_guid_resp {
	struct nsm_common_resp hdr;
} __attribute__((packed));

/** @struct nsm_get_system_guid_req
 *
 *  Structure representing NSM get system guid request.
 */
struct nsm_get_system_guid_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_get_system_guid_resp
 *
 *  Structure representing NSM get system guid response.
 */
struct nsm_get_system_guid_resp {
	struct nsm_common_resp hdr;
	uint8_t SysGUID_0;
	uint8_t SysGUID_1;
	uint8_t SysGUID_2;
	uint8_t SysGUID_3;
	uint8_t SysGUID_4;
	uint8_t SysGUID_5;
	uint8_t SysGUID_6;
	uint8_t SysGUID_7;
} __attribute__((packed));
#endif

/** @struct nsm_get_nvlink_agg_led_status_req
 *
 *  Structure representing NSM get system guid request.
 */
struct nsm_get_nvlink_agg_led_status_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_get_nvlink_agg_led_status_resp
 *
 *  Structure representing NSM get system guid response.
 */
struct nsm_get_nvlink_agg_led_status_resp {
	struct nsm_common_resp hdr;
	uint8_t LED_State_Aggregate;
	uint8_t LED_State_GPU_0;
	uint8_t LED_State_GPU_1;
	uint8_t LED_State_GPU_2;
	uint8_t LED_State_GPU_3;
	uint8_t LED_State_GPU_4;
	uint8_t LED_State_GPU_5;
	uint8_t LED_State_GPU_6;
	uint8_t LED_State_GPU_7;
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
	uint8_t data[1];
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

/** @struct nsm_get_switch_isolation_mode_resp
 *
 *  Structure representing NSM Get Switch Isolation Mode response.
 */
struct nsm_get_switch_isolation_mode_resp {
	struct nsm_common_resp hdr;
	uint8_t isolation_mode;
} __attribute__((packed));

enum nsm_switch_isolation_mode {
	SWITCH_COMMUNICATION_MODE_ENABLED = 0,
	SWITCH_COMMUNICATION_MODE_DISABLED = 1
};
/** @struct nsm_set_switch_isolation_mode_req
 *
 *  Structure representing NSM Set Switch Isolation Mode response.
 */
struct nsm_set_switch_isolation_mode_req {
	struct nsm_common_req hdr;
	uint8_t isolation_mode;
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

/** @struct nsm_health_event_payload
 *
 *  Structure representing NSM Ports health event payload.
 */
struct nsm_health_event_payload {
	uint8_t portNumber;
	uint32_t reserved1 : 24;
	uint8_t port_rcv_errors_threshold : 1;
	uint8_t port_xmit_discard_threshold : 1;
	uint8_t symbol_ber_threshold : 1;
	uint8_t port_rcv_remote_physical_errors_threshold : 1;
	uint8_t port_rcv_switch_relay_errors_threshold : 1;
	uint8_t effective_ber_threshold : 1;
	uint8_t estimated_effective_ber_threshold : 1;
	uint32_t reserved2 : 25;
} __attribute__((packed));

/** @struct nsm_get_fabric_manager_state_req
 *
 *  Structure representing NSM get fabric manager state request.
 */
typedef struct nsm_common_req nsm_get_fabric_manager_state_req;

/** @struct nsm_get_fabric_manager_state_resp
 *
 *  Structure representing NSM get fabric manager state response.
 */
struct nsm_get_fabric_manager_state_resp {
	struct nsm_common_resp hdr;
	struct nsm_fabric_manager_state_data data;
} __attribute__((packed));

/** @struct nsm_get_fabric_manager_state_event_payload
 *
 *  Structure representing payload of NSM get fabric manager state event
 */
typedef struct nsm_fabric_manager_state_data
    nsm_get_fabric_manager_state_event_payload;

/** @struct nsm_get_network_addresses_req
 *
 *  Structure representing NSM get port network addresses request.
 */
struct nsm_get_network_addresses_req {
	struct nsm_common_req hdr;
	uint16_t port_number;
} __attribute__((packed));

/** @struct network_address_sample_data
 *
 *  Union representing get network addresses response sample data.
 */
typedef union {
	uint8_t mac_address[MAC_ADDRESS_LENGTH];
	uint64_t network_identifier_64bit;
	uint8_t link_type;
} network_address_sample_data;

/** @brief Encode Get Network Addresses request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - Port number
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_network_addresses_req(uint8_t instance_id, uint16_t port_number,
				     struct nsm_msg *msg);

/** @brief Decode Get Network Addresses request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - port number
 *  @return nsm_completion_codes
 */
int decode_get_network_addresses_req(const struct nsm_msg *msg, size_t msg_len,
				     uint16_t *port_number);

/** @brief Decode a Get Network Addresses request message
 *
 *  @param[out] tag - pointer to telemetry sample tag
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @param[out] address - pointer to network address sample data
 *  @return nsm_completion_codes
 */
int decode_aggregate_network_address_data(uint8_t tag, const uint8_t *data,
					  size_t data_len,
					  network_address_sample_data *address);

/** @brief Encode a Aggregate Network Address Data request message
 *
 *  @param[in] tag - telemetry sample tag
 *  @param[in] address - pointer to network address sample data
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 */
int encode_aggregate_network_address_data(
    uint8_t tag, const network_address_sample_data *address, uint8_t *data,
    size_t *data_len);

/** @struct nsm_get_port_ecc_counters_req
 *
 *  Structure representing NSM get port ecc counters request.
 */
typedef struct nsm_common_port_req nsm_get_port_ecc_counters_req;

#ifdef ENABLE_SYSTEM_GUID
/** @brief Encode a Set System GUID request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @param[out] sysGuid - System Guid (8 bytes)
 *  @param[out] sysGuid_len - System Guid buffer length
 *  @return nsm_completion_codes
 */
int encode_set_system_guid_req(uint8_t instance_id, struct nsm_msg *msg,
			       uint8_t *sys_guid, size_t sys_guid_len);

/** @brief Decode a Set System GUID response message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_set_system_guid_resp(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a Get System GUID request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int encode_get_system_guid_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a Get System GUID response message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[in] cc - response message completion code
 *  @param[in] reason_code - reason code
 *  @param[out] sysGuid - System Guid will be written to this (8 bytes)
 *  @param[out] sysGuid_len - System Guid buffer length
 *  @return nsm_completion_codes
 */
int decode_get_system_guid_resp(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *cc, uint16_t *reason_code,
				uint8_t *sys_guid, size_t sys_guid_len);

#endif

/** @brief Encode a Get NVLink LED status request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */

int encode_get_nvlink_agg_led_status_req(uint8_t instance_id,
					 struct nsm_msg *msg);

/** @brief Decode a Get NVLink LED status response message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] nvlink_agg_led_status - NVLink LED status byte will be written
 * to this
 *  @return nsm_completion_codes
 */
int decode_get_nvlink_agg_led_status_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, enum nsm_nvlink_led_state *nvlink_agg_led_status);

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

/** @brief Encode a ports health event message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] ackr - acknowledgement request
 *  @param[in] payload - Pointer to health event payload
 *  @param[out] msg - Pointer to encoded message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_health_event(uint8_t instance_id, bool ackr,
			    const struct nsm_health_event_payload *payload,
			    struct nsm_msg *msg);

/** @brief Decode a ports health event message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] event_state - Pointer to decoded event state
 *  @param[out] payload - Pointer do decoded health event payload
 *  @return nsm_completion_codes
 */
int decode_nsm_health_event(const struct nsm_msg *msg, size_t msg_len,
			    uint16_t *event_state,
			    struct nsm_health_event_payload *payload);
/** @brief Encode Get Switch Isolation request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_switch_isolation_mode_req(uint8_t instance, struct nsm_msg *msg);

/** @brief Decode a Get Switch Isolation Mode request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_switch_isolation_mode_req(const struct nsm_msg *msg,
					 size_t msg_len);

/** @brief Encode Get Switch Isolation Mode response message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[in] reason_code - Reason Code
 *  @param[in] isolation_mode - isolation mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_switch_isolation_mode_resp(uint8_t instance, uint8_t cc,
					  uint16_t reason_code,
					  uint8_t isolation_mode,
					  struct nsm_msg *msg);

/** @brief Decode Get Switch Isolation Mode response message
 *
 *  @param[in] resp    - response message
 *  @param[in] respLen - Length of response message
 *  @param[out] cc     - Completion Code
 *  @param[out] reason_code  - Reason Code
 *  @param[out] isolation_mode - Isolation Mode
 *  @return nsm_completion_codes
 */
int decode_get_switch_isolation_mode_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code,
					  uint8_t *isolation_mode);

/** @brief Encode Set Switch Isolation request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] isolation_mode - Isolation Mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_switch_isolation_mode_req(uint8_t instance,
					 uint8_t isolation_mode,
					 struct nsm_msg *msg);

/** @brief Decode a Set Switch Isolation Mode request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] isolation_mode - Isolation Mode
 *  @return nsm_completion_codes
 */
int decode_set_switch_isolation_mode_req(const struct nsm_msg *msg,
					 size_t msg_len,
					 uint8_t *isolation_mode);
/** @brief Encode Set Switch Isolation Mode response message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[in] reason_code - Reason Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_switch_isolation_mode_resp(uint8_t instance, uint8_t cc,
					  uint16_t reason_code,
					  struct nsm_msg *msg);

/** @brief Decode Set Switch Isolation Mode response message
 *
 *  @param[in] resp    - response message
 *  @param[in] respLen - Length of response message
 *  @param[out] cc     - Completion Code
 *  @param[out] reason_code  - Reason Code
 *  @return nsm_completion_codes
 */
int decode_set_switch_isolation_mode_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code);

/** @brief Encode a get fabric manager state request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_fabric_manager_state_req(uint8_t instance_id,
					struct nsm_msg *msg);

/** @brief Decode a get fabric manager state request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_fabric_manager_state_req(const struct nsm_msg *msg,
					size_t msg_len);

/** @brief Encode a get fabric manager state response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - Response message completion code
 *  @param[in] reason_code - Reason code
 *  @param[in] data - Fabric manager state data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_fabric_manager_state_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_fabric_manager_state_data *data, struct nsm_msg *msg);

/** @brief Decode a get fabric manager state response message
 *
 *  @param[in] msg    - Response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - Pointer to response message completion code
 *  @param[out] reason_code     - Pointer to reason code
 *  @param[out] data_size - Data size in bytes
 *  @param[out] data - Fabric manager state data
 *  @return nsm_completion_codes
 */
int decode_get_fabric_manager_state_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *data_size,
    struct nsm_fabric_manager_state_data *data);

/** @brief Create a get fabric manager state event message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] ackr - acknowledgement request
 *  @param[in] payload - fabric manager state event payload
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_get_fabric_manager_state_event(
    uint8_t instance_id, bool ackr,
    nsm_get_fabric_manager_state_event_payload payload, struct nsm_msg *msg);

/** @brief Decode a get fabric manager state event message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] event_class - event class
 *  @param[out] event_state - event state
 *  @param[out] payload - fabric manager state event payload
 *  @return nsm_completion_codes
 */
int decode_nsm_get_fabric_manager_state_event(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *event_class,
    uint16_t *event_state, nsm_get_fabric_manager_state_event_payload *payload);

/** @brief Encode a get Ethernet port telemetry counter request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - Port number
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_eth_port_telemetry_counter_req(uint8_t instance_id,
					      uint16_t port_number,
					      struct nsm_msg *msg);

/** @brief Decode a get Ethernet port telemetry counter request message
 *
 *  @param[in] msg - Request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - Port number
 *  @return nsm_completion_codes
 */
int decode_get_eth_port_telemetry_counter_req(const struct nsm_msg *msg,
					      size_t msg_len,
					      uint16_t *port_number);

/** @brief Decode aggregate Ethernet port telemetry data
 *
 *  @param[in] data - Data buffer
 *  @param[in] data_len - Length of data buffer
 *  @param[out] counter_reading - Decoded counter_reading
 *  @return nsm_completion_codes
 */
int decode_aggregate_eth_port_telemetry_data(
    const uint8_t *data, size_t *data_len, uint8_t tag,
    nsm_ethernet_port_counter_data *counter_reading);

/** @brief Encode aggregate Ethernet port telemetry data
 *
 *  @param[in] counter_reading - counter_reading to encode
 *  @param[out] data - Data buffer
 *  @param[out] data_len - Length of data buffer
 *  @return nsm_completion_codes
 */
int encode_aggregate_eth_port_telemetry_data(
    uint8_t tag, nsm_ethernet_port_counter_data *counter_reading, uint8_t *data,
    size_t *data_len);

/** @brief Encode a Get Port ECC Counters request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - Port number
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_port_ecc_counters_req(uint8_t instance_id, uint8_t port_number,
				     struct nsm_msg *msg);

/** @brief Decode a Get Port ECC Counters request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - port number
 *  @return nsm_completion_codes
 */
int decode_get_port_ecc_counters_req(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *port_number);

/** @brief Decode a aggregate port ecc counter data
 *
 *  @param[in] tag - tag
 *  @param[in] data - data
 *  @param[in] data_len - data length
 *  @param[out] counter_value - counter value
 *  @return nsm_completion_codes
 */
int decode_aggregate_port_ecc_counter_data(uint8_t tag, const uint8_t *data,
					   size_t data_len,
					   uint64_t *counter_value);

/** @brief Encode a aggregate port ecc counter data
 *
 *  @param[in] tag - tag
 *  @param[in] counter_value - counter value
 *  @param[out] data - data
 *  @param[out] data_len - data length
 *  @return nsm_completion_codes
 */
int encode_aggregate_port_ecc_counter_data(uint8_t tag, uint64_t counter_value,
					   uint8_t *data, size_t *data_len);

#ifdef __cplusplus
}
#endif
#endif
