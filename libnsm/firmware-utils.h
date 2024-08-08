/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved. 
 * SPDX-License-Identifier: Apache-2.0
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

#ifndef FIRMWARE_UTILS_H
#define FIRMWARE_UTILS_H

#include "base.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief NSM Firmware Type Commands */
enum nsm_firmware_commands {
	NSM_FW_GET_EROT_STATE_INFORMATION = 0x01,
	NSM_FW_IRREVERSABLE_CONFIGURATION = 0x02,
	NSM_FW_QUERTY_CODE_AUTH_KEY = 0x03,
	NSM_FW_UPDATE_CODE_AUTH_KEY_PERM = 0x04,
	NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER = 0x05,
	NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER = 0x06
};

/** @struct nsm_firmware_state_information_fields
 *
 *  Enum representing field tags for command 1
 *   of msg type 6.
 */
enum nsm_firmware_state_information_fields {
	NSM_FIRMWARE_BACKGROUND_COPY_POLICY = 1,	   // Enum8
	NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT = 2,		   // NvU8
	NSM_FIRMWARE_ACTIVE_KEY_SET = 3,		   // NvU8
	NSM_FIRMWARE_WRITE_PROTECT_STATE = 4,		   // Enum8
	NSM_FIRMWARE_FIRMWARE_SLOT_COUNT = 5,		   // NvU8
	NSM_FIRMWARE_FIRMWARE_SLOT_ID = 6,		   // NvU8
	NSM_FIRMWARE_FIRMWARE_VERSION_STRING = 7,	   // char[]
	NSM_FIRMWARE_VERSION_COMPARISON_STAMP = 8,	   // NvU32
	NSM_FIRMWARE_BUILD_TYPE = 9,			   // Enum8
	NSM_FIRMWARE_SIGNING_TYPE = 10,			   // Enum8
	NSM_FIRMWARE_FIRMWARE_STATE = 11,		   // Enum8
	NSM_FIRMWARE_SECURITY_VERSION_NUMBER = 12,	   // NvU16
	NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER = 13, // NvU16
	NSM_FIRMWARE_SIGNING_KEY_INDEX = 14,		   // NvU16
	NSM_FIRMWARE_INBAND_UPDATE_POLICY = 15,		   // Enum8
	NSM_FIRMWARE_BOOT_STATUS_CODE = 16,		   // NvU64
};

/** @struct nsm_firmware_erot_state_info_hdr_resp
 *
 *  Structure representing all possible fields in
 *  header of the msg type 6, command 1 response
 */
struct nsm_firmware_erot_state_info_hdr_resp {
	uint8_t background_copy_policy;
	uint8_t active_slot;
	uint8_t active_keyset;
	uint16_t minimum_security_version;
	uint8_t inband_update_policy;
	uint8_t firmware_slot_count;
	uint64_t boot_status_code;
};

/* This is the maximum string length for firmware
 * slot information version - avoid dynamic allocation */
#define NSM_FIRMWARE_SLOT_INFO_VERSION_STRING_MAX 256

/** @struct nsm_firmware_slot_info
 *
 *  Structure representing all possible fields in
 *  slot information of the msg type 6, command 1 response
 */
struct nsm_firmware_slot_info {
	uint8_t slot_id;
	uint8_t
	    firmware_version_string[NSM_FIRMWARE_SLOT_INFO_VERSION_STRING_MAX];
	uint32_t version_comparison_stamp;
	uint8_t build_type;
	uint8_t signing_type;
	uint8_t write_protect_state;
	uint8_t firmware_state;
	uint16_t security_version_number;
	uint16_t signing_key_index;
};

/** @struct nsm_firmware_erot_state_info_resp
 *
 *  Structure representing combined fields in
 *  the msg type 6, command 1 response
 */
struct nsm_firmware_erot_state_info_resp {
	struct nsm_firmware_erot_state_info_hdr_resp fq_resp_hdr;
	struct nsm_firmware_slot_info *slot_info;
};

/** @struct struct nsm_firmware_erot_state_info_req
 *
 *  Structure representing all fields in
 *  the msg type 6, command 1 request
 */
struct nsm_firmware_erot_state_info_req {
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
} __attribute__((packed));

/** @struct nsm_firmware_get_erot_state_info_req
 *
 *  Structure representing
 *  the msg type 6, command 1 request
 */
struct nsm_firmware_get_erot_state_info_req {
	struct nsm_common_req hdr;
	struct nsm_firmware_erot_state_info_req fq_req;
} __attribute__((packed));

/** @struct nsm_firmware_get_erot_state_info_resp
 *
 *  Structure representing payload in
 *  the msg type 6, command 1 response
 */
struct nsm_firmware_get_erot_state_info_resp {
	struct nsm_common_telemetry_resp hdr;
	uint8_t payload[1];
} __attribute__((packed));

/**
 * @brief Decode nsm query request erot state parameters message.
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] fw_req - Pointer to the NSM query erot state parameters request
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
int decode_nsm_query_get_erot_state_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_firmware_erot_state_info_req *fw_req);

/**
 * @brief Encode nsm query request erot state parameters message.
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] fw_req - Pointer to the NSM query erot state parameters request
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
int encode_nsm_query_get_erot_state_parameters_req(
    uint8_t instance_id, const struct nsm_firmware_erot_state_info_req *fw_req,
    struct nsm_msg *msg);

/**
 * @brief Encode nsm response on query erot state parameters message.
 *          This version encodes all possible fields.
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - NSM Completion Code
 * @param[in] reason_code - Reason Code
 * @param[in] fw_req - Pointer to the NSM query erot state parameters request
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
int encode_nsm_query_get_erot_state_parameters_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_firmware_erot_state_info_resp *fw_info, struct nsm_msg *msg);

/**
 * @brief Decode nsm query erot state parameters response message.
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[in] cc - NSM Completion Code
 * @param[in] reason_code - Reason Code
 * @param[out] fw_req - Pointer to the NSM query erot state parameters request
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
int decode_nsm_query_get_erot_state_parameters_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_firmware_erot_state_info_resp *fw_resp);

#ifdef __cplusplus
}
#endif

#endif
