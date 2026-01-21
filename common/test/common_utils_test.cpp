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
