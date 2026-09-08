// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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

// Unit tests for the Link Health Indication Extension codecs of libnsm:
// Query Port Characteristics v2 (0x12) request / aggregate response header,
// the u32 and Link Health (Tag 0x04) record helpers, and Clear Port Metric
// State (0x13) request / response, including the 0x13 data_size erratum
// (6 + Count on the wire, 4 + Count tolerated by the decoder).

#include "base.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace
{

// Wire bit layout of the Link Health record (struct nsm_link_health_record).
static constexpr uint32_t linkHealthWord(uint32_t health, uint32_t trigger,
					 uint32_t metric, uint32_t config)
{
	return (health & 0x0F) | ((trigger & 0xFF) << 5) |
	       ((metric & 0x7F) << 13) | ((config & 0x03) << 20);
}

static std::vector<uint8_t> le32Bytes(uint32_t value)
{
	return {static_cast<uint8_t>(value & 0xFF),
		static_cast<uint8_t>((value >> 8) & 0xFF),
		static_cast<uint8_t>((value >> 16) & 0xFF),
		static_cast<uint8_t>((value >> 24) & 0xFF)};
}

} // namespace

// ===========================================================================
// Query Port Characteristics v2 (0x12) request
// ===========================================================================

TEST(queryPortCharacteristicsV2, testGoodEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_port_characteristics_v2_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc = encode_query_port_characteristics_v2_req(0, 0x0203, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(0, request->hdr.datagram);
	EXPECT_EQ(0, request->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, request->hdr.nvidia_msg_type);

	// command, data_size, port_number (LE), reserved
	const std::vector<uint8_t> expectedPayload{
	    NSM_QUERY_PORT_CHARACTERISTICS_V2, 0x04, 0x03, 0x02, 0x00, 0x00};
	EXPECT_EQ(std::vector<uint8_t>(requestMsg.begin() + sizeof(nsm_msg_hdr),
				       requestMsg.end()),
		  expectedPayload);
}

TEST(queryPortCharacteristicsV2, testBadEncodeRequest)
{
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) +
	    sizeof(nsm_query_port_characteristics_v2_req));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	EXPECT_EQ(encode_query_port_characteristics_v2_req(0, 1, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_query_port_characteristics_v2_req(NSM_INSTANCE_MAX + 1,
							   1, request),
		  NSM_SW_ERROR_DATA);
}

TEST(queryPortCharacteristicsV2, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,
	    0x80,
	    0x89,
	    NSM_TYPE_NETWORK_PORT,	       // NVIDIA_MSG_TYPE
	    NSM_QUERY_PORT_CHARACTERISTICS_V2, // command
	    0x04,			       // data size
	    0x07,
	    0x01, // port number 0x0107
	    0x00,
	    0x00 // reserved
	};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint16_t portNumber = 0;
	auto rc = decode_query_port_characteristics_v2_req(
	    request, requestMsg.size(), &portNumber);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(portNumber, 0x0107);
}

