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

#ifndef DEVICE_CONFIGURATION_H
#define DEVICE_CONFIGURATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

enum device_configuration_command {
	NSM_SET_ERROR_INJECTION_MODE_V1 = 0x03,
	NSM_GET_ERROR_INJECTION_MODE_V1 = 0x04,
	NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1 = 0x05,
	NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1 = 0x06,
	NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1 = 0x07,
	NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1 = 0x08,
	NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1 = 0x09,
	NSM_SET_RECONFIGURATION_PERMISSIONS_V1 = 0x40,
	NSM_GET_RECONFIGURATION_PERMISSIONS_V1 = 0x41,
	NSM_ENABLE_DISABLE_GPU_IST_MODE = 0x62,
	NSM_GET_FPGA_DIAGNOSTICS_SETTINGS = 0x64,
	NSM_SET_EGM_MODE = 0x42,
	NSM_GET_EGM_MODE = 0x43,
};

enum error_injection_type {
	EI_MEMORY_ERRORS = 0,
	EI_PCI_ERRORS = 1,
	EI_NVLINK_ERRORS = 2,
	EI_THERMAL_ERRORS = 3,
};

enum fpga_diagnostics_settings_data_index {
	GET_WP_SETTINGS = 0x00,
	GET_PCIe_FUNDAMENTAL_RESET_STATE = 0x01,
	GET_WP_JUMPER_PRESENCE = 0x02,
	GET_GPU_DEGRADE_MODE_SETTINGS = 0x03,
	GET_GPU_IST_MODE_SETTINGS = 0x04,
	GET_POWER_SUPPLY_STATUS = 0x05,
	GET_BOARD_POWER_SUPPLY_STATUS = 0x06,
	GET_POWER_BRAKE_STATE = 0x07,
	GET_THERMAL_ALERT_STATE = 0x08,
	GET_NVSW_FLASH_PRESENT_SETTINGS = 0x09,
	GET_NVSW_FUSE_SRC_SETTINGS = 0x0A,
	GET_RETIMER_LTSSM_DUMP_MODE_SETTINGS = 0x0B,
	GET_GPU_PRESENCE = 0x0C,
	GET_GPU_POWER_STATUS = 0x0D,
	GET_AGGREGATE_TELEMETRY = 0xFF,
};

enum reconfiguration_permissions_v1_index {
	RP_IN_SYSTEM_TEST = 0,
	RP_FUSING_MODE = 1,
	RP_CONFIDENTIAL_COMPUTE = 2,
	RP_BAR0_FIREWALL = 3,
	RP_CONFIDENTIAL_COMPUTE_DEV_MODE = 4,
	RP_TOTAL_GPU_POWER_CURRENT_LIMIT = 5,
	RP_TOTAL_GPU_POWER_RATED_LIMIT = 6,
	RP_TOTAL_GPU_POWER_MAX_LIMIT = 7,
	RP_TOTAL_GPU_POWER_MIN_LIMIT = 8,
	RP_CLOCK_LIMIT = 9,
	RP_NVLINK_DISABLE = 10,
	RP_ECC_ENABLE = 11,
	RP_PCIE_VF_CONFIGURATION = 12,
	RP_ROW_REMAPPING_ALLOWED = 13,
	RP_ROW_REMAPPING_FEATURE = 14,
	RP_HBM_FREQUENCY_CHANGE = 15,
	RP_HULK_LICENSE_UPDATE = 16,
	RP_FORCE_TEST_COUPLING = 17,
	RP_BAR0_TYPE_CONFIG = 18,
	RP_EDPP_SCALING_FACTOR = 19,
	RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_1 = 20,
	RP_POWER_SMOOTHING_PRIVILEGE_LEVEL_2 = 21,
	RP_EGM_MODE = 22,
};

enum reconfiguration_permissions_v1_setting {
	RP_ONESHOOT_HOT_RESET = 0,
	RP_PERSISTENT = 1,
	RP_ONESHOT_FLR = 2,
};

#define ALL_GPUS_DEVICE_INDEX 0xA

/** @struct nsm_error_injection_mode_v1
 *
 * Structure representing Error Injection Mode v1 data.
 */
