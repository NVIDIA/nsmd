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

#include "../utils.hpp"

#include <gtest/gtest.h>

// Tests for Bitfield256 class
TEST(Bitfield256Test, ConstructorClearsAllBits)
{
    utils::Bitfield256 bf;
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(Bitfield256Test, SetBitReturnsTrueForUnsetBit)
{
    utils::Bitfield256 bf;
    EXPECT_TRUE(bf.setBit(0));
    EXPECT_TRUE(bf.setBit(1));
    EXPECT_TRUE(bf.setBit(255));
}

TEST(Bitfield256Test, SetBitReturnsFalseForAlreadySetBit)
{
    utils::Bitfield256 bf;
    bf.setBit(5);
    EXPECT_FALSE(bf.setBit(5)); // Setting same bit again
}

TEST(Bitfield256Test, IsAnyBitSetReturnsTrueAfterSettingBit)
{
    utils::Bitfield256 bf;
    EXPECT_FALSE(bf.isAnyBitSet());
    bf.setBit(10);
    EXPECT_TRUE(bf.isAnyBitSet());
}

TEST(Bitfield256Test, GetSetBitsSingleBit)
{
    utils::Bitfield256 bf;
    bf.setBit(5);
    auto result = bf.getSetBits();
    EXPECT_EQ(result, "5");
}

TEST(Bitfield256Test, GetSetBitsMultipleBits)
{
    utils::Bitfield256 bf;
    bf.setBit(0);
    bf.setBit(5);
    bf.setBit(10);
    bf.setBit(255);
    auto result = bf.getSetBits();
    // Should contain comma-separated list
    EXPECT_NE(result.find("0"), std::string::npos);
    EXPECT_NE(result.find("5"), std::string::npos);
    EXPECT_NE(result.find("10"), std::string::npos);
    EXPECT_NE(result.find("255"), std::string::npos);
}

TEST(Bitfield256Test, ClearResetsAllBits)
{
    utils::Bitfield256 bf;
    bf.setBit(1);
    bf.setBit(50);
    bf.setBit(200);
    EXPECT_TRUE(bf.isAnyBitSet());

    bf.clear();
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(Bitfield256Test, SetBitBoundaryValues)
{
    utils::Bitfield256 bf;

    // Test first bit
    EXPECT_TRUE(bf.setBit(0));
    EXPECT_TRUE(bf.isAnyBitSet());

    bf.clear();

    // Test last bit
    EXPECT_TRUE(bf.setBit(255));
    EXPECT_TRUE(bf.isAnyBitSet());
}

// Tests for bitMapToBitfield256_t function
TEST(BitMapTest, BitMapToBitfield256ValidSize)
{
    std::vector<uint8_t> bitmap(32, 0x00);
    bitmap[0] = 0x01; // Set first bit

    auto result = utils::bitMapToBitfield256_t(bitmap);

    // Check that the conversion happened (checking struct is initialized)
    EXPECT_EQ(result.fields[0].byte, 0x01000000); // Big-endian conversion
}

TEST(BitMapTest, BitMapToBitfield256InvalidSize)
{
    std::vector<uint8_t> bitmap(16, 0x00); // Wrong size

    auto result = utils::bitMapToBitfield256_t(bitmap);

    // Should return zeroed bitfield
    bool allZero = true;
    for (const auto& field : result.fields)
    {
        if (field.byte != 0)
        {
            allZero = false;
            break;
        }
    }
    EXPECT_TRUE(allZero);
}

TEST(BitMapTest, BitMapToBitfield256AllOnes)
{
    std::vector<uint8_t> bitmap(32, 0xFF);

    auto result = utils::bitMapToBitfield256_t(bitmap);

    // All fields should be 0xFFFFFFFF
    for (const auto& field : result.fields)
    {
        EXPECT_EQ(field.byte, 0xFFFFFFFF);
    }
}

// Tests for vectorTo256BitHexString function
TEST(VectorToHexTest, VectorTo256BitHexStringValidSize)
{
    std::vector<uint8_t> value(32, 0x00);
    value[0] = 0xAB;
    value[31] = 0xCD;

    auto result = utils::vectorTo256BitHexString(value);

    EXPECT_EQ(result.substr(0, 2), "0x");
    EXPECT_NE(result.find("ab"), std::string::npos);
    EXPECT_NE(result.find("cd"), std::string::npos);
    EXPECT_EQ(result.length(), 2 + 64); // "0x" + 64 hex chars
}

TEST(VectorToHexTest, VectorTo256BitHexStringInvalidSize)
{
    std::vector<uint8_t> value(16, 0xAB); // Wrong size

    auto result = utils::vectorTo256BitHexString(value);

    // Should return all zeros
    EXPECT_EQ(result, "0x" + std::string(64, '0'));
}

TEST(VectorToHexTest, VectorTo256BitHexStringAllFF)
{
    std::vector<uint8_t> value(32, 0xFF);

    auto result = utils::vectorTo256BitHexString(value);

    EXPECT_EQ(result, "0x" + std::string(64, 'f'));
}

// Tests for requestMsgToHexString function
TEST(RequestMsgTest, RequestMsgToHexStringEmptyVector)
{
    std::vector<uint8_t> msg;

    auto result = utils::requestMsgToHexString(msg);

    EXPECT_TRUE(result.empty() || result == " ");
}

TEST(RequestMsgTest, RequestMsgToHexStringSingleByte)
{
    std::vector<uint8_t> msg = {0xAB};

    auto result = utils::requestMsgToHexString(msg);

    EXPECT_NE(result.find("ab"), std::string::npos);
}

TEST(RequestMsgTest, RequestMsgToHexStringMultipleBytes)
{
    std::vector<uint8_t> msg = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};

    auto result = utils::requestMsgToHexString(msg);

    // Should contain hex representation with spaces
    EXPECT_NE(result.find("01"), std::string::npos);
    EXPECT_NE(result.find("23"), std::string::npos);
    EXPECT_NE(result.find("ab"), std::string::npos);
}

// Tests for safe conversion functions
TEST(SafeConvertTest, ConvertAndScaleDownUint32ToDoubleValid)
{
    uint32_t value = 1000;
    double scaleFactor = 10.0;

    auto result = utils::convertAndScaleDownUint32ToDouble(value, scaleFactor);

    EXPECT_DOUBLE_EQ(result, 100.0);
}

TEST(SafeConvertTest, ConvertAndScaleDownUint32ToDoubleInvalid)
{
    uint32_t value = 0xFFFFFFFF; // INVALID_UINT32_VALUE
    double scaleFactor = 10.0;

    auto result = utils::convertAndScaleDownUint32ToDouble(value, scaleFactor);

    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFFFFFFFF));
}

