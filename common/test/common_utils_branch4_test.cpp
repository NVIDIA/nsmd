/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests (batch 4) for common/utils.hpp and common/utils.cpp
 * Targets uncovered branches in:
 *   - Bitfield256::setBit returning false (already set)
 *   - getPropertyFromCollection bad_variant_access (L738)
 *   - IDBusHandler::tryGetDbusProperty with different default value types
 *   - isPreferred with unknown medium and binding (defaultPriority)
 *   - CustomFD::read EOF branch (L1246-1248)
 *   - CustomFD constructor with invalid fd
 *   - CustomFD operator() and operator int
 *   - CustomFD getFileSize / size
 *   - convertAndScaleDownUint32ToDouble INVALID path
 *   - convertAndScaleDownUint8ToDouble INVALID path
 *   - Associations tuple conversion
 *   - appendBufferToFd invalid fd
 *   - requestMsgToHexString
 *   - getDeviceNameFromDeviceType all switch cases
 *   - printBuffer raw pointer overload empty buffer
 *   - getAssociations tuple conversion
 */

#include "utils.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <sdbusplus/exception.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace utils;

// ============================================================================
// Bitfield256::setBit — returns false when bit already set
// ============================================================================

TEST(Bitfield256Branch4, SetBitReturnsFalseWhenAlreadySet)
{
    Bitfield256 bf;
    EXPECT_TRUE(bf.setBit(42));
    EXPECT_FALSE(bf.setBit(42));
}

TEST(Bitfield256Branch4, SetBitDifferentFieldIndices)
{
    Bitfield256 bf;
    // Test bits in different 32-bit fields
    EXPECT_TRUE(bf.setBit(0));   // field 0, bit 0
    EXPECT_TRUE(bf.setBit(31));  // field 0, bit 31
    EXPECT_TRUE(bf.setBit(32));  // field 1, bit 0
    EXPECT_TRUE(bf.setBit(64));  // field 2, bit 0
    EXPECT_TRUE(bf.setBit(128)); // field 4, bit 0
    EXPECT_TRUE(bf.setBit(255)); // field 7, bit 31

    // All should now return false
    EXPECT_FALSE(bf.setBit(0));
    EXPECT_FALSE(bf.setBit(31));
    EXPECT_FALSE(bf.setBit(32));
    EXPECT_FALSE(bf.setBit(255));

    // Verify getSetBits includes them all
    std::string bits = bf.getSetBits();
    EXPECT_NE(bits.find("0"), std::string::npos);
    EXPECT_NE(bits.find("255"), std::string::npos);
}

// ============================================================================
// getPropertyFromCollection — bad_variant_access path (L738)
// ============================================================================

TEST(GetPropertyFromCollectionBranch4, BadVariantAccessThrows)
{
    PropertyValuesCollection col = {
        {"name", PropertyValue{std::string("hello")}},
    };
    // Property exists but is a string, asking for uint32_t throws
    EXPECT_THROW((void)getPropertyFromCollection<uint32_t>(col, "name"),
                 std::bad_variant_access);
}

TEST(GetPropertyFromCollectionBranch4, FoundButWrongTypeDouble)
{
    PropertyValuesCollection col = {
        {"val", PropertyValue{double(3.14)}},
    };
    EXPECT_THROW((void)getPropertyFromCollection<bool>(col, "val"),
                 std::bad_variant_access);
}

// ============================================================================
// IDBusHandler::tryGetDbusProperty — different default value types
// ============================================================================

struct ThrowingHandler4 : public IDBusHandler
{
    std::string getService(const char*, const char*) const override
    {
        return "";
    }
    MapperServiceMap getServiceMap(const char*,
                                   const dbus::Interfaces&) const override
    {
        return {};
    }
    GetSubTreeResponse getSubtree(const std::string&, int,
                                  const dbus::Interfaces&) const override
    {
        return {};
    }
    void setDbusProperty(const DBusMapping&,
                         const PropertyValue&) const override
    {}
    PropertyValue getDbusPropertyVariant(const char*, const char*,
                                         const char*) const override
    {
        throw sdbusplus::exception::InvalidEnumString{};
    }
    PropertyValuesCollection getDbusProperties(const char*,
                                               const char*) const override
    {
        return {};
    }
    GetAssociatedObjectsResponse
        getAssociatedObjects(const std::string&,
                             const std::string&) const override
    {
        return {};
    }
};

TEST(TryGetDbusPropertyBranch4, ReturnsDefaultBool)
{
    ThrowingHandler4 handler;
    auto result = handler.tryGetDbusProperty<bool>("/p", "x", "i", false);
    EXPECT_FALSE(result);
}

