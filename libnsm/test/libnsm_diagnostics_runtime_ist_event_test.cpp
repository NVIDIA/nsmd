/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Tests for encode_nsm_runtime_ist_complete_event /
 * decode_nsm_runtime_ist_complete_event (libnsm/diagnostics.{h,c}).
 *
 * The Runtime IST Complete v1 event is fixed-length, so coverage focuses on:
 *   - happy-path encode (header bits, version v1, event id/class, fields)
 *   - happy-path decode (round-trip, including negative NvS24.8 temperatures
 *     and a non-zero event_state parameter)
 *   - little-endian wire byte order of multi-byte fields
 *   - all NULL pointer error paths
 *   - too-short message length
 *   - data_size inconsistency vs msg_len
 *   - data_size != sizeof(payload) (wrong-sized "RIST" frame)
 */

#include "base.h"
#include "diagnostics.h"

#include <cstring>
#include <gtest/gtest.h>
#include <vector>

namespace
{

constexpr size_t kRistMsgLen = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
			       sizeof(nsm_runtime_ist_complete_event_payload);

nsm_runtime_ist_complete_event_payload makeSamplePayload()
{
	nsm_runtime_ist_complete_event_payload p{};
	// Device UUID and app version: short ASCII strings; remainder
	// zero-padded.
	std::strncpy(p.gpu_identifier, "GPU-12345-abcde",
		     NSM_RIST_GPU_UUID_LEN - 1);
	std::strncpy(p.app_version, "rist-1.2.3", NSM_RIST_APP_VERSION_LEN - 1);

	p.timestamp = 0x0123456789ABCDEFULL;
	p.result = 1; // Pass per spec
	p.status_code = 0xDEADBEEFCAFEBABEULL;
	// NvS24.8: 100.00 C => 100 << 8 = 0x6400
	p.max_temperature = static_cast<int32_t>(100 * (1 << 8));
	// 75.5 C => 75.5 * 256 = 19328 = 0x4B80
	p.avg_temperature = static_cast<int32_t>(75.5 * (1 << 8));
	return p;
}

} // namespace

// -------- encode happy path ----------------------------------------------

