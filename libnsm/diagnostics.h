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

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

typedef uint8_t enum8;

enum diagnostics_command {
	NSM_GET_DEVICE_RESET_STATISTICS = 0x00,
	NSM_GET_DEVICE_DEBUG_PARAMETERS = 0x04,
	NSM_SET_DEVICE_DEBUG_PARAMETERS = 0x05,
	NSM_GET_DEVICE_DIAGNOSTICS = 0x40,
	NSM_GET_NETWORK_DEVICE_DEBUG_INFO = 0x50,
	NSM_ERASE_TRACE = 0x51,
	NSM_GET_NETWORK_DEVICE_LOG_INFO = 0x52,
	NSM_RESET_NETWORK_DEVICE = 0x53,
	NSM_ERASE_DEBUG_INFO = 0x59,
	NSM_ENABLE_DISABLE_WP = 0x65
};

enum diagnostics_enable_disable_wp_data_index {
	RETIMER_EEPROM = 128,
	BASEBOARD_FRU_EEPROM = 129,
	PEX_SW_EEPROM = 130,
	NVSW_EEPROM_BOTH = 131,
	NVSW_EEPROM_1 = 133,
	NVSW_EEPROM_2 = 134,
	GPU_1_4_SPI_FLASH = 160,
	GPU_5_8_SPI_FLASH = 161,
	GPU_SPI_FLASH_1 = 162,
	GPU_SPI_FLASH_2 = 163,
	GPU_SPI_FLASH_3 = 164,
	GPU_SPI_FLASH_4 = 165,
	GPU_SPI_FLASH_5 = 166,
	GPU_SPI_FLASH_6 = 167,
	GPU_SPI_FLASH_7 = 168,
	GPU_SPI_FLASH_8 = 169,
	GPU_SPI_FLASH = 170,
	HMC_SPI_FLASH = 176,
	CX8_SPI_FLASH = 183,
	RETIMER_EEPROM_1 = 192,
	RETIMER_EEPROM_2 = 193,
	RETIMER_EEPROM_3 = 194,
	RETIMER_EEPROM_4 = 195,
	RETIMER_EEPROM_5 = 196,
	RETIMER_EEPROM_6 = 197,
	RETIMER_EEPROM_7 = 198,
	RETIMER_EEPROM_8 = 199,
	CPU_SPI_FLASH_1 = 224,
	CPU_SPI_FLASH_2 = 225,
	CPU_SPI_FLASH_3 = 226,
	CPU_SPI_FLASH_4 = 227,
	CPU_SPI_FLASH_5 = 228,
	CPU_SPI_FLASH_6 = 229,
	CPU_SPI_FLASH_7 = 230,
	CPU_SPI_FLASH_8 = 231,
	CX7_FRU_EEPROM = 232,
	HMC_FRU_EEPROM = 233,
};

/** @brief Modes allowed on reset network device cmd
 */
enum reset_network_device_mode {
	START_AFTER_RESPONSE = 0,
	ALL_HOST_PERST_LOW = 1,
	ALL_HOST_PCIE_LINK_DISABLE = 2,
	ALLOWED_BY_ALL_HOST = 3
};

enum nsm_debug_information_type {
	INFO_TYPE_DEVICE_INFO = 0,
	INFO_TYPE_FW_RUNTIME_INFO = 1,
	INFO_TYPE_FW_SAVED_INFO = 2,
	INFO_TYPE_DEVICE_DUMP = 3
};

enum nsm_erase_information_type { INFO_TYPE_FW_SAVED_DUMP_INFO = 0 };

/** @struct nsm_get_device_diagnostics_req
 *
 *  Structure representing NSM get device diagnostics request.
 */
struct nsm_get_device_diagnostics_req {
	struct nsm_common_req hdr;
	uint8_t segment_id;
} __attribute__((packed));

/** @struct nsm_get_device_diagnostics_resp
 *
 *  Structure representing NSM get device diagnostics response.
 */
struct nsm_get_device_diagnostics_resp {
	struct nsm_common_resp hdr;
	uint8_t next_segment_id;
	uint8_t segment_data[1];
} __attribute__((packed));

