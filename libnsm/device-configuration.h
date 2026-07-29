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

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

enum device_configuration_command {
	NSM_SET_PROTECTION_OPTIONS = 0x01,
	NSM_GET_PROTECTION_OPTIONS = 0x02,
	NSM_SET_ERROR_INJECTION_MODE_V1 = 0x03,
	NSM_GET_ERROR_INJECTION_MODE_V1 = 0x04,
	NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1 = 0x05,
	NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1 = 0x06,
	NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1 = 0x07,
	NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1 = 0x08,
	NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1 = 0x09,
	NSM_GET_ERROR_INJECTION_PAYLOAD = 0x0A,
	NSM_SET_ERROR_INJECTION_PAYLOAD = 0x0B,
	NSM_ACTIVATE_ERROR_INJECTION = 0x0C,
	NSM_SET_DEVICE_CONFIG_V2 = 0x10,
	NSM_GET_DEVICE_CONFIG_V2 = 0x11,
	NSM_SET_RECONFIGURATION_PERMISSIONS_V1 = 0x40,
	NSM_GET_RECONFIGURATION_PERMISSIONS_V1 = 0x41,
	NSM_ENABLE_DISABLE_GPU_IST_MODE = 0x62,
	NSM_GET_FPGA_DIAGNOSTICS_SETTINGS = 0x64,
	NSM_SET_EGM_MODE = 0x42,
	NSM_GET_EGM_MODE = 0x43,
	NSM_SET_DEVICE_MODE_SETTING = 0x80,
	NSM_GET_DEVICE_MODE_SETTING = 0x81,
	NSM_SET_DEVICE_MODE_SETTINGS_V2 = 0x82,
	NSM_GET_DEVICE_MODE_SETTINGS_V2 = 0x83,
	NSM_GET_SUPPORTED_DEVICE_MODES_V2 = 0x84,
};

/** NSM Type 5 (Device Configuration) event IDs — Fractal Boot (NVBugs 5919843).
 */
enum nsm_device_configuration_event_id {
	NSM_DEVICE_CONFIGURATION_REQUEST_EVENT_V1 = 0x01,
};

enum protection_mode {
	PROTECTION_NONE = 0,
	PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG = 1,
	PROTECTION_PREVENT_FW_UPDATE = 2,
	PROTECTION_PREVENT_CONFIG = 3,
};

enum error_injection_type {
	EI_MEMORY_ERRORS = 0,
	EI_PCI_ERRORS = 1,
	EI_NVLINK_ERRORS = 2,
	EI_THERMAL_ERRORS = 3,
	EI_DEVICE_ERRORS = 4,
	EI_GPIO_SPOOFING = 5
};

enum error_injection_device_errorsubtype {
	EI_DEVICE_ERRORS_SUBTYPE_FATAL = 0,
	EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY = 1,
	EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION = 2,
	EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT = 3
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
	RP_INFOROM_RECREATE_ALLOW_INB = 23,
	RP_RUNTIME_IN_SYSTEM_TEST = 24,
	RP_DUAL_PART_NUMBERS = 25,
};

enum reconfiguration_permissions_v1_setting {
	RP_ONESHOOT_HOT_RESET = 0,
	RP_PERSISTENT = 1,
	RP_ONESHOT_FLR = 2,
};

enum roconfiguration_permission {
	DISALLOW_HOST_DISALLOW_DOE = 0,
	ALLOW_HOST_DISALLOW_DOE = 1,
	DISALLOW_HOST_ALLOW_DOE = 2,
	ALLOW_HOST_ALLOW_DOE = 3,
};

#define ALL_GPUS_DEVICE_INDEX 0xA

/** @struct nsm_set_protection_options_req
 *
 * Structure representing Set Protection Options request.
 */
struct nsm_set_protection_options_req {
	struct nsm_common_req hdr;
	uint8_t protection_mode;
} __attribute__((packed));

/** @struct nsm_get_protection_options_resp
 *
 * Structure representing Get Protection Options response.
 */
struct nsm_get_protection_options_resp {
	struct nsm_common_resp hdr;
	uint8_t protection_mode;
} __attribute__((packed));

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