struct nsm_error_injection_mode_v1 {
	uint8_t mode; /*!< Global error injection mode knob.
    The following values are valid:
    0 – Error injection is disabled for device
    1 – Error injection is enabled for device
    */
	bitfield32_t flags;
} __attribute__((packed));

/** @struct nsm_set_error_injection_mode_v1_req
 *
 * Structure representing Set Error Injection Mode v1 request.
 */
struct nsm_set_error_injection_mode_v1_req {
	struct nsm_common_req hdr;
	uint8_t mode; /*!< Global error injection mode knob.
	   The following values are valid:
	   0 – Disable error injection for device
	   1 – Enable error injection for device
	   */
} __attribute__((packed));

/** @struct nsm_get_error_injection_mode_v1_resp
 *
 * Structure representing Get Error Injection Mode v1 response.
 */
struct nsm_get_error_injection_mode_v1_resp {
	struct nsm_common_resp hdr;
	struct nsm_error_injection_mode_v1 data;
} __attribute__((packed));

/** @struct nsm_error_injection_types_mask
 *
 * Structure representing Get Supported Error Injection Types v1 payload and Get
 * Current Error Injection Types v1 payload.
 */
struct nsm_error_injection_types_mask {
	uint8_t mask[8]; // Error injection types mask
} __attribute__((packed));

/** @struct nsm_set_error_injection_types_mask_req
 *
 * Structure representing Set Current Error Injection Types v1.
 */
struct nsm_set_error_injection_types_mask_req {
	struct nsm_common_req hdr;
	struct nsm_error_injection_types_mask data; /*!<
	Current Error Injection Types
    Each bit represents whether the given error injection type will be enabled.
    Bits values:
    0 – Disable error injection type
    1 – Enable error injection type
    */
} __attribute__((packed));

/** @struct nsm_get_error_injection_types_mask_resp
 *
 * Structure representing Get Supported Error Injection Types v1 and Get
 * Current Error Injection Types v1.
 */
struct nsm_get_error_injection_types_mask_resp {
	struct nsm_common_resp hdr;
	struct nsm_error_injection_types_mask data; /*!<
	Current Error Injection Types
	Each bit represents whether the given error injection type is enabled.
	Bits values:
	0 – Error injection type is disabled
	1 – Error injection type is enabled
	*/
} __attribute__((packed));

/** @struct nsm_get_fpga_diagnostics_settings_req
 *
 *  Structure representing Get FPGA Diagnostics Settings request.
 */
struct nsm_get_fpga_diagnostics_settings_req {
	struct nsm_common_req hdr;
	uint8_t data_index;
} __attribute__((packed));

/** @struct nsm_get_fpga_diagnostics_settings_resp
 *
 *  Structure representing Get FPGA Diagnostics Settings response.
 */
struct nsm_get_fpga_diagnostics_settings_resp {
	struct nsm_common_resp hdr;
	uint8_t data[1];
} __attribute__((packed));

/** @struct Get FPGA Diagnostics for Get WP Settings
 *
 *  Structure representing Get FPGA Diagnostics for Get WP Settings.
 */
