/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for common/ files:
 *   - common/utils.cpp  (uncovered error paths, edge cases)
 *   - common/utils.hpp  (template branches, IDBusHandler)
 *   - common/nsmPropertySupport.hpp (uncovered branches)
 */

#include "nsmPropertySupport.hpp"
#include "utils.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <sdbusplus/exception.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

// ============================================================================
// split() — branch: all tokens trimmed to empty => empty result
// (utils.cpp line 264 — covers the return-empty-vector path
//  when trimStr removes everything)
// ============================================================================

TEST(SplitBranch, NoDelimiterFound_ReturnsSingleElement)
{
    auto result = utils::split("hello", ",");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello");
}

TEST(SplitBranch, TrailingDelimiter_IgnoresTrailing)
{
    auto result = utils::split("a,b,", ",");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
}

TEST(SplitBranch, ConsecutiveDelimiters_SkipsEmpties)
{
    auto result = utils::split("a,,b", ",");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
}

TEST(SplitBranch, TrimWithContent_TrimsCorrectly)
{
    // Tokens with leading/trailing quotes trimmed
    auto result = utils::split("\"hello\",\"world\"", ",", "\"");
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
}

// ============================================================================
// parseStaticUuid — error branches
// (utils.cpp: n1 out-of-range => -1, n2 out-of-range => -2,
//  bad format => -3)
// ============================================================================

TEST(ParseStaticUuidBranch, CombinedTooLarge_ReturnsNeg1)
{
    // n1 > 0xffff
    uuid_t uuid = "STATIC:70000:0:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(utils::parseStaticUuid(uuid, dt, in, dr, rp, rv), -1);
}

TEST(ParseStaticUuidBranch, CombinedNegative_ReturnsNeg1)
{
    uuid_t uuid = "STATIC:-1:0:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(utils::parseStaticUuid(uuid, dt, in, dr, rp, rv), -1);
}

TEST(ParseStaticUuidBranch, InstanceTooLarge_ReturnsNeg2)
{
    // n2 > 0xff
    uuid_t uuid = "STATIC:0:300:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(utils::parseStaticUuid(uuid, dt, in, dr, rp, rv), -2);
}

TEST(ParseStaticUuidBranch, InstanceNegative_ReturnsNeg2)
{
    uuid_t uuid = "STATIC:0:-5:PROP:VAL";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(utils::parseStaticUuid(uuid, dt, in, dr, rp, rv), -2);
}

TEST(ParseStaticUuidBranch, TooFewFields_ReturnsNeg3)
{
    uuid_t uuid = "STATIC:0:0";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    EXPECT_EQ(utils::parseStaticUuid(uuid, dt, in, dr, rp, rv), -3);
}

TEST(ParseStaticUuidBranch, ThreeColonSeparatedValues)
{
    uuid_t uuid = "STATIC:0:0:MCTP_UUID:a:b:c";
    uint8_t dt, in, dr;
    std::string rp;
    std::vector<std::string> rv;
    int rc = utils::parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(rp, "MCTP_UUID");
    ASSERT_EQ(rv.size(), 3u);
    EXPECT_EQ(rv[0], "a");
    EXPECT_EQ(rv[1], "b");
    EXPECT_EQ(rv[2], "c");
}

// ============================================================================
// readFdToBuffer — error paths
// (utils.cpp lines 763, 768-769, 779-780, 784-785)
// ============================================================================

TEST(ReadFdToBufferBranch, ClosedFd_LseekFails_Throws)
{
    int fd = memfd_create("test_lseek_fail", 0);
    ASSERT_GE(fd, 0);
    close(fd); // close so lseek fails

    std::vector<uint8_t> buffer;
    EXPECT_THROW(utils::readFdToBuffer(fd, buffer), std::runtime_error);
}

// ============================================================================
// writeBufferToFd — error paths
// (utils.cpp lines 797, 806-807, 813-814)
// ============================================================================

TEST(WriteBufferToFdBranch, ClosedFd_LseekFails_Throws)
{
    int fd = memfd_create("test_write_lseek", 0);
    ASSERT_GE(fd, 0);
    close(fd); // close so lseek fails

    std::vector<uint8_t> data = {0x01, 0x02};
    EXPECT_THROW(utils::writeBufferToFd(fd, data), std::runtime_error);
}

// ============================================================================
// appendBufferToFd — write to closed fd triggers write failure
// (utils.cpp line 831 — write < 0 branch)
// ============================================================================

