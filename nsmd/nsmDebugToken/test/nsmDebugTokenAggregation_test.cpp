/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmDebugTokenAggregation.hpp"
#include "nsmDebugTokenUnified.hpp"

#include <debug-token.h>
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

TEST_F(NsmDebugTokenAggregationTest, ParseTlvTokensTruncatedStructureBody)
{
    NsmDebugTokenAggregationObject obj(bus, testPath);

    DebugTokenHeader header{};
    memset(&header, 0, sizeof(header));
    header.numberOfRecords = 1;
    header.offsetToListOfStructs = sizeof(DebugTokenHeader);

    auto headerBytes = makeValidHeaderBytes(1);
    std::vector<uint8_t> fileData = headerBytes;

    // Build a StructureHeader whose size field indicates 100 bytes of payload,
    // but append NO payload bytes → offset+totalStructSize > tokenData.size()
    // triggers the line 208 error path.
    debug_token::StructureHeader tlvHdr{};
    memset(&tlvHdr, 0, sizeof(tlvHdr));
    // Valid TLV1 identifier
    tlvHdr.identifier[0] = 0x54;
    tlvHdr.identifier[1] = 0x4C;
    tlvHdr.identifier[2] = 0x56;
    tlvHdr.identifier[3] = 0x31;
    tlvHdr.versionMajor = 1;
    tlvHdr.versionMinor = 0;
    tlvHdr.size = 100; // claims 100 bytes of payload
    fileData.insert(fileData.end(), reinterpret_cast<uint8_t*>(&tlvHdr),
                    reinterpret_cast<uint8_t*>(&tlvHdr) + sizeof(tlvHdr));
    // No payload bytes appended → total struct size (32+100=132) > tokenData
    // (32)

    memcpy(&header, fileData.data(), sizeof(header));

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

// ============================================================================
// installTokensToDevices() branch coverage
// Covers lines 265-267 (null guard), 271-276 (empty serial), 293-299
// (unmatched serial) in nsmDebugTokenAggregation.cpp
// ============================================================================

struct NsmDebugTokenInstallTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    NsmDebugTokenInstallTest() : SensorManagerTest(devices) {}

    ~NsmDebugTokenInstallTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Branch A (L265-267): nullptr entry in debugTokenList → skipped by null guard
TEST_F(NsmDebugTokenInstallTest, InstallTokensToDevices_NullTokenInList_Skipped)
{
    NsmDebugTokenAggregationObject obj(
        bus, "/xyz/openbmc_project/debug_token_install_ta");
    mockManager.debugTokenList.push_back(nullptr);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    tokens.emplace("SERIAL_A", std::vector<uint8_t>{0x01, 0x02, 0x03});

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus, "/com/nvidia/nsmd/aop/ta");
    auto valueIntf =
        std::make_shared<AsyncValueIntf>(bus, "/com/nvidia/nsmd/aop/ta");

    auto coro = obj.installTokensToDevices(std::move(tokens), statusIntf,
                                           valueIntf);
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

// Branch B (L271-276): empty tokenDeviceID() → skipped
// A freshly constructed NsmDebugTokenUnifiedObject has empty tokenDeviceID()
TEST_F(NsmDebugTokenInstallTest,
       InstallTokensToDevices_EmptySerialToken_Skipped)
{
    NsmDebugTokenAggregationObject obj(
        bus, "/xyz/openbmc_project/debug_token_install_tb");

    auto tokenObj = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "ut_dev_tb", "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0", "GPU");
    // tokenDeviceID() is "" by default → branch B is taken
    mockManager.debugTokenList.push_back(tokenObj);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    tokens.emplace("SERIAL_B", std::vector<uint8_t>{0x04, 0x05, 0x06});

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus, "/com/nvidia/nsmd/aop/tb");
    auto valueIntf =
        std::make_shared<AsyncValueIntf>(bus, "/com/nvidia/nsmd/aop/tb");

    auto coro = obj.installTokensToDevices(std::move(tokens), statusIntf,
                                           valueIntf);
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

// Branch C (L293-299): token serial not in deviceMap → unmatched warning
TEST_F(NsmDebugTokenInstallTest,
       InstallTokensToDevices_UnmatchedSerial_TokenSkipped)
{
    NsmDebugTokenAggregationObject obj(
        bus, "/xyz/openbmc_project/debug_token_install_tc");

    auto tokenObj = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "ut_dev_tc", "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0", "GPU");
    tokenObj->tokenDeviceID("DEVSERIAL_C");
    mockManager.debugTokenList.push_back(tokenObj);

    // Token uses a different serial → deviceMap.find() returns end()
    NsmDebugTokenAggregationObject::TokenMap tokens;
    tokens.emplace("OTHERSERIAL_C", std::vector<uint8_t>{0x07, 0x08, 0x09});

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus, "/com/nvidia/nsmd/aop/tc");
    auto valueIntf =
        std::make_shared<AsyncValueIntf>(bus, "/com/nvidia/nsmd/aop/tc");

    auto coro = obj.installTokensToDevices(std::move(tokens), statusIntf,
                                           valueIntf);
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
}

// ============================================================================
// Branch coverage: matched serial path in installTokensToDevices()
// Covers lines 302-352 (matched device found, installTokenToDevice called,
// errorCode==0 success branch and failureCount>0 InternalFailure branch)
// ============================================================================

struct NsmDebugTokenInstallWithDeviceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
    const uuid_t tokenUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDebugTokenInstallWithDeviceTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(tokenUuid));
    }

    ~NsmDebugTokenInstallWithDeviceTest()
    {
        cleanupDeviceSensors(devices);
    }

    static Response createInstallTokenResponse(uint8_t cc = NSM_SUCCESS,
                                               uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        encode_nsm_install_token_resp(0, cc, reasonCode, msg);
        return response;
    }
};

// Matched serial, installTokenDirect succeeds → successCount++ → Success
TEST_F(NsmDebugTokenInstallWithDeviceTest,
       InstallTokensToDevices_MatchedSerial_Success)
{
    NsmDebugTokenAggregationObject obj(
        bus, "/xyz/openbmc_project/debug_token_install_td");

    auto tokenObj = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "ut_dev_td", tokenUuid, "GPU");
    tokenObj->tokenDeviceID("DEVSERIAL_TD");
    tokenObj->installationChunkSize = 4096;
    mockManager.debugTokenList.push_back(tokenObj);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    tokens.emplace("DEVSERIAL_TD", std::vector<uint8_t>{0xAA, 0xBB, 0xCC});

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus, "/com/nvidia/nsmd/aop/td");
    auto valueIntf =
        std::make_shared<AsyncValueIntf>(bus, "/com/nvidia/nsmd/aop/td");

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    auto coro = obj.installTokensToDevices(std::move(tokens), statusIntf,
                                           valueIntf);
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// Matched serial, installTokenDirect fails → failureCount++ → InternalFailure
TEST_F(NsmDebugTokenInstallWithDeviceTest,
       InstallTokensToDevices_MatchedSerial_Failure)
{
    NsmDebugTokenAggregationObject obj(
        bus, "/xyz/openbmc_project/debug_token_install_te");

    auto tokenObj = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "ut_dev_te", tokenUuid, "GPU");
    tokenObj->tokenDeviceID("DEVSERIAL_TE");
    tokenObj->installationChunkSize = 4096;
    mockManager.debugTokenList.push_back(tokenObj);

    NsmDebugTokenAggregationObject::TokenMap tokens;
    tokens.emplace("DEVSERIAL_TE", std::vector<uint8_t>{0xDD, 0xEE});

    auto statusIntf =
        std::make_shared<AsyncStatusIntf>(bus, "/com/nvidia/nsmd/aop/te");
    auto valueIntf =
        std::make_shared<AsyncValueIntf>(bus, "/com/nvidia/nsmd/aop/te");

    // postPatchIO returns error → installTokenDirect sets errorCode != 0
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));

    auto coro = obj.installTokensToDevices(std::move(tokens), statusIntf,
                                           valueIntf);
    EXPECT_EQ(coro.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// installToken() success path (lines 87-90): parse OK + tokens non-empty →
// installTokensToDevices().detach() called → returns valid object path.
// Uses NsmDebugTokenInstallWithDeviceTest (has SensorManager initialized).
TEST_F(NsmDebugTokenInstallWithDeviceTest, InstallToken_ValidFile_ReturnsPath)
{
    NsmDebugTokenAggregationObject obj(
        bus, "/xyz/openbmc_project/debug_token_install_vf");

    // Build a valid single-record token file
    std::vector<uint8_t> serialBytes = {0x0A, 0x0B, 0x0C, 0x0D};
    auto tlvData = makeTlvWithSerial(serialBytes);
    uint32_t totalSize =
        static_cast<uint32_t>(sizeof(DebugTokenHeader) + tlvData.size());
    auto headerBytes = makeValidHeaderBytes(1, sizeof(DebugTokenHeader),
                                            totalSize);
    std::vector<uint8_t> fileData = headerBytes;
    fileData.insert(fileData.end(), tlvData.begin(), tlvData.end());

    int fd = createMemFd(fileData);
    ASSERT_GE(fd, 0);

    // No matching device serial → installTokensToDevices does nothing but
    // must not crash.  SensorManager is initialized by SensorManagerTest.
    sdbusplus::message::object_path result;
    EXPECT_NO_THROW(
        { result = obj.installToken(sdbusplus::message::unix_fd(fd)); });
    EXPECT_FALSE(result.str.empty());
    close(fd);
}

// ============================================================================
// installToken() public method branch coverage
// Lines 80-84: parse failure → throws InvalidArgument
// Lines 87-90: success path → returns valid object path
// ============================================================================

TEST_F(NsmDebugTokenAggregationTest, InstallToken_ParseFail_ThrowsInvalidArg)
{
    NsmDebugTokenAggregationObject obj(bus, testPath + "_it1");

    // File with wrong type field → parseHeader returns -1 → parseRc != 0
    auto headerBytes = makeValidHeaderBytes();
    headerBytes[1] = 0xFF; // corrupt type: FileTypeDebugToken != 0xFF
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    EXPECT_THROW(
        obj.installToken(sdbusplus::message::unix_fd(fd)),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
    close(fd);
}

TEST_F(NsmDebugTokenAggregationTest, InstallToken_EmptyTokens_ThrowsInvalidArg)
{
    NsmDebugTokenAggregationObject obj(bus, testPath + "_it2");

    // Valid header with 0 records → tokens remain empty → parseRc=-1
    auto headerBytes = makeValidHeaderBytes(0);
    int fd = createMemFd(headerBytes);
    ASSERT_GE(fd, 0);

    EXPECT_THROW(
        obj.installToken(sdbusplus::message::unix_fd(fd)),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
    close(fd);
}

// Success path (lines 87-90) requires SensorManager — use
// NsmDebugTokenInstallWithDeviceTest which extends SensorManagerTest.
// Test defined below in that fixture section.
