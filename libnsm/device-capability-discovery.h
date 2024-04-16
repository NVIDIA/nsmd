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

#ifndef DEVICE_CAPABILITY_DISCOVERY_H
#define DEVICE_CAPABILITY_DISCOVERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"
#include <stdbool.h>

#define NSM_GET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE 1
#define NSM_SET_EVENT_SUBSCRIPTION_REQ_DATA_SIZE 2
#define NSM_SET_CURRENT_EVENT_SOURCES_REQ_DATA_SIZE 9
#define NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT_REQ_DATA_SIZE 9
#define NSM_GET_EVENT_LOG_RECORD_RESP_MIN_DATA_SIZE 14

#define EVENT_SOURCES_LENGTH 8
#define EVENT_ACKNOWLEDGEMENT_MASK_LENGTH EVENT_SOURCES_LENGTH

/** @brief NSM Device Capability Discovery event ID
 */
enum nsm_device_capability_discovery_event_id {
	NSM_REDISCOVERY_EVENT = 1,
};

typedef enum {
	GLOBAL_EVENT_GENERATION_DISABLE = 0,
	GLOBAL_EVENT_GENERATION_ENABLE_POLLING = 1,
	GLOBAL_EVENT_GENERATION_ENABLE_PUSH = 2
} NsmGlobalEventGenerationSetting;

/** @struct nsm_get_supported_event_source_req
 *
 *  Structure representing NSM get supported event source request
 */
struct nsm_get_supported_event_source_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t nvidia_message_type;
} __attribute__((packed));

/** @struct nsm_get_supported_event_source_resp
 *
 *  Structure representing NSM get supported event source respones
 */
struct nsm_get_supported_event_source_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	bitfield8_t supported_event_sources[EVENT_SOURCES_LENGTH];
} __attribute__((packed));

/** @struct nsm_get_current_event_source_req
 *
 *  Structure representing NSM get current event source request
 */
struct nsm_get_current_event_source_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t nvidia_message_type;
} __attribute__((packed));

/** @struct nsm_get_current_event_source_resp
 *
 *  Structure representing NSM get current event source respones
 */
struct nsm_get_current_event_source_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
} __attribute__((packed));

/** @struct nsm_set_supported_event_source_req
 *
 *  Structure representing NSM set current event source request
 */
struct nsm_set_current_event_source_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t nvidia_message_type;
	bitfield8_t event_sources[EVENT_SOURCES_LENGTH];
} __attribute__((packed));

/** @struct nsm_set_supported_event_source_resp
 *
 *  Structure representing NSM set current event source respones
 */
struct nsm_set_current_event_source_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
} __attribute__((packed));

/** @struct nsm_set_event_subscription_req
 *
 *  Structure representing NSM set event subscription request
 */
struct nsm_set_event_subscription_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t global_event_generation_setting;
	uint8_t receiver_endpoint_id;
} __attribute__((packed));

/** @struct nsm_set_event_subscription_resp
 *
 *  Structure representing NSM set event subscription respones
 */
struct nsm_set_event_subscription_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
} __attribute__((packed));

/** @struct nsm_get_event_log_record_req
 *
 *  Structure representing NSM get event log record request
 */
struct nsm_get_event_log_record_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t selector_type;
	uint32_t selector;
} __attribute__((packed));

/** @struct nsm_get_event_log_record_resp
 *
 *  Structure representing NSM get event log record response
 */
struct nsm_get_event_log_record_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	uint8_t nvidia_message_type;
	uint8_t event_id;
	uint32_t event_handle;
	uint64_t timestamp;
	uint8_t payload[1];
} __attribute__((packed));

/** @struct nsm_configure_event_acknowledgement_req
 *
 *  Structure representing NSM set event subscription request
 */
struct nsm_configure_event_acknowledgement_req {
	uint8_t command;
	uint8_t data_size;
	uint8_t nvidia_message_type;
	bitfield8_t current_event_sources_acknowledgement_mask
	    [EVENT_ACKNOWLEDGEMENT_MASK_LENGTH];
} __attribute__((packed));

/** @struct nsm_configure_event_acknowledgement_resp
 *
 *  Structure representing NSM set event subscription respones
 */
struct nsm_configure_event_acknowledgement_resp {
	uint8_t command;
	uint8_t completion_code;
	uint16_t data_size;
	bitfield8_t new_event_sources_acknowledgement_mask
	    [EVENT_ACKNOWLEDGEMENT_MASK_LENGTH];
} __attribute__((packed));

/** @brief Create a get supported event sources request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] nvidia_message_type - message type for which event IDS are
 * requested
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_get_supported_event_source_req(uint8_t instance_id,
					      uint8_t nvidia_message_type,
					      struct nsm_msg *msg);

/** @brief Decode a get supported event source response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to completion code
 *  @param[out] supported_event_sources  - pointer to array bitfield8_t[8], each
 * bit represents whether the given event ID is supported for the NVIDIA Message
 * Type
 *
 *  @return nsm_completion_codes
 */
int decode_nsm_get_supported_event_source_resp(const struct nsm_msg *msg,
					       size_t msg_len, uint8_t *cc,
					       bitfield8_t **event_sources);

