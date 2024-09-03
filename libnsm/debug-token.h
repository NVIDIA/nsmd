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

#ifndef DEBUG_TOKEN_H
#define DEBUG_TOKEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"

#define NSM_DEBUG_TOKEN_DEVICE_ID_SIZE 8
#define NSM_DEBUG_TOKEN_DATA_MAX_SIZE 65535

/** @brief NSM debug token type
 */
enum nsm_debug_token_type {
	NSM_DEBUG_TOKEN_TYPE_FRC = 2,
	NSM_DEBUG_TOKEN_TYPE_CRCS = 5,
	NSM_DEBUG_TOKEN_TYPE_CRDT = 6,
	NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE = 7
};

/** @brief NSM debug token status
 */
enum nsm_debug_token_status {
	NSM_DEBUG_TOKEN_STATUS_QUERY_FAILURE = 0,
	NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE = 2,
	NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED = 3,
	NSM_DEBUG_TOKEN_STATUS_CHALLENGE_PROVIDED = 4,
	NSM_DEBUG_TOKEN_STATUS_INSTALLATION_TIMEOUT = 5,
	NSM_DEBUG_TOKEN_STATUS_TOKEN_TIMEOUT = 6
};

/** @brief NSM debug token status additional information
 */
enum nsm_debug_token_status_additional_info {
	NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE = 0,
	NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION = 1,
	NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_QUERY_DISALLOWED =
	    4,
	NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE = 5
};

/** @brief NSM debug token opcode
 */
enum nsm_debug_token_opcode {
	NSM_DEBUG_TOKEN_OPCODE_RMCS = 0,
	NSM_DEBUG_TOKEN_OPCODE_RMDT = 1,
	NSM_DEBUG_TOKEN_OPCODE_CRCS = 2,
	NSM_DEBUG_TOKEN_OPCODE_CRDT = 3,
	NSM_DEBUG_TOKEN_OPCODE_LINKX_FRC = 4
};

/** @brief NSM debug token device type ID
 */
enum nsm_debug_token_device_type_id {
	NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_EROT = 1,
	NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_GPU = 2,
	NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_NVSWITCH = 3,
	NSM_DEBUG_TOKEN_DEVICE_TYPE_ID_CX7 = 4
};

/** @brief NSM debug token challenge query status
 */
enum nsm_debug_token_challenge_query_status {
	NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_OK = 0,
	NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_TOKEN_ALREADY_APPLIED = 1,
	NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_TOKEN_NOT_SUPPORTED = 2,
	NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_NO_KEY_CONFIGURED = 3,
	NSM_DEBUG_TOKEN_CHALLENGE_QUERY_STATUS_INTERFACE_NOT_ALLOWED = 4,
};

/** @struct nsm_debug_token_request
 *
 *  Structure representing generated NSM debug token request data.
 */
struct nsm_debug_token_request {
	uint16_t token_request_version;
	uint16_t token_request_size;
	uint8_t reserved1[20];
	uint8_t device_uuid[8];
	uint16_t device_type;
	uint8_t reserved2[2];
	uint8_t token_opcode;
	uint8_t status;
	uint16_t device_index : 12;
	uint8_t reserved3 : 4;
	uint8_t keypair_uuid[16];
	uint8_t base_mac[8];
	uint8_t psid[16];
	uint8_t reserved4[3];
	uint8_t fw_version[5];
	uint8_t source_address[16];
	uint16_t session_id;
	uint8_t reserved5;
	uint8_t challenge_version;
	uint8_t challenge[32];
} __attribute__((packed));

/** @struct nsm_query_token_parameters_req
 *
 *  Structure representing NSM query token parameters request.
 */
struct nsm_query_token_parameters_req {
	struct nsm_common_req hdr;
	uint8_t token_opcode;
} __attribute__((packed));

/** @struct nsm_query_token_parameters_resp
 *
 *  Structure representing NSM query token parameters response.
 */
struct nsm_query_token_parameters_resp {
	struct nsm_common_resp hdr;
	struct nsm_debug_token_request token_request;
} __attribute__((packed));