struct nsm_fpga_diagnostics_settings_wp {
	/* Byte 0 */
	uint8_t retimer : 1;   // 0 – Any Retimer
	uint8_t baseboard : 1; // 1 – FRU EEPROM (Baseboard or CX7 or HMC)
	uint8_t pex : 1;       // 2 – PEXSW EROT
	uint8_t nvSwitch : 1;  // 3 – Any NVSW EROT
	uint8_t res1 : 3;      // 4:6 – Reserved
	uint8_t gpu1_4 : 1;    // Bit 7 – GPU 1-4 SPI Flash
	/* Byte 1 */
	uint8_t gpu5_8 : 1; // GPU 5-8 SPI Flash
	uint8_t cpu1_4 : 1; // CPU 1-4 SPI Flash
	uint8_t res2 : 6;   // 2:7 – Reserved
	/* Byte 2 */
	uint8_t retimer1 : 1; // Byte 2: Retimers (one per bit)
	uint8_t retimer2 : 1;
	uint8_t retimer3 : 1;
	uint8_t retimer4 : 1;
	uint8_t retimer5 : 1;
	uint8_t retimer6 : 1;
	uint8_t retimer7 : 1;
	uint8_t retimer8 : 1;
	/* Byte 3 */
	uint8_t nvSwitch1 : 1; // 0 - NVSW
	uint8_t nvSwitch2 : 1; // 1 - NVSW
	uint8_t res3 : 2;      // 2:3 Reserved
	uint8_t gpu1 : 1;      // 4:7 – GPU 1-4 SPI Flash (one per bit)
	uint8_t gpu2 : 1;
	uint8_t gpu3 : 1;
	uint8_t gpu4 : 1;
	/* Byte 4 */
	uint8_t gpu5 : 1; // 0:3 – GPU 5-8 SPI Flash (one per bit)
	uint8_t gpu6 : 1;
	uint8_t gpu7 : 1;
	uint8_t gpu8 : 1;
	uint8_t hmc : 1;  // 4 - HMC SPI Flash
	uint8_t cpu1 : 1; // 5:7 - CPU 1-3 SPI Flash (one per bit)
	uint8_t cpu2 : 1;
	uint8_t cpu3 : 1;
	/* Bytes 5 */
	uint8_t cpu4 : 1; // 0 - CPU 4 SPI Flash
	uint8_t res5 : 7;
	/* Bytes 6-7 reserved */
	uint8_t res6;
	uint8_t res7;

} __attribute__((packed));

/** @struct nsm_fpga_diagnostics_settings_wp_resp
 *
 *  Structure representing Get FPGA Diagnostics response for Get WP Settings.
 */
struct nsm_fpga_diagnostics_settings_wp_resp {
	struct nsm_common_resp hdr;
	struct nsm_fpga_diagnostics_settings_wp data;
} __attribute__((packed));

/** @struct Get FPGA Diagnostics Settings for Get WP Jumper
 *
 *  Structure representing Get FPGA Diagnostics Settings for Get WP Jumper.
 */
struct nsm_fpga_diagnostics_settings_wp_jumper {
	uint8_t presence : 1; // 0 – WP jumper presence
	uint8_t reserved : 7; // 1:7 – reserved
} __attribute__((packed));

/** @struct nsm_fpga_diagnostics_settings_wp_jumper_resp
 *
 *  Structure representing Get FPGA Diagnostics Settings response for Get WP
 * Jumper.
 */
struct nsm_fpga_diagnostics_settings_wp_jumper_resp {
	struct nsm_common_resp hdr;
	struct nsm_fpga_diagnostics_settings_wp_jumper data;
} __attribute__((packed));

/** @struct nsm_get_power_supply_status_resp
 *
 *  Structure representing NSM get power supply status information response.
 */
struct nsm_get_power_supply_status_resp {
	struct nsm_common_resp hdr;
	uint8_t power_supply_status;
} __attribute__((packed));

/** @struct nsm_get_gpu_presence_resp
 *
 *  Structure representing NSM get GPU presence information
 * response.
 */
struct nsm_get_gpu_presence_resp {
	struct nsm_common_resp hdr;
	uint8_t presence;
} __attribute__((packed));

/** @struct nsm_get_gpu_power_status_resp
 *
 *  Structure representing NSM get GPU power status information
 * response.
 */
struct nsm_get_gpu_power_status_resp {
	struct nsm_common_resp hdr;
	uint8_t power_status;
} __attribute__((packed));

/** @struct nsm_get_gpu_ist_mode_resp
 *
 *  Structure representing Get FPGA Diagnostics Settings response for GPU IST
 * Mode response
 */
struct nsm_get_gpu_ist_mode_resp {
	struct nsm_common_resp hdr;
	uint8_t mode; // 7:0 – setting per GPU
} __attribute__((packed));

/** @struct nsm_enable_disable_gpu_ist_mode_req
 *
 *  Structure representing Enable/Disable GPU IST Mode request.
 */
struct nsm_enable_disable_gpu_ist_mode_req {
	struct nsm_common_req hdr;
	uint8_t device_index; // 0-7: select GPU, 0xA all GPUs
	uint8_t value;	      // 0 - disable, 1 - enable
} __attribute__((packed));

/** @struct Get Reconfiguration Permissions v1 data
 *
 *  Structure representing Get Reconfiguration Permissions v1 data.
 */
