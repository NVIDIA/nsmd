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

#ifndef POWERSMOOTHING_POWERPROFILE_API_V2_H
#define POWERSMOOTHING_POWERPROFILE_API_V2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base.h"
#include "platform-environmental.h"
#include <stdbool.h>

// Get feature info tag
const uint8_t FEATURE_FLAG_TAG = 0;
const uint8_t CURRENT_TMP_SETTING_TAG = 1;
const uint8_t TMP_FLOOR_SETTING_TAG = 2;
const uint8_t MAX_TMP_FLOOR_SETTING_TAG = 3;
const uint8_t MIN_TMP_FLOOR_SETTING_TAG = 4;
const uint8_t FLOOR_WINDOW_MULTIPLIER_TAG = 5;
const uint8_t MIN_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG = 6;
const uint8_t MIN_PRIMARY_FLOOR_ACTIVATION_POINT_TAG = 7;

// Get current profile tag
const uint8_t ACTIVE_PRESET_PROFILE_ID_TAG = 0;
const uint8_t ADMIN_OVERRIDE_MASK_TAG = 1;
const uint8_t CURRENT_TMP_FLOOR_SETTING_TAG = 2;
const uint8_t CURRENT_RAMPUP_RATE_TAG = 3;
const uint8_t CURRENT_RAMPDOWN_RATE_TAG = 4;
const uint8_t CURRENT_RAMPDOWN_HYSTERESIS_TAG = 5;
const uint8_t CURRENT_SECONDARY_FLOOR_TAG = 6;
const uint8_t CURRENT_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG = 7;
const uint8_t CURRENT_PRIMARY_FLOOR_TARGET_WINDOW_TAG = 8;
const uint8_t CURRENT_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG = 9;

// Get preset profile tag
const uint8_t PRESET_PROFILE_TMP_FLOOR_SETTING_TAG = 0;
const uint8_t PRESET_PROFILE_RAMPUP_RATE_TAG = 1;
const uint8_t PRESET_PROFILE_RAMPDOWN_RATE_TAG = 2;
const uint8_t PRESET_PROFILE_RAMPDOWN_HYSTERESIS_TAG = 3;
const uint8_t PRESET_PROFILE_SECONDARY_FLOOR_TAG = 4;
const uint8_t PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG = 5;
const uint8_t PRESET_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG = 6;
const uint8_t PRESET_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG = 7;

// Get admin override profile tag
const uint8_t ADMIN_OVERRIDE_PROFILE_TMP_FLOOR_SETTING_TAG = 0;
const uint8_t ADMIN_OVERRIDE_PROFILE_RAMPUP_RATE_TAG = 1;
const uint8_t ADMIN_OVERRIDE_PROFILE_RAMPDOWN_RATE_TAG = 2;
const uint8_t ADMIN_OVERRIDE_PROFILE_RAMPDOWN_HYSTERESIS_TAG = 3;
const uint8_t ADMIN_OVERRIDE_PROFILE_SECONDARY_FLOOR_TAG = 4;
const uint8_t
    ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_WINDOW_MULTIPLIER_TAG = 5;
const uint8_t ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_TARGET_WINDOW_TAG = 6;
const uint8_t ADMIN_OVERRIDE_PROFILE_PRIMARY_FLOOR_ACTIVATION_OFFSET_TAG = 7;

int encode4ByteOfUint32Sample(uint32_t sample, uint8_t *data, size_t *data_len);
int decode4ByteOfUint32Sample(const uint8_t *data, size_t data_len,
			      uint32_t *sample);
int encode2ByteOfUint16Sample(uint16_t sample, uint8_t *data, size_t *data_len);
int decode2ByteOfUint16Sample(const uint8_t *data, size_t data_len,
			      uint16_t *sample);

/** @brief Encode a Get Power Smoothing Feature Info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_powersmoothing_featinfo_v2_req(uint8_t instance_id,
					      struct nsm_msg *msg);

/** @brief Decode a Get Power Smoothing Feature Info request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_powersmoothing_featinfo_v2_req(const struct nsm_msg *msg,
					      size_t msg_len);

/** @brief Encode a Feature Flag sample
 *
 *  @param[in] feature_flag - feature flag
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeFeatureFlagSample(uint32_t feature_flag, uint8_t *data,
			    size_t *data_len);

/** @brief Decode a Feature Flag sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] feature_flag - feature flag
 *  @return nsm_completion_codes
 */
int decodeFeatureFlagSample(const uint8_t *data, size_t data_len,
			    uint32_t *feature_flag);

