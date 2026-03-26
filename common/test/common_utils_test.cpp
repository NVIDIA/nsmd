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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::ElementsAre;

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

TEST(ConvertUUIDToString, testGoodConversionToString)
{
    std::vector<uint8_t> intUUID{0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
                                 0x0c, 0x0d, 0x0e, 0x0f};

    // convert intUUID to stringUUID
    uuid_t stringUUID = utils::convertUUIDToString(intUUID);

    EXPECT_STREQ(stringUUID.c_str(), "00010203-0405-0607-0809-0a0b0c0d0e0f\0");
}

TEST(ConvertUUIDToString, testGBadConversionToString)
{
    std::vector<uint8_t> intUUID{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

    // convert intUUID to stringUUID
    uuid_t stringUUID = utils::convertUUIDToString(intUUID);

    EXPECT_STREQ(stringUUID.c_str(), "");
}

TEST(convertHexToString, testGoodHexConversionToString)
{
    std::vector<uint8_t> data{0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};

    // convert HEX to a string
    std::string result = utils::convertHexToString(data, data.size());

    EXPECT_STREQ(result.c_str(), "0001020304050607\0");
}

TEST(convertHexToString, testBadHexConversionToString)
{
    std::vector<uint8_t> data;

    // convert HEX to a string
    std::string result = utils::convertHexToString(data, data.size());

    EXPECT_STREQ(result.c_str(), "");
}

TEST(isValidDbusString, testGoodIsValidDbusString)
{
    // test 1 byte
    EXPECT_TRUE(utils::isValidDbusString("\x41\x41\x41"));

    // test 2 bytes
    EXPECT_TRUE(utils::isValidDbusString("\xC3\xA9\xC3\xA9\xC3\xA9"));

    // test 3 bytes
    EXPECT_TRUE(
        utils::isValidDbusString("\xE2\x82\xAC\xE2\x82\xAC\xE2\x82\xAC"));

    // test 4 bytes
    EXPECT_TRUE(utils::isValidDbusString(
        "\xF0\x90\x8D\x88\xF0\x90\x8D\x88\xF0\x90\x8D\x88"));
}

TEST(isValidDbusString, testBadIsValidDbusString)
{
    // test 1 byte, it must be <= 0x7f
    EXPECT_FALSE(utils::isValidDbusString("\x80"));

    // test 2 bytes, the first two bits of the leading byte
    // should be 11b
    EXPECT_FALSE(utils::isValidDbusString("\xE3\xA9"));

    // test 2 bytes, the first two bits of the subsequent byte
    // should be 10b
    EXPECT_FALSE(utils::isValidDbusString("\xC3\xE9"));

    // test 3 bytes, the first three bits of the leading byte
    // should be 111b
    EXPECT_FALSE(utils::isValidDbusString("\xF2\x82\xAC"));

    // test 3 bytes, the first two bits of the subsequent byte
    // should be 10b
    EXPECT_FALSE(utils::isValidDbusString("\xE2\xF2\xAC"));

    // test 4 bytes, the first four bits of the leading byte
    // should be 1111b
    EXPECT_FALSE(utils::isValidDbusString("\xC0\x90\x8D\x88"));

    // test 4 bytes, the first two bits of the subsequent byte
    // should be 10b
    EXPECT_FALSE(utils::isValidDbusString("\xF0\x90\xFD\x88"));
}

TEST(makeDBusNameValid, Functional)
{
    const std::vector<std::array<std::string, 2>> data{
        {{"HGX_GPU_SXM 1 DRAM_0_Temp_0", "HGX_GPU_SXM_1_DRAM_0_Temp_0"}},

        {{"HGX_GPU_SXM    1 &^* DRAM_0_Temp_0", "HGX_GPU_SXM_1_DRAM_0_Temp_0"}},

        {{"/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1",
          "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1"}},

        {{"/xyz/openbmc_project/inventory/system/processors/GPU_SXM 1 DRAM_0",
          "/xyz/openbmc_project/inventory/system/processors/GPU_SXM_1_DRAM_0"}},

        {{"xyz.openbmc_project.Configuration.NSM_Temp",
          "xyz.openbmc_project.Configuration.NSM_Temp"}},

        {{"xyz.openbmc_project.Sensor.HGX_GPU_SXM 1 DRAM_0_Temp_0",
          "xyz.openbmc_project.Sensor.HGX_GPU_SXM_1_DRAM_0_Temp_0"}},
    };

    for (const auto& e : data)
    {
        EXPECT_STREQ(utils::makeDBusNameValid(e[0]).c_str(), e[1].c_str());
    }
}

TEST(getDeviceNameFromDeviceType, ValidDeviceTypes)
{
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(0), "GPU");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(1), "SWITCH");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(2), "BRIDGE");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(3), "BASEBOARD");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(4), "EROT");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(5), "MCTPBRIDGE");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(6), "CPU");
}

TEST(getDeviceNameFromDeviceType, UnknownDeviceType)
{
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(7), "NSM_DEV_ID_UNKNOWN");
    EXPECT_EQ(utils::getDeviceNameFromDeviceType(255), "NSM_DEV_ID_UNKNOWN");
}

TEST(getDeviceInstanceName, ValidInstances)
{
    EXPECT_EQ(utils::getDeviceInstanceName(0, 0), "GPU_0");
    EXPECT_EQ(utils::getDeviceInstanceName(1, 1), "SWITCH_1");
    EXPECT_EQ(utils::getDeviceInstanceName(2, 2), "BRIDGE_2");
    EXPECT_EQ(utils::getDeviceInstanceName(3, 3), "BASEBOARD_3");
    EXPECT_EQ(utils::getDeviceInstanceName(4, 4), "EROT_4");
    EXPECT_EQ(utils::getDeviceInstanceName(5, 5), "MCTPBRIDGE_5");
    EXPECT_EQ(utils::getDeviceInstanceName(6, 6), "CPU_6");
}

TEST(getDeviceInstanceName, UnknownTypeWithValidInstance)
{
    EXPECT_EQ(utils::getDeviceInstanceName(7, 0), "NSM_DEV_ID_UNKNOWN_0");
}

TEST(setBit, TestSettingBits)
{
    utils::Bitfield256 bitMap{};

    EXPECT_TRUE(bitMap.setBit(2));
    EXPECT_EQ(bitMap.fields[0].byte, 0b00000000000000000000000000000100);

    EXPECT_FALSE(bitMap.setBit(2));

    EXPECT_TRUE(bitMap.setBit(33));
    EXPECT_EQ(bitMap.fields[0].byte, 0b00000000000000000000000000000100);
    EXPECT_EQ(bitMap.fields[1].byte, 0b00000000000000000000000000000010);
}

TEST(getSetBits, TestNoSetBits)
{
    utils::Bitfield256 emptyBitField{};

    EXPECT_EQ(emptyBitField.getSetBits(), "");
}

TEST(getSetBits, TestSetBits)
{
    utils::Bitfield256 bitMap;
    bitMap.fields[0].byte = 0b00000000000000000000000000000001;

    EXPECT_EQ(bitMap.getSetBits(), "0");

    bitMap.fields[0].byte = 0b00000000000000000000000000001101;

    EXPECT_EQ(bitMap.getSetBits(), "0, 2, 3");

    bitMap.fields[0].byte = 0b00000000000000000000000011110000;
    bitMap.fields[2].byte = 0b00000000000000000000000000000001;

    EXPECT_EQ(bitMap.getSetBits(), "4, 5, 6, 7, 64");
}

TEST(memFd, TestGoodWriteRead)
{
    int fd = memfd_create("test", 0);
    std::vector<uint8_t> data{0x01, 0x02, 0x03, 0x04, 0x05};
    utils::writeBufferToFd(fd, data);
    EXPECT_GT(fd, 0);

    std::vector<uint8_t> readData;
    utils::readFdToBuffer(fd, readData);

    EXPECT_THAT(data, ElementsAre(0x01, 0x02, 0x03, 0x04, 0x05));
}

TEST(memFd, TestGoodWriteReadForEmptyBuffer)
{
    int fd = memfd_create("test", 0);
    std::vector<uint8_t> data;
    utils::writeBufferToFd(fd, data);
    EXPECT_GT(fd, 0);

    std::vector<uint8_t> readData;
    utils::readFdToBuffer(fd, readData);

    EXPECT_THAT(data, ElementsAre());
}