struct nsm_reconfiguration_permissions_v1 {
	// 0 - Allow oneshot configuration of feature by host SW
	uint8_t oneshot : 1;
	// 1 - Allow persistent configuration of feature by host SW
	uint8_t persistent : 1;
	// 2 - Allow FLR persistent configuration of this feature by host SW
	uint8_t flr_persistent : 1;
	// 3:7 – reserved
	uint8_t reserved : 5;
} __attribute__((packed));

/** @struct nsm_get_reconfiguration_permissions_v1_req
 *
 *  Structure representing Get Reconfiguration Permissions v1 request.
 */
struct nsm_get_reconfiguration_permissions_v1_req {
	struct nsm_common_req hdr;
	uint8_t setting_index;
} __attribute__((packed));

/** @struct nsm_get_reconfiguration_permissions_v1_resp
 *
 *  Structure representing Get Reconfiguration Permissions v1 response.
 */
struct nsm_get_reconfiguration_permissions_v1_resp {
	struct nsm_common_resp hdr;
	struct nsm_reconfiguration_permissions_v1 data;
} __attribute__((packed));

/** @struct nsm_set_reconfiguration_permissions_v1_req
 *
 *  Structure representing Set Reconfiguration Permissions v1 request.
 */
struct nsm_set_reconfiguration_permissions_v1_req {
	struct nsm_common_req hdr;
	uint8_t setting_index;
	uint8_t configuration;
	uint8_t permission;
} __attribute__((packed));

enum compute_mode {
	NO_MODE = 0,
	PRODUCTION_MODE = 1,
	DEVTOOLS_MODE = 2,
};

/** @struct nsm_get_confidential_compute_mode_v1_resp
 *
 *  Structure representing Get Confidential Compute Mode v1 response.
 */
struct nsm_get_confidential_compute_mode_v1_resp {
	struct nsm_common_resp hdr;
	uint8_t current_mode;
	uint8_t pending_mode;
} __attribute__((packed));

/** @struct nsm_set_confidential_compute_mode_v1_req
 *
 *  Structure representing Set Confidential Compute Mode v1 request.
 */
struct nsm_set_confidential_compute_mode_v1_req {
	struct nsm_common_req hdr;
	uint8_t mode;
} __attribute__((packed));

/** @struct nsm_get_EGM_reading_resp
 *
 *  Structure representing NSM get EGM Mode response.
 */
struct nsm_get_EGM_mode_resp {
	struct nsm_common_resp hdr;
	bitfield8_t flags;
} __attribute__((packed));

/** @struct nsm_set_EGM_mode_req
 *
 *  Structure representing NSM set EGM Mode request.
 */
struct nsm_set_EGM_mode_req {
	struct nsm_common_req hdr;
	uint8_t requested_mode;
} __attribute__((packed));

/** @brief Encode a Set Error Injection Mode v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] mode - Error injection mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_error_injection_mode_v1_req(uint8_t instance_id,
					   const uint8_t mode,
					   struct nsm_msg *msg);

/** @brief Decode a Set Error Injection Mode v1 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] mode  - pointer to error injection mode
 *  @return nsm_completion_codes
 */
int decode_set_error_injection_mode_v1_req(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *mode);

/** @brief Encode a Set Error Injection Mode v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_error_injection_mode_v1_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg);

/** @brief Decode a Set Error Injection Mode v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_error_injection_mode_v1_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code);

/** @brief Encode a Get Error Injection Mode v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_error_injection_mode_v1_req(uint8_t instance_id,
					   struct nsm_msg *msg);

/** @brief Decode a Get Error Injection Mode v1 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_error_injection_mode_v1_req(const struct nsm_msg *msg,
					   size_t msg_len);

/** @brief Encode a Get Error Injection Mode v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - pointer to error injection mode data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_error_injection_mode_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_error_injection_mode_v1 *data, struct nsm_msg *msg);

/** @brief Decode a Get Error Injection Mode v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @param[out] data  - pointer to error injection mode data
 *  @return nsm_completion_codes
 */
int decode_get_error_injection_mode_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_error_injection_mode_v1 *data);

