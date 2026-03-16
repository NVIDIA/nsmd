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

// Tests for isPreferred function
// Note: Lower priority number = higher preference (0=best, 6=worst)
// isPreferred returns true when current >= new in priority number,
// meaning current is worse or equal (confusing naming!)
TEST(UtilsTest, IsPreferredSameMediumDifferentBinding)
{
    std::tuple<MctpMedium, MctpBinding> current = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.USB"};

    // Current binding PCIe(0) < USB(1), so current has lower number = better
    // Function returns false when current is better
    EXPECT_FALSE(utils::isPreferred(current, newInfo));
}

TEST(UtilsTest, IsPreferredDifferentMediumSameBinding)
{
    std::tuple<MctpMedium, MctpBinding> current = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.USB",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.USB"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.USB"};

    // Current medium USB(1) > PCIe(0), so 1 >= 0 = true
    // Current is worse, function returns true
    EXPECT_TRUE(utils::isPreferred(current, newInfo));
}

TEST(UtilsTest, IsPreferredIdenticalInfo)
{
    std::tuple<MctpMedium, MctpBinding> info = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};

    // Identical info: 0 >= 0 = true
    EXPECT_TRUE(utils::isPreferred(info, info));
}

TEST(UtilsTest, IsPreferredLowerPriorityMedium)
{
    std::tuple<MctpMedium, MctpBinding> current = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.SMBus",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.SMBus"};

    // Current medium SMBus(6) > PCIe(0), so 6 >= 0 = true
    // Current is worse, function returns true
    EXPECT_TRUE(utils::isPreferred(current, newInfo));
}

// Tests for convertHexToString function
TEST(UtilsTest, ConvertHexToStringEmptyData)
{
    std::vector<uint8_t> data;
    auto result = utils::convertHexToString(data, 0);
    EXPECT_EQ(result, "");
}

TEST(UtilsTest, ConvertHexToStringSingleByte)
{
    std::vector<uint8_t> data = {0xAB};
    auto result = utils::convertHexToString(data, 1);
    EXPECT_EQ(result, "ab");
}

TEST(UtilsTest, ConvertHexToStringMultipleBytes)
{
    std::vector<uint8_t> data = {0x01, 0x23, 0x45, 0x67,
                                 0x89, 0xAB, 0xCD, 0xEF};
    auto result = utils::convertHexToString(data, 8);
    EXPECT_EQ(result, "0123456789abcdef");
}

TEST(UtilsTest, ConvertHexToStringZeroPadding)
{
    std::vector<uint8_t> data = {0x00, 0x0F, 0xF0, 0xFF};
    auto result = utils::convertHexToString(data, 4);
    EXPECT_EQ(result, "000ff0ff");
}

// Tests for convertMsgToString function
TEST(UtilsTest, ConvertMsgToStringEmptyBuffer)
{
    std::vector<uint8_t> buffer;
    auto result = utils::convertMsgToString(true, buffer, 0x03, 0x1D);
    EXPECT_EQ(result, "");
}

TEST(UtilsTest, ConvertMsgToStringTxSingleByte)
{
    std::vector<uint8_t> buffer = {0xAB};
    auto result = utils::convertMsgToString(true, buffer, 0x03, 0x1D);
    EXPECT_NE(result.find("EID: 1d"), std::string::npos);
    EXPECT_NE(result.find("TAG: 03"), std::string::npos);
    EXPECT_NE(result.find("Tx:"), std::string::npos);
    EXPECT_NE(result.find("ab"), std::string::npos);
}

TEST(UtilsTest, ConvertMsgToStringRxMultipleBytes)
{
    std::vector<uint8_t> buffer = {0x01, 0x23, 0x45};
    auto result = utils::convertMsgToString(false, buffer, 0x07, 0xAB);
    EXPECT_NE(result.find("EID: ab"), std::string::npos);
    EXPECT_NE(result.find("TAG: 07"), std::string::npos);
    EXPECT_NE(result.find("Rx:"), std::string::npos);
    EXPECT_NE(result.find("01 23 45"), std::string::npos);
}

// Tests for isValidDbusString function
TEST(UtilsTest, IsValidDbusStringAscii)
{
    EXPECT_TRUE(utils::isValidDbusString("ValidString123"));
}

TEST(UtilsTest, IsValidDbusStringEmptyString)
{
    EXPECT_TRUE(utils::isValidDbusString(""));
}

TEST(UtilsTest, IsValidDbusStringUtf8TwoBytes)
{
    // © symbol (U+00A9): 0xC2 0xA9
    EXPECT_TRUE(utils::isValidDbusString("\xC2\xA9"));
}

TEST(UtilsTest, IsValidDbusStringUtf8ThreeBytes)
{
    // € symbol (U+20AC): 0xE2 0x82 0xAC
    EXPECT_TRUE(utils::isValidDbusString("\xE2\x82\xAC"));
}

