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
#define TOTAL_PCIE_LANE_COUNT 16

#define DS_ID_0 0
#define DS_ID_1 1
#define DS_ID_2 2
#define DS_ID_3 3
#define DS_ID_4 4
#define DS_ID_5 5
#define DS_ID_6 6

#define MAX_SUPPORTED_DATA_MASK_LENGTH 1

enum pci_links_command {
	NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1 = 0x04,
	NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES = 0x02,
	NSM_CLEAR_DATA_SOURCE_V1 = 0x05,
	NSM_ASSERT_PCIE_FUNDAMENTAL_RESET = 0x60
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
	uint32_t negotiated_link_speed; // dsid 0
	uint32_t negotiated_link_width; // dsid 1
	uint32_t target_link_speed;	// dsid 2
	uint32_t max_link_speed;	// dsid 3
	uint32_t max_link_width;	// dsid 4
} __attribute__((packed));

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
	uint32_t non_fatal_errors;
	uint32_t fatal_errors;
	uint32_t unsupported_request_count;
	uint32_t correctable_errors;
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
	uint32_t L0ToRecoveryCount;
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
	uint32_t recv_err_cnt;
	uint32_t NAK_recv_cnt;
	uint32_t NAK_sent_cnt;
	uint32_t bad_TLP_cnt;
	uint32_t replay_rollover_cnt;
	uint32_t FC_timeout_err_cnt;
	uint32_t replay_cnt;
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
	uint32_t PCIeTXBytes;
	uint32_t PCIeRXBytes;
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
	/**
	 * @brief Current LTSSM state. The value is encoded as follows:
	 *      0x0 – Detect
	 *      0x1 – Polling
	 *      0x2 – Configuration
	 *      0x3 – Recovery
	 *      0x4 – Recovery.EQ
	 *      0x5 – L0
	 *      0x6 – L0s
	 *      0x7 – L1
	 *      0x8 – L1_PLL_PD
	 *      0x9 – L2
	 *      0xA – L1 CPM
	 *      0xB – L1.1
	 *      0xC – L1.2
	 *      0xD – Hot Reset
	 *      0xE – Loopback
	 *      0xF – Disabled
	 *      0x10 – Link down
	 *      0x11 – Link ready
	 *      0x12 – Lanes in sleep
	 *      0xFF – Illegal state
	 *
	 */
	uint32_t ltssm_state;
	/**
	 * @brief Invalid FLIT counter
	 *
	 */
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

/** @struct nsm_assert_pcie_fundamental_reset_req
 *
 *  Structure representing NSM Assert PCIe Fundamental Reset Request.
 */
struct nsm_assert_pcie_fundamental_reset_req {
	struct nsm_common_req hdr;
	uint8_t device_index;
	uint8_t action;
} __attribute__((packed));

enum action {NOT_RESET, RESET};
/** @brief Encode a Assert PCIe Fundamental Reset Request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_index - Device Index
 *  @param[in] action - Action to be performed 0/1 (not reset/ reset)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_assert_pcie_fundamental_reset_req(uint8_t instance, uint8_t device_index, uint8_t action,
			    struct nsm_msg *msg);

/** @brief Decode a Assert PCIe Fundamental Reset request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[in] device_index - Device Index
 *  @param[in] action - Action to be performed 0/1 (not reset/ reset)
 *  @return nsm_completion_codes
 */
int decode_assert_pcie_fundamental_reset_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *device_index, uint8_t *action);

/** @brief Encode a Assert PCIe Fundamental Reset response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_assert_pcie_fundamental_reset_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a Assert PCIe Fundamental Reset response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_assert_pcie_fundamental_reset_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
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
int encode_clear_data_source_v1_req(uint8_t instance_id, uint8_t device_index, uint8_t groupId, uint8_t dsId,
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
			    uint8_t *device_index, uint8_t *groupId, uint8_t *dsId);

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

#ifdef __cplusplus
}
#endif
#endif