TEST(SafeConvertTest, ConvertAndScaleDownUint8ToDoubleValid)
{
    uint8_t value = 100;

    auto result = utils::convertAndScaleDownUint8ToDouble(value);

    EXPECT_DOUBLE_EQ(result, 100.0);
}

TEST(SafeConvertTest, ConvertAndScaleDownUint8ToDoubleInvalid)
{
    uint8_t value = 0xFF; // INVALID_UINT8_VALUE

    auto result = utils::convertAndScaleDownUint8ToDouble(value);

    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFF));
}

TEST(SafeConvertTest, Uint64ToDoubleSafeConvertWithinRange)
{
    uint64_t value = 1000000ULL;

    auto result = utils::uint64ToDoubleSafeConvert(value);

    EXPECT_DOUBLE_EQ(result, 1000000.0);
}

TEST(SafeConvertTest, Uint64ToDoubleSafeConvertExceedsRange)
{
    uint64_t value = (1ULL << 60); // Very large value

    auto result = utils::uint64ToDoubleSafeConvert(value);

    // Should be capped to MAX_SAFE_INTEGER_IN_DOUBLE
    EXPECT_DOUBLE_EQ(result, static_cast<double>((1ULL << 53) - 1));
}

TEST(SafeConvertTest, Int64ToDoubleSafeConvertPositiveWithinRange)
{
    int64_t value = 1000000LL;

    auto result = utils::int64ToDoubleSafeConvert(value);

    EXPECT_DOUBLE_EQ(result, 1000000.0);
}

TEST(SafeConvertTest, Int64ToDoubleSafeConvertNegativeWithinRange)
{
    int64_t value = -1000000LL;

    auto result = utils::int64ToDoubleSafeConvert(value);

    EXPECT_DOUBLE_EQ(result, -1000000.0);
}

TEST(SafeConvertTest, Int64ToDoubleSafeConvertPositiveExceedsRange)
{
    int64_t value = (1LL << 60); // Very large positive value

    auto result = utils::int64ToDoubleSafeConvert(value);

    // Should be capped to MAX_SAFE_INTEGER_IN_DOUBLE
    EXPECT_DOUBLE_EQ(result, static_cast<double>((1ULL << 53) - 1));
}

TEST(SafeConvertTest, Int64ToDoubleSafeConvertNegativeExceedsRange)
{
    int64_t value = -(1LL << 60); // Very large negative value

    auto result = utils::int64ToDoubleSafeConvert(value);

    // Should be capped to -MAX_SAFE_INTEGER_IN_DOUBLE
    // Use 1LL (signed) to get correct negative value; 1ULL would wrap around
    EXPECT_DOUBLE_EQ(result, static_cast<double>(-((1LL << 53) - 1)));
}