TEST(UtilsTest, IsValidDbusStringUtf8FourBytes)
{
    // 😀 emoji (U+1F600): 0xF0 0x9F 0x98 0x80
    EXPECT_TRUE(utils::isValidDbusString("\xF0\x9F\x98\x80"));
}

TEST(UtilsTest, IsValidDbusStringInvalidLeadingByte)
{
    // Invalid UTF-8 leading byte 0xFF
    EXPECT_FALSE(utils::isValidDbusString("\xFF"));
}

TEST(UtilsTest, IsValidDbusStringIncompleteTwoByteSequence)
{
    // Incomplete 2-byte sequence (missing second byte)
    EXPECT_FALSE(utils::isValidDbusString("\xC2"));
}

TEST(UtilsTest, IsValidDbusStringInvalidContinuationByte)
{
    // Invalid continuation byte (should be 10xxxxxx, but is 11xxxxxx)
    EXPECT_FALSE(utils::isValidDbusString("\xC2\xC0"));
}

TEST(UtilsTest, IsValidDbusStringOverlongEncoding)
{
    // Overlong encoding for ASCII 'A' (should be 0x41, not 0xC1 0x81)
    EXPECT_FALSE(utils::isValidDbusString("\xC1\x81"));
}

TEST(UtilsTest, IsValidDbusStringNullCodepoint)
{
    // NULL codepoint is invalid in D-Bus strings
    EXPECT_FALSE(utils::isValidDbusString(std::string("\0", 1)));
}

TEST(UtilsTest, IsValidDbusStringCodepointTooLarge)
{
    // Codepoint > 0x10FFFF is invalid
    // 0xF4 0x90 0x80 0x80 would decode to 0x110000
    EXPECT_FALSE(utils::isValidDbusString("\xF4\x90\x80\x80"));
}

// Tests for split function
TEST(UtilsTest, SplitBasicCommaDelimiter)
{
    auto result = utils::split("a,b,c", ",");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsTest, SplitWithSpaceTrimming)
{
    auto result = utils::split(" a , b , c ", ",", " ");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsTest, SplitEmptyString)
{
    auto result = utils::split("", ",");
    EXPECT_TRUE(result.empty());
}

TEST(UtilsTest, SplitNoDelimiterFound)
{
    auto result = utils::split("abc", ",");
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], "abc");
}

TEST(UtilsTest, SplitMultipleConsecutiveDelimiters)
{
    auto result = utils::split("a,,b,,,c", ",");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsTest, SplitLeadingAndTrailingDelimiters)
{
    auto result = utils::split(",a,b,c,", ",");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilsTest, SplitMultiCharacterDelimiter)
{
    auto result = utils::split("a::b::c", "::");
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

// Tests for getCurrentSystemTime function
TEST(UtilsTest, GetCurrentSystemTimeReturnsNonEmpty)
{
    auto result = utils::getCurrentSystemTime();
    EXPECT_FALSE(result.empty());
}

TEST(UtilsTest, GetCurrentSystemTimeContainsDateComponents)
{
    auto result = utils::getCurrentSystemTime();
    // Should contain date separator '-' or '/'
    EXPECT_TRUE(result.find('-') != std::string::npos ||
                result.find('/') != std::string::npos);
}

TEST(UtilsTest, GetCurrentSystemTimeContainsTimeComponents)
{
    auto result = utils::getCurrentSystemTime();
    // Should contain time separator ':'
    EXPECT_NE(result.find(':'), std::string::npos);
}

// Tests for getUUIDFromEID function
TEST(UtilsTest, GetUUIDFromEIDFound)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    eidTable.insert({"uuid-1234", {0x1D, "PCIe", "BindingPCIe"}});
    eidTable.insert({"uuid-5678", {0xAB, "USB", "BindingUSB"}});

    auto result = utils::getUUIDFromEID(eidTable, 0x1D);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "uuid-1234");
}

TEST(UtilsTest, GetUUIDFromEIDNotFound)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    eidTable.insert({"uuid-1234", {0x1D, "PCIe", "BindingPCIe"}});

    auto result = utils::getUUIDFromEID(eidTable, 0xFF);
    EXPECT_FALSE(result.has_value());
}

TEST(UtilsTest, GetUUIDFromEIDEmptyTable)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;

    auto result = utils::getUUIDFromEID(eidTable, 0x1D);
    EXPECT_FALSE(result.has_value());
}

TEST(UtilsTest, GetUUIDFromEIDMultipleEntries)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    eidTable.insert({"uuid-1", {0x01, "PCIe", "BindingPCIe"}});
    eidTable.insert({"uuid-2", {0x02, "USB", "BindingUSB"}});
    eidTable.insert({"uuid-3", {0x03, "SMBus", "BindingSMBus"}});

    auto result = utils::getUUIDFromEID(eidTable, 0x02);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "uuid-2");
}

