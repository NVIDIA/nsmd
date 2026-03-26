/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests (batch 3) for common/utils.cpp
 * Targets additional zero-covered branches.
 */

#include "utils.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace utils;

// ============================================================================
// split(): trimStr that removes everything
// ============================================================================

TEST(SplitBranch4, TrimResultsInEmpty_SkippedInOutput)
{
    auto result = split("aaa,bbb", ",", "ab");
    EXPECT_TRUE(result.empty());
}

TEST(SplitBranch4, MultipleDelimiters_Consecutive)
{
    auto result = split("a,,b,,c", ",");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(SplitBranch4, OnlyDelimiters_Empty)
{
    auto result = split(",,,", ",");
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// isValidDbusString: various edge cases
// ============================================================================

TEST(IsValidDbusStringBranch4, NullCodepoint_ReturnsFalse)
{
    std::string_view s("\x00", 1);
    EXPECT_FALSE(isValidDbusString(s));
}

TEST(IsValidDbusStringBranch4, TwoByte_Overflow_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xC3"));
}

TEST(IsValidDbusStringBranch4, ThreeByte_Overflow_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xE2\x82"));
}

TEST(IsValidDbusStringBranch4, FourByte_Overflow_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xF0\x90\x80"));
}

TEST(IsValidDbusStringBranch4, Overlong2Byte_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xC1\x81"));
}

TEST(IsValidDbusStringBranch4, Overlong3Byte_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xE0\x81\x81"));
}

TEST(IsValidDbusStringBranch4, Overlong4Byte_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xF0\x80\xA0\x80"));
}

TEST(IsValidDbusStringBranch4, Valid4Byte_U10000)
{
    EXPECT_TRUE(isValidDbusString("\xF0\x90\x80\x80"));
}

TEST(IsValidDbusStringBranch4, Valid4Byte_MaxCodepoint)
{
    EXPECT_TRUE(isValidDbusString("\xF4\x8F\xBF\xBF"));
}

TEST(IsValidDbusStringBranch4, Beyond10FFFF_ReturnsFalse)
{
    EXPECT_FALSE(isValidDbusString("\xF4\x90\x80\x80"));
}

// ============================================================================
// convertMsgToString: isTx true/false and empty
// ============================================================================

TEST(ConvertMsgToStringBranch4, TxTrue)
{
    std::vector<uint8_t> buf = {0x01, 0x02};
    auto result = convertMsgToString(true, buf, 0x03, 0x1D);
    EXPECT_NE(result.find("Tx"), std::string::npos);
}

TEST(ConvertMsgToStringBranch4, TxFalse)
{
    std::vector<uint8_t> buf = {0x01, 0x02};
    auto result = convertMsgToString(false, buf, 0x03, 0x1D);
    EXPECT_NE(result.find("Rx"), std::string::npos);
}

TEST(ConvertMsgToStringBranch4, EmptyBuffer)
{
    std::vector<uint8_t> buf;
    auto result = convertMsgToString(true, buf, 0, 0);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// bitmapToIndices
// ============================================================================

TEST(BitmapToIndicesBranch4, MixedBits)
{
    std::vector<uint8_t> bitmap = {0xA5};
    auto [zeros, ones] = bitmapToIndices(bitmap);
    EXPECT_EQ(ones.size(), 4u);
    EXPECT_EQ(zeros.size(), 4u);
    EXPECT_EQ(ones[0], 0);
    EXPECT_EQ(ones[1], 2);
    EXPECT_EQ(ones[2], 5);
    EXPECT_EQ(ones[3], 7);
}

TEST(BitmapToIndicesBranch4, TwoBytes)
{
    std::vector<uint8_t> bitmap = {0x00, 0x01};
    auto [zeros, ones] = bitmapToIndices(bitmap);
    EXPECT_EQ(ones.size(), 1u);
    EXPECT_EQ(ones[0], 8);
    EXPECT_EQ(zeros.size(), 15u);
}

// ============================================================================
// getDeviceInstanceName
// ============================================================================

TEST(GetDeviceInstanceNameBranch4, GPU_0)
{
    EXPECT_EQ(getDeviceInstanceName(0, 0), "GPU_0");
}

TEST(GetDeviceInstanceNameBranch4, BRIDGE_5)
{
    EXPECT_EQ(getDeviceInstanceName(2, 5), "BRIDGE_5");
}

TEST(GetDeviceInstanceNameBranch4, Unknown_99)
{
    EXPECT_EQ(getDeviceInstanceName(255, 99), "NSM_DEV_ID_UNKNOWN_99");
}

// ============================================================================
// combineDeviceTypeAndRole / getDeviceTypeAndRole
// ============================================================================

TEST(DeviceTypeRoleBranch4, RoundTrip)
{
    uint16_t combined = combineDeviceTypeAndRole(2, 3);
    EXPECT_EQ(combined, (3 << 8) | 2);
    uint8_t outType = 0, outRole = 0;
    getDeviceTypeAndRole(combined, &outType, &outRole);
    EXPECT_EQ(outType, 2);
    EXPECT_EQ(outRole, 3);
}

TEST(DeviceTypeRoleBranch4, ZeroValues)
{
    uint16_t combined = combineDeviceTypeAndRole(0, 0);
    EXPECT_EQ(combined, 0);
    uint8_t outType = 0xFF, outRole = 0xFF;
    getDeviceTypeAndRole(combined, &outType, &outRole);
    EXPECT_EQ(outType, 0);
    EXPECT_EQ(outRole, 0);
}

// ============================================================================
// convertGuid64ToString
// ============================================================================

TEST(ConvertGuid64Branch4, KnownValue)
{
    std::string result;
    convertGuid64ToString(0x0001000200030004ULL, result);
    EXPECT_EQ(result, "0001-0002-0003-0004");
}

TEST(ConvertGuid64Branch4, AllZeros)
{
    std::string result;
    convertGuid64ToString(0, result);
    EXPECT_EQ(result, "0000-0000-0000-0000");
}

// ============================================================================
// Time utilities
// ============================================================================

TEST(TimeUtilsBranch4, GetCurrentSystemTime_NonEmpty)
{
    EXPECT_FALSE(getCurrentSystemTime().empty());
}

TEST(TimeUtilsBranch4, GetCurrentSteadyClockTimestamp_Positive)
{
    EXPECT_GT(getCurrentSteadyClockTimestamp(), 0u);
}

TEST(TimeUtilsBranch4, GetCurrentSteadyClockTimestampUs_Positive)
{
    EXPECT_GT(getCurrentSteadyClockTimestampUs(), 0u);
}

// ============================================================================
// makeDBusNameValid
// ============================================================================

TEST(MakeDBusNameValidBranch4, RemovesSpecialChars)
{
    auto result = makeDBusNameValid("hello world!@#$%");
    EXPECT_NE(result.find("hello_world_"), std::string::npos);
}

TEST(MakeDBusNameValidBranch4, AlreadyValid)
{
    EXPECT_EQ(makeDBusNameValid("hello_world.123/path"),
              "hello_world.123/path");
}

// ============================================================================
// convertBitfieldToVector
// ============================================================================

TEST(ConvertBitfieldToVectorBranch4, TwoBytes)
{
    std::vector<uint8_t> bitfield = {0x01, 0x80};
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    ASSERT_EQ(result.size(), 16u);
    EXPECT_TRUE(result[0]);
    EXPECT_FALSE(result[1]);
    EXPECT_FALSE(result[14]);
    EXPECT_TRUE(result[15]);
}

TEST(ConvertBitfieldToVectorBranch4, AllZeros)
{
    std::vector<uint8_t> bitfield = {0x00, 0x00};
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    ASSERT_EQ(result.size(), 16u);
    for (auto b : result)
    {
        EXPECT_FALSE(b);
    }
}

TEST(ConvertBitfieldToVectorBranch4, AllOnes)
{
    std::vector<uint8_t> bitfield = {0xFF};
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    ASSERT_EQ(result.size(), 8u);
    for (auto b : result)
    {
        EXPECT_TRUE(b);
    }
}

// ============================================================================
// parseStaticUuid
// ============================================================================

TEST(ParseStaticUuidBranch4, MultipleValues_ParsesCorrectly)
{
    uuid_t uuid = "STATIC:514:1:PROP:val1:val2:val3";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), 0);
    EXPECT_EQ(rp, "PROP");
    ASSERT_EQ(rv.size(), 3u);
    EXPECT_EQ(rv[0], "val1");
    EXPECT_EQ(rv[1], "val2");
    EXPECT_EQ(rv[2], "val3");
}

TEST(ParseStaticUuidBranch4, SingleValue_FastPath)
{
    uuid_t uuid = "STATIC:0:0:NAME:singleval";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), 0);
    ASSERT_EQ(rv.size(), 1u);
    EXPECT_EQ(rv[0], "singleval");
}