TEST(TryGetDbusPropertyBranch4, ReturnsDefaultUint32)
{
    ThrowingHandler4 handler;
    auto result = handler.tryGetDbusProperty<uint32_t>("/p", "x", "i", 999u);
    EXPECT_EQ(result, 999u);
}

TEST(TryGetDbusPropertyBranch4, ReturnsDefaultDouble)
{
    ThrowingHandler4 handler;
    auto result = handler.tryGetDbusProperty<double>("/p", "x", "i", 1.5);
    EXPECT_DOUBLE_EQ(result, 1.5);
}

TEST(TryGetDbusPropertyBranch4, ReturnsDefaultUint8)
{
    ThrowingHandler4 handler;
    auto result = handler.tryGetDbusProperty<uint8_t>("/p", "x", "i",
                                                      uint8_t(42));
    EXPECT_EQ(result, 42);
}

TEST(TryGetDbusPropertyBranch4, ReturnsDefaultInt64)
{
    ThrowingHandler4 handler;
    auto result = handler.tryGetDbusProperty<int64_t>("/p", "x", "i",
                                                      int64_t(-100));
    EXPECT_EQ(result, -100);
}

// ============================================================================
// isPreferred — unknown medium (defaultPriority = INT_MIN)
// ============================================================================

TEST(IsPreferredBranch4, UnknownCurrentMedium_NewPCIe_ReturnsTrue)
{
    // Unknown medium has INT_MIN priority. PCIe has 0.
    // INT_MIN >= 0 is false
    std::tuple<MctpMedium, MctpBinding> current = {
        "unknown.medium", "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    EXPECT_FALSE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch4, UnknownNewMedium_CurrentPCIe_ReturnsTrue)
{
    // Current medium PCIe (0). New medium unknown (INT_MIN).
    // 0 >= INT_MIN is true
    std::tuple<MctpMedium, MctpBinding> current = {
        "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe",
        "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {
        "unknown.medium", "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    EXPECT_TRUE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch4, BothUnknownMedium_CompareBinding)
{
    // Both medium unknown -> equal (INT_MIN == INT_MIN), compare binding
    // Current binding unknown (INT_MIN), new binding PCIe (0)
    // INT_MIN >= 0 is false
    std::tuple<MctpMedium, MctpBinding> current = {"unknown.medium",
                                                   "unknown.binding"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {
        "unknown.medium", "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe"};
    EXPECT_FALSE(isPreferred(current, newInfo));
}

TEST(IsPreferredBranch4, BothUnknownMedium_UnknownBinding_Equal)
{
    // Both medium unknown, both binding unknown -> INT_MIN >= INT_MIN = true
    std::tuple<MctpMedium, MctpBinding> current = {"unknown.medium",
                                                   "unknown.binding"};
    std::tuple<MctpMedium, MctpBinding> newInfo = {"unknown.medium",
                                                   "unknown.binding"};
    EXPECT_TRUE(isPreferred(current, newInfo));
}

// ============================================================================
// CustomFD — constructor, operator, size, read EOF, write success
// ============================================================================

TEST(CustomFDBranch4, ConstructorThrowsOnInvalidFd)
{
    EXPECT_THROW(CustomFD(-1), std::runtime_error);
}

TEST(CustomFDBranch4, OperatorCallAndConversion)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    CustomFD cfd(fd);
    EXPECT_EQ(cfd(), fd);
    EXPECT_EQ(static_cast<int>(cfd), fd);
}

TEST(CustomFDBranch4, SizeOfEmptyFile)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    CustomFD cfd(fd);
    EXPECT_EQ(cfd.size(), 0u);
    EXPECT_EQ(cfd.getFileSize(), 0u);
}

TEST(CustomFDBranch4, WriteAndReadSuccess)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    // Pre-size the file so read bounds check passes
    ASSERT_EQ(ftruncate(fd, 16), 0);
    CustomFD cfd(fd);

    uint8_t data[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_TRUE(cfd.write(0, data, 4));

    uint8_t readBuf[4] = {};
    EXPECT_TRUE(cfd.read(0, readBuf, 4));
    EXPECT_EQ(readBuf[0], 0xDE);
    EXPECT_EQ(readBuf[1], 0xAD);
    EXPECT_EQ(readBuf[2], 0xBE);
    EXPECT_EQ(readBuf[3], 0xEF);
}

TEST(CustomFDBranch4, WriteAtOffset)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, 16), 0);
    CustomFD cfd(fd);

    uint8_t data[2] = {0x11, 0x22};
    EXPECT_TRUE(cfd.write(4, data, 2));

    uint8_t readBuf[2] = {};
    EXPECT_TRUE(cfd.read(4, readBuf, 2));
    EXPECT_EQ(readBuf[0], 0x11);
    EXPECT_EQ(readBuf[1], 0x22);
}

