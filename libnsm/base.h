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

#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NSM_INSTANCE_MAX 31

#define PCI_VENDOR_ID 0x10de // NVIDIA

#define OCP_TYPE 8
#define OCP_VERSION 9

#define SUPPORTED_MSG_TYPE_DATA_SIZE 32
#define SUPPORTED_COMMAND_CODE_DATA_SIZE 32

#define NSM_AGGREGATE_MAX_SAMPLE_TAG_VALUE 0xFF
#define NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE 0xEF
// NSM Aggregate sample size is represented in 3 bits as power of 2
#define NSM_AGGREGATE_MAX_SAMPLE_SIZE_AS_POWER_OF_2 7

#define DEFAULT_INSTANCE_ID 0
#define INSTANCEID_MASK 0x1f
#define NSM_EVENT_VERSION 0

#define NSM_EVENT_MIN_LEN 6
// rsvd:ackr:version(1byte) + event id(1byte) + event class(1byte) + event +
// state(2byte) + data size(1byte)

#define NSM_EVENT_CONVENTION_LEN 3
// rsvd_ackr_version(1byte) + data size(1byte) + eventId(1byte)

#define NSM_EVENT_ACK_LEN 1
// eventId(1byte)

#define NSM_EVENT_DATA_MAX_LEN 256
#define NSM_EVENT_MAX_EVENT_ID 256

#define NUM_NSM_TYPES 7
#define NUM_COMMAND_CODES 256

#define UNKNOWN_INSTANCE_ID 255

enum nsm_type {
	NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY = 0,
	NSM_TYPE_NETWORK_PORT = 1,
	NSM_TYPE_PCI_LINK = 2,
	NSM_TYPE_PLATFORM_ENVIRONMENTAL = 3,
	NSM_TYPE_DIAGNOSTIC = 4,
	NSM_TYPE_DEVICE_CONFIGURATION = 5,
	NSM_TYPE_FIRMWARE = 6,
};

/** @brief NSM Type0 Device Capability Discovery Commands
 */
enum nsm_device_capability_discovery_commands {
	NSM_PING = 0x00,
	NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES = 0x01,
	NSM_SUPPORTED_COMMAND_CODES = 0x02,
	NSM_SUPPORTED_EVENT_SOURCES = 0x03,
	NSM_GET_CURRENT_EVENT_SOURCES = 0x04,
	NSM_SET_CURRENT_EVENT_SOURCES = 0x05,
	NSM_SET_EVENT_SUBSCRIPTION = 0x06,
	NSM_GET_EVENT_SUBSCRIPTION = 0x07,
	NSM_GET_EVENT_LOG_RECORD = 0x08,
	NSM_QUERY_DEVICE_IDENTIFICATION = 0x09,
	NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT = 0x0A,
	NSM_GET_DEVICE_CAPABILITIES = 0x0B
};

/** @brief NSM completion codes
 */
enum nsm_completion_codes {
	NSM_SUCCESS = 0x00,
	NSM_ERROR = 0x01,
	NSM_ERR_INVALID_DATA = 0x02,
	NSM_ERR_INVALID_DATA_LENGTH = 0x03,
	NSM_ERR_NOT_READY = 0x04,
	NSM_ERR_UNSUPPORTED_COMMAND_CODE = 0x05,
	NSM_ERR_UNSUPPORTED_MSG_TYPE = 0x06,
	NSM_ACCEPTED = 0x7e,
	NSM_ERR_BUS_ACCESS = 0x7f
};

/** @brief NSM reason codes
 */
enum nsm_reason_codes {
	ERR_NULL = 0x00,
	ERR_INVALID_PCI = 0x01,
	ERR_INVALID_RQD = 0x02,
	ERR_TIMEOUT = 0x03,
	ERR_DOWNSTREAM_TIMEOUT = 0x04,
	ERR_I2C_NACK_FROM_DEV_ADDR = 0x05,
	ERR_I2C_NACK_FROM_DEV_CMD_DATA = 0x06,
	ERR_I2C_NACK_FROM_DEV_ADDR_RS = 0x07,
	ERR_NVLINK_PORT_INVALID = 0x08,
	ERR_NVLINK_PORT_DISABLED = 0x09,
	ERR_NOT_SUPPORTED = 0x0A
};

/** @brief NSM Software Error codes
 */