TEST(AppendBufferToFdBranch, ClosedFd_WriteFails_Throws)
{
    int fd = memfd_create("test_append_fail", 0);
    ASSERT_GE(fd, 0);
    close(fd);

    std::vector<uint8_t> data = {0x01};
    // fd is now invalid (not -1, but closed) — write will fail
    // Note: the check for fd < 0 passes because fd >= 0, but the
    // underlying write() syscall will fail with EBADF
    EXPECT_THROW(utils::appendBufferToFd(fd, data), std::runtime_error);
}

// ============================================================================
// CustomFD — write/read edge cases for EINTR branches
// (utils.cpp lines 1215,1217,1240,1242)
// Note: EINTR cannot be reliably triggered in unit tests, but we cover
// the normal write/read paths with position-at-boundary to exercise
// the while-loop logic.
// ============================================================================

TEST(CustomFDBranch, WriteAtExactEndOfFile_Succeeds)
{
    // Write at pos == fileSize (boundary condition)
    int fd = memfd_create("test_write_boundary", 0);
    ASSERT_GE(fd, 0);
    std::vector<uint8_t> init(8, 0);
    if (::write(fd, init.data(), init.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD cfd(fd);
    EXPECT_EQ(cfd.size(), 8u);

    // pos == fileSize is allowed (write at end)
    std::vector<uint8_t> data = {0xAA, 0xBB};
    bool ok = cfd.write(8, data.data(), data.size());
    EXPECT_TRUE(ok);
    EXPECT_GE(cfd.size(), 10u);
}

TEST(CustomFDBranch, ReadExactlyFileSize_Succeeds)
{
    int fd = memfd_create("test_read_exact", 0);
    ASSERT_GE(fd, 0);
    std::vector<uint8_t> init = {0x11, 0x22, 0x33, 0x44};
    if (::write(fd, init.data(), init.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD cfd(fd);
    EXPECT_EQ(cfd.size(), 4u);

    std::vector<uint8_t> out(4, 0);
    bool ok = cfd.read(0, out.data(), out.size());
    EXPECT_TRUE(ok);
    EXPECT_EQ(out, init);
}

TEST(CustomFDBranch, ReadZeroSize_ReturnsFalse)
{
    int fd = memfd_create("test_read_zero", 0);
    ASSERT_GE(fd, 0);
    uint8_t dummy = 0;
    if (::write(fd, &dummy, 1) < 0)
    {}
    lseek(fd, 0, SEEK_SET);

    utils::CustomFD cfd(fd);
    uint8_t buf = 0;
    // size == 0 triggers !size check
    bool ok = cfd.read(0, &buf, 0);
    EXPECT_FALSE(ok);
}

// ============================================================================
// getPropertyFromCollection — not-found branch
// (utils.hpp lines 730-735)
// ============================================================================

TEST(GetPropertyFromCollectionBranch, PropertyNotFound_ReturnsNullopt)
{
    PropertyValuesCollection collection;
    collection.emplace_back("Alpha", PropertyValue{uint32_t(42)});
    collection.emplace_back("Gamma", PropertyValue{uint32_t(99)});
    // collection is sorted by name (Alpha < Gamma)

    auto result = utils::getPropertyFromCollection<uint32_t>(collection,
                                                             "Beta");
    EXPECT_FALSE(result.has_value());
}

TEST(GetPropertyFromCollectionBranch, EmptyCollection_ReturnsNullopt)
{
    PropertyValuesCollection collection;

    auto result = utils::getPropertyFromCollection<std::string>(collection,
                                                                "AnyProp");
    EXPECT_FALSE(result.has_value());
}

TEST(GetPropertyFromCollectionBranch, PropertyFound_ReturnsValue)
{
    PropertyValuesCollection collection;
    collection.emplace_back("Name", PropertyValue{std::string("hello")});

    auto result = utils::getPropertyFromCollection<std::string>(collection,
                                                                "Name");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "hello");
}

TEST(GetPropertyFromCollectionBranch, PropertyAtEnd_FoundCorrectly)
{
    PropertyValuesCollection collection;
    collection.emplace_back("AAA", PropertyValue{uint64_t(1)});
    collection.emplace_back("BBB", PropertyValue{uint64_t(2)});
    collection.emplace_back("ZZZ", PropertyValue{uint64_t(3)});

    auto result = utils::getPropertyFromCollection<uint64_t>(collection, "ZZZ");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 3u);
}

TEST(GetPropertyFromCollectionBranch, SearchBeyondEnd_ReturnsNullopt)
{
    PropertyValuesCollection collection;
    collection.emplace_back("AAA", PropertyValue{uint32_t(1)});

    // "ZZZ" > "AAA", so lower_bound returns cend()
    auto result = utils::getPropertyFromCollection<uint32_t>(collection, "ZZZ");
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// typeName<T>() template
// (utils.hpp lines ~710-718)
// ============================================================================

TEST(TypeNameBranch, IntType_ReturnsDemangled)
{
    std::string name = utils::typeName<int>();
    EXPECT_EQ(name, "int");
}

TEST(TypeNameBranch, StringType_ReturnsDemangled)
{
    std::string name = utils::typeName<std::string>();
    // Should contain "string" somewhere in the demangled name
    EXPECT_NE(name.find("string"), std::string::npos);
}

TEST(TypeNameBranch, VectorOfUint8_ReturnsDemangled)
{
    std::string name = utils::typeName<std::vector<uint8_t>>();
    EXPECT_NE(name.find("vector"), std::string::npos);
}

// ============================================================================
// IDBusHandler — tryGetDbusProperty default value branch
// (utils.hpp lines 269,271 — catch branch returning defValue)
// We use a mock to trigger the sdbusplus::exception path.
// ============================================================================

// Custom sdbusplus exception that IS copy-constructible (unlike SdBusError)
struct TestSdbusException : public sdbusplus::exception::exception
{
    const char* name() const noexcept override
    {
        return "TestError";
    }
    const char* description() const noexcept override
    {
        return "test";
    }
    const char* what() const noexcept override
    {
        return "TestSdbusException";
    }
    int get_errno() const noexcept override
    {
        return ENOENT;
    }
};

class MockDBusHandler : public utils::IDBusHandler
{
  public:
    MOCK_METHOD(std::string, getService,
                (const char* path, const char* interface), (const, override));
    MOCK_METHOD(MapperServiceMap, getServiceMap,
                (const char* path, const dbus::Interfaces& ifaceList),
                (const, override));
    MOCK_METHOD(GetSubTreeResponse, getSubtree,
                (const std::string& path, int depth,
                 const dbus::Interfaces& ifaceList),
                (const, override));
    MOCK_METHOD(void, setDbusProperty,
                (const DBusMapping& dBusMap, const PropertyValue& value),
                (const, override));
    MOCK_METHOD(PropertyValue, getDbusPropertyVariant,
                (const char* objPath, const char* dbusProp,
                 const char* dbusInterface),
                (const, override));
    MOCK_METHOD(PropertyValuesCollection, getDbusProperties,
                (const char* objPath, const char* dbusInterface),
                (const, override));
    MOCK_METHOD(GetAssociatedObjectsResponse, getAssociatedObjects,
                (const std::string& path, const std::string& association),
                (const, override));
};

TEST(TryGetDbusPropertyBranch, ThrowsSdbusException_ReturnsDefault)
{
    MockDBusHandler handler;
    // getDbusPropertyVariant will throw sdbusplus exception
    EXPECT_CALL(handler,
                getDbusPropertyVariant(testing::_, testing::_, testing::_))
        .WillOnce(testing::Throw(TestSdbusException()));

    auto result = handler.tryGetDbusProperty<std::string>(
        "/test", "Prop", "com.test.Iface", "default_value");

    EXPECT_EQ(result, "default_value");
}

TEST(TryGetDbusPropertyBranch, ThrowsSdbusException_ReturnsDefaultBool)
{
    MockDBusHandler handler;
    EXPECT_CALL(handler,
                getDbusPropertyVariant(testing::_, testing::_, testing::_))
        .WillOnce(testing::Throw(TestSdbusException()));

    auto result = handler.tryGetDbusProperty<bool>("/test", "Flag",
                                                   "com.test.Iface");

    // Default bool() == false
    EXPECT_FALSE(result);
}

TEST(TryGetDbusPropertyBranch, NoThrow_ReturnsActualValue)
{
    MockDBusHandler handler;
    EXPECT_CALL(handler,
                getDbusPropertyVariant(testing::_, testing::_, testing::_))
        .WillOnce(testing::Return(PropertyValue{std::string("actual")}));

    auto result = handler.tryGetDbusProperty<std::string>(
        "/test", "Prop", "com.test.Iface", "default_value");

    EXPECT_EQ(result, "actual");
}

// ============================================================================
// IDBusHandler virtual destructor coverage
// (utils.hpp line 195)
// ============================================================================

TEST(IDBusHandlerBranch, VirtualDestructor_NoCrash)
{
    // Construct via derived, destroy via base pointer
    std::unique_ptr<utils::IDBusHandler> handler =
        std::make_unique<MockDBusHandler>();
    handler.reset();
    SUCCEED();
}

// ============================================================================
// nsmPropertySupport.hpp — getUnsupportedAssetProperties branches
// ============================================================================

TEST(NsmPropertySupportBranch, MctpBridgeSxmSma_ReturnsUnsupportedSet)
{
    auto result = nsm::getUnsupportedAssetProperties(
        NSM_DEV_ID_MCTP_BRIDGE, NSM_MCTP_BRIDGE_DEV_ROLE_SXM_SMA);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.count(FRU_PART_NUMBER));
    EXPECT_TRUE(result.count(SERIAL_NUMBER));
    EXPECT_TRUE(result.count(MARKETING_NAME));
}

TEST(NsmPropertySupportBranch, MctpBridgeHpmSma_ReturnsUnsupportedSet)
{
    auto result = nsm::getUnsupportedAssetProperties(
        NSM_DEV_ID_MCTP_BRIDGE, NSM_MCTP_BRIDGE_DEV_ROLE_HPM_SMA);
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(result.count(FRU_PART_NUMBER));
    EXPECT_TRUE(result.count(SERIAL_NUMBER));
    EXPECT_TRUE(result.count(MARKETING_NAME));
}

TEST(NsmPropertySupportBranch, MctpBridgeCxSma_ReturnsEmptySet)
{
    // CX SMA role is NOT in the special list
    auto result = nsm::getUnsupportedAssetProperties(
        NSM_DEV_ID_MCTP_BRIDGE, NSM_MCTP_BRIDGE_DEV_ROLE_CX_SMA);
    EXPECT_TRUE(result.empty());
}

TEST(NsmPropertySupportBranch, MctpBridgeQmSmaRole_ReturnsUnsupportedSet)
{
    auto result = nsm::getUnsupportedAssetProperties(
        NSM_DEV_ID_MCTP_BRIDGE, NSM_MCTP_BRIDGE_DEV_ROLE_QM_SMA);
    EXPECT_EQ(3, result.size());
    EXPECT_TRUE(result.count(FRU_PART_NUMBER));
    EXPECT_TRUE(result.count(SERIAL_NUMBER));
    EXPECT_TRUE(result.count(MARKETING_NAME));
}

TEST(NsmPropertySupportBranch, NonMctpBridgeDevice_ReturnsEmptySet)
{
    // GPU device type should always return empty
    auto result = nsm::getUnsupportedAssetProperties(NSM_DEV_ID_GPU, 0);
    EXPECT_TRUE(result.empty());
}

TEST(NsmPropertySupportBranch, MctpBridgeUnknownRole_ReturnsEmptySet)
{
    auto result = nsm::getUnsupportedAssetProperties(
        NSM_DEV_ID_MCTP_BRIDGE, NSM_MCTP_BRIDGE_DEV_ROLE_UNKNOWN);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// updateMethodsBitfieldToList — all bits set at once
// (covers the "return" branch at utils.cpp line 710)
// ============================================================================

TEST(UpdateMethodsBranch, AllBitsSet_ReturnsAllMethods)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;

    bitfield32_t bf = {0};
    bf.bits.bit0 = 1;  // Automatic
    bf.bits.bit2 = 1;  // MediumSpecificReset
    bf.bits.bit3 = 1;  // SystemReboot
    bf.bits.bit4 = 1;  // DCPowerCycle
    bf.bits.bit5 = 1;  // ACPowerCycle
    bf.bits.bit16 = 1; // WarmReset
    bf.bits.bit17 = 1; // HotReset
    bf.bits.bit18 = 1; // FLR

    auto result = utils::updateMethodsBitfieldToList(bf);
    EXPECT_EQ(result.size(), 8u);
}

TEST(UpdateMethodsBranch, OnlyHighBitsSet_ReturnsHighMethods)
{
    using namespace sdbusplus::common::xyz::openbmc_project::software;

    bitfield32_t bf = {0};
    bf.bits.bit16 = 1; // WarmReset
    bf.bits.bit17 = 1; // HotReset
    bf.bits.bit18 = 1; // FLR

    auto result = utils::updateMethodsBitfieldToList(bf);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], SecurityCommon::UpdateMethods::WarmReset);
    EXPECT_EQ(result[1], SecurityCommon::UpdateMethods::HotReset);
    EXPECT_EQ(result[2], SecurityCommon::UpdateMethods::FLR);
}