/** @struct nsm_error_injection_id
 *
 * Structure representing Error Injection id.
 */
struct nsm_error_injection_id {
	uint32_t error_injection_type : 16;
	uint32_t error_injection_subtype : 16;
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

/** @struct nsm_error_injection_fatal_payload
 *
 * Structure representing Error Injection fatal payload data.
 */
struct nsm_error_injection_fatal_payload {
	uint16_t error_injection_subtype;
	uint32_t payload_bitmap;
} __attribute__((packed));

/** @struct nsm_error_injection_usb_emu_payload
 *
 * Structure representing Error Injection USB emulation payload data.
 */
struct nsm_error_injection_usb_emu_payload {
	uint16_t error_injection_subtype;
	uint8_t payload;
	uint8_t bus_number;
	uint8_t error;
	uint8_t address;
} __attribute__((packed));

/** @struct nsm_error_injection_port_recovery_payload
 *
 * Structure representing Error Injection port recovery payload data.
 */
struct nsm_error_injection_port_recovery_payload {
	uint16_t error_injection_subtype;
	uint8_t l1_payload;
	uint8_t l2_payload;
	uint8_t l3_payload;
	uint8_t reserved;
} __attribute__((packed));

/** @struct nsm_error_injection_leak_payload
 *
 * Structure representing Error Injection leak payload data.
 */
struct nsm_error_injection_leak_payload {
	uint16_t error_injection_subtype;
	uint16_t number_of_sensors;
	uint16_t sensors_data[1];
} __attribute__((packed));

/** @struct nsm_error_injection_gpio_spoofing_payload
 *
 * Structure representing Error Injection GPIO spoofing payload data.
 */
struct nsm_error_injection_gpio_spoofing_payload {
	uint16_t count_of_gpio;
	uint16_t gpio_data[1];
} __attribute__((packed));

/** @struct nsm_set_error_injection_payload_req
 *
 * Structure representing Set Error Injection payload request.
 */
struct nsm_set_error_injection_payload_req {
	struct nsm_common_req_v2 hdr;
	uint32_t offset;
	uint16_t error_injection_type;
	uint8_t fault_payload[1];
} __attribute__((packed));

/** @struct nsm_get_error_injection_payload_req
 *
 * Structure representing Get Error Injection payload request.
 */
struct nsm_get_error_injection_payload_req {
	struct nsm_common_req_v2 hdr;
	uint32_t error_injection_id;
} __attribute__((packed));

/** @struct nsm_get_error_injection_payload_resp
 *
 * Structure representing Get Error Injection payload response.
 */
struct nsm_get_error_injection_payload_resp {
	struct nsm_common_resp hdr;
	uint32_t offset;
	uint16_t error_injection_type;
	uint8_t fault_payload[1];
} __attribute__((packed));

/** @struct nsm_activate_error_injection_payload_req
 *
 * Structure representing Activate Error Injection payload request.
 */
struct nsm_activate_error_injection_payload_req {
	struct nsm_common_req_v2 hdr;
	uint32_t error_injection_id;
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
	uint8_t cx8 : 1;       // 4 – All CX8 SPI Flash
	uint8_t res1 : 2;      // 5:6 – Reserved
	uint8_t gpu1_4 : 1;    // Bit 7 – GPU 1-4 SPI Flash
	/* Byte 1 */
	uint8_t gpu5_8 : 1;   // 0 – GPU 5-8 SPI Flash
	uint8_t cpu1_4 : 1;   // 1 – CPU 1-4 SPI Flash
	uint8_t gpu9_12 : 1;  // 2 – GPU 9-12 SPI Flash
	uint8_t gpu13_16 : 1; // 3 – GPU 13-16 SPI Flash
	uint8_t res2 : 4;     // 4:7 – Reserved
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
	/* Byte 5 */
	uint8_t cpu4 : 1; // 0 - CPU 4 SPI Flash
	uint8_t res5 : 7;
	/* Byte 6 */
	uint8_t gpu9 : 1; // Byte 6: GPU 9-16 SPI Flash (one per bit)
	uint8_t gpu10 : 1;
	uint8_t gpu11 : 1;
	uint8_t gpu12 : 1;
	uint8_t gpu13 : 1;
	uint8_t gpu14 : 1;
	uint8_t gpu15 : 1;
	uint8_t gpu16 : 1;
	/* Byte 7 reserved */
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
	uint8_t host_oneshot : 1;
	// 1 - Allow persistent configuration of feature by host SW
	uint8_t host_persistent : 1;
	// 2 - Allow FLR persistent configuration of this feature by host SW
	uint8_t host_flr_persistent : 1;
	// 3 - Allow oneshot configuration of feature by Data Object Exchange
	uint8_t DOE_oneshot : 1;
	// 4 - Allow persistent configuration of feature by Data Object Exchange
	uint8_t DOE_persistent : 1;
	// 5 - Allow FLR persistent configuration of this feature by Data Object
	// Exchange
	uint8_t DOE_flr_persistent : 1;