// Tests for convertUUIDToString function
TEST(UtilsTest, ConvertUUIDToStringValidUUID)
{
    std::vector<uint8_t> uuidInt = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC,
                                    0xDE, 0xF0, 0x11, 0x22, 0x33, 0x44,
                                    0x55, 0x66, 0x77, 0x88};
    auto result = utils::convertUUIDToString(uuidInt);
    // Use c_str() to handle null terminator correctly
    EXPECT_STREQ(result.c_str(), "12345678-9abc-def0-1122-334455667788");
}

TEST(UtilsTest, ConvertUUIDToStringInvalidSize)
{
    std::vector<uint8_t> uuidInt = {0x12, 0x34, 0x56}; // Too short
    auto result = utils::convertUUIDToString(uuidInt);
    EXPECT_STREQ(result.c_str(), ""); // Should return empty string on error
}

TEST(UtilsTest, ConvertUUIDToStringAllZeros)
{
    std::vector<uint8_t> uuidInt(16, 0x00);
    auto result = utils::convertUUIDToString(uuidInt);
    EXPECT_STREQ(result.c_str(), "00000000-0000-0000-0000-000000000000");
}

TEST(UtilsTest, ConvertUUIDToStringAllFFs)
{
    std::vector<uint8_t> uuidInt(16, 0xFF);
    auto result = utils::convertUUIDToString(uuidInt);
    EXPECT_STREQ(result.c_str(), "ffffffff-ffff-ffff-ffff-ffffffffffff");
}

// Tests for bitmapToIndices function
// Converts bitmap (bit array) to pair of (zero indices, one indices)

TEST(UtilsTest, BitmapToIndicesEmptyBitmap)
{
    std::vector<uint8_t> bitmap = {};
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_TRUE(zeroIndices.empty());
    EXPECT_TRUE(oneIndices.empty());
}

TEST(UtilsTest, BitmapToIndicesAllZeros)
{
    std::vector<uint8_t> bitmap = {0x00, 0x00};
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_EQ(zeroIndices.size(), 16); // 2 bytes * 8 bits
    EXPECT_TRUE(oneIndices.empty());
    // Verify all indices 0-15 are in zeroIndices
    for (uint8_t i = 0; i < 16; i++)
    {
        EXPECT_EQ(zeroIndices[i], i);
    }
}

TEST(UtilsTest, BitmapToIndicesAllOnes)
{
    std::vector<uint8_t> bitmap = {0xFF, 0xFF};
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_TRUE(zeroIndices.empty());
    EXPECT_EQ(oneIndices.size(), 16);
    // Verify all indices 0-15 are in oneIndices
    for (uint8_t i = 0; i < 16; i++)
    {
        EXPECT_EQ(oneIndices[i], i);
    }
}

TEST(UtilsTest, BitmapToIndicesSingleBit)
{
    std::vector<uint8_t> bitmap = {0x01}; // Bit 0 set
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_EQ(zeroIndices.size(), 7);     // Bits 1-7 are zero
    EXPECT_EQ(oneIndices.size(), 1);
    EXPECT_EQ(oneIndices[0], 0);
}

TEST(UtilsTest, BitmapToIndicesMixedBits)
{
    std::vector<uint8_t> bitmap = {0b10101010}; // Alternating bits
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_EQ(zeroIndices.size(), 4); // Bits 0,2,4,6 are zero (LSB first)
    EXPECT_EQ(oneIndices.size(), 4);  // Bits 1,3,5,7 are one
    // Bitmap is processed LSB first
    EXPECT_EQ(zeroIndices[0], 0);
    EXPECT_EQ(oneIndices[0], 1);
    EXPECT_EQ(zeroIndices[1], 2);
    EXPECT_EQ(oneIndices[1], 3);
}

TEST(UtilsTest, BitmapToIndicesMultipleBytes)
{
    std::vector<uint8_t> bitmap = {
        0x0F, 0xF0}; // First byte: low nibble, second byte: high nibble
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_EQ(zeroIndices.size(), 8);
    EXPECT_EQ(oneIndices.size(), 8);
    // First byte 0x0F: bits 0-3 are one, bits 4-7 are zero
    // Second byte 0xF0: bits 8-11 are zero, bits 12-15 are one
}

// Tests for indicesToBitmap function
// Converts list of bit indices to bitmap representation

TEST(UtilsTest, IndicesToBitmapEmptyIndices)
{
    std::vector<uint8_t> indices = {};
    auto bitmap = utils::indicesToBitmap(indices, 2);
    EXPECT_EQ(bitmap.size(), 2);
    EXPECT_EQ(bitmap[0], 0x00);
    EXPECT_EQ(bitmap[1], 0x00);
}

TEST(UtilsTest, IndicesToBitmapSingleIndex)
{
    std::vector<uint8_t> indices = {0};
    auto bitmap = utils::indicesToBitmap(indices, 1);
    EXPECT_EQ(bitmap.size(), 1);
    EXPECT_EQ(bitmap[0], 0x01); // Bit 0 set (LSB)
}

