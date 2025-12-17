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
	NSM_FW_QUERY_CODE_AUTH_KEY_PERM = 0x03,
	NSM_FW_UPDATE_CODE_AUTH_KEY_PERM = 0x04,
	NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER = 0x05,
	NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER = 0x06,
	NSM_FW_SET_ROT_PROPERTY = 0x08,
	NSM_FW_IMAGE_COPY_CONTROL = 0x09,
	NSM_FW_DOT_GET_INFO = 0x20,
	NSM_FW_DOT_CAK_INSTALL = 0x21,
	NSM_FW_DOT_LOCK = 0x22,
	NSM_FW_DOT_UNLOCK = 0x23,
	NSM_FW_DOT_CAK_ROTATE = 0x24,
	NSM_FW_DOT_DISABLE = 0x25,
	NSM_FW_DOT_OVERRIDE = 0x26,
	NSM_FW_DOT_UNLOCK_CHALLENGE = 0x27,
	NSM_FW_DOT_RECOVERY = 0x28,
	NSM_FW_DOT_CAK_BYPASS = 0x29,
	NSM_FW_DOT_GET_STATUS = 0x2A,
};

#define DOT_KEY_AUTH_DATA_SIZE 148
#define DOT_STATIC_CHALLENGE_SIZE 32
#define DOT_CHALLENGE_SIZE 32
#define DOT_BLOB_SIZE 1024
#define DOT_SIGNATURE_SIZE 1840
#define DOT_GET_INFO_DATA_SIZE 1028

/** @struct nsm_firmware_state_information_fields
 *
 *  Enum representing field tags for command 1
 *   of msg type 6.
 */
enum nsm_firmware_state_information_fields {
	NSM_FIRMWARE_BACKGROUND_COPY_POLICY_PERSISTENT = 1, // Enum8
	NSM_FIRMWARE_ACTIVE_FIRMWARE_SLOT = 2,		    // NvU8
	NSM_FIRMWARE_ACTIVE_KEY_SET = 3,		    // NvU8
	NSM_FIRMWARE_WRITE_PROTECT_STATE = 4,		    // Enum8
	NSM_FIRMWARE_FIRMWARE_SLOT_COUNT = 5,		    // NvU8
	NSM_FIRMWARE_FIRMWARE_SLOT_ID = 6,		    // NvU8
	NSM_FIRMWARE_FIRMWARE_VERSION_STRING = 7,	    // char[]
	NSM_FIRMWARE_VERSION_COMPARISON_STAMP = 8,	    // NvU32
	NSM_FIRMWARE_BUILD_TYPE = 9,			    // Enum8
	NSM_FIRMWARE_SIGNING_TYPE = 10,			    // Enum8
	NSM_FIRMWARE_FIRMWARE_STATE = 11,		    // Enum8
	NSM_FIRMWARE_SECURITY_VERSION_NUMBER = 12,	    // NvU16
	NSM_FIRMWARE_MINIMUM_SECURITY_VERSION_NUMBER = 13,  // NvU16
	NSM_FIRMWARE_SIGNING_KEY_INDEX = 14,		    // NvU16
	NSM_FIRMWARE_INBAND_UPDATE_POLICY_PERSISTENT = 15,  // Enum8
	NSM_FIRMWARE_BOOT_STATUS_CODE = 16,		    // NvU64
	NSM_FIRMWARE_INBAND_UPDATE_POLICY_CURRENT = 17,	    // Enum8
	NSM_FIRMWARE_BACKGROUND_COPY_POLICY_CURRENT = 18,   // Enum8
	NSM_FIRMWARE_AP_SKU_ID = 19,			    // NvU32
};

/** @brief NSM code authentication key permissions request type
 */
enum nsm_code_auth_key_perm_request_type {
	NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE = 0,
	NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE = 1,
};

/** @brief NSM RoT Property values
 */
enum nsm_rot_property_values {
	NSM_ROT_PROPERTY_REDUNDANCY_POLICY = 0,
	NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY = 1,
	NSM_ROT_PROPERTY_AP_SKU_ID = 2,
};

/** @brief NSM RoT Redundancy Policy values (Property = 0)
 */
enum nsm_rot_redundancy_policy {
	NSM_ROT_REDUNDANCY_POLICY_MANUAL_BACKGROUND_COPY = 0,
	NSM_ROT_REDUNDANCY_POLICY_AUTOMATIC_BACKGROUND_COPY = 1,
};

/** @brief NSM RoT Redundancy Policy Lifespan values
 */
enum nsm_rot_redundancy_policy_lifespan {
	NSM_ROT_REDUNDANCY_POLICY_LIFESPAN_PERSISTENT = 0,
	NSM_ROT_REDUNDANCY_POLICY_LIFESPAN_ONE_SHOT = 1,
};

/** @brief NSM RoT In-band Update Policy Lifespan values
 */
enum nsm_rot_inband_update_policy_lifespan {
	NSM_ROT_INBAND_UPDATE_POLICY_LIFESPAN_PERSISTENT = 0,
	NSM_ROT_INBAND_UPDATE_POLICY_LIFESPAN_VOLATILE = 1,
};

/** @brief NSM RoT AP SKU ID Lifespan values
 */
enum nsm_rot_ap_sku_id_lifespan {
	NSM_ROT_AP_SKU_ID_LIFESPAN_PERSISTENT = 0,
	NSM_ROT_AP_SKU_ID_LIFESPAN_VOLATILE = 1,
};

/** @brief NSM RoT AP SKU ID argument length
 * 4 bytes for SKU ID + 1 byte for lifespan
 */
#define NSM_ROT_AP_SKU_ID_ARGUMENT_LENGTH 5

/** @brief NSM RoT In-band Update Policy values (Property = 1)
 */
enum nsm_rot_inband_update_policy {
	NSM_ROT_INBAND_UPDATE_POLICY_DISABLE = 0,
	NSM_ROT_INBAND_UPDATE_POLICY_ENABLE = 1,
};

/** @brief NSM Image Copy Control types
 */
enum nsm_image_copy_control_types {
	NSM_IMAGE_COPY_QUERY_PROGRESS = 0,
	NSM_IMAGE_COPY_INITIATE_IMAGE_COPY = 1,
};

/** @brief NSM EFUSE update method
 */