enum nsm_debug_configuration_type {
	DEBUG_CONFIGURATION_TYPE_L1_POWER_DATA = 0,
};

enum nsm_debug_parameter_id_l1_power_data {
	NSM_DEBUG_PARAMETER_ID_MLPC =
	    0, // Management L1 Performance Counter Register
	NSM_DEBUG_PARAMETER_ID_PPSLC =
	    1, // Port Power Saving L1 Configuration Register
	NSM_DEBUG_PARAMETER_ID_PPSLS =
	    2, // Port Power Saving L1 Status Register
	NSM_DEBUG_PARAMETER_ID_PPSPI =
	    3, // Port Power Saving L1 Prediction Info Register
	NSM_DEBUG_PARAMETER_ID_PPSPHI =
	    4, // Port Power Saving L1 Prediction Histogram Info Register
	NSM_DEBUG_PARAMETER_ID_PPSPC =
	    5, // Port Power Saving L1 Prediction Config Register
	NSM_DEBUG_PARAMETER_ID_PPCNT = 6, // Ports Performance Counters Register
					  // (uses Counter Group sub ID)
	NSM_DEBUG_PARAMETER_ID_MPSCR =
	    7, // Management Power Saving L1 Configuration Register
	NSM_DEBUG_PARAMETER_ID_PPSLG = 8, // Port Power Saving L1 General
					  // Register (uses Page Select sub ID)
};

enum nsm_debug_parameter_sub_id_l1_power_data {
	NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CAPABILITIES_AND_STATUS =
	    0, // Page Select L1 Capabilities and Status
	NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CONFIGURATION =
	    1, // Page Select L1 Configuration
	NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_DEBUG =
	    2, // Page Select L1 Debug
	NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_CAPABILITIES_AND_STATUS =
	    3, // Page Select L0 Capabilities and Status
	NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_DEBUG =
	    4, // Page Select L0 Debug
	NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L0_GENERAL_COUNTERS =
	    0x1D, // Counter Group L0 General Counters
	NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_GENERAL_COUNTERS =
	    0x1E, // Counter Group L1 General Counters
	NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_STAT_COUNTERS =
	    0x27, // Counter Group L1 Stat Counters
};

struct nsm_debug_parameter_id {
	uint8_t reserved;
	uint16_t port_number;
	uint8_t index;
} __attribute__((packed));

typedef union {
	struct {
		uint32_t sub_id : 6;
		uint32_t reserved : 26;
	} __attribute__((packed)) bits;
	uint32_t value;
} nsm_debug_parameter_sub_id_bitfield;

struct nsm_get_device_debug_parameters_req {
	struct nsm_common_req_v2 hdr;
	uint8_t debug_configuration_type;
	uint8_t reserved[3];
	struct nsm_debug_parameter_id parameter_id;
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id;
} __attribute__((packed));

struct nsm_get_device_debug_parameters_resp {
	struct nsm_common_resp hdr;
	uint8_t data[1];
} __attribute__((packed));

struct nsm_set_device_debug_parameters_req {
	struct nsm_common_req_v2 hdr;
	uint8_t debug_configuration_type;
	uint8_t data_size;
	uint8_t reserved[2];
	struct nsm_debug_parameter_id parameter_id;
	nsm_debug_parameter_sub_id_bitfield parameter_sub_id;
	uint8_t data[1];
} __attribute__((packed));

enum nsm_erase_trace_status {
	ERASE_TRACE_NO_DATA_ERASED = 0,
	ERASE_TRACE_DATA_ERASED = 1,
	ERASE_TRACE_DATA_ERASE_INPROGRESS = 2
};

enum nsm_log_info_time_synced {
	SYNCED_TIME_TYPE_BOOT = 0,
	SYNCED_TIME_TYPE_SYNCED = 1
};

/** @struct nsm_reset_network_device_req
 *
 *  Structure representing NSM reset network device request.
 */
struct nsm_reset_network_device_req {
	struct nsm_common_req hdr;
	uint8_t mode;
} __attribute__((packed));

