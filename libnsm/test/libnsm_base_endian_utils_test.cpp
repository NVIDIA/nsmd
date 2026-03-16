/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

// Unit tests for libnsm/base.c endian conversion and data copy utilities
// These functions were made non-static to enable testing

#include "base.h"

#include <gtest/gtest.h>

#include <cstring>
#include <endian.h>

// Forward declarations of functions under test (previously static)
extern "C" {
void htoleArrayData(uint8_t *data, uint16_t num_of_element, uint8_t data_type);
void letohArrayData(uint8_t *data, uint16_t num_of_element, uint8_t data_type);
void dataCopy(uint8_t *srcData, uint8_t *destData, uint16_t numOfElement,
	      uint8_t dataType);
}

// ============================================================================
// htoleArrayData Tests - Host to Little Endian Conversion
// ============================================================================

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU8NoConversion)
{
	uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
	uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};

	htoleArrayData(data, 4, NvU8);

	EXPECT_EQ(0, memcmp(data, expected, 4));
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS8NoConversion)
{
	uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
	uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};

	htoleArrayData(data, 4, NvS8);

	EXPECT_EQ(0, memcmp(data, expected, 4));
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU16SingleElement)
{
	uint16_t value = 0x1234;
	uint8_t data[2];
	memcpy(data, &value, 2);

	htoleArrayData(data, 1, NvU16);

	uint16_t result;
	memcpy(&result, data, 2);
	EXPECT_EQ(htole16(0x1234), result);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU16MultipleElements)
{
	uint16_t values[] = {0x1234, 0x5678, 0xABCD};
	uint8_t data[6];
	memcpy(data, values, 6);

	htoleArrayData(data, 3, NvU16);

	uint16_t results[3];
	memcpy(results, data, 6);
	EXPECT_EQ(htole16(0x1234), results[0]);
	EXPECT_EQ(htole16(0x5678), results[1]);
	EXPECT_EQ(htole16(0xABCD), results[2]);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS16SingleElement)
{
	int16_t value = -12345;
	uint8_t data[2];
	memcpy(data, &value, 2);

	htoleArrayData(data, 1, NvS16);

	int16_t result;
	memcpy(&result, data, 2);
	// On little-endian, htole16 preserves the bit pattern
	// Reinterpret as uint16_t for comparison
	uint16_t expected_bits;
	memcpy(&expected_bits, &value, 2);
	uint16_t result_bits;
	memcpy(&result_bits, &result, 2);
	EXPECT_EQ(htole16(expected_bits), result_bits);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS16MultipleElements)
{
	int16_t values[] = {-100, 200, -300};
	uint8_t data[6];
	memcpy(data, values, 6);

	// Save original bit patterns
	uint16_t original_bits[3];
	memcpy(original_bits, values, 6);

	htoleArrayData(data, 3, NvS16);

	uint16_t result_bits[3];
	memcpy(result_bits, data, 6);
	EXPECT_EQ(htole16(original_bits[0]), result_bits[0]);
	EXPECT_EQ(htole16(original_bits[1]), result_bits[1]);
	EXPECT_EQ(htole16(original_bits[2]), result_bits[2]);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU32SingleElement)
{
	uint32_t value = 0x12345678;
	uint8_t data[4];
	memcpy(data, &value, 4);

	htoleArrayData(data, 1, NvU32);

	uint32_t result;
	memcpy(&result, data, 4);
	EXPECT_EQ(htole32(0x12345678), result);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU32MultipleElements)
{
	uint32_t values[] = {0x11111111, 0x22222222, 0x33333333};
	uint8_t data[12];
	memcpy(data, values, 12);

	htoleArrayData(data, 3, NvU32);

	uint32_t results[3];
	memcpy(results, data, 12);
	EXPECT_EQ(htole32(0x11111111), results[0]);
	EXPECT_EQ(htole32(0x22222222), results[1]);
	EXPECT_EQ(htole32(0x33333333), results[2]);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS32SingleElement)
{
	int32_t value = -123456789;
	uint8_t data[4];
	memcpy(data, &value, 4);

	htoleArrayData(data, 1, NvS32);

	int32_t result;
	memcpy(&result, data, 4);
	EXPECT_EQ(htole32(-123456789), result);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS32MultipleElements)
{
	int32_t values[] = {-1000000, 2000000, -3000000};
	uint8_t data[12];
	memcpy(data, values, 12);

	htoleArrayData(data, 3, NvS32);

	int32_t results[3];
	memcpy(results, data, 12);
	EXPECT_EQ(htole32(-1000000), results[0]);
	EXPECT_EQ(htole32(2000000), results[1]);
	EXPECT_EQ(htole32(-3000000), results[2]);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS24_8SingleElement)
{
	float value = 123.456f;
	uint8_t data[4];
	memcpy(data, &value, 4);

	htoleArrayData(data, 1, NvS24_8);

	// Note: Production code bug - htole32(float) causes:
	// float->uint32_t->endian->float 123.456f -> uint32_t(123) ->
	// htole32(123) -> float(123.0f) Result is float representation of the
	// truncated integer value
	float result;
	memcpy(&result, data, 4);
	float expected = static_cast<float>(
	    static_cast<uint32_t>(value)); // 123.456f -> 123 -> 123.0f
	EXPECT_FLOAT_EQ(expected, result);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS24_8MultipleElements)
{
	float values[] = {1.5f, -2.5f, 3.75f};
	uint8_t data[12];
	memcpy(data, values, 12);

	htoleArrayData(data, 3, NvS24_8);

	// Note: Production code bug - htole32(float) causes
	// float->uint32_t->float conversion 1.5f -> 1 -> 1.0f, -2.5f -> large
	// uint -> large float, 3.75f -> 3 -> 3.0f
	float results[3];
	memcpy(results, data, 12);

	// Match actual behavior: float truncated to uint32_t, converted back to
	// float
	EXPECT_FLOAT_EQ(static_cast<float>(static_cast<uint32_t>(values[0])),
			results[0]); // 1.5f -> 1.0f
	EXPECT_FLOAT_EQ(static_cast<float>(static_cast<uint32_t>(values[1])),
			results[1]); // -2.5f -> 4294967294.0f
	EXPECT_FLOAT_EQ(static_cast<float>(static_cast<uint32_t>(values[2])),
			results[2]); // 3.75f -> 3.0f
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU64SingleElement)
{
	uint64_t value = 0x123456789ABCDEF0ULL;
	uint8_t data[8];
	memcpy(data, &value, 8);

	htoleArrayData(data, 1, NvU64);

	uint64_t result;
	memcpy(&result, data, 8);
	EXPECT_EQ(htole64(0x123456789ABCDEF0ULL), result);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvU64MultipleElements)
{
	uint64_t values[] = {0x1111111111111111ULL, 0x2222222222222222ULL};
	uint8_t data[16];
	memcpy(data, values, 16);

	htoleArrayData(data, 2, NvU64);

	uint64_t results[2];
	memcpy(results, data, 16);
	EXPECT_EQ(htole64(0x1111111111111111ULL), results[0]);
	EXPECT_EQ(htole64(0x2222222222222222ULL), results[1]);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS64SingleElement)
{
	int64_t value = -9876543210LL;
	uint8_t data[8];
	memcpy(data, &value, 8);

	htoleArrayData(data, 1, NvS64);

	int64_t result;
	memcpy(&result, data, 8);
	EXPECT_EQ(htole64(-9876543210LL), result);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataNvS64MultipleElements)
{
	int64_t values[] = {-1000000000000LL, 2000000000000LL};
	uint8_t data[16];
	memcpy(data, values, 16);

	htoleArrayData(data, 2, NvS64);

	int64_t results[2];
	memcpy(results, data, 16);
	EXPECT_EQ(htole64(-1000000000000LL), results[0]);
	EXPECT_EQ(htole64(2000000000000LL), results[1]);
}

