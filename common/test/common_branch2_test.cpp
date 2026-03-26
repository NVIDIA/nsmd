/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests (batch 2) for common/utils.cpp
 * Targets half-covered branches in:
 *   - isValidDbusString (continuation byte checks, overlong checks)
 *   - split() with trimStr edge cases
 *   - parseStaticUuid boundary cases
 *   - convertMacAddressToString edge cases
 *   - convertBitMaskToVector individual bit branches
 *   - indicesToBitmap error and boundary paths
 *   - readFdToBuffer / writeBufferToFd / appendBufferToFd fd paths
 *   - CustomFD write/read boundary paths
 *   - vectorTo256BitHexString
 *   - bitfield256_tToBitMap / bitfield256_tToBitArray / bitMapToBitfield256_t
 *   - nsmCompletionCodeToString / nsmReasonCodeToString all switch cases
 *   - nsmSwCodeToString all switch cases
 *   - int64ToDoubleSafeConvert positive overflow
 *   - updateMethodsBitfieldToList individual bit branches
 *   - isPreferred medium priority branches
 *   - getEidFromUUID match vs no-match
 *   - convertHexToString
 *   - requestMsgToHexString
 *   - Bitfield256::getSetBits empty case
 */

#include "utils.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace utils;

// ============================================================================
// isValidDbusString — cover each numBytes branch and continuation checks
// ============================================================================

// 2-byte sequence with invalid continuation byte (second byte not 10xxxxxx)
TEST(IsValidDbusStringBranch3, TwoByte_BadContinuation)
{
    // C3 followed by 0x40 (01xxxxxx instead of 10xxxxxx)
    EXPECT_FALSE(isValidDbusString("\xC3\x40"));
}

// 3-byte sequence with bad second continuation byte
TEST(IsValidDbusStringBranch3, ThreeByte_BadSecondContinuation)
{
    // E2 82 followed by 0x40 instead of 10xxxxxx
    EXPECT_FALSE(isValidDbusString("\xE2\x82\x40"));
}

// 3-byte sequence with bad first continuation byte
TEST(IsValidDbusStringBranch3, ThreeByte_BadFirstContinuation)
{
    // E2 followed by 0x40 instead of 10xxxxxx
    EXPECT_FALSE(isValidDbusString("\xE2\x40\x80"));
}

// 4-byte sequence with bad third continuation byte
TEST(IsValidDbusStringBranch3, FourByte_BadThirdContinuation)
{
    EXPECT_FALSE(isValidDbusString("\xF0\x90\x80\x40"));
}

// 4-byte sequence with bad second continuation byte
TEST(IsValidDbusStringBranch3, FourByte_BadSecondContinuation)
{
    EXPECT_FALSE(isValidDbusString("\xF0\x90\x40\x80"));
}

// 4-byte sequence with bad first continuation byte
TEST(IsValidDbusStringBranch3, FourByte_BadFirstContinuation)
{
    EXPECT_FALSE(isValidDbusString("\xF0\x40\x80\x80"));
}

// Invalid leading byte (0xFF is never valid in UTF-8)
TEST(IsValidDbusStringBranch3, InvalidLeadingByte_FF)
{
    EXPECT_FALSE(isValidDbusString("\xFF"));
}

// Empty string is valid (while loop body not entered)
TEST(IsValidDbusStringBranch3, EmptyString_ReturnsTrue)
{
    EXPECT_TRUE(isValidDbusString(""));
}

// Valid ASCII string (1-byte path exercised multiple times)
TEST(IsValidDbusStringBranch3, PureAscii_ReturnsTrue)
{
    EXPECT_TRUE(isValidDbusString("Hello World 123!@#"));
}

// Valid 2-byte at max boundary: U+07FF = DF BF
TEST(IsValidDbusStringBranch3, Valid2ByteMax)
{
    EXPECT_TRUE(isValidDbusString("\xDF\xBF"));
}

// Valid 3-byte at max boundary: U+FFFF = EF BF BF
TEST(IsValidDbusStringBranch3, Valid3ByteMax)
{
    EXPECT_TRUE(isValidDbusString("\xEF\xBF\xBF"));
}

// ============================================================================
// split() — additional trimStr edge cases
// ============================================================================

// Empty source string yields empty result
TEST(SplitBranch3, EmptySource_EmptyResult)
{
    auto result = split("", ",");
    EXPECT_TRUE(result.empty());
}

