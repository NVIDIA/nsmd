/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

// Line Coverage Tests for libnsm/firmware-utils.c
// Focus: Firmware aggregate tag decoding functions

#include "firmware-utils.h"

#include <cstdint>
#include <cstring>
#include <endian.h>
#include <gtest/gtest.h>
#include <vector>

// Test fixture for firmware aggregate tests
class FirmwareAggregateTest : public ::testing::Test
{
      protected:
	void SetUp() override
	{
		// Common setup if needed
	}
};

// ========== decode_nsm_firmware_aggregate_tag_uint8 Tests ==========

TEST_F(FirmwareAggregateTest, DecodeAggregateUint8Valid)
{
	// Use encode to generate correct buffer format
	std::vector<uint8_t> buffer(10, 0);
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = 0;
	encode_nsm_firmware_aggregate_tag_uint8(&buf_ptr, 0x01, 0xAB,
						&buf_size);

	// Now decode it
	buf_ptr = buffer.data();
	buf_size = 10;
	uint8_t tag = 0, valid = 0, value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint8(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x01);
	EXPECT_EQ(valid, 0x01);
	EXPECT_EQ(value, 0xAB);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint8Invalid)
{
	// For invalid (valid=0), manually create buffer since encode always
	// sets valid=1
	std::vector<uint8_t> buffer(10, 0);
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)buffer.data();
	field->tag = 0x02;
	field->valid = 0; // Invalid
	field->length = 0;
	field->reserved = 0;
	field->data[0] = 0x00;

	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0xFF, value = 0xFF;

	bool result = decode_nsm_firmware_aggregate_tag_uint8(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x02);
	EXPECT_EQ(valid, 0x00);
	// value should remain unchanged when valid=false
	EXPECT_EQ(value, 0xFF);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint8InsufficientBuffer)
{
	// Buffer too small (only 2 bytes, need 3)
	std::vector<uint8_t> buffer = {0x01, 0x01};
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0, value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint8(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_FALSE(result);
}

// ========== decode_nsm_firmware_aggregate_tag_uint16 Tests ==========

TEST_F(FirmwareAggregateTest, DecodeAggregateUint16Valid)
{
	// Use encode to generate correct buffer format
	std::vector<uint8_t> buffer(10, 0);
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = 0;
	encode_nsm_firmware_aggregate_tag_uint16(&buf_ptr, 0x03, 0x1234,
						 &buf_size);

	// Now decode it
	buf_ptr = buffer.data();
	buf_size = 10;
	uint8_t tag = 0, valid = 0;
	uint16_t value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint16(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x03);
	EXPECT_EQ(valid, 0x01);
	EXPECT_EQ(value, 0x1234);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint16Invalid)
{
	// For invalid (valid=0), manually create buffer since encode always
	// sets valid=1
	std::vector<uint8_t> buffer(10, 0);
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)buffer.data();
	field->tag = 0x04;
	field->valid = 0;  // Invalid
	field->length = 1; // 2 bytes for uint16
	field->reserved = 0;
	*((uint16_t *)field->data) = htole16(0xFFFF);

	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0xFF;
	uint16_t value = 0xFFFF;

	bool result = decode_nsm_firmware_aggregate_tag_uint16(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x04);
	EXPECT_EQ(valid, 0x00);
	EXPECT_EQ(value, 0xFFFF); // unchanged
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint16InsufficientBuffer)
{
	std::vector<uint8_t> buffer = {0x03, 0x01,
				       0x01}; // Only 3 bytes, need 4
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0;
	uint16_t value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint16(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_FALSE(result);
}

// ========== decode_nsm_firmware_aggregate_tag_uint32 Tests ==========

TEST_F(FirmwareAggregateTest, DecodeAggregateUint32Valid)
{
	// Use encode to generate correct buffer format
	std::vector<uint8_t> buffer(12, 0);
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = 0;
	encode_nsm_firmware_aggregate_tag_uint32(&buf_ptr, 0x05, 0x12345678U,
						 &buf_size);

	// Now decode it
	buf_ptr = buffer.data();
	buf_size = 12;
	uint8_t tag = 0, valid = 0;
	uint32_t value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint32(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x05);
	EXPECT_EQ(valid, 0x01);
	EXPECT_EQ(value, 0x12345678U);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint32Invalid)
{
	// For invalid (valid=0), manually create buffer since encode always
	// sets valid=1
	std::vector<uint8_t> buffer(12, 0);
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)buffer.data();
	field->tag = 0x06;
	field->valid = 0;  // Invalid
	field->length = 2; // 4 bytes for uint32
	field->reserved = 0;
	uint32_t val = htole32(0xFFFFFFFF);
	memcpy(field->data, &val, sizeof(val));

	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0xFF;
	uint32_t value = 0xFFFFFFFF;

	bool result = decode_nsm_firmware_aggregate_tag_uint32(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x06);
	EXPECT_EQ(valid, 0x00);
	EXPECT_EQ(value, 0xFFFFFFFFU); // unchanged
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint32InsufficientBuffer)
{
	std::vector<uint8_t> buffer = {0x05, 0x01, 0x02, 0x78,
				       0x56}; // Only 5 bytes, need 6
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0;
	uint32_t value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint32(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_FALSE(result);
}

// ========== decode_nsm_firmware_aggregate_tag_uint64 Tests ==========

TEST_F(FirmwareAggregateTest, DecodeAggregateUint64Valid)
{
	// Use encode to generate correct buffer format
	std::vector<uint8_t> buffer(16, 0);
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = 0;
	encode_nsm_firmware_aggregate_tag_uint64(
	    &buf_ptr, 0x07, 0x1234567890ABCDEFULL, &buf_size);

	// Now decode it
	buf_ptr = buffer.data();
	buf_size = 16;
	uint8_t tag = 0, valid = 0;
	uint64_t value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint64(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x07);
	EXPECT_EQ(valid, 0x01);
	EXPECT_EQ(value, 0x1234567890ABCDEFULL);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint64Invalid)
{
	// For invalid (valid=0), manually create buffer since encode always
	// sets valid=1
	std::vector<uint8_t> buffer(16, 0);
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)buffer.data();
	field->tag = 0x08;
	field->valid = 0;  // Invalid
	field->length = 3; // 8 bytes for uint64
	field->reserved = 0;
	uint64_t val64 = htole64(0xFFFFFFFFFFFFFFFFULL);
	memcpy(field->data, &val64, sizeof(val64));

	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0xFF;
	uint64_t value = 0xFFFFFFFFFFFFFFFFULL;

	bool result = decode_nsm_firmware_aggregate_tag_uint64(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x08);
	EXPECT_EQ(valid, 0x00);
	EXPECT_EQ(value, 0xFFFFFFFFFFFFFFFFULL); // unchanged
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint64InsufficientBuffer)
{
	std::vector<uint8_t> buffer = {
	    0x07, 0x01, 0x03, 0xEF, 0xCD,
	    0xAB, 0x90, 0x78, 0x56}; // Only 9 bytes, need 10
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0;
	uint64_t value = 0;

	bool result = decode_nsm_firmware_aggregate_tag_uint64(
	    &buf_ptr, &tag, &valid, &value, &buf_size);

	EXPECT_FALSE(result);
}