TEST(RuntimeISTCompleteEvent, EncodeHappyPath)
{
	std::vector<uint8_t> buf(kRistMsgLen);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	const auto payload = makeSamplePayload();
	const uint16_t event_state = 0; // spec value

	int rc = encode_nsm_runtime_ist_complete_event(
	    /*instance_id=*/3,
	    /*ackr=*/true, event_state, &payload, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	// Header sanity: request flag and datagram bit set for events.
	EXPECT_EQ(msg->hdr.request, 1);
	EXPECT_EQ(msg->hdr.datagram, 1);
	EXPECT_EQ(msg->hdr.nvidia_msg_type, NSM_TYPE_DIAGNOSTIC);

	auto *event = reinterpret_cast<nsm_event *>(msg->payload);
	EXPECT_EQ(event->version, NSM_EVENT_VERSION_V1);
	EXPECT_EQ(event->ackr, 1u);
	EXPECT_EQ(event->event_id, NSM_RUNTIME_IST_COMPLETE_EVENT);
	EXPECT_EQ(event->event_class, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(event->event_state, 0);
	EXPECT_EQ(event->data_size,
		  sizeof(nsm_runtime_ist_complete_event_payload));
}

TEST(RuntimeISTCompleteEvent, EncodePassesThroughEventState)
{
	std::vector<uint8_t> buf(kRistMsgLen);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	const auto payload = makeSamplePayload();
	const uint16_t event_state = 0xBEEF;

	int rc = encode_nsm_runtime_ist_complete_event(0, false, event_state,
						       &payload, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *event = reinterpret_cast<nsm_event *>(msg->payload);
	// event_state on the wire is little-endian; on LE hosts the field reads
	// back as-is via direct struct access.
	EXPECT_EQ(le16toh(event->event_state), event_state);
}

TEST(RuntimeISTCompleteEvent, EncodeAckrFalseClearsAckrBit)
{
	std::vector<uint8_t> buf(kRistMsgLen);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());
	const auto payload = makeSamplePayload();

	int rc =
	    encode_nsm_runtime_ist_complete_event(0, false, 0, &payload, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	auto *event = reinterpret_cast<nsm_event *>(msg->payload);
	EXPECT_EQ(event->ackr, 0u);
}

TEST(RuntimeISTCompleteEvent, EncodeDoesNotMutateCallerPayload)
{
	std::vector<uint8_t> buf(kRistMsgLen);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	nsm_runtime_ist_complete_event_payload payload = makeSamplePayload();
	const auto original = payload;

	int rc =
	    encode_nsm_runtime_ist_complete_event(0, true, 0, &payload, msg);
	EXPECT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(payload.timestamp, original.timestamp);
	EXPECT_EQ(payload.status_code, original.status_code);
	EXPECT_EQ(payload.max_temperature, original.max_temperature);
	EXPECT_EQ(payload.avg_temperature, original.avg_temperature);
	EXPECT_EQ(0,
		  std::memcmp(payload.gpu_identifier, original.gpu_identifier,
			      NSM_RIST_GPU_UUID_LEN));
	EXPECT_EQ(0, std::memcmp(payload.app_version, original.app_version,
				 NSM_RIST_APP_VERSION_LEN));
}

TEST(RuntimeISTCompleteEvent, EncodeWritesLittleEndianWireBytes)
{
	std::vector<uint8_t> buf(kRistMsgLen);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	nsm_runtime_ist_complete_event_payload payload{};
	payload.timestamp = 0x0011223344556677ULL;
	payload.status_code = 0x8899AABBCCDDEEFFULL;
	payload.max_temperature = static_cast<int32_t>(0x12345678);
	payload.avg_temperature = static_cast<int32_t>(0x7FFFFFFF);
	payload.result = 1;

	int rc =
	    encode_nsm_runtime_ist_complete_event(0, true, 0, &payload, msg);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	auto *event = reinterpret_cast<nsm_event *>(msg->payload);
	auto *wire =
	    reinterpret_cast<const nsm_runtime_ist_complete_event_payload *>(
		event->data);

	// Read the raw little-endian bytes from the wire field, convert with
	// le*toh, and confirm we recover the host values. This is the most
	// portable way to assert byte order without making assumptions about
	// host endianness.
	EXPECT_EQ(le64toh(wire->timestamp), 0x0011223344556677ULL);
	EXPECT_EQ(le64toh(wire->status_code), 0x8899AABBCCDDEEFFULL);
	EXPECT_EQ(static_cast<int32_t>(
		      le32toh(static_cast<uint32_t>(wire->max_temperature))),
		  0x12345678);
	EXPECT_EQ(static_cast<int32_t>(
		      le32toh(static_cast<uint32_t>(wire->avg_temperature))),
		  0x7FFFFFFF);
}

// -------- encode error paths ---------------------------------------------

TEST(RuntimeISTCompleteEvent, EncodeNullPayload)
{
	std::vector<uint8_t> buf(kRistMsgLen);
	auto *msg = reinterpret_cast<nsm_msg *>(buf.data());

	int rc =
	    encode_nsm_runtime_ist_complete_event(0, true, 0, nullptr, msg);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST(RuntimeISTCompleteEvent, EncodeNullMsg)
{
	const auto payload = makeSamplePayload();

	int rc = encode_nsm_runtime_ist_complete_event(0, true, 0, &payload,
						       nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

// -------- decode happy paths --------------------------------------------

class RuntimeISTCompleteEventDecode : public testing::Test
{
      protected:
	RuntimeISTCompleteEventDecode() : event_msg(kRistMsgLen) {}

	void encodePayload(const nsm_runtime_ist_complete_event_payload &p,
			   uint16_t event_state = 0)
	{
		auto rc = encode_nsm_runtime_ist_complete_event(
		    0, true, event_state, &p,
		    reinterpret_cast<nsm_msg *>(event_msg.data()));
		ASSERT_EQ(rc, NSM_SW_SUCCESS);
	}

	nsm_msg *msg() { return reinterpret_cast<nsm_msg *>(event_msg.data()); }

	std::vector<uint8_t> event_msg;
};

TEST_F(RuntimeISTCompleteEventDecode, RoundTripPass)
{
	const auto encoded_payload = makeSamplePayload();
	encodePayload(encoded_payload, /*event_state=*/0);

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), &event_class, &event_state, &decoded);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(event_class, NSM_GENERAL_EVENT_CLASS);
	EXPECT_EQ(event_state, 0);

	EXPECT_EQ(decoded.timestamp, encoded_payload.timestamp);
	EXPECT_EQ(decoded.status_code, encoded_payload.status_code);
	EXPECT_EQ(decoded.result, encoded_payload.result);
	EXPECT_EQ(decoded.max_temperature, encoded_payload.max_temperature);
	EXPECT_EQ(decoded.avg_temperature, encoded_payload.avg_temperature);
	EXPECT_EQ(0, std::memcmp(decoded.gpu_identifier,
				 encoded_payload.gpu_identifier,
				 NSM_RIST_GPU_UUID_LEN));
	EXPECT_EQ(0,
		  std::memcmp(decoded.app_version, encoded_payload.app_version,
			      NSM_RIST_APP_VERSION_LEN));
}

TEST_F(RuntimeISTCompleteEventDecode, RoundTripFailWithNonZeroEventState)
{
	auto encoded_payload = makeSamplePayload();
	encoded_payload.result = 0; // Fail
	encoded_payload.status_code = 0x42;

	encodePayload(encoded_payload, /*event_state=*/0xCAFE);

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), &event_class, &event_state, &decoded);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(event_state, 0xCAFE);
	EXPECT_EQ(decoded.result, 0u);
	EXPECT_EQ(decoded.status_code, 0x42u);
}

TEST_F(RuntimeISTCompleteEventDecode, RoundTripNegativeTemperatures)
{
	auto encoded_payload = makeSamplePayload();
	// -25.5 C in NvS24.8: -25.5 * 256 = -6528 = 0xFFFFE680 (two's
	// complement).
	encoded_payload.max_temperature =
	    static_cast<int32_t>(-25.5 * (1 << 8));
	encoded_payload.avg_temperature = static_cast<int32_t>(-40 * (1 << 8));

	encodePayload(encoded_payload);

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), &event_class, &event_state, &decoded);
	ASSERT_EQ(rc, NSM_SW_SUCCESS);

	EXPECT_EQ(decoded.max_temperature, encoded_payload.max_temperature);
	EXPECT_EQ(decoded.avg_temperature, encoded_payload.avg_temperature);

	// Sanity check the NvS24.8 -> double mapping that consumers will use.
	EXPECT_DOUBLE_EQ(static_cast<double>(decoded.max_temperature) / 256.0,
			 -25.5);
	EXPECT_DOUBLE_EQ(static_cast<double>(decoded.avg_temperature) / 256.0,
			 -40.0);
}