// Trim only affects edges, not middle characters
TEST(SplitBranch3, TrimEdges_LeavesMiddle)
{
    // "aahelloa" with trimStr "a" => "hello"
    auto result = split("aahelloa,aworlda", ",", "a");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
}

// No trimStr (empty) => no trimming path
TEST(SplitBranch3, NoTrimStr_NoTrimming)
{
    auto result = split("  hello  , world ", ",", "");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "  hello  ");
    EXPECT_EQ(result[1], " world ");
}

// ============================================================================
// parseStaticUuid — cover all boundary return codes
// ============================================================================

// Boundary: n1 exactly 0xFFFF (max valid)
TEST(ParseStaticUuidBranch3, CombinedMaxValid_ReturnsSuccess)
{
    uuid_t uuid = "STATIC:65535:0:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), 0);
    EXPECT_EQ(dt, 0xFF);
    EXPECT_EQ(dr, 0xFF);
}

// Boundary: n2 exactly 0xFF (max valid)
TEST(ParseStaticUuidBranch3, InstanceMaxValid_ReturnsSuccess)
{
    uuid_t uuid = "STATIC:0:255:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), 0);
    EXPECT_EQ(in, 255);
}

// Boundary: n2 == 256 (just above max)
TEST(ParseStaticUuidBranch3, Instance256_ReturnsNeg2)
{
    uuid_t uuid = "STATIC:0:256:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -2);
}

// Completely bogus string
TEST(ParseStaticUuidBranch3, GarbageInput_ReturnsNeg3)
{
    uuid_t uuid = "GARBAGE";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -3);
}

// ============================================================================
// convertMacAddressToString — edge cases
// ============================================================================

// Zero-length data
TEST(ConvertMacAddressBranch3, ZeroLength_ClearsString)
{
    uint8_t mac[1] = {0};
    std::string result = "old";
    convertMacAddressToString(mac, 0, result);
    EXPECT_TRUE(result.empty());
}

// Exactly MAC_ADDRESS_DATA_LEN - 1 (5 bytes) is too short
TEST(ConvertMacAddressBranch3, FiveBytesTooShort_ClearsString)
{
    uint8_t mac[5] = {1, 2, 3, 4, 5};
    std::string result = "old";
    convertMacAddressToString(mac, 5, result);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// convertBitMaskToVector — exercise each individual bit branch
// ============================================================================

TEST(ConvertBitMaskBranch3, Bit0Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x01;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 0);
}

TEST(ConvertBitMaskBranch3, Bit1Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x02;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 1);
}

TEST(ConvertBitMaskBranch3, Bit2Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x04;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 2);
}

TEST(ConvertBitMaskBranch3, Bit3Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x08;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 3);
}

TEST(ConvertBitMaskBranch3, Bit4Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x10;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 4);
}

TEST(ConvertBitMaskBranch3, Bit5Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x20;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 5);
}

TEST(ConvertBitMaskBranch3, Bit6Only)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x40;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 6);
}

TEST(ConvertBitMaskBranch3, AllBitsSet)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0xFF;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 8u);
    for (uint8_t i = 0; i < 8; i++)
    {
        EXPECT_EQ(data[i], i);
    }
}

TEST(ConvertBitMaskBranch3, NoBitsSet)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x00;
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 1);
    EXPECT_TRUE(data.empty());
}

// Two bytes: second byte has only bit7
TEST(ConvertBitMaskBranch3, SecondByte_Bit7)
{
    bitfield8_t value[2] = {};
    value[0].byte = 0x00;
    value[1].byte = 0x80; // bit7 of second byte = index 15
    std::vector<uint8_t> data;
    convertBitMaskToVector(data, value, 2);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 15);
}

// ============================================================================
// indicesToBitmap — error and boundary paths
// ============================================================================

TEST(IndicesToBitmapBranch3, SizeTooLarge_Throws)
{
    std::vector<uint8_t> indices = {0};
    EXPECT_THROW(indicesToBitmap(indices, 9), std::invalid_argument);
}

TEST(IndicesToBitmapBranch3, IndexOutOfBounds_Throws)
{
    // size=1 means max index=7. Index 8 is out of bounds.
    std::vector<uint8_t> indices = {8};
    EXPECT_THROW(indicesToBitmap(indices, 1), std::invalid_argument);
}

