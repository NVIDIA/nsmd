#ifndef PCI_LINKS_H
#define PCI_LINKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

// GPU device_index 0 [as per spec]
#define GPU_DEVICE_INDEX 0
// Retimer device_index 1-8 [as per spec]
#define PCIE_RETIMER_DEVICE_INDEX_START 1

#define GROUP_ID_0 0
#define GROUP_ID_1 1
#define GROUP_ID_2 2
#define GROUP_ID_3 3
#define GROUP_ID_4 4
#define GROUP_ID_5 5
#define GROUP_ID_6 6
#define GROUP_ID_7 7
#define GROUP_ID_8 8
#define GROUP_ID_9 9
#define GROUP_ID_10 10
#define TOTAL_PCIE_LANE_COUNT 16

#define DS_ID_0 0
#define DS_ID_1 1
#define DS_ID_2 2
#define DS_ID_3 3
#define DS_ID_4 4
#define DS_ID_5 5
#define DS_ID_6 6
#define DS_ID_7 7
#define DS_ID_8 8
#define DS_ID_9 9
#define DS_ID_10 10
#define DS_ID_11 11
#define DS_ID_12 12
#define DS_ID_13 13

#define MAX_SUPPORTED_DATA_MASK_LENGTH 1
#define BYTES_PER_DWORD 4

enum pci_links_command {
	NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES = 0x02,
	NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1 = 0x04,
	NSM_CLEAR_DATA_SOURCE_V1 = 0x05,
	NSM_LIST_AVAILABLE_PCIE_PORTS = 0x07,
	NSM_GET_PORT_CONFIGURATION = 0x08,
	NSM_SET_PORT_CONFIGURATION = 0x09,
	NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2 = 0x24,
	NSM_QUERY_VECTOR_DATA_SOURCES_V2 = 0x26,
	NSM_ASSERT_PCIE_FUNDAMENTAL_RESET = 0x60,
};

/** @struct nsm_query_scalar_group_telemetry_v1_req
 *
 *  Structure representing Query Scalar Group Telemetry v1 request.
 */
struct nsm_query_scalar_group_telemetry_v1_req {
	struct nsm_common_req hdr;
	uint8_t device_id;
	uint8_t group_index;
} __attribute__((packed));