// ========== decode_nsm_firmware_aggregate_tag_uint8_array Tests ==========

TEST_F(FirmwareAggregateTest, DecodeAggregateUint8ArrayValid)
{
	// Use encode to generate correct buffer format (always 16 bytes)
	std::vector<uint8_t> buffer(24, 0);
	uint8_t test_data[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
				 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
				 0x77, 0x88, 0x99, 0x00};
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = 0;
	encode_nsm_firmware_aggregate_tag_uint8_array(&buf_ptr, 0x09, test_data,
						      &buf_size);

	// Now decode it
	buf_ptr = buffer.data();
	buf_size = 24;
	uint8_t tag = 0, valid = 0;
	uint8_t value[16] = {0};

	bool result = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &buf_ptr, &tag, &valid, value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x09);
	EXPECT_EQ(valid, 0x01);
	EXPECT_EQ(value[0], 0xAA);
	EXPECT_EQ(value[1], 0xBB);
	EXPECT_EQ(value[2], 0xCC);
	EXPECT_EQ(value[3], 0xDD);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateUint8ArrayInvalid)
{
	// For invalid (valid=0), manually create buffer since encode always
	// sets valid=1
	std::vector<uint8_t> buffer(24, 0xFF);
	struct nsm_firmware_aggregate_tag *field =
	    (struct nsm_firmware_aggregate_tag *)buffer.data();
	field->tag = 0x0A;
	field->valid = 0;  // Invalid
	field->length = 4; // 16 bytes for array
	field->reserved = 0;

	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0xFF;
	uint8_t value[16] = {0x00};

	bool result = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &buf_ptr, &tag, &valid, value, &buf_size);

	EXPECT_TRUE(result);
	EXPECT_EQ(tag, 0x0A);
	EXPECT_EQ(valid, 0x00);
	// value should remain unchanged when valid=false
	EXPECT_EQ(value[0], 0x00);
}

TEST_F(FirmwareAggregateTest,
       DecodeAggregateUint8ArrayInsufficientBufferInitial)
{
	std::vector<uint8_t> buffer = {0x09, 0x01,
				       0x02}; // Only 3 bytes, need at least 6
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0;
	uint8_t value[4] = {0};

	bool result = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &buf_ptr, &tag, &valid, value, &buf_size);

	EXPECT_FALSE(result);
}

TEST_F(FirmwareAggregateTest,
       DecodeAggregateUint8ArrayInsufficientBufferForLength)
{
	std::vector<uint8_t> buffer = {
	    0x09,      // tag
	    0x01,      // valid = true
	    0x02,      // length (2 = 4 bytes)
	    0xAA, 0xBB // only 2 bytes instead of 4
	};
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint8_t tag = 0, valid = 0;
	uint8_t value[4] = {0};

	bool result = decode_nsm_firmware_aggregate_tag_uint8_array(
	    &buf_ptr, &tag, &valid, value, &buf_size);

	EXPECT_FALSE(result);
}

// ========== decode_nsm_firmware_aggregate_tag_skip Tests ==========

TEST_F(FirmwareAggregateTest, DecodeAggregateSkipValid)
{
	std::vector<uint8_t> buffer = {
	    0x0B, // tag
	    0x01, // valid (bitfield: valid=1, length=0, reserved=0)
	    0xAA, // data[0] (1 byte for length=0)
	    0x0C, // next tag
	    0x01, // next valid
	    0x00  // next length
	};
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();
	uint16_t original_size = buf_size;

	bool result =
	    decode_nsm_firmware_aggregate_tag_skip(&buf_ptr, &buf_size);

	EXPECT_TRUE(result);
	// Buffer should have advanced by: sizeof(struct) + data_len - 1 = 3 + 1
	// - 1 = 3 bytes
	EXPECT_EQ(buf_size, original_size - 3);
}

TEST_F(FirmwareAggregateTest, DecodeAggregateSkipInsufficientBuffer)
{
	std::vector<uint8_t> buffer = {0x0B,
				       0x01}; // Only 2 bytes, need at least 3
	uint8_t *buf_ptr = buffer.data();
	uint16_t buf_size = buffer.size();

	bool result =
	    decode_nsm_firmware_aggregate_tag_skip(&buf_ptr, &buf_size);

	EXPECT_FALSE(result);
}