TEST(IndicesToBitmapBranch3, EmptyIndices_ReturnsZeroBitmap)
{
    auto bitmap = indicesToBitmap({}, 4);
    EXPECT_EQ(bitmap.size(), 4u);
    for (auto b : bitmap)
    {
        EXPECT_EQ(b, 0);
    }
}

TEST(IndicesToBitmapBranch3, SizeZero_AutoSizes)
{
    // size=0 means auto-size from max index
    std::vector<uint8_t> indices = {0, 7};
    auto bitmap = indicesToBitmap(indices, 0);
    EXPECT_EQ(bitmap.size(), 1u);
    EXPECT_EQ(bitmap[0], 0x81); // bit0 and bit7
}

TEST(IndicesToBitmapBranch3, SizeZero_LargerIndex)
{
    std::vector<uint8_t> indices = {15};
    auto bitmap = indicesToBitmap(indices, 0);
    EXPECT_EQ(bitmap.size(), 2u);
    EXPECT_EQ(bitmap[1], 0x80); // bit7 of second byte
}

// ============================================================================
// readFdToBuffer — invalid fd
// ============================================================================

TEST(ReadFdToBufferBranch3, NegativeFd_Throws)
{
    std::vector<uint8_t> buffer;
    EXPECT_THROW(readFdToBuffer(-1, buffer), std::runtime_error);
}

// readFdToBuffer — successful read
TEST(ReadFdToBufferBranch3, ValidFd_ReadsData)
{
    int fd = memfd_create("test_read_ok", 0);
    ASSERT_GE(fd, 0);
    std::vector<uint8_t> writeData = {0xAA, 0xBB, 0xCC};
    if (::write(fd, writeData.data(), writeData.size()) < 0)
    {}

    std::vector<uint8_t> buffer;
    readFdToBuffer(fd, buffer);
    EXPECT_EQ(buffer, writeData);
    close(fd);
}

// ============================================================================
// writeBufferToFd — invalid fd
// ============================================================================

TEST(WriteBufferToFdBranch3, NegativeFd_Throws)
{
    std::vector<uint8_t> data = {1};
    EXPECT_THROW(writeBufferToFd(-1, data), std::runtime_error);
}

// writeBufferToFd — successful write and truncate
TEST(WriteBufferToFdBranch3, ValidFd_WritesData)
{
    int fd = memfd_create("test_write_ok", 0);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    writeBufferToFd(fd, data);

    // Read back to verify
    std::vector<uint8_t> buffer;
    readFdToBuffer(fd, buffer);
    EXPECT_EQ(buffer, data);
    close(fd);
}

// ============================================================================
// appendBufferToFd — invalid fd
// ============================================================================

TEST(AppendBufferToFdBranch3, NegativeFd_Throws)
{
    std::vector<uint8_t> data = {1};
    EXPECT_THROW(appendBufferToFd(-1, data), std::runtime_error);
}

// appendBufferToFd — successful append
TEST(AppendBufferToFdBranch3, ValidFd_AppendsData)
{
    int fd = memfd_create("test_append_ok", 0);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> data1 = {0xAA};
    std::vector<uint8_t> data2 = {0xBB};
    appendBufferToFd(fd, data1);
    appendBufferToFd(fd, data2);

    std::vector<uint8_t> buffer;
    readFdToBuffer(fd, buffer);
    EXPECT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer[0], 0xAA);
    EXPECT_EQ(buffer[1], 0xBB);
    close(fd);
}

// ============================================================================
// CustomFD — invalid fd throws
// ============================================================================

TEST(CustomFDBranch3, NegativeFd_Throws)
{
    EXPECT_THROW(CustomFD(-1), std::runtime_error);
}

// CustomFD — write with null data returns false
TEST(CustomFDBranch3, WriteNullData_ReturnsFalse)
{
    int fd = memfd_create("test_null", 0);
    ASSERT_GE(fd, 0);
    CustomFD cfd(fd);
    EXPECT_FALSE(cfd.write(0, nullptr, 10));
}

// CustomFD — write with zero size returns false
TEST(CustomFDBranch3, WriteZeroSize_ReturnsFalse)
{
    int fd = memfd_create("test_zero_sz", 0);
    ASSERT_GE(fd, 0);
    uint8_t data = 0;
    CustomFD cfd(fd);
    EXPECT_FALSE(cfd.write(0, &data, 0));
}

