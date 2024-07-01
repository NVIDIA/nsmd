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

enum diagnostics_command { NSM_ENABLE_DISABLE_WP = 0x65 };

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
	HMC_SPI_FLASH = 176,
	RETIMER_EEPROM_1 = 192,
	RETIMER_EEPROM_2 = 193,
	RETIMER_EEPROM_3 = 194,
	RETIMER_EEPROM_4 = 195,
	RETIMER_EEPROM_5 = 196,
	RETIMER_EEPROM_6 = 197,
	RETIMER_EEPROM_7 = 198,
	RETIMER_EEPROM_8 = 199,
	CX7_FRU_EEPROM = 232,
	HMC_FRU_EEPROM = 233,
};

/** @struct nsm_enable_disable_wp_req
 *
 *  Structure representing Diagnostics Enable/Disable WP request.
 */
struct nsm_enable_disable_wp_req {
	struct nsm_common_req hdr;
	uint8_t data_index;
	uint8_t value; // 0 - disable, 1 - enable
} __attribute__((packed));

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

#ifdef __cplusplus
}
#endif
#endif