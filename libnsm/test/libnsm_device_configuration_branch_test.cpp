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
 * Branch coverage for device-configuration.c partial branches.
 *
 * Targets uncovered 1/2 and 3/4 branches via:
 * - Error injection subtype mismatches in encode static helpers
 * - Wrong data_size in encode static helpers
 * - Loop body coverage for leak_detect and gpio_spoofing helpers
 * - rc/cc FALSE branches in decode wrappers
 * - data_size validation checks in decode functions
 */

#include "base.h"
#include "device-configuration.h"
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::vector<uint8_t> makeBuf(size_t sz)
{
	return std::vector<uint8_t>(sz, 0);
}

// Minimum NSM message buffer for encode (nsm_msg_hdr + request/response)
static constexpr size_t kMsgBufSz =
    sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_payload_req) + 256;

// ===========================================================================
// L43 FALSE: decode_set_protection_options_req
// decode_common_req returns non-SUCCESS (msg=NULL) → if body not entered
// ===========================================================================
TEST(DevConfigBranch, DecodeSetProtectionOptionsReq_CommonReqFail)
{
	uint8_t mode = 0;
	// NULL msg → decode_common_req → NSM_SW_ERROR_NULL → L43 FALSE
	auto rc = decode_set_protection_options_req(nullptr, 0, &mode);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L119 FALSE: decode_get_protection_options_resp
// rc == NSM_SW_SUCCESS && *cc == NSM_SUCCESS → FALSE when decode_common_resp
// returns non-SUCCESS (msg=NULL)
// ===========================================================================
TEST(DevConfigBranch, DecodeGetProtectionOptionsResp_CommonRespFail)
{
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t mode = 0;
	// NULL msg → decode_common_resp fails → L119 FALSE
	auto rc =
	    decode_get_protection_options_resp(nullptr, 0, &cc, &reason, &mode);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// Encode error injection static helpers: subtype mismatch and wrong data_size
// Called indirectly through encode_set_error_injection_payload_req.
// ===========================================================================

// L152 TRUE: encode_fatal_error_injection_payload subtype mismatch
// dispatch=FATAL but in_payload->error_injection_subtype != FATAL
TEST(DevConfigBranch, EncodeSetEIPayloadReq_FatalSubtypeMismatch)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_fatal_payload payload = {};
	payload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY; // ≠ FATAL(0)
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload),
	    EI_DEVICE_ERRORS, EI_DEVICE_ERRORS_SUBTYPE_FATAL, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L165 TRUE: encode_port_recovery_error_injection_payload wrong data_size
TEST(DevConfigBranch, EncodeSetEIPayloadReq_PortRecoveryWrongSize)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_port_recovery_payload payload = {};
	payload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY;
	// Pass wrong data_size (1 instead of sizeof struct)
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), 1, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L176 TRUE: encode_port_recovery subtype mismatch
TEST(DevConfigBranch, EncodeSetEIPayloadReq_PortRecoverySubtypeMismatch)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_port_recovery_payload payload = {};
	payload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_FATAL; // ≠ PORT_RECOVERY(1)
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload),
	    EI_DEVICE_ERRORS, EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L192 TRUE: encode_usb_emulation_error_injection_payload wrong data_size
TEST(DevConfigBranch, EncodeSetEIPayloadReq_UsbEmuWrongSize)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_usb_emu_payload payload = {};
	payload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION;
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), 1, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L202 TRUE: encode_usb_emulation subtype mismatch
TEST(DevConfigBranch, EncodeSetEIPayloadReq_UsbEmuSubtypeMismatch)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_usb_emu_payload payload = {};
	payload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY; // ≠ USB_EMULATION(2)
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), sizeof(payload),
	    EI_DEVICE_ERRORS, EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L219 TRUE: encode_leak_detect min size too small (data_size < base - 2)