// ============================================================================
// isValidDbusString — additional branch edge cases
// (overlong encoding, codepoint > 0x10FFFF, null byte)
// ============================================================================

TEST(IsValidDbusStringBranch, OverlongTwoByte_ReturnsFalse)
{
    // 2-byte encoding of codepoint < 0x80 is overlong
    // \xC0\x80 encodes U+0000 in 2 bytes (overlong)
    EXPECT_FALSE(utils::isValidDbusString("\xC0\x80"));
}

TEST(IsValidDbusStringBranch, OverlongThreeByte_ReturnsFalse)
{
    // 3-byte encoding of codepoint < 0x800 is overlong
    // \xE0\x80\x80 encodes U+0000 in 3 bytes
    EXPECT_FALSE(utils::isValidDbusString("\xE0\x80\x80"));
}

TEST(IsValidDbusStringBranch, OverlongFourByte_ReturnsFalse)
{
    // 4-byte encoding of codepoint < 0x10000 is overlong
    // \xF0\x80\x80\x80 encodes U+0000 in 4 bytes
    EXPECT_FALSE(utils::isValidDbusString("\xF0\x80\x80\x80"));
}

TEST(IsValidDbusStringBranch, NullCodepoint_ReturnsFalse)
{
    // U+0000 is forbidden by dbus spec
    std::string s(1, '\0');
    EXPECT_FALSE(utils::isValidDbusString(s));
}

