#ifndef BASE_H
#define BASE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#define MCTP_MSG_TYPE_PCI_VDM 0x7E
#define NSM_INSTANCE_MAX 31

#define PCI_VENDOR_ID 0x10de // NVIDIA

#define OCP_TYPE 8
#define OCP_VERSION 9

enum nsm_type {
	NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY = 0,
	NSM_TYPE_NETWORK_PORT = 1,
	NSM_TYPE_PCI_LINK = 2,
	NSM_TYPE_PLATFORM_ENVIRONMENTAL = 3,
	NSM_TYPE_DIAGNOSTIC = 4,
	NSM_TYPE_DEVICE_CONFIGURATION = 5,
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
};

/** @brief NSM completion codes
 */
enum nsm_completion_codes {
	NSM_SUCCESS = 0x00,
	NSM_ACCEPTED = 0x01,
	NSM_ERR_GENERIC = 0x02,
	NSM_ERR_NOT_READY = 0x03,
	NSM_ERR_REQUEST = 0x04,
	NSM_ERR_UNSUPPORTED_MSG_TYPE = 0x05,
	NSM_ERR_UNSUPPORTED_COMMAND_CODE = 0x06,
	NSM_ERR_INVALID_DATA_SIZE = 0x07,
	NSM_ERR_INVALID_ARG1 = 0x08,
	NSM_ERR_INVALID_ARG2 = 0x09,
	NSM_ERR_INVALID_DATA = 0x0A,
	NSM_ERR_BUSY = 0x0B,
	NSM_ERR_DATA_NOT_AVAILABLE = 0x0C,
	NSM_ERR_BUS_ACCESS = 0x0D,
	NSM_ERR_AGAIN = 0x0E,
	NSM_PARTIAL_SUCCESS = 0x0F
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

typedef float real32_t;

#define NSM_REQUEST_CONVENTION_LEN 2
#define NSM_RESPONSE_CONVENTION_LEN 4

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
	NSM_DEV_ID_UNKNOWN = 0xff,
} NsmDeviceIdentification;

/** @struct nsm_msg_hdr
 *
 * Structure representing NSM message header fields
 */
struct nsm_msg_hdr {
	uint8_t message_type : 7; //!< vendor-defined message - PCI(0x7E)
	uint8_t ic : 1;		  //!< Integrity check
	uint16_t pci_vendor_id;	  //!< PCI defined vendor ID for NVIDIA (0x10DE)

	uint8_t instance_id : 5; //!< Instance ID
	uint8_t reserved : 1;	 //!< Reserved
	uint8_t datagram : 1;	 //!< Datagram bit
	uint8_t request : 1;	 //!< Request bit

	uint8_t ocp_type : 4;	 //!< OCP type
	uint8_t ocp_version : 4; //!< OCP version

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
 *  Structure representing NSM request.
 */
struct nsm_common_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t arg1;
	uint8_t arg2;
} __attribute__((packed));

/** @struct nsm_common_resp
 *
 *  Structure representing NSM response.
 */
struct nsm_common_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
} __attribute__((packed));

/** @struct nsm_get_supported_nvidia_message_types_req
 *
 *  Structure representing NSM get supported NVIDIA message types request.
 */
struct nsm_get_supported_nvidia_message_types_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t arg1;
	uint8_t arg2;
} __attribute__((packed));

/** @struct nsm_get_supported_nvidia_message_types_resp
 *
 *  Structure representing NSM get supported NVIDIA message types response.
 */
struct nsm_get_supported_nvidia_message_types_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	bitfield8_t types[8];
} __attribute__((packed));

/** @struct nsm_get_supported_command_code_req
 *
 *  Structure representing NSM get supported command codes request.
 */
struct nsm_get_supported_command_codes_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t arg1;
	uint8_t arg2;
} __attribute__((packed));

/** @struct nsm_get_supported_command_code_resp
 *
 *  Structure representing NSM get supported command codes response.
 */
struct nsm_get_supported_command_codes_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	bitfield8_t supported_command_codes[32];
} __attribute__((packed));

/** @struct nsm_query_device_identification_req
 *
 *  Structure representing NSM query device identification request
 */
struct nsm_query_device_identification_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t arg1;
	uint8_t arg2;
} __attribute__((packed));

/** @struct nsm_query_device_identification_resp
 *
 *  Structure representing NSM query device identification response.
 */
struct nsm_query_device_identification_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
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
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_cc_only_resp(uint8_t instance_id, uint8_t type, uint8_t command,
			uint8_t cc, struct nsm_msg *msg);

/** @brief Create a NSM ping request message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_ping_req(uint8_t instance, struct nsm_msg *msg);

/** @brief Create a NSM ping response message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_ping_resp(uint8_t instance, struct nsm_msg *req);

/** @brief decode a NSM ping response message
 *
 *  @param[in] resp    - response message
 *  @param[in] respLen - Length of response message
 *  @param[out] cc     - Completion Code
 *  @return nsm_completion_codes
 */
int decode_ping_resp(const struct nsm_msg *msg, size_t msgLen, uint8_t *cc);

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
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_nvidia_message_types_resp(uint8_t instance,
						   const bitfield8_t *types,
						   struct nsm_msg *msg);

/** @brief Decode a Get Supported Nvidia Message Type response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] types  - pointer to array bitfield8_t[8] for receiving supported
 *                       types
 *  @return nsm_completion_codes
 */
int decode_get_supported_nvidia_message_types_resp(const struct nsm_msg *msg,
						   size_t msg_len, uint8_t *cc,
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
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_command_codes_resp(uint8_t instance_id,
					    const bitfield8_t *command_codes,
					    struct nsm_msg *msg);

/** @brief Decode a Get Supported Command codes response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] command_codes  - pointer to array bitfield8_t[8] containing
 * supported command codes
 *  @return nsm_completion_codes
 */
int decode_get_supported_command_codes_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
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
 *  @param[in] device_identification - device identification
 *  @param[in] instance_id - instance id
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_query_device_identification_resp(uint8_t instance,
					    const uint8_t device_identification,
					    const uint8_t instance_id,
					    struct nsm_msg *msg);

/** @brief Decode a Query device identification response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to completion code
 *  @param[out] device_identification - pointer to device_identification
 *  @param[out] instance_id - pointer to instance id
 *  @return nsm_completion_codes
 */
int decode_query_device_identification_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint8_t *device_identification,
					    uint8_t *device_instance);

#ifdef __cplusplus
}
#endif
#endif /* BASE_H */