// CustomFD — write beyond fileSize returns false
TEST(CustomFDBranch3, WriteBeyondEnd_ReturnsFalse)
{
    int fd = memfd_create("test_beyond", 0);
    ASSERT_GE(fd, 0);
    // File is empty (size 0), writing at pos=1 > fileSize=0
    uint8_t data = 0xAA;
    CustomFD cfd(fd);
    EXPECT_FALSE(cfd.write(1, &data, 1));
}

// CustomFD — read with null data returns false
TEST(CustomFDBranch3, ReadNullData_ReturnsFalse)
{
    int fd = memfd_create("test_read_null", 0);
    ASSERT_GE(fd, 0);
    uint8_t init = 0;
    if (::write(fd, &init, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    CustomFD cfd(fd);
    EXPECT_FALSE(cfd.read(0, nullptr, 1));
}

// CustomFD — read beyond fileSize returns false
TEST(CustomFDBranch3, ReadBeyondEnd_ReturnsFalse)
{
    int fd = memfd_create("test_read_beyond", 0);
    ASSERT_GE(fd, 0);
    uint8_t init = 0;
    if (::write(fd, &init, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    CustomFD cfd(fd);
    uint8_t buf = 0;
    // pos=0, size=2 but file is 1 byte => (0+2) > 1
    EXPECT_FALSE(cfd.read(0, &buf, 2));
}

// CustomFD — operator() and operator int
TEST(CustomFDBranch3, OperatorInt_ReturnsFd)
{
    int fd = memfd_create("test_op_int", 0);
    ASSERT_GE(fd, 0);
    CustomFD cfd(fd);
    EXPECT_EQ(cfd(), fd);
    EXPECT_EQ(static_cast<int>(cfd), fd);
}

// ============================================================================
// vectorTo256BitHexString — wrong size and correct size
// ============================================================================

TEST(VectorTo256BitHexBranch3, WrongSize_ReturnsZeroPadded)
{
    std::vector<uint8_t> v(16, 0);
    auto result = vectorTo256BitHexString(v);
    EXPECT_EQ(result.size(), 2 + 64); // "0x" + 64 hex chars
    EXPECT_EQ(result, "0x" + std::string(64, '0'));
}

TEST(VectorTo256BitHexBranch3, CorrectSize_ReturnsHex)
{
    std::vector<uint8_t> v(32, 0xFF);
    auto result = vectorTo256BitHexString(v);
    EXPECT_EQ(result, "0x" + std::string(64, 'f'));
}

TEST(VectorTo256BitHexBranch3, EmptyVector_ReturnsZeroPadded)
{
    std::vector<uint8_t> v;
    auto result = vectorTo256BitHexString(v);
    EXPECT_EQ(result, "0x" + std::string(64, '0'));
}

// ============================================================================
// bitfield256_tToBitMap / bitfield256_tToBitArray
// ============================================================================

TEST(Bitfield256ToBitMapBranch3, AllZeros)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    auto bitmap = bitfield256_tToBitMap(bf);
    EXPECT_EQ(bitmap.size(), 32u);
    for (auto b : bitmap)
    {
        EXPECT_EQ(b, 0);
    }
}

TEST(Bitfield256ToBitMapBranch3, SingleBitSet)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    bf.fields[0].byte = 1;   // bit 0
    auto bitmap = bitfield256_tToBitMap(bf);
    EXPECT_EQ(bitmap[0], 1); // bit 0 set in first byte
}

TEST(Bitfield256ToBitArrayBranch3, AllZeros)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    auto bitmap = bitfield256_tToBitArray(bf);
    EXPECT_EQ(bitmap.size(), 32u);
    for (auto b : bitmap)
    {
        EXPECT_EQ(b, 0);
    }
}

TEST(Bitfield256ToBitArrayBranch3, KnownValue)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    bf.fields[0].byte = 0x12345678;
    auto bitmap = bitfield256_tToBitArray(bf);
    EXPECT_EQ(bitmap[0], 0x12);
    EXPECT_EQ(bitmap[1], 0x34);
    EXPECT_EQ(bitmap[2], 0x56);
    EXPECT_EQ(bitmap[3], 0x78);
}

// ============================================================================
// bitMapToBitfield256_t
// ============================================================================