struct nsm_query_scalar_group_telemetry_v1_resp {
	struct nsm_common_resp hdr;
	uint8_t data[1];
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_0
 *
 *  Structure representing Scalar Group Telemetry Data for Group 0.
 */
struct nsm_query_scalar_group_telemetry_group_0 {
	uint32_t pci_vendor_id;
	uint32_t pci_device_id;
	uint32_t pci_subsystem_vendor_id;
	uint32_t pci_subsystem_device_id;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_0_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 0.
 */
struct nsm_query_scalar_group_telemetry_v1_group_0_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_0 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_1
 *
 *  Structure representing Scalar Group Telemetry Data for Group 1.
 */
struct nsm_query_scalar_group_telemetry_group_1 {
	uint32_t negotiated_link_speed;	      // dsid 0
	uint32_t negotiated_link_width;	      // dsid 1
	uint32_t target_link_speed;	      // dsid 2
	uint32_t max_link_speed;	      // dsid 3
	uint32_t max_link_width;	      // dsid 4
	uint32_t max_read_request_size_bytes; // dsid 5
	uint32_t max_payload_size_bytes;      // dsid 6
	uint32_t clock_mode;		      // dsid 7
} __attribute__((packed));

/** @enum nsm_pcie_clock_mode
 *
 *  Enum representing PCIe clock mode configuration.
 */
enum nsm_pcie_clock_mode {
	NSM_PCIE_CLOCK_MODE_SEPARATE = 0,
	NSM_PCIE_CLOCK_MODE_COMMON = 1,
};

/** @enum nsm_pcie_link_speed_code
 *
 *  Enum representing PCIe link speed codes (selector_0 values for vector
 * telemetry).
 */
enum nsm_pcie_link_speed_code {
	NSM_PCIE_LINK_SPEED_CODE_UNKNOWN = 0,
	NSM_PCIE_LINK_SPEED_CODE_GEN1 = 1,
	NSM_PCIE_LINK_SPEED_CODE_GEN2 = 2,
	NSM_PCIE_LINK_SPEED_CODE_GEN3 = 3,
	NSM_PCIE_LINK_SPEED_CODE_GEN4 = 4,
	NSM_PCIE_LINK_SPEED_CODE_GEN5 = 5,
	NSM_PCIE_LINK_SPEED_CODE_GEN6 = 6,
	NSM_PCIE_LINK_SPEED_CODE_MAX = 6,
};

/** @brief PCIe lane count constants */
#define NSM_PCIE_LANE_COUNT_INVALID 0
#define NSM_PCIE_LANE_COUNT_MAX 32

/** @brief PCIe link speed in Gbps */
#define NSM_PCIE_SPEED_GBPS_GEN1 2.5
#define NSM_PCIE_SPEED_GBPS_GEN2 5.0
#define NSM_PCIE_SPEED_GBPS_GEN3 8.0
#define NSM_PCIE_SPEED_GBPS_GEN4 16.0
#define NSM_PCIE_SPEED_GBPS_GEN5 32.0
#define NSM_PCIE_SPEED_GBPS_GEN6 64.0

/** @enum nsm_pcie_port_type
 *
 *  Enum representing PCIe port types
 */
enum nsm_pcie_port_type {
	NSM_PCIE_PORT_TYPE_ENDPOINT = 0,
	NSM_PCIE_PORT_TYPE_ROOT_PORT = 4,
	NSM_PCIE_PORT_TYPE_UPSTREAM = 5,
	NSM_PCIE_PORT_TYPE_DOWNSTREAM = 6,
};

/** @struct nsm_query_scalar_group_telemetry_v1_group_1_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 1.
 */
struct nsm_query_scalar_group_telemetry_v1_group_1_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_1 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_2
 *
 *  Structure representing Scalar Group Telemetry Data for Group 2.
 */
struct nsm_query_scalar_group_telemetry_group_2 {
	uint32_t non_fatal_errors;	    // dsid 0
	uint32_t fatal_errors;		    // dsid 1
	uint32_t unsupported_request_count; // dsid 2
	uint32_t correctable_errors;	    // dsid 3
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_2_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 2.
 */
struct nsm_query_scalar_group_telemetry_v1_group_2_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_2 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_3
 *
 *  Structure representing Scalar Group Telemetry Data for Group 3.
 */
struct nsm_query_scalar_group_telemetry_group_3 {
	uint32_t L0ToRecoveryCount;   // dsid 0
	uint32_t eieos_timeout;	      // dsid 1
	uint32_t training_seq_errors; // dsid 2
	uint32_t framing_errors;      // dsid 3
	uint32_t link_down_count;     // dsid 4
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_3_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 3.
 */
struct nsm_query_scalar_group_telemetry_v1_group_3_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_3 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_4
 *
 *  Structure representing Scalar Group Telemetry Data for Group 4.
 */
struct nsm_query_scalar_group_telemetry_group_4 {
	uint32_t recv_err_cnt;	      // dsid 0
	uint32_t NAK_recv_cnt;	      // dsid 1
	uint32_t NAK_sent_cnt;	      // dsid 2
	uint32_t bad_TLP_cnt;	      // dsid 3
	uint32_t replay_rollover_cnt; // dsid 4
	uint32_t FC_timeout_err_cnt;  // dsid 5
	uint32_t replay_cnt;	      // dsid 6
	uint32_t dllp_crc_errors;     // dsid 7
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_4_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 4.
 */
struct nsm_query_scalar_group_telemetry_v1_group_4_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_4 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_5
 *
 *  Structure representing Scalar Group Telemetry Data for Group 5.
 */
struct nsm_query_scalar_group_telemetry_group_5 {
	uint32_t PCIeTXDwords;
	uint32_t PCIeRXDwords;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_5_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 5.
 */
struct nsm_query_scalar_group_telemetry_v1_group_5_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_5 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_6
 *
 *  Structure representing Scalar Group Telemetry Data for Group 6.
 */
struct nsm_query_scalar_group_telemetry_group_6 {
	uint32_t ltssm_state;
	uint32_t invalid_flit_counter;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_6_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 6.
 */
struct nsm_query_scalar_group_telemetry_v1_group_6_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_6 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_7
 *
 *  Structure representing Scalar Group Telemetry Data for Group 7.
 */
struct nsm_query_scalar_group_telemetry_group_7 {
	uint32_t bar0_base_addr_low;  // dsid 0
	uint32_t bar0_base_addr_high; // dsid 1
	uint32_t bar0_size;	      // dsid 2
	uint32_t port_type;	      // dsid 3
	uint32_t pcie_bus_number;     // dsid 4
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_7_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 7.
 */
struct nsm_query_scalar_group_telemetry_v1_group_7_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_7 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_8
 *
 *  Structure representing Scalar Group Telemetry Data for Group 8.
 */
struct nsm_query_scalar_group_telemetry_group_8 {
	uint32_t error_counts[TOTAL_PCIE_LANE_COUNT];
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_8_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 8.
 */
struct nsm_query_scalar_group_telemetry_v1_group_8_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_8 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_9
 *
 *  Structure representing Scalar Group Telemetry Data for Group 9.
 */
struct nsm_query_scalar_group_telemetry_group_9 {
	uint32_t aer_uncorrectable_error_status;
	uint32_t aer_correctable_error_status;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_9_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for group 9.
 */
struct nsm_query_scalar_group_telemetry_v1_group_9_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_9 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_10
 *
 *  Structure representing Scalar Group Telemetry Data for Group 10.
 */
struct nsm_query_scalar_group_telemetry_group_10 {
	uint32_t outbound_read_tlp_count;
	uint32_t dwords_transferred_in_outbound_read_tlp_high;
	uint32_t dwords_transferred_in_outbound_read_tlp_low;
	uint32_t outbound_write_tlp_count;
	uint32_t dwords_transferred_in_outbound_write_tlp_high;
	uint32_t dwords_transferred_in_outbound_write_tlp_low;
	uint32_t outbound_completion_tlp_count;
	uint32_t dwords_transferred_in_outbound_completion;
	uint32_t read_requests_dropped_tag_unavailable;
	uint32_t read_requests_dropped_credit_exhaustion;
	uint32_t read_requests_dropped_credit_not_posted;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_10_extended
 *
 *  Structure representing Scalar Group Telemetry Data for Group 10 Extended.
 */
struct nsm_query_scalar_group_telemetry_group_10_extended {
	uint32_t outbound_read_tlp_count;
	uint32_t dwords_transferred_in_outbound_read_tlp_high;
	uint32_t dwords_transferred_in_outbound_read_tlp_low;
	uint32_t outbound_write_tlp_count;
	uint32_t dwords_transferred_in_outbound_write_tlp_high;
	uint32_t dwords_transferred_in_outbound_write_tlp_low;
	uint32_t outbound_completion_tlp_count;
	uint32_t dwords_transferred_in_outbound_completion;
	uint32_t read_requests_dropped_tag_unavailable;
	uint32_t read_requests_dropped_credit_exhaustion;
	uint32_t read_requests_dropped_credit_not_posted;
	uint32_t inbound_completion_tlp_count;
	uint32_t inbound_completion_tlp_bytes_high;
	uint32_t inbound_completion_tlp_bytes_low;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_10_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for
 * group 10.
 */
struct nsm_query_scalar_group_telemetry_v1_group_10_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_10 data;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_v1_group_10_extended_resp
 *
 *  Structure representing Query Scalar Group Telemetry v1 response for
 *  group 10 extended.
 */
struct nsm_query_scalar_group_telemetry_v1_group_10_extended_resp {
	struct nsm_common_resp hdr;
	struct nsm_query_scalar_group_telemetry_group_10_extended data;
} __attribute__((packed));

/** @struct nsm_pcie_upstream_port_info
 *
 *  Structure representing Upstream Port Info.
 */
struct nsm_pcie_upstream_port_info {
	uint8_t type; // 0 - external, 1 - internal
	uint8_t downstream_ports_count;
} __attribute__((packed));

/** @struct nsm_list_available_pcie_ports_info
 *
 *  Structure representing List Available PCIe Ports Port Info.
 */
struct nsm_list_available_pcie_ports_info {
	uint16_t ports_count;
	struct nsm_pcie_upstream_port_info ports[1];
} __attribute__((packed));

/** @struct nsm_list_available_pcie_ports_resp
 *
 *  Structure representing List Available PCIe Ports response.
 */
struct nsm_list_available_pcie_ports_resp {
	struct nsm_common_resp hdr;
	struct nsm_list_available_pcie_ports_info port_info;
} __attribute__((packed));

#define NSM_LIST_AVAILABLE_PCIE_PORTS_RESPONSE_MIN_LEN                         \
	(sizeof(struct nsm_msg_hdr) + NSM_RESPONSE_CONVENTION_LEN +            \
	 sizeof(uint16_t) + sizeof(struct nsm_pcie_upstream_port_info))

/** @enum nsm_port_type
 *
 *  Enum representing port type.
 */
enum nsm_port_type {
	NSM_PORT_TYPE_UPSTREAM = 0,
	NSM_PORT_TYPE_DOWNSTREAM = 1,
};

/** @struct nsm_multiport_query_scalar_group_telemetry_v2_req_data
 *
 *  Structure representing Multiport Query Scalar Group Telemetry v1 request
 * data.
 */
struct nsm_multiport_query_scalar_group_telemetry_v2_req_data {
	uint8_t upstream_port_index : 7; // number of upstream port
	uint8_t type : 1;    // 0 - upstream port, 1 - downstream port
	uint8_t index;	     // index of the upstream/downstream port
	uint8_t group_index; // group index
} __attribute__((packed));

/** @struct nsm_multiport_query_scalar_group_telemetry_v2_req
 *
 *  Structure representing Multiport Query Scalar Group Telemetry v1 request.
 */
struct nsm_multiport_query_scalar_group_telemetry_v2_req {
	struct nsm_common_req hdr;
	struct nsm_multiport_query_scalar_group_telemetry_v2_req_data data;
} __attribute__((packed));

/** @struct nsm_query_vector_group_telemetry_v2_req
 *
 *  Structure representing Query Vector Data Sources v2 request.
 */
struct nsm_query_vector_group_telemetry_v2_req {
	struct nsm_common_req hdr;
	uint8_t port_number : 7; // Bits 0:6 - Upstream port number
	uint8_t port_type : 1;	 // Bit 7 - Type (0-upstream, 1-downstream)
	uint8_t index;		 // Index
	uint8_t group_id;	 // Group ID
	uint8_t selector_0;	 // Selector 0 (e.g., Lane number)
	uint8_t selector_1;	 // Selector 1 (reserved for vector group 1)
} __attribute__((packed));

/** @struct nsm_query_vector_data_sources_v2_resp
 *
 *  Structure representing Query Vector Data Sources v2 response.
 */
struct nsm_query_vector_data_sources_v2_resp {
	struct nsm_common_resp hdr;
	uint32_t data_values[1]; // Variable length array of data values
} __attribute__((packed));

/** @struct nsm_query_vector_group_1_data
 *
 *  Structure representing Vector Group 1 data
 */
struct nsm_query_vector_group_1_data {
	uint32_t cdr_error_per_lane;
} __attribute__((packed));

/** @struct nsm_get_port_config_aggregate_resp
 *
 *  Structure representing NSM aggregator variant response.
 */
struct nsm_get_port_config_aggregate_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t telemetry_count;
} __attribute__((packed));

/** @struct nsm_set_port_config_aggregate_req_sample
 *
 *  Structure representing NSM telemetry sample of aggregator variant request.
 */
struct nsm_set_port_config_aggregate_req_sample {
	uint8_t tag;
	uint8_t valid : 1;
	uint8_t length : 3;
	uint8_t reserved : 4;
	uint8_t data[1];
} __attribute__((packed));

/** @struct nsm_set_port_config_aggregate_req
 *
 *  Structure representing Set PCIe Port Config aggregate request.
 */
struct nsm_set_port_config_aggregate_req {
	struct nsm_common_req hdr;
	uint8_t port_number : 7; // number of port
	uint8_t port_type : 1;	 // 0 - upstream, 1 - downstream
	uint8_t port_index;	 // index of the port
	uint16_t sample_count;
	uint8_t sample_data[1];
} __attribute__((packed));

/** @struct nsm_get_port_config_req
 *
 *  Structure representing Get PCIe Port Config request.
 */
struct nsm_get_port_config_req {
	struct nsm_common_req hdr;
	uint8_t port_number : 7; // number of port
	uint8_t port_type : 1;	 // 0 - upstream, 1 - downstream
	uint8_t port_index;	 // index of the port
} __attribute__((packed));

/** @brief Encode a Get PCIe Port Config request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - port number
 *  @param[in] port_type - port type
 *  @param[in] port_index - port index
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_pcie_port_config_req(uint8_t instance_id, uint8_t port_number,
				    uint8_t port_type, uint8_t port_index,
				    struct nsm_msg *msg);

/** @brief Decode a Get PCIe Port Config response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[in] port_number - port number
 *  @param[in] port_type - port type
 *  @param[in] port_index - port index
 *  @return nsm_completion_codes
 */
int decode_get_pcie_port_config_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *port_number, uint8_t *port_type,
				    uint8_t *port_index);

/** @brief Encode a Get PCIe Port Config response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_port_config_aggregate_resp(uint8_t instance_id, uint8_t command,
					  uint8_t cc, uint16_t telemetry_count,
					  struct nsm_msg *msg);

/** @brief Encode a preset PCIe data
 *
 *  @param[in] preset - preset
 *  @param[out] data - data
 *  @param[out] data_len - length of data
 *  @return nsm_completion_codes
 */
int encode_preset_PCIe_data(uint8_t preset, uint8_t *data, size_t *data_len);

/** @brief Decode a preset PCIe data
 *
 *  @param[in] data - data
 *  @param[in] data_len - length of data
 *  @param[out] preset_0 - preset 0
 *  @param[out] preset_1 - preset 1
 *  @return nsm_completion_codes
 */
int decode_preset_PCIe_data(const uint8_t *data, size_t data_len,
			    uint8_t *preset_0, uint8_t *preset_1);

/** @brief Decode a PCIe Tx Amplitude data
 *
 *  @param[in] data - data
 *  @param[in] data_len - length of data
 *  @param[out] tx_amplitude - tx amplitude
 *  @return nsm_completion_codes
 */
int decode_PCIe_TxAmplitude_data(const uint8_t *data, size_t data_len,
				 uint8_t *tx_amplitude);

/** @brief Encode a PCIe Tx Amplitude data
 *
 *  @param[in] tx_amplitude - tx amplitude
 *  @param[out] data - data
 *  @param[out] data_len - length of data
 *  @return nsm_completion_codes
 */
int encode_PCIe_TxAmplitude_data(uint8_t tx_amplitude, uint8_t *data,
				 size_t *data_len);

/** @brief Encode a Set PCIe Port Config aggregate request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_id - port id
 *  @param[in] port_type - port type
 *  @param[in] port_index - port index
 *  @param[in] sample_count - sample count
 *  @param[in] sample_data - sample data
 *  @param[in] sample_data_len - length of sample data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_port_config_aggregate_req(uint8_t instance_id, uint8_t port_id,
					 uint8_t port_type, uint8_t port_index,
					 uint16_t sample_count,
					 const uint8_t *sample_data,
					 size_t sample_data_len,
					 struct nsm_msg *msg);

/** @brief Decode a Set PCIe Port Config aggregate request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - port number
 *  @param[out] port_type - port type
 *  @param[out] port_index - port index
 *  @param[out] sample_count - sample count
 *  @param[out] sample_data - sample data
 *  @param[out] sample_data_len - length of sample data
 *  @return nsm_completion_codes
 */
int decode_set_port_config_aggregate_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *port_number,
    uint8_t *port_type, uint8_t *port_index, uint16_t *sample_count,
    const uint8_t **sample_data, size_t *sample_data_len);

/** @brief Encode a Set PCIe Port Config aggregate response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_port_config_aggregate_resp(uint8_t instance_id, uint8_t cc,
					  uint16_t reason_code,
					  struct nsm_msg *msg);

/** @brief Decode a Set PCIe Port Config aggregate response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_port_config_aggregate_resp(const struct nsm_msg *msg,
					  size_t msg_len, uint8_t *cc,
					  uint16_t *reason_code);

/** @brief Encode a Query Scalar Group Telemetry v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_id - device id for the query
 *  @param[in] group_index - index of group
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_req(uint8_t instance_id,
					       uint8_t device_id,
					       uint8_t group_index,
					       struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[in] device_id - device id for the query
 *  @param[in] group_index - index of group
 *  @return nsm_completion_codes
 */
int decode_query_scalar_group_telemetry_v1_req(const struct nsm_msg *msg,
					       size_t msg_len,
					       uint8_t *device_id,
					       uint8_t *group_index);

//---------------------------------------------------------------------------------------------------------------------------

/** @brief Encode a Query Scalar Group Telemetry v1 of  response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] flags - bits indicating row remap state flags
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_resp(uint8_t instance_id, uint8_t cc,
						uint16_t reason_code,
						const uint16_t data_size,
						uint8_t *data,
						struct nsm_msg *msg);

/** @brief Decode a Get row remap state response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[out] data  - pointer to the array of data
 *  @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_resp(const struct nsm_msg *msg,
						size_t msg_len, uint8_t *cc,
						uint16_t *data_size,
						uint16_t *reason_code,
						uint8_t *data);
//----------------------------------------------------------------------------------------------------------------------
/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 0
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 0 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group0_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_0 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 0
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 0 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group0_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_0 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 1
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 1 data source
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_1 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 1
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 1 data source
 *  @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_1 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 2
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 2 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group2_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_2 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 2
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 2 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group2_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_2 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 3
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 3 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group3_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_3 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 3
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 3 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group3_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_3 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 4
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 4 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group4_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_4 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 4
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 4 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group4_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_4 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 5
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 5 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group5_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_5 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 5
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 5 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group5_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_5 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 6
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 6 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group6_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_6 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 6
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 6 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group6_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_6 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 7
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 7 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group7_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_7 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 7
 *
 *  @param[in] msg - Response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - Completion code
 *  @param[out] data_size - Size of the group data
 *  @param[out] reason_code - NSM reason code
 *  @param[out] data - struct pointer group 7 data source
 * @return nsm_completion_codes
 */
int decode_query_scalar_group_telemetry_v1_group7_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_7 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 8
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 6 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group8_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_8 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 6
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 6 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group8_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_8 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 6
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 6 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group9_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_9 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 6
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 6 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group9_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_9 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 10
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 10 data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group10_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_query_scalar_group_telemetry_group_10 *data,
    struct nsm_msg *msg);

/** @brief Encode a Query Scalar Group Telemetry v1 response msg of GroupID 10
 * Extended
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 10 extended data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group10_extended_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_query_scalar_group_telemetry_group_10_extended *data,
    struct nsm_msg *msg);

/**  @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 10
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 10 data source
 * @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group10_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_10 *data);

/**  @brief Decode a Query Scalar Group Telemetry v1 response msg of GroupID 10
 * Extended
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data - struct pointer group 10 extended data source
 * @return nsm_completion_codes
 */
int decode_query_scalar_group_telemetry_v1_group10_extended_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_10_extended *data);

/** @struct nsm_assert_pcie_fundamental_reset_req
 *
 *  Structure representing NSM Assert PCIe Fundamental Reset Request.
 */
struct nsm_assert_pcie_fundamental_reset_req {
	struct nsm_common_req hdr;
	uint8_t device_index;
	uint8_t action;
} __attribute__((packed));

enum action { NOT_RESET, RESET };
/** @brief Encode a Assert PCIe Fundamental Reset Request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_index - Device Index
 *  @param[in] action - Action to be performed 0/1 (not reset/ reset)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_assert_pcie_fundamental_reset_req(uint8_t instance,
					     uint8_t device_index,
					     uint8_t action,
					     struct nsm_msg *msg);

/** @brief Decode a Assert PCIe Fundamental Reset request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[in] device_index - Device Index
 *  @param[in] action - Action to be performed 0/1 (not reset/ reset)
 *  @return nsm_completion_codes
 */
int decode_assert_pcie_fundamental_reset_req(const struct nsm_msg *msg,
					     size_t msg_len,
					     uint8_t *device_index,
					     uint8_t *action);

/** @brief Encode a Assert PCIe Fundamental Reset response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_assert_pcie_fundamental_reset_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_msg *msg);

/** @brief Decode a Assert PCIe Fundamental Reset response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_assert_pcie_fundamental_reset_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *data_size,
					      uint16_t *reason_code);

/** @struct nsm_query_available_clearable_scalar_data_sources_v1_req
 *
 *  Structure representing Query Scalable and Clearable Scalar data sources v1
 * request.
 */
struct nsm_query_available_clearable_scalar_data_sources_v1_req {
	struct nsm_common_req hdr;
	uint8_t device_index;
	uint8_t group_id;
} __attribute__((packed));

/** Maximum scalar data-source mask length libnsm will decode into a caller
 *  buffer. Bounds the wire mask_length so a malicious response cannot overflow
 *  the fixed-size destination masks (see decoder). Callers size their
 *  available/clearable mask buffers to at least this many bytes.
 */
#define NSM_MAX_SCALAR_DATA_SOURCE_MASK_SIZE 4

/** @struct nsm_query_available_clearable_scalar_data_sources_v1_resp
 *
 *  Structure representing Query Scalable and Clearable Scalar data sources v1
 * response.
 */
struct nsm_query_available_clearable_scalar_data_sources_v1_resp {
	struct nsm_common_resp hdr;
	uint8_t mask_length;
	uint8_t data[1];
} __attribute__((packed));

/** @brief Encode a Query Scalable and Clearable Scalar data sources v1 request
 * message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_index - device index for the query
 *  @param[in] group_id - id of group
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_available_clearable_scalar_data_sources_v1_req(
    uint8_t instance_id, uint8_t device_index, uint8_t group_id,
    struct nsm_msg *msg);

/** @brief Decode a Query Scalable and Clearable Scalar data sources v1 request
 * message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_index - device id for the query
 *  @param[out] group_id - id of group
 *  @return nsm_completion_codes
 */
int decode_query_available_clearable_scalar_data_sources_v1_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *device_index,
    uint8_t *group_id);

/** @brief Encode a Query Scalable and Clearable Scalar data sources v1 response
 * message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] mask_length - length of supported masks
 *  @param[in] available_data_source_mask -  mask representing available data
 * sources
 *  @param[in] clearable_data_source_mask -  mask representing clearable data
 * sources
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_available_clearable_scalar_data_sources_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const uint16_t data_size, uint8_t mask_length,
    uint8_t *available_data_source_mask, uint8_t *clearable_data_source_mask,
    struct nsm_msg *msg);

/** @brief Decode Query Scalable and Clearable Scalar data sources v1 message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] data_size - data size
 *  @param[out] mask_length - length of supported masks
 *  @param[out] available_data_source_mask -  mask representing available data
 * sources
 *  @param[out] clearable_data_source_mask -  mask representing clearable data
 * sources
 *  @return nsm_completion_codes
 */

int decode_query_available_clearable_scalar_data_sources_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint8_t *mask_length,
    uint8_t *available_data_source_mask, uint8_t *clearable_data_source_mask);

/** @struct nsm_clear_data_source_v1_req
 *
 *  Structure representing NSM Clear Data Source V1 Request.
 */
struct nsm_clear_data_source_v1_req {
	struct nsm_common_req hdr;
	uint8_t device_index;
	uint8_t groupId;
	uint8_t dsId;
} __attribute__((packed));

/** @brief Encode a NSM Clear Data Source V1 Request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_index - Device Index
 *  @param[in] groupId - Identifier of group to query
 *  @param[in] dsId - Index of data source within the group.
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_clear_data_source_v1_req(uint8_t instance_id, uint8_t device_index,
				    uint8_t groupId, uint8_t dsId,
				    struct nsm_msg *msg);

/** @brief Decode a NSM Clear Data Source V1 Request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_index - Device Index
 *  @param[out] groupId - Identifier of group to query
 *  @param[out] dsId - Index of data source within the group.
 *  @return nsm_completion_codes
 */
int decode_clear_data_source_v1_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *device_index, uint8_t *groupId,
				    uint8_t *dsId);

/** @brief Encode a NSM Clear Data Source V1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_clear_data_source_v1_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a NSM Clear Data Source V1 Response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] data_size - data size
 *  @param[out] reason_code - reason code
 *  @return nsm_completion_codes
 */
int decode_clear_data_source_v1_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *data_size,
				     uint16_t *reason_code);

/** @brief Encode a List Available PCIe Ports Request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_sw_codes
 */
int encode_list_available_pcie_ports_req(uint8_t instance_id,
					 struct nsm_msg *msg);

/** @brief Decode a List Available PCIe Ports Request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_sw_codes
 */
int decode_list_available_pcie_ports_req(const struct nsm_msg *msg,
					 size_t msg_len);

/** @brief Encode a List Available PCIe Ports Response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_sw_codes
 */
int encode_list_available_pcie_ports_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_list_available_pcie_ports_info *info, struct nsm_msg *msg);

/** @brief Decode a List Available PCIe Ports Response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - NSM reason code
 *  @return nsm_sw_codes
 */
int decode_list_available_pcie_ports_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_list_available_pcie_ports_info *info);

/** @brief Encode a Multiport Query Scalar Group Telemetry v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] data - data to be encoded
 *  @param[out] msg - Message will be written to this
 *  @return nsm_sw_codes
 */
int encode_multiport_query_scalar_group_telemetry_v2_req(
    uint8_t instance_id,
    const struct nsm_multiport_query_scalar_group_telemetry_v2_req_data *data,
    struct nsm_msg *msg);

/** @brief Decode a Multiport Query Scalar Group Telemetry v1 request message
 *
 *  @param[in] msg - Decoded request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] data - data to be decoded
 *  @return nsm_sw_codes
 */
int decode_multiport_query_scalar_group_telemetry_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data *data);

