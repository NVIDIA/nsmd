#ifndef PCI_LINKS_H
#define PCI_LINKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

enum pci_links_command { NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1 = 0x04 };

/** @struct nsm_query_scalar_group_telemetry_v1_req
 *
 *  Structure representing Query Scalar Group Telemetry v1 request.
 */
struct nsm_query_scalar_group_telemetry_v1_req {
	struct nsm_common_req hdr;
	uint8_t device_id;
	uint8_t group_index;
} __attribute__((packed));

/** @struct nsm_query_scalar_group_telemetry_group_1
 *
 *  Structure representing Scalar Group Telemetry Data for Group 1.
 */
struct nsm_query_scalar_group_telemetry_group_1 {
	uint32_t negotiated_link_speed;
	uint32_t negotiated_link_width;
	uint32_t target_link_speed;
	uint32_t max_link_speed;
	uint32_t max_link_width;
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


/** @brief Encode a Query Scalar Group Telemetry v1 response message of GroupID
 * 1
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

/** @brief Decode a Query Scalar Group Telemetry v1 response message of GroupID
 * 1
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

/** @brief Encode a Query Scalar Group Telemetry v1 response message of Group ID
 * 2
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 2 data source
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group2_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_2 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response message of GroupID
 * 2
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 2 data source
 *  @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group2_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_2 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response message of Group ID
 * 3
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 3 data source
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_scalar_group_telemetry_v1_group3_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_3 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response message of GroupID
 * 3
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 3 data source
 *  @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group3_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_3 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response message of Group ID
 * 4
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 4 data source
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */

int encode_query_scalar_group_telemetry_v1_group4_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_4 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response message of GroupID
 * 4
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 4 data source
 *  @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group4_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_4 *data);

/** @brief Encode a Query Scalar Group Telemetry v1 response message of Group ID
 * 5
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer group 5 data source
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */

int encode_query_scalar_group_telemetry_v1_group5_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_query_scalar_group_telemetry_group_5 *data, struct nsm_msg *msg);

/** @brief Decode a Query Scalar Group Telemetry v1 response message of GroupID
 * 5
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer group 5 data source
 *  @return nsm_completion_codes
 */

int decode_query_scalar_group_telemetry_v1_group5_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code,
    struct nsm_query_scalar_group_telemetry_group_5 *data);

#ifdef __cplusplus
}
#endif
#endif