TEST(BitMapToBitfield256Branch3, WrongSize_ReturnsZero)
{
    std::vector<uint8_t> bitmap(16, 0xFF); // wrong size
    auto bf = bitMapToBitfield256_t(bitmap);
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(bf.fields[i].byte, 0u);
    }
}

TEST(BitMapToBitfield256Branch3, CorrectSize_RoundTrips)
{
    bitfield256_t original = {};
    memset(&original, 0, sizeof(original));
    original.fields[0].byte = 0xDEADBEEF;
    original.fields[7].byte = 0x12345678;

    auto bitmap = bitfield256_tToBitArray(original);
    auto result = bitMapToBitfield256_t(bitmap);

    EXPECT_EQ(result.fields[0].byte, original.fields[0].byte);
    EXPECT_EQ(result.fields[7].byte, original.fields[7].byte);
}

// ============================================================================
// nsmSwCodeToString — cover all switch cases
// ============================================================================

TEST(NsmSwCodeBranch3, AllCases)
{
    EXPECT_NE(nsmSwCodeToString(NSM_SW_SUCCESS).find("NSM_SW_SUCCESS"),
              std::string::npos);
    EXPECT_NE(nsmSwCodeToString(NSM_SW_ERROR).find("NSM_SW_ERROR"),
              std::string::npos);
    EXPECT_NE(nsmSwCodeToString(NSM_SW_ERROR_DATA).find("NSM_SW_ERROR_DATA"),
              std::string::npos);
    EXPECT_NE(
        nsmSwCodeToString(NSM_SW_ERROR_LENGTH).find("NSM_SW_ERROR_LENGTH"),
        std::string::npos);
    EXPECT_NE(nsmSwCodeToString(NSM_SW_ERROR_NULL).find("NSM_SW_ERROR_NULL"),
              std::string::npos);
    EXPECT_NE(nsmSwCodeToString(NSM_SW_ERROR_COMMAND_FAIL)
                  .find("NSM_SW_ERROR_COMMAND_FAIL"),
              std::string::npos);
    EXPECT_NE(
        nsmSwCodeToString(NSM_SW_ERROR_TIMEOUT).find("NSM_SW_ERROR_TIMEOUT"),
        std::string::npos);
    EXPECT_NE(nsmSwCodeToString(999).find("UNKNOWN"), std::string::npos);
}

// ============================================================================
// nsmCompletionCodeToString — cover all switch cases
// ============================================================================

TEST(NsmCompletionCodeBranch3, AllCases)
{
    EXPECT_NE(nsmCompletionCodeToString(NSM_SUCCESS).find("NSM_SUCCESS"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERROR).find("NSM_ERROR"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_INVALID_DATA)
                  .find("NSM_ERR_INVALID_DATA"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_INVALID_DATA_LENGTH)
                  .find("NSM_ERR_INVALID_DATA_LENGTH"),
              std::string::npos);
    EXPECT_NE(
        nsmCompletionCodeToString(NSM_ERR_NOT_READY).find("NSM_ERR_NOT_READY"),
        std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_UNSUPPORTED_COMMAND_CODE)
                  .find("NSM_ERR_UNSUPPORTED_COMMAND_CODE"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_UNSUPPORTED_MSG_TYPE)
                  .find("NSM_ERR_UNSUPPORTED_MSG_TYPE"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ACCEPTED).find("NSM_ACCEPTED"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_BUSY).find("NSM_BUSY"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_BUS_ACCESS)
                  .find("NSM_ERR_BUS_ACCESS"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_INVALID_STATE_FOR_COMMAND)
                  .find("NSM_ERR_INVALID_STATE_FOR_COMMAND"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(NSM_ERR_INVALID_REQUEST_TYPE)
                  .find("NSM_ERR_INVALID_REQUEST_TYPE"),
              std::string::npos);
    EXPECT_NE(nsmCompletionCodeToString(200).find("UNKNOWN"),
              std::string::npos);
}

// ============================================================================
// nsmReasonCodeToString — cover all switch cases
// ============================================================================