/** @brief Encode a Query Vector Data Sources v2 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] port_number - upstream port number (bits 0:6)
 *  @param[in] port_type - port type (bit 7: 0-upstream, 1-downstream)
 *  @param[in] index - port index
 *  @param[in] group_id - vector group id
 *  @param[in] selector_0 - selector 0 (e.g., lane number for group 1)
 *  @param[in] selector_1 - selector 1 (varies by group)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_vector_group_telemetry_v2_req(
    uint8_t instance_id, uint8_t port_number, uint8_t port_type, uint8_t index,
    uint8_t group_id, uint8_t selector_0, uint8_t selector_1,
    struct nsm_msg *msg);

/** @brief Decode a Query Vector Data Sources v2 request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] port_number - upstream port number
 *  @param[out] port_type - port type
 *  @param[out] index - port index
 *  @param[out] group_id - vector group id
 *  @param[out] selector_0 - selector 0
 *  @param[out] selector_1 - selector 1
 *  @return nsm_completion_codes
 */
int decode_query_vector_group_telemetry_v2_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *port_number,
    uint8_t *port_type, uint8_t *index, uint8_t *group_id, uint8_t *selector_0,
    uint8_t *selector_1);

/** @brief Encode a Query Vector Data Sources v2 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data_count - number of data values
 *  @param[in] data_values - array of 32-bit data values
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_vector_group_telemetry_v2_resp(uint8_t instance_id, uint8_t cc,
						uint16_t reason_code,
						uint16_t data_count,
						const uint32_t *data_values,
						struct nsm_msg *msg);

/** @brief Decode a Query Vector Data Sources v2 response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - completion code
 *  @param[out] data_size - data size in bytes
 *  @param[out] reason_code - NSM reason code
 *  @param[out] data - pointer to the array of data
 *  @return nsm_completion_codes
 */
int decode_query_vector_group_telemetry_v2_resp(const struct nsm_msg *msg,
						size_t msg_len, uint8_t *cc,
						uint16_t *data_size,
						uint16_t *reason_code,
						uint8_t *data);

/** @brief Encode a Query Vector Group Telemetry v2 response for Vector Group 1
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - Vector Group 1 data (error counter for one lane)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_vector_group_telemetry_v2_group1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_query_vector_group_1_data *data, struct nsm_msg *msg);

/** @brief Decode a Query Vector Group Telemetry v2 response for Vector Group 1
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - completion code
 *  @param[out] reason_code - NSM reason code
 *  @param[out] data - Vector Group 1 data (error counter for one lane)
 *  @return nsm_completion_codes
 */
int decode_query_vector_group_telemetry_v2_group1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_query_vector_group_1_data *data);

#ifdef __cplusplus
}
#endif
#endif