TEST(convertMacAddressToString, TestGoodConversionMacAddressToString)
{
    std::vector<uint8_t> macAddress{0x01, 0x01, 0x02, 0x03,
                                    0x04, 0x05, 0x00, 0x00};
    std::string macAddressString;
    macAddressString.resize(sizeof("XX:XX:XX:XX:XX:XX"));
    utils::convertMacAddressToString(macAddress.data(), macAddress.size(),
                                     macAddressString);
    macAddressString.resize(strlen(macAddressString.c_str()));
    EXPECT_EQ(macAddressString, "01:01:02:03:04:05");
}

TEST(convertMacAddressToString, TestBadConversionMacAddressToString)
{
    std::vector<uint8_t> macAddress{0x01, 0x01, 0x02, 0x03};
    std::string macAddressString;
    macAddressString.resize(sizeof("XX:XX:XX:XX:XX:XX"));
    utils::convertMacAddressToString(macAddress.data(), macAddress.size(),
                                     macAddressString);
    macAddressString.resize(strlen(macAddressString.c_str()));
    EXPECT_EQ(macAddressString, "");
}

TEST(convertGuid64ToString, TestGoodConversionGuid64ToString)
{
    uint64_t guid = 0x0102030405060708;
    std::string guidString;
    utils::convertGuid64ToString(guid, guidString);
    EXPECT_EQ(guidString, "0102-0304-0506-0708");
}
/*
 * Additional tests for uncovered functions in common/utils.cpp
 * Generated to improve functional coverage from 57.5% toward 90% target
 * Focus: High-value utility functions (MCTP priority, string ops, conversions,
 * bitmaps) Functions covered: ~15 of 27 uncovered (most impactful subset)
 */

// ============================================================================
// MCTP Priority Comparison Tests
// ============================================================================

TEST(MctpPriority, testIsPreferredSameMediumDifferentBinding)
{
    MctpMedium pcieMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding pcieBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";
    MctpBinding smbusBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.SMBus";

    auto current = std::make_tuple(pcieMedium, pcieBinding);
    auto newInfo = std::make_tuple(pcieMedium, smbusBinding);

    // Note: isPreferred() returns FALSE when current is better (lower priority
    // number) PCIe binding (0) < SMBus binding (5), so current is better,
    // function returns false
    EXPECT_FALSE(utils::isPreferred(current, newInfo));
    EXPECT_TRUE(utils::isPreferred(newInfo, current));
}

TEST(MctpPriority, testIsPreferredDifferentMedium)
{
    MctpMedium pcieMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpMedium smbusMedium =
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.SMBus";
    MctpBinding pcieBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";
    MctpBinding smbusBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.SMBus";

    auto current = std::make_tuple(pcieMedium, pcieBinding);
    auto newInfo = std::make_tuple(smbusMedium, smbusBinding);

    // Note: isPreferred() returns FALSE when current is better (lower priority
    // number) PCIe medium (0) < SMBus medium (6), so current is better,
    // function returns false
    EXPECT_FALSE(utils::isPreferred(current, newInfo));
    EXPECT_TRUE(utils::isPreferred(newInfo, current));
}

TEST(MctpPriority, testIsPreferredSamePriority)
{
    MctpMedium pcieMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding pcieBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    auto current = std::make_tuple(pcieMedium, pcieBinding);
    auto same = std::make_tuple(pcieMedium, pcieBinding);

    // Same priority - current is preferred (>=)
    EXPECT_TRUE(utils::isPreferred(current, same));
}

// ============================================================================
// String Utility Tests
// ============================================================================

TEST(StringUtils, testConvertMsgToStringTx)
{
    std::vector<uint8_t> buffer{0x01, 0x02, 0x03, 0x04};
    uint8_t tag = 0xAB;
    eid_t eid = 10;

    std::string result = utils::convertMsgToString(utils::Tx, buffer, tag, eid);

    // Should contain "Tx" and hex data
    EXPECT_NE(result.find("Tx"), std::string::npos);
    EXPECT_NE(result.find("01"), std::string::npos); // First byte
    // EID format varies, just check result is non-empty
    EXPECT_FALSE(result.empty());
}

TEST(StringUtils, testConvertMsgToStringRx)
{
    std::vector<uint8_t> buffer{0xAA, 0xBB, 0xCC};
    uint8_t tag = 0x12;
    eid_t eid = 5;

    std::string result = utils::convertMsgToString(utils::Rx, buffer, tag, eid);

    // Should contain "Rx", EID, tag
    EXPECT_NE(result.find("Rx"), std::string::npos);
    EXPECT_NE(result.find("5"), std::string::npos); // EID
}

TEST(StringUtils, testSplitBasic)
{
    std::string input = "one,two,three";
    auto result = utils::split(input, ",");

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "one");
    EXPECT_EQ(result[1], "two");
    EXPECT_EQ(result[2], "three");
}

TEST(StringUtils, testSplitWithTrim)
{
    std::string input = " one , two , three ";
    auto result = utils::split(input, ",", " ");

    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "one");
    EXPECT_EQ(result[1], "two");
    EXPECT_EQ(result[2], "three");
}

TEST(StringUtils, testSplitEmptyString)
{
    std::string input = "";
    auto result = utils::split(input, ",");

    EXPECT_TRUE(result.empty());
}

TEST(StringUtils, testGetCurrentSystemTime)
{
    std::string timeStr = utils::getCurrentSystemTime();

    // Should return a non-empty string with timestamp format
    EXPECT_FALSE(timeStr.empty());
    // Basic sanity: should contain date/time separators
    EXPECT_NE(timeStr.find("-"), std::string::npos); // Date separator
    EXPECT_NE(timeStr.find(":"), std::string::npos); // Time separator
}

// ============================================================================
// UUID/EID Conversion Tests
// ============================================================================

TEST(UuidEidConversion, testGetUUIDFromEIDFound)
{
    // Create a test EID table
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;

    std::string uuid1 = "test-uuid-123";
    std::string uuid2 = "test-uuid-456";
    eid_t eid1 = 10;
    eid_t eid2 = 20;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    eidTable.emplace(uuid1, std::make_tuple(eid1, medium, binding));
    eidTable.emplace(uuid2, std::make_tuple(eid2, medium, binding));

    auto result = utils::getUUIDFromEID(eidTable, eid1);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), uuid1);
}

TEST(UuidEidConversion, testGetUUIDFromEIDNotFound)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;

    std::string uuid1 = "test-uuid-123";
    eid_t eid1 = 10;
    eid_t eid_not_found = 99;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    eidTable.emplace(uuid1, std::make_tuple(eid1, medium, binding));

    auto result = utils::getUUIDFromEID(eidTable, eid_not_found);

    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Bitmap/Bitfield Conversion Tests
// ============================================================================

TEST(BitmapConversion, testBitfield256ToBitMap)
{
    bitfield256_t bitfield;
    memset(&bitfield, 0, sizeof(bitfield));
    bitfield.fields[0].byte = 0b00000101; // Bits 0 and 2 set

    auto result = utils::bitfield256_tToBitMap(bitfield);

    // Function returns 32-byte array (256 bits / 8 bits per byte)
    EXPECT_EQ(result.size(), 32);
    // First byte should have bits 0 and 2 set (value 0b00000101 = 5)
    EXPECT_EQ(result[0], 0b00000101);
}

TEST(BitmapConversion, testBitfield256ToBitArray)
{
    bitfield256_t bitfield;
    memset(&bitfield, 0, sizeof(bitfield));
    bitfield.fields[0].byte = 0x0F0D0B0A; // Set bytes: 0x0A, 0x0B, 0x0D, 0x0F

    auto result = utils::bitfield256_tToBitArray(bitfield);

    // Function returns 32-byte array representation of 256-bit bitfield
    ASSERT_EQ(result.size(), 32);
    // First 32-bit field (fields[0]) is split into 4 bytes (big-endian)
    EXPECT_EQ(result[0], 0x0F); // Most significant byte
    EXPECT_EQ(result[1], 0x0D);
    EXPECT_EQ(result[2], 0x0B);
    EXPECT_EQ(result[3], 0x0A); // Least significant byte
}

TEST(BitmapConversion, testVectorTo256BitHexString)
{
    std::vector<uint8_t> data(32, 0); // 32 bytes = 256 bits
    data[0] = 0xAB;
    data[1] = 0xCD;
    data[31] = 0xEF;

    std::string result = utils::vectorTo256BitHexString(data);

    // Should be a hex string representation (lowercase)
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("ab"), std::string::npos); // First byte (lowercase)
    EXPECT_NE(result.find("cd"), std::string::npos); // Second byte (lowercase)
}

// ============================================================================
// Type Conversion Tests (Safe Conversions)
// ============================================================================

// ============================================================================
// NSM Code/Reason String Conversion Tests
// ============================================================================