// base size (zero sensors) = sizeof(nsm_error_injection_leak_payload) - 2
TEST(DevConfigBranch, EncodeSetEIPayloadReq_LeakDetectMinSizeTooSmall)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_leak_payload payload = {};
	payload.error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	// min valid size = sizeof - sizeof(uint16_t) = sizeof - 2
	// Pass size of 1 (< min) → L219 TRUE
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), 1, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L230 TRUE: encode_leak_detect subtype mismatch
TEST(DevConfigBranch, EncodeSetEIPayloadReq_LeakDetectSubtypeMismatch)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// Use min valid size (number_of_sensors=0) but wrong subtype
	nsm_error_injection_leak_payload payload = {};
	payload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_PORT_RECOVERY; // ≠ LEAK_DETECT(3)
	payload.number_of_sensors = 0;
	size_t dataSize =
	    sizeof(nsm_error_injection_leak_payload) - sizeof(uint16_t);
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), dataSize,
	    EI_DEVICE_ERRORS, EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// L239 TRUE: encode_leak_detect exact size mismatch
// number_of_sensors=1 → expected = base + 1*2*2 = base+4
// but pass base (= base + 0 sensors worth)
TEST(DevConfigBranch, EncodeSetEIPayloadReq_LeakDetectExactSizeMismatch)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// Allocate enough for 1 sensor
	std::vector<uint8_t> payloadBuf(
	    sizeof(nsm_error_injection_leak_payload) + 4, 0);
	auto *payload = reinterpret_cast<nsm_error_injection_leak_payload *>(
	    payloadBuf.data());
	payload->error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	payload->number_of_sensors = 1; // expects base+4 bytes total
	// But pass base size only (sensors=0 worth) → size mismatch
	size_t wrongSize =
	    sizeof(nsm_error_injection_leak_payload) - sizeof(uint16_t);
	auto rc = encode_set_error_injection_payload_req(
	    0, payloadBuf.data(), wrongSize, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L243 TRUE: encode_leak_detect loop body (number_of_sensors=1, correct size)
TEST(DevConfigBranch, EncodeSetEIPayloadReq_LeakDetectLoopBody)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// base + 1 sensor = base - sizeof(uint16_t) + 1*2*sizeof(uint16_t)
	//                 = (sizeof(leak_payload)-2) + 4
	size_t dataSize = sizeof(nsm_error_injection_leak_payload) -
			  sizeof(uint16_t) + (1 * 2 * sizeof(uint16_t));
	std::vector<uint8_t> payloadBuf(dataSize, 0);
	auto *payload = reinterpret_cast<nsm_error_injection_leak_payload *>(
	    payloadBuf.data());
	payload->error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	payload->number_of_sensors = 1;
	payload->sensors_data[0] = 0x0001; // sensor_id
	payload->sensors_data[1] = 0x0002; // sensor_value
	auto rc = encode_set_error_injection_payload_req(
	    0, payloadBuf.data(), dataSize, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// L444 TRUE: encode_gpio_spoofing min size too small
TEST(DevConfigBranch, EncodeSetEIPayloadReq_GpioSpoofingMinSizeTooSmall)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// min valid = sizeof(gpio_payload) - sizeof(uint16_t) = 4 - 2 = 2
	// Pass 1 (< 2) → L444 TRUE
	nsm_error_injection_gpio_spoofing_payload payload = {};
	auto rc = encode_set_error_injection_payload_req(
	    0, reinterpret_cast<const uint8_t *>(&payload), 1, EI_GPIO_SPOOFING,
	    0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L458 TRUE: encode_gpio_spoofing exact size mismatch
// count_of_gpio=1 → expected = base + 1*2 = 2+2=4
// but pass base (2) → mismatch
TEST(DevConfigBranch, EncodeSetEIPayloadReq_GpioSpoofingExactSizeMismatch)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	std::vector<uint8_t> payloadBuf(
	    sizeof(nsm_error_injection_gpio_spoofing_payload) + 4, 0);
	auto *payload =
	    reinterpret_cast<nsm_error_injection_gpio_spoofing_payload *>(
		payloadBuf.data());
	payload->count_of_gpio = 1; // expects 4 bytes total
	// Pass base only = sizeof - sizeof(uint16_t) = 2 → mismatch
	size_t wrongSize = sizeof(nsm_error_injection_gpio_spoofing_payload) -
			   sizeof(uint16_t);
	auto rc = encode_set_error_injection_payload_req(
	    0, payloadBuf.data(), wrongSize, EI_GPIO_SPOOFING, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// L461 TRUE: encode_gpio_spoofing loop body (count_of_gpio=1, correct size)
TEST(DevConfigBranch, EncodeSetEIPayloadReq_GpioSpoofingLoopBody)
{
	auto buf = makeBuf(kMsgBufSz);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// size = base - sizeof(uint16_t) + 1*sizeof(uint16_t) = 2 + 2 = 4
	size_t dataSize = sizeof(nsm_error_injection_gpio_spoofing_payload) -
			  sizeof(uint16_t) + sizeof(uint16_t);
	std::vector<uint8_t> payloadBuf(dataSize, 0);
	auto *payload =
	    reinterpret_cast<nsm_error_injection_gpio_spoofing_payload *>(
		payloadBuf.data());
	payload->count_of_gpio = 1;
	payload->gpio_data[0] = 0x0042;
	auto rc = encode_set_error_injection_payload_req(
	    0, payloadBuf.data(), dataSize, EI_GPIO_SPOOFING, 0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L1206 TRUE: decode_get_fpga_diagnostics_settings_req
// if (request->hdr.data_size < 1) TRUE — data_size=0 < 1
// Function doesn't call decode_common_req so no valid header needed
// ===========================================================================
TEST(DevConfigBranch, DecodeGetFpgaDiagSettingsReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req),
	    0);
	// request->hdr.data_size at payload[1] = 0 < 1 → L1206 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	enum fpga_diagnostics_settings_data_index idx = {};
	auto rc =
	    decode_get_fpga_diagnostics_settings_req(msg, buf.size(), &idx);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1369 path B: decode_get_gpu_presence_resp
// ret = NSM_SW_SUCCESS but *cc != NSM_SUCCESS → L1369 TRUE via second cond
// Need decode_get_fpga_diagnostics_settings_resp to return SUCCESS with cc!=0
// Use: msg_len == nsm_hdr+4, payload[1]=0xFF (error CC) →
// decode_reason_code_and_cc
//      returns NSM_SW_SUCCESS with *cc=0xFF
// ===========================================================================
TEST(DevConfigBranch, DecodeGetGpuPresenceResp_CcNonSuccess)
{
	// Exactly sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp) = 9
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF; // completion_code = error
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t presence = 0;
	// decode_get_fpga_diagnostics_settings_resp → decode_reason_code_and_cc
	//   → *cc=0xFF, rc=NSM_SW_SUCCESS, returns NSM_SW_SUCCESS
	// Then at L1369: ret=0, *cc=0xFF → TRUE path B
	auto rc = decode_get_gpu_presence_resp(msg, buf.size(), &cc, &reason,
					       &presence);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// L1413 path B: decode_get_gpu_ist_mode_resp — same pattern
TEST(DevConfigBranch, DecodeGetGpuIstModeResp_CcNonSuccess)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t mode = 0;
	auto rc =
	    decode_get_gpu_ist_mode_resp(msg, buf.size(), &cc, &reason, &mode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L1454 3/4 path B: decode_get_fpga_diagnostics_settings_wp_resp
// Same pattern: ret=SUCCESS but *cc != NSM_SUCCESS
// ===========================================================================
TEST(DevConfigBranch, DecodeGetFpgaDiagSettingsWpResp_CcNonSuccess)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
	buf[sizeof(nsm_msg_hdr) + 1] = 0xFF;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_fpga_diagnostics_settings_wp data = {};
	auto rc = decode_get_fpga_diagnostics_settings_wp_resp(
	    msg, buf.size(), &cc, &reason, &data);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_NE(cc, NSM_SUCCESS);
}

// ===========================================================================
// L877 TRUE: decode_activate_error_injection_payload_resp
// if (data_size != 0) TRUE — data_size=1 set at payload[4..5]
// decode_common_resp returns SUCCESS when cc=NSM_SUCCESS and msg_len >= 11
// ===========================================================================
TEST(DevConfigBranch, DecodeActivateEiPayloadResp_DataSizeNonZero)
{
	// sizeof(nsm_msg_hdr)+sizeof(nsm_common_resp) = 5+6 = 11
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // data_size = 1 != 0 → L877 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_activate_error_injection_payload_resp(msg, buf.size(),
							       &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L914 TRUE: decode_set_error_injection_mode_v1_req
// if (req->hdr.data_size != sizeof(uint8_t)) TRUE — data_size=0 != 1
// Needs valid PCI_VENDOR_ID header: encode first, then corrupt data_size
// ===========================================================================
TEST(DevConfigBranch, DecodeSetEiModeV1Req_DataSizeWrong)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_mode_v1_req),
	    0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// Encode to get valid header (PCI_VENDOR_ID)
	encode_set_error_injection_mode_v1_req(0, 1, msg);
	// Corrupt data_size to 0 ≠ sizeof(uint8_t)=1 → L914 TRUE
	auto *req = reinterpret_cast<nsm_set_error_injection_mode_v1_req *>(
	    msg->payload);
	req->hdr.data_size = 0;
	uint8_t mode = 0;
	auto rc = decode_set_error_injection_mode_v1_req(
	    reinterpret_cast<const nsm_msg *>(msg), buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L937 TRUE: decode_set_error_injection_mode_v1_resp
// if (data_size != 0) TRUE — data_size=1
// ===========================================================================
TEST(DevConfigBranch, DecodeSetEiModeV1Resp_DataSizeNonZero)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // data_size = 1 != 0 → L937 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_error_injection_mode_v1_resp(msg, buf.size(), &cc,
							  &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L992 TRUE: decode_get_error_injection_mode_v1_resp
// if (data_size != sizeof(nsm_error_injection_mode_v1)) TRUE
// data_size=0 (zero-fill) ≠ sizeof(nsm_error_injection_mode_v1)=5
// Needs: decode_common_resp SUCCESS, msg_len >= nsm_hdr+sizeof(resp)=16
// ===========================================================================
TEST(DevConfigBranch, DecodeGetEiModeV1Resp_DataSizeMismatch)
{
	// sizeof(nsm_get_error_injection_mode_v1_resp) = 6+5 = 11
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_mode_v1_resp),
	    0);
	// data_size at payload[4..5] = 0 ≠ 5 → L992 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_error_injection_mode_v1 data = {};
	auto rc = decode_get_error_injection_mode_v1_resp(msg, buf.size(), &cc,
							  &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1041 TRUE: decode_set_current_error_injection_types_v1_req
// if (req->hdr.data_size != sizeof(nsm_error_injection_types_mask)) TRUE
// data_size=0 ≠ 8; needs valid header via encode
// ===========================================================================
TEST(DevConfigBranch, DecodeSetCurrentEiTypesV1Req_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_set_error_injection_types_mask_req),
	    0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	nsm_error_injection_types_mask mask = {};
	// Encode to get valid header
	encode_set_current_error_injection_types_v1_req(0, &mask, msg);
	// Corrupt data_size to 0 ≠ sizeof(nsm_error_injection_types_mask)=8
	auto *req = reinterpret_cast<nsm_set_error_injection_types_mask_req *>(
	    msg->payload);
	req->hdr.data_size = 0;
	nsm_error_injection_types_mask out = {};
	auto rc = decode_set_current_error_injection_types_v1_req(
	    reinterpret_cast<const nsm_msg *>(msg), buf.size(), &out);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1067 TRUE: decode_set_current_error_injection_types_v1_resp
// if (data_size != 0) TRUE — data_size=1; returns NSM_SW_ERROR_NULL
// ===========================================================================
TEST(DevConfigBranch, DecodeSetCurrentEiTypesV1Resp_DataSizeNonZero)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // data_size = 1 != 0 → L1067 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_current_error_injection_types_v1_resp(
	    msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L1149 TRUE: decode_get_error_injection_types_v1_resp
// if (data_size != sizeof(nsm_error_injection_types_mask)) TRUE
// data_size=0 (zero-fill) ≠ 8; msg_len >= nsm_hdr+sizeof(resp)=19
// ===========================================================================
TEST(DevConfigBranch, DecodeGetEiTypesV1Resp_DataSizeMismatch)
{
	// sizeof(nsm_get_error_injection_types_mask_resp) = 6+8 = 14
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_get_error_injection_types_mask_resp),
	    0);
	// data_size at payload[4..5] = 0 ≠ 8 → L1149 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	nsm_error_injection_types_mask data = {};
	auto rc = decode_get_error_injection_types_v1_resp(msg, buf.size(), &cc,
							   &reason, &data);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1810 TRUE: decode_set_confidential_compute_mode_v1_req
// if (msg_len != exact_size) TRUE — msg_len = exact+1 → mismatch
// ===========================================================================
TEST(DevConfigBranch, DecodeSetCCModeV1Req_MsgLenWrong)
{
	// exact = nsm_hdr + sizeof(nsm_set_confidential_compute_mode_v1_req)
	//       = 5+3 = 8; pass 9 → mismatch
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_set_confidential_compute_mode_v1_req) + 1,
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc =
	    decode_set_confidential_compute_mode_v1_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L1819 TRUE: decode_set_confidential_compute_mode_v1_req
// if (request->hdr.data_size != expected) TRUE
// msg_len == exact (8), but req->hdr.data_size = 0 ≠ 1
// ===========================================================================
TEST(DevConfigBranch, DecodeSetCCModeV1Req_DataSizeWrong)
{
	// exact msg_len = 5+3 = 8; data_size expected = 3-2 = 1
	// req->hdr.data_size at payload[1] = 0 (zero-fill) ≠ 1 → L1819 TRUE
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) +
		sizeof(nsm_set_confidential_compute_mode_v1_req),
	    0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc =
	    decode_set_confidential_compute_mode_v1_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1916 TRUE: decode_get_EGM_mode_resp
// if (resp->hdr.command != NSM_GET_EGM_MODE) TRUE
// command at payload[0] = 0 (zero-fill) ≠ 0x43 = NSM_GET_EGM_MODE
// Needs: decode_reason_code_and_cc SUCCESS (cc=0), msg_len == exact (12)
// ===========================================================================
TEST(DevConfigBranch, DecodeGetEGMModeResp_CommandMismatch)
{
	// sizeof(nsm_get_EGM_mode_resp) = 6+1 = 7; exact = 5+7 = 12
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
	// command at payload[0] = 0 ≠ 0x43 → L1916 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1921 TRUE: decode_get_EGM_mode_resp
// if (*data_size != expected_size) TRUE
// Set command = NSM_GET_EGM_MODE (0x43), data_size = 0 ≠ 1
// ===========================================================================
TEST(DevConfigBranch, DecodeGetEGMModeResp_DataSizeMismatch)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp), 0);
	buf[sizeof(nsm_msg_hdr) + 0] = NSM_GET_EGM_MODE; // command = 0x43
	// data_size at payload[4..5] = 0 ≠ sizeof(nsm_get_EGM_mode_resp)-6=1
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t ds = 0, reason = 0;
	bitfield8_t flags = {};
	auto rc = decode_get_EGM_mode_resp(msg, buf.size(), &cc, &ds, &reason,
					   &flags);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L1962 TRUE: decode_set_EGM_mode_req
// if (request->hdr.data_size != sizeof(uint8_t)) TRUE
// msg_len == exact (8), data_size at payload[1] = 0 ≠ 1
// ===========================================================================
TEST(DevConfigBranch, DecodeSetEGMModeReq_DataSizeWrong)
{
	// sizeof(nsm_set_EGM_mode_req) = 2+1 = 3; exact = 5+3 = 8
	// data_size at payload[1] = 0 (zero-fill) ≠ sizeof(uint8_t)=1
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_EGM_mode_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t mode = 0;
	auto rc = decode_set_EGM_mode_req(msg, buf.size(), &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L2023 TRUE: decode_set_device_mode_settings_req
// if (request->hdr.data_size != expected) TRUE
// msg_len == exact (9), data_size at payload[1] = 0 ≠ 2
// ===========================================================================
TEST(DevConfigBranch, DecodeSetDeviceModeSettingsReq_DataSizeWrong)
{
	// sizeof(nsm_set_device_mode_setting_req) = 2+1+1 = 4; exact = 5+4 = 9
	// expected data_size = 4-2 = 2
	// req->hdr.data_size at payload[1] = 0 (zero-fill) ≠ 2 → L2023 TRUE
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_setting_req), 0);
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t idx = 0;
	nsm_l1_prediction_mode_config mode = DISABLED;
	auto rc =
	    decode_set_device_mode_settings_req(msg, buf.size(), &idx, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// L2050 TRUE: decode_set_device_mode_setting_resp
// if (data_size != 0) TRUE — data_size=1
// ===========================================================================
TEST(DevConfigBranch, DecodeSetDeviceModeSettingResp_DataSizeNonZero)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	buf[sizeof(nsm_msg_hdr) + 4] = 1; // data_size = 1 != 0 → L2050 TRUE
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc =
	    decode_set_device_mode_setting_resp(msg, buf.size(), &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L388-391: decode_leak_detect_error_injection_payload loop body
// Call encode_get_error_injection_payload_resp + decode with 1 sensor so
// the loop body (i < number_of_sensors) executes at least once.
// ===========================================================================
TEST(DevConfigBranch, DecodeGetEiPayloadResp_LeakDetectOneSensor)
{
	// leak payload with 1 sensor: 4 (base) + 1*2*2 (sensor_id+value) = 8
	const size_t payloadSize = sizeof(nsm_error_injection_leak_payload) -
				   sizeof(uint16_t) +
				   (1 * 2 * sizeof(uint16_t));
	std::vector<uint8_t> payloadBuf(payloadSize, 0);
	auto *payload = reinterpret_cast<nsm_error_injection_leak_payload *>(
	    payloadBuf.data());
	payload->error_injection_subtype = EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	payload->number_of_sensors = 1;
	payload->sensors_data[0] = 0x0001; // sensor_id
	payload->sensors_data[1] = 0x0002; // sensor_value

	// msg_len = 5(hdr) + 13(resp) - 1(fault_payload[1]) + payloadSize
	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(nsm_get_error_injection_payload_resp) -
			      sizeof(uint8_t) + payloadSize;
	std::vector<uint8_t> buf(msgLen + 8, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_get_error_injection_payload_resp(
	    0, NSM_SUCCESS, 0, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, payloadBuf.data(),
	    payloadSize, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode: loop body at L388-391 runs once (number_of_sensors=1)
	uint8_t outCc = 0;
	uint16_t outReason = 0;
	std::vector<uint8_t> outBuf(128, 0);
	size_t outSize = 0;
	rc = decode_get_error_injection_payload_resp(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, &outCc, &outReason,
	    outBuf.data(), &outSize);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(outSize, payloadSize);
}

// ===========================================================================
// L432: decode_gpio_spoofing_error_injection_payload loop body
// Same pattern but with EI_GPIO_SPOOFING and count_of_gpio=1.
// ===========================================================================
TEST(DevConfigBranch, DecodeGetEiPayloadResp_GpioSpoofingOneGpio)
{
	// gpio payload with 1 gpio: 2 (base) + 1*2 = 4 bytes
	const size_t payloadSize =
	    sizeof(nsm_error_injection_gpio_spoofing_payload) -
	    sizeof(uint16_t) + sizeof(uint16_t);
	std::vector<uint8_t> payloadBuf(payloadSize, 0);
	auto *payload =
	    reinterpret_cast<nsm_error_injection_gpio_spoofing_payload *>(
		payloadBuf.data());
	payload->count_of_gpio = 1;
	payload->gpio_data[0] = 0x0042;

	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(nsm_get_error_injection_payload_resp) -
			      sizeof(uint8_t) + payloadSize;
	std::vector<uint8_t> buf(msgLen + 8, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_get_error_injection_payload_resp(
	    0, NSM_SUCCESS, 0, EI_GPIO_SPOOFING, 0, payloadBuf.data(),
	    payloadSize, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode: loop body at L432 runs once (count_of_gpio=1)
	uint8_t outCc = 0;
	uint16_t outReason = 0;
	std::vector<uint8_t> outBuf(128, 0);
	size_t outSize = 0;
	rc = decode_get_error_injection_payload_resp(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, EI_GPIO_SPOOFING, 0,
	    &outCc, &outReason, outBuf.data(), &outSize);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(outSize, payloadSize);
}

// ===========================================================================
// L705-706: encode_get_error_injection_payload_resp USB emulation path
// L772-773: decode_get_error_injection_payload_resp USB emulation path
// ===========================================================================
TEST(DevConfigBranch, EncodeDecodeGetEiPayloadResp_UsbEmulation)
{
	nsm_error_injection_usb_emu_payload usbPayload = {};
	usbPayload.error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION;
	usbPayload.payload = 1;
	usbPayload.bus_number = 2;
	usbPayload.error = 3;
	usbPayload.address = 4;
	const size_t payloadSize = sizeof(nsm_error_injection_usb_emu_payload);

	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(nsm_get_error_injection_payload_resp) -
			      sizeof(uint8_t) + payloadSize;
	std::vector<uint8_t> buf(msgLen + 8, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	// Encode: covers L705-706
	// (encode_usb_emulation_error_injection_payload)
	auto rc = encode_get_error_injection_payload_resp(
	    0, NSM_SUCCESS, 0, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION,
	    reinterpret_cast<const uint8_t *>(&usbPayload), payloadSize, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Decode: covers L772-773
	// (decode_usb_emulation_error_injection_payload)
	uint8_t outCc = 0;
	uint16_t outReason = 0;
	std::vector<uint8_t> outBuf(128, 0);
	size_t outSize = 0;
	rc = decode_get_error_injection_payload_resp(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_USB_EMULATION, &outCc, &outReason,
	    outBuf.data(), &outSize);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(outSize, payloadSize);
}

// ===========================================================================
// L30 FALSE: encode_set_protection_options_req with instance_id > MAX
// ===========================================================================
TEST(DevConfigBranch, EncodeSetProtectionOptionsReq_BadInstanceId)
{
	std::vector<uint8_t> buf(kMsgBufSz, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// NSM_INSTANCE_MAX is 31; 32 causes encode_common_req to fail
	auto rc = encode_set_protection_options_req(32, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// ===========================================================================
// L44 TRUE: decode_set_protection_options_req with protection_mode=NULL
// encode first to get valid PCI vendor-id header so decode_common_req passes
// ===========================================================================
TEST(DevConfigBranch, DecodeSetProtectionOptionsReq_NullProtectionMode)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_protection_options_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	encode_set_protection_options_req(0, 0, msg);
	auto rc = decode_set_protection_options_req(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L47 TRUE: decode_set_protection_options_req with msg too short
// encode into full buffer first; decode with shortLen passes decode_common_req
// (>= hdr+nsm_common_req=7) but fails the set_protection_options size check
// (< hdr+nsm_set_protection_options_req=8)
// ===========================================================================
TEST(DevConfigBranch, DecodeSetProtectionOptionsReq_MsgTooShort)
{
	const size_t fullLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_protection_options_req);
	std::vector<uint8_t> buf(fullLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	encode_set_protection_options_req(0, 0, msg);
	const size_t shortLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_req);
	uint8_t mode = 0;
	auto rc = decode_set_protection_options_req(
	    reinterpret_cast<const nsm_msg *>(msg), shortLen, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L55 TRUE: decode_set_protection_options_req with wrong data_size
// encode first to get valid header, then corrupt req->hdr.data_size to 0
// ===========================================================================
TEST(DevConfigBranch, DecodeSetProtectionOptionsReq_WrongDataSize)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_set_protection_options_req);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	encode_set_protection_options_req(0, 0, msg);
	// corrupt nsm_common_req.data_size (payload[1]) to 0 (expect 1)
	msg->payload[1] = 0;
	uint8_t mode = 0;
	auto rc = decode_set_protection_options_req(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L78 TRUE: decode_set_protection_options_resp with data_size != 0
// ===========================================================================
TEST(DevConfigBranch, DecodeSetProtectionOptionsResp_DataSizeNonZero)
{
	const size_t msgLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp);
	std::vector<uint8_t> buf(msgLen, 0);
	// nsm_common_resp.data_size at payload[4..5]: set to 1
	buf[sizeof(nsm_msg_hdr) + 4] = 1;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	auto rc = decode_set_protection_options_resp(msg, msgLen, &cc, &reason);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L123 TRUE: decode_get_protection_options_resp with msg too short
// (cc=NSM_SUCCESS, data_size=0 so decode_common_resp succeeds,
//  but then msg_len < sizeof(hdr)+sizeof(resp) → return LENGTH error)
// ===========================================================================
TEST(DevConfigBranch, DecodeGetProtectionOptionsResp_MsgTooShort)
{
	// Just the header + common_resp, shorter than the full
	// get_protection_options_resp
	const size_t msgLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp);
	std::vector<uint8_t> buf(msgLen, 0);
	// cc=NSM_SUCCESS (payload[1]=0), data_size=0 (payload[4..5]=0)
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t mode = 0;
	auto rc = decode_get_protection_options_resp(msg, msgLen, &cc, &reason,
						     &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L128 TRUE: decode_get_protection_options_resp with data_size mismatch
// ===========================================================================
TEST(DevConfigBranch, DecodeGetProtectionOptionsResp_DataSizeMismatch)
{
	const size_t msgLen =
	    sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp);
	std::vector<uint8_t> buf(msgLen, 0);
	// cc=NSM_SUCCESS (payload[1]=0), data_size=99≠sizeof(uint8_t)=1
	buf[sizeof(nsm_msg_hdr) + 4] = 99;
	auto *msg = reinterpret_cast<const nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reason = 0;
	uint8_t mode = 0;
	auto rc = decode_get_protection_options_resp(msg, msgLen, &cc, &reason,
						     &mode);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// L528 TRUE: decode_set_error_injection_payload_req with NULL data
// Must encode first and fix hdr.data_size (same pattern as
// libnsm_device_configuration_test.cpp setErrorInjectionPayload test)
// ===========================================================================
TEST(DevConfigBranch, DecodeSetEiPayloadReq_NullData)
{
	// Use LEAK_DETECT payload with 0 sensors as a minimal valid payload
	const size_t payloadSize =
	    sizeof(nsm_error_injection_leak_payload) - sizeof(uint16_t);
	std::vector<uint8_t> payloadBuf(payloadSize, 0);
	auto *leakPayload =
	    reinterpret_cast<nsm_error_injection_leak_payload *>(
		payloadBuf.data());
	leakPayload->error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	leakPayload->number_of_sensors = 0;

	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(nsm_set_error_injection_payload_req) +
			      payloadSize - sizeof(uint8_t);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_set_error_injection_payload_req(
	    0, payloadBuf.data(), payloadSize, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Fix hdr.data_size for decode: decode computes
	// fault_payload_size = data_size - (sizeof(req)-1), so set
	// data_size = payloadSize + sizeof(req) - 1
	auto *req = reinterpret_cast<nsm_set_error_injection_payload_req *>(
	    msg->payload);
	uint16_t correctedDataSize =
	    static_cast<uint16_t>(payloadSize) +
	    (sizeof(nsm_set_error_injection_payload_req) - sizeof(uint8_t));
	req->hdr.data_size = htole16(correctedDataSize);

	uint16_t eiType = 0, eiSubtype = 0;
	size_t dataSz = 0;
	rc = decode_set_error_injection_payload_req(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, &eiType, &eiSubtype,
	    nullptr, &dataSz);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// ===========================================================================
// L504-507 (encode) and L569-573 (decode) LEAK_DETECT subtype
// hdr.data_size must be corrected after encode for decode to compute the
// correct fault_payload_size (same workaround as main test file)
// ===========================================================================
TEST(DevConfigBranch, EncodeDecodeSetEiPayloadReq_LeakDetect)
{
	// 1 sensor: base(4) + 1*2*sizeof(uint16_t)(4) = 8 bytes
	const size_t payloadSize = sizeof(nsm_error_injection_leak_payload) -
				   sizeof(uint16_t) +
				   (1 * 2 * sizeof(uint16_t));
	std::vector<uint8_t> payloadBuf(payloadSize, 0);
	auto *leakPayload =
	    reinterpret_cast<nsm_error_injection_leak_payload *>(
		payloadBuf.data());
	leakPayload->error_injection_subtype =
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT;
	leakPayload->number_of_sensors = 1;
	leakPayload->sensors_data[0] = 0x0001;
	leakPayload->sensors_data[1] = 0x0002;

	const size_t msgLen = sizeof(nsm_msg_hdr) +
			      sizeof(nsm_set_error_injection_payload_req) +
			      payloadSize - sizeof(uint8_t);
	std::vector<uint8_t> buf(msgLen, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	// Encode: covers L504-507
	auto rc = encode_set_error_injection_payload_req(
	    0, payloadBuf.data(), payloadSize, EI_DEVICE_ERRORS,
	    EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Fix hdr.data_size: encode sets it to data_size+6, but decode needs
	// data_size = payloadSize + sizeof(req) - 1 (= payloadSize + 12)
	auto *req = reinterpret_cast<nsm_set_error_injection_payload_req *>(
	    msg->payload);
	uint16_t correctedDataSize =
	    static_cast<uint16_t>(payloadSize) +
	    (sizeof(nsm_set_error_injection_payload_req) - sizeof(uint8_t));
	req->hdr.data_size = htole16(correctedDataSize);

	// Decode: covers L569-573
	uint16_t outEiType = 0, outEiSubtype = 0;
	std::vector<uint8_t> outBuf(256, 0);
	size_t outSize = 0;
	rc = decode_set_error_injection_payload_req(
	    reinterpret_cast<const nsm_msg *>(msg), msgLen, &outEiType,
	    &outEiSubtype, outBuf.data(), &outSize);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(outEiSubtype, EI_DEVICE_ERRORS_SUBTYPE_LEAK_DETECT);
}