/** @struct nsm_reset_network_device_resp
 *
 *  Structure representing NSM reset network device response.
 */
typedef struct nsm_common_resp nsm_reset_network_device_resp;

/** @struct nsm_enable_disable_wp_req
 *
 *  Structure representing Diagnostics Enable/Disable WP request.
 */
struct nsm_enable_disable_wp_req {
	struct nsm_common_req hdr;
	uint8_t data_index;
	uint8_t value; // 0 - disable, 1 - enable
} __attribute__((packed));

/** @struct nsm_get_network_device_debug_info_req
 *
 *  Structure representing NSM get network device debug info request.
 */
struct nsm_get_network_device_debug_info_req {
	struct nsm_common_req hdr;
	uint8_t debug_info_type;
	uint8_t reserved;
	uint32_t record_handle;
} __attribute__((packed));

/** @struct nsm_get_network_device_debug_info_resp
 *
 *  Structure representing NSM get network device debug info response.
 */
struct nsm_get_network_device_debug_info_resp {
	struct nsm_common_resp hdr;
	uint32_t next_record_handle;
	uint8_t segment_data[1];
} __attribute__((packed));

/** @struct nsm_erase_trace_req
 *
 *  Structure representing NSM Erase trace request.
 */
struct nsm_erase_trace_req {
	struct nsm_common_req hdr;
} __attribute__((packed));

/** @struct nsm_erase_trace_resp
 *
 *  Structure representing NSM Erase trace response.
 */
struct nsm_erase_trace_resp {
	struct nsm_common_resp hdr;
	uint8_t result_status;
} __attribute__((packed));

/** @struct nsm_get_network_device_log_info_req
 *
 *  Structure representing NSM get network device log info request.
 */
struct nsm_get_network_device_log_info_req {
	struct nsm_common_req hdr;
	uint32_t record_handle;
} __attribute__((packed));

/** @struct nsm_get_network_device_debug_info_resp
 *
 *  Structure representing NSM get network device log info response.
 */

// TODO: Spec specifies 2 fields with NVU24 data type which is not supported as
// per spec, but with our understand it is NVU32 type which combines 2 fields
// one NVU24 and one NVU8. Have raised question on spec once we get confirmation
// we might have to update, and if things are as per our understanding then no
// change required. JIRA: https://jirasw.nvidia.com/browse/DGXOPENBMC-13373
struct nsm_device_log_info_breakdown {
	uint8_t lost_events : 4;
	uint8_t unused : 3;
	uint8_t synced_time : 1;

	uint8_t reserved1;
	uint16_t reserved2;

	uint32_t time_low;
	uint32_t time_high;

	uint32_t entry_prefix : 24;
	uint32_t length : 8;

	uint64_t entry_suffix;
} __attribute__((packed));

struct nsm_device_log_info {
	uint8_t lost_events_and_synced_time;
	uint8_t reserved1;
	uint16_t reserved2;
	uint32_t time_low;
	uint32_t time_high;
	uint32_t entry_prefix_and_length;
	uint64_t entry_suffix;
} __attribute__((packed));

struct nsm_get_network_device_log_info_resp {
	struct nsm_common_resp hdr;
	uint32_t next_record_handle;
	struct nsm_device_log_info log_info;
	uint8_t log_data[1];
} __attribute__((packed));

/** @struct nsm_erase_debug_info_req
 *
 *  Structure representing NSM Erase debug info request.
 */
struct nsm_erase_debug_info_req {
	struct nsm_common_req hdr;
	uint8_t debug_info_type;
	uint8_t reserved;
} __attribute__((packed));

/** @struct nsm_erase_debug_info_resp
 *
 *  Structure representing NSM Erase debug info response.
 */
struct nsm_erase_debug_info_resp {
	struct nsm_common_resp hdr;
	uint8_t result_status;
	uint8_t reserved;
} __attribute__((packed));

/** @struct nsm_boot_reason_type_breakdown
 *
 *  Structure representing NSM Boot Reason Type breakdown.
 */
