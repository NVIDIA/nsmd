/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmDebugTokenAggregation.hpp"

#include <debug-token/tlv.h>
#include <debug-token/types.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <vector>

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Test Helpers
// ============================================================================

static int createMemFd(const std::vector<uint8_t>& data)
{
    int fd = memfd_create("test_token", 0);
    if (fd < 0)
        return -1;
    if (!data.empty())
    {
        ssize_t written = write(fd, data.data(), data.size());
        if (written != static_cast<ssize_t>(data.size()))
        {
            close(fd);
            return -1;
        }
    }
    lseek(fd, 0, SEEK_SET);
    return fd;
}

static std::vector<uint8_t>
    makeValidHeaderBytes(uint16_t numRecords = 0,
                         uint16_t offsetToStructs = sizeof(DebugTokenHeader),
                         uint32_t fileSize = sizeof(DebugTokenHeader))
{
    DebugTokenHeader h{};
    memset(&h, 0, sizeof(h));
    h.version = 1;
    h.type = FileTypeDebugToken;
    h.numberOfRecords = numRecords;
    h.offsetToListOfStructs = offsetToStructs;
    h.fileSize = fileSize;
    return std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&h),
                                reinterpret_cast<uint8_t*>(&h) + sizeof(h));
}

static std::vector<uint8_t> makeTlvWithSerial(
    const std::vector<uint8_t>& serialBytes = {0x01, 0x02, 0x03, 0x04})
{
    debug_token::tlv_encoder::Structure enc;
    enc.setVersion(1, 0);
    enc.add(debug_token::types::Common::DeviceSerialNumber, serialBytes);
    return enc.encode();
}

// ============================================================================
// Test Fixture
// ============================================================================

struct NsmDebugTokenAggregationTest : public Test, public utils::DBusTest
{
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const std::string testPath = "/xyz/openbmc_project/debug_token_agg_test";
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmDebugTokenAggregationTest, ConstructorBasicPath)
{
    EXPECT_NO_THROW({ NsmDebugTokenAggregationObject obj(bus, testPath); });
}

// ============================================================================
// parseHeader Tests
// ============================================================================

TEST_F(NsmDebugTokenAggregationTest, ParseHeaderValidHeader)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    auto headerBytes = makeValidHeaderBytes();
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    DebugTokenHeader outHeader{};
    int rc = obj.parseHeader(fd, outHeader);

    EXPECT_EQ(rc, 0);
    EXPECT_EQ(outHeader.type, FileTypeDebugToken);
    EXPECT_EQ(outHeader.version, 1);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseHeaderInvalidFileType)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    auto headerBytes = makeValidHeaderBytes();
    headerBytes[1] = 99; // Corrupt the type field
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    DebugTokenHeader outHeader{};
    int rc = obj.parseHeader(fd, outHeader);

    EXPECT_EQ(rc, -1);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseHeaderEmptyFile)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> emptyData;
    int fd = createMemFd(emptyData);
    ASSERT_GE(fd, 0);

    DebugTokenHeader outHeader{};
    int rc = obj.parseHeader(fd, outHeader);

    EXPECT_EQ(rc, -1);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseHeaderTruncatedData)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    // Only 4 bytes — less than sizeof(DebugTokenHeader)
    std::vector<uint8_t> truncated = {0x01, FileTypeDebugToken, 0x00, 0x00};
    int fd = createMemFd(truncated);
    ASSERT_GE(fd, 0);

    DebugTokenHeader outHeader{};
    int rc = obj.parseHeader(fd, outHeader);

    EXPECT_EQ(rc, -1);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseHeaderCorrectFieldValues)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    auto headerBytes = makeValidHeaderBytes(5, sizeof(DebugTokenHeader), 256);
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    DebugTokenHeader outHeader{};
    int rc = obj.parseHeader(fd, outHeader);

    EXPECT_EQ(rc, 0);
    EXPECT_EQ(outHeader.numberOfRecords, 5);
    EXPECT_EQ(outHeader.fileSize, 256u);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseHeaderZeroTypeIsInvalid)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    auto headerBytes = makeValidHeaderBytes();
    headerBytes[1] = 0; // type = 0, not FileTypeDebugToken (=2)
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    DebugTokenHeader outHeader{};
    int rc = obj.parseHeader(fd, outHeader);

    EXPECT_EQ(rc, -1);
    close(fd);
}