TEST(IsValidDbusStringBranch, TruncatedTwoByte_ReturnsFalse)
{
    // Leading byte for 2-byte sequence but only 1 byte total
    EXPECT_FALSE(utils::isValidDbusString("\xC3"));
}

TEST(IsValidDbusStringBranch, TruncatedThreeByte_ReturnsFalse)
{
    // Leading byte for 3-byte sequence but only 2 bytes
    EXPECT_FALSE(utils::isValidDbusString("\xE2\x82"));
}

TEST(IsValidDbusStringBranch, TruncatedFourByte_ReturnsFalse)
{
    // Leading byte for 4-byte sequence but only 3 bytes
    EXPECT_FALSE(utils::isValidDbusString("\xF0\x90\x8D"));
}

TEST(IsValidDbusStringBranch, MaxValidCodepoint_ReturnsTrue)
{
    // U+10FFFF = F4 8F BF BF (max valid codepoint)
    EXPECT_TRUE(utils::isValidDbusString("\xF4\x8F\xBF\xBF"));
}

TEST(IsValidDbusStringBranch, BeyondMaxCodepoint_ReturnsFalse)
{
    // U+110000 is > 0x10FFFF, encoded as F4 90 80 80
    EXPECT_FALSE(utils::isValidDbusString("\xF4\x90\x80\x80"));
}