enum nsm_efuse_update_method {
	NSM_EFUSE_UPDATE_METHOD_AUTO = (1 << 0),
	NSM_EFUSE_UPDATE_METHOD_MEDIUM_SPECIFIC_RESET = (1 << 2),
	NSM_EFUSE_UPDATE_METHOD_SYSTEM_REBOOT = (1 << 3),
	NSM_EFUSE_UPDATE_METHOD_DC_POWER_CYCLE = (1 << 4),
	NSM_EFUSE_UPDATE_METHOD_AC_POWER_CYCLE = (1 << 5),
	NSM_EFUSE_UPDATE_METHOD_WARM_RESET = (1 << 16),
	NSM_EFUSE_UPDATE_METHOD_HOT_RESET = (1 << 17),
	NSM_EFUSE_UPDATE_METHOD_FUNCTION_LEVEL_RESET = (1 << 18)
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
	uint8_t inband_update_policy_current;
	uint8_t background_copy_policy_current;
	uint32_t ap_sku_id;
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

/* Security Version Number Request and Response Structure */
/**
 * @struct Structure representing nsm firmware security version number request
 *
 */
struct nsm_firmware_security_version_number_req {
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
} __attribute__((packed));

/**
 * @struct Structure representing nsm firmware security version number request
 * used in nsm callbacks
 */
struct nsm_firmware_security_version_number_req_command {
	struct nsm_common_req hdr;
	struct nsm_firmware_security_version_number_req fq_req;
} __attribute__((packed));

/**
 * @struct Structure representing nsm firmware security version number response
 *
 */
struct nsm_firmware_security_version_number_resp {
	uint16_t active_component_security_version;
	uint16_t pending_component_security_version;
	uint16_t minimum_security_version;
	uint16_t pending_minimum_security_version;
} __attribute__((packed));

/**
 * @struct Structure representing nsm firmware security version number response
 * used in nsm callbacks
 */
struct nsm_firmware_security_version_number_resp_command {
	struct nsm_common_resp hdr;
	struct nsm_firmware_security_version_number_resp sec_ver_resp;
} __attribute__((packed));

/**
 * @brief enum for sec update request types
 *
 */
enum sec_update_request_types {
	REQUEST_TYPE_MOST_RESTRICTIVE_VALUE = 0,
	REQUEST_TYPE_SPECIFIED_VALUE = 1
};

/**
 * @struct Structure representing nsm update firmware security version number
 * request parameter
 */
struct nsm_firmware_update_min_sec_ver_req {
	uint8_t request_type;
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
	uint64_t nonce;
	uint16_t req_min_security_version;
} __attribute__((packed));

/**
 * @struct Structure representing nsm update firmware security version number
 * request command
 */
struct nsm_firmware_update_min_sec_ver_req_command {
	struct nsm_common_req hdr;
	struct nsm_firmware_update_min_sec_ver_req ver_update_req;
} __attribute__((packed));

/**
 * @struct Structure representing NSM firmware set RoT property request
 * parameters
 *
 * This structure contains the parameters needed to set Root of Trust (RoT)
 * properties for firmware components. The property field determines which RoT
 * property is being set (redundancy policy or in-band update policy), and the
 * argument_data contains the specific value for that property.
 *
 * @note For Property 0 (redundancy policy): argument_data[0] = policy,
 * argument_data[1] = lifespan
 * @note For Property 1 (in-band update policy): argument_data[0] = policy,
 * argument_data[1] = lifespan
 * @note For Property 2 (AP SKU ID): argument_data[0:3] = AP SKU ID,
 * argument_data[4] = lifespan
 */
struct nsm_firmware_set_rot_property_req {
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
	uint8_t property;
	uint8_t argument_length;
	uint8_t argument_data[5]; // Policy value and lifespan
} __attribute__((packed));

/**
 * @struct Structure representing NSM firmware set RoT property request command
 *
 * This structure wraps the RoT property request parameters in a complete
 * command structure that includes the common request header and the specific
 * RoT property request data.
 */
struct nsm_firmware_set_rot_property_req_command {
	struct nsm_common_req hdr;
	struct nsm_firmware_set_rot_property_req rot_property_req;
} __attribute__((packed));

/**
 * @struct Structure representing NSM firmware set RoT property response command
 *
 * This structure represents the response to a RoT property set request. It
 * contains only the common response header, indicating success or failure of
 * the operation.
 */
struct nsm_firmware_set_rot_property_resp_command {
	struct nsm_common_resp hdr;
} __attribute__((packed));

/**
 * @struct Structure representing nsm firmware image copy control request
 * parameters
 */
struct nsm_firmware_image_copy_control_req {
	uint8_t request_type;
	uint8_t component_count;
} __attribute__((packed));

/**
 * @struct Structure representing a single component identity entry
 * for image copy control
 */
struct nsm_firmware_image_copy_component_entry {
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
} __attribute__((packed));

/**
 * @struct Structure representing nsm firmware image copy control request
 * command
 */
struct nsm_firmware_image_copy_control_req_command {
	struct nsm_common_req hdr;
	struct nsm_firmware_image_copy_control_req image_copy_control_req;
} __attribute__((packed));

/** @brief NSM Image Copy Status values
 */
enum nsm_image_copy_status {
	NSM_IMAGE_COPY_NOT_TRIGGERED = 0,
	NSM_IMAGE_COPY_IN_PROGRESS = 1,
	NSM_IMAGE_COPY_COMPLETE = 2,
	NSM_IMAGE_COPY_UNDEFINED_FAILURE = 3,
	NSM_IMAGE_COPY_NO_VALID_IMAGE = 4,
	NSM_IMAGE_COPY_DESTINATION_WRITE_PROTECTED = 5,
	NSM_IMAGE_COPY_FAIL_FLASH_ACCESS = 6,
	NSM_IMAGE_COPY_FAILED_VERIFY = 7,
};

/**
 * @struct Structure representing image copy control response for Query
 * Progress image copy control state for Query Progress
 */
struct nsm_firmware_image_copy_control_query_progress_resp {
	uint8_t image_copy_status;
	uint8_t image_copy_progress;
} __attribute__((packed));

/**
 * @struct Structure representing image copy control response for Query
 * image copy control state command for Query Progress
 */
struct nsm_firmware_image_copy_control_query_progress_resp_command {
	struct nsm_common_resp hdr;
	struct nsm_firmware_image_copy_control_query_progress_resp
	    image_copy_control_query;
} __attribute__((packed));

/**
 * @struct Structure representing image copy control response for Initiate
 * the Components commands
 */
struct nsm_firmware_image_copy_control_initiate_copy_resp_command {
	struct nsm_common_resp hdr;
} __attribute__((packed));

/**
 * @struct Structure representing nsm update firmware security version number
 * response parameter
 */
struct nsm_firmware_update_min_sec_ver_resp {
	uint32_t update_methods;
} __attribute__((packed));

/**
 * @struct Structure representing nsm update firmware security version number
 * response command
 */
struct nsm_firmware_update_min_sec_ver_resp_command {
	struct nsm_common_resp hdr;
	struct nsm_firmware_update_min_sec_ver_resp sec_ver_resp;
} __attribute__((packed));

/**
 * @brief enum for irreversible request types
 *
 */
enum irreversible_cfg_request_types {
	QUERY_IRREVERSIBLE_CFG,
	DISABLE_IRREVERSIBLE_CFG,
	ENABLE_IRREVERSIBLE_CFG,
};

/**
 * @struct Structure representing irreversible configuration request parameter
 */
struct nsm_firmware_irreversible_config_req {
	uint8_t request_type;
} __attribute__((packed));

/**
 * @struct Structure representing irreversible configuration request parameter
 * command.
 */
struct nsm_firmware_irreversible_config_req_command {
	struct nsm_common_req hdr;
	struct nsm_firmware_irreversible_config_req irreversible_cfg_req;
} __attribute__((packed));

/**
 * @struct Structure representing irreversible configuration response for Query
 * irreversible configuration state
 */
struct nsm_firmware_irreversible_config_request_0_resp {
	uint8_t irreversible_config_state;
} __attribute__((packed));

/**
 * @struct Structure representing irreversible configuration response for Enable
 * irreversible configuration changes
 */
struct nsm_firmware_irreversible_config_request_2_resp {
	uint64_t nonce;
} __attribute__((packed));

/**
 * @struct Structure representing irreversible configuration response for Query
 * irreversible configuration state command
 */
struct nsm_firmware_irreversible_config_request_0_resp_command {
	struct nsm_common_resp hdr;
	struct nsm_firmware_irreversible_config_request_0_resp
	    irreversible_cfg_query;
} __attribute__((packed));

/**
 * @struct Structure representing irreversible configuration response for
 * Disable irreversible configuration changes command
 */
struct nsm_firmware_irreversible_config_request_1_resp_command {
	struct nsm_common_resp hdr;
} __attribute__((packed));

/**
 * @struct Structure representing irreversible configuration response for Enable
 * irreversible configuration changes command
 */
struct nsm_firmware_irreversible_config_request_2_resp_command {
	struct nsm_common_resp hdr;
	struct nsm_firmware_irreversible_config_request_2_resp
	    irreversible_cfg_enable_response;
} __attribute__((packed));

/** @struct nsm_code_auth_key_perm_query_req
 *
 *  Structure representing code authentication key permissions query
 * request.
 */
struct nsm_code_auth_key_perm_query_req {
	struct nsm_common_req hdr;
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
} __attribute__((packed));

/** @struct nsm_code_auth_key_perm_query_resp
 *
 *  Structure representing code authentication key permissions query
 * response.
 */
struct nsm_code_auth_key_perm_query_resp {
	struct nsm_common_resp hdr;
	uint16_t active_component_key_index;
	uint16_t pending_component_key_index;
	uint8_t permission_bitmap_length;
} __attribute__((packed));

/** @struct nsm_code_auth_key_perm_update_req
 *
 *  Structure representing code authentication key permissions update
 * request.
 */
struct nsm_code_auth_key_perm_update_req {
	struct nsm_common_req hdr;
	uint8_t request_type;
	uint16_t component_classification;
	uint16_t component_identifier;
	uint8_t component_classification_index;
	uint64_t nonce;
	uint8_t permission_bitmap_length;
} __attribute__((packed));

/** @struct nsm_code_auth_key_perm_update_resp
 *
 *  Structure representing code authentication key permissions update
 * response.
 */
struct nsm_code_auth_key_perm_update_resp {
	struct nsm_common_resp hdr;
	uint32_t update_method;
} __attribute__((packed));

/** @brief Key authentication scheme types for DotCAKInstall */
enum nsm_key_auth_scheme {
	NSM_KEY_AUTH_SCHEME_ECDSA = 0,
	NSM_KEY_AUTH_SCHEME_HYBRID = 1
};

/** @brief DOT Completion Codes */
enum nsm_dot_completion_codes {
	DOT_NO_ERROR = 0x0,	       /* DOT command success */
	DOT_ERROR_GENERAL_FAULT = 0x81 /* DOT command fail with error that is
					  not defined specifically */
};

/** @brief DOT Reason Codes */
enum nsm_dot_reason_codes {
	DOT_RC_INTERNAL_ERROR = 0x2000,
	DOT_RC_STATE_INVALID = 0x2001,
	DOT_RC_SIGNATURE_VERIFICATION_FAILED = 0x2002,
	DOT_RC_STORAGE_ERROR = 0x2003,
	DOT_RC_LOCK_DISABLED = 0x2004,
	DOT_RC_KEY_MISMATCH = 0x2005,
	DOT_RC_INVALID_UNLOCK_TYPE = 0x2006,
	DOT_RC_INVALID_UNLOCK_METHOD = 0x2007,
	DOT_RC_CRYPTO_ERROR = 0x2008,
	DOT_RC_BLOB_CREATION_FAILED = 0x2009,
	DOT_RC_INVALID_STATE_FOR_LOCK = 0x200A,
	DOT_RC_INVALID_STATE_FOR_UNLOCK = 0x200B,
	DOT_RC_INVALID_STATE_FOR_DISABLE = 0x200C,
	DOT_RC_INVALID_STATE_FOR_CAK_ROTATE = 0x200D,
	DOT_RC_INVALID_STATE_FOR_CAK_INSTALL = 0x200E,
	DOT_RC_NULL_POINTER = 0x200F,
	DOT_RC_FUSE_INCREMENT_FAILED = 0x2010,
	DOT_RC_RECOVERY_FAILED = 0x2011,
	DOT_RC_INVALID_COMMAND = 0x2012,
	DOT_RC_UNSUPPORTED_COMMAND = 0x202E,
	DOT_RC_INVALID_LENGTH = 0x202F,
};

/** @struct nsm_dot_cak_install_req
 *
 *  Structure representing DotCAKInstall request parameters.
 *  According to spec:
 *  - CAK.pub: 148-byte key authentication data per spec
 *  - LAK.pub: 148-byte key authentication data per spec
 */
struct nsm_dot_cak_install_req {
	uint8_t cak_pub[DOT_KEY_AUTH_DATA_SIZE];
	uint8_t lak_pub[DOT_KEY_AUTH_DATA_SIZE];
	uint8_t lock_disable; /* 0: Allow DOT_LOCK, 1: DOT_LOCK not allowed */
	uint32_t min_svn;     /* MIN_SVN for minimal SVN */
} __attribute__((packed));

/** @struct nsm_dot_cak_install_req_command
 *
 *  Structure representing DotCAKInstall request command.
 *  Uses nsm_common_req_v2 for large payload support (similar to
 * install_token_req)
 */
struct nsm_dot_cak_install_req_command {
	struct nsm_common_req_v2 hdr;
	struct nsm_dot_cak_install_req dot_cak_install_req;
} __attribute__((packed));

/** @struct nsm_dot_cak_bypass_req
 *
 *  Structure representing DotCAKBypass request.
 *  Request contains only command (no additional data).
 */
typedef struct nsm_common_req_v2 nsm_dot_cak_bypass_req;

/** @struct nsm_dot_cak_bypass_resp
 *
 *  Structure representing DotCAKBypass response.
 *  Contains only success / error information.
 */
typedef struct nsm_common_resp nsm_dot_cak_bypass_resp;

/** @struct nsm_dot_lock_req
 *
 *  Structure representing DOT LOCK request parameters.
 *  According to spec:
 *  - CAK.pub: 148-byte key authentication data per spec
 *  - LAK.pub: 148-byte key authentication data per spec
 *  - unlock_method: 4-byte unlock method for future DOT_UNLOCK
 *  - s_challenge: 32-byte static nonce (only when unlock_method == 2)
 *  - signature: 1840-byte LAK signature
 */
struct nsm_dot_lock_req {
	uint8_t cak_pub[DOT_KEY_AUTH_DATA_SIZE];
	uint8_t lak_pub[DOT_KEY_AUTH_DATA_SIZE];
	uint32_t unlock_method;
	uint8_t s_challenge[DOT_STATIC_CHALLENGE_SIZE];
	uint8_t signature[DOT_SIGNATURE_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_lock_req_command
 *
 *  Structure representing DOT LOCK request command.
 *  Uses nsm_common_req_v2 for large payload support.
 */
struct nsm_dot_lock_req_command {
	struct nsm_common_req_v2 hdr;
	struct nsm_dot_lock_req dot_lock_req;
} __attribute__((packed));

/** @struct nsm_dot_cak_rotate_req
 *
 *  Structure representing DOT CAK ROTATE request parameters.
 *  According to spec:
 *  - new_cak: 148-byte new CAK public key data
 *  - signature: 1840-byte LAK signature over new_cak
 */
struct nsm_dot_cak_rotate_req {
	uint8_t new_cak[DOT_KEY_AUTH_DATA_SIZE];
	uint8_t signature[DOT_SIGNATURE_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_cak_rotate_req_command
 *
 *  Structure representing DOT CAK ROTATE request command.
 *  Uses nsm_common_req_v2 for large payload support.
 */
struct nsm_dot_cak_rotate_req_command {
	struct nsm_common_req_v2 hdr;
	struct nsm_dot_cak_rotate_req dot_cak_rotate_req;
} __attribute__((packed));

/** @struct nsm_dot_cak_rotate_resp
 *
 *  Structure representing DOT CAK ROTATE response.
 *  Contains DOT blob (1024 bytes) on success.
 */
struct nsm_dot_cak_rotate_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t reserved;
	uint16_t data_size;
	uint8_t dot_blob[DOT_BLOB_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_lock_resp
 *
 *  Structure representing DOT LOCK response.
 *  Contains DOT blob (1024 bytes) on success.
 */
struct nsm_dot_lock_resp {
	uint8_t command; /* Command code (0x22) */
	uint8_t completion_code;
	uint16_t reserved;
	uint16_t data_size;
	uint8_t dot_blob[DOT_BLOB_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_unlock_challenge_req
 *
 *  Structure representing DOT UNLOCK CHALLENGE request.
 *  Command code: 0x27
 *  Total size: 16 bytes (12-byte header + 4-byte payload)
 */
struct nsm_dot_unlock_challenge_req {
	uint32_t unlock_type; /* 1=Owner_Unlock, 2=Vendor_Unlock */
} __attribute__((packed));

/** @struct nsm_dot_unlock_challenge_req_command
 *
 *  Structure representing DOT UNLOCK CHALLENGE request command.
 */
struct nsm_dot_unlock_challenge_req_command {
	struct nsm_common_req_v2 hdr;
	struct nsm_dot_unlock_challenge_req unlock_challenge_req;
} __attribute__((packed));

/** @struct nsm_dot_unlock_challenge_resp
 *
 *  Structure representing DOT UNLOCK CHALLENGE response.
 *  Contains a 32-byte challenge on success.
 */
struct nsm_dot_unlock_challenge_resp {
	uint8_t command; /* Command code (0x27) */
	uint8_t completion_code;
	uint16_t reserved;
	uint16_t data_size;
	uint8_t challenge[DOT_CHALLENGE_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_unlock_req
 *
 *  Structure representing DOT UNLOCK request parameters.
 *  According to spec:
 *  - signature: 1840-byte LAK signature over (LAK.pub | challenge)
 *    - ECDSA-only: 96 bytes ECDSA + 1744 bytes padding (zeros)
 *    - Hybrid: 96 bytes ECDSA + 1744 bytes LMS
 *  Total size: 1852 bytes (12-byte header + 1840-byte signature)
 */
struct nsm_dot_unlock_req {
	uint8_t signature[DOT_SIGNATURE_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_unlock_req_command
 *
 *  Structure representing DOT UNLOCK request command.
 *  Uses nsm_common_req_v2 for large payload support.
 */
struct nsm_dot_unlock_req_command {
	struct nsm_common_req_v2 hdr;
	struct nsm_dot_unlock_req dot_unlock_req;
} __attribute__((packed));

typedef struct nsm_common_resp nsm_dot_unlock_resp;

/** @struct nsm_dot_get_info_req
 *
 *  Structure representing DOT GET INFO request.
 *  Command code: 0x20
 *  Request contains only header (no additional data).
 */
typedef struct nsm_common_req_v2 nsm_dot_get_info_req;

/** @struct nsm_dot_get_info_resp
 *
 *  Structure representing DOT GET INFO response.
 *  Command code: 0x20
 *  Total size: 1040 bytes (16-byte header + 1024-byte payload)
 *  Contains DOT information including version, fuse state, transfers remaining,
 * and DOT blob.
 */
struct nsm_dot_get_info_resp {
	uint8_t command;	   /* Command code (0x20) */
	uint8_t completion_code;   /* Completion code */
	uint16_t reserved;	   /* Reserved field */
	uint16_t data_size;	   /* Size of data (1028 bytes) */
	uint16_t version;	   /* DOT commands format version */
	uint8_t fuse_change_state; /* Fuse state change indicator: 0x00=none,
				      0x01=in progress, 0x02=completed */
	uint8_t transfers_remaining;
	uint8_t dot_blob[DOT_BLOB_SIZE];
} __attribute__((packed));

/** @struct nsm_dot_get_status_req
 *
 *  Structure representing DOT GET STATUS request.
 *  Command code: 0x2A
 *  Request contains only header (no additional data).
 */
typedef struct nsm_common_req_v2 nsm_dot_get_status_req;

/** @struct nsm_dot_get_status_resp
 *
 *  Structure representing DOT GET STATUS response.
 *  Command code: 0x2A
 *  Total size: 13 bytes (12-byte header + 1-byte payload)
 *  Contains current DOT status.
 */
struct nsm_dot_get_status_resp {
	uint8_t command; /* Command code (0x2A) */
	uint8_t completion_code;
	uint16_t reserved;  /* Reserved field */
	uint16_t data_size; /* Size of data (1 byte) */
	uint8_t status;	    /* DOT state: 0=Uninitialized, 1=Volatile, 2=Mutable
			       Locked, 3=Mutable Disabled */
} __attribute__((packed));

/** @struct nsm_firmware_aggregate_tag
 *
 *  Structure representing firmware aggregate tag format
 */
struct nsm_firmware_aggregate_tag {
	uint8_t tag;
	uint8_t valid : 1;
	uint8_t length : 3;
	uint8_t reserved : 4;
	uint8_t data[1];
} __attribute__((packed));

/**
 * @brief Encode nsm firmware aggregate tag with uint8 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[in] tag - Tag value
 * @param[in] value - uint8 value to encode
 * @param[in,out] buffer_size - Pointer to buffer size
 */
void encode_nsm_firmware_aggregate_tag_uint8(uint8_t **buffer, uint8_t tag,
					     uint8_t value,
					     uint16_t *buffer_size);

/**
 * @brief Encode nsm firmware aggregate tag with uint16 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[in] tag - Tag value
 * @param[in] value - uint16 value to encode
 * @param[in,out] buffer_size - Pointer to buffer size
 */
void encode_nsm_firmware_aggregate_tag_uint16(uint8_t **buffer, uint8_t tag,
					      uint16_t value,
					      uint16_t *buffer_size);

/**
 * @brief Encode nsm firmware aggregate tag with uint32 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[in] tag - Tag value
 * @param[in] value - uint32 value to encode
 * @param[in,out] buffer_size - Pointer to buffer size
 */
void encode_nsm_firmware_aggregate_tag_uint32(uint8_t **buffer, uint8_t tag,
					      uint32_t value,
					      uint16_t *buffer_size);

/**
 * @brief Encode nsm firmware aggregate tag with uint64 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[in] tag - Tag value
 * @param[in] value - uint64 value to encode
 * @param[in,out] buffer_size - Pointer to buffer size
 */
void encode_nsm_firmware_aggregate_tag_uint64(uint8_t **buffer, uint8_t tag,
					      uint64_t value,
					      uint16_t *buffer_size);

/**
 * @brief Encode nsm firmware aggregate tag with uint8 array value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[in] tag - Tag value
 * @param[in] value - uint8 array value to encode
 * @param[in,out] buffer_size - Pointer to buffer size
 */
void encode_nsm_firmware_aggregate_tag_uint8_array(uint8_t **buffer,
						   uint8_t tag, uint8_t *value,
						   uint16_t *buffer_size);

/**
 * @brief Decode nsm firmware aggregate tag with uint8 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[out] tag - Tag value
 * @param[out] valid - Valid flag
 * @param[out] value - uint8 value decoded
 * @param[in,out] buffer_size - Pointer to buffer size
 * @return true on success, false on failure
 */
bool decode_nsm_firmware_aggregate_tag_uint8(uint8_t **buffer, uint8_t *tag,
					     uint8_t *valid, uint8_t *value,
					     uint16_t *buffer_size);

/**
 * @brief Decode nsm firmware aggregate tag with uint16 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[out] tag - Tag value
 * @param[out] valid - Valid flag
 * @param[out] value - uint16 value decoded
 * @param[in,out] buffer_size - Pointer to buffer size
 * @return true on success, false on failure
 */
bool decode_nsm_firmware_aggregate_tag_uint16(uint8_t **buffer, uint8_t *tag,
					      uint8_t *valid, uint16_t *value,
					      uint16_t *buffer_size);

/**
 * @brief Decode nsm firmware aggregate tag with uint32 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[out] tag - Tag value
 * @param[out] valid - Valid flag
 * @param[out] value - uint32 value decoded
 * @param[in,out] buffer_size - Pointer to buffer size
 * @return true on success, false on failure
 */
bool decode_nsm_firmware_aggregate_tag_uint32(uint8_t **buffer, uint8_t *tag,
					      uint8_t *valid, uint32_t *value,
					      uint16_t *buffer_size);

/**
 * @brief Decode nsm firmware aggregate tag with uint64 value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[out] tag - Tag value
 * @param[out] valid - Valid flag
 * @param[out] value - uint64 value decoded
 * @param[in,out] buffer_size - Pointer to buffer size
 * @return true on success, false on failure
 */
bool decode_nsm_firmware_aggregate_tag_uint64(uint8_t **buffer, uint8_t *tag,
					      uint8_t *valid, uint64_t *value,
					      uint16_t *buffer_size);

/**
 * @brief Decode nsm firmware aggregate tag with uint8 array value
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[out] tag - Tag value
 * @param[out] valid - Valid flag
 * @param[out] value - uint8 array value decoded
 * @param[in,out] buffer_size - Pointer to buffer size
 * @return true on success, false on failure
 */
bool decode_nsm_firmware_aggregate_tag_uint8_array(uint8_t **buffer,
						   uint8_t *tag, uint8_t *valid,
						   uint8_t *value,
						   uint16_t *buffer_size);

/**
 * @brief Skip nsm firmware aggregate tag by advancing buffer pointer
 *
 * @param[in,out] buffer - Pointer to buffer pointer
 * @param[in,out] buffer_size - Pointer to buffer size
 * @return true on success, false on failure
 */
bool decode_nsm_firmware_aggregate_tag_skip(uint8_t **buffer,
					    uint16_t *buffer_size);

/**
 * @brief Decode nsm query request erot state parameters message.
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] fw_req - Pointer to the NSM query erot state parameters
 * request
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
 * @param[in] fw_req - Pointer to the NSM query erot state parameters
 * request
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
 * @param[in] fw_req - Pointer to the NSM query erot state parameters
 * request
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
 * @param[out] fw_req - Pointer to the NSM query erot state parameters
 * request
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 *         and hdr params
 */
int decode_nsm_query_get_erot_state_parameters_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, struct nsm_firmware_erot_state_info_resp *fw_resp);

/**
 * @brief Encode nsm firmware Irreversible config request
 *
 * @param[in] instance_id - instance id
 * @param[in] fw_req - Irreversible config request
 * @param[out] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_irreversible_config_req(
    uint8_t instance_id,
    const struct nsm_firmware_irreversible_config_req *fw_req,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware Irreversible config request
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] fw_req - Irreversible config request
 * @return int
 */
int decode_nsm_firmware_irreversible_config_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_firmware_irreversible_config_req *fw_req);

/**
 * @brief Encode nsm firmware Irreversible config response for request 0
 *
 * @param[in] instance_id - instance id
 * @param[in] cc - command completion code
 * @param[in] reason_code - command reason code
 * @param[in] cfg_resp - Irreversible config response for request 0
 * @param[in] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_irreversible_config_request_0_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_firmware_irreversible_config_request_0_resp *cfg_resp,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware Irreversible config response for request 0
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @param[out] cfg_resp - Irreversible config response for request 0
 * @return int
 */
int decode_nsm_firmware_irreversible_config_request_0_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_firmware_irreversible_config_request_0_resp *cfg_resp);

/**
 * @brief Encode nsm firmware Irreversible config response for request 1
 *
 * @param[in] instance_id - instance id
 * @param[in] cc - command completion code
 * @param[in] reason_code - command reason code
 * @param[in] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_irreversible_config_request_1_resp(uint8_t instance_id,
							   uint8_t cc,
							   uint16_t reason_code,
							   struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware Irreversible config response for request 1
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @return int
 */
int decode_nsm_firmware_irreversible_config_request_1_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code);

/**
 * @brief Encode nsm firmware Irreversible config response for request 2
 *
 * @param[in] instance_id - instance id
 * @param[in] cc - command completion code
 * @param[in] reason_code - command reason code
 * @param[in] cfg_resp - Irreversible config response for request 2
 * @param[in] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_irreversible_config_request_2_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_firmware_irreversible_config_request_2_resp *cfg_resp,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware Irreversible config response for request 2
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @param[out] cfg_resp - Irreversible config response for request 2
 * @return int
 */
int decode_nsm_firmware_irreversible_config_request_2_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_firmware_irreversible_config_request_2_resp *cfg_resp);

/**
 * @brief Decode a code authentication key permissions query request
 * message.
 *
 * @param[in] msg - Pointer to the NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] component_classification - Pointer to store the component
 * classification
 * @param[out] component_identifier - Pointer to store the component
 * identifier
 * @param[out] component_classification_index - Pointer to store the
 * component classification index
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int decode_nsm_code_auth_key_perm_query_req(
    const struct nsm_msg *msg, size_t msg_len,
    uint16_t *component_classification, uint16_t *component_identifier,
    uint8_t *component_classification_index);

/**
 * @brief Encode a code authentication key permissions query request
 * message.
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] component_classification - Component classification value
 * @param[in] component_identifier - Component identifier value
 * @param[in] component_classification_index - Component classification
 * index
 * @param[out] msg - Pointer to the NSM message to be encoded
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int encode_nsm_code_auth_key_perm_query_req(
    uint8_t instance_id, uint16_t component_classification,
    uint16_t component_identifier, uint8_t component_classification_index,
    struct nsm_msg *msg);

/**
 * @brief Decode a code authentication key permissions query response
 * message.
 *
 * @param[in] msg - Pointer to the NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] cc - Command completion code
 * @param[out] reason_code - Reason code
 * @param[out] active_component_key_index - Pointer to store the active
 * component key index
 * @param[out] pending_component_key_index - Pointer to store the
 * pending component key index
 * @param[out] permission_bitmap_length - Pointer to store the length of
 * the permission bitmap
 * @param[out] active_component_key_perm_bitmap - Pointer to store the
 * active component key permissions bitmap
 * @param[out] pending_component_key_perm_bitmap - Pointer to store the
 * pending component key permissions bitmap
 * @param[out] efuse_key_perm_bitmap - Pointer to store the efuse key
 * permissions bitmap
 * @param[out] pending_efuse_key_perm_bitmap - Pointer to store the
 * pending efuse key permissions bitmap
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int decode_nsm_code_auth_key_perm_query_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code, uint16_t *active_component_key_index,
    uint16_t *pending_component_key_index, uint8_t *permission_bitmap_length,
    uint8_t *active_component_key_perm_bitmap,
    uint8_t *pending_component_key_perm_bitmap, uint8_t *efuse_key_perm_bitmap,
    uint8_t *pending_efuse_key_perm_bitmap);

/**
 * @brief Encode a code authentication key permissions query response
 * message.
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Command completion code
 * @param[in] reason_code - Reason code
 * @param[in] active_component_key_index - Active component key index
 * @param[in] pending_component_key_index - Pending component key index
 * @param[in] permission_bitmap_length - Length of the permission bitmap
 * @param[in] active_component_key_perm_bitmap - Pointer to the active
 * component key permissions bitmap
 * @param[in] pending_component_key_perm_bitmap - Pointer to the pending
 * component key permissions bitmap
 * @param[in] efuse_key_perm_bitmap - Pointer to the efuse key
 * permissions bitmap
 * @param[in] pending_efuse_key_perm_bitmap - Pointer to the pending
 * efuse key permissions bitmap
 * @param[out] msg - Pointer to the NSM message to be encoded
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int encode_nsm_code_auth_key_perm_query_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    uint16_t active_component_key_index, uint16_t pending_component_key_index,
    uint8_t permission_bitmap_length, uint8_t *active_component_key_perm_bitmap,
    uint8_t *pending_component_key_perm_bitmap, uint8_t *efuse_key_perm_bitmap,
    uint8_t *pending_efuse_key_perm_bitmap, struct nsm_msg *msg);

/**
 * @brief Decode a code authentication key permissions update request
 * message.
 *
 * @param[in] msg - Pointer to the NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] request_type - Pointer to store the request type
 * @param[out] component_classification - Pointer to store the component
 * classification
 * @param[out] component_identifier - Pointer to store the component
 * identifier
 * @param[out] component_classification_index - Pointer to store the
 * component classification index
 * @param[out] nonce - Pointer to store the nonce value
 * @param[out] permission_bitmap_length - Pointer to store the length of
 * the permission bitmap
 * @param[out] permission_bitmap - Pointer to store the permission
 * bitmap
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int decode_nsm_code_auth_key_perm_update_req(
    const struct nsm_msg *msg, size_t msg_len,
    enum nsm_code_auth_key_perm_request_type *request_type,
    uint16_t *component_classification, uint16_t *component_identifier,
    uint8_t *component_classification_index, uint64_t *nonce,
    uint8_t *permission_bitmap_length, uint8_t *permission_bitmap);

/**
 * @brief Encode a code authentication key permissions update request
 * message.
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] request_type - Type of the request
 * @param[in] component_classification - Component classification value
 * @param[in] component_identifier - Component identifier value
 * @param[in] component_classification_index - Component classification
 * index
 * @param[in] nonce - Nonce value
 * @param[in] permission_bitmap_length - Length of the permission bitmap
 * @param[in] permission_bitmap - Pointer to the permission bitmap
 * @param[out] msg - Pointer to the NSM message to be encoded
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int encode_nsm_code_auth_key_perm_update_req(
    uint8_t instance_id, enum nsm_code_auth_key_perm_request_type request_type,
    uint16_t component_classification, uint16_t component_identifier,
    uint8_t component_classification_index, uint64_t nonce,
    uint8_t permission_bitmap_length, uint8_t *permission_bitmap,
    struct nsm_msg *msg);

/**
 * @brief Decode a code authentication key permissions update response
 * message.
 *
 * @param[in] msg - Pointer to the NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] cc - Command completion code
 * @param[out] reason_code - Reason code
 * @param[out] update_method - Pointer to store the update method
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int decode_nsm_code_auth_key_perm_update_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *reason_code,
					      uint32_t *update_method);

/**
 * @brief Encode a code authentication key permissions update response
 * message.
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Command completion code
 * @param[in] reason_code - Reason code
 * @param[in] update_method - Update method value
 * @param[out] msg - Pointer to the NSM message to be encoded
 *
 * @return 0 on success, otherwise NSM error codes.
 */
int encode_nsm_code_auth_key_perm_update_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      uint32_t update_method,
					      struct nsm_msg *msg);

/**
 * @brief Decode nsm query firmware security version number req
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] fw_req - firmware security version request
 * @return int
 */
int decode_nsm_query_firmware_security_version_number_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_firmware_security_version_number_req *fw_req);

/**
 * @brief Encode nsm query firmware security version number req
 *
 * @param[in] instance_id - instance id
 * @param[in] fw_req - firmware security version request
 * @param[out] msg - nsm message
 * @return int
 */
int encode_nsm_query_firmware_security_version_number_req(
    uint8_t instance_id,
    const struct nsm_firmware_security_version_number_req *fw_req,
    struct nsm_msg *msg);

/**
 * @brief Encode nsm query firmware security version number Response
 *
 * @param[in] instance_id - instance id
 * @param[in] cc - command completion code
 * @param[in] reason_code - command reason code
 * @param[in] sec_info - firmware security version response
 * @param[in] msg - nsm message
 * @return int
 */
int encode_nsm_query_firmware_security_version_number_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_firmware_security_version_number_resp *sec_info,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm query firmware security version number Response
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @param[out] sec_info - firmware security version response
 * @return int
 */
int decode_nsm_query_firmware_security_version_number_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_firmware_security_version_number_resp *sec_resp);

/**
 * @brief Encode nsm firmware update security version number req
 *
 * @param[in] instance_id - instance id
 * @param[in] fw_req - firmware security version request
 * @param[out] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_update_sec_ver_req(
    uint8_t instance_id,
    const struct nsm_firmware_update_min_sec_ver_req *fw_req,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware update security version number req
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] fw_req - firmware security version request
 * @return int
 */
int decode_nsm_firmware_update_sec_ver_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_firmware_update_min_sec_ver_req *fw_req);

/**
 * @brief Encode nsm firmware update security version number response
 *
 * @param[in] instance_id - instance id
 * @param[in] cc - command completion code
 * @param[in] reason_code - command reason code
 * @param[in] sec_resp - firmware security version response
 * @param[in] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_update_sec_ver_resp(
    uint8_t instance_id, uint8_t cc, uint16_t reason_code,
    struct nsm_firmware_update_min_sec_ver_resp *sec_resp, struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware update security version number response
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @param[out] sec_resp - firmware security version response
 * @return int
 */
int decode_nsm_firmware_update_sec_ver_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_firmware_update_min_sec_ver_resp *sec_resp);

/**
 * @brief Encode nsm firmware set rot property req
 *
 * @param[in] instance_id - instance id
 * @param[in] fw_req - firmware set rot property request
 * @param[out] msg - nsm message
 * @return int
 */
int encode_nsm_firmware_set_rot_property_req(
    uint8_t instance_id, const struct nsm_firmware_set_rot_property_req *fw_req,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware set rot property resp
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @param[out] resp - firmware set rot property response
 * @return int
 */
int decode_nsm_firmware_set_rot_property_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc,
					      uint16_t *reason_code);

/**
 * @brief Encode nsm firmware image copy control request
 *
 * @param[in] instance_id - instance id
 * @param[in] image_copy_control_req - image copy control request
 * @param[in] component_entries - array of component entries (can be NULL if
 * component_count is 0)
 * @param[in] msg - nsm message (must be pre-allocated with sufficient size)
 * @return int - NSM_SW_SUCCESS on success, error code otherwise
 *
 * @note The caller must ensure msg buffer is large enough to hold:
 *       sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
 *       sizeof(nsm_firmware_image_copy_control_req) +
 *       (component_count * sizeof(nsm_firmware_image_copy_component_entry))
 */
int encode_nsm_firmware_image_copy_control_req(
    uint8_t instance_id,
    const struct nsm_firmware_image_copy_control_req *image_copy_control_req,
    const struct nsm_firmware_image_copy_component_entry *component_entries,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm firmware image copy control response for Query
 * Progress
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @param[out] image_copy_control_query - image copy control query
 * @return int
 */
int decode_nsm_firmware_image_copy_control_query_progress_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code,
    struct nsm_firmware_image_copy_control_query_progress_resp
	*image_copy_control_query);

/**
 * @brief Decode nsm firmware image copy control response for Initiate One
 * Component and Initiate All Components commands
 *
 * @param[in] msg - nsm message
 * @param[in] msg_len - message length
 * @param[out] cc - command completion code
 * @param[out] reason_code - command reason code
 * @return int
 */
int decode_nsm_firmware_image_copy_control_initiate_copy_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint16_t *reason_code);

/**
 * @brief Encode nsm firmware set rot property resp
 *
 * @param[in] instance_id - instance id
 * @param[in] cc - command completion code
 * @param[in] reason_code - command reason code
 * @param[out] msg - nsm response message
 * @return nsm_sw return codes
 */
int encode_nsm_firmware_set_rot_property_resp(uint8_t instance_id, uint8_t cc,
					      uint16_t reason_code,
					      struct nsm_msg *msg);

/**
 * @brief Encode nsm DotCAKInstall request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] dot_cak_req - Pointer to the DotCAKInstall request parameters
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int encode_nsm_dot_cak_install_req(
    uint8_t instance_id, const struct nsm_dot_cak_install_req *dot_cak_req,
    struct nsm_msg *msg);

/**
 * @brief Decode nsm DotCAKInstall request message
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] dot_cak_req - Pointer to the DotCAKInstall request parameters
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int decode_nsm_dot_cak_install_req(const struct nsm_msg *msg, size_t msg_len,
				   struct nsm_dot_cak_install_req *dot_cak_req);

/**
 * @brief Encode nsm DotCAKInstall response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Command completion code
 * @param[in] reason_code - Reason code
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int encode_nsm_dot_cak_install_resp(uint8_t instance_id, uint8_t cc,
				    uint16_t reason_code, struct nsm_msg *msg);

/**
 * @brief Decode nsm DotCAKInstall response message
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] cc - Command completion code
 * @param[out] reason_code - Reason code
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 * @note   Response uses standard nsm_common_resp structure
 */
int decode_nsm_dot_cak_install_resp(const struct nsm_msg *msg, size_t msg_len,
				    uint8_t *cc, uint16_t *reason_code);

/**
 * @brief Encode nsm DotCAKBypass request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int encode_nsm_dot_cak_bypass_req(uint8_t instance_id, struct nsm_msg *msg);

/**
 * @brief Decode nsm DotCAKBypass request message
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int decode_nsm_dot_cak_bypass_req(const struct nsm_msg *msg, size_t msg_len);

/**
 * @brief Encode nsm DotCAKBypass response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Command completion code
 * @param[in] reason_code - Reason code
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int encode_nsm_dot_cak_bypass_resp(uint8_t instance_id, uint8_t cc,
				   uint16_t reason_code, struct nsm_msg *msg);

/**
 * @brief Decode nsm DotCAKBypass response message
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] cc - Command completion code
 * @param[out] reason_code - Reason code
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int decode_nsm_dot_cak_bypass_resp(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *cc, uint16_t *reason_code);

/**
 * @brief Encode nsm DOT LOCK request message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] dot_lock_req - Pointer to the DOT LOCK request parameters
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int encode_nsm_dot_lock_req(uint8_t instance_id,
			    const struct nsm_dot_lock_req *dot_lock_req,
			    struct nsm_msg *msg);

/**
 * @brief Decode nsm DOT LOCK request message
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] dot_lock_req - Pointer to the DOT LOCK request parameters
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int decode_nsm_dot_lock_req(const struct nsm_msg *msg, size_t msg_len,
			    struct nsm_dot_lock_req *dot_lock_req);

/**
 * @brief Encode nsm DOT LOCK response message
 *
 * @param[in] instance_id - NSM instance ID
 * @param[in] cc - Command completion code
 * @param[in] reason_code - Reason code (for error response)
 * @param[in] dot_blob - Pointer to DOT blob data (1024 bytes, NULL for error)
 * @param[out] msg - Pointer to NSM message
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 */
int encode_nsm_dot_lock_resp(uint8_t instance_id, uint8_t cc,
			     uint16_t reason_code, const uint8_t *dot_blob,
			     struct nsm_msg *msg);

/**
 * @brief Decode nsm DOT LOCK response message
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of the received message
 * @param[out] cc - Command completion code
 * @param[out] reason_code - Reason code
 * @param[out] dot_blob - Pointer to buffer for DOT blob (1024 bytes, can be
 * NULL)
 *
 * @return 0 on success, otherwise NSM error codes.
 * @note   Caller is responsible for alloc and dealloc of msg
 * @note   If dot_blob is NULL, blob data is not copied (useful for checking
 * success)
 */
int decode_nsm_dot_lock_resp(const struct nsm_msg *msg, size_t msg_len,
			     uint8_t *cc, uint16_t *reason_code,
			     uint8_t *dot_blob);

/**
 * @brief Encode NSM DOT UNLOCK CHALLENGE request
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] unlock_challenge_req - Pointer to DOT unlock challenge request
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_unlock_challenge_req(
    uint8_t instance_id,
    const struct nsm_dot_unlock_challenge_req *unlock_challenge_req,
    struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT UNLOCK CHALLENGE request
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] unlock_challenge_req - Pointer to store decoded request
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_unlock_challenge_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_dot_unlock_challenge_req *unlock_challenge_req);

/**
 * @brief Encode NSM DOT UNLOCK CHALLENGE response
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code (used if completion code indicates
 * error)
 * @param[in] challenge - Pointer to 32-byte challenge (NULL if error)
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_unlock_challenge_resp(uint8_t instance_id, uint8_t cc,
					 uint16_t reason_code,
					 const uint8_t *challenge,
					 struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT UNLOCK CHALLENGE response
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] cc - Pointer to store completion code
 * @param[out] reason_code - Pointer to store reason code
 * @param[out] challenge - Pointer to store 32-byte challenge
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_unlock_challenge_resp(const struct nsm_msg *msg,
					 size_t msg_len, uint8_t *cc,
					 uint16_t *reason_code,
					 uint8_t *challenge);

/**
 * @brief Encode NSM DOT UNLOCK request
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] dot_unlock_req - Pointer to DOT unlock request
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_unlock_req(uint8_t instance_id,
			      const struct nsm_dot_unlock_req *dot_unlock_req,
			      struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT UNLOCK request
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] dot_unlock_req - Pointer to store decoded request
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_unlock_req(const struct nsm_msg *msg, size_t msg_len,
			      struct nsm_dot_unlock_req *dot_unlock_req);

/**
 * @brief Encode NSM DOT UNLOCK response
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code (used if completion code indicates
 * error)
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_unlock_resp(uint8_t instance_id, uint8_t cc,
			       uint16_t reason_code, struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT UNLOCK response
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] cc - Pointer to store completion code
 * @param[out] reason_code - Pointer to store reason code
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_unlock_resp(const struct nsm_msg *msg, size_t msg_len,
			       uint8_t *cc, uint16_t *reason_code);

/**
 * @brief Encode NSM DOT GET INFO request
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_get_info_req(uint8_t instance_id, struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT GET INFO request
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_get_info_req(const struct nsm_msg *msg, size_t msg_len);

/**
 * @brief Encode NSM DOT GET INFO response
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code (used if completion code indicates
 * error)
 * @param[in] version - DOT commands format version
 * @param[in] fuse_change_state - Fuse state change indicator
 * @param[in] transfers_remaining - Number of ownership transfers remaining
 * @param[in] dot_blob - Pointer to 1024-byte DOT blob (NULL if error)
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_get_info_resp(uint8_t instance_id, uint8_t cc,
				 uint16_t reason_code, uint16_t version,
				 uint8_t fuse_change_state,
				 uint8_t transfers_remaining,
				 const uint8_t *dot_blob, struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT GET INFO response
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] cc - Pointer to store completion code
 * @param[out] reason_code - Pointer to store reason code
 * @param[out] version - Pointer to store DOT commands format version
 * @param[out] fuse_change_state - Pointer to store fuse state change indicator
 * @param[out] transfers_remaining - Pointer to store number of transfers
 * remaining
 * @param[out] dot_blob - Pointer to store 1024-byte DOT blob
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_get_info_resp(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *cc, uint16_t *reason_code,
				 uint16_t *version, uint8_t *fuse_change_state,
				 uint8_t *transfers_remaining,
				 uint8_t *dot_blob);

/**
 * @brief Encode NSM DOT GET STATUS request
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_get_status_req(uint8_t instance_id, struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT GET STATUS request
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_get_status_req(const struct nsm_msg *msg, size_t msg_len);

/**
 * @brief Encode NSM DOT GET STATUS response
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code (used if completion code indicates
 * error)
 * @param[in] status - DOT status (0-3)
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_get_status_resp(uint8_t instance_id, uint8_t cc,
				   uint16_t reason_code, uint8_t status,
				   struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT GET STATUS response
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] cc - Pointer to store completion code
 * @param[out] reason_code - Pointer to store reason code
 * @param[out] status - Pointer to store DOT status
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_get_status_resp(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *cc, uint16_t *reason_code,
				   uint8_t *status);

/**
 * @brief Encode NSM DOT CAK ROTATE request
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] dot_cak_rotate_req - Pointer to DOT CAK ROTATE request
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_cak_rotate_req(
    uint8_t instance_id,
    const struct nsm_dot_cak_rotate_req *dot_cak_rotate_req,
    struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT CAK ROTATE request
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] dot_cak_rotate_req - Pointer to store decoded request
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_cak_rotate_req(
    const struct nsm_msg *msg, size_t msg_len,
    struct nsm_dot_cak_rotate_req *dot_cak_rotate_req);

/**
 * @brief Encode NSM DOT CAK ROTATE response
 *
 * @param[in] instance_id - Instance ID for the message
 * @param[in] cc - Completion code
 * @param[in] reason_code - Reason code (used if completion code indicates
 * error)
 * @param[in] dot_blob - Pointer to 1024-byte DOT blob (NULL if error)
 * @param[out] msg - Pointer to NSM message buffer
 * @return 0 on success, negative error code on failure
 */
int encode_nsm_dot_cak_rotate_resp(uint8_t instance_id, uint8_t cc,
				   uint16_t reason_code,
				   const uint8_t *dot_blob,
				   struct nsm_msg *msg);

/**
 * @brief Decode NSM DOT CAK ROTATE response
 *
 * @param[in] msg - Pointer to NSM message
 * @param[in] msg_len - Length of message
 * @param[out] cc - Pointer to store completion code
 * @param[out] reason_code - Pointer to store reason code
 * @param[out] dot_blob - Pointer to store 1024-byte DOT blob (can be NULL)
 * @return 0 on success, negative error code on failure
 */
int decode_nsm_dot_cak_rotate_resp(const struct nsm_msg *msg, size_t msg_len,
				   uint8_t *cc, uint16_t *reason_code,
				   uint8_t *dot_blob);

#ifdef __cplusplus
}
#endif

#endif