/** @brief Encode a Current TMP Floor Setting sample
 *
 *  @param[in] current_tmp_floor_setting - current TMP floor setting
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentTMPFloorSettingSample(uint32_t current_tmp_floor_setting,
				       uint8_t *data, size_t *data_len);

/** @brief Decode a Current TMP Floor Setting sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_tmp_floor_setting - current TMP floor setting
 *  @return nsm_completion_codes
 */
int decodeCurrentTMPFloorSettingSample(const uint8_t *data, size_t data_len,
				       uint32_t *current_tmp_floor_setting);

/** @brief Encode a Current TMP Setting sample
 *
 *  @param[in] current_tmp_setting - current TMP setting
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentTMPSettingSample(uint32_t current_tmp_setting, uint8_t *data,
				  size_t *data_len);

/** @brief Decode a Current TMP Setting sample

 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_tmp_setting - current TMP setting
 *  @return nsm_completion_codes
 */
int decodeCurrentTMPSettingSample(const uint8_t *data, size_t data_len,
				  uint32_t *current_tmp_setting);

/** @brief Encode a Max TMP Floor Setting sample
 *
 *  @param[in] max_tmp_floor_setting - max TMP floor setting
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeMaxTmpFloorSettingSample(uint16_t max_tmp_floor_setting,
				   uint8_t *data, size_t *data_len);

/** @brief Decode a Max TMP Floor Setting sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] max_tmp_floor_setting - max TMP floor setting
 *  @return nsm_completion_codes
 */
int decodeMaxTmpFloorSettingSample(const uint8_t *data, size_t data_len,
				   uint16_t *max_tmp_floor_setting);

/** @brief Encode a Min TMP Floor Setting sample
 *
 *  @param[in] min_tmp_floor_setting - min TMP floor setting
 *  @param[in] min_tmp_floor_setting - min TMP floor setting
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeMinTmpFloorSettingSample(uint16_t min_tmp_floor_setting,
				   uint8_t *data, size_t *data_len);

/** @brief Decode a Min TMP Floor Setting sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] min_tmp_floor_setting - min TMP floor setting
 *  @param[out] min_tmp_floor_setting - min TMP floor setting
 *  @return nsm_completion_codes
 */
int decodeMinTmpFloorSettingSample(const uint8_t *data, size_t data_len,
				   uint16_t *min_tmp_floor_setting);

/** @brief Encode a Current Secondary Floor Setting sample

 *  @param[in] floor_window_multiplier - floor window multiplier
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeFloorWindowMultiplierSample(uint32_t floor_window_multiplier,
				      uint8_t *data, size_t *data_len);

/** @brief Decode a Current Secondary Floor Setting sample

 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] floor_window_multiplier - floor window multiplier
 *  @return nsm_completion_codes
 */
int decodeFloorWindowMultiplierSample(const uint8_t *data, size_t data_len,
				      uint32_t *floor_window_multiplier);

/** @brief Encode a Current Target Average Power sample

 *  @param[in] min_primary_floor_activation_offset - min primary floor
 activation offset
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeMinPrimaryFloorActivationOffset(
    uint32_t min_primary_floor_activation_offset, uint8_t *data,
    size_t *data_len);

/** @brief Decode a Current Target Average Power sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] min_primary_floor_activation_offset - min primary floor
 * activation offset
 *  @return nsm_completion_codes
 */
int decodeMinPrimaryFloorActivationOffset(
    const uint8_t *data, size_t data_len,
    uint32_t *min_primary_floor_activation_offset);

/** @brief Encode a Min Primary Floor Activation Point sample
 *
 *  @param[in] min_primary_floor_activation_point - min primary floor
 * activation point
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeMinPrimaryFloorActivationPoint(
    uint32_t min_primary_floor_activation_point, uint8_t *data,
    size_t *data_len);

/** @brief Decode a Min Primary Floor Activation Point sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] min_primary_floor_activation_point - min primary floor
 * activation point
 *  @return nsm_completion_codes
 */
int decodeMinPrimaryFloorActivationPoint(
    const uint8_t *data, size_t data_len,
    uint32_t *min_primary_floor_activation_point);

/** @brief Encode a Get Power Smoothing Feature Info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] telemetry_count - telemetry count
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_powersmoothing_featinfo_v2_resp(uint8_t instance_id, uint8_t cc,
					       uint16_t telemetry_count,
					       struct nsm_msg *msg);

/** @brief Decode a Get Power Smoothing Feature Info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @return nsm_completion_codes
 */
int decode_get_powersmoothing_featinfo_v2_resp(const struct nsm_msg *msg,
					       size_t msg_len,
					       size_t *consumed_len,
					       uint8_t *cc,
					       uint16_t *telemetry_count);