TEST(NsmStringConversion, testNsmCompletionCodeToStringSuccess)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_SUCCESS);

    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("SUCCESS"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringError)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_ERROR);

    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("ERROR"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringKnownCode)
{
    // Test with a known reason code (exact values depend on implementation)
    uint16_t reason_code = 0x0000; // Typically "No error" or similar

    std::string result = utils::nsmReasonCodeToString(reason_code);

    EXPECT_FALSE(result.empty());
}

TEST(NsmStringConversion, testNsmReasonCodeToStringUnknownCode)
{
    uint16_t unknown_code = 0xFFFF;

    std::string result = utils::nsmReasonCodeToString(unknown_code);

    // Should return some representation even for unknown codes
    EXPECT_FALSE(result.empty());
}

// ============================================================================
// Device Type/Role Combination Test
// ============================================================================

TEST(DeviceUtils, testCombineDeviceTypeAndRole)
{
    uint8_t deviceType = 1; // e.g., SWITCH
    uint8_t deviceRole = 2; // e.g., some role

    uint16_t result = utils::combineDeviceTypeAndRole(deviceType, deviceRole);

    // Function combines: (role << 8) | type (role in high byte, type in low
    // byte)
    EXPECT_EQ(result >> 8, deviceRole);
    EXPECT_EQ(result & 0xFF, deviceType);
}

TEST(DeviceUtils, testGetDeviceInstanceName)
{
    std::string result = utils::getDeviceInstanceName(NSM_DEV_ID_GPU, 5);
    EXPECT_EQ(result, "GPU_5"); // Function adds underscore separator

    result = utils::getDeviceInstanceName(NSM_DEV_ID_SWITCH, 0);
    EXPECT_EQ(result, "SWITCH_0"); // Function adds underscore separator
}

// ============================================================================
// Utility Functions Tests (Batch 1 remaining)
// ============================================================================

TEST(UtilityFunctions, testRequestMsgToHexString)
{
    // Test with sample message buffer
    std::vector<uint8_t> msg = {0xAB, 0xCD, 0xEF, 0x12, 0x34};

    std::string result = utils::requestMsgToHexString(msg);

    // Should format as hex with spaces: "ab cd ef 12 34 "
    EXPECT_EQ(result, "ab cd ef 12 34 ");
}

TEST(UtilityFunctions, testRequestMsgToHexStringEmpty)
{
    std::vector<uint8_t> msg;

    std::string result = utils::requestMsgToHexString(msg);

    // Empty message should return empty string
    EXPECT_EQ(result, "");
}

TEST(UtilityFunctions, testRequestMsgToHexStringSingleByte)
{
    std::vector<uint8_t> msg = {0xFF};

    std::string result = utils::requestMsgToHexString(msg);

    EXPECT_EQ(result, "ff ");
}

TEST(UtilityFunctions, testGetCurrentSteadyClockTimestamp)
{
    // Get timestamp twice with small delay
    uint64_t t1 = utils::getCurrentSteadyClockTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    uint64_t t2 = utils::getCurrentSteadyClockTimestamp();

    // Second timestamp should be greater than first
    EXPECT_GT(t2, t1);

    // Difference should be at least 10ms (with some tolerance for timing)
    EXPECT_GE(t2 - t1, 9);

    // Should be reasonable (not zero, not absurdly large)
    EXPECT_GT(t1, 0);
}

TEST(UtilityFunctions, testGetCurrentSteadyClockTimestampUs)
{
    // Get timestamp twice with small delay
    uint64_t t1 = utils::getCurrentSteadyClockTimestampUs();
    std::this_thread::sleep_for(std::chrono::microseconds(500));
    uint64_t t2 = utils::getCurrentSteadyClockTimestampUs();

    // Second timestamp should be greater than first
    EXPECT_GT(t2, t1);

    // Difference should be at least 500us (with some tolerance)
    EXPECT_GE(t2 - t1, 400);

    // Should be reasonable (not zero, not absurdly large)
    EXPECT_GT(t1, 0);
}

TEST(UtilityFunctions, testSteadyClockTimestampRelationship)
{
    // Microsecond timestamp should be ~1000x millisecond timestamp
    uint64_t tsMs = utils::getCurrentSteadyClockTimestamp();
    uint64_t tsUs = utils::getCurrentSteadyClockTimestampUs();

    // Convert ms to us and compare
    // Allow 100ms (100000us) tolerance for Valgrind environments
    // where clock calls can be significantly slower
    uint64_t tsMsAsUs = tsMs * 1000;
    uint64_t diff = (tsUs > tsMsAsUs) ? (tsUs - tsMsAsUs) : (tsMsAsUs - tsUs);

    EXPECT_LT(diff, 100000);
}

// Total tests added: ~47
// Functions covered: ~18 of 27 uncovered utility functions
// Session additions (Batch 1 remaining):
//  - Utility functions: 7 tests (3 functions)
//    - requestMsgToHexString (3 tests)
//    - getCurrentSteadyClockTimestamp (1 test)
//    - getCurrentSteadyClockTimestampUs (1 test)
//    - Timestamp relationship verification (1 test)
//    - getDeviceInstanceName (1 test - was missing explicit test)
//
// Remaining uncovered (require mocks or are low-value logging):
//  - printBuffer (2 overloads) - logging utility, low ROI
//  - appendBufferToFd - requires file descriptor mocking (write syscall)
//  - CustomFD operations - explicitly deleted (copy/move) or require fd mocking
//  - Some internal helper functions
// These require infrastructure or have very low ROI for coverage target

// ============================================================================
// NSM SW Code String Conversion Tests (Complete Coverage)
// ============================================================================