struct nsm_boot_reason_type_breakdown {
	uint64_t wake_up : 1;
	uint64_t power_on : 1;
	uint64_t voltage_detect : 1;
	uint64_t reserved3 : 1;
	uint64_t warm_reset : 1;
	uint64_t fatal_error : 1;
	uint64_t reserved6 : 1;
	uint64_t reserved7 : 1;
	uint64_t pin : 1;
	uint64_t debug_access_port : 1;
	uint64_t reset_timeout : 1;
	uint64_t low_power_acknowledge_timeout : 1;
	uint64_t system_clock_generator : 1;
	uint64_t windowed_watchdog_0 : 1;
	uint64_t software : 1;
	uint64_t lockup_reset : 1;
	uint64_t cpu1 : 1;
	uint64_t reserved17to23 : 7;
	uint64_t vbat : 1;
	uint64_t windowed_watchdog_1 : 1;
	uint64_t code_watchdog_0 : 1;
	uint64_t code_watchdog_1 : 1;
	uint64_t jtag : 1;
	uint64_t reserved29 : 1;
	uint64_t security_violation : 1;
	uint64_t tamper : 1;
	uint64_t iaccviol : 1;
	uint64_t daccviol : 1;
	uint64_t reserved34 : 1;
	uint64_t munstkerr : 1;
	uint64_t mstkerr : 1;
	uint64_t reserved37to38 : 2;
	uint64_t mmfarvalid : 1;
	uint64_t bfarvalid : 1;
	uint64_t reserved41to42 : 2;
	uint64_t stkerr : 1;
	uint64_t unstkerr : 1;
	uint64_t imprecise_error : 1;
	uint64_t precise_error : 1;
	uint64_t ibuserr : 1;
	uint64_t undefinstr : 1;
	uint64_t invstate : 1;
	uint64_t invpc : 1;
	uint64_t nocp : 1;
	uint64_t reserved52to55 : 4;
	uint64_t unaligned : 1;
	uint64_t devbyzero : 1;
	uint64_t reserved58to64 : 7;
	uint64_t vecttbl : 1;
	uint64_t reserved66to93 : 28;
	uint64_t forced : 1;
	uint64_t debugevt : 1;
	uint64_t mctp : 1;
	uint64_t i2c : 1;
	uint64_t i3c : 1;
	uint64_t pldm : 1;
	uint64_t usb : 1;
	uint64_t flash : 1;
	uint64_t logger : 1;
	uint64_t spdm : 1;
	uint64_t reserved104to127 : 24;
	uint64_t reserved128to191 : 64;
	uint64_t reserved192to255 : 64;
} __attribute__((packed));

/** @brief Encode a Get device diagnostics request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] segment_id - segment ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_diagnostics_req(uint8_t instance_id, uint8_t segment_id,
				      struct nsm_msg *msg);

/** @brief Decode a Get device diagnostics request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] segment_id - segment ID
 *  @return nsm_completion_codes
 */
int decode_get_device_diagnostics_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *segment_id);

/** @brief Encode a Get device diagnostics response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_diagnostics_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code,
				       const uint8_t *seg_data,
				       const uint16_t seg_data_size,
				       const uint8_t next_segment_id,
				       struct nsm_msg *msg);

/** @brief Decode a Get device diagnostics response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - NSM reason code
 *  @param[out] seg_data - segment data
 *  @param[out] seg_data_size - segment data size
 *  @param[out] next_segment_id - next segment ID
 *  @return nsm_completion_codes
 */
int decode_get_device_diagnostics_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code, uint8_t *seg_data,
				       uint16_t *seg_data_size,
				       uint8_t *next_segment_id);

/**
 * @brief Encode a request message for the Get Device Reset Statistics command.
 *
 * This function encodes the request for the Get Device Reset Statistics
 * command, which retrieves statistics related to device resets. The request
 * message consists of only the header and does not require additional payload.
 *
 * @param[in] instance_id - NSM instance ID to specify the target instance.
 * @param[out] msg - Pointer to the message buffer where the encoded request
 * will be stored.
 *
 * @return nsm_completion_codes - Returns NSM_SW_SUCCESS on success, or an error
 * code (e.g., NSM_SW_ERROR_NULL, NSM_SW_ERROR_DATA) on failure.
 */