/** @brief Encode a Get Current Profile Info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_profile_info_v2_req(uint8_t instance_id,
					   struct nsm_msg *msg);

/** @brief Decode a Get Current Profile Info request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_current_profile_info_v2_req(const struct nsm_msg *msg,
					   size_t msg_len);

/** @brief Encode a Active Preset Profile sample
 *
 *  @param[in] active_preset_profile_id - active preset profile id
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeActivePresetProfileSample(uint8_t active_preset_profile_id,
				    uint8_t *data, size_t *data_len);

/** @brief Decode a Active Preset Profile sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] active_preset_profile_id - active preset profile id
 *  @return nsm_completion_codes
 */
int decodeActivePresetProfileSample(const uint8_t *data, size_t data_len,
				    uint8_t *active_preset_profile_id);

/** @brief Encode a Admin Override Mask sample
 *
 *  @param[in] admin_override_id - admin override id
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeAdminOverrideMaskSample(uint16_t admin_override_id, uint8_t *data,
				  size_t *data_len);

/** @brief Decode a Admin Override Mask sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] admin_override_id - admin override id
 *  @return nsm_completion_codes
 */
int decodeAdminOverrideMaskSample(const uint8_t *data, size_t data_len,
				  uint16_t *admin_override_id);

/** @brief Encode a Current TMP Floor sample
 *
 *  @param[in] current_tmp_floor_setting - current TMP floor setting
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentTMPFloorSample(uint16_t current_tmp_floor_setting,
				uint8_t *data, size_t *data_len);

/** @brief Decode a Current TMP Floor sample

 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_tmp_floor_setting - current TMP floor setting
 *  @return nsm_completion_codes
 */
int decodeCurrentTMPFloorSample(const uint8_t *data, size_t data_len,
				uint16_t *current_tmp_floor_setting);

/** @brief Encode a Current Rampup Rate sample
 *
 *  @param[in] current_rampup_rate - current rampup rate
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentRampupRateSample(uint32_t current_rampup_rate, uint8_t *data,
				  size_t *data_len);

/** @brief Decode a Current Rampup Rate sample

 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_rampup_rate - current rampup rate
 *  @return nsm_completion_codes
 */
int decodeCurrentRampupRateSample(const uint8_t *data, size_t data_len,
				  uint32_t *current_rampup_rate);

/** @brief Encode a Current Rampdown Rate sample
 *
 *  @param[in] current_rampdown_rate - current rampdown rate
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentRampdownRateSample(uint32_t current_rampdown_rate,
				    uint8_t *data, size_t *data_len);

/** @brief Decode a Current Rampdown Rate sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_rampdown_rate - current rampdown rate
 *  @return nsm_completion_codes
 */
int decodeCurrentRampdownRateSample(const uint8_t *data, size_t data_len,
				    uint32_t *current_rampdown_rate);

/** @brief Encode a Current Rampdown Hysteresis sample
 *
 *  @param[in] current_rampdown_hysteresis - current rampdown hysteresis
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentRampdownHysteresisSample(uint32_t current_rampdown_hysteresis,
					  uint8_t *data, size_t *data_len);

/** @brief Decode a Current Rampdown Hysteresis sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_rampdown_hysteresis - current rampdown
 * hysteresis
 *  @return nsm_completion_codes
 */
int decodeCurrentRampdownHysteresisSample(
    const uint8_t *data, size_t data_len,
    uint32_t *current_rampdown_hysteresis);

/** @brief Encode a Current Secondary Floor sample

 *  @param[in] current_secondary_floor - current secondary floor
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentSecondaryFloorSample(uint32_t current_secondary_floor,
				      uint8_t *data, size_t *data_len);

/** @brief Decode a Current Secondary Floor sample
 *
 *  @param[in] current_secondary_floor - current secondary floor
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int decodeCurrentSecondaryFloorSample(const uint8_t *data, size_t data_len,
				      uint32_t *current_secondary_floor);

/** @brief Encode a Current Primary Floor Activation Window Multiplier
 * sample
 *
 *  @param[in] current_primary_floor_activation_window_multiplier -
 * current primary floor activation window multiplier
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
    uint8_t current_primary_floor_activation_window_multiplier, uint8_t *data,
    size_t *data_len);

/** @brief Decode a Current Primary Floor Activation Window Multiplier
 * sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_primary_floor_activation_window_multiplier -
 * current primary floor activation window multiplier
 *  @return nsm_completion_codes
 */
int decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
    const uint8_t *data, size_t data_len,
    uint8_t *current_primary_floor_activation_window_multiplier);