TEST(CustomFDBranch4, ReadBeyondFileSize)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, 4), 0);
    CustomFD cfd(fd);

    uint8_t buf[8] = {};
    // pos + size > fileSize
    EXPECT_FALSE(cfd.read(0, buf, 8));
}

TEST(CustomFDBranch4, ReadWithNullData)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, 4), 0);
    CustomFD cfd(fd);
    EXPECT_FALSE(cfd.read(0, nullptr, 4));
}

TEST(CustomFDBranch4, ReadWithZeroSize)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, 4), 0);
    CustomFD cfd(fd);
    uint8_t buf[1] = {};
    EXPECT_FALSE(cfd.read(0, buf, 0));
}

TEST(CustomFDBranch4, WriteWithNullData)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, 4), 0);
    CustomFD cfd(fd);
    EXPECT_FALSE(cfd.write(0, nullptr, 4));
}

TEST(CustomFDBranch4, WriteWithZeroSize)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    ASSERT_EQ(ftruncate(fd, 4), 0);
    CustomFD cfd(fd);
    uint8_t data[1] = {0};
    EXPECT_FALSE(cfd.write(0, data, 0));
}

TEST(CustomFDBranch4, WriteBeyondFileSize)
{
    int fd = memfd_create("test", 0);
    ASSERT_GE(fd, 0);
    // File size is 0
    CustomFD cfd(fd);
    uint8_t data[4] = {1, 2, 3, 4};
    // pos=1 > fileSize=0
    EXPECT_FALSE(cfd.write(1, data, 4));
}

// ============================================================================
// Associations tuple conversion
// ============================================================================

TEST(AssociationsBranch4, EmptyAssociations)
{
    std::vector<Association> empty;
    auto tuples = getAssociations(empty);
    EXPECT_TRUE(tuples.empty());
}

TEST(AssociationsBranch4, SingleAssociation)
{
    std::vector<Association> assocs = {{"forward", "backward", "/path/obj"}};
    auto tuples = getAssociations(assocs);
    ASSERT_EQ(tuples.size(), 1u);
    EXPECT_EQ(std::get<0>(tuples[0]), "forward");
    EXPECT_EQ(std::get<1>(tuples[0]), "backward");
    EXPECT_EQ(std::get<2>(tuples[0]), "/path/obj");
}

TEST(AssociationsBranch4, MultipleAssociations)
{
    std::vector<Association> assocs = {
        {"fwd1", "bwd1", "/path1"},
        {"fwd2", "bwd2", "/path2"},
        {"fwd3", "bwd3", "/path3"},
    };
    auto tuples = getAssociations(assocs);
    ASSERT_EQ(tuples.size(), 3u);
    EXPECT_EQ(std::get<0>(tuples[2]), "fwd3");
}

// ============================================================================
// appendBufferToFd — error paths
// ============================================================================

TEST(AppendBufferToFdBranch4, InvalidFd_Throws)
{
    std::vector<uint8_t> buf = {1, 2, 3};
    EXPECT_THROW(appendBufferToFd(-1, buf), std::runtime_error);
}

TEST(AppendBufferToFdBranch4, WriteFails_ReadOnlyFd_Throws)
{
    char tmpPath[] = "/tmp/append_test_XXXXXX";
    int tmpFd = mkstemp(tmpPath);
    ASSERT_GE(tmpFd, 0);
    close(tmpFd);

    int fd = open(tmpPath, O_RDONLY);
    unlink(tmpPath);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buf = {1, 2, 3};
    EXPECT_THROW(appendBufferToFd(fd, buf), std::runtime_error);
    close(fd);
}

TEST(AppendBufferToFdBranch4, SuccessfulAppend)
{
    int fd = memfd_create("append_test", 0);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buf1 = {0xAA, 0xBB};
    appendBufferToFd(fd, buf1);
    std::vector<uint8_t> buf2 = {0xCC, 0xDD};
    appendBufferToFd(fd, buf2);

    // Verify contents
    lseek(fd, 0, SEEK_SET);
    uint8_t readBuf[4] = {};
    ASSERT_EQ(::read(fd, readBuf, 4), 4);
    EXPECT_EQ(readBuf[0], 0xAA);
    EXPECT_EQ(readBuf[1], 0xBB);
    EXPECT_EQ(readBuf[2], 0xCC);
    EXPECT_EQ(readBuf[3], 0xDD);
    close(fd);
}