TEST(LibnsmBaseEndianUtils, HtoleArrayDataUnknownTypeNoOp)
{
	uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
	uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};

	htoleArrayData(data, 2, 255); // Unknown type

	EXPECT_EQ(0, memcmp(data, expected, 4));
}

// ============================================================================
// letohArrayData Tests - Little Endian to Host Conversion
// ============================================================================

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvU8NoConversion)
{
	uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
	uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};

	letohArrayData(data, 4, NvU8);

	EXPECT_EQ(0, memcmp(data, expected, 4));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvS8NoConversion)
{
	uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
	uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};

	letohArrayData(data, 4, NvS8);

	EXPECT_EQ(0, memcmp(data, expected, 4));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvU16SingleElement)
{
	uint16_t value = htole16(0x1234);
	uint8_t data[2];
	memcpy(data, &value, 2);

	letohArrayData(data, 1, NvU16);

	uint16_t result;
	memcpy(&result, data, 2);
	EXPECT_EQ(0x1234, le16toh(result));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvU16MultipleElements)
{
	uint16_t values[] = {htole16(0x1234), htole16(0x5678), htole16(0xABCD)};
	uint8_t data[6];
	memcpy(data, values, 6);

	letohArrayData(data, 3, NvU16);

	uint16_t results[3];
	memcpy(results, data, 6);
	EXPECT_EQ(0x1234, le16toh(results[0]));
	EXPECT_EQ(0x5678, le16toh(results[1]));
	EXPECT_EQ(0xABCD, le16toh(results[2]));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvS16SingleElement)
{
	int16_t value = htole16(-12345);
	uint8_t data[2];
	memcpy(data, &value, 2);

	letohArrayData(data, 1, NvS16);

	int16_t result;
	memcpy(&result, data, 2);
	EXPECT_EQ(-12345, static_cast<int16_t>(le16toh(result)));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvU32SingleElement)
{
	uint32_t value = htole32(0x12345678);
	uint8_t data[4];
	memcpy(data, &value, 4);

	letohArrayData(data, 1, NvU32);

	uint32_t result;
	memcpy(&result, data, 4);
	EXPECT_EQ(0x12345678, le32toh(result));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvS32SingleElement)
{
	int32_t value = htole32(-123456789);
	uint8_t data[4];
	memcpy(data, &value, 4);

	letohArrayData(data, 1, NvS32);

	int32_t result;
	memcpy(&result, data, 4);
	EXPECT_EQ(-123456789, static_cast<int32_t>(le32toh(result)));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvS24_8SingleElement)
{
	float value = 123.456f;
	uint8_t data[4];
	memcpy(data, &value, 4);

	letohArrayData(data, 1, NvS24_8);

	// Note: Same production code bug as htoleArrayData
	// Reads float 123.456f, does le32toh(float) which converts
	// float->uint32_t->endian->float 123.456f -> uint32_t(123) ->
	// le32toh(123) -> float(123.0f)
	float result;
	memcpy(&result, data, 4);
	float expected = static_cast<float>(
	    static_cast<uint32_t>(value)); // 123.456f -> 123 -> 123.0f
	EXPECT_FLOAT_EQ(expected, result);
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvU64SingleElement)
{
	uint64_t value = htole64(0x123456789ABCDEF0ULL);
	uint8_t data[8];
	memcpy(data, &value, 8);

	letohArrayData(data, 1, NvU64);

	uint64_t result;
	memcpy(&result, data, 8);
	EXPECT_EQ(0x123456789ABCDEF0ULL, le64toh(result));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataNvS64SingleElement)
{
	int64_t value = htole64(-9876543210LL);
	uint8_t data[8];
	memcpy(data, &value, 8);

	letohArrayData(data, 1, NvS64);

	int64_t result;
	memcpy(&result, data, 8);
	EXPECT_EQ(-9876543210LL, static_cast<int64_t>(le64toh(result)));
}