// ============================================================================
// extractSerialNumber Tests
// ============================================================================

TEST_F(NsmDebugTokenAggregationTest, ExtractSerialNumberEmptyData)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> emptyData;
    std::string serial = obj.extractSerialNumber(emptyData);

    EXPECT_TRUE(serial.empty());
}

TEST_F(NsmDebugTokenAggregationTest, ExtractSerialNumberInvalidData)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> garbageData = {0xFF, 0xFF, 0xFF, 0xFF, 0x00,
                                        0x00, 0x00, 0x01, 0xAB, 0xCD};
    std::string serial = obj.extractSerialNumber(garbageData);

    // Exception caught, returns empty
    EXPECT_TRUE(serial.empty());
}

TEST_F(NsmDebugTokenAggregationTest, ExtractSerialNumberValidTlv)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> serialBytes = {0xAB, 0xCD, 0xEF, 0x12};
    auto tlvData = makeTlvWithSerial(serialBytes);

    std::string serial = obj.extractSerialNumber(tlvData);

    EXPECT_FALSE(serial.empty());
    EXPECT_EQ(serial.substr(0, 2), "0x");
}

TEST_F(NsmDebugTokenAggregationTest, ExtractSerialNumberTlvWithoutSerial)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    // TLV with DeviceType but no DeviceSerialNumber
    debug_token::tlv_encoder::Structure enc;
    enc.setVersion(1, 0);
    enc.add(debug_token::types::Common::DeviceType, static_cast<uint8_t>(0x01));
    auto tlvData = enc.encode();

    std::string serial = obj.extractSerialNumber(tlvData);

    // get() throws when type not found → catch returns ""
    EXPECT_TRUE(serial.empty());
}

TEST_F(NsmDebugTokenAggregationTest, ExtractSerialNumberSingleByte)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> serialBytes = {0xFF};
    auto tlvData = makeTlvWithSerial(serialBytes);

    std::string serial = obj.extractSerialNumber(tlvData);

    EXPECT_FALSE(serial.empty());
    EXPECT_EQ(serial, "0xFF");
}

TEST_F(NsmDebugTokenAggregationTest, ExtractSerialNumberMultipleBytes)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> serialBytes = {0x00, 0x11, 0x22, 0x33,
                                        0x44, 0x55, 0x66, 0x77};
    auto tlvData = makeTlvWithSerial(serialBytes);

    std::string serial = obj.extractSerialNumber(tlvData);

    EXPECT_FALSE(serial.empty());
    EXPECT_EQ(serial.substr(0, 2), "0x");
}

// ============================================================================
// parseTlvTokens Tests
// ============================================================================

TEST_F(NsmDebugTokenAggregationTest, ParseTlvTokensZeroRecords)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    DebugTokenHeader header{};
    memset(&header, 0, sizeof(header));
    header.numberOfRecords = 0;
    header.offsetToListOfStructs = sizeof(DebugTokenHeader);

    auto headerBytes = makeValidHeaderBytes(0);
    std::vector<uint8_t> fileData = headerBytes;

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTlvTokens(fileData, header, tokens);

    // Zero records → tokens empty → returns -1
    EXPECT_EQ(rc, -1);
    EXPECT_TRUE(tokens.empty());
}

TEST_F(NsmDebugTokenAggregationTest, ParseTlvTokensNoDataForRecords)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    DebugTokenHeader header{};
    memset(&header, 0, sizeof(header));
    header.numberOfRecords = 1;
    header.offsetToListOfStructs = sizeof(DebugTokenHeader);

    // File has only the header, no TLV data after it
    auto headerBytes = makeValidHeaderBytes(1);
    std::vector<uint8_t> fileData = headerBytes;

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTlvTokens(fileData, header, tokens);

    // offset=0 >= tokenData.size()=0 → returns -1
    EXPECT_EQ(rc, -1);
}