	// 6:7 – reserved
	uint8_t reserved : 2;
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

enum nsm_l1_prediction_mode_config { DISABLED = 0, ENABLED = 1 };

enum device_mode_index {
	DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE = 7,
	DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED = 8,
	DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX = 9,
	DEVICE_MODE_SOC_POWER_BRAKE_ENABLED = 10,
	DEVICE_MODE_DPU_OPERATION_MODE = 3,
	DEVICE_MODE_PCIE_DEVICE_MODE = 4,
	DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT = 11,
	DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT = 12,
	DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY = 13,
	DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY = 14,
	DEVICE_MODE_ONE_SHOT_GPU_COPY_SWITCH_POWER_LIMIT = 15,
	DEVICE_MODE_PERSISTENT_GPU_COPY_SWITCH_POWER_LIMIT = 16,
	DEVICE_MODE_ADAPTIVE_TGPMODE = 21,
	DEVICE_MODE_LLDP = 24,
};

/** @brief AdaptiveTGPMode (Dual Part Number) data byte values per NSM Type 5
 *         Device Mode Index 21.
 */
enum nsm_adaptive_tgpmode {
	NSM_ADAPTIVE_TGPMODE_DISABLED = 0,
	NSM_ADAPTIVE_TGPMODE_ENABLED = 1,
};

/** @brief LLDP TX/RX sub-mode values per NSM Type 5 Device Mode Index 24
 *         Bits 0:1 (TX) and 2:3 (RX) of the data byte.
 */
enum nsm_lldp_direction_mode {
	NSM_LLDP_DIR_MODE_OFF = 0,
	NSM_LLDP_DIR_MODE_MANDATORY = 1,
	NSM_LLDP_DIR_MODE_ALL = 2,
};

/** @brief DCBX sub-mode value per NSM Type 5 Device Mode Index 24
 *         Bit 4 of the data byte. Effective only when both
 *         TX and RX are NSM_LLDP_DIR_MODE_ALL.
 */
enum nsm_lldp_dcbx_mode {
	NSM_LLDP_DCBX_DISABLED = 0,
	NSM_LLDP_DCBX_ENABLED = 1,
};

/** @brief NSM Type 5 idx 24 LLDP-mode data byte.
 *  Wire bit layout (single byte):
 *    bits 0:1 – LLDP TX mode (enum nsm_lldp_direction_mode)
 *    bits 2:3 – LLDP RX mode (enum nsm_lldp_direction_mode)
 *    bit  4   – LLDP DCBX mode (enum nsm_lldp_dcbx_mode)
 *    bits 5:7 – reserved (must be 0 on transmit)
 *
 *  Field order matches the wire layout on GCC/Clang targets used by OpenBMC.
 *  Use memcpy to convert between this struct and the wire-format byte.
 */
struct nsm_lldp_mode_bitfield {
	uint8_t tx_mode : 2;
	uint8_t rx_mode : 2;
	uint8_t dcbx_mode : 1;
	uint8_t reserved : 3;
} __attribute__((packed));

enum device_sub_mode_offset {
	SUB_MODE_DPU_OPERATION = 0,
	SUB_MODE_PCIE_MULTI_SOCKET = 0,
	SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC = 1,
	SUB_MODE_PCIE_BIFURCATION = 2,
};

#define DEVICE_MODE_NOT_SUPPORTED (-1)
#define PCIE_DEVICE_MODE_SINGLE_SOCKET 1
#define PCIE_DEVICE_MODE_NO_BIFURCATION 1
#define DPU_DEVICE_MODE_DATA_SIZE 1
#define PCIE_DEVICE_MODE_DATA_SIZE 8

/** @struct nsm_get_device_mode_setting_req
 *
 *  Structure representing Get Device Mode Setting request.
 */
struct nsm_get_device_mode_setting_req {
	struct nsm_common_req hdr;
	uint8_t device_mode_index;
} __attribute__((packed));

/** @struct nsm_get_device_mode_setting_resp
 *
 *  Structure representing Get network interface mode response.
 */
struct nsm_get_device_mode_setting_resp {
	struct nsm_common_resp hdr;
	uint8_t device_mode;
} __attribute__((packed));

/** @struct nsm_set_network_interface_mode_req
 *
 *  Structure representing Set network interface mode request.
 */
struct nsm_set_device_mode_setting_req {
	struct nsm_common_req hdr;
	uint8_t device_mode_index;
	uint8_t device_mode;
} __attribute__((packed));

/** @struct nsm_get_device_mode_settings_v2_req
 *
 *  Structure representing Get Device Mode Settings v2 request.
 */
struct nsm_get_device_mode_settings_v2_req {
	struct nsm_common_req hdr;
	uint32_t device_mode_index;
} __attribute__((packed));

/** @struct nsm_get_device_mode_settings_v2_resp
 *
 *  Structure representing Get Device Mode Settings v2 response.
 */
struct nsm_get_device_mode_settings_v2_resp {
	struct nsm_common_resp hdr;
	uint16_t current_mode_length;
	uint16_t pending_mode_length;
	uint8_t mode_data[1];
} __attribute__((packed));

/** @struct nsm_set_device_mode_settings_v2_req
 *
 *  Structure representing Set Device Mode Settings v2 request.
 */
struct nsm_set_device_mode_settings_v2_req {
	struct nsm_common_req hdr;
	uint32_t device_mode_index;
	uint8_t device_mode_data[1];
} __attribute__((packed));

/** @struct nsm_get_supported_device_modes_resp
 *
 *  Structure representing Get Supported Device Modes response.
 */
struct nsm_get_supported_device_modes_resp {
	struct nsm_common_resp hdr;
	uint16_t handle;
	uint16_t mode_count;
	uint32_t supported_mode_list[1];
} __attribute__((packed));

/** @struct nsm_set_device_config_v2_req
 *
 *  Set Device Config request (Type 5, command 0x10). Uses V2 header.
 *  Spec: Device configuration type (NvU32) + Data (Variable).
 */
struct nsm_set_device_config_v2_req {
	struct nsm_common_req_v2 hdr;
	uint32_t device_config_type;
	uint8_t data[1];
} __attribute__((packed));

/** @struct nsm_get_device_config_v2_req
 *
 *  Get Device Config request (Type 5, command 0x11). Uses V2 header.
 *  Spec: Device configuration type (NvU32) + Device Configuration (Variable,
 * query).
 */
struct nsm_get_device_config_v2_req {
	struct nsm_common_req_v2 hdr;
	uint32_t device_config_type;
	uint8_t device_config_query[1];
} __attribute__((packed));

/** @struct nsm_get_device_config_v2_resp
 *
 *  Get Device Config response (Type 5, command 0x11).
 *  Spec: current/pending lengths (NvU16) + Current + Pending Device
 * Configuration.
 */
struct nsm_get_device_config_v2_resp {
	struct nsm_common_resp hdr;
	uint16_t current_config_length;
	uint16_t pending_config_length;
	uint8_t config_data[1];
} __attribute__((packed));

/** @brief Encode a Set Protection Options request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] protection_mode - Protection mode setting
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_protection_options_req(uint8_t instance_id,
				      uint8_t protection_mode,
				      struct nsm_msg *msg);

/** @brief Decode a Set Protection Options request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] protection_mode - pointer to protection mode setting
 *  @return nsm_completion_codes
 */
int decode_set_protection_options_req(const struct nsm_msg *msg, size_t msg_len,
				      uint8_t *protection_mode);

/** @brief Encode a Set Protection Options response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_protection_options_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code,
				       struct nsm_msg *msg);

/** @brief Decode a Set Protection Options response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_protection_options_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code);

/** @brief Encode a Get Protection Options request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_protection_options_req(uint8_t instance_id, struct nsm_msg *msg);

/** @brief Decode a Get Protection Options request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_protection_options_req(const struct nsm_msg *msg,
				      size_t msg_len);

/** @brief Encode a Get Protection Options response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] protection_mode - protection mode setting
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_protection_options_resp(uint8_t instance_id, uint8_t cc,
				       uint16_t reason_code,
				       uint8_t protection_mode,
				       struct nsm_msg *msg);

/** @brief Decode a Get Protection Options response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @param[out] protection_mode - pointer to protection mode setting
 *  @return nsm_completion_codes
 */
int decode_get_protection_options_resp(const struct nsm_msg *msg,
				       size_t msg_len, uint8_t *cc,
				       uint16_t *reason_code,
				       uint8_t *protection_mode);

/** @brief Encode a Set Device Mode Settings request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] mode - Network interface mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_mode_setting_req(
    uint8_t instance_id, uint8_t device_mode_index,
    enum nsm_l1_prediction_mode_config device_mode, struct nsm_msg *msg);

/** @brief Decode a Set Device Mode Settings request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_set_device_mode_settings_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *device_mode_index,
    enum nsm_l1_prediction_mode_config *device_mode);

/** @brief Encode a Set Device Mode Settings response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_mode_settings_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 struct nsm_msg *msg);

/** @brief Decode a Set Device Mode Settings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @return nsm_completion_codes
 */
int decode_set_device_mode_setting_resp(const struct nsm_msg *msg,
					size_t msg_len, uint8_t *cc,
					uint16_t *reason_code);

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

/** @brief Encode Activate Error Injection Payload request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] error_injection_type - Error injection type
 *  @param[in] error_injection_subtype - Error injection subtype
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_activate_error_injection_payload_req(
    uint8_t instance_id, uint16_t error_injection_type,
    uint16_t error_injection_subtype, struct nsm_msg *msg);

/** @brief Decode Activate  Error Injection Payload request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] error_injection_type - pointer to error injection type
 *  @param[out] error_injection_subtype - pointer to error injection subtype
 *  @return nsm_completion_codes
 */
int decode_activate_error_injection_payload_req(
    const struct nsm_msg *msg, size_t msg_len, uint16_t *error_injection_type,
    uint16_t *error_injection_subtype);

/** @brief Encode Activate Error Injection Payload response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_activate_error_injection_payload_resp(uint8_t instance_id,
						 uint8_t cc,
						 uint16_t reason_code,
						 struct nsm_msg *msg);

/** @brief Decode Activate Error Injection Payload response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @return nsm_completion_codes
 */
int decode_activate_error_injection_payload_resp(const struct nsm_msg *msg,
						 size_t msg_len, uint8_t *cc,
						 uint16_t *reason_code);

/** @brief Encode a Get Error Injection Payload request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] error_injection_type - Error injection type
 *  @param[in] error_injection_subtype - Error injection subtype
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_error_injection_payload_req(uint8_t instance_id,
					   uint16_t error_injection_type,
					   uint16_t error_injection_subtype,
					   struct nsm_msg *msg);

/** @brief Decode a Get Error Injection Payload request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] error_injection_type - pointer to error injection type
 *  @param[out] error_injection_subtype - pointer to error injection subtype
 *  @return nsm_completion_codes
 */
int decode_get_error_injection_payload_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint16_t *error_injection_type,
					   uint16_t *error_injection_subtype);

/** @brief Encode a Get Error Injection Payload response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] error_injection_type - Error injection type
 *  @param[in] error_injection_subtype - Error injection subtype
 *  @param[in] data - pointer to error injection payload data array
 *  @param[in] data_size - size of the data array
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_error_injection_payload_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    uint16_t error_injection_type, uint16_t error_injection_subtype,
    const uint8_t *data, size_t data_size, struct nsm_msg *msg);

/** @brief Decode a Get Error Injection Payload response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[in] error_injection_type - Error injection type
 *  @param[in] error_injection_subtype - Error injection subtype
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @param[out] data  - pointer to error injection payload data array
 *  @param[out] data_size - pointer to receive the size of decoded data
 *  @return nsm_completion_codes
 */
int decode_get_error_injection_payload_resp(const struct nsm_msg *msg,
					    size_t msg_len,
					    uint16_t error_injection_type,
					    uint16_t error_injection_subtype,
					    uint8_t *cc, uint16_t *reason_code,
					    uint8_t *data, size_t *data_size);

/** @brief Encode a Set Current Error Injection payload request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] data - pointer to error injection payload data
 *  @param[in] data_size - size of the data array
 *  @param[in] error_injection_type - error injection type
 *  @param[in] error_injection_subtype - error injection subtype
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_error_injection_payload_req(uint8_t instance_id,
					   const uint8_t *data,
					   size_t data_size,
					   uint16_t error_injection_type,
					   uint16_t error_injection_subtype,
					   struct nsm_msg *msg);

/** @brief Decode a Set Current Error Injection payload
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] error_injection_type - pointer to error injection type
 *  @param[out] error_injection_subtype - pointer to error injection subtype
 *  @param[out] data  - pointer to error injection payload data
 *  @return nsm_completion_codes
 */
int decode_set_error_injection_payload_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint16_t *error_injection_type,
					   uint16_t *error_injection_subtype,
					   uint8_t *data, size_t *data_size);