TEST(NsmStringConversion, testNsmSwCodeToStringSuccess)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_SUCCESS);
    EXPECT_NE(result.find("NSM_SW_SUCCESS"), std::string::npos);
    EXPECT_NE(result.find("(0)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringError)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_ERROR);
    EXPECT_NE(result.find("NSM_SW_ERROR"), std::string::npos);
    EXPECT_NE(result.find("(1)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringErrorData)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_ERROR_DATA);
    EXPECT_NE(result.find("NSM_SW_ERROR_DATA"), std::string::npos);
    EXPECT_NE(result.find("(2)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringErrorLength)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_ERROR_LENGTH);
    EXPECT_NE(result.find("NSM_SW_ERROR_LENGTH"), std::string::npos);
    EXPECT_NE(result.find("(3)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringErrorNull)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_ERROR_NULL);
    EXPECT_NE(result.find("NSM_SW_ERROR_NULL"), std::string::npos);
    EXPECT_NE(result.find("(4)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringErrorCommandFail)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_ERROR_COMMAND_FAIL);
    EXPECT_NE(result.find("NSM_SW_ERROR_COMMAND_FAIL"), std::string::npos);
    EXPECT_NE(result.find("(5)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringErrorTimeout)
{
    std::string result = utils::nsmSwCodeToString(NSM_SW_ERROR_TIMEOUT);
    EXPECT_NE(result.find("NSM_SW_ERROR_TIMEOUT"), std::string::npos);
    EXPECT_NE(result.find("(6)"), std::string::npos);
}

TEST(NsmStringConversion, testNsmSwCodeToStringUnknown)
{
    std::string result = utils::nsmSwCodeToString(99);
    EXPECT_NE(result.find("UNKNOWN"), std::string::npos);
    EXPECT_NE(result.find("(99)"), std::string::npos);
}

// ============================================================================
// NSM Completion Code String Conversion Tests (Complete Coverage)
// ============================================================================

TEST(NsmStringConversion, testNsmCompletionCodeToStringInvalidData)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_ERR_INVALID_DATA);
    EXPECT_NE(result.find("NSM_ERR_INVALID_DATA"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringInvalidDataLength)
{
    std::string result =
        utils::nsmCompletionCodeToString(NSM_ERR_INVALID_DATA_LENGTH);
    EXPECT_NE(result.find("NSM_ERR_INVALID_DATA_LENGTH"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringNotReady)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_ERR_NOT_READY);
    EXPECT_NE(result.find("NSM_ERR_NOT_READY"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringUnsupportedCommand)
{
    std::string result =
        utils::nsmCompletionCodeToString(NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    EXPECT_NE(result.find("NSM_ERR_UNSUPPORTED_COMMAND_CODE"),
              std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringUnsupportedMsgType)
{
    std::string result =
        utils::nsmCompletionCodeToString(NSM_ERR_UNSUPPORTED_MSG_TYPE);
    EXPECT_NE(result.find("NSM_ERR_UNSUPPORTED_MSG_TYPE"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringAccepted)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_ACCEPTED);
    EXPECT_NE(result.find("NSM_ACCEPTED"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringBusy)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_BUSY);
    EXPECT_NE(result.find("NSM_BUSY"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringBusAccess)
{
    std::string result = utils::nsmCompletionCodeToString(NSM_ERR_BUS_ACCESS);
    EXPECT_NE(result.find("NSM_ERR_BUS_ACCESS"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringInvalidState)
{
    std::string result =
        utils::nsmCompletionCodeToString(NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_NE(result.find("NSM_ERR_INVALID_STATE_FOR_COMMAND"),
              std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringInvalidRequestType)
{
    std::string result =
        utils::nsmCompletionCodeToString(NSM_ERR_INVALID_REQUEST_TYPE);
    EXPECT_NE(result.find("NSM_ERR_INVALID_REQUEST_TYPE"), std::string::npos);
}

TEST(NsmStringConversion, testNsmCompletionCodeToStringUnknownCode)
{
    std::string result = utils::nsmCompletionCodeToString(0xFF);
    EXPECT_NE(result.find("UNKNOWN"), std::string::npos);
}

// ============================================================================
// NSM Reason Code String Conversion Tests (Complete Coverage)
// ============================================================================

TEST(NsmStringConversion, testNsmReasonCodeToStringInvalidPci)
{
    std::string result = utils::nsmReasonCodeToString(ERR_INVALID_PCI);
    EXPECT_NE(result.find("ERR_INVALID_PCI"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringInvalidRqd)
{
    std::string result = utils::nsmReasonCodeToString(ERR_INVALID_RQD);
    EXPECT_NE(result.find("ERR_INVALID_RQD"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringTimeout)
{
    std::string result = utils::nsmReasonCodeToString(ERR_TIMEOUT);
    EXPECT_NE(result.find("ERR_TIMEOUT"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringDownstreamTimeout)
{
    std::string result = utils::nsmReasonCodeToString(ERR_DOWNSTREAM_TIMEOUT);
    EXPECT_NE(result.find("ERR_DOWNSTREAM_TIMEOUT"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringI2cNackDevAddr)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_I2C_NACK_FROM_DEV_ADDR);
    EXPECT_NE(result.find("ERR_I2C_NACK_FROM_DEV_ADDR"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringI2cNackDevCmdData)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_I2C_NACK_FROM_DEV_CMD_DATA);
    EXPECT_NE(result.find("ERR_I2C_NACK_FROM_DEV_CMD_DATA"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringI2cNackDevAddrRs)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_I2C_NACK_FROM_DEV_ADDR_RS);
    EXPECT_NE(result.find("ERR_I2C_NACK_FROM_DEV_ADDR_RS"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringNvlinkPortInvalid)
{
    std::string result = utils::nsmReasonCodeToString(ERR_NVLINK_PORT_INVALID);
    EXPECT_NE(result.find("ERR_NVLINK_PORT_INVALID"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringNvlinkPortDisabled)
{
    std::string result = utils::nsmReasonCodeToString(ERR_NVLINK_PORT_DISABLED);
    EXPECT_NE(result.find("ERR_NVLINK_PORT_DISABLED"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringNotSupported)
{
    std::string result = utils::nsmReasonCodeToString(ERR_NOT_SUPPORTED);
    EXPECT_NE(result.find("ERR_NOT_SUPPORTED"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringPropertyNotSupported)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_PROPERTY_NOT_SUPPORTED);
    EXPECT_NE(result.find("ERR_PROPERTY_NOT_SUPPORTED"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringLifespanVolatileNotSupported)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_LIFESPAN_VOLATILE_NOT_SUPPORTED);
    EXPECT_NE(result.find("ERR_LIFESPAN_VOLATILE_NOT_SUPPORTED"),
              std::string::npos);
}

TEST(NsmStringConversion,
     testNsmReasonCodeToStringLifespanPersistentNotSupported)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_LIFESPAN_PERSISTENT_NOT_SUPPORTED);
    EXPECT_NE(result.find("ERR_LIFESPAN_PERSISTENT_NOT_SUPPORTED"),
              std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringNoBootComplete)
{
    std::string result = utils::nsmReasonCodeToString(ERR_NO_BOOT_COMPLETE);
    EXPECT_NE(result.find("ERR_NO_BOOT_COMPLETE"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringUpdateInProgress)
{
    std::string result = utils::nsmReasonCodeToString(ERR_UPDATE_IN_PROGRESS);
    EXPECT_NE(result.find("ERR_UPDATE_IN_PROGRESS"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringImageCopyInProgress)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_IMAGE_COPY_IN_PROGRESS);
    EXPECT_NE(result.find("ERR_IMAGE_COPY_IN_PROGRESS"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringImageCopyCompleted)
{
    std::string result = utils::nsmReasonCodeToString(ERR_IMAGE_COPY_COMPLETED);
    EXPECT_NE(result.find("ERR_IMAGE_COPY_COMPLETED"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringFlashWearMitigation)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_FLASH_WEAR_MITIGATION);
    EXPECT_NE(result.find("ERR_FLASH_WEAR_MITIGATION"), std::string::npos);
}

TEST(NsmStringConversion, testNsmReasonCodeToStringIncompleteComponentSet)
{
    std::string result =
        utils::nsmReasonCodeToString(ERR_INCOMPLETE_COMPONENT_SET);
    EXPECT_NE(result.find("ERR_INCOMPLETE_COMPONENT_SET"), std::string::npos);
}

// ============================================================================
// Device Type and Role Extraction Tests
// ============================================================================

TEST(DeviceUtils, testGetDeviceTypeAndRoleExtraction)
{
    uint8_t deviceType = 0;
    uint8_t deviceRole = 0;
    uint16_t combined = utils::combineDeviceTypeAndRole(3, 5);

    utils::getDeviceTypeAndRole(combined, &deviceType, &deviceRole);

    EXPECT_EQ(deviceType, 3);
    EXPECT_EQ(deviceRole, 5);
}

TEST(DeviceUtils, testGetDeviceTypeAndRoleZeroValues)
{
    uint8_t deviceType = 0xFF;
    uint8_t deviceRole = 0xFF;
    uint16_t combined = utils::combineDeviceTypeAndRole(0, 0);

    utils::getDeviceTypeAndRole(combined, &deviceType, &deviceRole);

    EXPECT_EQ(deviceType, 0);
    EXPECT_EQ(deviceRole, 0);
}

TEST(DeviceUtils, testGetDeviceTypeAndRoleMaxValues)
{
    uint8_t deviceType = 0;
    uint8_t deviceRole = 0;
    uint16_t combined = utils::combineDeviceTypeAndRole(0xFF, 0xFF);

    utils::getDeviceTypeAndRole(combined, &deviceType, &deviceRole);

    EXPECT_EQ(deviceType, 0xFF);
    EXPECT_EQ(deviceRole, 0xFF);
}

// ============================================================================
// Scaling and Conversion Tests with Invalid Value Handling
// ============================================================================

TEST(TypeConversion, testConvertAndScaleDownUint32ToDoubleNormalValue)
{
    double result = utils::convertAndScaleDownUint32ToDouble(1000, 100.0);
    EXPECT_DOUBLE_EQ(result, 10.0);
}

TEST(TypeConversion, testConvertAndScaleDownUint32ToDoubleInvalidValue)
{
    double result = utils::convertAndScaleDownUint32ToDouble(0xFFFFFFFF, 100.0);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFFFFFFFF));
}

TEST(TypeConversion, testConvertAndScaleDownUint32ToDoubleZero)
{
    double result = utils::convertAndScaleDownUint32ToDouble(0, 100.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(TypeConversion, testConvertAndScaleDownUint8ToDoubleNormalValue)
{
    double result = utils::convertAndScaleDownUint8ToDouble(100);
    EXPECT_DOUBLE_EQ(result, 100.0);
}

TEST(TypeConversion, testConvertAndScaleDownUint8ToDoubleInvalidValue)
{
    double result = utils::convertAndScaleDownUint8ToDouble(0xFF);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFF));
}

TEST(TypeConversion, testConvertAndScaleDownUint8ToDoubleZero)
{
    double result = utils::convertAndScaleDownUint8ToDouble(0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// ============================================================================
// Update Methods Bitfield to List Conversion Tests
// ============================================================================

TEST(BitmapConversion, testUpdateMethodsBitfieldToListEmpty)
{
    bitfield32_t updateMethodBitfield = {0};
    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);
    EXPECT_TRUE(result.empty());
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListAutomatic)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit0 = 1; // Automatic

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::Automatic);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListMediumSpecificReset)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit2 = 1; // MediumSpecificReset

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0],
              sdbusplus::common::xyz::openbmc_project::software::
                  SecurityCommon::UpdateMethods::MediumSpecificReset);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListSystemReboot)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit3 = 1; // SystemReboot

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::SystemReboot);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListDCPowerCycle)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit4 = 1; // DCPowerCycle

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::DCPowerCycle);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListACPowerCycle)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit5 = 1; // ACPowerCycle

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::ACPowerCycle);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListWarmReset)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit16 = 1; // WarmReset

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::WarmReset);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListHotReset)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit17 = 1; // HotReset

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::HotReset);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListFLR)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit18 = 1; // FLR

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], sdbusplus::common::xyz::openbmc_project::software::
                             SecurityCommon::UpdateMethods::FLR);
}

TEST(BitmapConversion, testUpdateMethodsBitfieldToListMultipleBits)
{
    bitfield32_t updateMethodBitfield = {0};
    updateMethodBitfield.bits.bit0 = 1;  // Automatic
    updateMethodBitfield.bits.bit3 = 1;  // SystemReboot
    updateMethodBitfield.bits.bit17 = 1; // HotReset

    auto result = utils::updateMethodsBitfieldToList(updateMethodBitfield);

    EXPECT_EQ(result.size(), 3);
}

// ============================================================================
// Bitfield256 Additional Tests
// ============================================================================

TEST(Bitfield256, testIsAnyBitSetEmpty)
{
    utils::Bitfield256 bf;
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(Bitfield256, testIsAnyBitSetAfterSet)
{
    utils::Bitfield256 bf;
    bf.setBit(5);
    EXPECT_TRUE(bf.isAnyBitSet());
}

TEST(Bitfield256, testIsAnyBitSetMultipleBits)
{
    utils::Bitfield256 bf;
    bf.setBit(0);
    bf.setBit(100);
    bf.setBit(255);
    EXPECT_TRUE(bf.isAnyBitSet());
}

TEST(Bitfield256, testClearAfterSet)
{
    utils::Bitfield256 bf;
    bf.setBit(10);
    bf.setBit(50);
    EXPECT_TRUE(bf.isAnyBitSet());

    bf.clear();
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(Bitfield256, testClearEmptyBitfield)
{
    utils::Bitfield256 bf;
    bf.clear();
    EXPECT_FALSE(bf.isAnyBitSet());
}

// ============================================================================
// Bitfield to Bool Vector Conversion Tests
// ============================================================================

TEST(BitmapConversion, testConvertBitfieldToVectorAllZeros)
{
    std::vector<uint8_t> bitfield = {0x00, 0x00, 0x00};
    std::vector<bool> supportedMetrics;

    utils::convertBitfieldToVector(bitfield, supportedMetrics);

    EXPECT_EQ(supportedMetrics.size(), 24); // 3 bytes * 8 bits
    for (const auto& bit : supportedMetrics)
    {
        EXPECT_FALSE(bit);
    }
}

TEST(BitmapConversion, testConvertBitfieldToVectorAllOnes)
{
    std::vector<uint8_t> bitfield = {0xFF, 0xFF};
    std::vector<bool> supportedMetrics;

    utils::convertBitfieldToVector(bitfield, supportedMetrics);

    EXPECT_EQ(supportedMetrics.size(), 16); // 2 bytes * 8 bits
    for (const auto& bit : supportedMetrics)
    {
        EXPECT_TRUE(bit);
    }
}

TEST(BitmapConversion, testConvertBitfieldToVectorMixedBits)
{
    std::vector<uint8_t> bitfield = {0xAA}; // 10101010
    std::vector<bool> supportedMetrics;

    utils::convertBitfieldToVector(bitfield, supportedMetrics);

    EXPECT_EQ(supportedMetrics.size(), 8);
    EXPECT_FALSE(supportedMetrics[0]); // bit 0
    EXPECT_TRUE(supportedMetrics[1]);  // bit 1
    EXPECT_FALSE(supportedMetrics[2]); // bit 2
    EXPECT_TRUE(supportedMetrics[3]);  // bit 3
    EXPECT_FALSE(supportedMetrics[4]); // bit 4
    EXPECT_TRUE(supportedMetrics[5]);  // bit 5
    EXPECT_FALSE(supportedMetrics[6]); // bit 6
    EXPECT_TRUE(supportedMetrics[7]);  // bit 7
}

TEST(BitmapConversion, testConvertBitfieldToVectorEmptyInput)
{
    std::vector<uint8_t> bitfield;
    std::vector<bool> supportedMetrics;

    utils::convertBitfieldToVector(bitfield, supportedMetrics);

    EXPECT_TRUE(supportedMetrics.empty());
}

// ============================================================================
// getEidFromUUID Tests
// ============================================================================

TEST(UuidEidConversion, getEidFromUUID_Found_ReturnsCorrectEid)
{
    // Arrange
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    std::string uuid = "00010203-0405-0607-0809-0a0b0c0d0e0f";
    eid_t expectedEid = 42;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    eidTable.emplace(uuid, std::make_tuple(expectedEid, medium, binding));

    // Act
    eid_t result = utils::getEidFromUUID(eidTable, uuid);

    // Assert
    EXPECT_EQ(result, expectedEid);
}

TEST(UuidEidConversion, getEidFromUUID_NotFound_ReturnsMax)
{
    // Arrange
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    std::string uuid = "00010203-0405-0607-0809-0a0b0c0d0e0f";
    eid_t someEid = 10;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    eidTable.emplace(uuid, std::make_tuple(someEid, medium, binding));

    std::string otherUuid = "11111111-2222-3333-4444-555555555555";

    // Act
    eid_t result = utils::getEidFromUUID(eidTable, otherUuid);

    // Assert - should return max uint8_t when not found
    EXPECT_EQ(result, std::numeric_limits<uint8_t>::max());
}

TEST(UuidEidConversion, getEidFromUUID_EmptyTable_ReturnsMax)
{
    // Arrange
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    std::string uuid = "00010203-0405-0607-0809-0a0b0c0d0e0f";

    // Act
    eid_t result = utils::getEidFromUUID(eidTable, uuid);

    // Assert
    EXPECT_EQ(result, std::numeric_limits<uint8_t>::max());
}

// ============================================================================
// parseStaticUuid Tests
// ============================================================================

TEST(ParseStaticUuid, ValidFormat_ReturnsZero)
{
    // Arrange
    uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    uint8_t deviceType = 0xFF;
    uint8_t instanceNumber = 0xFF;
    uint8_t deviceRole = 0xFF;
    std::string remapPropName;
    std::vector<std::string> remapPropValues;

    // Act
    int result = utils::parseStaticUuid(uuid, deviceType, instanceNumber,
                                        deviceRole, remapPropName,
                                        remapPropValues);

    // Assert
    EXPECT_EQ(result, 0);
    EXPECT_EQ(deviceType, 0);
    EXPECT_EQ(instanceNumber, 0);
    EXPECT_EQ(remapPropName, "NSM_DEVICE_INSTANCE_NUMBER");
    ASSERT_EQ(remapPropValues.size(), 1);
    EXPECT_EQ(remapPropValues[0], "0");
}

TEST(ParseStaticUuid, ValidFormatWithDeviceRole_ReturnsZero)
{
    // Arrange - deviceType 3 means BASEBOARD, combined with role=5 -> (5<<8)|3
    // = 1283
    uuid_t uuid = "STATIC:1283:2:MCTP_EID:10";
    uint8_t deviceType = 0xFF;
    uint8_t instanceNumber = 0xFF;
    uint8_t deviceRole = 0xFF;
    std::string remapPropName;
    std::vector<std::string> remapPropValues;

    // Act
    int result = utils::parseStaticUuid(uuid, deviceType, instanceNumber,
                                        deviceRole, remapPropName,
                                        remapPropValues);

    // Assert
    EXPECT_EQ(result, 0);
    EXPECT_EQ(deviceType, 3); // low byte of 1283
    EXPECT_EQ(deviceRole, 5); // high byte of 1283
    EXPECT_EQ(instanceNumber, 2);
    EXPECT_EQ(remapPropName, "MCTP_EID");
    ASSERT_EQ(remapPropValues.size(), 1);
    EXPECT_EQ(remapPropValues[0], "10");
}

TEST(ParseStaticUuid, InvalidFormat_ReturnsNegative)
{
    // Arrange - not starting with STATIC:
    uuid_t uuid = "NOT_STATIC:0:0:PROP:VAL";
    uint8_t deviceType, instanceNumber, deviceRole;
    std::string remapPropName;
    std::vector<std::string> remapPropValues;

    // Act
    int result = utils::parseStaticUuid(uuid, deviceType, instanceNumber,
                                        deviceRole, remapPropName,
                                        remapPropValues);

    // Assert
    EXPECT_LT(result, 0);
}

TEST(ParseStaticUuid, EmptyUuid_ReturnsNegative)
{
    // Arrange
    uuid_t uuid = "";
    uint8_t deviceType, instanceNumber, deviceRole;
    std::string remapPropName;
    std::vector<std::string> remapPropValues;

    // Act
    int result = utils::parseStaticUuid(uuid, deviceType, instanceNumber,
                                        deviceRole, remapPropName,
                                        remapPropValues);

    // Assert
    EXPECT_LT(result, 0);
}

TEST(ParseStaticUuid, MultipleValues_ParsesCorrectly)
{
    // Arrange - multiple colon-separated values
    uuid_t uuid = "STATIC:0:0:MCTP_UUID:val1:val2";
    uint8_t deviceType, instanceNumber, deviceRole;
    std::string remapPropName;
    std::vector<std::string> remapPropValues;

    // Act
    int result = utils::parseStaticUuid(uuid, deviceType, instanceNumber,
                                        deviceRole, remapPropName,
                                        remapPropValues);

    // Assert
    EXPECT_EQ(result, 0);
    EXPECT_EQ(remapPropName, "MCTP_UUID");
    ASSERT_GE(remapPropValues.size(), 2u);
    EXPECT_EQ(remapPropValues[0], "val1");
    EXPECT_EQ(remapPropValues[1], "val2");
}

// ============================================================================
// uint64ToDoubleSafeConvert Tests
// ============================================================================

TEST(TypeConversion, uint64ToDoubleSafeConvert_NormalValue_ReturnsExact)
{
    // Arrange
    uint64_t value = 1000;

    // Act
    double result = utils::uint64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, 1000.0);
}

TEST(TypeConversion, uint64ToDoubleSafeConvert_Zero_ReturnsZero)
{
    // Arrange
    uint64_t value = 0;

    // Act
    double result = utils::uint64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(TypeConversion, uint64ToDoubleSafeConvert_MaxSafeValue_ReturnsExact)
{
    // Arrange - MAX_SAFE_INTEGER_IN_DOUBLE = (1ULL << 53) - 1
    uint64_t value = (1ULL << 53) - 1;

    // Act
    double result = utils::uint64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, static_cast<double>(value));
}

TEST(TypeConversion, uint64ToDoubleSafeConvert_ExceedsSafe_ReturnsCapped)
{
    // Arrange - value beyond safe range
    uint64_t value = (1ULL << 53) + 100;

    // Act
    double result = utils::uint64ToDoubleSafeConvert(value);

    // Assert - should be capped to MAX_SAFE_INTEGER_IN_DOUBLE
    EXPECT_DOUBLE_EQ(result, static_cast<double>((1ULL << 53) - 1));
}

TEST(TypeConversion, uint64ToDoubleSafeConvert_MaxUint64_ReturnsCapped)
{
    // Arrange
    uint64_t value = std::numeric_limits<uint64_t>::max();

    // Act
    double result = utils::uint64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, static_cast<double>((1ULL << 53) - 1));
}

// ============================================================================
// int64ToDoubleSafeConvert Tests
// ============================================================================

TEST(TypeConversion, int64ToDoubleSafeConvert_PositiveNormal_ReturnsExact)
{
    // Arrange
    int64_t value = 12345;

    // Act
    double result = utils::int64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, 12345.0);
}

TEST(TypeConversion, int64ToDoubleSafeConvert_NegativeNormal_ReturnsExact)
{
    // Arrange
    int64_t value = -67890;

    // Act
    double result = utils::int64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, -67890.0);
}

TEST(TypeConversion, int64ToDoubleSafeConvert_Zero_ReturnsZero)
{
    // Arrange
    int64_t value = 0;

    // Act
    double result = utils::int64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(TypeConversion, int64ToDoubleSafeConvert_PositiveExceedsSafe_ReturnsCapped)
{
    // Arrange - positive value beyond safe range
    int64_t value = static_cast<int64_t>((1ULL << 53) + 100);

    // Act
    double result = utils::int64ToDoubleSafeConvert(value);

    // Assert
    EXPECT_DOUBLE_EQ(result, static_cast<double>((1ULL << 53) - 1));
}

TEST(TypeConversion, int64ToDoubleSafeConvert_NegativeExceedsSafe_ReturnsCapped)
{
    // Arrange - negative value beyond safe range
    int64_t value = -static_cast<int64_t>((1ULL << 53) + 100);

    // Act
    double result = utils::int64ToDoubleSafeConvert(value);

    // Assert
    // Use 1LL (signed) to get correct negative value; 1ULL would wrap around
    EXPECT_DOUBLE_EQ(result, static_cast<double>(-((1LL << 53) - 1)));
}

TEST(TypeConversion, int64ToDoubleSafeConvert_MinInt64_ReturnsCapped)
{
    // Arrange
    int64_t value = std::numeric_limits<int64_t>::min();

    // Act
    double result = utils::int64ToDoubleSafeConvert(value);

    // Assert - capped to negative MAX_SAFE_INTEGER_IN_DOUBLE
    EXPECT_DOUBLE_EQ(result, static_cast<double>(-((1LL << 53) - 1)));
}

// ============================================================================
// convertBitMaskToVector Tests
// ============================================================================

TEST(BitmapConversion, convertBitMaskToVector_AllZeros_EmptyResult)
{
    // Arrange
    bitfield8_t value[2] = {};
    value[0].byte = 0;
    value[1].byte = 0;
    std::vector<uint8_t> data;

    // Act
    utils::convertBitMaskToVector(data, value, 2);

    // Assert
    EXPECT_TRUE(data.empty());
}

TEST(BitmapConversion, convertBitMaskToVector_AllOnes_AllBitsPresent)
{
    // Arrange
    bitfield8_t value[1] = {};
    value[0].byte = 0xFF;
    std::vector<uint8_t> data;

    // Act
    utils::convertBitMaskToVector(data, value, 1);

    // Assert - all 8 bits should be in the vector
    ASSERT_EQ(data.size(), 8u);
    EXPECT_EQ(data[0], 0); // bit 0
    EXPECT_EQ(data[1], 1); // bit 1
    EXPECT_EQ(data[2], 2); // bit 2
    EXPECT_EQ(data[3], 3); // bit 3
    EXPECT_EQ(data[4], 4); // bit 4
    EXPECT_EQ(data[5], 5); // bit 5
    EXPECT_EQ(data[6], 6); // bit 6
    EXPECT_EQ(data[7], 7); // bit 7
}

TEST(BitmapConversion, convertBitMaskToVector_SomeBits_CorrectIndices)
{
    // Arrange - 0b00000101 = bits 0 and 2 set
    bitfield8_t value[1] = {};
    value[0].byte = 0x05;
    std::vector<uint8_t> data;

    // Act
    utils::convertBitMaskToVector(data, value, 1);

    // Assert
    ASSERT_EQ(data.size(), 2u);
    EXPECT_EQ(data[0], 0); // bit 0
    EXPECT_EQ(data[1], 2); // bit 2
}

TEST(BitmapConversion, convertBitMaskToVector_MultipleBytes_CorrectOffsets)
{
    // Arrange - byte 0 has bit 0, byte 1 has bit 0 (which is bit 8 overall)
    bitfield8_t value[2] = {};
    value[0].byte = 0x01; // bit 0
    value[1].byte = 0x01; // bit 8
    std::vector<uint8_t> data;

    // Act
    utils::convertBitMaskToVector(data, value, 2);

    // Assert
    ASSERT_EQ(data.size(), 2u);
    EXPECT_EQ(data[0], 0); // bit 0 in byte 0
    EXPECT_EQ(data[1], 8); // bit 0 in byte 1 = index 8
}

// ============================================================================
// bitMapToBitfield256_t Tests
// ============================================================================

TEST(BitmapConversion, bitMapToBitfield256_t_ValidBitmap_ConvertsCorrectly)
{
    // Arrange
    std::vector<uint8_t> bitmap(32, 0);
    bitmap[0] = 0xAB; // MSB of first field
    bitmap[1] = 0xCD;
    bitmap[2] = 0xEF;
    bitmap[3] = 0x12; // LSB of first field

    // Act
    bitfield256_t result = utils::bitMapToBitfield256_t(bitmap);

    // Assert - fields[0] should contain the big-endian representation
    uint32_t expected = (0xAB << 24) | (0xCD << 16) | (0xEF << 8) | 0x12;
    EXPECT_EQ(result.fields[0].byte, expected);
}

TEST(BitmapConversion, bitMapToBitfield256_t_WrongSize_ReturnsZero)
{
    // Arrange - bitmap not 32 bytes
    std::vector<uint8_t> bitmap(16, 0xFF);

    // Act
    bitfield256_t result = utils::bitMapToBitfield256_t(bitmap);

    // Assert - all fields should be zero
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(result.fields[i].byte, 0u);
    }
}

TEST(BitmapConversion, bitMapToBitfield256_t_EmptyBitmap_ReturnsZero)
{
    // Arrange
    std::vector<uint8_t> bitmap;

    // Act
    bitfield256_t result = utils::bitMapToBitfield256_t(bitmap);

    // Assert
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(result.fields[i].byte, 0u);
    }
}

TEST(BitmapConversion, bitMapToBitfield256_t_AllOnes_CorrectResult)
{
    // Arrange
    std::vector<uint8_t> bitmap(32, 0xFF);

    // Act
    bitfield256_t result = utils::bitMapToBitfield256_t(bitmap);

    // Assert
    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(result.fields[i].byte, 0xFFFFFFFFu);
    }
}

// ============================================================================
// indicesToBitmap Tests
// ============================================================================

TEST(BitmapConversion, indicesToBitmap_SingleIndex_CorrectBitSet)
{
    // Arrange
    std::vector<uint8_t> indices = {3};

    // Act
    auto result = utils::indicesToBitmap(indices, 1);

    // Assert
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 0x08); // bit 3 set
}

TEST(BitmapConversion, indicesToBitmap_MultipleIndices_CorrectBitsSet)
{
    // Arrange
    std::vector<uint8_t> indices = {0, 2, 7};

    // Act
    auto result = utils::indicesToBitmap(indices, 1);

    // Assert
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], 0x85); // bits 0, 2, 7 set = 0b10000101
}

TEST(BitmapConversion, indicesToBitmap_EmptyIndices_ReturnsZeroBitmap)
{
    // Arrange
    std::vector<uint8_t> indices;

    // Act
    auto result = utils::indicesToBitmap(indices, 2);

    // Assert
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], 0);
    EXPECT_EQ(result[1], 0);
}

TEST(BitmapConversion, indicesToBitmap_ZeroSize_AutoSizes)
{
    // Arrange
    std::vector<uint8_t> indices = {9}; // requires at least 2 bytes

    // Act
    auto result = utils::indicesToBitmap(indices, 0);

    // Assert
    ASSERT_GE(result.size(), 2u);
    EXPECT_EQ(result[1], 0x02); // bit 1 in byte 1 (overall bit 9)
}

TEST(BitmapConversion, indicesToBitmap_SizeTooLarge_Throws)
{
    // Arrange
    std::vector<uint8_t> indices = {0};

    // Act & Assert
    EXPECT_THROW(utils::indicesToBitmap(indices, 9), std::invalid_argument);
}

TEST(BitmapConversion, indicesToBitmap_IndexOutOfBounds_Throws)
{
    // Arrange - index 16 requires at least 3 bytes, but we only allocate 1
    std::vector<uint8_t> indices = {16};

    // Act & Assert
    EXPECT_THROW(utils::indicesToBitmap(indices, 1), std::invalid_argument);
}

// ============================================================================
// CustomFD Tests
// ============================================================================

TEST(CustomFD, Constructor_ValidFd_CreatesObject)
{
    // Arrange
    int fd = memfd_create("test_customfd", 0);
    ASSERT_GE(fd, 0);

    // Act
    utils::CustomFD customFd(fd);

    // Assert
    EXPECT_EQ(static_cast<int>(customFd), fd);
    EXPECT_EQ(customFd(), fd);
    EXPECT_EQ(customFd.size(), 0u);
}

TEST(CustomFD, Constructor_InvalidFd_Throws)
{
    // Act & Assert
    EXPECT_THROW(utils::CustomFD(-1), std::runtime_error);
}

TEST(CustomFD, WriteAndRead_ValidData_Succeeds)
{
    // Arrange
    int fd = memfd_create("test_rw", 0);
    ASSERT_GE(fd, 0);
    // Pre-allocate space in the file
    std::vector<uint8_t> initData(10, 0);
    if (write(fd, initData.data(), initData.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD customFd(fd);

    std::vector<uint8_t> writeData = {0xAA, 0xBB, 0xCC, 0xDD};

    // Act - write at position 0
    bool writeOk = customFd.write(0, writeData.data(), writeData.size());

    // Assert
    EXPECT_TRUE(writeOk);

    // Act - read back
    std::vector<uint8_t> readData(4, 0);
    bool readOk = customFd.read(0, readData.data(), readData.size());

    // Assert
    EXPECT_TRUE(readOk);
    EXPECT_EQ(readData, writeData);
}

TEST(CustomFD, Write_BeyondFileSize_ReturnsFalse)
{
    // Arrange
    int fd = memfd_create("test_write_beyond", 0);
    ASSERT_GE(fd, 0);
    utils::CustomFD customFd(fd);

    std::vector<uint8_t> data = {0x01};

    // Act - write beyond file size (file is empty, pos 100 > size 0)
    bool result = customFd.write(100, data.data(), data.size());

    // Assert
    EXPECT_FALSE(result);
}

TEST(CustomFD, Read_BeyondFileSize_ReturnsFalse)
{
    // Arrange
    int fd = memfd_create("test_read_beyond", 0);
    ASSERT_GE(fd, 0);
    utils::CustomFD customFd(fd);

    std::vector<uint8_t> data(4, 0);

    // Act - read beyond file size (file is empty)
    bool result = customFd.read(0, data.data(), data.size());

    // Assert
    EXPECT_FALSE(result);
}

TEST(CustomFD, Write_NullData_ReturnsFalse)
{
    // Arrange
    int fd = memfd_create("test_null_write", 0);
    ASSERT_GE(fd, 0);
    // Write some data first to have a non-zero file size
    uint8_t dummy = 0;
    if (write(fd, &dummy, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD customFd(fd);

    // Act
    bool result = customFd.write(0, nullptr, 4);

    // Assert
    EXPECT_FALSE(result);
}

TEST(CustomFD, Read_NullData_ReturnsFalse)
{
    // Arrange
    int fd = memfd_create("test_null_read", 0);
    ASSERT_GE(fd, 0);
    uint8_t dummy = 0;
    if (write(fd, &dummy, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD customFd(fd);

    // Act
    bool result = customFd.read(0, nullptr, 1);

    // Assert
    EXPECT_FALSE(result);
}

TEST(CustomFD, Write_ZeroSize_ReturnsFalse)
{
    // Arrange
    int fd = memfd_create("test_zero_write", 0);
    ASSERT_GE(fd, 0);
    uint8_t dummy = 0;
    if (write(fd, &dummy, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD customFd(fd);
    uint8_t data = 0xAA;

    // Act
    bool result = customFd.write(0, &data, 0);

    // Assert
    EXPECT_FALSE(result);
}

TEST(CustomFD, Size_AfterWrite_UpdatesCorrectly)
{
    // Arrange
    int fd = memfd_create("test_size_update", 0);
    ASSERT_GE(fd, 0);
    // Pre-populate with 5 bytes
    std::vector<uint8_t> initData(5, 0);
    if (write(fd, initData.data(), initData.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD customFd(fd);
    EXPECT_EQ(customFd.size(), 5u);

    // Act - write 10 bytes starting at position 0 (extends file)
    std::vector<uint8_t> largeData(10, 0xAA);
    bool writeOk = customFd.write(0, largeData.data(), largeData.size());

    // Assert
    EXPECT_TRUE(writeOk);
    EXPECT_GE(customFd.size(), 10u);
}

// ============================================================================
// appendBufferToFd Tests
// ============================================================================

TEST(FileDescriptorOps, appendBufferToFd_ValidData_Appends)
{
    // Arrange
    int fd = memfd_create("test_append", 0);
    ASSERT_GE(fd, 0);
    std::vector<uint8_t> firstData = {0x01, 0x02};
    utils::writeBufferToFd(fd, firstData);

    std::vector<uint8_t> appendData = {0x03, 0x04, 0x05};

    // Act
    utils::appendBufferToFd(fd, appendData);

    // Assert - read back total data
    std::vector<uint8_t> readBack;
    utils::readFdToBuffer(fd, readBack);
    ASSERT_EQ(readBack.size(), 5u);
    EXPECT_EQ(readBack[0], 0x01);
    EXPECT_EQ(readBack[1], 0x02);
    EXPECT_EQ(readBack[2], 0x03);
    EXPECT_EQ(readBack[3], 0x04);
    EXPECT_EQ(readBack[4], 0x05);

    close(fd);
}

TEST(FileDescriptorOps, appendBufferToFd_InvalidFd_Throws)
{
    // Arrange
    std::vector<uint8_t> data = {0x01};

    // Act & Assert
    EXPECT_THROW(utils::appendBufferToFd(-1, data), std::runtime_error);
}

TEST(FileDescriptorOps, readFdToBuffer_InvalidFd_Throws)
{
    // Arrange
    std::vector<uint8_t> buffer;

    // Act & Assert
    EXPECT_THROW(utils::readFdToBuffer(-1, buffer), std::runtime_error);
}

TEST(FileDescriptorOps, writeBufferToFd_InvalidFd_Throws)
{
    // Arrange
    std::vector<uint8_t> data = {0x01, 0x02};

    // Act & Assert
    EXPECT_THROW(utils::writeBufferToFd(-1, data), std::runtime_error);
}

// ============================================================================
// Associations helper function tests
// ============================================================================

TEST(Associations, getAssociationsTuple_EmptyVector_ReturnsEmpty)
{
    // Arrange
    std::vector<utils::Association> emptyAssociations;

    // Act
    auto result = utils::getAssociations(emptyAssociations);

    // Assert
    EXPECT_TRUE(result.empty());
}

TEST(Associations, getAssociationsTuple_ValidAssociations_ReturnsTuples)
{
    // Arrange
    std::vector<utils::Association> associations;
    utils::Association assoc1;
    assoc1.forward = "contains";
    assoc1.backward = "contained_by";
    assoc1.absolutePath = "/xyz/openbmc_project/inventory/test";
    associations.push_back(assoc1);

    utils::Association assoc2;
    assoc2.forward = "parent";
    assoc2.backward = "child";
    assoc2.absolutePath = "/xyz/openbmc_project/inventory/test2";
    associations.push_back(assoc2);

    // Act
    auto result = utils::getAssociations(associations);

    // Assert
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(std::get<0>(result[0]), "contains");
    EXPECT_EQ(std::get<1>(result[0]), "contained_by");
    EXPECT_EQ(std::get<2>(result[0]), "/xyz/openbmc_project/inventory/test");
    EXPECT_EQ(std::get<0>(result[1]), "parent");
    EXPECT_EQ(std::get<1>(result[1]), "child");
    EXPECT_EQ(std::get<2>(result[1]), "/xyz/openbmc_project/inventory/test2");
}

// ============================================================================
// vectorTo256BitHexString Edge Case Tests
// ============================================================================

TEST(BitmapConversion, vectorTo256BitHexString_WrongSize_ReturnsDefaultZero)
{
    // Arrange - not 32 bytes
    std::vector<uint8_t> data(16, 0xAB);

    // Act
    std::string result = utils::vectorTo256BitHexString(data);

    // Assert - should return default 0x000...0
    EXPECT_EQ(result.size(), 66u); // "0x" + 64 hex chars
    EXPECT_EQ(result.substr(0, 2), "0x");
    EXPECT_EQ(result, "0x" + std::string(64, '0'));
}

TEST(BitmapConversion, vectorTo256BitHexString_AllZeros_ReturnsZeroString)
{
    // Arrange
    std::vector<uint8_t> data(32, 0x00);

    // Act
    std::string result = utils::vectorTo256BitHexString(data);

    // Assert
    EXPECT_EQ(result, "0x" + std::string(64, '0'));
}

TEST(BitmapConversion, vectorTo256BitHexString_AllFF_ReturnsAllFF)
{
    // Arrange
    std::vector<uint8_t> data(32, 0xFF);

    // Act
    std::string result = utils::vectorTo256BitHexString(data);

    // Assert
    EXPECT_EQ(result, "0x" + std::string(64, 'f'));
}

// ============================================================================
// MCTP Priority Additional Tests
// ============================================================================

TEST(MctpPriority, isPreferred_UnknownMedium_HandlesGracefully)
{
    // Arrange - Use unknown medium types
    MctpMedium unknownMedium1 =
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.Unknown1";
    MctpMedium unknownMedium2 =
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.Unknown2";
    MctpBinding unknownBinding =
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.Unknown";

    auto current = std::make_tuple(unknownMedium1, unknownBinding);
    auto newInfo = std::make_tuple(unknownMedium2, unknownBinding);

    // Act - should not crash even with unknown types
    // Both get MAX_INT priority, so they're equal; isPreferred returns true
    // for same priority
    bool result = utils::isPreferred(current, newInfo);

    // Assert - just ensure no crash; exact behavior depends on fallback
    (void)result;
    SUCCEED();
}

// ============================================================================
// printBuffer Tests (coverage for logging utility)
// ============================================================================

TEST(StringUtils, printBuffer_VectorOverload_NoCrash)
{
    // Arrange
    std::vector<uint8_t> buffer = {0x01, 0x02, 0x03};
    uint8_t tag = 0x10;
    eid_t eid = 5;

    // Act - just ensure no crash (prints to lg2 logger)
    utils::printBuffer(utils::Tx, buffer, tag, eid);
    utils::printBuffer(utils::Rx, buffer, tag, eid);

    // Assert
    SUCCEED();
}

TEST(StringUtils, printBuffer_PtrOverload_NoCrash)
{
    // Arrange
    std::vector<uint8_t> buffer = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t tag = 0x20;
    eid_t eid = 10;

    // Act - just ensure no crash
    utils::printBuffer(utils::Tx, buffer.data(), buffer.size(), tag, eid);
    utils::printBuffer(utils::Rx, buffer.data(), buffer.size(), tag, eid);

    // Assert
    SUCCEED();
}

TEST(StringUtils, printBuffer_EmptyVector_NoCrash)
{
    // Arrange
    std::vector<uint8_t> buffer;
    uint8_t tag = 0;
    eid_t eid = 0;

    // Act
    utils::printBuffer(utils::Tx, buffer, tag, eid);

    // Assert
    SUCCEED();
}

// ============================================================================
// convertMsgToString Additional Tests
// ============================================================================

TEST(StringUtils, convertMsgToString_EmptyBuffer_ReturnsEmpty)
{
    // Arrange
    std::vector<uint8_t> buffer;
    uint8_t tag = 0;
    eid_t eid = 0;

    // Act
    std::string result = utils::convertMsgToString(utils::Tx, buffer, tag, eid);

    // Assert - empty buffer returns "" (early return branch)
    EXPECT_TRUE(result.empty());
}

TEST(StringUtils, convertMsgToString_LargeBuffer_Succeeds)
{
    // Arrange
    std::vector<uint8_t> buffer(256, 0xDE);
    uint8_t tag = 0xFF;
    eid_t eid = 255;

    // Act
    std::string result = utils::convertMsgToString(utils::Rx, buffer, tag, eid);

    // Assert
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("Rx"), std::string::npos);
}

// ============================================================================
// Bitfield256 Additional Edge Cases
// ============================================================================

TEST(Bitfield256, setBit_Bit0_SetsCorrectly)
{
    // Arrange
    utils::Bitfield256 bf;

    // Act
    bool wasNew = bf.setBit(0);

    // Assert
    EXPECT_TRUE(wasNew);
    EXPECT_EQ(bf.fields[0].byte, 1u);
}

TEST(Bitfield256, setBit_Bit255_SetsCorrectly)
{
    // Arrange
    utils::Bitfield256 bf;

    // Act
    bool wasNew = bf.setBit(255);

    // Assert
    EXPECT_TRUE(wasNew);
    // Bit 255 = field 7 (255/32=7), bit 31 (255%32=31)
    EXPECT_EQ(bf.fields[7].byte, (1u << 31));
}

TEST(Bitfield256, getSetBits_MultipleBitsAcrossFields_FormatsCorrectly)
{
    // Arrange
    utils::Bitfield256 bf;
    bf.setBit(0);
    bf.setBit(32);  // First bit of field 1
    bf.setBit(64);  // First bit of field 2
    bf.setBit(128); // First bit of field 4

    // Act
    std::string result = bf.getSetBits();

    // Assert
    EXPECT_NE(result.find("0"), std::string::npos);
    EXPECT_NE(result.find("32"), std::string::npos);
    EXPECT_NE(result.find("64"), std::string::npos);
    EXPECT_NE(result.find("128"), std::string::npos);
}