TEST_F(NsmDebugTokenAggregationTest, ParseTlvTokensInsufficientTlvHeader)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    DebugTokenHeader header{};
    memset(&header, 0, sizeof(header));
    header.numberOfRecords = 1;
    header.offsetToListOfStructs = sizeof(DebugTokenHeader);

    auto headerBytes = makeValidHeaderBytes(1);
    std::vector<uint8_t> fileData = headerBytes;
    // Append only 10 bytes (< sizeof(StructureHeader) = 32)
    for (int i = 0; i < 10; i++)
        fileData.push_back(0x00);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTlvTokens(fileData, header, tokens);

    EXPECT_EQ(rc, -1);
}

TEST_F(NsmDebugTokenAggregationTest, ParseTlvTokensOneValidToken)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> serialBytes = {0x01, 0x02, 0x03, 0x04};
    auto tlvData = makeTlvWithSerial(serialBytes);

    uint32_t totalSize = sizeof(DebugTokenHeader) + tlvData.size();
    auto headerBytes = makeValidHeaderBytes(1, sizeof(DebugTokenHeader),
                                            totalSize);

    std::vector<uint8_t> fileData = headerBytes;
    fileData.insert(fileData.end(), tlvData.begin(), tlvData.end());

    DebugTokenHeader header{};
    memcpy(&header, fileData.data(), sizeof(header));

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTlvTokens(fileData, header, tokens);

    EXPECT_EQ(rc, 0);
    EXPECT_EQ(tokens.size(), 1u);
}

TEST_F(NsmDebugTokenAggregationTest, ParseTlvTokensRecordWithNoSerial)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    // TLV without serial number — record will be skipped
    debug_token::tlv_encoder::Structure enc;
    enc.setVersion(1, 0);
    enc.add(debug_token::types::Common::DeviceType, static_cast<uint8_t>(0x01));
    auto tlvData = enc.encode();

    uint32_t totalSize = sizeof(DebugTokenHeader) + tlvData.size();
    auto headerBytes = makeValidHeaderBytes(1, sizeof(DebugTokenHeader),
                                            totalSize);

    std::vector<uint8_t> fileData = headerBytes;
    fileData.insert(fileData.end(), tlvData.begin(), tlvData.end());

    DebugTokenHeader header{};
    memcpy(&header, fileData.data(), sizeof(header));

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTlvTokens(fileData, header, tokens);

    // Serial not found → token skipped → tokens empty → returns -1
    EXPECT_EQ(rc, -1);
    EXPECT_TRUE(tokens.empty());
}

// ============================================================================
// parseTokenFile Tests
// ============================================================================

TEST_F(NsmDebugTokenAggregationTest, ParseTokenFileInvalidHeader)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    auto headerBytes = makeValidHeaderBytes();
    headerBytes[1] = 99; // Wrong type
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTokenFile(fd, tokens);

    EXPECT_EQ(rc, -1);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseTokenFileEmptyFile)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> emptyData;
    int fd = createMemFd(emptyData);
    ASSERT_GE(fd, 0);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTokenFile(fd, tokens);

    EXPECT_EQ(rc, -1);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseTokenFileValidWithOneToken)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    std::vector<uint8_t> serialBytes = {0x11, 0x22, 0x33, 0x44};
    auto tlvData = makeTlvWithSerial(serialBytes);

    uint32_t totalSize = sizeof(DebugTokenHeader) + tlvData.size();
    auto headerBytes = makeValidHeaderBytes(1, sizeof(DebugTokenHeader),
                                            totalSize);

    std::vector<uint8_t> fileData = headerBytes;
    fileData.insert(fileData.end(), tlvData.begin(), tlvData.end());

    int fd = createMemFd(fileData);
    ASSERT_GE(fd, 0);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTokenFile(fd, tokens);

    EXPECT_EQ(rc, 0);
    EXPECT_EQ(tokens.size(), 1u);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, ParseTokenFileHeaderOnlyNoTokens)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    // Valid header but zero records
    auto headerBytes = makeValidHeaderBytes(0, sizeof(DebugTokenHeader),
                                            sizeof(DebugTokenHeader));
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    int rc = obj.parseTokenFile(fd, tokens);

    // Zero records → tokens empty → returns -1
    EXPECT_EQ(rc, -1);
    close(fd);
}