TEST(queryPortCharacteristicsV2, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{0x10,
					0xDE,
					0x80,
					0x89,
					NSM_TYPE_NETWORK_PORT,
					NSM_QUERY_PORT_CHARACTERISTICS_V2,
					0x00, // data size [should be 4]
					0x01,
					0x00,
					0x00,
					0x00};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	size_t msgLen = requestMsg.size();
	uint16_t portNumber = 0;

	EXPECT_EQ(decode_query_port_characteristics_v2_req(nullptr, msgLen,
							   &portNumber),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    decode_query_port_characteristics_v2_req(request, msgLen, nullptr),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_query_port_characteristics_v2_req(request, msgLen - 1,
							   &portNumber),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(decode_query_port_characteristics_v2_req(request, msgLen,
							   &portNumber),
		  NSM_SW_ERROR_DATA);
}

// ===========================================================================
// Query Port Characteristics v2 (0x12) aggregate response header
// ===========================================================================

TEST(queryPortCharacteristicsV2, testGoodEncodeDecodeResponseHeader)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_aggregate_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_query_port_characteristics_v2_resp(
	    3, NSM_SUCCESS, ERR_NULL, 0x0105, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(3, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);

	const std::vector<uint8_t> expectedPayload{
	    NSM_QUERY_PORT_CHARACTERISTICS_V2, NSM_SUCCESS, 0x05, 0x01};
	EXPECT_EQ(
	    std::vector<uint8_t>(responseMsg.begin() + sizeof(nsm_msg_hdr),
				 responseMsg.end()),
	    expectedPayload);

	uint8_t cc = NSM_ERROR;
	uint16_t telemetryCount = 0;
	size_t consumedLen = 0;
	rc = decode_aggregate_resp(response, responseMsg.size(), &consumedLen,
				   &cc, &telemetryCount);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
	EXPECT_EQ(telemetryCount, 0x0105);
	EXPECT_EQ(consumedLen, responseMsg.size());
}

TEST(queryPortCharacteristicsV2, testEncodeResponseErrorCc)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	EXPECT_EQ(encode_query_port_characteristics_v2_resp(
		      0, NSM_SUCCESS, ERR_NULL, 0, nullptr),
		  NSM_SW_ERROR_NULL);

	auto rc = encode_query_port_characteristics_v2_resp(
	    0, NSM_ERR_UNSUPPORTED_COMMAND_CODE, ERR_NOT_SUPPORTED, 0,
	    response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Reason-code layout: command, cc, reason code (LE)
	const std::vector<uint8_t> expectedPayload{
	    NSM_QUERY_PORT_CHARACTERISTICS_V2, NSM_ERR_UNSUPPORTED_COMMAND_CODE,
	    ERR_NOT_SUPPORTED, 0x00};
	EXPECT_EQ(
	    std::vector<uint8_t>(responseMsg.begin() + sizeof(nsm_msg_hdr),
				 responseMsg.end()),
	    expectedPayload);

	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	rc = decode_reason_code_and_cc(response, responseMsg.size(), &cc,
				       &reasonCode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERR_UNSUPPORTED_COMMAND_CODE);
	EXPECT_EQ(reasonCode, ERR_NOT_SUPPORTED);
}

// ===========================================================================
// u32 record helpers (Tags 0x00-0x03)
// ===========================================================================

TEST(portCharacteristicsV2U32Record, testGoodEncodeDecode)
{
	uint8_t data[sizeof(uint32_t)]{};
	size_t dataLen = 0;

	auto rc = encode_port_characteristics_v2_u32_record(0x11223344, data,
							    &dataLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));
	EXPECT_EQ(std::vector<uint8_t>(data, data + dataLen),
		  le32Bytes(0x11223344));

	uint32_t value = 0;
	rc = decode_port_characteristics_v2_u32_record(data, dataLen, &value);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(value, 0x11223344u);
}

