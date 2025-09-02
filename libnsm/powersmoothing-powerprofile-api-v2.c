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

#include "powersmoothing-powerprofile-api-v2.h"
#include "base.h"
#include "platform-environmental.h"

#include <endian.h>
#include <stdio.h>
#include <string.h>

int encode_get_powersmoothing_featinfo_v2_req(uint8_t instance_id,
    struct nsm_msg *msg)
{
return encode_get_platform_env_command_no_payload_req(
instance_id, msg, NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2);
}

int decode_get_powersmoothing_featinfo_v2_req(const struct nsm_msg *msg,
    size_t msg_len)
{
return decode_get_platform_env_command_no_payload_req(
msg, msg_len, NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2);
}

int encode4ByteOfUint32Sample(uint32_t sample, uint8_t *data, size_t *data_len)
{
    if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	uint32_t le_reading = htole32(sample);

	memcpy(data, &le_reading, sizeof(uint32_t));
	*data_len = sizeof(uint32_t);

	return NSM_SW_SUCCESS;
}

int decode4ByteOfUint32Sample(const uint8_t *data, size_t data_len,
			      uint32_t *sample)
{
    if (data == NULL || sample == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint32_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint32_t le_reading;
	memcpy(&le_reading, data, sizeof(uint32_t));

	*sample = le32toh(le_reading);

	return NSM_SW_SUCCESS;
}

int encode2ByteOfUint16Sample(uint16_t sample, uint8_t *data, size_t *data_len)
{
    if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}
    
	uint16_t le_reading = htole16(sample);

	memcpy(data, &le_reading, sizeof(uint16_t));
	*data_len = sizeof(uint16_t);

	return NSM_SW_SUCCESS;
}