int encode_get_device_reset_statistics_req(uint8_t instance_id,
					   struct nsm_msg *msg);

/** @brief Encode a Diagnostics Enable/Disable WP request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] data_index - data index
 *  @param[in] value - Enable/Disable value
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_enable_disable_wp_req(
    uint8_t instance_id,
    enum diagnostics_enable_disable_wp_data_index data_index, uint8_t value,
    struct nsm_msg *msg);

/** @brief Encode Reset network device request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] mode - Mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_reset_network_device_req(uint8_t instance_id, uint8_t mode,
				    struct nsm_msg *msg);

/** @brief Decode Reset network device request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] mode - Mode
 *  @return nsm_completion_codes
 */
int decode_reset_network_device_req(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *mode);

/** @brief Encode a NSM Reset network device response message
 *
 *  @param[in] cc - NSM Completion Code
 *  @param[in] reason_code - Reason Code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_reset_network_device_resp(uint8_t instance, uint16_t reason_code,
				     struct nsm_msg *req);

/** @brief Decode a NSM Reset network device response message
 *
 *  @param[in] resp    - Response message
 *  @param[in] respLen - Length of response message
 *  @param[out] cc     - Completion Code
 *  @param[out] reason_code  - Reason Code
 *  @return nsm_completion_codes
 */
int decode_reset_network_device_resp(const struct nsm_msg *msg, size_t msgLen,
				     uint8_t *cc, uint16_t *reason_code);

/** @brief Decode a Diagnostics Enable/Disable WP request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] data_index - data index
 *  @param[out] value - Enable/Disable value
 *  @return nsm_completion_codes
 */
int decode_enable_disable_wp_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum diagnostics_enable_disable_wp_data_index *data_index, uint8_t *value);

/** @brief Encode a Diagnostics Enable/Disable WP response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_enable_disable_wp_resp(uint8_t instance_id, uint8_t cc,
				  uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a Diagnostics Enable/Disable WP response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_enable_disable_wp_resp(const struct nsm_msg *msg, size_t msg_len,
				  uint8_t *cc, uint16_t *reason_code);

/** @brief Encode a Get network device debug info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] debug_type - debug information type to fetch
 *  @param[in] handle - record handle
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_network_device_debug_info_req(uint8_t instance_id,
					     uint8_t debug_type,
					     uint32_t handle,
					     struct nsm_msg *msg);

/** @brief Decode a Get network device debug info request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[in] debug_type - debug information type to fetch
 *  @param[in] handle - record handle
 *  @return nsm_completion_codes
 */
int decode_get_network_device_debug_info_req(const struct nsm_msg *msg,
					     size_t msg_len,
					     uint8_t *debug_type,
					     uint32_t *handle);

/** @brief Encode a Get network device debug info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] seg_data - network device debug info segment
 *  @param[in] seg_data_size - segment size in bytes
 *  @param[in] next_handle - next record handle
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_network_device_debug_info_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      const uint8_t *seg_data,
					      const uint16_t seg_data_size,
					      const uint32_t next_handle,
					      struct nsm_msg *msg);

/** @brief Decode a Get network device debug info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] seg_data - data size in bytes
 *  @param[out] seg_data_size - network device debug info segement data
 *  @param[out] next_handle - next record handle
 *  @return nsm_completion_codes
 */
int decode_get_network_device_debug_info_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *reason_code,
					      uint16_t *seg_data_size,
					      uint8_t *seg_data,
					      uint32_t *next_handle);

/** @brief Encode a Erase Trace request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_erase_trace_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a Erase Trace request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_erase_trace_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a Erase Trace response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] result_status - erase trace result status
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_erase_trace_resp(uint8_t instance_id, uint8_t cc,
			    uint16_t reason_code, uint8_t result_status,
			    struct nsm_msg *msg);

/** @brief Decode a Erase Trace response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] result_status - erase trace result status
 *  @return nsm_completion_codes
 */