TEST(NsmReasonCodeBranch3, AllCases)
{
    EXPECT_NE(nsmReasonCodeToString(ERR_NULL).find("ERR_NULL"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_INVALID_PCI).find("ERR_INVALID_PCI"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_INVALID_RQD).find("ERR_INVALID_RQD"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_TIMEOUT).find("ERR_TIMEOUT"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_DOWNSTREAM_TIMEOUT)
                  .find("ERR_DOWNSTREAM_TIMEOUT"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_I2C_NACK_FROM_DEV_ADDR)
                  .find("ERR_I2C_NACK_FROM_DEV_ADDR"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_I2C_NACK_FROM_DEV_CMD_DATA)
                  .find("ERR_I2C_NACK_FROM_DEV_CMD_DATA"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_I2C_NACK_FROM_DEV_ADDR_RS)
                  .find("ERR_I2C_NACK_FROM_DEV_ADDR_RS"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_NVLINK_PORT_INVALID)
                  .find("ERR_NVLINK_PORT_INVALID"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_NVLINK_PORT_DISABLED)
                  .find("ERR_NVLINK_PORT_DISABLED"),
              std::string::npos);
    EXPECT_NE(
        nsmReasonCodeToString(ERR_NOT_SUPPORTED).find("ERR_NOT_SUPPORTED"),
        std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_PROPERTY_NOT_SUPPORTED)
                  .find("ERR_PROPERTY_NOT_SUPPORTED"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_LIFESPAN_VOLATILE_NOT_SUPPORTED)
                  .find("ERR_LIFESPAN_VOLATILE_NOT_SUPPORTED"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_LIFESPAN_PERSISTENT_NOT_SUPPORTED)
                  .find("ERR_LIFESPAN_PERSISTENT_NOT_SUPPORTED"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_NO_BOOT_COMPLETE)
                  .find("ERR_NO_BOOT_COMPLETE"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_UPDATE_IN_PROGRESS)
                  .find("ERR_UPDATE_IN_PROGRESS"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_IMAGE_COPY_IN_PROGRESS)
                  .find("ERR_IMAGE_COPY_IN_PROGRESS"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_IMAGE_COPY_COMPLETED)
                  .find("ERR_IMAGE_COPY_COMPLETED"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_FLASH_WEAR_MITIGATION)
                  .find("ERR_FLASH_WEAR_MITIGATION"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(ERR_INCOMPLETE_COMPONENT_SET)
                  .find("ERR_INCOMPLETE_COMPONENT_SET"),
              std::string::npos);
    EXPECT_NE(nsmReasonCodeToString(60000).find("UNKNOWN"), std::string::npos);
}

// ============================================================================
// int64ToDoubleSafeConvert — positive overflow branch
// ============================================================================

TEST(Int64ToDoubleBranch3, PositiveOverflow_Capped)
{
    int64_t value = static_cast<int64_t>(MAX_SAFE_INTEGER_IN_DOUBLE) + 1;
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(MAX_SAFE_INTEGER_IN_DOUBLE));
}

TEST(Int64ToDoubleBranch3, NegativeOverflow_Capped)
{
    int64_t value = -static_cast<int64_t>(MAX_SAFE_INTEGER_IN_DOUBLE) - 1;
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(-static_cast<int64_t>(
                                 MAX_SAFE_INTEGER_IN_DOUBLE)));
}

// ============================================================================
// uint64ToDoubleSafeConvert — overflow branch
// ============================================================================

TEST(Uint64ToDoubleBranch3, Overflow_Capped)
{
    uint64_t value = MAX_SAFE_INTEGER_IN_DOUBLE + 1;
    double result = uint64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(MAX_SAFE_INTEGER_IN_DOUBLE));
}

// ============================================================================
// updateMethodsBitfieldToList — individual bit branches
// ============================================================================

TEST(UpdateMethodsBranch3, NoBitsSet_Empty)
{
    bitfield32_t bf = {0};
    auto result = updateMethodsBitfieldToList(bf);
    EXPECT_TRUE(result.empty());
}

TEST(UpdateMethodsBranch3, OnlyBit0_Automatic)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit0 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::Automatic);
}

TEST(UpdateMethodsBranch3, OnlyBit2_MediumSpecificReset)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit2 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::MediumSpecificReset);
}

TEST(UpdateMethodsBranch3, OnlyBit3_SystemReboot)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit3 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::SystemReboot);
}

TEST(UpdateMethodsBranch3, OnlyBit4_DCPowerCycle)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit4 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::DCPowerCycle);
}

TEST(UpdateMethodsBranch3, OnlyBit5_ACPowerCycle)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit5 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::ACPowerCycle);
}