/** @struct nsm_provide_token_req
 *
 *  Structure representing NSM provide token request.
 */
struct nsm_provide_token_req {
	struct nsm_common_req_v2 hdr;
	uint8_t token_data[NSM_DEBUG_TOKEN_DATA_MAX_SIZE];
} __attribute__((packed));

/** @struct nsm_provide_token_resp
 *
 *  Structure representing NSM provide token response.
 *  Contains only success / error information.
 */
typedef struct nsm_common_resp nsm_provide_token_resp;

/** @struct nsm_disable_tokens_req
 *
 *  Structure representing NSM disable tokens request.
 *  Does not carry any payload data.
 */
typedef struct nsm_common_req nsm_disable_tokens_req;

/** @struct nsm_disable_tokens_resp
 *
 *  Structure representing NSM disable tokens response.
 *  Contains only success / error information.
 */
typedef struct nsm_common_resp nsm_disable_tokens_resp;

/** @struct nsm_query_token_status_req
 *
 *  Structure representing NSM query token status request.
 */
struct nsm_query_token_status_req {
	struct nsm_common_req hdr;
	uint8_t token_type;
} __attribute__((packed));

/** @struct nsm_query_token_status_resp
 *
 *  Structure representing NSM query token status response.
 */
struct nsm_query_token_status_resp {
	struct nsm_common_resp hdr;
	uint8_t token_type;
	uint8_t reserved;
	uint8_t additional_info;
	uint8_t status;
	uint32_t time_left;
} __attribute__((packed));

/** @struct nsm_query_device_ids_req
 *
 *  Structure representing NSM query device IDs request.
 *  Does not carry any payload data.
 */
typedef struct nsm_common_req nsm_query_device_ids_req;

/** @struct nsm_query_device_ids_resp
 *
 *  Structure representing NSM query device IDs response.
 */
struct nsm_query_device_ids_resp {
	struct nsm_common_resp hdr;
	uint8_t device_id[8];
} __attribute__((packed));

/** @brief Decode a Query token parameters request message
 *
 *  @param[in] msg - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] token_opcode - Pointer to store the token opcode
 *  @return nsm_completion_codes
 */
int decode_nsm_query_token_parameters_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum nsm_debug_token_opcode *token_opcode);

/** @brief Encode a Query token parameters request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] token_opcode - token opcode
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_query_token_parameters_req(
    uint8_t instance_id, enum nsm_debug_token_opcode token_opcode,
    struct nsm_msg *msg);

/**
 * @brief Decode a Query token parameters response message
 *
 * @param[in] msg - Response message
 * @param[in] msg_len - Length of response message
 * @param[out] cc - Completion code
 * @param[out] reason_code - Reason code
 * @param[out] token_request - Pointer to a structure to store the token request
 * @return nsm_completion_codes
 */
int decode_nsm_query_token_parameters_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_debug_token_request *token_request);

/**
 * @brief Encode a Query token parameters response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code
 * @param[in] token_request - Pointer to a structure containing the token
 * request
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_query_token_parameters_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_debug_token_request *token_request, struct nsm_msg *msg);

/**
 * @brief Decode a Provide token request message
 *
 * @param[in] msg - Request message
 * @param[in] msg_len - Length of request message
 * @param[out] token_data - Pointer to a buffer to store the token data
 * @param[out] token_data_len - Pointer to store the length of the token data
 * @return nsm_completion_codes
 */
int decode_nsm_provide_token_req(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *token_data, uint8_t *token_data_len);

/**
 * @brief Encode a Provide token request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] token_data - Pointer to the token data
 * @param[in] token_data_len - Length of the token data
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_provide_token_req(uint8_t instance_id, const uint8_t *token_data,
				 const uint16_t token_data_len,
				 struct nsm_msg *msg);

/**
 * @brief Decode a Provide token response message
 *
 * @param[in] msg - Response message
 * @param[in] msg_len - Length of response message
 * @param[out] cc - Completion code
 * @param[out] reason_code - Reason code
 * @return nsm_completion_codes
 */
int decode_nsm_provide_token_resp(const struct nsm_msg *msg, size_t msg_len,
				  uint8_t *cc, uint16_t *reason_code);