/** @brief Encode a Set Current Error Injection Types v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] data - pointer to supported error injection types data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_current_error_injection_types_v1_req(
    uint8_t instance_id, const struct nsm_error_injection_types_mask *data,
    struct nsm_msg *msg);

/** @brief Decode a Set Current Error Injection Types v1
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] data  - pointer to supported error injection types data
 *  @return nsm_completion_codes
 */
int decode_set_current_error_injection_types_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_error_injection_types_mask *data);

/** @brief Encode a Set Current Error Injection Types v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_current_error_injection_types_v1_resp(uint8_t instance_id,
						     uint8_t cc,
						     uint16_t reason_code,
						     struct nsm_msg *msg);

/** @brief Decode a Set Supported Error Injection Types v1 response message Set
 * Current Error Injection Types v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_current_error_injection_types_v1_resp(const struct nsm_msg *msg,
						     size_t msg_len,
						     uint8_t *cc,
						     uint16_t *reason_code);

/** @brief Encode a Get Supported Error Injection Types v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_error_injection_types_v1_req(uint8_t instance_id,
						      struct nsm_msg *msg);

/** @brief Encode a Get Current Error Injection Types v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_error_injection_types_v1_req(uint8_t instance_id,
						    struct nsm_msg *msg);

/** @brief Decode a Get Supported Error Injection Types v1 request message and
 * Get Current Error Injection Types v1
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_error_injection_types_v1_req(const struct nsm_msg *msg,
					    size_t msg_len);

/** @brief Encode a Get Supported Error Injection Types v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - pointer to supported error injection types data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_error_injection_types_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_error_injection_types_mask *data, struct nsm_msg *msg);

/** @brief Encode a Get Current Error Injection Types v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - pointer to supported error injection types data
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_error_injection_types_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const struct nsm_error_injection_types_mask *data, struct nsm_msg *msg);

/** @brief Decode a Get Supported Error Injection Types v1 response message Get
 * Current Error Injection Types v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @param[out] data  - pointer to supported error injection types data
 *  @return nsm_completion_codes
 */
int decode_get_error_injection_types_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_error_injection_types_mask *data);

/** @brief Encode a Get FPGA Diagnostics Settings request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] data_index - data index
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_fpga_diagnostics_settings_req(
    uint8_t instance_id, enum fpga_diagnostics_settings_data_index data_index,
    struct nsm_msg *msg);

/** @brief Decode a Get FPGA Diagnostics Settings request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] data_index - data index
 *  @return nsm_completion_codes
 */
int decode_get_fpga_diagnostics_settings_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum fpga_diagnostics_settings_data_index *data_index);

/** @brief Encode a Get FPGA Diagnostics Settings response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] flags - bits indicating row remap state flags
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_fpga_diagnostics_settings_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      const uint16_t data_size,
					      uint8_t *data,
					      struct nsm_msg *msg);

/** @brief Decode a Get FPGA Diagnostics Settings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] data_size - data size
 *  @param[out] data  - pointer to the array of data
 *  @return nsm_completion_codes
 */
int decode_get_fpga_diagnostics_settings_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *data_size,
					      uint16_t *reason_code,
					      uint8_t *data);

/** @brief Encode a Get FPGA Diagnostics Settings response msg for
 * Get WP Settings
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer Get WP Settings data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_get_fpga_diagnostics_settings_wp_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_fpga_diagnostics_settings_wp *data, struct nsm_msg *msg);

/** @brief Decode a Get FPGA Diagnostics Settings response msg for
 * Get WP Settings
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer Get WP Settings data source
 * @return nsm_completion_codes
 */

int decode_get_fpga_diagnostics_settings_wp_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_fpga_diagnostics_settings_wp *data);

/** @brief Encode a Get FPGA Diagnostics Settings response msg for
 * Get WP Jumper
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer Get WP Jumper data source
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_get_fpga_diagnostics_settings_wp_jumper_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_fpga_diagnostics_settings_wp_jumper *data, struct nsm_msg *msg);

/** @brief Decode a Get FPGA Diagnostics Settings response msg for
 * Get WP Jumper
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer Get WP Jumper data source
 * @return nsm_completion_codes
 */
int decode_get_fpga_diagnostics_settings_wp_jumper_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_fpga_diagnostics_settings_wp_jumper *data);