int decode_erase_trace_resp(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *cc, uint16_t *reason_code,
			    uint8_t *result_status);

/** @brief Encode a Get network device log info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] record_handle - record handle
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_network_device_log_info_req(uint8_t instance_id,
					   uint32_t record_handle,
					   struct nsm_msg *msg);

/** @brief Decode a Get network device log info request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] record_handle - record handle
 *  @return nsm_completion_codes
 */
int decode_get_network_device_log_info_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint32_t *record_handle);

/** @brief Encode a Get network device logs info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] seg_data - network device debug info segment
 *  @param[in] seg_data_size - segment size in bytes
 *  @param[in] next_handle - next record handle
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_network_device_log_info_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const uint32_t next_handle, struct nsm_device_log_info_breakdown log_info,
    const uint8_t *log_data, const uint16_t log_data_size, struct nsm_msg *msg);

/** @brief Decode a Get network device logs info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] seg_data - data size in bytes
 *  @param[out] seg_data_size - network device debug info segement data
 *  @param[out] next_handle - next record handle
 *  @return nsm_completion_codes
 */
int decode_get_network_device_log_info_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint32_t *next_handle,
    struct nsm_device_log_info_breakdown *log_info, uint8_t *log_data,
    uint16_t *log_data_size);

/** @brief Encode a Erase Debug Info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] info_type - Information type to erase
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_erase_debug_info_req(uint8_t instance_id, uint8_t info_type,
				struct nsm_msg *msg);

/** @brief Decode a Erase Debug Info request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] info_type - Information type to erase
 *  @return nsm_completion_codes
 */
int decode_erase_debug_info_req(const struct nsm_msg *msg, size_t msg_len,
				uint8_t *info_type);

/** @brief Encode a Erase Debug Info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] result_status - erase trace result status
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_erase_debug_info_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint8_t result_status,
				 struct nsm_msg *msg);

/** @brief Decode a Erase Debug Info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] result_status - erase trace result status
 *  @return nsm_completion_codes
 */
int decode_erase_debug_info_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint8_t *result_status);

/**
 * @brief Encodes a reset type enumeration into a byte array.
 *
 * @param[in] resetType - Reset type as uint8_t.
 * @param[out] data - Encoded byte array.
 * @param[out] data_len - Length of the encoded data.
 *
 * @return NSM_SW_SUCCESS on success, or appropriate error code.
 */
int encode_reset_enum_data(uint8_t resetType, uint8_t *data, size_t *data_len);

/**
 * @brief Decodes a reset type enumeration from a byte array.
 *
 * @param[in] data - Encoded byte array.
 * @param[in] data_len - Length of the encoded data.
 * @param[out] resetType - Decoded reset type as uint8_t.
 *
 * @return NSM_SW_SUCCESS on success, or appropriate error code.
 */
int decode_reset_enum_data(const uint8_t *data, size_t data_len,
			   uint8_t *resetType);

/**
 * @brief Encodes a reset count (e.g., ResetEntryCount) into a byte array.
 *
 * @param[in] count - Reset count as uint16_t.
 * @param[out] data - Encoded byte array.
 * @param[out] data_len - Length of the encoded data.
 *
 * @return NSM_SW_SUCCESS on success, or appropriate error code.
 */
int encode_reset_count_data(uint16_t count, uint8_t *data, size_t *data_len);

/**
 * @brief Decodes a reset count (e.g., ResetEntryCount) from a byte array.
 *
 * @param[in] data - Encoded byte array.
 * @param[in] data_len - Length of the encoded data.
 * @param[out] count - Decoded reset count as uint16_t.
 *
 * @return NSM_SW_SUCCESS on success, or appropriate error code.
 */
int decode_reset_count_data(const uint8_t *data, size_t data_len,
			    uint16_t *count);

/**
 * @brief Encodes a uint64 reset count into a byte array.
 *
 * @param[in] count - Reset count as uint64_t array.
 * @param[out] data - Encoded byte array.
 * @param[out] data_len - Length of the encoded data.
 *
 * @return NSM_SW_SUCCESS on success, or appropriate error code.
 */