/** @brief Encode a Set Current Error Injection payload response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_error_injection_payload_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg);

/** @brief Decode a Set Supported Error Injection payload response message Set
 * Current Error Injection payload response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_error_injection_payload_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code);

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
 *  @param[in] data_size_max - maximum size of the data buffer in bytes; the
 *             decoded data_size must not exceed this
 *  @return nsm_completion_codes
 */
int decode_get_fpga_diagnostics_settings_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc, uint16_t *data_size,
    uint16_t *reason_code, uint8_t *data, size_t data_size_max);

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

/** @brief Encode a Get Device Mode Setting request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_mode_index - Device mode index
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_mode_setting_req(uint8_t instance_id,
				       uint8_t device_mode_index,
				       struct nsm_msg *msg);

/** @brief Decode a Get Device Mode Setting request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_mode_index - Device mode index
 *  @return nsm_completion_codes
 */
int decode_get_device_mode_setting_req(const struct nsm_msg *msg,
				       size_t msg_len,
				       uint8_t *device_mode_index);

/** @brief Encode a Get Device Mode Settings response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] device_mode - Device mode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_mode_settings_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 uint8_t device_mode,
					 struct nsm_msg *msg);

/** @brief Decode a Get Device Mode Settings response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @param[out] device_mode - pointer to device mode
 *  @return nsm_completion_codes
 */