/** @brief Encode a Get power supply status response message
 *  *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] power_supply_status - GPUs power supply status
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_power_supply_status_resp(uint8_t instance_id, uint8_t cc,
					uint16_t reason_code,
					uint8_t power_supply_status,
					struct nsm_msg *msg);

/** @brief Decode a Get power supply status response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] power_supply_status - pointer to GPUs power supply status
 *  @return nsm_completion_codes
 */
int decode_get_power_supply_status_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code,
					uint8_t *power_supply_status);

/** @brief Encode a Get GPUs presence response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] presence - GPUs presence
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_gpu_presence_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint8_t presence,
				 struct nsm_msg *msg);

/** @brief Decode a Get GPUs presence response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] presence - pointer to GPUs presence
 *  @return nsm_completion_codes
 */
int decode_get_gpu_presence_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint8_t *presence);

/** @brief Encode a Get GPUs power status response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] power_status - GPUs power status
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_gpu_power_status_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, uint8_t power_status,
				     struct nsm_msg *msg);

/** @brief Decode a Get GPUs power status response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code     - pointer to reason code
 *  @param[out] power_status - pointer to GPUs power status
 *  @return nsm_completion_codes
 */
int decode_get_gpu_power_status_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code,
				     uint8_t *power_status);

/** @brief Encode a Get GPU IST Mode Settings response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] mode - GPU IST Mode Settings 7:0 – setting per GPU
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_get_gpu_ist_mode_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint8_t mode,
				 struct nsm_msg *msg);

/** @brief Decode a Get GPU IST Mode Settings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] mode  - GPU IST Mode Settings 7:0 – setting per GPU
 * @return nsm_completion_codes
 */
int decode_get_gpu_ist_mode_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint8_t *mode);

/** @brief Encode a Enable/Disable GPU IST Mode Settings request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_index - Device index 0-7: select GPU, 0xA all GPUs
 *  @param[in] value - 0 = disable, 1 = enable
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_enable_disable_gpu_ist_mode_req(uint8_t instance_id,
					   uint8_t device_index, uint8_t value,
					   struct nsm_msg *msg);

/** @brief Decode a Enable/Disable GPU IST Mode Settings request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_index - Device index 0-7: select GPU, 0xA all GPUs
 *  @param[out] value - 0 = disable, 1 = enable
 *  @return nsm_completion_codes
 */
int decode_enable_disable_gpu_ist_mode_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint8_t *device_index,
					   uint8_t *value);

/** @brief Encode a Enable/Disable GPU IST Mode Settings response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_enable_disable_gpu_ist_mode_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg);

/** @brief Decode a Enable/Disable GPU IST Mode Settings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 * @return nsm_completion_codes
 */
int decode_enable_disable_gpu_ist_mode_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code);

/** @brief Encode a Get Reconfiguration Permissions v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] setting_index - setting index
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_reconfiguration_permissions_v1_req(
    uint8_t instance_id,
    enum reconfiguration_permissions_v1_index setting_index,
    struct nsm_msg *msg);

/** @brief Decode a Get Reconfiguration Permissions v1 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] setting_index - setting index
 *  @return nsm_completion_codes
 */
int decode_get_reconfiguration_permissions_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum reconfiguration_permissions_v1_index *setting_index);

/** @brief Encode a Get Reconfiguration Permissions v1 response
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] data - struct pointer with data source
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_reconfiguration_permissions_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_reconfiguration_permissions_v1 *data, struct nsm_msg *msg);

/** @brief Decode a Get Reconfiguration Permissions v1 response
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] data  - struct pointer with data source
 *  @return nsm_completion_codes
 */
int decode_get_reconfiguration_permissions_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_reconfiguration_permissions_v1 *data);

/** @brief Encode a Set Reconfiguration Permissions v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] setting_index - Setting ID
 *  @param[in] configuration - Configuration
 *  @param[in] permission - Permission - 0:Disallow - 1:Allow
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_reconfiguration_permissions_v1_req(
    uint8_t instance_id,
    enum reconfiguration_permissions_v1_index setting_index,
    enum reconfiguration_permissions_v1_setting configuration,
    uint8_t permission, struct nsm_msg *msg);

/** @brief Decode a Set Reconfiguration Permissions v1 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] setting_index - Setting ID
 *  @param[out] configuration - Configuration
 *  @param[out] permission - Permission - 0:Disallow - 1:Allow
 *  @return nsm_completion_codes
 */