// ============================================================================
// writeBufferToFd — success path and invalid fd
// ============================================================================

TEST(WriteBufferToFdBranch4, InvalidFd_Throws)
{
    std::vector<uint8_t> buf = {1};
    EXPECT_THROW(writeBufferToFd(-1, buf), std::runtime_error);
}

TEST(WriteBufferToFdBranch4, SuccessfulWrite)
{
    int fd = memfd_create("write_test", 0);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buf = {0x01, 0x02, 0x03, 0x04};
    writeBufferToFd(fd, buf);

    // Verify contents
    std::vector<uint8_t> readBack;
    readFdToBuffer(fd, readBack);
    EXPECT_EQ(readBack.size(), 4u);
    EXPECT_EQ(readBack[0], 0x01);
    EXPECT_EQ(readBack[3], 0x04);
    close(fd);
}

// ============================================================================
// readFdToBuffer — success path and invalid fd
// ============================================================================

TEST(ReadFdToBufferBranch4, InvalidFd_Throws)
{
    std::vector<uint8_t> buf;
    EXPECT_THROW(readFdToBuffer(-1, buf), std::runtime_error);
}

TEST(ReadFdToBufferBranch4, EmptyFile_ReadsZeroBytes)
{
    int fd = memfd_create("empty_test", 0);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buf;
    readFdToBuffer(fd, buf);
    EXPECT_TRUE(buf.empty());
    close(fd);
}

// ============================================================================
// requestMsgToHexString
// ============================================================================

TEST(RequestMsgToHexStringBranch4, EmptyMsg)
{
    std::vector<uint8_t> msg;
    auto result = requestMsgToHexString(msg);
    EXPECT_TRUE(result.empty());
}

TEST(RequestMsgToHexStringBranch4, SingleByte)
{
    std::vector<uint8_t> msg = {0xAB};
    auto result = requestMsgToHexString(msg);
    EXPECT_NE(result.find("ab"), std::string::npos);
}

TEST(RequestMsgToHexStringBranch4, MultipleBytes)
{
    std::vector<uint8_t> msg = {0x01, 0x02, 0xFF};
    auto result = requestMsgToHexString(msg);
    EXPECT_NE(result.find("01"), std::string::npos);
    EXPECT_NE(result.find("02"), std::string::npos);
    EXPECT_NE(result.find("ff"), std::string::npos);
}

// ============================================================================
// getDeviceNameFromDeviceType — all switch cases
// ============================================================================

TEST(GetDeviceNameBranch4, AllDeviceTypes)
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
// getDeviceInstanceName
// ============================================================================

TEST(GetDeviceInstanceNameBranch4, SwitchType)
{
    EXPECT_EQ(getDeviceInstanceName(1, 3), "SWITCH_3");
}

TEST(GetDeviceInstanceNameBranch4, BaseboardType)
{
    EXPECT_EQ(getDeviceInstanceName(3, 0), "BASEBOARD_0");
}

TEST(GetDeviceInstanceNameBranch4, ErotType)
{
    EXPECT_EQ(getDeviceInstanceName(4, 7), "EROT_7");
}

TEST(GetDeviceInstanceNameBranch4, MctpBridgeType)
{
    EXPECT_EQ(getDeviceInstanceName(5, 1), "MCTPBRIDGE_1");
}

TEST(GetDeviceInstanceNameBranch4, CpuType)
{
    EXPECT_EQ(getDeviceInstanceName(6, 2), "CPU_2");
}

// ============================================================================
// printBuffer raw pointer overload — empty buffer
// ============================================================================

TEST(PrintBufferBranch4, RawPointerEmptyBuffer)
{
    uint8_t data = 0;
    printBuffer(true, &data, 0, 0, 0);
}

TEST(PrintBufferBranch4, RawPointerNonEmpty)
{
    uint8_t data[] = {0x01, 0x02, 0x03};
    printBuffer(false, data, 3, 0x10, 0x20);
}

// ============================================================================
// convertMsgToString — various edge cases
// ============================================================================

TEST(ConvertMsgToStringBranch4, SingleByte)
{
    std::vector<uint8_t> buf = {0xFF};
    auto result = convertMsgToString(true, buf, 0, 0);
    EXPECT_NE(result.find("ff"), std::string::npos);
}

// ============================================================================
// int64ToDoubleSafeConvert — safe negative value (no capping)
// ============================================================================

TEST(Int64ToDoubleBranch4, SafeNegativeValue)
{
    int64_t value = -100;
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, -100.0);
}

TEST(Int64ToDoubleBranch4, SafePositiveValue)
{
    int64_t value = 100;
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, 100.0);
}