TEST(UtilsTest, IndicesToBitmapMultipleIndicesSameByte)
{
    std::vector<uint8_t> indices = {0, 2, 4, 6};
    auto bitmap = utils::indicesToBitmap(indices, 1);
    EXPECT_EQ(bitmap.size(), 1);
    EXPECT_EQ(bitmap[0], 0b01010101); // Bits 0,2,4,6 set
}

TEST(UtilsTest, IndicesToBitmapMultipleIndicesMultipleBytes)
{
    std::vector<uint8_t> indices = {0, 8, 15};
    auto bitmap = utils::indicesToBitmap(indices, 2);
    EXPECT_EQ(bitmap.size(), 2);
    EXPECT_EQ(bitmap[0], 0x01); // Bit 0 set
    EXPECT_EQ(bitmap[1], 0x81); // Bits 8 and 15 set (0x01 | 0x80)
}

TEST(UtilsTest, IndicesToBitmapSizeZero)
{
    std::vector<uint8_t> indices = {0, 7, 15};
    auto bitmap = utils::indicesToBitmap(indices, 0); // Size 0 means auto-size
    EXPECT_EQ(bitmap.size(), 2); // Should size to fit index 15 (0-15 = 2 bytes)
    EXPECT_EQ(bitmap[0], 0x81);  // Bits 0 and 7 set
    EXPECT_EQ(bitmap[1], 0x80);  // Bit 15 set
}

TEST(UtilsTest, IndicesToBitmapMaxSize)
{
    std::vector<uint8_t> indices = {0, 63}; // Max index for 8 bytes
    auto bitmap = utils::indicesToBitmap(indices, 8);
    EXPECT_EQ(bitmap.size(), 8);
    EXPECT_EQ(bitmap[0], 0x01); // Bit 0 set
    EXPECT_EQ(bitmap[7], 0x80); // Bit 63 set
}

TEST(UtilsTest, IndicesToBitmapSizeExceedsMax)
{
    std::vector<uint8_t> indices = {0};
    EXPECT_THROW(utils::indicesToBitmap(indices, 9), std::invalid_argument);
}

TEST(UtilsTest, IndicesToBitmapIndexOutOfBounds)
{
    std::vector<uint8_t> indices = {
        0, 16}; // Index 16 exceeds 2-byte bitmap (0-15)
    EXPECT_THROW(utils::indicesToBitmap(indices, 2), std::invalid_argument);
}

TEST(UtilsTest, IndicesToBitmapRoundTrip)
{
    // Test that bitmapToIndices(indicesToBitmap(x)) == x
    std::vector<uint8_t> originalIndices = {1, 3, 5, 10, 15};
    auto bitmap = utils::indicesToBitmap(originalIndices, 2);
    auto [zeroIndices, oneIndices] = utils::bitmapToIndices(bitmap);
    EXPECT_EQ(oneIndices.size(), originalIndices.size());
    for (size_t i = 0; i < originalIndices.size(); i++)
    {
        EXPECT_EQ(oneIndices[i], originalIndices[i]);
    }
}

// Tests for getEidFromUUID function
// Searches multimap for UUID and returns corresponding EID

TEST(UtilsTest, GetEidFromUUIDFound)
{
    // Create test EID table with UUID -> (EID, Medium, Binding) mappings
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> eidTable;
    uuid_t uuid1 = "12345678-1234-5678-1234-567812345678";
    uuid_t uuid2 = "87654321-4321-8765-4321-876543218765";

    eidTable.insert({uuid1, std::make_tuple(0x10, "PCIe", "PCIe")});
    eidTable.insert({uuid2, std::make_tuple(0x20, "USB", "USB")});

    eid_t result = utils::getEidFromUUID(eidTable, uuid1);
    EXPECT_EQ(result, 0x10);
}

TEST(UtilsTest, GetEidFromUUIDNotFound)
{
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> eidTable;
    uuid_t uuid1 = "12345678-1234-5678-1234-567812345678";
    uuid_t searchUuid = "00000000-0000-0000-0000-000000000000";

    eidTable.insert({uuid1, std::make_tuple(0x10, "PCIe", "PCIe")});

    eid_t result = utils::getEidFromUUID(eidTable, searchUuid);
    EXPECT_EQ(
        result,
        std::numeric_limits<uint8_t>::max()); // Max value indicates not found
}

TEST(UtilsTest, GetEidFromUUIDEmptyTable)
{
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> eidTable;
    uuid_t searchUuid = "12345678-1234-5678-1234-567812345678";

    eid_t result = utils::getEidFromUUID(eidTable, searchUuid);
    EXPECT_EQ(result, std::numeric_limits<uint8_t>::max());
}