TEST(portCharacteristicsV2U32Record, testBadEncodeDecode)
{
	uint8_t data[8]{};
	size_t dataLen = 0;
	uint32_t value = 0;

	EXPECT_EQ(
	    encode_port_characteristics_v2_u32_record(1, nullptr, &dataLen),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_port_characteristics_v2_u32_record(1, data, nullptr),
		  NSM_SW_ERROR_NULL);

	EXPECT_EQ(decode_port_characteristics_v2_u32_record(nullptr, 4, &value),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_port_characteristics_v2_u32_record(data, 4, nullptr),
		  NSM_SW_ERROR_NULL);
	// The record is exactly one u32; both a short and a long payload are
	// rejected rather than truncated.
	EXPECT_EQ(decode_port_characteristics_v2_u32_record(data, 3, &value),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(decode_port_characteristics_v2_u32_record(data, 8, &value),
		  NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Link Health record (Tag 0x04)
// ===========================================================================

TEST(linkHealthRecord, testGoodEncodeBitPlacement)
{
	struct nsm_link_health_record record{};
	record.link_health = NSM_LINK_HEALTH_HEALTHY;		     // [3:0]
	record.attention_trigger = NSM_ATTENTION_TRIGGER_SYMBOL_BER; // [12:5]
	record.attention_trigger_metric =
	    NSM_LINK_HEALTH_METRIC_SLOT_MAX; // [19:13]
	record.link_health_config_changed =
	    NSM_LINK_HEALTH_CONFIG_PREVIOUS; // [21:20]

	uint8_t data[sizeof(uint32_t)]{};
	size_t dataLen = 0;
	auto rc = encode_link_health_record(&record, data, &dataLen);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(dataLen, sizeof(uint32_t));

	const uint32_t expected = linkHealthWord(2, 9, 15, 2);
	EXPECT_EQ(expected, 0x0021E122u);
	EXPECT_EQ(std::vector<uint8_t>(data, data + dataLen),
		  le32Bytes(expected));
}

TEST(linkHealthRecord, testGoodDecodeBitPlacement)
{
	// Reserved bit 4 and bits 31:22 set: they must not leak into any field.
	const uint32_t word =
	    linkHealthWord(NSM_LINK_HEALTH_ATTENTION,
			   NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 1,
			   NSM_LINK_HEALTH_CONFIG_CURRENT) |
	    (1u << 4) | (0x3FFu << 22);
	auto data = le32Bytes(word);

	struct nsm_link_health_record record{};
	auto rc = decode_link_health_record(data.data(), data.size(), &record);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(record.link_health, NSM_LINK_HEALTH_ATTENTION);
	EXPECT_EQ(record.attention_trigger,
		  NSM_ATTENTION_TRIGGER_EFFECTIVE_BER);
	EXPECT_EQ(record.attention_trigger_metric, 1);
	EXPECT_EQ(record.link_health_config_changed,
		  NSM_LINK_HEALTH_CONFIG_CURRENT);
}

TEST(linkHealthRecord, testEncodeMaskTruncation)
{
	// Out-of-range field values are truncated to the field width rather
	// than spilling into the neighboring field.
	struct nsm_link_health_record record{};
	record.link_health = 0x1F;		  // 4-bit field -> 0x0F
	record.attention_trigger = 0xFF;	  // 8-bit field, unchanged
	record.attention_trigger_metric = 0xFF;	  // 7-bit field -> 0x7F
	record.link_health_config_changed = 0x07; // 2-bit field -> 0x03

	uint8_t data[sizeof(uint32_t)]{};
	size_t dataLen = 0;
	EXPECT_EQ(encode_link_health_record(&record, data, &dataLen),
		  NSM_SW_SUCCESS);

	uint32_t value = 0;
	EXPECT_EQ(
	    decode_port_characteristics_v2_u32_record(data, dataLen, &value),
	    NSM_SW_SUCCESS);
	EXPECT_EQ(value, linkHealthWord(0x0F, 0xFF, 0x7F, 0x03));

	struct nsm_link_health_record decoded{};
	EXPECT_EQ(decode_link_health_record(data, dataLen, &decoded),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(decoded.link_health, 0x0F);
	EXPECT_EQ(decoded.attention_trigger, 0xFF);
	EXPECT_EQ(decoded.attention_trigger_metric, 0x7F);
	EXPECT_EQ(decoded.link_health_config_changed, 0x03);
}

TEST(linkHealthRecord, testBadEncodeDecode)
{
	struct nsm_link_health_record record{};
	uint8_t data[8]{};
	size_t dataLen = 0;

	EXPECT_EQ(encode_link_health_record(nullptr, data, &dataLen),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_link_health_record(&record, nullptr, &dataLen),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_link_health_record(&record, data, nullptr),
		  NSM_SW_ERROR_NULL);

	EXPECT_EQ(decode_link_health_record(data, 4, nullptr),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_link_health_record(nullptr, 4, &record),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_link_health_record(data, 3, &record),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(decode_link_health_record(data, 8, &record),
		  NSM_SW_ERROR_LENGTH);
}

// ===========================================================================
// Clear Port Metric State (0x13) request
// ===========================================================================

TEST(clearPortMetricState, testGoodEncodeRequest)
{
	const uint8_t tagIds[] = {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH};
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_req) +
	    sizeof(tagIds));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	auto rc =
	    encode_clear_port_metric_state_req(0, 0x0007, 1, tagIds, request);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(1, request->hdr.request);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, request->hdr.nvidia_msg_type);

	// command, data_size = 6 fixed bytes + Count, port (LE), reserved,
	// Count (LE), TagID list
	const std::vector<uint8_t> expectedPayload{
	    NSM_CLEAR_PORT_METRIC_STATE,
	    0x07,
	    0x07,
	    0x00,
	    0x00,
	    0x00,
	    0x01,
	    0x00,
	    NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH};
	EXPECT_EQ(std::vector<uint8_t>(requestMsg.begin() + sizeof(nsm_msg_hdr),
				       requestMsg.end()),
		  expectedPayload);
}

TEST(clearPortMetricState, testEncodeRequestBounds)
{
	// data_size is one byte and the fixed fields take six of it, so Count
	// is limited to 1..249.
	constexpr uint16_t maxCount =
	    UINT8_MAX - NSM_CLEAR_PORT_METRIC_STATE_REQ_FIXED_DATA_SIZE;
	std::vector<uint8_t> tagIds(
	    maxCount + 1, NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH);
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_req) +
	    tagIds.size());
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	EXPECT_EQ(
	    encode_clear_port_metric_state_req(0, 1, 1, tagIds.data(), nullptr),
	    NSM_SW_ERROR_NULL);
	EXPECT_EQ(encode_clear_port_metric_state_req(0, 1, 1, nullptr, request),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(
	    encode_clear_port_metric_state_req(0, 1, 0, tagIds.data(), request),
	    NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_clear_port_metric_state_req(0, 1, maxCount + 1,
						     tagIds.data(), request),
		  NSM_SW_ERROR_DATA);

	EXPECT_EQ(encode_clear_port_metric_state_req(0, 1, maxCount,
						     tagIds.data(), request),
		  NSM_SW_SUCCESS);
	auto fixed = reinterpret_cast<nsm_clear_port_metric_state_req *>(
	    request->payload);
	EXPECT_EQ(fixed->hdr.data_size, UINT8_MAX);
	EXPECT_EQ(le16toh(fixed->tag_count), maxCount);
}

TEST(clearPortMetricState, testEncodeRequestInstanceIdOutOfRange)
{
	// The request encoder hands the instance id to the header packer
	// unmasked, so an out-of-range id is rejected rather than truncated.
	const uint8_t tagIds[] = {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH};
	std::vector<uint8_t> requestMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_req) +
	    sizeof(tagIds));
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	EXPECT_EQ(encode_clear_port_metric_state_req(NSM_INSTANCE_MAX + 1, 1, 1,
						     tagIds, request),
		  NSM_SW_ERROR_DATA);
	EXPECT_EQ(encode_clear_port_metric_state_req(NSM_INSTANCE_MAX, 1, 1,
						     tagIds, request),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(request->hdr.instance_id, NSM_INSTANCE_MAX);
}