enum nsm_sw_codes {
	NSM_SW_SUCCESS = 0x00,
	NSM_SW_ERROR = 0x01,
	NSM_SW_ERROR_DATA = 0x02,
	NSM_SW_ERROR_LENGTH = 0x03,
	NSM_SW_ERROR_NULL = 0x04,
	NSM_SW_ERROR_COMMAND_FAIL = 0x05
};

/** @brief NSM event class
 */
enum nsm_evnet_class {
	NSM_GENERAL_EVENT_CLASS = 0x00,
	NSM_ASSERTION_DEASSERTION_EVENT_CLASS = 0x01
};

typedef union {
	uint8_t byte;
	struct {
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
	} __attribute__((packed)) bits;
} bitfield8_t;

typedef union {
	uint16_t byte;
	struct {
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
		uint8_t bit8 : 1;
		uint8_t bit9 : 1;
		uint8_t bit10 : 1;
		uint8_t bit11 : 1;
		uint8_t bit12 : 1;
		uint8_t bit13 : 1;
		uint8_t bit14 : 1;
		uint8_t bit15 : 1;
	} __attribute__((packed)) bits;
} bitfield16_t;

typedef union {
	uint32_t byte;
	struct {
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
		uint8_t bit8 : 1;
		uint8_t bit9 : 1;
		uint8_t bit10 : 1;
		uint8_t bit11 : 1;
		uint8_t bit12 : 1;
		uint8_t bit13 : 1;
		uint8_t bit14 : 1;
		uint8_t bit15 : 1;
		uint8_t bit16 : 1;
		uint8_t bit17 : 1;
		uint8_t bit18 : 1;
		uint8_t bit19 : 1;
		uint8_t bit20 : 1;
		uint8_t bit21 : 1;
		uint8_t bit22 : 1;
		uint8_t bit23 : 1;
		uint8_t bit24 : 1;
		uint8_t bit25 : 1;
		uint8_t bit26 : 1;
		uint8_t bit27 : 1;
		uint8_t bit28 : 1;
		uint8_t bit29 : 1;
		uint8_t bit30 : 1;
		uint8_t bit31 : 1;
	} __attribute__((packed)) bits;
} bitfield32_t;

typedef union {
	uint64_t byte;
	struct {
		uint8_t bit0 : 1;
		uint8_t bit1 : 1;
		uint8_t bit2 : 1;
		uint8_t bit3 : 1;
		uint8_t bit4 : 1;
		uint8_t bit5 : 1;
		uint8_t bit6 : 1;
		uint8_t bit7 : 1;
		uint8_t bit8 : 1;
		uint8_t bit9 : 1;
		uint8_t bit10 : 1;
		uint8_t bit11 : 1;
		uint8_t bit12 : 1;
		uint8_t bit13 : 1;
		uint8_t bit14 : 1;
		uint8_t bit15 : 1;
		uint8_t bit16 : 1;
		uint8_t bit17 : 1;
		uint8_t bit18 : 1;
		uint8_t bit19 : 1;
		uint8_t bit20 : 1;
		uint8_t bit21 : 1;
		uint8_t bit22 : 1;
		uint8_t bit23 : 1;
		uint8_t bit24 : 1;
		uint8_t bit25 : 1;
		uint8_t bit26 : 1;
		uint8_t bit27 : 1;
		uint8_t bit28 : 1;
		uint8_t bit29 : 1;
		uint8_t bit30 : 1;
		uint8_t bit31 : 1;
		uint8_t bit32 : 1;
		uint8_t bit33 : 1;
		uint8_t bit34 : 1;
		uint8_t bit35 : 1;
		uint8_t bit36 : 1;
		uint8_t bit37 : 1;
		uint8_t bit38 : 1;
		uint8_t bit39 : 1;
		uint8_t bit40 : 1;
		uint8_t bit41 : 1;
		uint8_t bit42 : 1;
		uint8_t bit43 : 1;
		uint8_t bit44 : 1;
		uint8_t bit45 : 1;
		uint8_t bit46 : 1;
		uint8_t bit47 : 1;
		uint8_t bit48 : 1;
		uint8_t bit49 : 1;
		uint8_t bit50 : 1;
		uint8_t bit51 : 1;
		uint8_t bit52 : 1;
		uint8_t bit53 : 1;
		uint8_t bit54 : 1;
		uint8_t bit55 : 1;
		uint8_t bit56 : 1;
		uint8_t bit57 : 1;
		uint8_t bit58 : 1;
		uint8_t bit59 : 1;
		uint8_t bit60 : 1;
		uint8_t bit61 : 1;
		uint8_t bit62 : 1;
		uint8_t bit63 : 1;
	} __attribute__((packed)) bits;
} bitfield64_t;