TEST(UtilsTest, GetEidFromUUIDMultipleEntries)
{
    // Test that it returns the first matching UUID's EID
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> eidTable;
    uuid_t uuid = "12345678-1234-5678-1234-567812345678";

    eidTable.insert({uuid, std::make_tuple(0x10, "PCIe", "PCIe")});
    eidTable.insert(
        {uuid, std::make_tuple(0x20, "USB", "USB")}); // Duplicate UUID

    eid_t result = utils::getEidFromUUID(eidTable, uuid);
    EXPECT_EQ(result, 0x10); // Should return first match
}

TEST(UtilsTest, GetEidFromUUIDPartialMatch)
{
    // Function uses starts_with for UUID matching (length UUID_LEN)
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> eidTable;
    uuid_t storedUuid = "12345678-1234-5678-1234-567812345678";
    uuid_t searchPrefix = "12345678-1234-5678-1234-567812345678"; // Exact match

    eidTable.insert({storedUuid, std::make_tuple(0x15, "PCIe", "PCIe")});

    eid_t result = utils::getEidFromUUID(eidTable, searchPrefix);
    EXPECT_EQ(result, 0x15);
}

// Tests for convertBitMaskToVector function
// Converts bitfield8_t array to vector of set bit indices

TEST(UtilsTest, ConvertBitMaskToVectorAllZeros)
{
    bitfield8_t mask[2] = {{.byte = 0x00}, {.byte = 0x00}};
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 2);
    EXPECT_TRUE(data.empty()); // No bits set
}

TEST(UtilsTest, ConvertBitMaskToVectorAllOnes)
{
    bitfield8_t mask[2] = {{.byte = 0xFF}, {.byte = 0xFF}};
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 2);
    EXPECT_EQ(data.size(), 16); // All 16 bits set
    // Verify all indices 0-15 are present
    for (uint8_t i = 0; i < 16; i++)
    {
        EXPECT_EQ(data[i], i);
    }
}

TEST(UtilsTest, ConvertBitMaskToVectorSingleBit)
{
    bitfield8_t mask[1] = {{.byte = 0x01}}; // Only bit0 set
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 1);
    EXPECT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], 0);
}

TEST(UtilsTest, ConvertBitMaskToVectorMultipleBitsSameByte)
{
    bitfield8_t mask[1] = {{.byte = 0b10101010}}; // Bits 1,3,5,7 set
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 1);
    EXPECT_EQ(data.size(), 4);
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[1], 3);
    EXPECT_EQ(data[2], 5);
    EXPECT_EQ(data[3], 7);
}

TEST(UtilsTest, ConvertBitMaskToVectorMultipleBytes)
{
    bitfield8_t mask[2] = {{.byte = 0x0F}, {.byte = 0xF0}};
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 2);
    EXPECT_EQ(data.size(), 8);
    // First byte 0x0F: bits 0,1,2,3 set
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 1);
    EXPECT_EQ(data[2], 2);
    EXPECT_EQ(data[3], 3);
    // Second byte 0xF0: bits 12,13,14,15 set (byte 1 offset = 8)
    EXPECT_EQ(data[4], 12);
    EXPECT_EQ(data[5], 13);
    EXPECT_EQ(data[6], 14);
    EXPECT_EQ(data[7], 15);
}

TEST(UtilsTest, ConvertBitMaskToVectorUsingBitfieldStructure)
{
    // Test using the bits structure directly
    bitfield8_t mask[1] = {};
    mask[0].bits.bit0 = 1;
    mask[0].bits.bit3 = 1;
    mask[0].bits.bit7 = 1;

    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 1);
    EXPECT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 3);
    EXPECT_EQ(data[2], 7);
}

TEST(UtilsTest, ConvertBitMaskToVectorSizeZero)
{
    bitfield8_t mask[1] = {{.byte = 0xFF}};
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask,
                                  0); // Size 0 means process nothing
    EXPECT_TRUE(data.empty());
}

TEST(UtilsTest, ConvertBitMaskToVectorLargeArray)
{
    bitfield8_t mask[4] = {
        {.byte = 0x01}, // Bit 0 set
        {.byte = 0x00}, // No bits
        {.byte = 0x80}, // Bit 23 set (byte 2, bit 7)
        {.byte = 0x01}  // Bit 24 set (byte 3, bit 0)
    };
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, mask, 4);
    EXPECT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 23);
    EXPECT_EQ(data[2], 24);
}

TEST(UtilsTest, ConvertBitMaskToVectorAppendToExisting)
{
    bitfield8_t mask[1] = {{.byte = 0x03}}; // Bits 0,1 set
    std::vector<uint8_t> data = {99, 100};  // Pre-existing data
    utils::convertBitMaskToVector(data, mask, 1);
    EXPECT_EQ(data.size(), 4);
    EXPECT_EQ(data[0], 99);  // Original data preserved
    EXPECT_EQ(data[1], 100); // Original data preserved
    EXPECT_EQ(data[2], 0);   // New data appended
    EXPECT_EQ(data[3], 1);   // New data appended
}