TEST(UpdateMethodsBranch3, OnlyBit16_WarmReset)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit16 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::WarmReset);
}

TEST(UpdateMethodsBranch3, OnlyBit17_HotReset)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit17 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::HotReset);
}

TEST(UpdateMethodsBranch3, OnlyBit18_FLR)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;
    bitfield32_t bf = {0};
    bf.bits.bit18 = 1;
    auto result = updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::FLR);
}

// ============================================================================
// isPreferred — different medium priorities
// ============================================================================

TEST(IsPreferredBranch3, HigherMedium_CurrentWins)
{
    MctpMedium pcie = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpMedium usb = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.USB";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    auto current = std::make_tuple(pcie, binding);
    auto newInfo = std::make_tuple(usb, binding);

    // PCIe has priority 0, USB has priority 1. 0 < 1 but isPreferred checks >=
    // so PCIe (0) < USB (1) => 0 >= 1 is false => not preferred
    EXPECT_FALSE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch3, LowerMedium_NewWins)
{
    MctpMedium usb = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.USB";
    MctpMedium pcie = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    auto current = std::make_tuple(usb, binding);
    auto newInfo = std::make_tuple(pcie, binding);

    // USB priority=1, PCIe priority=0. 1 >= 0 => true => current is preferred
    EXPECT_TRUE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch3, UnknownMedium_DefaultPriority)
{
    MctpMedium unknown = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.Unknown";
    MctpMedium pcie = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    auto current = std::make_tuple(unknown, binding);
    auto newInfo = std::make_tuple(pcie, binding);

    // Unknown gets INT_MIN, PCIe gets 0. INT_MIN >= 0 is false
    EXPECT_FALSE(isPreferred(current, newInfo));
}

// ============================================================================
// getEidFromUUID — match and no-match
// ============================================================================

TEST(GetEidFromUUIDBranch3, MatchFound)
{
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> table;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    // UUID has UUID_LEN (36) chars
    uuid_t uuid = "aabbccdd-1122-3344-5566-778899aabbcc";
    table.emplace(uuid, std::make_tuple(eid_t(42), medium, binding));

    auto eid = getEidFromUUID(table, uuid);
    EXPECT_EQ(eid, 42);
}

TEST(GetEidFromUUIDBranch3, NoMatch_ReturnsMax)
{
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> table;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    uuid_t uuid = "aabbccdd-1122-3344-5566-778899aabbcc";
    table.emplace(uuid, std::make_tuple(eid_t(42), medium, binding));

    uuid_t other = "11111111-2222-3333-4444-555555555555";
    auto eid = getEidFromUUID(table, other);
    EXPECT_EQ(eid, std::numeric_limits<uint8_t>::max());
}

TEST(GetEidFromUUIDBranch3, EmptyTable_ReturnsMax)
{
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> table;
    uuid_t uuid = "aabbccdd-1122-3344-5566-778899aabbcc";
    auto eid = getEidFromUUID(table, uuid);
    EXPECT_EQ(eid, std::numeric_limits<uint8_t>::max());
}

// ============================================================================
// convertHexToString
// ============================================================================

TEST(ConvertHexToStringBranch3, EmptyData)
{
    std::vector<uint8_t> data;
    auto result = convertHexToString(data, 0);
    EXPECT_TRUE(result.empty());
}

TEST(ConvertHexToStringBranch3, NonEmpty)
{
    std::vector<uint8_t> data = {0xAB, 0xCD, 0xEF};
    auto result = convertHexToString(data, 3);
    EXPECT_EQ(result, "abcdef");
}

TEST(ConvertHexToStringBranch3, PartialSize)
{
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    auto result = convertHexToString(data, 2);
    EXPECT_EQ(result, "0102");
}

// ============================================================================
// requestMsgToHexString
// ============================================================================

TEST(RequestMsgToHexStringBranch3, EmptyMsg)
{
    std::vector<uint8_t> msg;
    auto result = requestMsgToHexString(msg);
    EXPECT_TRUE(result.empty());
}

TEST(RequestMsgToHexStringBranch3, NonEmpty)
{
    std::vector<uint8_t> msg = {0x0A, 0xFF};
    auto result = requestMsgToHexString(msg);
    EXPECT_NE(result.find("0a"), std::string::npos);
    EXPECT_NE(result.find("ff"), std::string::npos);
}