int decode2ByteOfUint16Sample(const uint8_t *data, size_t data_len,
			      uint16_t *sample)
{
    if (data == NULL || sample == NULL) {
		return NSM_SW_ERROR_NULL;
	}
    
	if (data_len != sizeof(uint16_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	uint16_t le_reading;
	memcpy(&le_reading, data, sizeof(uint16_t));

	*sample = le16toh(le_reading);

	return NSM_SW_SUCCESS;
}

int encodeFeatureFlagSample(uint32_t feature_flag, uint8_t *data,
    size_t *data_len)
{
	if (encode4ByteOfUint32Sample(feature_flag, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeFeatureFlagSample(const uint8_t *data, size_t data_len,
    uint32_t *feature_flag)
{
	if (decode4ByteOfUint32Sample(data, data_len, feature_flag) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentTMPSettingSample(uint32_t current_tmp_setting, uint8_t *data,
    size_t *data_len)
{
	if (encode4ByteOfUint32Sample(current_tmp_setting, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentTMPSettingSample(const uint8_t *data, size_t data_len,
    uint32_t *current_tmp_setting)
{
	if (decode4ByteOfUint32Sample(data, data_len, current_tmp_setting) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentTMPFloorSettingSample(uint32_t current_tmp_floor_setting, uint8_t *data,
    size_t *data_len)
{
	if (encode4ByteOfUint32Sample(current_tmp_floor_setting, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentTMPFloorSettingSample(const uint8_t *data, size_t data_len,
    uint32_t *current_tmp_floor_setting)
{
	if (decode4ByteOfUint32Sample(
		data, data_len, current_tmp_floor_setting) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeMaxTmpFloorSettingSample(uint16_t max_tmp_floor_setting, uint8_t *data,
    size_t *data_len)
{
	if (encode2ByteOfUint16Sample(max_tmp_floor_setting, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeMaxTmpFloorSettingSample(const uint8_t *data, size_t data_len,
    uint16_t *max_tmp_floor_setting)
{
	if (decode2ByteOfUint16Sample(data, data_len, max_tmp_floor_setting) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeMinTmpFloorSettingSample(uint16_t min_tmp_floor_setting, uint8_t *data,
    size_t *data_len)
{
	if (encode2ByteOfUint16Sample(min_tmp_floor_setting, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeMinTmpFloorSettingSample(const uint8_t *data, size_t data_len,
    uint16_t *min_tmp_floor_setting)
{
	if (decode2ByteOfUint16Sample(data, data_len, min_tmp_floor_setting) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeFloorWindowMultiplierSample(uint32_t floor_window_multiplier,
				      uint8_t *data, size_t *data_len)
{
	if (encode4ByteOfUint32Sample(floor_window_multiplier, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeFloorWindowMultiplierSample(const uint8_t *data, size_t data_len,
				      uint32_t *floor_window_multiplier)
{
	if (decode4ByteOfUint32Sample(
		data, data_len, floor_window_multiplier) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeMinPrimaryFloorActivationOffset(
    uint32_t min_primary_floor_activation_offset, uint8_t *data,
    size_t *data_len)
{
	if (encode4ByteOfUint32Sample(min_primary_floor_activation_offset, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeMinPrimaryFloorActivationOffset(
    const uint8_t *data, size_t data_len,
    uint32_t *min_primary_floor_activation_offset)
{
	if (decode4ByteOfUint32Sample(data, data_len,
				      min_primary_floor_activation_offset) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeMinPrimaryFloorActivationPoint(
    uint32_t min_primary_floor_activation_point, uint8_t *data,
    size_t *data_len)
{
	if (encode4ByteOfUint32Sample(min_primary_floor_activation_point, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeMinPrimaryFloorActivationPoint(
    const uint8_t *data, size_t data_len,
    uint32_t *min_primary_floor_activation_point)
{
	if (decode4ByteOfUint32Sample(data, data_len,
				      min_primary_floor_activation_point) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encode_get_powersmoothing_featinfo_v2_resp(uint8_t instance_id, uint8_t cc,
    uint16_t telemetry_count, struct nsm_msg *msg)
{
    return encode_aggregate_resp(instance_id, NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2, cc, telemetry_count, msg);
}

int decode_get_powersmoothing_featinfo_v2_resp(const struct nsm_msg *msg, size_t msg_len,
    size_t *consumed_len, uint8_t *cc,
    uint16_t *telemetry_count)
{
    return decode_aggregate_resp(msg, msg_len, consumed_len, cc, telemetry_count);
}

int encode_get_current_profile_info_v2_req(uint8_t instance_id,
					   struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg,
	    NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2);
}

int decode_get_current_profile_info_v2_req(const struct nsm_msg *msg,
					   size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2);
}

int encodeActivePresetProfileSample(uint8_t active_preset_profile_id,
				    uint8_t *data, size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*data_len = sizeof(uint8_t);
	memcpy(data, &active_preset_profile_id, sizeof(uint8_t));
	return NSM_SW_SUCCESS;
}

int decodeActivePresetProfileSample(const uint8_t *data, size_t data_len,
				    uint8_t *active_preset_profile_id)
{
	if (data == NULL || active_preset_profile_id == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (data_len != sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}
	*active_preset_profile_id = data[0];
	return NSM_SW_SUCCESS;
}

int encodeAdminOverrideMaskSample(uint16_t admin_override_id, uint8_t *data,
				  size_t *data_len)
{
	if (encode2ByteOfUint16Sample(admin_override_id, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeAdminOverrideMaskSample(const uint8_t *data, size_t data_len,
				  uint16_t *admin_override_id)
{
	if (decode2ByteOfUint16Sample(data, data_len, admin_override_id) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentTMPFloorSample(uint16_t current_tmp_floor_setting,
				uint8_t *data, size_t *data_len)
{
	if (encode2ByteOfUint16Sample(current_tmp_floor_setting, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentTMPFloorSample(const uint8_t *data, size_t data_len,
				uint16_t *current_tmp_floor_setting)
{
	if (decode2ByteOfUint16Sample(
		data, data_len, current_tmp_floor_setting) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentRampupRateSample(uint32_t current_rampup_rate, uint8_t *data,
				  size_t *data_len)
{
	if (encode4ByteOfUint32Sample(current_rampup_rate, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentRampupRateSample(const uint8_t *data, size_t data_len,
				  uint32_t *current_rampup_rate)
{
	if (decode4ByteOfUint32Sample(data, data_len, current_rampup_rate) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentRampdownRateSample(uint32_t current_rampdown_rate,
				    uint8_t *data, size_t *data_len)
{
	if (encode4ByteOfUint32Sample(current_rampdown_rate, data, data_len) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentRampdownRateSample(const uint8_t *data, size_t data_len,
				    uint32_t *current_rampdown_rate)
{
	if (decode4ByteOfUint32Sample(data, data_len, current_rampdown_rate) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentRampdownHysteresisSample(uint32_t current_rampdown_hysteresis,
					  uint8_t *data, size_t *data_len)
{
	if (encode4ByteOfUint32Sample(current_rampdown_hysteresis, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentRampdownHysteresisSample(const uint8_t *data, size_t data_len,
					  uint32_t *current_rampdown_hysteresis)
{
	if (decode4ByteOfUint32Sample(data, data_len,
				      current_rampdown_hysteresis) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentSecondaryFloorSample(uint32_t current_secondary_floor,
				      uint8_t *data, size_t *data_len)

{
	if (encode4ByteOfUint32Sample(current_secondary_floor, data,
				      data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentSecondaryFloorSample(const uint8_t *data, size_t data_len,
				      uint32_t *current_secondary_floor)
{
	if (decode4ByteOfUint32Sample(
		data, data_len, current_secondary_floor) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encodeCurrentPrimaryFloorActivationWindowMultiplierSample(
    uint8_t current_primary_floor_activation_window_multiplier, uint8_t *data,
    size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*data_len = sizeof(uint8_t);
	memcpy(data, &current_primary_floor_activation_window_multiplier,
	       sizeof(uint8_t));
	return NSM_SW_SUCCESS;
}

int decodeCurrentPrimaryFloorActivationWindowMultiplierSample(
    const uint8_t *data, size_t data_len,
    uint8_t *current_primary_floor_activation_window_multiplier)
{
	if (data == NULL ||
	    current_primary_floor_activation_window_multiplier == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	if (data_len != sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*current_primary_floor_activation_window_multiplier = data[0];
	return NSM_SW_SUCCESS;
}

int encodeCurrentPrimaryFloorTargetWindowSample(
    uint8_t current_primary_floor_target_window, uint8_t *data,
    size_t *data_len)
{
	if (data == NULL || data_len == NULL) {
		return NSM_SW_ERROR_NULL;
	}

	*data_len = sizeof(uint8_t);
	memcpy(data, &current_primary_floor_target_window, sizeof(uint8_t));
	return NSM_SW_SUCCESS;
}

int decodeCurrentPrimaryFloorTargetWindowSample(
    const uint8_t *data, size_t data_len,
    uint8_t *current_primary_floor_target_window)
{
	if (data == NULL || current_primary_floor_target_window == NULL) {
		return NSM_SW_ERROR_NULL;
	}
	if (data_len != sizeof(uint8_t)) {
		return NSM_SW_ERROR_LENGTH;
	}

	*current_primary_floor_target_window = data[0];
	return NSM_SW_SUCCESS;
}

int encodeCurrentPrimaryFloorActivationOffsetSample(
    uint32_t current_primary_floor_activation_offset, uint8_t *data,
    size_t *data_len)
{
	if (encode4ByteOfUint32Sample(current_primary_floor_activation_offset,
				      data, data_len) != NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int decodeCurrentPrimaryFloorActivationOffsetSample(
    const uint8_t *data, size_t data_len,
    uint32_t *current_primary_floor_activation_offset)
{
	if (decode4ByteOfUint32Sample(
		data, data_len, current_primary_floor_activation_offset) !=
	    NSM_SW_SUCCESS) {
		return NSM_SW_ERROR_NULL;
	}
	return NSM_SW_SUCCESS;
}

int encode_get_current_profile_info_v2_resp(uint8_t instance_id, uint8_t cc,
					    uint16_t telemetry_count,
					    struct nsm_msg *msg)
{
	return encode_aggregate_resp(
	    instance_id, NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2,
	    cc, telemetry_count, msg);
}

int decode_get_current_profile_info_v2_resp(const struct nsm_msg *msg,
					    size_t msg_len,
					    size_t *consumed_len, uint8_t *cc,
					    uint16_t *telemetry_count)
{
	return decode_aggregate_resp(msg, msg_len, consumed_len, cc,
				     telemetry_count);
}

int encode_get_preset_profile_info_v2_req(uint8_t instance_id,
					  struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg,
	    NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2);
}

int decode_get_preset_profile_info_v2_req(const struct nsm_msg *msg,
					  size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2);
}

int encode_get_preset_profile_info_v2_resp(uint8_t instance_id, uint8_t cc,
					   uint16_t telemetry_count,
					   struct nsm_msg *msg)
{
	return encode_aggregate_resp(
	    instance_id, NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2,
	    cc, telemetry_count, msg);
}

int decode_get_preset_profile_info_v2_resp(const struct nsm_msg *msg,
					   size_t msg_len, size_t *consumed_len,
					   uint8_t *cc,
					   uint16_t *telemetry_count)
{
	return decode_aggregate_resp(msg, msg_len, consumed_len, cc,
				     telemetry_count);
}

int encode_get_admin_override_profile_info_v2_req(uint8_t instance_id,
						  struct nsm_msg *msg)
{
	return encode_get_platform_env_command_no_payload_req(
	    instance_id, msg, NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2);
}

int decode_get_admin_override_profile_info_v2_req(const struct nsm_msg *msg,
						  size_t msg_len)
{
	return decode_get_platform_env_command_no_payload_req(
	    msg, msg_len, NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2);
}

int encode_get_admin_override_profile_info_v2_resp(uint8_t instance_id,
						   uint8_t cc,
						   uint16_t telemetry_count,
						   struct nsm_msg *msg)
{
	return encode_aggregate_resp(instance_id,
				     NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2,
				     cc, telemetry_count, msg);
}

int decode_get_admin_override_profile_info_v2_resp(const struct nsm_msg *msg,
						   size_t msg_len,
						   size_t *consumed_len,
						   uint8_t *cc,
						   uint16_t *telemetry_count)
{
	return decode_aggregate_resp(msg, msg_len, consumed_len, cc,
				     telemetry_count);
}