// ===========================================================================
// Tests for typeName<T>() template function
// ===========================================================================

TEST(UtilsTest, TypeNamePrimitiveTypes)
{
    // Test fundamental types
    EXPECT_EQ(utils::typeName<int>(), "int");
    EXPECT_EQ(utils::typeName<unsigned int>(), "unsigned int");
    EXPECT_EQ(utils::typeName<long>(), "long");
    EXPECT_EQ(utils::typeName<unsigned long>(), "unsigned long");
    EXPECT_EQ(utils::typeName<float>(), "float");
    EXPECT_EQ(utils::typeName<double>(), "double");
    EXPECT_EQ(utils::typeName<bool>(), "bool");
    EXPECT_EQ(utils::typeName<char>(), "char");
}

TEST(UtilsTest, TypeNameStdTypes)
{
    // Test standard library types
    EXPECT_EQ(utils::typeName<std::string>(),
              "std::__cxx11::basic_string<char, std::char_traits<char>, "
              "std::allocator<char> >");
    EXPECT_EQ(utils::typeName<std::vector<int>>(),
              "std::vector<int, std::allocator<int> >");
    EXPECT_EQ(utils::typeName<std::vector<uint8_t>>(),
              "std::vector<unsigned char, std::allocator<unsigned char> >");
}

TEST(UtilsTest, TypeNameFixedWidthIntegers)
{
    // Test fixed-width integer types from <cstdint>
    EXPECT_EQ(utils::typeName<uint8_t>(), "unsigned char");
    EXPECT_EQ(utils::typeName<int8_t>(), "signed char");
    EXPECT_EQ(utils::typeName<uint16_t>(), "unsigned short");
    EXPECT_EQ(utils::typeName<int16_t>(), "short");
    EXPECT_EQ(utils::typeName<uint32_t>(), "unsigned int");
    EXPECT_EQ(utils::typeName<int32_t>(), "int");
    EXPECT_EQ(utils::typeName<uint64_t>(), "unsigned long");
    EXPECT_EQ(utils::typeName<int64_t>(), "long");
}

TEST(UtilsTest, TypeNamePointerTypes)
{
    // Test pointer types
    EXPECT_EQ(utils::typeName<int*>(), "int*");
    EXPECT_EQ(utils::typeName<const char*>(), "char const*");
    EXPECT_EQ(utils::typeName<void*>(), "void*");
}

TEST(UtilsTest, TypeNameConstTypes)
{
    // Test const-qualified types
    EXPECT_EQ(utils::typeName<const int>(), "int");
    EXPECT_EQ(utils::typeName<const double>(), "double");
}

TEST(UtilsTest, TypeNameReferenceTypes)
{
    // Test reference types (references are stripped by typeid)
    EXPECT_EQ(utils::typeName<int&>(), "int");
    EXPECT_EQ(utils::typeName<const std::string&>(),
              "std::__cxx11::basic_string<char, std::char_traits<char>, "
              "std::allocator<char> >");
}

TEST(UtilsTest, TypeNameComplexTypes)
{
    // Test more complex nested types
    using ComplexType = std::vector<std::pair<int, std::string>>;
    std::string result = utils::typeName<ComplexType>();
    // Just verify it contains expected components
    EXPECT_NE(result.find("vector"), std::string::npos);
    EXPECT_NE(result.find("pair"), std::string::npos);
}

TEST(UtilsTest, TypeNameTupleType)
{
    // Test tuple type
    using TupleType = std::tuple<int, double, std::string>;
    std::string result = utils::typeName<TupleType>();
    EXPECT_NE(result.find("tuple"), std::string::npos);
}

TEST(UtilsTest, TypeNameOptionalType)
{
    // Test std::optional
    using OptType = std::optional<int>;
    std::string result = utils::typeName<OptType>();
    EXPECT_NE(result.find("optional"), std::string::npos);
}

TEST(UtilsTest, TypeNameCustomStructs)
{
    // Test with custom types defined in project
    struct TestStruct
    {
        int x;
        double y;
    };

    std::string result = utils::typeName<TestStruct>();
    // Should contain the struct name
    EXPECT_NE(result.find("TestStruct"), std::string::npos);
}

// ===========================================================================
// Tests for getPropertyFromCollection<T>() template function
// ===========================================================================

TEST(UtilsTest, GetPropertyFromCollectionBoolType)
{
    // Test with bool type
    PropertyValuesCollection collection = {
        {"property_a", false}, {"property_b", true}, {"property_c", false}};

    auto result = utils::getPropertyFromCollection<bool>(collection,
                                                         "property_b");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), true);

    // Test not found case
    auto notFound = utils::getPropertyFromCollection<bool>(collection,
                                                           "property_z");
    EXPECT_FALSE(notFound.has_value());
}

