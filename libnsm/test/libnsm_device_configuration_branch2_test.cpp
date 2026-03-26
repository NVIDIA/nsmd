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
 * Branch coverage batch 2 for device-configuration.c
 *
 * Targets (uncovered branches not in existing branch test):
 *  L1174  encode_get_fpga_diagnostics_settings_req: pack fail
 *  L1257  decode_get_fpga_diagnostics_settings_resp: NULL args
 *  L1262  decode_get_fpga_diagnostics_settings_resp: cc != NSM_SUCCESS
 *  L1266  decode_get_fpga_diagnostics_settings_resp: msg_len too short
 *  L1424  encode_enable_disable_gpu_ist_mode_req: NULL msg / pack fail
 *  L1454  decode_enable_disable_gpu_ist_mode_req: NULL args / msg too short
 *  L1466  decode_enable_disable_gpu_ist_mode_req: data_size too small
 *  L1492  decode_enable_disable_gpu_ist_mode_resp: data_size != 0
 *  L1513  encode_get_reconfiguration_permissions_v1_req: pack fail
 *  L1531  decode_get_reconfiguration_permissions_v1_req: NULL args / short
 *  L1544  decode_get_reconfiguration_permissions_v1_req: data_size too small
 *  L1558  encode_get_reconfiguration_permissions_v1_resp: NULL msg
 *  L1593  decode_get_reconfiguration_permissions_v1_resp: NULL args
 *  L1597  decode_get_reconfiguration_permissions_v1_resp: cc != NSM_SUCCESS
 *  L1601  decode_get_reconfiguration_permissions_v1_resp: msg too short
 *  L1630  encode_set_reconfiguration_permissions_v1_req: pack fail
 *  L1653  decode_set_reconfiguration_permissions_v1_req: NULL args / short
 *  L1667  decode_set_reconfiguration_permissions_v1_req: data_size too small
 *  L1696  decode_set_reconfiguration_permissions_v1_resp: data_size != 0
 *  L1719  encode_get_confidential_compute_mode_v1_resp: NULL msg
 *  L1755  decode_get_confidential_compute_mode_v1_resp: NULL args
 *  L1760  decode_get_confidential_compute_mode_v1_resp: cc != NSM_SUCCESS
 *  L1764  decode_get_confidential_compute_mode_v1_resp: msg_len mismatch
 *  L2060  encode_get_device_mode_setting_req: NULL msg / pack fail
 *  L2088  decode_get_device_mode_setting_req: NULL args / short / data_size
 *  L2115  encode_get_device_mode_settings_resp: NULL msg / pack fail
 *  L2152  decode_get_device_mode_setting_resp: NULL args / cc / short
 *  L2179  encode_get_device_mode_settings_v2_req: NULL msg / pack fail
 *  L2207  decode_get_device_mode_settings_v2_req: NULL / short / data_size
 *  L2235  encode_get_device_mode_settings_v2_resp: NULL msg / null data
 *  L2293  decode_get_device_mode_settings_v2_resp: NULL / cc / short
 *  L2342  encode_set_device_mode_settings_v2_req: NULL msg / null data / pack
 *  L2381  decode_set_device_mode_settings_v2_req: NULL / short / data_size
 *  L2426  decode_set_device_mode_settings_v2_resp: data_size != 0
 */

#include "base.h"
#include "device-configuration.h"
#include <gtest/gtest.h>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// 9-byte buf with cc=0xFF: for decode_reason_code_and_cc cc!=NSM_SUCCESS.
// base.c decode_reason_code_and_cc requires msg_len==5+4=9 exactly.
static std::vector<uint8_t> makeErrCcBuf()
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(struct nsm_common_non_success_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // cc = non-success
	return buf;
}