// ============================================================================
// Bitfield256 — getSetBits with no bits set
// ============================================================================

TEST(Bitfield256Branch3, GetSetBits_Empty)
{
    Bitfield256 bf;
    auto bits = bf.getSetBits();
    EXPECT_TRUE(bits.empty());
}

TEST(Bitfield256Branch3, IsAnyBitSet_NoSet)
{
    Bitfield256 bf;
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(Bitfield256Branch3, ClearAfterSet)
{
    Bitfield256 bf;
    bf.setBit(100);
    EXPECT_TRUE(bf.isAnyBitSet());
    bf.clear();
    EXPECT_FALSE(bf.isAnyBitSet());
}

// ============================================================================
// getDeviceNameFromDeviceType — all switch cases
// ============================================================================

TEST(GetDeviceNameBranch3, AllDeviceTypes)
{
    EXPECT_EQ(getDeviceNameFromDeviceType(0), "GPU");
    EXPECT_EQ(getDeviceNameFromDeviceType(1), "SWITCH");
    EXPECT_EQ(getDeviceNameFromDeviceType(2), "BRIDGE");
    EXPECT_EQ(getDeviceNameFromDeviceType(3), "BASEBOARD");
    EXPECT_EQ(getDeviceNameFromDeviceType(4), "EROT");
    EXPECT_EQ(getDeviceNameFromDeviceType(5), "MCTPBRIDGE");
    EXPECT_EQ(getDeviceNameFromDeviceType(6), "CPU");
    EXPECT_EQ(getDeviceNameFromDeviceType(7), "NSM_DEV_ID_UNKNOWN");
    EXPECT_EQ(getDeviceNameFromDeviceType(255), "NSM_DEV_ID_UNKNOWN");
}

// ============================================================================
// convertAndScaleDownUint32ToDouble — invalid value branch
// ============================================================================

TEST(ConvertScaleBranch3, InvalidUint32_ReturnsInvalidDouble)
{
    double result = convertAndScaleDownUint32ToDouble(0xFFFFFFFF, 1.0);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFFFFFFFF));
}

TEST(ConvertScaleBranch3, ValidUint32_ScalesCorrectly)
{
    double result = convertAndScaleDownUint32ToDouble(1000, 10.0);
    EXPECT_DOUBLE_EQ(result, 100.0);
}

// ============================================================================
// convertAndScaleDownUint8ToDouble — invalid value branch
// ============================================================================

TEST(ConvertScaleUint8Branch3, InvalidUint8_ReturnsInvalidDouble)
{
    double result = convertAndScaleDownUint8ToDouble(0xFF);
    EXPECT_DOUBLE_EQ(result, 255.0);
}

TEST(ConvertScaleUint8Branch3, ValidUint8_ReturnsDouble)
{
    double result = convertAndScaleDownUint8ToDouble(100);
    EXPECT_DOUBLE_EQ(result, 100.0);
}

// ============================================================================
// getUUIDFromEID — found vs not-found
// ============================================================================

TEST(GetUUIDFromEIDBranch3, NotFound_ReturnsNullopt)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        table;
    MctpMedium m = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding b = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";
    table.emplace("uuid-A", std::make_tuple(eid_t(10), m, b));

    auto result = getUUIDFromEID(table, 99);
    EXPECT_FALSE(result.has_value());
}

TEST(GetUUIDFromEIDBranch3, EmptyTable_ReturnsNullopt)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        table;
    auto result = getUUIDFromEID(table, 1);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// printBuffer — empty vs non-empty (just exercise, no crash)
// ============================================================================

TEST(PrintBufferBranch3, EmptyBuffer_NoCrash)
{
    std::vector<uint8_t> buf;
    printBuffer(true, buf, 0, 0);
    SUCCEED();
}

TEST(PrintBufferBranch3, NonEmptyBuffer_NoCrash)
{
    std::vector<uint8_t> buf = {0x01, 0x02};
    printBuffer(true, buf, 0x10, 0x20);
    SUCCEED();
}

TEST(PrintBufferBranch3, RawPointerVersion_NoCrash)
{
    uint8_t data[] = {0xAA, 0xBB};
    printBuffer(false, data, sizeof(data), 0x05, 0x0A);
    SUCCEED();
}