TEST(UtilsTest, GetPropertyFromCollectionIntegerTypes)
{
    // Test with various integer types from PropertyValue variant
    PropertyValuesCollection collection = {
        {"prop_int16", static_cast<int16_t>(100)},
        {"prop_int32", static_cast<int32_t>(200)},
        {"prop_int64", static_cast<int64_t>(300)},
        {"prop_uint16", static_cast<uint16_t>(400)},
        {"prop_uint32", static_cast<uint32_t>(500)},
        {"prop_uint64", static_cast<uint64_t>(600)},
        {"prop_uint8", static_cast<uint8_t>(50)}};

    auto int16_result = utils::getPropertyFromCollection<int16_t>(collection,
                                                                  "prop_int16");
    ASSERT_TRUE(int16_result.has_value());
    EXPECT_EQ(int16_result.value(), 100);

    auto int32_result = utils::getPropertyFromCollection<int32_t>(collection,
                                                                  "prop_int32");
    ASSERT_TRUE(int32_result.has_value());
    EXPECT_EQ(int32_result.value(), 200);

    auto int64_result = utils::getPropertyFromCollection<int64_t>(collection,
                                                                  "prop_int64");
    ASSERT_TRUE(int64_result.has_value());
    EXPECT_EQ(int64_result.value(), 300);

    auto uint16_result =
        utils::getPropertyFromCollection<uint16_t>(collection, "prop_uint16");
    ASSERT_TRUE(uint16_result.has_value());
    EXPECT_EQ(uint16_result.value(), 400);

    auto uint32_result =
        utils::getPropertyFromCollection<uint32_t>(collection, "prop_uint32");
    ASSERT_TRUE(uint32_result.has_value());
    EXPECT_EQ(uint32_result.value(), 500);

    auto uint64_result =
        utils::getPropertyFromCollection<uint64_t>(collection, "prop_uint64");
    ASSERT_TRUE(uint64_result.has_value());
    EXPECT_EQ(uint64_result.value(), 600);

    auto uint8_result = utils::getPropertyFromCollection<uint8_t>(collection,
                                                                  "prop_uint8");
    ASSERT_TRUE(uint8_result.has_value());
    EXPECT_EQ(uint8_result.value(), 50);
}

TEST(UtilsTest, GetPropertyFromCollectionDoubleType)
{
    // Test with double type
    PropertyValuesCollection collection = {
        {"temperature", 25.5}, {"voltage", 3.3}, {"current", 1.2}};

    auto result = utils::getPropertyFromCollection<double>(collection,
                                                           "voltage");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value(), 3.3);
}

TEST(UtilsTest, GetPropertyFromCollectionStringType)
{
    // Test with std::string type
    PropertyValuesCollection collection = {
        {"name", std::string("sensor1")},
        {"status", std::string("active")},
        {"type", std::string("temperature")}};

    auto result = utils::getPropertyFromCollection<std::string>(collection,
                                                                "status");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "active");

    // Test not found
    auto notFound = utils::getPropertyFromCollection<std::string>(collection,
                                                                  "missing");
    EXPECT_FALSE(notFound.has_value());
}

TEST(UtilsTest, GetPropertyFromCollectionVectorStringType)
{
    // Test with std::vector<std::string> type
    std::vector<std::string> sensorNames = {"sensor1", "sensor2", "sensor3"};
    PropertyValuesCollection collection = {{"sensors", sensorNames}};

    auto result = utils::getPropertyFromCollection<std::vector<std::string>>(
        collection, "sensors");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 3);
    EXPECT_EQ(result.value()[0], "sensor1");
    EXPECT_EQ(result.value()[1], "sensor2");
    EXPECT_EQ(result.value()[2], "sensor3");
}

TEST(UtilsTest, GetPropertyFromCollectionVectorUint64Type)
{
    // Test with std::vector<uint64_t> type
    std::vector<uint64_t> values = {100, 200, 300, 400};
    PropertyValuesCollection collection = {{"data_points", values}};

    auto result = utils::getPropertyFromCollection<std::vector<uint64_t>>(
        collection, "data_points");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().size(), 4);
    EXPECT_EQ(result.value()[0], 100);
    EXPECT_EQ(result.value()[3], 400);
}

TEST(UtilsTest, GetPropertyFromCollectionNotFoundReturnsNullopt)
{
    // Test that missing properties return std::nullopt
    PropertyValuesCollection collection = {{"existing_prop", 42}};

    auto result = utils::getPropertyFromCollection<int32_t>(collection,
                                                            "non_existent");
    EXPECT_FALSE(result.has_value());
}

TEST(UtilsTest, GetPropertyFromCollectionEmptyCollection)
{
    // Test with empty collection
    PropertyValuesCollection emptyCollection;

    auto result = utils::getPropertyFromCollection<int32_t>(emptyCollection,
                                                            "any_prop");
    EXPECT_FALSE(result.has_value());
}