// 11-byte buf with data_size=1 to trigger decode resp data_size != 0 checks.
// cc=0 (NSM_SUCCESS); payload[4]=1 → data_size=1 != 0.
static std::vector<uint8_t> makeNonZeroDataSizeBuf()
{
	std::vector<uint8_t> buf(11, 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // nsm_common_resp.data_size = 1
	return buf;
}

// ===========================================================================
// encode_get_fpga_diagnostics_settings_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetFpgaDiagSettingsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_fpga_diagnostics_settings_req(
	    kBadIid, GET_WP_SETTINGS, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_fpga_diagnostics_settings_resp: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetFpgaDiagSettingsResp_NullCc)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t data_size = 0, reason_code = 0;
	uint8_t data[64] = {};
	auto rc = decode_get_fpga_diagnostics_settings_resp(
	    msg, buf.size(), nullptr, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_fpga_diagnostics_settings_resp: cc != NSM_SUCCESS (no pre-check)
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetFpgaDiagSettingsResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	uint8_t data[64] = {};
	auto rc = decode_get_fpga_diagnostics_settings_resp(
	    msg, buf.size(), &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_fpga_diagnostics_settings_resp: msg_len too short (cc=0=success)
// 9-byte buf with cc=0: decode_reason_code_and_cc returns OK for cc=NSM_SUCCESS
// then msg_len=9 < sizeof(hdr+resp_struct) → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetFpgaDiagSettingsResp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0=NSM_SUCCESS, short message
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t data_size = 0, reason_code = 0;
	uint8_t data[64] = {};
	auto rc = decode_get_fpga_diagnostics_settings_resp(
	    msg, buf.size(), &cc, &data_size, &reason_code, data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_enable_disable_gpu_ist_mode_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch2, EncodeEnableDisableGpuIstModeReq_NullMsg)
{
	auto rc = encode_enable_disable_gpu_ist_mode_req(0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_enable_disable_gpu_ist_mode_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeEnableDisableGpuIstModeReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_enable_disable_gpu_ist_mode_req(kBadIid, 0, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_enable_disable_gpu_ist_mode_req: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeEnableDisableGpuIstModeReq_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t value = 0;
	auto rc = decode_enable_disable_gpu_ist_mode_req(msg, buf.size(),
							 nullptr, &value);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_enable_disable_gpu_ist_mode_req: msg too short
// ===========================================================================
TEST(DevConfigBranch2, DecodeEnableDisableGpuIstModeReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dev = 0, value = 0;
	auto rc = decode_enable_disable_gpu_ist_mode_req(msg, buf.size(), &dev,
							 &value);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_enable_disable_gpu_ist_mode_req: data_size too small (correct len,
// data_size=0)
// ===========================================================================
TEST(DevConfigBranch2, DecodeEnableDisableGpuIstModeReq_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_enable_disable_gpu_ist_mode_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 < required
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t dev = 0, value = 0;
	auto rc =
	    decode_enable_disable_gpu_ist_mode_req(msg, msgLen, &dev, &value);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_enable_disable_gpu_ist_mode_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch2, DecodeEnableDisableGpuIstModeResp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_enable_disable_gpu_ist_mode_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_reconfiguration_permissions_v1_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetReconfigPermV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_reconfiguration_permissions_v1_req(
	    kBadIid, static_cast<enum reconfiguration_permissions_v1_index>(0),
	    msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_reconfiguration_permissions_v1_req: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetReconfigPermV1Req_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_reconfiguration_permissions_v1_req(msg, buf.size(),
								nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_reconfiguration_permissions_v1_req: msg too short
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetReconfigPermV1Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx =
	    static_cast<enum reconfiguration_permissions_v1_index>(0);
	auto rc = decode_get_reconfiguration_permissions_v1_req(msg, buf.size(),
								&idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_reconfiguration_permissions_v1_req: data_size too small
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetReconfigPermV1Req_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_get_reconfiguration_permissions_v1_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 < required
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx =
	    static_cast<enum reconfiguration_permissions_v1_index>(0);
	auto rc =
	    decode_get_reconfiguration_permissions_v1_req(msg, msgLen, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_reconfiguration_permissions_v1_resp: NULL msg
// (uses instance_id & 0x1f masking so pack never fails; test NULL msg)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetReconfigPermV1Resp_NullMsg)
{
	struct nsm_reconfiguration_permissions_v1 data = {};
	auto rc = encode_get_reconfiguration_permissions_v1_resp(
	    0, NSM_SUCCESS, 0, &data, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_reconfiguration_permissions_v1_resp: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetReconfigPermV1Resp_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	struct nsm_reconfiguration_permissions_v1 data = {};
	uint16_t reason = 0;
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), nullptr, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_reconfiguration_permissions_v1_resp: cc != NSM_SUCCESS
// (no pre-size check before decode_reason_code_and_cc)
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetReconfigPermV1Resp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_reconfiguration_permissions_v1 data = {};
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_reconfiguration_permissions_v1_resp: msg too short (cc=0=success)
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetReconfigPermV1Resp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0=NSM_SUCCESS, short message
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	struct nsm_reconfiguration_permissions_v1 data = {};
	auto rc = decode_get_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_reconfiguration_permissions_v1_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeSetReconfigPermV1Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_reconfiguration_permissions_v1_req(
	    kBadIid, static_cast<enum reconfiguration_permissions_v1_index>(0),
	    static_cast<enum reconfiguration_permissions_v1_setting>(0), 0,
	    msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_reconfiguration_permissions_v1_req: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetReconfigPermV1Req_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_setting cfg =
	    static_cast<enum reconfiguration_permissions_v1_setting>(0);
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, buf.size(), nullptr, &cfg, &perm);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_reconfiguration_permissions_v1_req: msg too short
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetReconfigPermV1Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx =
	    static_cast<enum reconfiguration_permissions_v1_index>(0);
	enum reconfiguration_permissions_v1_setting cfg =
	    static_cast<enum reconfiguration_permissions_v1_setting>(0);
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, buf.size(), &idx, &cfg, &perm);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_set_reconfiguration_permissions_v1_req: data_size too small
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetReconfigPermV1Req_DataSizeTooSmall)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_set_reconfiguration_permissions_v1_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 < required
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum reconfiguration_permissions_v1_index idx =
	    static_cast<enum reconfiguration_permissions_v1_index>(0);
	enum reconfiguration_permissions_v1_setting cfg =
	    static_cast<enum reconfiguration_permissions_v1_setting>(0);
	uint8_t perm = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_req(
	    msg, msgLen, &idx, &cfg, &perm);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_reconfiguration_permissions_v1_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetReconfigPermV1Resp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_reconfiguration_permissions_v1_resp(
	    msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_confidential_compute_mode_v1_resp: NULL msg
// (uses instance_id & 0x1f masking so pack never fails; test NULL msg)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetCCModeV1Resp_NullMsg)
{
	auto rc = encode_get_confidential_compute_mode_v1_resp(
	    0, NSM_SUCCESS, 0, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_confidential_compute_mode_v1_resp: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetCCModeV1Resp_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t data_size = 0, reason = 0;
	uint8_t cur = 0, pend = 0;
	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    msg, buf.size(), nullptr, &data_size, &reason, &cur, &pend);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_confidential_compute_mode_v1_resp: cc != NSM_SUCCESS
// (no pre-size check before decode_reason_code_and_cc)
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetCCModeV1Resp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, cur = 0, pend = 0;
	uint16_t data_size = 0, reason = 0;
	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &cur, &pend);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_confidential_compute_mode_v1_resp: msg_len mismatch (uses !=)
// 9-byte buf with cc=0=NSM_SUCCESS; 9 != expected_size → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetCCModeV1Resp_MsgLenMismatch)
{
	std::vector<uint8_t> buf(9, 0); // cc=0=NSM_SUCCESS, short message
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0, cur = 0, pend = 0;
	uint16_t data_size = 0, reason = 0;
	auto rc = decode_get_confidential_compute_mode_v1_resp(
	    msg, buf.size(), &cc, &data_size, &reason, &cur, &pend);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_get_device_mode_setting_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingReq_NullMsg)
{
	auto rc = encode_get_device_mode_setting_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_device_mode_setting_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_setting_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_device_mode_setting_req: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingReq_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc = decode_get_device_mode_setting_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_device_mode_setting_req: msg too short
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingReq_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	auto rc = decode_get_device_mode_setting_req(msg, buf.size(), &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_device_mode_setting_req: data_size too small
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingReq_DataSizeTooSmall)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(struct nsm_get_device_mode_setting_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 < required
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	auto rc = decode_get_device_mode_setting_req(msg, msgLen, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_device_mode_settings_resp: NULL msg
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingsResp_NullMsg)
{
	auto rc =
	    encode_get_device_mode_settings_resp(0, NSM_SUCCESS, 0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_device_mode_settings_resp: pack fail
// (uses instance_id directly without masking, so pack CAN fail with iid=32)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingsResp_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_settings_resp(kBadIid, NSM_SUCCESS, 0,
						       0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_device_mode_setting_resp: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingResp_NullArgs)
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

// ===========================================================================
// decode_get_device_mode_setting_resp: cc != NSM_SUCCESS
// (no pre-size check before decode_reason_code_and_cc)
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingResp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	enum nsm_l1_prediction_mode_config mode =
	    static_cast<enum nsm_l1_prediction_mode_config>(0);
	auto rc = decode_get_device_mode_setting_resp(msg, buf.size(), &cc,
						      &reason, &mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_device_mode_setting_resp: msg too short (cc=0=success)
// 9-byte buf with cc=0; 9 < sizeof(hdr+resp) → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingResp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0=NSM_SUCCESS, short message
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
// encode_get_device_mode_settings_v2_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingsV2Req_NullMsg)
{
	auto rc = encode_get_device_mode_settings_v2_req(0, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_device_mode_settings_v2_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingsV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_settings_v2_req(kBadIid, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_get_device_mode_settings_v2_req: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingsV2Req_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	auto rc =
	    decode_get_device_mode_settings_v2_req(msg, buf.size(), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_device_mode_settings_v2_req: msg too short
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingsV2Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	auto rc = decode_get_device_mode_settings_v2_req(msg, buf.size(), &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_get_device_mode_settings_v2_req: data_size !=
// sizeof(device_mode_index) data_size=0 != sizeof(uint32_t)=4 →
// NSM_SW_ERROR_DATA
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingsV2Req_DataSizeMismatch)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_get_device_mode_settings_v2_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 != sizeof(uint32_t)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	auto rc = decode_get_device_mode_settings_v2_req(msg, msgLen, &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_get_device_mode_settings_v2_resp: NULL msg
// (uses instance_id & INSTANCEID_MASK masking so pack never fails)
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingsV2Resp_NullMsg)
{
	uint8_t data[4] = {0};
	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, 0, data, 4, nullptr, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_get_device_mode_settings_v2_resp: NULL current_mode_data with length>0
// cc=NSM_SUCCESS, NULL data with non-zero length → NSM_SW_ERROR_NULL
// ===========================================================================
TEST(DevConfigBranch2, EncodeGetDeviceModeSettingsV2Resp_NullCurrentDataWithLen)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_get_device_mode_settings_v2_resp(
	    0, NSM_SUCCESS, 0, nullptr, 4, nullptr, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_device_mode_settings_v2_resp: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingsV2Resp_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t cur_len = 0, pend_len = 0, reason = 0;
	uint8_t cur_data[64] = {}, pend_data[64] = {};
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), nullptr, &reason, cur_data, &cur_len, pend_data,
	    &pend_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_get_device_mode_settings_v2_resp: cc != NSM_SUCCESS
// (no pre-size check before decode_reason_code_and_cc)
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingsV2Resp_CcNonSuccess)
{
	auto buf = makeErrCcBuf(); // 9 bytes, cc=0xFF
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t cur_len = 0, pend_len = 0, reason = 0;
	uint8_t cur_data[64] = {}, pend_data[64] = {};
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), &cc, &reason, cur_data, &cur_len, pend_data,
	    &pend_len);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, 0xFF);
}

// ===========================================================================
// decode_get_device_mode_settings_v2_resp: msg too short (cc=0=success)
// 9-byte buf with cc=0; 9 < min_size → NSM_SW_ERROR_LENGTH
// ===========================================================================
TEST(DevConfigBranch2, DecodeGetDeviceModeSettingsV2Resp_MsgTooShort)
{
	std::vector<uint8_t> buf(9, 0); // cc=0=NSM_SUCCESS, short message
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t cur_len = 0, pend_len = 0, reason = 0;
	uint8_t cur_data[64] = {}, pend_data[64] = {};
	auto rc = decode_get_device_mode_settings_v2_resp(
	    msg, buf.size(), &cc, &reason, cur_data, &cur_len, pend_data,
	    &pend_len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_set_device_mode_settings_v2_req: NULL msg
// ===========================================================================
TEST(DevConfigBranch2, EncodeSetDeviceModeSettingsV2Req_NullMsg)
{
	uint8_t data[4] = {0};
	auto rc =
	    encode_set_device_mode_settings_v2_req(0, 0, data, 4, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_device_mode_settings_v2_req: NULL data with non-zero length
// ===========================================================================
TEST(DevConfigBranch2, EncodeSetDeviceModeSettingsV2Req_NullDataWithLen)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_set_device_mode_settings_v2_req(0, 0, nullptr, 4, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_set_device_mode_settings_v2_req: pack fail (iid=32, no masking)
// ===========================================================================
TEST(DevConfigBranch2, EncodeSetDeviceModeSettingsV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t data[4] = {0};
	auto rc =
	    encode_set_device_mode_settings_v2_req(kBadIid, 0, data, 4, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// decode_set_device_mode_settings_v2_req: NULL args
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetDeviceModeSettingsV2Req_NullArgs)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint16_t len = 0;
	uint8_t data[64] = {};
	auto rc = decode_set_device_mode_settings_v2_req(msg, buf.size(),
							 nullptr, data, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// decode_set_device_mode_settings_v2_req: msg too short
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetDeviceModeSettingsV2Req_MsgTooShort)
{
	std::vector<uint8_t> buf(3, 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	uint16_t len = 0;
	uint8_t data[64] = {};
	auto rc = decode_set_device_mode_settings_v2_req(msg, buf.size(), &idx,
							 data, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_set_device_mode_settings_v2_req: data_size < sizeof(device_mode_index)
// data_size=0 < sizeof(uint32_t)=4 → NSM_SW_ERROR_DATA
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetDeviceModeSettingsV2Req_DataSizeTooSmall)
{
	// Size of req header: sizeof(nsm_msg_hdr) + sizeof(req) -
	// sizeof(uint8_t) is the minimum needed to pass msg_len check
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) +
	    sizeof(struct nsm_set_device_mode_settings_v2_req);
	std::vector<uint8_t> buf(msgLen, 0); // data_size=0 < sizeof(uint32_t)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint32_t idx = 0;
	uint16_t len = 0;
	uint8_t data[64] = {};
	auto rc = decode_set_device_mode_settings_v2_req(msg, msgLen, &idx,
							 data, &len);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_set_device_mode_settings_v2_resp: data_size != 0
// ===========================================================================
TEST(DevConfigBranch2, DecodeSetDeviceModeSettingsV2Resp_DataSizeNonZero)
{
	auto buf = makeNonZeroDataSizeBuf(); // 11 bytes, payload[4]=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_device_mode_settings_v2_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