/**
 * @brief Encode a Provide token response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_provide_token_resp(uint8_t instance_id, uint8_t cc,
				  uint16_t reason_code, struct nsm_msg *msg);

/**
 * @brief Decode a Disable tokens request message
 *
 * @param[in] msg - Request message
 * @param[in] msg_len - Length of request message
 * @return nsm_completion_codes
 */
int decode_nsm_disable_tokens_req(const struct nsm_msg *msg, size_t msg_len);

/**
 * @brief Encode a Disable tokens request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_disable_tokens_req(uint8_t instance_id, struct nsm_msg *msg);

/**
 * @brief Decode a Disable tokens response message
 *
 * @param[in] msg - Response message
 * @param[in] msg_len - Length of response message
 * @param[out] cc - Completion code
 * @param[out] reason_code - Reason code
 * @return nsm_completion_codes
 */
int decode_nsm_disable_tokens_resp(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *cc, uint16_t *reason_code);

/**
 * @brief Encode a Disable tokens response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_disable_tokens_resp(uint8_t instance_id, uint8_t cc,
				   uint16_t reason_code, struct nsm_msg *msg);

/**
 * @brief Decode a Query token status request message
 *
 * @param[in] msg - Request message
 * @param[in] msg_len - Length of request message
 * @param[out] token_type - Pointer to store the token type
 * @return nsm_completion_codes
 */
int decode_nsm_query_token_status_req(const struct nsm_msg *msg, size_t msg_len,
				      enum nsm_debug_token_type *token_type);

/**
 * @brief Encode a Query token status request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] token_type - Token type
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_query_token_status_req(uint8_t instance_id,
				      enum nsm_debug_token_type token_type,
				      struct nsm_msg *msg);

/**
 * @brief Decode a Query token status response message
 *
 * @param[in] msg - Response message
 * @param[in] msg_len - Length of response message
 * @param[out] cc - Completion code
 * @param[out] reason_code - Reason code
 * @param[out] status - Pointer to store the token status
 * @param[out] additional_info - Pointer to store additional info data
 * @param[out] token_type - Pointer to store the token type
 * @param[out] time_left - Pointer to store the time left
 * @return nsm_completion_codes
 */
int decode_nsm_query_token_status_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, enum nsm_debug_token_status *status,
    enum nsm_debug_token_status_additional_info *additional_info,
    enum nsm_debug_token_type *token_type, uint32_t *time_left);

/**
 * @brief Encode a Query token status response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code
 * @param[in] status - Token status
 * @param[in] additional_info - Additional information about the status
 * @param[in] token_type - Token type
 * @param[in] time_left - Time left for the token
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_query_token_status_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    enum nsm_debug_token_status status,
    enum nsm_debug_token_status_additional_info additional_info,
    enum nsm_debug_token_type token_type, uint32_t time_left,
    struct nsm_msg *msg);

/**
 * @brief Decode a Query device IDs request message
 *
 * @param[in] msg - Request message
 * @param[in] msg_len - Length of request message
 * @return nsm_completion_codes
 */
int decode_nsm_query_device_ids_req(const struct nsm_msg *msg, size_t msg_len);

/**
 * @brief Encode a Query device IDs request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_query_device_ids_req(uint8_t instance_id, struct nsm_msg *msg);

/**
 * @brief Decode a Query device IDs response message
 *
 * @param[in] msg - Response message
 * @param[in] msg_len - Length of response message
 * @param[out] cc - Completion code
 * @param[out] reason_code - Reason code
 * @param[out] device_id - Pointer to store the device ID
 * @return nsm_completion_codes
 */
int decode_nsm_query_device_ids_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint8_t device_id[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE]);

/**
 * @brief Encode a Query device IDs response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code
 * @param[in] device_id - Device ID data
 * @param[out] msg - Message will be written to this
 * @return nsm_completion_codes
 */
int encode_nsm_query_device_ids_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    const uint8_t device_id[NSM_DEBUG_TOKEN_DEVICE_ID_SIZE],
    struct nsm_msg *msg);

#ifdef __cplusplus
}
#endif
#endif