TEST(UtilsTest, GetPropertyFromCollectionSortedLookup)
{
    // Test that lower_bound search works correctly with sorted collection
    // Properties should be in alphabetical order for std::lower_bound
    PropertyValuesCollection collection = {
        {"aaa", 1}, {"bbb", 2}, {"ccc", 3}, {"ddd", 4}, {"eee", 5}};

    // Test finding first element
    auto first = utils::getPropertyFromCollection<int32_t>(collection, "aaa");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first.value(), 1);

    // Test finding middle element
    auto middle = utils::getPropertyFromCollection<int32_t>(collection, "ccc");
    ASSERT_TRUE(middle.has_value());
    EXPECT_EQ(middle.value(), 3);

    // Test finding last element
    auto last = utils::getPropertyFromCollection<int32_t>(collection, "eee");
    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last.value(), 5);

    // Test not found (before first)
    auto before = utils::getPropertyFromCollection<int32_t>(collection, "000");
    EXPECT_FALSE(before.has_value());

    // Test not found (after last)
    auto after = utils::getPropertyFromCollection<int32_t>(collection, "zzz");
    EXPECT_FALSE(after.has_value());

    // Test not found (between existing)
    auto between = utils::getPropertyFromCollection<int32_t>(collection, "abc");
    EXPECT_FALSE(between.has_value());
}

// ===========================================================================
// Tests for getAssociations() function
// ===========================================================================

TEST(UtilsTest, GetAssociationsEmptyVector)
{
    // Test with empty vector
    std::vector<utils::Association> emptyAssociations;

    auto result = utils::getAssociations(emptyAssociations);

    EXPECT_TRUE(result.empty());
    EXPECT_EQ(result.size(), 0);
}

TEST(UtilsTest, GetAssociationsSingleElement)
{
    // Test with single association
    std::vector<utils::Association> associations = {
        {"contains", "contained_by", "/xyz/openbmc_project/inventory/system"}};

    auto result = utils::getAssociations(associations);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(std::get<0>(result[0]), "contains");
    EXPECT_EQ(std::get<1>(result[0]), "contained_by");
    EXPECT_EQ(std::get<2>(result[0]), "/xyz/openbmc_project/inventory/system");
}

TEST(UtilsTest, GetAssociationsMultipleElements)
{
    // Test with multiple associations
    std::vector<utils::Association> associations = {
        {"contains", "contained_by", "/xyz/openbmc_project/inventory/system"},
        {"cooled_by", "cooling", "/xyz/openbmc_project/inventory/cooling"},
        {"powered_by", "powering", "/xyz/openbmc_project/inventory/power"}};

    auto result = utils::getAssociations(associations);

    ASSERT_EQ(result.size(), 3);

    // Check first association
    EXPECT_EQ(std::get<0>(result[0]), "contains");
    EXPECT_EQ(std::get<1>(result[0]), "contained_by");
    EXPECT_EQ(std::get<2>(result[0]), "/xyz/openbmc_project/inventory/system");

    // Check second association
    EXPECT_EQ(std::get<0>(result[1]), "cooled_by");
    EXPECT_EQ(std::get<1>(result[1]), "cooling");
    EXPECT_EQ(std::get<2>(result[1]), "/xyz/openbmc_project/inventory/cooling");

    // Check third association
    EXPECT_EQ(std::get<0>(result[2]), "powered_by");
    EXPECT_EQ(std::get<1>(result[2]), "powering");
    EXPECT_EQ(std::get<2>(result[2]), "/xyz/openbmc_project/inventory/power");
}

TEST(UtilsTest, GetAssociationsPreservesOrder)
{
    // Test that order is preserved
    std::vector<utils::Association> associations = {
        {"first", "first_back", "/path/first"},
        {"second", "second_back", "/path/second"},
        {"third", "third_back", "/path/third"}};

    auto result = utils::getAssociations(associations);

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(std::get<0>(result[0]), "first");
    EXPECT_EQ(std::get<0>(result[1]), "second");
    EXPECT_EQ(std::get<0>(result[2]), "third");
}

TEST(UtilsTest, GetAssociationsWithEmptyStrings)
{
    // Test with empty strings (edge case but valid)
    std::vector<utils::Association> associations = {{"", "", ""}};

    auto result = utils::getAssociations(associations);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(std::get<0>(result[0]), "");
    EXPECT_EQ(std::get<1>(result[0]), "");
    EXPECT_EQ(std::get<2>(result[0]), "");
}

TEST(UtilsTest, GetAssociationsWithSpecialCharacters)
{
    // Test with special characters in strings
    std::vector<utils::Association> associations = {
        {"forward/path", "backward\\path", "/special/!@#$%^&*()"}};

    auto result = utils::getAssociations(associations);

    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(std::get<0>(result[0]), "forward/path");
    EXPECT_EQ(std::get<1>(result[0]), "backward\\path");
    EXPECT_EQ(std::get<2>(result[0]), "/special/!@#$%^&*()");
}