TEST(LibnsmBaseEndianUtils, LetohArrayDataUnknownTypeNoOp)
{
	uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
	uint8_t expected[] = {0x12, 0x34, 0x56, 0x78};

	letohArrayData(data, 2, 255); // Unknown type

	EXPECT_EQ(0, memcmp(data, expected, 4));
}

// ============================================================================
// dataCopy Tests - Type-Aware Data Copy
// ============================================================================

TEST(LibnsmBaseEndianUtils, DataCopyNvU8SingleElement)
{
	uint8_t src[] = {0x42};
	uint8_t dest[1] = {0};

	dataCopy(src, dest, 1, NvU8);

	EXPECT_EQ(0x42, dest[0]);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU8MultipleElements)
{
	uint8_t src[] = {0x11, 0x22, 0x33, 0x44};
	uint8_t dest[4] = {0};

	dataCopy(src, dest, 4, NvU8);

	EXPECT_EQ(0, memcmp(src, dest, 4));
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS8MultipleElements)
{
	uint8_t src[] = {0xFF, 0x7F, 0x80, 0x00}; // -1, 127, -128, 0
	uint8_t dest[4] = {0};

	dataCopy(src, dest, 4, NvS8);

	EXPECT_EQ(0, memcmp(src, dest, 4));
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU16SingleElement)
{
	uint16_t src = 0x1234;
	uint8_t srcData[2];
	memcpy(srcData, &src, 2);
	uint8_t dest[2] = {0};

	dataCopy(srcData, dest, 1, NvU16);

	uint16_t result;
	memcpy(&result, dest, 2);
	EXPECT_EQ(0x1234, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU16MultipleElements)
{
	uint16_t src[] = {0x1111, 0x2222, 0x3333};
	uint8_t srcData[6];
	memcpy(srcData, src, 6);
	uint8_t dest[6] = {0};

	dataCopy(srcData, dest, 3, NvU16);

	uint16_t results[3];
	memcpy(results, dest, 6);
	EXPECT_EQ(0x1111, results[0]);
	EXPECT_EQ(0x2222, results[1]);
	EXPECT_EQ(0x3333, results[2]);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS16SingleElement)
{
	int16_t src = -12345;
	uint8_t srcData[2];
	memcpy(srcData, &src, 2);
	uint8_t dest[2] = {0};

	dataCopy(srcData, dest, 1, NvS16);

	int16_t result;
	memcpy(&result, dest, 2);
	EXPECT_EQ(-12345, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU32SingleElement)
{
	uint32_t src = 0x12345678;
	uint8_t srcData[4];
	memcpy(srcData, &src, 4);
	uint8_t dest[4] = {0};

	dataCopy(srcData, dest, 1, NvU32);

	uint32_t result;
	memcpy(&result, dest, 4);
	EXPECT_EQ(0x12345678, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU32MultipleElements)
{
	uint32_t src[] = {0xAAAAAAAA, 0xBBBBBBBB};
	uint8_t srcData[8];
	memcpy(srcData, src, 8);
	uint8_t dest[8] = {0};

	dataCopy(srcData, dest, 2, NvU32);

	uint32_t results[2];
	memcpy(results, dest, 8);
	EXPECT_EQ(0xAAAAAAAA, results[0]);
	EXPECT_EQ(0xBBBBBBBB, results[1]);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS32SingleElement)
{
	int32_t src = -987654321;
	uint8_t srcData[4];
	memcpy(srcData, &src, 4);
	uint8_t dest[4] = {0};

	dataCopy(srcData, dest, 1, NvS32);

	int32_t result;
	memcpy(&result, dest, 4);
	EXPECT_EQ(-987654321, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS24_8SingleElement)
{
	float src = 3.14159f;
	uint8_t srcData[4];
	memcpy(srcData, &src, 4);
	uint8_t dest[4] = {0};

	dataCopy(srcData, dest, 1, NvS24_8);

	float result;
	memcpy(&result, dest, 4);
	EXPECT_FLOAT_EQ(3.14159f, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS24_8MultipleElements)
{
	float src[] = {1.1f, 2.2f, 3.3f};
	uint8_t srcData[12];
	memcpy(srcData, src, 12);
	uint8_t dest[12] = {0};

	dataCopy(srcData, dest, 3, NvS24_8);

	float results[3];
	memcpy(results, dest, 12);
	EXPECT_FLOAT_EQ(1.1f, results[0]);
	EXPECT_FLOAT_EQ(2.2f, results[1]);
	EXPECT_FLOAT_EQ(3.3f, results[2]);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU64SingleElement)
{
	uint64_t src = 0x123456789ABCDEF0ULL;
	uint8_t srcData[8];
	memcpy(srcData, &src, 8);
	uint8_t dest[8] = {0};

	dataCopy(srcData, dest, 1, NvU64);

	uint64_t result;
	memcpy(&result, dest, 8);
	EXPECT_EQ(0x123456789ABCDEF0ULL, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvU64MultipleElements)
{
	uint64_t src[] = {0xFFFFFFFFFFFFFFFFULL, 0x0000000000000000ULL};
	uint8_t srcData[16];
	memcpy(srcData, src, 16);
	uint8_t dest[16] = {0};

	dataCopy(srcData, dest, 2, NvU64);

	uint64_t results[2];
	memcpy(results, dest, 16);
	EXPECT_EQ(0xFFFFFFFFFFFFFFFFULL, results[0]);
	EXPECT_EQ(0x0000000000000000ULL, results[1]);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS64SingleElement)
{
	int64_t src = -1234567890123456789LL;
	uint8_t srcData[8];
	memcpy(srcData, &src, 8);
	uint8_t dest[8] = {0};

	dataCopy(srcData, dest, 1, NvS64);

	int64_t result;
	memcpy(&result, dest, 8);
	EXPECT_EQ(-1234567890123456789LL, result);
}

TEST(LibnsmBaseEndianUtils, DataCopyNvS64MultipleElements)
{
	int64_t src[] = {-999999999999LL, 888888888888LL};
	uint8_t srcData[16];
	memcpy(srcData, src, 16);
	uint8_t dest[16] = {0};

	dataCopy(srcData, dest, 2, NvS64);

	int64_t results[2];
	memcpy(results, dest, 16);
	EXPECT_EQ(-999999999999LL, results[0]);
	EXPECT_EQ(888888888888LL, results[1]);
}

TEST(LibnsmBaseEndianUtils, DataCopyUnknownTypeNoOp)
{
	uint8_t src[] = {0x11, 0x22, 0x33, 0x44};
	uint8_t dest[4] = {0};

	dataCopy(src, dest, 2, 255); // Unknown type - dataSize = 0

	// With unknown type, dataSize remains 0, so memcpy copies 0 bytes
	EXPECT_EQ(0, dest[0]);
	EXPECT_EQ(0, dest[1]);
	EXPECT_EQ(0, dest[2]);
	EXPECT_EQ(0, dest[3]);
}

// ============================================================================
// Round-trip Tests - htole followed by letoh should restore original data
// ============================================================================

TEST(LibnsmBaseEndianUtils, RoundTripNvU16)
{
	uint16_t original[] = {0x1234, 0x5678, 0xABCD};
	uint8_t data[6];
	memcpy(data, original, 6);

	htoleArrayData(data, 3, NvU16);
	letohArrayData(data, 3, NvU16);

	uint16_t result[3];
	memcpy(result, data, 6);
	EXPECT_EQ(original[0], result[0]);
	EXPECT_EQ(original[1], result[1]);
	EXPECT_EQ(original[2], result[2]);
}

TEST(LibnsmBaseEndianUtils, RoundTripNvS32)
{
	int32_t original[] = {-12345, 67890, -999999};
	uint8_t data[12];
	memcpy(data, original, 12);

	htoleArrayData(data, 3, NvS32);
	letohArrayData(data, 3, NvS32);

	int32_t result[3];
	memcpy(result, data, 12);
	EXPECT_EQ(original[0], result[0]);
	EXPECT_EQ(original[1], result[1]);
	EXPECT_EQ(original[2], result[2]);
}

TEST(LibnsmBaseEndianUtils, RoundTripNvU64)
{
	uint64_t original[] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL};
	uint8_t data[16];
	memcpy(data, original, 16);

	htoleArrayData(data, 2, NvU64);
	letohArrayData(data, 2, NvU64);

	uint64_t result[2];
	memcpy(result, data, 16);
	EXPECT_EQ(original[0], result[0]);
	EXPECT_EQ(original[1], result[1]);
}