TEST(clearPortMetricState, testGoodDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,
	    0x80,
	    0x89,
	    NSM_TYPE_NETWORK_PORT,
	    NSM_CLEAR_PORT_METRIC_STATE,
	    0x08, // data size = 6 + 2
	    0x02,
	    0x00, // port number 2
	    0x00,
	    0x00, // reserved
	    0x02,
	    0x00, // Count 2
	    NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH,
	    NSM_PORT_CHARACTERISTICS_V2_TAG_PORT_STATUS};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());

	uint16_t portNumber = 0;
	uint16_t tagCount = 0;
	const uint8_t *tagIds = nullptr;
	auto rc = decode_clear_port_metric_state_req(
	    request, requestMsg.size(), &portNumber, &tagCount, &tagIds);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(portNumber, 2);
	EXPECT_EQ(tagCount, 2);
	ASSERT_NE(tagIds, nullptr);
	// The TagID list is returned in place, right after the fixed fields.
	EXPECT_EQ(tagIds,
		  request->payload + sizeof(nsm_clear_port_metric_state_req));
	EXPECT_EQ(tagIds[0], NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH);
	EXPECT_EQ(tagIds[1], NSM_PORT_CHARACTERISTICS_V2_TAG_PORT_STATUS);
}

TEST(clearPortMetricState, testDecodeRequestDataSizeErratum)
{
	// One TagID: the repo convention writes data_size 6 + 1; the literal
	// spec text gives 4 + 1. Both are accepted, anything else is rejected.
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,
	    0x80,
	    0x89,
	    NSM_TYPE_NETWORK_PORT,
	    NSM_CLEAR_PORT_METRIC_STATE,
	    0x07, // data size, patched below
	    0x01,
	    0x00,
	    0x00,
	    0x00,
	    0x01,
	    0x00,
	    NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH};
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto fixed = reinterpret_cast<nsm_clear_port_metric_state_req *>(
	    request->payload);
	uint16_t portNumber = 0;
	uint16_t tagCount = 0;
	const uint8_t *tagIds = nullptr;

	fixed->hdr.data_size =
	    NSM_CLEAR_PORT_METRIC_STATE_REQ_FIXED_DATA_SIZE + 1;
	EXPECT_EQ(decode_clear_port_metric_state_req(request, requestMsg.size(),
						     &portNumber, &tagCount,
						     &tagIds),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(tagCount, 1);

	fixed->hdr.data_size =
	    NSM_CLEAR_PORT_METRIC_STATE_REQ_FIXED_DATA_SIZE - 2 + 1;
	EXPECT_EQ(decode_clear_port_metric_state_req(request, requestMsg.size(),
						     &portNumber, &tagCount,
						     &tagIds),
		  NSM_SW_SUCCESS);
	EXPECT_EQ(tagCount, 1);

	fixed->hdr.data_size = NSM_CLEAR_PORT_METRIC_STATE_REQ_FIXED_DATA_SIZE;
	EXPECT_EQ(decode_clear_port_metric_state_req(request, requestMsg.size(),
						     &portNumber, &tagCount,
						     &tagIds),
		  NSM_SW_ERROR_DATA);
}