// command(1byte) + data_size(1byte)
#define NSM_REQUEST_CONVENTION_LEN 2
// command(1byte) + completion code(1byte) + reserved(2bytes) +
// data_size(2bytes)
#define NSM_RESPONSE_CONVENTION_LEN 6
// command(1byte) + error completion code(1byte) + reason code(2bytes)
#define NSM_RESPONSE_ERROR_LEN 4
// The minimum size of NSM response message is the case response with error CC.
#define NSM_RESPONSE_MIN_LEN NSM_RESPONSE_ERROR_LEN

typedef float real32_t;

/** @enum MessageType
 *
 *  The different message types supported by the NSM specification.
 */
typedef enum {
	NSM_RESPONSE = 0,	      //!< NSM response message
	NSM_EVENT_ACKNOWLEDGMENT = 1, //!< NSM event acknowledgement
	NSM_REQUEST = 2,	      //!< NSM request message
	NSM_EVENT = 3,		      //!< NSM event message
} NsmMessageType;

typedef enum {
	NSM_DEV_ID_GPU = 0,
	NSM_DEV_ID_SWITCH = 1,
	NSM_DEV_ID_PCIE_BRIDGE = 2,
	NSM_DEV_ID_BASEBOARD = 3,
	NSM_DEV_ID_EROT = 4,
	NSM_DEV_ID_UNKNOWN = 0xff,
} NsmDeviceIdentification;

/** @struct nsm_msg_hdr
 *
 * Structure representing NSM message header fields
 */
struct nsm_msg_hdr {
	uint16_t pci_vendor_id; //!< PCI defined vendor ID for NVIDIA (0x10DE)

	uint8_t instance_id : 5; //!< Instance ID
	uint8_t reserved : 1;	 //!< Reserved
	uint8_t datagram : 1;	 //!< Datagram bit
	uint8_t request : 1;	 //!< Request bit

	uint8_t ocp_version : 4; //!< OCP version
	uint8_t ocp_type : 4;	 //!< OCP type

	uint8_t nvidia_msg_type; //!< NVIDIA Message Type
} __attribute__((packed));

/** @struct nsm_msg
 *
 * Structure representing NSM message
 */
struct nsm_msg {
	struct nsm_msg_hdr hdr; //!< NSM message header
	uint8_t payload[1];	//!< &payload[0] is the beginning of the payload
} __attribute__((packed));

/** @struct nsm_header_info
 *
 *  The information needed to prepare NSM header and this is passed to the
 *  pack_nsm_header and unpack_nsm_header API.
 */
struct nsm_header_info {
	uint8_t nsm_msg_type;
	uint8_t instance_id;
	uint8_t nvidia_msg_type;
};

/** @struct nsm_common_req
 *
 *  Structure representing NSM request without data
 */
struct nsm_common_req {
	uint8_t command;
	uint8_t data_size;
} __attribute__((packed));

/** @struct nsm_common_resp
 *
 *  Structure representing NSM response without data
 */
struct nsm_common_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t reserved;
	uint16_t data_size;
} __attribute__((packed));

/** @struct nsm_common_non_success_resp
 *
 *  Structure representing NSM response with reason code when CC != Success
 */
struct nsm_common_non_success_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t reason_code;
} __attribute__((packed));

/** @struct nsm_common_telemetry_resp
 *
 *  Structure representing NSM successful telemetry response.
 *  It is used with aggregate commands.
 */
struct nsm_common_telemetry_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t telemetry_count;
} __attribute__((packed));

/** @struct nsm_event
 *
 *  Structure representing NSM event message.
 */
struct nsm_event {
	uint8_t version : 4;
	uint8_t ackr : 1;
	uint16_t resvd : 3;
	uint8_t event_id;
	uint8_t event_class;
	uint16_t event_state;
	uint8_t data_size;
	uint8_t data[1];
} __attribute__((packed));