// -------- decode error paths --------------------------------------------

TEST_F(RuntimeISTCompleteEventDecode, DecodeNullMsg)
{
	encodePayload(makeSamplePayload());

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    nullptr, event_msg.size(), &event_class, &event_state, &decoded);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(RuntimeISTCompleteEventDecode, DecodeNullEventClass)
{
	encodePayload(makeSamplePayload());

	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), nullptr, &event_state, &decoded);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(RuntimeISTCompleteEventDecode, DecodeNullEventState)
{
	encodePayload(makeSamplePayload());

	uint8_t event_class{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), &event_class, nullptr, &decoded);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(RuntimeISTCompleteEventDecode, DecodeNullPayload)
{
	encodePayload(makeSamplePayload());

	uint8_t event_class{};
	uint16_t event_state{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), &event_class, &event_state, nullptr);
	EXPECT_EQ(rc, NSM_SW_ERROR_NULL);
}

TEST_F(RuntimeISTCompleteEventDecode, DecodeShortMsgLen)
{
	encodePayload(makeSamplePayload());

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	// msg_len < hdr + NSM_EVENT_MIN_LEN -> LENGTH error.
	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN - 1, &event_class,
	    &event_state, &decoded);
	EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST_F(RuntimeISTCompleteEventDecode, DecodeDataSizeInconsistentWithMsgLen)
{
	encodePayload(makeSamplePayload());

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	// Trim msg_len so that the wire data_size (= sizeof(payload)) no longer
	// equals msg_len - hdr - NSM_EVENT_MIN_LEN. RIST is fixed-length, so
	// any mismatch is a hard failure.
	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size() - 1, &event_class, &event_state, &decoded);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

TEST_F(RuntimeISTCompleteEventDecode, DecodeDataSizeNotEqualPayloadSize)
{
	encodePayload(makeSamplePayload());

	// Tamper with the wire frame so it advertises a payload size that
	// matches the buffer (passes the consistency check) but does not match
	// sizeof(*payload). Append 1 byte and bump data_size by 1 to keep the
	// first check happy and exercise the second check.
	event_msg.push_back(0xAB);
	auto *event = reinterpret_cast<nsm_event *>(msg()->payload);
	event->data_size += 1;

	uint8_t event_class{};
	uint16_t event_state{};
	nsm_runtime_ist_complete_event_payload decoded{};

	auto rc = decode_nsm_runtime_ist_complete_event(
	    msg(), event_msg.size(), &event_class, &event_state, &decoded);
	EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