TEST(clearPortMetricState, testBadDecodeRequest)
{
	std::vector<uint8_t> requestMsg{
	    0x10,
	    0xDE,
	    0x80,
	    0x89,
	    NSM_TYPE_NETWORK_PORT,
	    NSM_CLEAR_PORT_METRIC_STATE,
	    0x08, // data size = 6 + 2
	    0x01,
	    0x00,
	    0x00,
	    0x00,
	    0x02,
	    0x00, // Count 2 ...
	    NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH};
	// ... but only one TagID byte present
	auto request = reinterpret_cast<nsm_msg *>(requestMsg.data());
	auto fixed = reinterpret_cast<nsm_clear_port_metric_state_req *>(
	    request->payload);
	size_t msgLen = requestMsg.size();
	uint16_t portNumber = 0;
	uint16_t tagCount = 0;
	const uint8_t *tagIds = nullptr;

	EXPECT_EQ(decode_clear_port_metric_state_req(
		      nullptr, msgLen, &portNumber, &tagCount, &tagIds),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_clear_port_metric_state_req(request, msgLen, nullptr,
						     &tagCount, &tagIds),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_clear_port_metric_state_req(
		      request, msgLen, &portNumber, nullptr, &tagIds),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_clear_port_metric_state_req(
		      request, msgLen, &portNumber, &tagCount, nullptr),
		  NSM_SW_ERROR_NULL);

	// Shorter than the fixed part
	EXPECT_EQ(decode_clear_port_metric_state_req(
		      request,
		      sizeof(nsm_msg_hdr) +
			  sizeof(nsm_clear_port_metric_state_req) - 1,
		      &portNumber, &tagCount, &tagIds),
		  NSM_SW_ERROR_LENGTH);

	// Count 2 with one TagID in the message
	EXPECT_EQ(decode_clear_port_metric_state_req(
		      request, msgLen, &portNumber, &tagCount, &tagIds),
		  NSM_SW_ERROR_LENGTH);

	// Count 0 is never valid
	fixed->tag_count = 0;
	fixed->hdr.data_size = NSM_CLEAR_PORT_METRIC_STATE_REQ_FIXED_DATA_SIZE;
	EXPECT_EQ(decode_clear_port_metric_state_req(
		      request, msgLen, &portNumber, &tagCount, &tagIds),
		  NSM_SW_ERROR_DATA);
}