/** @struct nsm_event_ack
 *
 *  Structure representing NSM event ack message.
 */
struct nsm_event_ack {
	uint8_t event_id;
} __attribute__((packed));

/** @struct nsm_get_supported_nvidia_message_types_req
 *
 *  Structure representing NSM get supported NVIDIA message types request.
 */
struct nsm_get_supported_nvidia_message_types_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_get_supported_nvidia_message_types_resp
 *
 *  Structure representing NSM get supported NVIDIA message types response.
 */
struct nsm_get_supported_nvidia_message_types_resp {
	struct nsm_common_resp hdr;
	bitfield8_t
	    supported_nvidia_message_types[SUPPORTED_MSG_TYPE_DATA_SIZE];
} __attribute__((packed));

/** @struct nsm_get_supported_command_code_req
 *
 *  Structure representing NSM get supported command codes request.
 */
struct nsm_get_supported_command_codes_req {
	struct nsm_common_req hdr;
	uint8_t nvidia_message_type;
} __attribute__((packed));

/** @struct nsm_get_supported_command_code_resp
 *
 *  Structure representing NSM get supported command codes response.
 */
struct nsm_get_supported_command_codes_resp {
	struct nsm_common_resp hdr;
	bitfield8_t supported_command_codes[SUPPORTED_COMMAND_CODE_DATA_SIZE];
} __attribute__((packed));

/** @struct nsm_query_device_identification_req
 *
 *  Structure representing NSM query device identification request
 */
struct nsm_query_device_identification_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_query_device_identification_resp
 *
 *  Structure representing NSM query device identification response.
 */
struct nsm_query_device_identification_resp {
	struct nsm_common_resp hdr;
	uint8_t device_identification;
	uint8_t instance_id;
} __attribute__((packed));

/**
 * @brief Populate the NSM message with the NSM header.The caller of this API
 *        allocates buffer for the NSM header when forming the NSM message.
 *        The buffer is passed to this API to pack the NSM header.
 *
 * @param[in] hdr - Pointer to the NSM header information
 * @param[out] msg - Pointer to NSM message header
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
uint8_t pack_nsm_header(const struct nsm_header_info *hdr,
			struct nsm_msg_hdr *msg);

/**
 * @brief Unpack the NSM header from the NSM message.
 *
 * @param[in] msg - Pointer to the NSM message header
 * @param[out] hdr - Pointer to the NSM header information
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
uint8_t unpack_nsm_header(const struct nsm_msg_hdr *msg,
			  struct nsm_header_info *hdr);

/** @brief Create a NSM response message containing only cc
 *
 *  @param[in] command - NSM Command
 *  @param[in] cc - NSM Completion Code
 *  @param[in] reason_code - NSM reason Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_cc_only_resp(uint8_t instance_id, uint8_t type, uint8_t command,
			uint8_t cc, uint16_t reason_code, struct nsm_msg *msg);

/** @brief Create a NSM ping request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_ping_req(uint8_t instance, struct nsm_msg *msg);

/** @brief Create a NSM ping response message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[in] reason_code - Reason Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_ping_resp(uint8_t instance, uint16_t reason_code,
		     struct nsm_msg *req);

/** @brief decode a NSM ping response message
 *
 *  @param[in] resp    - response message
 *  @param[in] respLen - Length of response message
 *  @param[out] cc     - Completion Code
 *  @param[out] reason_code  - Reason Code
 *  @return nsm_completion_codes
 */
int decode_ping_resp(const struct nsm_msg *msg, size_t msgLen, uint8_t *cc,
		     uint16_t *reason_code);

/** @brief Encode reason code
 *
 *  @param[in] cc     - Completion Code
 *  @param[in] reason_code - reason code
 *  @param[out] msg     - msg
 *  @return nsm_completion_code
 */
int encode_reason_code(uint8_t cc, uint16_t reason_code, uint8_t command_code,
		       struct nsm_msg *msg);

/** @brief Decode to get reason code
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to completion code
 *  @param[out] reason_code - pointer to reason_code
 *  @return nsm_completion_code
 */
int decode_reason_code_and_cc(const struct nsm_msg *msg, size_t msg_len,
			      uint8_t *cc, uint16_t *reason_code);