int decode_get_device_mode_setting_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, enum nsm_l1_prediction_mode_config *device_mode);

/** @brief Encode a Get Device Mode Settings v2 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_mode_index - Device mode index (NvU32)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_mode_settings_v2_req(uint8_t instance_id,
					   uint32_t device_mode_index,
					   struct nsm_msg *msg);

/** @brief Decode a Get Device Mode Settings v2 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_mode_index - Device mode index
 *  @return nsm_completion_codes
 */
int decode_get_device_mode_settings_v2_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint32_t *device_mode_index);

/** @brief Encode a Get Device Mode Settings v2 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] current_mode_data - pointer to current mode data
 *  @param[in] current_mode_length - length of current mode data in bytes
 *  @param[in] pending_mode_data - pointer to pending mode data (can be NULL)
 *  @param[in] pending_mode_length - length of pending mode data in bytes (0 if
 * not available)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_mode_settings_v2_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    const uint8_t *current_mode_data,
					    uint16_t current_mode_length,
					    const uint8_t *pending_mode_data,
					    uint16_t pending_mode_length,
					    struct nsm_msg *msg);

/** @brief Decode a Get Device Mode Settings v2 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @param[out] current_mode_data - pointer to buffer for current mode data
 *  @param[out] current_mode_length - pointer to current mode data length
 *  @param[out] pending_mode_data - pointer to buffer for pending mode data
 *  @param[out] pending_mode_length - pointer to pending mode data length
 *  @return nsm_completion_codes
 */