int decode_set_reconfiguration_permissions_v1_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum reconfiguration_permissions_v1_index *setting_index,
    enum reconfiguration_permissions_v1_setting *configuration,
    uint8_t *permission);

/** @brief Encode a Set Reconfiguration Permissions v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_set_reconfiguration_permissions_v1_resp(uint8_t instance_id,
						   uint8_t cc,
						   uint16_t reason_code,
						   struct nsm_msg *msg);

/** @brief Decode a Set Reconfiguration Permissions v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 * @return nsm_completion_codes
 */
int decode_set_reconfiguration_permissions_v1_resp(const struct nsm_msg *msg,
						   size_t msg_len, uint8_t *cc,
						   uint16_t *reason_code);

/** @brief Encode a Get Confidential Compute Mode v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_confidential_compute_mode_v1_req(uint8_t instance_id,
						struct nsm_msg *msg);

/** @brief Decode a Get Confidential Compute Mode v1 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_confidential_compute_mode_v1_req(const struct nsm_msg *msg,
						size_t msg_len);

/** @brief Encode a Get Confidential Compute Mode v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] current_mode - Current Confidential Compute modes
 *  @param[in] pending_mode - Pending Confidential Compute modes
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_get_confidential_compute_mode_v1_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code, uint8_t current_mode,
    uint8_t pending_mode, struct nsm_msg *msg);

/** @brief Decode a Get Confidential Compute Mode v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] current_mode - current confidential compute mode
 *  @param[in] pending_mode - pending confidential compute mode
 * @return nsm_completion_codes
 */
int decode_get_confidential_compute_mode_v1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint8_t *current_mode, uint8_t *pending_mode);

/** @brief Encode a Set Confidential Compute Mode v1 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] mode - 0 : Confidential Compute DevTools Mode, 1 : Confidential
 * Compute Mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_confidential_compute_mode_v1_req(uint8_t instance_id,
						uint8_t mode,
						struct nsm_msg *msg);

/** @brief Decode a Set Confidential Compute Mode v1 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] mode - 0 : Confidential Compute DevTools Mode, 1 : Confidential
 *  @return nsm_completion_codes
 */
int decode_set_confidential_compute_mode_v1_req(const struct nsm_msg *msg,
						size_t msg_len, uint8_t *mode);

/** @brief Encode a Set Confidential Compute Mode v1 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_set_confidential_compute_mode_v1_resp(uint8_t instance_id,
						 uint8_t cc,
						 uint16_t reason_code,
						 struct nsm_msg *msg);

/** @brief Decode a Set Confidential Compute Mode v1 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 * @return nsm_completion_codes
 */
int decode_set_confidential_compute_mode_v1_resp(const struct nsm_msg *msg,
						 size_t msg_len, uint8_t *cc,
						 uint16_t *data_size,
						 uint16_t *reason_code);

/** @brief Encode a Get EGM mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_EGM_mode_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a Get EGM mode request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_EGM_mode_req(const struct nsm_msg *msg, size_t msg_len);

/** @brief Encode a Get EGM mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] flags - bits indicating EGM modes
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_EGM_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, bitfield8_t *flags,
			     struct nsm_msg *msg);

/** @brief Decode a Get EGM mode response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[in] flags - bits indicating EGM modes
 *  @return nsm_completion_codes
 */
int decode_get_EGM_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code, bitfield8_t *flags);

/** @brief Encode a Set EGM mode request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] requested_mode - Requested Mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_EGM_mode_req(uint8_t instance, uint8_t requested_mode,
			    struct nsm_msg *msg);

/** @brief Decode a Set EGM mode request request message
 *
 *  @param[in] msg     - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] requested_mode - Requested Mode
 *  @return nsm_completion_codes
 */
int decode_set_EGM_mode_req(const struct nsm_msg *msg, size_t msg_len,
			    uint8_t *requested_mode);

/** @brief Encode a Get EGM mode response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_EGM_mode_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a Get EGM mode response message
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @return nsm_completion_codes
 */
int decode_set_EGM_mode_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *data_size,
			     uint16_t *reason_code);

#ifdef __cplusplus
}
#endif
#endif