int encode_reset_count_256data(const uint64_t *count, uint8_t *data,
			       size_t *data_len);

/**
 * @brief Decodes a uint64 reset count from a byte array.
 *
 * @param[in] data - Encoded byte array.
 * @param[in] data_len - Length of the encoded data.
 * @param[out] count - Decoded reset count as uint64_t.
 * @param[in] count_len - Length of the decoded count.
 *
 * @return NSM_SW_SUCCESS on success, or appropriate error code.
 */
int decode_reset_count_256data(const uint8_t *data, size_t data_len,
			       uint64_t *count, size_t count_len);

/**
 * @brief Decodes a "Get Device Reset Statistics" request message.
 *
 * Validates the message structure and ensures it adheres to the expected
 * format.
 *
 * @param[in] msg - Pointer to the NSM message structure.
 * @param[in] msg_len - Length of the NSM message.
 *
 * @return NSM_SW_SUCCESS on successful decoding.
 *         NSM_SW_ERROR_NULL if the msg pointer is null.
 *         NSM_SW_ERROR_LENGTH if the message length is invalid.
 *         NSM_SW_ERROR_DATA if the data size in the message is unexpected.
 */
int decode_get_device_reset_statistics_req(const struct nsm_msg *msg,
					   size_t msg_len);

/** @brief Encode a Get Device Debug Parameters request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] debug_configuration_type - Debug configuration type
 *  @param[in] parameter_id - Parameter ID
 *  @param[in] parameter_sub_id - Parameter sub ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_debug_parameters_req(
    uint8_t instance_id, uint8_t debug_configuration_type,
    struct nsm_debug_parameter_id parameter_id,
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id, struct nsm_msg *msg);

/** @brief Decode a Get Device Debug Parameters request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] debug_configuration_type - Debug configuration type
 *  @param[out] parameter_id - Parameter ID
 *  @param[out] parameter_sub_id - Parameter sub ID
 *  @return nsm_completion_codes
 */
int decode_get_device_debug_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    uint8_t *debug_configuration_type,
    struct nsm_debug_parameter_id *parameter_id,
    nsm_debug_parameter_sub_id_bitfield *parameter_sub_id);

/** @brief Encode a Get Device Debug Parameters response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data_size - Data size
 *  @param[in] data - Data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_debug_parameters_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    uint16_t *data_size, uint8_t *data,
					    struct nsm_msg *msg);

/** @brief Decode a Get Device Debug Parameters response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] data_size - Data size
 *  @param[out] data - Data
 *  @return nsm_completion_codes
 */
int decode_get_device_debug_parameters_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *data_size, uint8_t *data);

/** @brief Encode a Set Device Debug Parameters request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] debug_configuration_type - Debug configuration type
 *  @param[in] parameter_id - Parameter ID
 *  @param[in] parameter_sub_id - Parameter sub ID
 *  @param[in] data_size - Data size
 *  @param[in] data - Data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_debug_parameters_req(
    uint8_t instance_id, uint8_t debug_configuration_type,
    struct nsm_debug_parameter_id parameter_id,
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id, uint8_t data_size,
    uint8_t *data, struct nsm_msg *msg);

/** @brief Decode a Set Device Debug Parameters request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] debug_configuration_type - Debug configuration type
 *  @param[out] parameter_id - Parameter ID
 *  @param[out] parameter_sub_id - Parameter sub ID
 *  @param[out] data_size - Data size
 *  @param[out] data - Data
 *  @return nsm_completion_codes
 */
int decode_set_device_debug_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    uint8_t *debug_configuration_type,
    struct nsm_debug_parameter_id *parameter_id,
    nsm_debug_parameter_sub_id_bitfield *parameter_sub_id, uint8_t *data_size,
    uint8_t **data);

/** @brief Decode a Set Device Debug Parameters response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_set_device_debug_parameters_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc);

#ifdef __cplusplus
}
#endif
#endif