// ===========================================================================
// Clear Port Metric State (0x13) response
// ===========================================================================

TEST(clearPortMetricState, testGoodEncodeDecodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	EXPECT_EQ(encode_clear_port_metric_state_resp(0, NSM_SUCCESS, ERR_NULL,
						      nullptr),
		  NSM_SW_ERROR_NULL);

	auto rc = encode_clear_port_metric_state_resp(2, NSM_SUCCESS, ERR_NULL,
						      response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(0, response->hdr.request);
	EXPECT_EQ(2, response->hdr.instance_id);
	EXPECT_EQ(NSM_TYPE_NETWORK_PORT, response->hdr.nvidia_msg_type);

	// command, cc, reserved, data_size = 0
	const std::vector<uint8_t> expectedPayload{
	    NSM_CLEAR_PORT_METRIC_STATE, NSM_SUCCESS, 0x00, 0x00, 0x00, 0x00};
	EXPECT_EQ(
	    std::vector<uint8_t>(responseMsg.begin() + sizeof(nsm_msg_hdr),
				 responseMsg.end()),
	    expectedPayload);

	uint8_t cc = NSM_ERROR;
	uint16_t reasonCode = ERR_NULL;
	rc = decode_clear_port_metric_state_resp(response, responseMsg.size(),
						 &cc, &reasonCode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_SUCCESS);
}

TEST(clearPortMetricState, testDecodeResponseErrorCc)
{
	std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
					 sizeof(nsm_common_non_success_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());

	auto rc = encode_clear_port_metric_state_resp(
	    0, NSM_ERR_INVALID_DATA, ERR_NVLINK_PORT_INVALID, response);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;
	rc = decode_clear_port_metric_state_resp(response, responseMsg.size(),
						 &cc, &reasonCode);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);
	EXPECT_EQ(cc, NSM_ERR_INVALID_DATA);
	EXPECT_EQ(reasonCode, ERR_NVLINK_PORT_INVALID);
}

TEST(clearPortMetricState, testBadDecodeResponse)
{
	std::vector<uint8_t> responseMsg(
	    sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_resp));
	auto response = reinterpret_cast<nsm_msg *>(responseMsg.data());
	EXPECT_EQ(encode_clear_port_metric_state_resp(0, NSM_SUCCESS, ERR_NULL,
						      response),
		  NSM_SW_SUCCESS);
	uint8_t cc = NSM_SUCCESS;
	uint16_t reasonCode = ERR_NULL;

	EXPECT_EQ(decode_clear_port_metric_state_resp(
		      nullptr, responseMsg.size(), &cc, &reasonCode),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_clear_port_metric_state_resp(
		      response, responseMsg.size(), nullptr, &reasonCode),
		  NSM_SW_ERROR_NULL);
	EXPECT_EQ(decode_clear_port_metric_state_resp(
		      response, responseMsg.size(), &cc, nullptr),
		  NSM_SW_ERROR_NULL);

	// Too short for the completion code, and too short for the full
	// success response
	EXPECT_EQ(decode_clear_port_metric_state_resp(
		      response, sizeof(nsm_msg_hdr) + 1, &cc, &reasonCode),
		  NSM_SW_ERROR_LENGTH);
	EXPECT_EQ(decode_clear_port_metric_state_resp(
		      response, responseMsg.size() - 1, &cc, &reasonCode),
		  NSM_SW_ERROR_LENGTH);

	// A success response carries no data
	auto common = reinterpret_cast<nsm_common_resp *>(response->payload);
	common->data_size = htole16(1);
	EXPECT_EQ(decode_clear_port_metric_state_resp(
		      response, responseMsg.size(), &cc, &reasonCode),
		  NSM_SW_ERROR_DATA);
}
