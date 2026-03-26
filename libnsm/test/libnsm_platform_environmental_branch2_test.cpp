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

/**
 * Branch coverage for platform-environmental.c — batch 2.
 *
 * Targets the `if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS)` TRUE branch
 * (the *cc != NSM_SUCCESS side) in 40+ decode_xxx_resp functions, plus
 * secondary null checks and data_size mismatch branches not covered by
 * the existing test files.
 *
 * Pattern:
 *   errCcBuf9()  — 9-byte buf, payload[1]=0xFF  → decode_reason_code_and_cc
 *                  returns NSM_SW_SUCCESS with *cc=0xFF (non-success path)
 *   errCcBuf32() — 11-byte buf (for functions that size-check before decode)
 */

#include "base.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"
#include <gtest/gtest.h>
#include <string.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Minimum buffer for decode_reason_code_and_cc to return NSM_SW_SUCCESS
// with *cc = 0xFF (non-success completion code)
static std::vector<uint8_t>
errCcBuf9(size_t sz = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp))
{
	std::vector<uint8_t> buf(sz, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code = non-success
	return buf;
}

// Large buffer — for functions that check msg_len >=
// nsm_msg_hdr+nsm_common_resp before calling decode_reason_code_and_cc. Use 32
// bytes to cover any alignment/padding variation in nsm_common_resp.
static std::vector<uint8_t> errCcBuf32()
{
	std::vector<uint8_t> buf(32, 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF;
	return buf;
}

// Minimal buffer of only nsm_msg_hdr bytes (for msg_len < required checks)
static std::vector<uint8_t> minBuf()
{
	return std::vector<uint8_t>(sizeof(nsm_msg_hdr), 0);
}

// ===========================================================================
// decode_read_thermal_parameter_resp — L753
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) TRUE — *cc = 0xFF
// ===========================================================================
TEST(PlatEnvBranch2, ReadThermalParamResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	int32_t threshold = 0;
	auto rc = decode_read_thermal_parameter_resp(msg, buf.size(), &cc,
						     &reason, &threshold);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_max_observed_power_resp — L1025
// ===========================================================================
TEST(PlatEnvBranch2, GetMaxObservedPowerResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t reading = 0;
	auto rc = decode_get_max_observed_power_resp(msg, buf.size(), &cc,
						     &reason, &reading);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_driver_info_resp — L1139
// ===========================================================================
TEST(PlatEnvBranch2, GetDriverInfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum8 driver_state = 0;
	char driver_version[64] = {};
	auto rc = decode_get_driver_info_resp(msg, buf.size(), &cc, &reason,
					      &driver_state, driver_version);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_current_energy_count_resp — L1297
// ===========================================================================
TEST(PlatEnvBranch2, GetCurrentEnergyCountResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint64_t energy = 0;
	auto rc = decode_get_current_energy_count_resp(msg, buf.size(), &cc,
						       &reason, &energy);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_voltage_resp — L1410
// ===========================================================================
TEST(PlatEnvBranch2, GetVoltageResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t voltage = 0;
	auto rc =
	    decode_get_voltage_resp(msg, buf.size(), &cc, &reason, &voltage);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_altitude_pressure_resp — L1498
// ===========================================================================
TEST(PlatEnvBranch2, GetAltitudePressureResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint32_t reading = 0;
	auto rc = decode_get_altitude_pressure_resp(msg, buf.size(), &cc,
						    &reason, &reading);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_MIG_mode_resp — L1834
// ===========================================================================
TEST(PlatEnvBranch2, GetMIGModeResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_MIG_mode_resp(msg, buf.size(), &cc, &data_size,
					   &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_ECC_mode_resp — L2031
// ===========================================================================
TEST(PlatEnvBranch2, GetECCModeResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_ECC_mode_resp(msg, buf.size(), &cc, &data_size,
					   &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_ECC_error_counts_resp — L2254
// ===========================================================================
TEST(PlatEnvBranch2, GetECCErrorCountsResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_ECC_error_counts counts = {};
	auto rc = decode_get_ECC_error_counts_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &counts);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_programmable_EDPp_scaling_factor_resp — L2347
// ===========================================================================
TEST(PlatEnvBranch2, GetEDPpScalingFactorResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_EDPp_scaling_factors sf = {};
	auto rc = decode_get_programmable_EDPp_scaling_factor_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &sf);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_power_limit_resp — L2701
// ===========================================================================
TEST(PlatEnvBranch2, GetPowerLimitResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint32_t persistent = 0, oneshot = 0, enforced = 0;
	auto rc = decode_get_power_limit_resp(msg, buf.size(), &cc, &data_size,
					      &reason, &persistent, &oneshot,
					      &enforced);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_clock_limit_resp — L2849
// ===========================================================================
TEST(PlatEnvBranch2, GetClockLimitResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_clock_limit clk = {};
	auto rc = decode_get_clock_limit_resp(msg, buf.size(), &cc, &data_size,
					      &reason, &clk);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_curr_clock_freq_resp — L2970
// ===========================================================================
TEST(PlatEnvBranch2, GetCurrClockFreqResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint32_t freq = 0;
	auto rc = decode_get_curr_clock_freq_resp(msg, buf.size(), &cc,
						  &data_size, &reason, &freq);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_current_clock_event_reason_code_resp — L3074
// ===========================================================================
TEST(PlatEnvBranch2, GetClockEventReasonCodeResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	bitfield32_t flags = {};
	auto rc = decode_get_current_clock_event_reason_code_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_accum_GPU_util_time_resp — L3167
// ===========================================================================
TEST(PlatEnvBranch2, GetAccumGPUUtilTimeResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint32_t ctx = 0, sm = 0;
	auto rc = decode_get_accum_GPU_util_time_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &ctx, &sm);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_current_utilization_resp — L3258
// ===========================================================================
TEST(PlatEnvBranch2, GetCurrentUtilizationResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_get_current_utilization_data data = {};
	auto rc = decode_get_current_utilization_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_row_remap_state_resp — L3383
// ===========================================================================
TEST(PlatEnvBranch2, GetRowRemapStateResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_row_remap_state_resp(msg, buf.size(), &cc,
						  &data_size, &reason, &flags);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_row_remapping_counts_resp — L3486
// ===========================================================================
TEST(PlatEnvBranch2, GetRowRemappingCountsResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint32_t corr = 0, uncorr = 0;
	auto rc = decode_get_row_remapping_counts_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &corr, &uncorr);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_row_remap_availability_resp — L3584
// ===========================================================================
TEST(PlatEnvBranch2, GetRowRemapAvailabilityResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_row_remap_availability avail = {};
	auto rc = decode_get_row_remap_availability_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &avail);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_memory_capacity_util_resp — L3688
// ===========================================================================
TEST(PlatEnvBranch2, GetMemoryCapacityUtilResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_memory_capacity_utilization data = {};
	auto rc = decode_get_memory_capacity_util_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_clock_output_enable_state_resp — L3838
// ===========================================================================
TEST(PlatEnvBranch2, GetClockOutputEnableStateResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	uint32_t clk_buf = 0;
	auto rc = decode_get_clock_output_enable_state_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &clk_buf);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_powersmoothing_featinfo_resp — L4607
// ===========================================================================
TEST(PlatEnvBranch2, GetPowersmoothingFeatinfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_pwr_smoothing_featureinfo_data data = {};
	auto rc = decode_get_powersmoothing_featinfo_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_hardware_lifetime_cricuitry_resp — L4701
// ===========================================================================
TEST(PlatEnvBranch2, GetHardwareLifetimeCricuitryResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_hardwarecircuitry_data data = {};
	auto rc = decode_get_hardware_lifetime_cricuitry_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_current_profile_info_resp — L4824
// ===========================================================================
TEST(PlatEnvBranch2, GetCurrentProfileInfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_get_current_profile_data data = {};
	auto rc = decode_get_current_profile_info_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_query_admin_override_resp — L4936
// ===========================================================================
TEST(PlatEnvBranch2, QueryAdminOverrideResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct nsm_admin_override_data data = {};
	auto rc = decode_query_admin_override_resp(msg, buf.size(), &cc,
						   &reason, &data_size, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_set_active_preset_profile_resp — L5060
// Note: checks msg_len >= nsm_msg_hdr + nsm_common_resp BEFORE decode_cc_and_rc
// so we need errCcBuf11 (11 bytes)
// ===========================================================================
TEST(PlatEnvBranch2, SetActivePresetProfileResp_CcNonSuccess)
{
	// Functions that pre-check msg_len >= nsm_common_resp size then call
	// decode_reason_code_and_cc: with cc!=NSM_SUCCESS and msg_len > 9,
	// decode_reason_code_and_cc returns NSM_SW_ERROR_LENGTH (rc != 0),
	// which triggers the TRUE branch of `if (rc != NSM_SW_SUCCESS || *cc !=
	// NSM_SUCCESS)`
	auto buf = errCcBuf32();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_active_preset_profile_resp(msg, buf.size(), &cc,
							&reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH); // rc != NSM_SW_SUCCESS branch taken
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_setup_admin_override_resp — L5169
// ===========================================================================
TEST(PlatEnvBranch2, SetupAdminOverrideResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_setup_admin_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_apply_admin_override_resp — L5245
// Note: checks msg_len >= nsm_msg_hdr + nsm_common_resp BEFORE decode_cc_and_rc
// ===========================================================================
TEST(PlatEnvBranch2, ApplyAdminOverrideResp_CcNonSuccess)
{
	// Same pattern as SetActivePresetProfile: pre-check msg_len >= 11,
	// then decode_reason_code_and_cc with cc!=NSM_SUCCESS and msg_len=32
	// returns NSM_SW_ERROR_LENGTH → TRUE branch of `if (rc !=
	// NSM_SW_SUCCESS ...)`
	auto buf = errCcBuf32();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_apply_admin_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH); // rc != NSM_SW_SUCCESS branch taken
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_toggle_immediate_rampdown_resp — L5353
// ===========================================================================
TEST(PlatEnvBranch2, ToggleImmediateRampdownResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_toggle_immediate_rampdown_resp(msg, buf.size(), &cc,
							&reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_toggle_feature_state_resp — L5461
// ===========================================================================
TEST(PlatEnvBranch2, ToggleFeatureStateResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_toggle_feature_state_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_preset_profile_metadata_resp — L5577
// ===========================================================================
TEST(PlatEnvBranch2, GetPresetProfileMetadataResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t num_profiles = 0;
	struct nsm_get_all_preset_profile_meta_data meta = {};
	auto rc = decode_get_preset_profile_metadata_resp(
	    msg, buf.size(), &cc, &reason, &meta, &num_profiles);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_preset_profile_data_from_resp — L5615
// ===========================================================================
TEST(PlatEnvBranch2, GetPresetProfileDataFromResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_preset_profile_data profile = {};
	auto rc = decode_get_preset_profile_data_from_resp(
	    msg, buf.size(), &cc, &reason, 1, 0, &profile);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_update_preset_profile_param_resp — L5748
// ===========================================================================
TEST(PlatEnvBranch2, UpdatePresetProfileParamResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_update_preset_profile_param_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_enable_workload_power_profile_resp — L5926
// ===========================================================================
TEST(PlatEnvBranch2, EnableWorkloadPowerProfileResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_enable_workload_power_profile_resp(msg, buf.size(),
							    &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_disable_workload_power_profile_resp — L6052
// ===========================================================================
TEST(PlatEnvBranch2, DisableWorkloadPowerProfileResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_disable_workload_power_profile_resp(msg, buf.size(),
							     &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_workload_power_profile_status_resp — L6214
// ===========================================================================
TEST(PlatEnvBranch2, GetWorkloadPowerProfileStatusResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason = 0;
	struct workload_power_profile_status status = {};
	auto rc = decode_get_workload_power_profile_status_resp(
	    msg, buf.size(), &cc, &reason, &data_size, &status);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_workload_power_profile_info_metadata_resp — L6388
// ===========================================================================
TEST(PlatEnvBranch2, GetWorkloadPowerProfileInfoMetadataResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t num_profiles = 0;
	struct nsm_all_workload_power_profile_meta_data meta = {};
	auto rc = decode_get_workload_power_profile_info_metadata_resp(
	    msg, buf.size(), &cc, &reason, &meta, &num_profiles);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_workload_power_profile_info_data_resp — L6430
// ===========================================================================
TEST(PlatEnvBranch2, GetWorkloadPowerProfileInfoDataResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_workload_power_profile_data profile = {};
	auto rc = decode_get_workload_power_profile_info_data_resp(
	    msg, buf.size(), &cc, &reason, 1, 0, &profile);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_leak_detection_info_resp — L6599
// ===========================================================================
TEST(PlatEnvBranch2, GetLeakDetectionInfoResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t num_sensors = 0, num_thresh = 0;
	uint8_t sensors_data[256] = {};
	size_t sensors_data_len = 0;
	auto rc = decode_get_leak_detection_info_resp(
	    msg, buf.size(), &cc, &reason, &num_sensors, &num_thresh,
	    sensors_data, &sensors_data_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_set_leak_detection_thresholds_resp — L6830
// ===========================================================================
TEST(PlatEnvBranch2, SetLeakDetectionThresholdsResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_leak_detection_thresholds_resp(msg, buf.size(),
							    &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_inventory_information_resp — L470
// if (rc != NSM_SW_SUCCESS || *cc != NSM_SUCCESS) TRUE
// ===========================================================================
TEST(PlatEnvBranch2, GetInventoryInformationResp_CcNonSuccess)
{
	auto buf = errCcBuf9();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0, data_size = 0;
	uint8_t inv_data[64] = {};
	auto rc = decode_get_inventory_information_resp(
	    msg, buf.size(), &cc, &reason, &data_size, inv_data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// Additional Pattern B / secondary null checks
// ===========================================================================

// decode_read_thermal_parameter_req — L692 (msg_len too small)
TEST(PlatEnvBranch2, ReadThermalParamReq_MsgLenTooSmall)
{
	auto buf = minBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t param_id = 0;
	auto rc = decode_read_thermal_parameter_req(msg, buf.size(), &param_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_read_thermal_parameter_req — secondary null (parameter_id == NULL)
TEST(PlatEnvBranch2, ReadThermalParamReq_NullParameterId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_read_thermal_parameter_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_max_observed_power_req — null sensor_id
TEST(PlatEnvBranch2, GetMaxObservedPowerReq_NullSensorId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t avg = 0;
	auto rc =
	    decode_get_max_observed_power_req(msg, buf.size(), nullptr, &avg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_max_observed_power_req — null averaging_interval
TEST(PlatEnvBranch2, GetMaxObservedPowerReq_NullAvgInterval)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t sensor_id = 0;
	auto rc = decode_get_max_observed_power_req(msg, buf.size(), &sensor_id,
						    nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_voltage_req — L1352 (msg_len too small)
TEST(PlatEnvBranch2, GetVoltageReq_MsgLenTooSmall)
{
	auto buf = minBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t sensor_id = 0;
	auto rc = decode_get_voltage_req(msg, buf.size(), &sensor_id);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// decode_get_clock_limit_req — secondary null (clock_id == NULL)
TEST(PlatEnvBranch2, GetClockLimitReq_NullClockId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_clock_limit_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_curr_clock_freq_req — secondary null (clock_id == NULL)
TEST(PlatEnvBranch2, GetCurrClockFreqReq_NullClockId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_curr_clock_freq_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_clock_output_enable_state_req — secondary null (index == NULL)
TEST(PlatEnvBranch2, GetClockOutputEnableStateReq_NullIndex)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_clock_output_enabled_state_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_clock_output_enable_state_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_power_limit_req — secondary null (id == NULL)
TEST(PlatEnvBranch2, GetPowerLimitReq_NullId)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_power_limit_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_get_workload_power_profile_info_req — null identifier
TEST(PlatEnvBranch2, GetWorkloadPowerProfileInfoReq_NullIdentifier)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_workload_power_profile_info_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_workload_power_profile_info_req(msg, buf.size(),
							     nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_toggle_immediate_rampdown_req — null ramp_down_toggle
TEST(PlatEnvBranch2, ToggleImmediateRampdownReq_NullToggle)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_immediate_rampdown_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_toggle_immediate_rampdown_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_toggle_feature_state_req — null feature_state
TEST(PlatEnvBranch2, ToggleFeatureStateReq_NullFeatureState)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_feature_state_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_toggle_feature_state_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_setup_admin_override_req — null param_value
TEST(PlatEnvBranch2, SetupAdminOverrideReq_NullParamValue)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t param_id = 0;
	auto rc = decode_setup_admin_override_req(msg, buf.size(), &param_id,
						  nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_update_preset_profile_param_req — null param_value
TEST(PlatEnvBranch2, UpdatePresetProfileParamReq_NullParamValue)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_update_preset_profile_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t profile_id = 0, param_id = 0;
	auto rc = decode_update_preset_profile_param_req(
	    msg, buf.size(), &profile_id, &param_id, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_enable_workload_power_profile_req — null profile_mask
TEST(PlatEnvBranch2, EnableWorkloadPowerProfileReq_NullMask)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(bitfield256_t) + 4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_enable_workload_power_profile_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// decode_disable_workload_power_profile_req — null profile_mask
TEST(PlatEnvBranch2, DisableWorkloadPowerProfileReq_NullMask)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(bitfield256_t) + 4, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_disable_workload_power_profile_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}