TEST(ParseStaticUuidBranch4, N1Negative_ReturnsNeg1)
{
    uuid_t uuid = "STATIC:-1:0:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -1);
}

TEST(ParseStaticUuidBranch4, N1TooLarge_ReturnsNeg1)
{
    uuid_t uuid = "STATIC:65536:0:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -1);
}

TEST(ParseStaticUuidBranch4, N2Negative_ReturnsNeg2)
{
    uuid_t uuid = "STATIC:0:-1:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -2);
}

// ============================================================================
// CustomFD: write and read
// ============================================================================

TEST(CustomFDBranch4, WriteAndRead_Success)
{
    int fd = memfd_create("test_wr_b4", 0);
    ASSERT_GE(fd, 0);
    uint8_t init[4] = {0, 0, 0, 0};
    if (::write(fd, init, 4) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    CustomFD cfd(fd);
    uint8_t writeData[3] = {0xAA, 0xBB, 0xCC};
    EXPECT_TRUE(cfd.write(0, writeData, 3));

    uint8_t readData[3] = {};
    EXPECT_TRUE(cfd.read(0, readData, 3));
    EXPECT_EQ(readData[0], 0xAA);
    EXPECT_EQ(readData[1], 0xBB);
    EXPECT_EQ(readData[2], 0xCC);
}

TEST(CustomFDBranch4, ReadZeroSize_ReturnsFalse)
{
    int fd = memfd_create("test_rdsz_b4", 0);
    ASSERT_GE(fd, 0);
    uint8_t init = 0;
    if (::write(fd, &init, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    CustomFD cfd(fd);
    uint8_t buf;
    EXPECT_FALSE(cfd.read(0, &buf, 0));
}

TEST(CustomFDBranch4, Write_ExtendsFileSize)
{
    int fd = memfd_create("test_extend_b4", 0);
    ASSERT_GE(fd, 0);

    CustomFD cfd(fd);
    EXPECT_EQ(cfd.size(), 0u);

    uint8_t data[4] = {1, 2, 3, 4};
    EXPECT_TRUE(cfd.write(0, data, 4));
    EXPECT_EQ(cfd.size(), 4u);

    uint8_t data2[3] = {5, 6, 7};
    EXPECT_TRUE(cfd.write(2, data2, 3));
    EXPECT_EQ(cfd.size(), 5u);
}

// ============================================================================
// Bitfield256
// ============================================================================

TEST(Bitfield256Branch4, SetBitTwice_SecondReturnsFalse)
{
    Bitfield256 bf;
    EXPECT_TRUE(bf.setBit(42));
    EXPECT_FALSE(bf.setBit(42));
}

TEST(Bitfield256Branch4, GetSetBits_WithBitsSet)
{
    Bitfield256 bf;
    bf.setBit(0);
    bf.setBit(31);
    bf.setBit(255);
    auto bits = bf.getSetBits();
    EXPECT_NE(bits.find("0"), std::string::npos);
    EXPECT_NE(bits.find("31"), std::string::npos);
    EXPECT_NE(bits.find("255"), std::string::npos);
}

// ============================================================================
// isPreferred: same medium, different binding
// ============================================================================

TEST(IsPreferredBranch4, SameMedium_DifferentBinding)
{
    MctpMedium pcie = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding pcieBind = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";
    MctpBinding usbBind = "xyz.openbmc_project.MCTP.Binding.BindingTypes.USB";

    auto current = std::make_tuple(pcie, pcieBind);
    auto newInfo = std::make_tuple(pcie, usbBind);
    EXPECT_FALSE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch4, SameMedium_SameBinding)
{
    MctpMedium pcie = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding pcieBind = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    auto current = std::make_tuple(pcie, pcieBind);
    auto newInfo = std::make_tuple(pcie, pcieBind);
    EXPECT_TRUE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch4, UnknownBinding_DefaultPriority)
{
    MctpMedium pcie = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding unknownBind =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.Unknown";
    MctpBinding pcieBind = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    auto current = std::make_tuple(pcie, unknownBind);
    auto newInfo = std::make_tuple(pcie, pcieBind);
    EXPECT_FALSE(isPreferred(current, newInfo));
}

// ============================================================================
// getUUIDFromEID: found
// ============================================================================

TEST(GetUUIDFromEIDBranch4, Found_ReturnsUUID)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        table;
    MctpMedium m = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding b = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";
    table.emplace("test-uuid-1234", std::make_tuple(eid_t(42), m, b));

    auto result = getUUIDFromEID(table, 42);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "test-uuid-1234");
}

// ============================================================================
// safe convert functions: within range
// ============================================================================

TEST(SafeConvertBranch4, Int64_WithinRange)
{
    EXPECT_DOUBLE_EQ(int64ToDoubleSafeConvert(12345), 12345.0);
    EXPECT_DOUBLE_EQ(int64ToDoubleSafeConvert(-12345), -12345.0);
    EXPECT_DOUBLE_EQ(int64ToDoubleSafeConvert(0), 0.0);
}

TEST(SafeConvertBranch4, Uint64_WithinRange)
{
    EXPECT_DOUBLE_EQ(uint64ToDoubleSafeConvert(12345), 12345.0);
    EXPECT_DOUBLE_EQ(uint64ToDoubleSafeConvert(0), 0.0);
}

// ============================================================================
// convertUUIDToString
// ============================================================================

TEST(ConvertUUIDBranch4, ValidSize)
{
    std::vector<uint8_t> arr = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    auto result = convertUUIDToString(arr);
    EXPECT_EQ(result.size(), UUID_LEN + 1);
}

TEST(ConvertUUIDBranch4, InvalidSize_ReturnsEmpty)
{
    std::vector<uint8_t> arr = {0x01, 0x02};
    auto result = convertUUIDToString(arr);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// convertMacAddressToString: valid
// ============================================================================

TEST(ConvertMacAddressBranch4, ValidMac)
{
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::string result;
    convertMacAddressToString(mac, 6, result);
    EXPECT_EQ(result, "aa:bb:cc:dd:ee:ff");
}

// ============================================================================
// indicesToBitmap
// ============================================================================

TEST(IndicesToBitmapBranch4, ValidIndices)
{
    std::vector<uint8_t> indices = {0, 3, 7};
    auto bitmap = indicesToBitmap(indices, 1);
    EXPECT_EQ(bitmap.size(), 1u);
    EXPECT_EQ(bitmap[0], 0x89);
}

// ============================================================================
// CustomFD size()
// ============================================================================

TEST(CustomFDBranch4, Size_ReportsCorrectly)
{
    int fd = memfd_create("test_size_b4", 0);
    ASSERT_GE(fd, 0);
    uint8_t data[10] = {};
    if (::write(fd, data, 10) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    CustomFD cfd(fd);
    EXPECT_EQ(cfd.size(), 10u);
}
