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
 * Coverage for the optical module telemetry codec added to network-ports.c
 * (NSM Type-1 cmd 0x14 Query Port Telemetry Counter v2, cmd 0x15 Query Port
 * Telemetry Capabilities, and the group 0x09 Optical Module per-lane sample
 * decoders):
 *
 *  - decode_optical_module_power_bias_lane_record
 *  - decode_optical_module_snr_lane_record
 *  - encode_query_port_telemetry_v2_req / decode_query_port_telemetry_v2_req
 *  - encode_query_port_telemetry_caps_req /
 *    decode_query_port_telemetry_caps_req
 *  - encode_query_port_telemetry_caps_resp /
 *    decode_query_port_telemetry_caps_resp (32-byte supported_groups_bitmask)
 */

#include "base.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

static constexpr uint8_t kBadIid = 32; // > NSM_INSTANCE_MAX(31)

// ===========================================================================
// decode_optical_module_power_bias_lane_record
// ===========================================================================

TEST(OpticalModuleCodec, DecodePowerBias_Success_LittleEndianRoundTrip)
{
	uint16_t raw = htole16(12345);
	uint16_t out = 0;
	auto rc = decode_optical_module_power_bias_lane_record(
	    reinterpret_cast<const uint8_t *>(&raw), sizeof(raw), &out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(out, 12345);
}

TEST(OpticalModuleCodec, DecodePowerBias_NullData_ReturnsErrorNull)
{
	uint16_t out = 0;
	auto rc =
	    decode_optical_module_power_bias_lane_record(nullptr, 2, &out);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, DecodePowerBias_NullOutValue_ReturnsErrorNull)
{
	uint16_t raw = 0;
	auto rc = decode_optical_module_power_bias_lane_record(
	    reinterpret_cast<const uint8_t *>(&raw), sizeof(raw), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, DecodePowerBias_TooShort_ReturnsErrorLength)
{
	uint8_t raw[1] = {0};
	uint16_t out = 0;
	auto rc = decode_optical_module_power_bias_lane_record(raw, sizeof(raw),
							       &out);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(OpticalModuleCodec, DecodePowerBias_WrongLength_TooLong_ReturnsErrorLength)
{
	// data_len must equal sizeof(NvU16) exactly, not just be >= it.
	uint8_t raw[4] = {0};
	uint16_t out = 0;
	auto rc = decode_optical_module_power_bias_lane_record(raw, sizeof(raw),
							       &out);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// decode_optical_module_snr_lane_record
// ===========================================================================

TEST(OpticalModuleCodec, DecodeSnr_Success_RawValueNotFloat)
{
	// raw PRM value 5665 -> 22.13 dB (caller divides by 256); decoder must
	// return the raw integer, not bit-cast it as an IEEE754 float.
	uint32_t raw = htole32(5665);
	uint32_t out = 0;
	auto rc = decode_optical_module_snr_lane_record(
	    reinterpret_cast<const uint8_t *>(&raw), sizeof(raw), &out);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(out, 5665u);
	EXPECT_NEAR(static_cast<double>(out) / 256.0, 22.13, 0.01);
}

TEST(OpticalModuleCodec, DecodeSnr_NullData_ReturnsErrorNull)
{
	uint32_t out = 0;
	auto rc = decode_optical_module_snr_lane_record(nullptr, 4, &out);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, DecodeSnr_NullOutValue_ReturnsErrorNull)
{
	uint32_t raw = 0;
	auto rc = decode_optical_module_snr_lane_record(
	    reinterpret_cast<const uint8_t *>(&raw), sizeof(raw), nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, DecodeSnr_TooShort_ReturnsErrorLength)
{
	uint8_t raw[3] = {0};
	uint32_t out = 0;
	auto rc = decode_optical_module_snr_lane_record(raw, sizeof(raw), &out);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// encode_query_port_telemetry_v2_req / decode_query_port_telemetry_v2_req
// ===========================================================================

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Req_NullMsg)
{
	auto rc = encode_query_port_telemetry_v2_req(
	    0, 1, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE, 0, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Req_GroupBelowRange_Rejected)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(0, 1, 0x00, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Req_GroupAboveRange_Rejected)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(0, 1, 0x0A, 0, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Req_GroupLowerBoundary_Ok)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(
	    0, 1, NSM_PORT_TELEMETRY_GROUP_PHY_ERRORS, 0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Req_OpticalModuleGroup_Ok)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(
	    0, 1, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE, 0, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Req_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(
	    kBadIid, 1, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE, 0, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryV2Req_RoundTrip)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(
	    0, 7, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE, 0xDEADBEEF, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint16_t portIndex = 0;
	uint8_t groupId = 0;
	uint32_t sequenceToken = 0;
	rc = decode_query_port_telemetry_v2_req(msg, buf.size(), &portIndex,
						&groupId, &sequenceToken);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(portIndex, 7);
	EXPECT_EQ(groupId, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE);
	EXPECT_EQ(sequenceToken, 0xDEADBEEFu);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryV2Req_NullParams)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint16_t portIndex = 0;
	uint8_t groupId = 0;
	uint32_t sequenceToken = 0;
	EXPECT_EQ(decode_query_port_telemetry_v2_req(nullptr, buf.size(),
						     &portIndex, &groupId,
						     &sequenceToken),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_v2_req(msg, buf.size(), nullptr,
						     &groupId, &sequenceToken),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_v2_req(
		      msg, buf.size(), &portIndex, nullptr, &sequenceToken),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_v2_req(
		      msg, buf.size(), &portIndex, &groupId, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec,
     DecodeQueryPortTelemetryV2Req_TooShort_ReturnsErrorLength)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint16_t portIndex = 0;
	uint8_t groupId = 0;
	uint32_t sequenceToken = 0;
	auto rc = decode_query_port_telemetry_v2_req(
	    msg, buf.size(), &portIndex, &groupId, &sequenceToken);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryV2Req_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_v2_req(
	    0, 1, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE, 0, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	auto *req =
	    reinterpret_cast<nsm_query_port_telemetry_v2_req *>(msg->payload);
	req->hdr.data_size = 0; // too small

	uint16_t portIndex = 0;
	uint8_t groupId = 0;
	uint32_t sequenceToken = 0;
	rc = decode_query_port_telemetry_v2_req(msg, buf.size(), &portIndex,
						&groupId, &sequenceToken);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// decode_query_port_telemetry_v2_resp (thin wrapper over decode_aggregate_resp)
// ===========================================================================

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryV2Resp_Success)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_aggregate_resp(0, NSM_QUERY_PORT_TELEMETRY_COUNTER_V2,
					NSM_SUCCESS, 5, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint16_t reasonCode = 0xFFFF;
	uint16_t telemetryCount = 0;
	size_t consumedLen = 0;
	rc = decode_query_port_telemetry_v2_resp(
	    msg, buf.size(), &cc, &reasonCode, &telemetryCount, &consumedLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(reasonCode, 0); // aggregate header has no reason_code field
	EXPECT_EQ(telemetryCount, 5);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryV2Resp_NullParams)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t cc = 0;
	uint16_t reasonCode = 0;
	uint16_t telemetryCount = 0;
	size_t consumedLen = 0;
	EXPECT_EQ(decode_query_port_telemetry_v2_resp(
		      nullptr, buf.size(), &cc, &reasonCode, &telemetryCount,
		      &consumedLen),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_v2_resp(
		      msg, buf.size(), nullptr, &reasonCode, &telemetryCount,
		      &consumedLen),
		  NSM_SW_ERROR_NULL);
}

// ===========================================================================
// encode_query_port_telemetry_v2_resp
// ===========================================================================

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Resp_NullMsg)
{
	auto rc =
	    encode_query_port_telemetry_v2_resp(0, NSM_SUCCESS, 5, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryV2Resp_StampsNetworkPortType)
{
	// Unlike the generic encode_aggregate_resp() (which hardcodes
	// NSM_TYPE_PLATFORM_ENVIRONMENTAL), cmd 0x14 belongs to the Type-1
	// Network Ports protocol, so the wire header must carry
	// NSM_TYPE_NETWORK_PORT.
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	auto rc = encode_query_port_telemetry_v2_resp(0, NSM_SUCCESS, 5, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, msg->hdr.nvidia_msg_type);
	EXPECT_EQ(NSM_RESPONSE, msg->hdr.request);

	uint8_t cc = 0;
	uint16_t reasonCode = 0xFFFF;
	uint16_t telemetryCount = 0;
	size_t consumedLen = 0;
	rc = decode_query_port_telemetry_v2_resp(
	    msg, buf.size(), &cc, &reasonCode, &telemetryCount, &consumedLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(telemetryCount, 5);
}

// ===========================================================================
// encode_query_port_telemetry_caps_req / decode_query_port_telemetry_caps_req
// ===========================================================================

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryCapsReq_NullMsg)
{
	auto rc = encode_query_port_telemetry_caps_req(0, 1, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryCapsReq_PackFail)
{
	std::vector<uint8_t> buf(4096, 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_caps_req(kBadIid, 1, msg);
	EXPECT_NE(rc, NSM_SW_SUCCESS);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsReq_RoundTrip)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_caps_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_caps_req(0, 3, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint16_t portIndex = 0;
	rc = decode_query_port_telemetry_caps_req(msg, buf.size(), &portIndex);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(portIndex, 3);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsReq_NullParams)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_caps_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint16_t portIndex = 0;
	EXPECT_EQ(decode_query_port_telemetry_caps_req(nullptr, buf.size(),
						       &portIndex),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_query_port_telemetry_caps_req(msg, buf.size(), nullptr),
	    NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsReq_TooShort)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint16_t portIndex = 0;
	auto rc =
	    decode_query_port_telemetry_caps_req(msg, buf.size(), &portIndex);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsReq_DataSizeTooSmall)
{
	std::vector<uint8_t> buf(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_caps_req), 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_caps_req(0, 1, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	auto *req =
	    reinterpret_cast<nsm_query_port_telemetry_caps_req *>(msg->payload);
	req->hdr.data_size = 0;

	uint16_t portIndex = 0;
	rc = decode_query_port_telemetry_caps_req(msg, buf.size(), &portIndex);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// ===========================================================================
// encode_query_port_telemetry_caps_resp /
// decode_query_port_telemetry_caps_resp
// ===========================================================================

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryCapsResp_NullMsg)
{
	uint8_t bitmask[32] = {};
	auto rc = encode_query_port_telemetry_caps_resp(
	    0, NSM_SUCCESS, ERR_NULL, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE,
	    32, bitmask, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, EncodeQueryPortTelemetryCapsResp_NullBitmask)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(nsm_query_port_telemetry_caps_resp),
				 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	auto rc = encode_query_port_telemetry_caps_resp(
	    0, NSM_SUCCESS, ERR_NULL, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE,
	    32, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, QueryPortTelemetryCapsResp_RoundTrip_FullBitmask)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(nsm_query_port_telemetry_caps_resp),
				 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	// groups 1-8 -> byte 0 = 0xFF; group 9 (Optical Module) -> byte 1 bit
	// 0.
	uint8_t bitmask[32] = {};
	bitmask[0] = 0xFF;
	bitmask[1] = 0x01;

	auto rc = encode_query_port_telemetry_caps_resp(
	    0, NSM_SUCCESS, ERR_NULL, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE,
	    32, bitmask, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint8_t maxSupportedGroupId = 0;
	uint8_t maxCounterRecordsPerResp = 0;
	uint8_t decodedBitmask[32] = {};
	rc = decode_query_port_telemetry_caps_resp(
	    msg, buf.size(), &cc, &maxSupportedGroupId,
	    &maxCounterRecordsPerResp, decodedBitmask);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(maxSupportedGroupId, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE);
	EXPECT_EQ(maxCounterRecordsPerResp, 32);
	// Full 32-byte round trip, not just the first byte -- this is the
	// regression check for the single-byte -> NvU8[32] widening.
	EXPECT_EQ(0, std::memcmp(decodedBitmask, bitmask, sizeof(bitmask)));
	EXPECT_EQ(decodedBitmask[1] & 0x01, 0x01); // group 9 bit set
}

TEST(OpticalModuleCodec, QueryPortTelemetryCapsResp_ErrorCc_EncodesReasonCode)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(nsm_query_port_telemetry_caps_resp),
				 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t bitmask[32] = {};

	auto rc = encode_query_port_telemetry_caps_resp(0, NSM_ERROR, ERR_NULL,
							0, 0, bitmask, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint8_t maxSupportedGroupId = 0xFF;
	uint8_t maxCounterRecordsPerResp = 0xFF;
	uint8_t decodedBitmask[32] = {};
	rc = decode_query_port_telemetry_caps_resp(
	    msg, buf.size(), &cc, &maxSupportedGroupId,
	    &maxCounterRecordsPerResp, decodedBitmask);
	// decode_query_port_telemetry_caps_resp returns early on cc !=
	// NSM_SUCCESS without touching the out params beyond cc.
	EXPECT_EQ(cc, NSM_ERROR);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsResp_NullParams)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(nsm_query_port_telemetry_caps_resp),
				 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	uint8_t bitmask[32] = {};
	auto rc = encode_query_port_telemetry_caps_resp(
	    0, NSM_SUCCESS, ERR_NULL, 1, 1, bitmask, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = 0;
	uint8_t a = 0;
	uint8_t b = 0;
	uint8_t out[32] = {};
	EXPECT_EQ(decode_query_port_telemetry_caps_resp(nullptr, buf.size(),
							&cc, &a, &b, out),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_caps_resp(msg, buf.size(),
							nullptr, &a, &b, out),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_caps_resp(msg, buf.size(), &cc,
							nullptr, &b, out),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_caps_resp(msg, buf.size(), &cc,
							&a, nullptr, out),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_telemetry_caps_resp(msg, buf.size(), &cc,
							&a, &b, nullptr),
		  NSM_SW_ERROR_NULL);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsResp_TooShort)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp),
				 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	// Build a minimal well-formed common-resp header with cc = NSM_SUCCESS
	// so decode_reason_code_and_cc succeeds and the length check is what
	// actually gets exercised.
	auto *resp = reinterpret_cast<nsm_common_resp *>(msg->payload);
	resp->command = NSM_QUERY_PORT_TELEMETRY_CAPABILITIES;
	resp->completion_code = NSM_SUCCESS;
	resp->data_size = 0;

	uint8_t cc = 0;
	uint8_t a = 0;
	uint8_t b = 0;
	uint8_t out[32] = {};
	auto rc = decode_query_port_telemetry_caps_resp(msg, buf.size(), &cc,
							&a, &b, out);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(OpticalModuleCodec, DecodeQueryPortTelemetryCapsResp_DataSizeMismatch)
{
	std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) +
				     sizeof(nsm_query_port_telemetry_caps_resp),
				 0);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	uint8_t bitmask[32] = {};
	auto rc = encode_query_port_telemetry_caps_resp(
	    0, NSM_SUCCESS, ERR_NULL, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE,
	    32, bitmask, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	// A buffer that is physically long enough (msg_len check passes) but
	// whose declared data_size does not match the response payload must
	// still be rejected.
	auto *resp = reinterpret_cast<nsm_query_port_telemetry_caps_resp *>(
	    msg->payload);
	resp->hdr.data_size = 0;

	uint8_t cc = 0;
	uint8_t a = 0;
	uint8_t b = 0;
	uint8_t out[32] = {};
	rc = decode_query_port_telemetry_caps_resp(msg, buf.size(), &cc, &a, &b,
						   out);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
