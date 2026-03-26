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
 * Branch coverage batch 4 for device-configuration.c
 *
 * Targets: remaining half-covered branches for encode/decode functions
 * including v2 device mode settings, reconfiguration permissions,
 * FPGA diagnostics response, error injection payload roundtrips,
 * and cc=NSM_ERROR decode paths.
 */

#include "base.h"
#include "device-configuration.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// Helper: build an error response buffer with cc=NSM_ERROR
static std::vector<uint8_t> makeErrorResp()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc = non-success
	return buf;
}

// 11-byte buf with data_size=1 for decode resp data_size != 0 checks
static std::vector<uint8_t> makeNonZeroDataSizeBuf()
{
	std::vector<uint8_t> buf(11, 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // nsm_common_resp.data_size = 1
	return buf;
}

// ===========================================================================
// Get Reconfiguration Permissions V1: NULL checks and roundtrips
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetReconfigPermV1Req_NullMsg)
{
	auto rc = encode_get_reconfiguration_permissions_v1_req(
	    0, RP_IN_SYSTEM_TEST, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetReconfigPermV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_reconfiguration_permissions_v1_req(
	    kBadIid, RP_IN_SYSTEM_TEST, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeGetReconfigPermV1Req_NullMsg)
{
	enum reconfiguration_permissions_v1_index idx = RP_IN_SYSTEM_TEST;
	auto rc =
	    decode_get_reconfiguration_permissions_v1_req(nullptr, 100, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetReconfigPermV1Req_NullIdx)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_reconfiguration_permissions_v1_req(msg, buf.size(),
								nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetReconfigPermV1Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx = RP_IN_SYSTEM_TEST;
	auto rc = decode_get_reconfiguration_permissions_v1_req(msg, buf.size(),
								&idx);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, EncodeGetReconfigPermV1Resp_NullMsg)
{
	struct nsm_reconfiguration_permissions_v1 data = {};
	auto rc = encode_get_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, 0, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetReconfigPermV1Resp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetReconfigPermV1Resp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	struct nsm_reconfiguration_permissions_v1 data = {};
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), nullptr, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetReconfigPermV1Resp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetReconfigPermV1Resp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_reconfiguration_permissions_v1 data = {};
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Set Reconfiguration Permissions V1: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeSetReconfigPermV1Req_NullMsg)
{
	auto rc = encode_set_reconfiguration_permissions_v1_req(
	    0, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 1, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeSetReconfigPermV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_reconfiguration_permissions_v1_req(
	    kBadIid, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 1, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeSetReconfigPermV1Req_NullMsg)
{
	enum reconfiguration_permissions_v1_index idx = RP_IN_SYSTEM_TEST;
	enum reconfiguration_permissions_v1_setting cfg = RP_ONESHOOT_HOT_RESET;
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    nullptr, 100, &idx, &cfg, &perm);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetReconfigPermV1Req_NullIdx)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_setting cfg = RP_ONESHOOT_HOT_RESET;
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, buf.size(), nullptr, &cfg, &perm);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetReconfigPermV1Req_NullCfg)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx = RP_IN_SYSTEM_TEST;
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, buf.size(), &idx, nullptr, &perm);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetReconfigPermV1Req_NullPerm)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx = RP_IN_SYSTEM_TEST;
	enum reconfiguration_permissions_v1_setting cfg = RP_ONESHOOT_HOT_RESET;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, buf.size(), &idx, &cfg, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetReconfigPermV1Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx = RP_IN_SYSTEM_TEST;
	enum reconfiguration_permissions_v1_setting cfg = RP_ONESHOOT_HOT_RESET;
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, buf.size(), &idx, &cfg, &perm);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeSetReconfigPermV1Resp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Device Mode Setting (v1): NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetDeviceModeSettingReq_NullMsg)
{
	auto rc = encode_get_device_mode_setting_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetDeviceModeSettingReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_setting_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingReq_NullMsg)
{
	uint8_t idx = 0;
	auto rc = decode_get_device_mode_setting_req(nullptr, 100, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingReq_NullIdx)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_device_mode_setting_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	auto rc = decode_get_device_mode_setting_req(msg, buf.size(), &idx);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, EncodeGetDeviceModeSettingsResp_NullMsg)
{
	auto rc =
	    encode_get_device_mode_settings_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	enum nsm_l1_prediction_mode_config mode =
	    static_cast<enum nsm_l1_prediction_mode_config>(0);
	auto rc = decode_get_device_mode_setting_resp(msg, buf.size(), nullptr,
						      &reason, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingResp_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_device_mode_setting_resp(msg, buf.size(), &cc,
						      &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingResp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_l1_prediction_mode_config mode =
	    static_cast<enum nsm_l1_prediction_mode_config>(0);
	auto rc = decode_get_device_mode_setting_resp(msg, buf.size(), &cc,
						      &reason, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Get Device Mode Settings V2: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetDeviceModeSettingsV2Req_NullMsg)
{
	auto rc = encode_get_device_mode_settings_v2_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetDeviceModeSettingsV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_settings_v2_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Req_NullMsg)
{
	uint32_t idx = 0;
	auto rc = decode_get_device_mode_settings_v2_req(nullptr, 100, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Req_NullIdx)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_device_mode_settings_v2_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	auto rc = decode_get_device_mode_settings_v2_req(msg, buf.size(), &idx);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, EncodeGetDeviceModeSettingsV2Resp_NullMsg)
{
	uint8_t curData[4] = {};
	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, 0, curData, 4, nullptr, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetDeviceModeSettingsV2Resp_NullCurData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, 0, nullptr, 4, nullptr, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Resp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	uint8_t curData[128] = {}, pendData[128] = {};
	uint16_t curLen = 0, pendLen = 0;
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), nullptr, &reason, curData, &curLen, pendData,
	    &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Resp_NullPendLen)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t curData[128] = {}, pendData[128] = {};
	uint16_t curLen = 0;
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), &cc, &reason, curData, &curLen, pendData, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Resp_NullCurLen)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t curData[128] = {}, pendData[128] = {};
	uint16_t pendLen = 0;
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), &cc, &reason, curData, nullptr, pendData,
	    &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetDeviceModeSettingsV2Resp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t curData[128] = {}, pendData[128] = {};
	uint16_t curLen = 0, pendLen = 0;
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), &cc, &reason, curData, &curLen, pendData,
	    &pendLen);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Set Device Mode Settings V2: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeSetDeviceModeSettingsV2Req_NullMsg)
{
	uint8_t data[4] = {};
	auto rc =
	    encode_set_device_mode_settings_v2_req(0, 0, data, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeSetDeviceModeSettingsV2Req_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_device_mode_settings_v2_req(0, 0, nullptr, 4, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeSetDeviceModeSettingsV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[4] = {};
	auto rc =
	    encode_set_device_mode_settings_v2_req(kBadIid, 0, data, 4, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeSetDeviceModeSettingsV2Req_NullMsg)
{
	uint32_t idx = 0;
	uint8_t data[128] = {};
	uint16_t len = 0;
	auto rc = decode_set_device_mode_settings_v2_req(nullptr, 100, &idx,
							 data, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetDeviceModeSettingsV2Req_NullIdx)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t data[128] = {};
	uint16_t len = 0;
	auto rc = decode_set_device_mode_settings_v2_req(msg, buf.size(),
							 nullptr, data, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetDeviceModeSettingsV2Req_Roundtrip)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t dataIn[4] = {0xAA, 0xBB, 0xCC, 0xDD};
	auto rc = encode_set_device_mode_settings_v2_req(0, 42, dataIn, 4, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	uint32_t idx = 0;
	uint8_t dataOut[128] = {};
	uint16_t len = 0;
	rc = decode_set_device_mode_settings_v2_req(
	    msg,
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_settings_v2_req) -
		1 + 4,
	    &idx, dataOut, &len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(idx, 42u);
	EXPECT_EQ(len, 4);
}

TEST(DevConfigBranch4, DecodeSetDeviceModeSettingsV2Req_NullLen)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	uint8_t data[128] = {};
	auto rc = decode_set_device_mode_settings_v2_req(msg, buf.size(), &idx,
							 data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetDeviceModeSettingsV2Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	uint8_t data[128] = {};
	uint16_t len = 0;
	auto rc = decode_set_device_mode_settings_v2_req(msg, buf.size(), &idx,
							 data, &len);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, EncodeSetDeviceModeSettingsV2Resp_NullMsg)
{
	auto rc =
	    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetDeviceModeSettingsV2Resp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_device_mode_settings_v2_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Enable/Disable GPU IST Mode: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeEnableDisableGpuIstModeReq_NullMsg)
{
	auto rc = encode_enable_disable_gpu_ist_mode_req(0, 0, 1, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeEnableDisableGpuIstModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_enable_disable_gpu_ist_mode_req(kBadIid, 0, 1, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeEnableDisableGpuIstModeReq_NullMsg)
{
	uint8_t idx = 0, val = 0;
	auto rc =
	    decode_enable_disable_gpu_ist_mode_req(nullptr, 100, &idx, &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeEnableDisableGpuIstModeReq_NullIdx)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t val = 0;
	auto rc = decode_enable_disable_gpu_ist_mode_req(msg, buf.size(),
							 nullptr, &val);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeEnableDisableGpuIstModeReq_NullVal)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	auto rc = decode_enable_disable_gpu_ist_mode_req(msg, buf.size(), &idx,
							 nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeEnableDisableGpuIstModeReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0, val = 0;
	auto rc =
	    decode_enable_disable_gpu_ist_mode_req(msg, buf.size(), &idx, &val);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeEnableDisableGpuIstModeResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_enable_disable_gpu_ist_mode_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// FPGA Diagnostics Settings: response encode/decode NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetFpgaDiagSettingsResp_NullMsg)
{
	uint8_t data[4] = {};
	auto rc = encode_get_fpga_diagnostics_settings_resp(0, NSM_SUCCESS, 0,
							    4, data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetFpgaDiagSettingsResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_fpga_diagnostics_settings_resp(0, NSM_SUCCESS, 0,
							    4, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetFpgaDiagSettingsResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t ds = 0, reason = 0;
	uint8_t data[32] = {};
	auto rc = decode_get_fpga_diagnostics_settings_resp(
	    msg, buf.size(), nullptr, &ds, &reason, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetFpgaDiagSettingsResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_fpga_diagnostics_settings_resp(
	    msg, buf.size(), &cc, &ds, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// FPGA Diagnostics WP: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetFpgaDiagWpResp_NullMsg)
{
	struct nsm_fpga_diagnostics_settings_wp data = {};
	auto rc = encode_get_fpga_diagnostics_settings_wp_resp(
	    0, NSM_SUCCESS, 0, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetFpgaDiagWpResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_fpga_diagnostics_settings_wp_resp(0, NSM_SUCCESS,
							       0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetFpgaDiagWpResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	struct nsm_fpga_diagnostics_settings_wp data = {};
	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    msg, buf.size(), nullptr, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetFpgaDiagWpResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// FPGA Diagnostics WP Jumper: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetFpgaDiagWpJumperResp_NullMsg)
{
	struct nsm_fpga_diagnostics_settings_wp_jumper data = {};
	auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    0, NSM_SUCCESS, 0, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetFpgaDiagWpJumperResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    0, NSM_SUCCESS, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetFpgaDiagWpJumperResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	struct nsm_fpga_diagnostics_settings_wp_jumper data = {};
	auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    msg, buf.size(), nullptr, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetFpgaDiagWpJumperResp_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_fpga_diagnostics_settings_wp_jumper_resp(
	    msg, buf.size(), &cc, &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Power Supply / GPU Presence / GPU Power Status resp: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetPowerSupplyStatusResp_NullMsg)
{
	auto rc = encode_get_power_supply_status_resp(0, NSM_SUCCESS, 0, 0xFF,
						      nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetPowerSupplyStatusResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	uint8_t status = 0;
	auto rc = decode_get_power_supply_status_resp(msg, buf.size(), nullptr,
						      &reason, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetPowerSupplyStatusResp_NullStatus)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_power_supply_status_resp(msg, buf.size(), &cc,
						      &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetGpuPresenceResp_NullMsg)
{
	auto rc =
	    encode_get_gpu_presence_resp(0, NSM_SUCCESS, 0, 0xFF, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetGpuPresenceResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	uint8_t presence = 0;
	auto rc = decode_get_gpu_presence_resp(msg, buf.size(), nullptr,
					       &reason, &presence);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetGpuPresenceResp_NullPresence)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_gpu_presence_resp(msg, buf.size(), &cc, &reason,
					       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetGpuPowerStatusResp_NullMsg)
{
	auto rc =
	    encode_get_gpu_power_status_resp(0, NSM_SUCCESS, 0, 0xFF, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetGpuPowerStatusResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	uint8_t status = 0;
	auto rc = decode_get_gpu_power_status_resp(msg, buf.size(), nullptr,
						   &reason, &status);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetGpuPowerStatusResp_NullStatus)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_gpu_power_status_resp(msg, buf.size(), &cc,
						   &reason, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// GPU IST Mode resp: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetGpuIstModeResp_NullMsg)
{
	auto rc =
	    encode_get_gpu_ist_mode_resp(0, NSM_SUCCESS, 0, 0xFF, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetGpuIstModeResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t reason = 0;
	uint8_t mode = 0;
	auto rc = decode_get_gpu_ist_mode_resp(msg, buf.size(), nullptr,
					       &reason, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetGpuIstModeResp_NullMode)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_get_gpu_ist_mode_resp(msg, buf.size(), &cc, &reason,
					       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// Error Injection Payload decode/encode: additional NULL checks
// ===========================================================================
TEST(DevConfigBranch4, EncodeSetErrInjPayloadReq_NullMsg)
{
	uint8_t data[4] = {};
	auto rc =
	    encode_set_error_injection_payload_req(0, data, 4, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeSetErrInjPayloadReq_NullData)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc =
	    encode_set_error_injection_payload_req(0, nullptr, 4, 0, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetErrInjPayloadReq_NullMsg)
{
	uint16_t type = 0, subtype = 0;
	uint8_t data[128] = {};
	size_t dataSize = 0;
	auto rc = decode_set_error_injection_payload_req(
	    nullptr, 100, &type, &subtype, data, &dataSize);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetErrInjPayloadReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t type = 0, subtype = 0;
	uint8_t data[128] = {};
	size_t dataSize = 0;
	auto rc = decode_set_error_injection_payload_req(
	    msg, buf.size(), &type, &subtype, data, &dataSize);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeGetErrInjPayloadReq_NullMsg)
{
	uint16_t type = 0, subtype = 0;
	auto rc = decode_get_error_injection_payload_req(nullptr, 100, &type,
							 &subtype);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeGetErrInjPayloadReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t type = 0, subtype = 0;
	auto rc = decode_get_error_injection_payload_req(msg, buf.size(), &type,
							 &subtype);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Activate Error Injection Payload: decode req NULL checks
// ===========================================================================
TEST(DevConfigBranch4, DecodeActivateErrInjPayloadReq_NullMsg)
{
	uint16_t type = 0, subtype = 0;
	auto rc = decode_activate_error_injection_payload_req(nullptr, 100,
							      &type, &subtype);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeActivateErrInjPayloadReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t type = 0, subtype = 0;
	auto rc = decode_activate_error_injection_payload_req(msg, buf.size(),
							      &type, &subtype);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Confidential Compute Mode V1 response: cc=NSM_ERROR path
// ===========================================================================
TEST(DevConfigBranch4, DecodeGetCCModeV1Resp_CcError)
{
	auto buf = makeErrorResp();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, cur = 0, pend = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    msg, buf.size(), &cc, &ds, &reason, &cur, &pend);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

TEST(DevConfigBranch4, DecodeSetCCModeV1Resp_CcError)
{
	auto buf = makeErrorResp();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_set_confidential_compute_mode_v1_resp(
	    msg, buf.size(), &cc, &ds, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// Set EGM mode resp: cc=NSM_ERROR path
// ===========================================================================
TEST(DevConfigBranch4, DecodeSetEgmModeResp_CcError)
{
	auto buf = makeErrorResp();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_set_EGM_mode_resp(msg, buf.size(), &cc, &ds, &reason);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// Get EGM mode resp: cc=NSM_ERROR path
// ===========================================================================
TEST(DevConfigBranch4, DecodeGetEgmModeResp_CcError)
{
	auto buf = makeErrorResp();
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {0};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   &flags);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// Set Error Injection Mode V1 req decode: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, DecodeSetErrInjModeV1Req_NullMsg)
{
	uint8_t mode = 0;
	auto rc = decode_set_error_injection_mode_v1_req(nullptr, 100, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetErrInjModeV1Req_Roundtrip)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_error_injection_mode_v1_req(0, 1, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	uint8_t mode = 0;
	rc = decode_set_error_injection_mode_v1_req(
	    msg,
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_mode_v1_req),
	    &mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(mode, 1);
}

TEST(DevConfigBranch4, DecodeSetErrInjModeV1Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc =
	    decode_set_error_injection_mode_v1_req(msg, buf.size(), &mode);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Set Current Error Injection Types V1 req decode: NULL checks
// ===========================================================================
TEST(DevConfigBranch4, DecodeSetCurrErrInjTypesV1Req_NullMsg)
{
	struct nsm_error_injection_types_mask data = {};
	auto rc = decode_set_current_error_injection_types_v1_req(nullptr, 100,
								  &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, DecodeSetCurrErrInjTypesV1Req_Roundtrip)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	struct nsm_error_injection_types_mask maskIn = {};
	maskIn.mask[0] = 0x55;
	auto rc =
	    encode_set_current_error_injection_types_v1_req(0, &maskIn, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	struct nsm_error_injection_types_mask maskOut = {};
	rc = decode_set_current_error_injection_types_v1_req(
	    msg,
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_set_error_injection_types_mask_req),
	    &maskOut);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(maskOut.mask[0], 0x55);
}

TEST(DevConfigBranch4, DecodeSetCurrErrInjTypesV1Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_error_injection_types_mask data = {};
	auto rc = decode_set_current_error_injection_types_v1_req(
	    msg, buf.size(), &data);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Encode/Decode Confidential Compute Mode V1 resp: pack fail
// ===========================================================================
TEST(DevConfigBranch4, EncodeGetCCModeV1Resp_NullMsg)
{
	auto rc = encode_get_confidential_compute_mode_v1_resp(
	    0, NSM_SUCCESS, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(DevConfigBranch4, EncodeGetCCModeV1Resp_Success)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_confidential_compute_mode_v1_resp(
	    0, NSM_SUCCESS, 0, PRODUCTION_MODE, DEVTOOLS_MODE, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(DevConfigBranch4, DecodeGetCCModeV1Resp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, cur = 0, pend = 0;
	uint16_t ds = 0, reason = 0;
	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    msg, buf.size(), &cc, &ds, &reason, &cur, &pend);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