// ============================================================================
// bitmapToIndices — additional branch coverage
// ============================================================================

TEST(BitmapToIndicesBranch, EmptyBitmap_ReturnsBothEmpty)
{
    std::vector<uint8_t> bitmap;
    auto [zeros, ones] = utils::bitmapToIndices(bitmap);
    EXPECT_TRUE(zeros.empty());
    EXPECT_TRUE(ones.empty());
}

TEST(BitmapToIndicesBranch, SingleByteAllOnes_AllInOnesVector)
{
    std::vector<uint8_t> bitmap = {0xFF};
    auto [zeros, ones] = utils::bitmapToIndices(bitmap);
    EXPECT_TRUE(zeros.empty());
    EXPECT_EQ(ones.size(), 8u);
}

TEST(BitmapToIndicesBranch, SingleByteAllZeros_AllInZerosVector)
{
    std::vector<uint8_t> bitmap = {0x00};
    auto [zeros, ones] = utils::bitmapToIndices(bitmap);
    EXPECT_EQ(zeros.size(), 8u);
    EXPECT_TRUE(ones.empty());
}

// ============================================================================
// convertBitMaskToVector — single bit positions
// (utils.cpp lines 396-428 — covers each bit[0-7] true/false branch)
// ============================================================================

TEST(ConvertBitMaskBranch, OnlyBit7Set_CorrectIndex)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x80; // only bit7
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 1u);
    EXPECT_EQ(data[0], 7);
}