int decode_get_device_mode_settings_v2_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code,
					    uint8_t *current_mode_data,
					    uint16_t *current_mode_length,
					    uint8_t *pending_mode_data,
					    uint16_t *pending_mode_length);

/** @brief Encode a Set Device Mode Settings v2 request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_mode_index - Device mode index (NvU32)
 *  @param[in] device_mode_data - pointer to device mode data
 *  @param[in] device_mode_data_length - length of device mode data in bytes
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_mode_settings_v2_req(uint8_t instance_id,
					   uint32_t device_mode_index,
					   const uint8_t *device_mode_data,
					   uint16_t device_mode_data_length,
					   struct nsm_msg *msg);

/** @brief Decode a Set Device Mode Settings v2 request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_mode_index - Device mode index
 *  @param[out] device_mode_data - pointer to buffer for device mode data
 *  @param[out] device_mode_data_length - pointer to device mode data length
 *  @return nsm_completion_codes
 */
int decode_set_device_mode_settings_v2_req(const struct nsm_msg *msg,
					   size_t msg_len,
					   uint32_t *device_mode_index,
					   uint8_t *device_mode_data,
					   uint16_t *device_mode_data_length);

/** @brief Encode a Set Device Mode Settings v2 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_mode_settings_v2_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t reason_code,
					    struct nsm_msg *msg);

/** @brief Decode a Set Device Mode Settings v2 response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_device_mode_settings_v2_resp(const struct nsm_msg *msg,
					    size_t msg_len, uint8_t *cc,
					    uint16_t *reason_code);

/** @brief Encode a Get Supported Device Modes request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_device_modes_req(uint8_t instance_id,
					  struct nsm_msg *msg);

/** @brief Decode a Get Supported Device Modes request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_supported_device_modes_req(const struct nsm_msg *msg,
					  size_t msg_len);

/** @brief Encode a Get Supported Device Modes response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] handle - handle value
 *  @param[in] mode_count - number of supported modes
 *  @param[in] supported_mode_list - array of supported mode indices
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_supported_device_modes_resp(uint8_t instance_id, uint8_t cc,
					   uint16_t reason_code,
					   uint16_t handle, uint16_t mode_count,
					   const uint32_t *supported_mode_list,
					   struct nsm_msg *msg);

/** @brief Decode a Get Supported Device Modes response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] reason_code - pointer to reason code
 *  @param[out] handle - pointer to handle value
 *  @param[out] mode_count - pointer to number of supported modes
 *  @param[out] supported_mode_list - array to store supported mode indices
 *  @return nsm_completion_codes
 */