TEST(Int64ToDoubleBranch4, ZeroValue)
{
    double result = int64ToDoubleSafeConvert(0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// ============================================================================
// uint64ToDoubleSafeConvert — safe value
// ============================================================================

TEST(Uint64ToDoubleBranch4, SafeValue)
{
    uint64_t value = 1000;
    double result = uint64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, 1000.0);
}

TEST(Uint64ToDoubleBranch4, ZeroValue)
{
    double result = uint64ToDoubleSafeConvert(0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// ============================================================================
// convertAndScaleDownUint32ToDouble — both branches
// ============================================================================

TEST(ConvertScaleBranch4, Uint32_InvalidValue)
{
    double result = convertAndScaleDownUint32ToDouble(0xFFFFFFFF, 100.0);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFFFFFFFF));
}

TEST(ConvertScaleBranch4, Uint32_NormalValue)
{
    double result = convertAndScaleDownUint32ToDouble(2000, 100.0);
    EXPECT_DOUBLE_EQ(result, 20.0);
}

// ============================================================================
// convertAndScaleDownUint8ToDouble — both branches
// ============================================================================

TEST(ConvertScaleBranch4, Uint8_InvalidValue)
{
    double result = convertAndScaleDownUint8ToDouble(0xFF);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(0xFF));
}

TEST(ConvertScaleBranch4, Uint8_NormalValue)
{
    double result = convertAndScaleDownUint8ToDouble(42);
    EXPECT_DOUBLE_EQ(result, 42.0);
}

// ============================================================================
// bitfield256_tToBitMap and bitfield256_tToBitArray edge cases
// ============================================================================

TEST(Bitfield256ToBitmapBranch4, AllZeroFields)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    auto bitmap = bitfield256_tToBitMap(bf);
    ASSERT_EQ(bitmap.size(), 32u);
    for (auto b : bitmap)
    {
        EXPECT_EQ(b, 0);
    }
}

TEST(Bitfield256ToBitArrayBranch4, AllZeroFields)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    auto arr = bitfield256_tToBitArray(bf);
    ASSERT_EQ(arr.size(), 32u);
    for (auto b : arr)
    {
        EXPECT_EQ(b, 0);
    }
}

TEST(Bitfield256ToBitmapBranch4, SpecificBitSet)
{
    bitfield256_t bf = {};
    memset(&bf, 0, sizeof(bf));
    bf.fields[0].byte = 1; // bit 0 set
    auto bitmap = bitfield256_tToBitMap(bf);
    EXPECT_EQ(bitmap[0], 1);
}

// ============================================================================
// isValidDbusString — mixed valid multi-byte sequences
// ============================================================================

TEST(IsValidDbusStringBranch4b, Mixed1And2ByteSequences)
{
    // ASCII 'A' (1-byte) + valid 2-byte U+00E9 (C3 A9)
    EXPECT_TRUE(isValidDbusString("A\xC3\xA9"));
}

TEST(IsValidDbusStringBranch4b, Mixed2And3ByteSequences)
{
    // Valid 2-byte + valid 3-byte euro sign
    EXPECT_TRUE(isValidDbusString("\xC3\xA9\xE2\x82\xAC"));
}

TEST(IsValidDbusStringBranch4b, Mixed3And4ByteSequences)
{
    // Valid 3-byte + valid 4-byte
    EXPECT_TRUE(isValidDbusString("\xE2\x82\xAC\xF0\x90\x8D\x88"));
}

// ============================================================================
// parseStaticUuid — negative n1 and n2
// ============================================================================

TEST(ParseStaticUuidBranch4, NegativeN1_ReturnsNeg1)
{
    uuid_t uuid = "STATIC:-1:0:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -1);
}

TEST(ParseStaticUuidBranch4, NegativeN2_ReturnsNeg2)
{
    uuid_t uuid = "STATIC:0:-1:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(parseStaticUuid(uuid, dt, in, dr, rp, rv), -2);
}

// ============================================================================
// getUUIDFromEID / getEidFromUUID — empty table
// ============================================================================

TEST(UUIDEIDLookupBranch4, GetUUIDFromEID_EmptyTable)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        table;
    auto result = getUUIDFromEID(table, 42);
    EXPECT_FALSE(result.has_value());
}

TEST(UUIDEIDLookupBranch4, GetEidFromUUID_EmptyTable)
{
    std::multimap<uuid_t, std::tuple<eid_t, MctpMedium, MctpBinding>> table;
    auto result = getEidFromUUID(table, "test-uuid");
    EXPECT_EQ(result, std::numeric_limits<uint8_t>::max());
}