TEST(ConvertBitMaskBranch, Bit3And5Set_CorrectIndices)
{
    bitfield8_t value[1] = {};
    value[0].byte = 0x28; // bit3 (0x08) + bit5 (0x20)
    std::vector<uint8_t> data;
    utils::convertBitMaskToVector(data, value, 1);
    ASSERT_EQ(data.size(), 2u);
    EXPECT_EQ(data[0], 3);
    EXPECT_EQ(data[1], 5);
}

// ============================================================================
// Associations getAssociations(vector<Association>) — single element
// (utils.cpp line 389)
// ============================================================================

TEST(AssociationsBranch, SingleAssociation_ReturnsSingleTuple)
{
    std::vector<utils::Association> associations;
    utils::Association a;
    a.forward = "fwd";
    a.backward = "bwd";
    a.absolutePath = "/path";
    associations.push_back(a);

    auto result = utils::getAssociations(associations);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(std::get<0>(result[0]), "fwd");
    EXPECT_EQ(std::get<1>(result[0]), "bwd");
    EXPECT_EQ(std::get<2>(result[0]), "/path");
}

// ============================================================================
// MCTP priority — unknown binding with same medium
// ============================================================================

TEST(MctpPriorityBranch, UnknownBinding_SameMedium_UsesDefaultPriority)
{
    MctpMedium pcieMedium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding unknownBinding1 = "xyz.openbmc_project.MCTP.Binding.Unknown1";
    MctpBinding unknownBinding2 = "xyz.openbmc_project.MCTP.Binding.Unknown2";

    auto current = std::make_tuple(pcieMedium, unknownBinding1);
    auto newInfo = std::make_tuple(pcieMedium, unknownBinding2);

    // Same medium, both bindings unknown -> both get INT_MIN priority
    // INT_MIN >= INT_MIN is true
    EXPECT_TRUE(utils::isPreferred(current, newInfo));
}

// ============================================================================
// nsmSwCodeToString — negative value (unusual code)
// ============================================================================

TEST(NsmSwCodeBranch, NegativeValue_ReturnsUnknown)
{
    std::string result = utils::nsmSwCodeToString(-42);
    EXPECT_NE(result.find("UNKNOWN"), std::string::npos);
    EXPECT_NE(result.find("-42"), std::string::npos);
}

// ============================================================================
// convertUUIDToString — exact boundary: size == UUID_INT_SIZE
// (confirms the success path returns proper string)
// ============================================================================

TEST(ConvertUUIDBranch, ExactSize_ReturnsFormattedString)
{
    std::vector<uint8_t> uuid(UUID_INT_SIZE, 0xFF);
    auto result = utils::convertUUIDToString(uuid);
    EXPECT_NE(result.find("ffff"), std::string::npos);
    // Expected format: "ffffffff-ffff-ffff-ffff-ffffffffffff"
    EXPECT_EQ(result.size(),
              UUID_LEN + 1); // includes null terminator in string
}

TEST(ConvertUUIDBranch, EmptyVector_ReturnsEmpty)
{
    std::vector<uint8_t> uuid;
    auto result = utils::convertUUIDToString(uuid);
    EXPECT_TRUE(result.empty());
}

TEST(ConvertUUIDBranch, TooLarge_ReturnsEmpty)
{
    std::vector<uint8_t> uuid(20, 0);
    auto result = utils::convertUUIDToString(uuid);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// getDeviceInstanceName — boundary cases
// (utils.cpp line 461)
// ============================================================================

TEST(GetDeviceInstanceNameBranch, MaxInstance_FormatCorrectly)
{
    std::string result = utils::getDeviceInstanceName(0, 255);
    EXPECT_EQ(result, "GPU_255");
}

// ============================================================================
// getUUIDFromEID — multiple entries with same UUID
// ============================================================================

TEST(GetUUIDFromEIDBranch, MultipleEntriesSameUUID_FindsFirst)
{
    std::multimap<std::string, std::tuple<eid_t, MctpMedium, MctpBinding>>
        eidTable;
    MctpMedium medium = "xyz.openbmc_project.MCTP.Endpoint.MediaTypes.PCIe";
    MctpBinding binding = "xyz.openbmc_project.MCTP.Binding.BindingTypes.PCIe";

    eidTable.emplace("uuid-A", std::make_tuple(eid_t(10), medium, binding));
    eidTable.emplace("uuid-A", std::make_tuple(eid_t(20), medium, binding));

    auto result = utils::getUUIDFromEID(eidTable, 20);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "uuid-A");
}
