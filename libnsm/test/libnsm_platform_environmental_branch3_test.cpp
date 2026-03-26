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
 * Branch coverage for platform-environmental.c — batch 3.
 *
 * Targets:
 *  (A) decode_*_resp "data_size != 0" TRUE branches (L5068, L5182, L5253,
 *      L5366, L5474, L5761, L5939, L6066): zero-filled nsm_msg_hdr +
 *      nsm_common_resp buffer with data_size=1 at payload[4..5].
 *  (B) encode_*_req pack_nsm_header failure TRUE branches (L2391, L2496,
 *      L3877, L4136, L4977, L5088, L5274, L5385, L5662, L5834, L5961,
 *      L6679): instance_id=32 > NSM_INSTANCE_MAX(31).
 *  (C) Special:
 *      L4006 decode_nsm_xid_event "data_size == sizeof(*payload)" branch.
 *      L6505 decode_get_leak_detection_info_req "data_size != 0" branch.
 */

#include "base.h"
#include "platform-environmental.h"
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Buffer for decode_*_resp data_size != 0 tests:
// sizeof(nsm_msg_hdr)+sizeof(nsm_common_resp)=11 bytes; cc=0, data_size=1
static std::vector<uint8_t> dsNonZeroBuf()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // data_size = 1 != 0
	return buf;
}

// ===========================================================================
// GROUP A: decode_*_resp "data_size != 0" TRUE branches
// All expect NSM_SW_ERROR_DATA.
// ===========================================================================

// L5068: decode_set_active_preset_profile_resp
TEST(PlatEnvBranch3, SetActivePresetProfileResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_active_preset_profile_resp(msg, buf.size(), &cc,
							&reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5182: decode_setup_admin_override_resp
TEST(PlatEnvBranch3, SetupAdminOverrideResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_setup_admin_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5253: decode_apply_admin_override_resp
TEST(PlatEnvBranch3, ApplyAdminOverrideResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_apply_admin_override_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5366: decode_toggle_immediate_rampdown_resp
TEST(PlatEnvBranch3, ToggleImmediateRampdownResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_toggle_immediate_rampdown_resp(msg, buf.size(), &cc,
							&reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5474: decode_toggle_feature_state_resp
TEST(PlatEnvBranch3, ToggleFeatureStateResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_toggle_feature_state_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5761: decode_update_preset_profile_param_resp
TEST(PlatEnvBranch3, UpdatePresetProfileParamResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_update_preset_profile_param_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5939: decode_enable_workload_power_profile_resp
TEST(PlatEnvBranch3, EnableWorkloadPowerProfileResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_enable_workload_power_profile_resp(msg, buf.size(),
							    &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L6066: decode_disable_workload_power_profile_resp
TEST(PlatEnvBranch3, DisableWorkloadPowerProfileResp_DataSizeNonZero)
{
	auto buf = dsNonZeroBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_disable_workload_power_profile_resp(msg, buf.size(),
							     &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// GROUP B: encode_*_req pack_nsm_header failure (instance_id=32 > MAX=31)
// All expect NSM_SW_ERROR_DATA.
// ===========================================================================

// L2391: encode_set_programmable_EDPp_scaling_factor_req
TEST(PlatEnvBranch3, SetEDPpScalingFactorReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_set_programmable_EDPp_scaling_factor_req(32, 0, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L2496: encode_set_power_limit_req
TEST(PlatEnvBranch3, SetPowerLimitReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_power_limit_req(32, 0, 0, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L3877: encode_set_clock_limit_req
TEST(PlatEnvBranch3, SetClockLimitReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_clock_limit_req(32, 0, 0, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L4136: encode_query_per_instance_gpm_metrics_req
TEST(PlatEnvBranch3, QueryPerInstanceGpmMetricsReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_query_per_instance_gpm_metrics_req(32, 0, 0, 0, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L4977: encode_set_active_preset_profile_req
TEST(PlatEnvBranch3, SetActivePresetProfileReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_active_preset_profile_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5088: encode_setup_admin_override_req
TEST(PlatEnvBranch3, SetupAdminOverrideReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_setup_admin_override_req(32, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5274: encode_toggle_immediate_rampdown_req
TEST(PlatEnvBranch3, ToggleImmediateRampdownReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_toggle_immediate_rampdown_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5385: encode_toggle_feature_state_req
TEST(PlatEnvBranch3, ToggleFeatureStateReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_toggle_feature_state_req(32, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5662: encode_update_preset_profile_param_req
TEST(PlatEnvBranch3, UpdatePresetProfileParamReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_update_preset_profile_param_req(32, 0, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5834: encode_enable_workload_power_profile_req
TEST(PlatEnvBranch3, EnableWorkloadPowerProfileReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	bitfield256_t mask = {};
	auto rc = encode_enable_workload_power_profile_req(32, &mask, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L5961: encode_disable_workload_power_profile_req
TEST(PlatEnvBranch3, DisableWorkloadPowerProfileReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	bitfield256_t mask = {};
	auto rc = encode_disable_workload_power_profile_req(32, &mask, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L6679: encode_set_leak_detection_thresholds_req
TEST(PlatEnvBranch3, SetLeakDetectionThresholdsReq_PackFail)
{
	std::vector<uint8_t> buf(256, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t dummy = 0;
	auto rc =
	    encode_set_leak_detection_thresholds_req(32, 0, 0, &dummy, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// GROUP C: Special branches
// ===========================================================================

// L4006: decode_nsm_xid_event
// "else if (event->data_size == sizeof(*payload))" TRUE branch
// → *message_text_size = 0
// data_size = sizeof(nsm_xid_event_payload) = 20
// msg_len = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN + 20 = 31
// ===========================================================================
TEST(PlatEnvBranch3, XidEvent_DataSizeEqualsPayload)
{
	// NSM_EVENT_MIN_LEN = 6; nsm_xid_event_payload = 20 bytes
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
				     sizeof(nsm_xid_event_payload),
				 0);
	// event->data_size at payload[5] = sizeof(nsm_xid_event_payload) = 20
	buf[sizeof(nsm_msg_hdr) + 5] =
	    static_cast<uint8_t>(sizeof(nsm_xid_event_payload));
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t ec = 0;
	uint16_t es = 0;
	nsm_xid_event_payload payload = {};
	char text[64] = {};
	size_t text_size = 1; // will be set to 0 by the branch
	auto rc = decode_nsm_xid_event(msg, buf.size(), &ec, &es, &payload,
				       text, &text_size);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(text_size, 0u);
}

// L6505: decode_get_leak_detection_info_req
// if (request->hdr.data_size != 0) TRUE — data_size=1 at payload[1]
// ===========================================================================
TEST(PlatEnvBranch3, GetLeakDetectionInfoReq_DataSizeNonZero)
{
	// sizeof(nsm_get_leak_detection_info_req) = sizeof(nsm_common_req) = 2
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_leak_detection_info_req), 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 1; // hdr.data_size = 1 != 0 → L6505 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_leak_detection_info_req(msg, buf.size());
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
