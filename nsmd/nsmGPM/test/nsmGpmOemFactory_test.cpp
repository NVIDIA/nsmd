/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

// Forward-declare the internal function from nsmGpmOemFactory.cpp
namespace nsm
{
std::vector<uint8_t> convertToBytes(const std::vector<uint64_t>& data);
} // namespace nsm

using namespace nsm;

// ============================================================================
// convertToBytes Tests
// ============================================================================

TEST(NsmGpmOemFactoryTest, ConvertToBytesEmptyVector)
{
    std::vector<uint64_t> input;
    auto result = convertToBytes(input);

    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.size(), 0u);
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesZeroValue)
{
    std::vector<uint64_t> input = {0};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesMaxUint8)
{
    std::vector<uint64_t> input = {255};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(255));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesValueTruncatedToUint8)
{
    // 0x1FF = 511 truncated to uint8 = 0xFF = 255
    std::vector<uint64_t> input = {0x1FF};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0xFF));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesLargeValueTruncated)
{
    // 0xABCD takes low byte 0xCD
    std::vector<uint64_t> input = {0xABCD};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0xCD));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesMaxUint64Truncated)
{
    // UINT64_MAX = 0xFFFFFFFFFFFFFFFF, low byte = 0xFF
    std::vector<uint64_t> input = {UINT64_MAX};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0xFF));
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesMultipleElements)
{
    std::vector<uint64_t> input = {0, 1, 2, 3, 255};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0));
    EXPECT_EQ(result[1], static_cast<uint8_t>(1));
    EXPECT_EQ(result[2], static_cast<uint8_t>(2));
    EXPECT_EQ(result[3], static_cast<uint8_t>(3));
    EXPECT_EQ(result[4], static_cast<uint8_t>(255));
}

TEST(NsmGpmOemFactoryTest, ConvertBytesSizePreserved)
{
    std::vector<uint64_t> input = {10, 20, 30};
    auto result = convertToBytes(input);

    EXPECT_EQ(result.size(), input.size());
}

TEST(NsmGpmOemFactoryTest, ConvertToBytesValuesAreOnlyLowByte)
{
    // Each value has different bits set; result should be just the low byte
    std::vector<uint64_t> input = {0x0102030405060708ULL,
                                   0xFF00FF00FF00FF00ULL};
    auto result = convertToBytes(input);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], static_cast<uint8_t>(0x08));
    EXPECT_EQ(result[1], static_cast<uint8_t>(0x00));
}
