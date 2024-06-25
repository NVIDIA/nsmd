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

enum device_configuration_command { NSM_GET_FPGA_DIAGNOSTICS_SETTINGS = 0x64 };

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
	uint8_t res2 : 7;   // 1:7 – Reserved
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
	uint8_t res4 : 4; // 4:7 – Reserved
	/* Bytes 5-7 reserved */
	uint8_t res5;
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

#ifdef __cplusplus
}
#endif
#endif