/** @brief Create a Get Supported Nvidia Message Type request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_nvidia_message_types_req(uint8_t instance_id,
						  struct nsm_msg *msg);

/** @brief Create a Get Supported Nvidia Message Type response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] types - pointer to array bitfield8_t[8] containing supported
 *                       types
 *  @param[in] cc - completion code
 *  @param[in] reason_code - reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_nvidia_message_types_resp(uint8_t instance, uint8_t cc,
						   uint16_t reason_code,
						   const bitfield8_t *types,
						   struct nsm_msg *msg);

/** @brief Decode a Get Supported Nvidia Message Type response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code  - pointer to reason code
 *  @param[out] types  - pointer to array bitfield8_t[8] for receiving supported
 *                       types
 *  @return nsm_completion_codes
 */
int decode_get_supported_nvidia_message_types_resp(const struct nsm_msg *msg,
						   size_t msg_len, uint8_t *cc,
						   uint16_t *reason_code,
						   bitfield8_t *types);

/** @brief Create a Get Supported Command codes request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_command_codes_req(uint8_t instance_id,
					   uint8_t msg_type,
					   struct nsm_msg *msg);

/** @brief Create a Get Supported Command codes response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] command_codes - pointer to array bitfield8_t[8] containing
 * supported command codes
 *  @param[in] reason_code - reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_command_codes_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    const bitfield8_t *command_codes,
					    struct nsm_msg *msg);

/** @brief Decode a Get Supported Command codes response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] command_codes  - pointer to array bitfield8_t[8] containing
 * supported command codes
 *  @return nsm_completion_codes
 */
int decode_get_supported_command_codes_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code,
					    bitfield8_t *command_codes);

/** @brief Create a Query device identification request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_query_device_identification_req(uint8_t instance_id,
					       struct nsm_msg *msg);

/** @brief Encode a Query device identification response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc     - NSM completion code
 *  @param[in] reason_code     - NSM reason code
 *  @param[in] device_identification - device identification
 *  @param[in] instance_id - instance id
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_device_identification_resp(uint8_t instance, uint8_t cc,
					    uint16_t reason_code,
					    const uint8_t device_identification,
					    const uint8_t instance_id,
					    struct nsm_msg *msg);

/** @brief Decode a Query device identification response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] device_identification - pointer to device_identification
 *  @param[out] instance_id - pointer to instance id
 *  @return nsm_completion_codes
 */
int decode_query_device_identification_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code,
					    uint8_t *device_identification,
					    uint8_t *device_instance);

/** @brief Create an event acknowledgement message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] nsm_type - NSM Message type
 *  @param[in] event_id - event ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_event_acknowledgement(uint8_t instance_id, uint8_t nsm_type,
				     uint8_t event_id, struct nsm_msg *msg);

/** @brief Decode an event acknowledgement message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] instance_id - pointer to instance id
 *  @param[out] nsm_type - pointer to NSM Message type
 *  @param[out] event_id - pointer to NSM Event ID
 *  @return nsm_completion_codes
 */
int decode_nsm_event_acknowledgement(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *instance_id, uint8_t *nsm_type,
				     uint8_t *event_id);

/** @brief Create a event message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] nsm_type - Nvidia Message Type
 *  @param[in] ackr - acknowledgement request
 *  @param[in] version - event payload version
 *  @param[in] event_id - event identifier
 *  @param[in] event_class - category of the reported event
 *  @param[in] event_state - additional information about the event
 *  @param[in] data_size - the length of data
 *  @param[in] data - point to data payload
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_event(uint8_t instance_id, uint8_t nsm_type, bool ackr,
		     uint8_t version, uint8_t event_id, uint8_t event_class,
		     uint16_t event_state, uint8_t data_size, uint8_t *data,
		     struct nsm_msg *msg);

int decode_common_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a Common response message
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] nvidia_msg_type - NVIDIA message type
 *  @param[in] command - command identifier
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_common_resp(uint8_t instance_id, uint8_t cc, uint16_t reason_code,
		       uint8_t nvidia_msg_type, uint8_t command,
		       struct nsm_msg *msg);

/** @brief Decode a Common response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] data_size - Data Size
 *  @param[out] reason_code - Reason Code
 *  @return nsm_completion_codes
 */
int decode_common_resp(const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
		       uint16_t *data_size, uint16_t *reason_code);
#ifdef __cplusplus
}
#endif
#endif /* BASE_H */