/** @brief Create a set current event sources request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] nvidia_message_type - message type for current event source
 *  @param[in] supported_event_sources - pointer to array bitfield8_t[8], each
 * bit represents whether the given event ID is supported for the NVIDIA Message
 * type
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_set_current_event_sources_req(uint8_t instance_id,
					     uint8_t nvidia_message_type,
					     bitfield8_t *event_sources,
					     struct nsm_msg *msg);

/** @brief Decode a set current event sources request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] nvidia_message_type - message type for current event source
 *  @param[out] event_sources - a pointer to event_source array
 *  @return nsm_completion_codes
 */
int decode_nsm_set_current_event_source_req(const struct nsm_msg *msg,
					    size_t msg_len,
					    uint8_t *nvidia_message_type,
					    bitfield8_t **event_sources);

/** @brief Decode a set current event source response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to completion code
 *  @return nsm_completion_codes
 */
int decode_nsm_set_current_event_sources_resp(const struct nsm_msg *msg,
					      size_t msg_len, uint8_t *cc);

/** @brief Create a set event subscription request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] global_setting - global event generation setting
 *  @param[in] receiver_eid - receiver endpoint ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_set_event_subscription_req(uint8_t instance_id,
					  uint8_t global_setting,
					  uint8_t receiver_eid,
					  struct nsm_msg *msg);

/** @brief Decode a set event subscription request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] global_setting - global event generation setting
 *  @param[out] receiver_eid - receiver endpoint ID
 *  @return nsm_completion_codes
 */
int decode_nsm_set_event_subscription_req(const struct nsm_msg *msg,
					  size_t msg_len,
					  uint8_t *global_setting,
					  uint8_t *receiver_eid);

/** @brief Decode a set event subscription response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to completion code
 *  @return nsm_completion_codes
 */
int decode_nsm_set_event_subscription_resp(const struct nsm_msg *msg,
					   size_t msg_len, uint8_t *cc);

/** @brief Create a Get Event Log Record request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] selector_type - determines the selector field to use for event
 * log record lookup
 *  @param[in] selector - selector type: 0=event handle, 1=timestamp in seconds
 * from epoch UTC
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_get_event_log_record_req(uint8_t instance_id,
					uint8_t selector_type,
					uint32_t selector, struct nsm_msg *msg);

/** @brief Decode a Get Event Log Record response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to completion code
 *  @param[out] nvidia_message_type - pointer to message type for the event
 *  @param[out] event_id - pointer to identifier for the event.
 *  @param[out] event_handle - pointer to the integer handle for the event
 * record that uniquely identifies it in the event log
 *  @param[out] timestamp - pointer to the time at which the log entry was added
 * to the event log
 *  @param[out] payload - pointer to payload address in response message
 *  @param[out] payload_len - pointer to payload length
 *  @return nsm_completion_codes
 */
int decode_nsm_get_event_log_record_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    uint8_t *nvidia_message_type, uint8_t *event_id, uint32_t *event_handle,
    uint64_t *timestamp, uint8_t **payload, uint16_t *payload_len);

/** @brief Create a configure event acknowledgement request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] nvidia_message_type - message type for which event IDS are
 * requested
 *  @param[in] current_event_sources_acknowledgement_mask - pointer to array
 * bitfield8_t[8], each bit represents whether the given event ID will generate
 * an acknowledgement
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_configure_event_acknowledgement_req(
    uint8_t instance_id, uint8_t nvidia_message_type,
    bitfield8_t *current_event_sources_acknowledgement_mask,
    struct nsm_msg *msg);

/** @brief Decode a configure event acknowledgement request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @param[out] nvidia_message_type - target NVIDIA message type for which event
 * sources are being configured
 *  @param[out] current_event_sources_acknowledgement_mask - a pointer to
 * current event sources acknowledgement mask array bitfield8_t[8]
 *  @return nsm_completion_codes
 */
int decode_nsm_configure_event_acknowledgement_req(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *nvidia_message_type,
    bitfield8_t **current_event_sources_acknowledgement_mask);

/** @brief Encode configure event acknowledgement response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] new_event_sources_acknowledgement_mask - pointer to array
 * bitfield8_t[8], each bit represents whether the given event ID will generate
 * an acknowledgement
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_configure_event_acknowledgement_resp(
    uint8_t instance_id, uint8_t cc,
    bitfield8_t *new_event_sources_acknowledgement_mask, struct nsm_msg *msg);

/** @brief Decode configure event acknowledgement response message
 *
 *  @param[in] msg - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to completion code
 *  @param[out] new_event_sources_acknowledgement_mask  - pointer to array
 * bitfield8_t[8], each bit represents whether the given event ID will generate
 * an acknowledgement
 *  @return nsm_completion_codes
 */
int decode_nsm_configure_event_acknowledgement_resp(
    const struct nsm_msg *msg, size_t msg_len, uint8_t *cc,
    bitfield8_t **new_event_sources_acknowledgement_mask);

/** @brief Create a rediscovery event message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] ackr - acknowledgement request
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_nsm_rediscovery_event(uint8_t instance_id, bool ackr,
				 struct nsm_msg *msg);

/** @brief Decode a rediscovery event message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc     - pointer to completion code
 *  @return nsm_completion_codes
 */
int decode_nsm_rediscovery_event(const struct nsm_msg *msg, size_t msg_len,
				 uint8_t *event_class, uint16_t *event_state);

#ifdef __cplusplus
}
#endif
#endif /* DEVICE_CAPABILITY_DISCOVERY_H */