int decode_get_supported_device_modes_resp(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *cc,
					   uint16_t *reason_code,
					   uint16_t *handle,
					   uint16_t *mode_count,
					   uint32_t *supported_mode_list);

/** @brief Encode a Set Device Config request (Type 5, command 0x10, V2 header)
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_config_type - Device configuration type (NvU32)
 *  @param[in] data - Device configuration to apply (may be NULL if data_length
 * 0)
 *  @param[in] data_length - Length of data in bytes; with the NvU32 config
 * type, sizeof(device_config_type)+data_length must fit in the V2 request
 *  hdr.data_size (uint16_t), i.e. at most UINT16_MAX - sizeof(uint32_t)
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_config_v2_req(uint8_t instance_id,
				    uint32_t device_config_type,
				    const uint8_t *data, uint16_t data_length,
				    struct nsm_msg *msg);

/** @brief Decode a Set Device Config request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_config_type - Device configuration type
 *  @param[out] data - Buffer for config data; must be non-NULL if the decoded
 *  payload has non-zero data length
 *  @param[out] data_length - Length of config data
 *  @return nsm_completion_codes
 */
int decode_set_device_config_v2_req(const struct nsm_msg *msg, size_t msg_len,
				    uint32_t *device_config_type, uint8_t *data,
				    uint16_t *data_length);