/** @brief Encode a Current Primary Floor Target Window sample
 *
 *  @param[in] current_primary_floor_target_window - current primary
 * floor target window
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentPrimaryFloorTargetWindowSample(
    uint8_t current_primary_floor_target_window, uint8_t *data,
    size_t *data_len);

/** @brief Decode a Current Primary Floor Target Window sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_primary_floor_target_window - current primary
 * floor target window
 *  @return nsm_completion_codes
 */
int decodeCurrentPrimaryFloorTargetWindowSample(
    const uint8_t *data, size_t data_len,
    uint8_t *current_primary_floor_target_window);

/** @brief Encode a Current Primary Floor Activation Offset sample
 *
 *  @param[in] current_primary_floor_activation_offset - current primary
 * floor activation offset
 *  @param[out] data - pointer to telemetry sample data
 *  @param[out] data_len - number of bytes in telemetry sample data
 *  @return nsm_completion_codes
 */
int encodeCurrentPrimaryFloorActivationOffsetSample(
    uint32_t current_primary_floor_activation_offset, uint8_t *data,
    size_t *data_len);

/** @brief Decode a Current Primary Floor Activation Offset sample
 *
 *  @param[in] data - pointer to telemetry sample data
 *  @param[in] data_len - number of bytes in telemetry sample data
 *  @param[out] current_primary_floor_activation_offset - current
 * primary floor activation offset
 *  @return nsm_completion_codes
 */
int decodeCurrentPrimaryFloorActivationOffsetSample(
    const uint8_t *data, size_t data_len,
    uint32_t *current_primary_floor_activation_offset);

/** @brief Encode a Get Current Profile Info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] telemetry_count - telemetry count
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_current_profile_info_v2_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t telemetry_count,
					    struct nsm_msg *msg);

/** @brief Decode a Get Current Profile Info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] telemetry_count - telemetry count
 *  @return nsm_completion_codes
 */
int decode_get_current_profile_info_v2_resp(const struct nsm_msg *msg,
					    size_t msg_len,
					    size_t *consumed_len, uint8_t *cc,
					    uint16_t *telemetry_count);

/** @brief Encode a Get Preset Profile Info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_preset_profile_info_v2_req(uint8_t instance_id,
					  struct nsm_msg *msg);

/** @brief Decode a Get Preset Profile Info request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_preset_profile_info_v2_req(const struct nsm_msg *msg,
					  size_t msg_len);

/** @brief Encode a Get Preset Profile Info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] telemetry_count - telemetry count
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_preset_profile_info_v2_resp(uint8_t instance_id, uint8_t cc,
					   uint16_t telemetry_count,
					   struct nsm_msg *msg);

/** @brief Decode a Get Preset Profile Info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] telemetry_count - telemetry count
 *  @return nsm_completion_codes
 */
int decode_get_preset_profile_info_v2_resp(const struct nsm_msg *msg,
					   size_t msg_len, size_t *consumed_len,
					   uint8_t *cc,
					   uint16_t *telemetry_count);

/** @brief Encode a Get Admin Override Profile Info request message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_admin_override_profile_info_v2_req(uint8_t instance_id,
						  struct nsm_msg *msg);

/** @brief Decode a Get Admin Override Profile Info request message
 *
 *  @param[in] msg    - request message
 *  @param[in] msg_len - Length of request message
 *  @return nsm_completion_codes
 */
int decode_get_admin_override_profile_info_v2_req(const struct nsm_msg *msg,
						  size_t msg_len);

/** @brief Encode a Get Admin Override Profile Info response message
 *
 *  @param[in] instance_id - NSM instance ID
 *  @param[in] cc - pointer to response message completion code
 *  @param[in] telemetry_count - telemetry count
 *  @param[out] msg - Message will be written to this
 *  @return nsm_completion_codes
 */
int encode_get_admin_override_profile_info_v2_resp(uint8_t instance_id,
						   uint8_t cc,
						   uint16_t telemetry_count,
						   struct nsm_msg *msg);

/** @brief Decode a Get Admin Override Profile Info response message
 *
 *  @param[in] msg    - response message
 *  @param[in] msg_len - Length of response message
 *  @param[out] cc - pointer to response message completion code
 *  @param[out] telemetry_count - telemetry count
 *  @return nsm_completion_codes
 */
int decode_get_admin_override_profile_info_v2_resp(const struct nsm_msg *msg,
						   size_t msg_len,
						   size_t *consumed_len,
						   uint8_t *cc,
						   uint16_t *telemetry_count);

#ifdef __cplusplus
}
#endif
#endif