/** @brief Encode a Set Device Config V2 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_set_device_config_v2_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code, struct nsm_msg *msg);

/** @brief Decode a Set Device Config V2 response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - completion code
 *  @param[out] reason_code - NSM reason code
 *  @return nsm_completion_codes
 */
int decode_set_device_config_v2_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code);

/** @brief Encode a Get Device Config request (Type 5, command 0x11, V2 header)
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] device_config_type - Device configuration type (NvU32)
 *  @param[in] query_data - Query (identifier fields); NULL only if
 * query_length is 0; sizeof(device_config_type)+query_length must fit in
 * hdr.data_size (uint16_t)
 *  @param[in] query_length - Length of query in bytes
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_config_v2_req(uint8_t instance_id,
				    uint32_t device_config_type,
				    const uint8_t *query_data,
				    uint16_t query_length, struct nsm_msg *msg);

/** @brief Decode a Get Device Config request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] device_config_type - Device configuration type
 *  @param[out] query_data - Buffer for query; must be non-NULL if the decoded
 *  query length is non-zero
 *  @param[out] query_length - Length of query data
 *  @return nsm_completion_codes
 */
int decode_get_device_config_v2_req(const struct nsm_msg *msg, size_t msg_len,
				    uint32_t *device_config_type,
				    uint8_t *query_data,
				    uint16_t *query_length);

/** @brief Encode a Get Device Config V2 response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - completion code
 *  @param[in] reason_code - NSM reason code
 *  @param[in] current_config_data - current config blob (may be NULL if length
 * 0)
 *  @param[in] current_config_length - length of current config in bytes
 *  @param[in] pending_config_data - pending config blob (may be NULL if length
 * 0)
 *  @param[in] pending_config_length - length of pending config in bytes
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_device_config_v2_resp(uint8_t instance_id, uint8_t cc,
				     uint16_t reason_code,
				     const uint8_t *current_config_data,
				     uint16_t current_config_length,
				     const uint8_t *pending_config_data,
				     uint16_t pending_config_length,
				     struct nsm_msg *msg);

/** @brief Decode a Get Device Config V2 response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - completion code
 *  @param[out] reason_code - NSM reason code
 *  @param[out] current_config_data - buffer for current config; must be
 *  non-NULL if decoded current length is non-zero
 *  @param[out] current_config_length - length of current config
 *  @param[out] pending_config_data - buffer for pending config; must be
 *  non-NULL if decoded pending length is non-zero
 *  @param[out] pending_config_length - length of pending config
 *  @return nsm_completion_codes
 */
int decode_get_device_config_v2_resp(const struct nsm_msg *msg, size_t msg_len,
				     uint8_t *cc, uint16_t *reason_code,
				     uint8_t *current_config_data,
				     uint16_t *current_config_length,
				     uint8_t *pending_config_data,
				     uint16_t *pending_config_length);

/** @brief Encode Device Configuration Request event v1 (Type 5, event ID 0x01,
 *         class 0). No payload beyond standard NSM event header.
 */
int encode_nsm_device_config_request_event_v1(uint8_t instance_id, bool ackr,
					      struct nsm_msg *msg);

/** @brief Decode Device Configuration Request event v1; optional event_class
 *         echo (or NULL). Fails if message type, id, class, or data_size wrong.
 */
int decode_nsm_device_config_request_event_v1(const struct nsm_msg *msg,
					      size_t msg_len,
					      uint8_t *event_class,
					      uint16_t *event_state);

#ifdef __cplusplus
}
#endif
#endif
