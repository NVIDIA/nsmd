/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for common/utils.hpp template methods.
 * Targets: getDbusProperty<T>, tryGetDbusProperty<T> with many variant types,
 *          getPropertyFromCollection<T> boundary cases,
 *          split() edge cases, convertBitfieldToVector, isValidDbusString,
 *          convertMacAddressToString, convertGuid64ToString,
 *          nsmSwCodeToString/nsmCompletionCodeToString/nsmReasonCodeToString,
 *          parseStaticUuid additional paths.
 */

#include "utils.hpp"

#include <sdbusplus/exception.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace utils;
using ::testing::_;
using ::testing::Return;
using ::testing::Throw;

// ============================================================================
// Mock DBus handler for template method branch coverage
// ============================================================================

struct TestSdbusException2 : public sdbusplus::exception::exception
{
    const char* name() const noexcept override
    {
        return "TestError2";
    }
    const char* description() const noexcept override
    {
        return "test2";
    }
    const char* what() const noexcept override
    {
        return "TestSdbusException2";
    }
    int get_errno() const noexcept override
    {
        return ENOENT;
    }
};

class MockDBusHandler2 : public IDBusHandler
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

// ============================================================================
// getDbusProperty<T> — success path with many PropertyValue variant types
// Each instantiation exercises the std::get<T> branch in the template.
// ============================================================================

TEST(GetDbusPropertyBranch2, Bool_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{true}));
    auto result = handler.getDbusProperty<bool>("/obj", "prop", "iface");
    EXPECT_TRUE(result);
}

TEST(GetDbusPropertyBranch2, Uint8_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{uint8_t(42)}));
    auto result = handler.getDbusProperty<uint8_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, 42);
}

TEST(GetDbusPropertyBranch2, Int16_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{int16_t(-100)}));
    auto result = handler.getDbusProperty<int16_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, -100);
}

TEST(GetDbusPropertyBranch2, Uint16_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{uint16_t(1000)}));
    auto result = handler.getDbusProperty<uint16_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, 1000);
}

TEST(GetDbusPropertyBranch2, Int32_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{int32_t(-50000)}));
    auto result = handler.getDbusProperty<int32_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, -50000);
}

TEST(GetDbusPropertyBranch2, Uint32_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{uint32_t(123456)}));
    auto result = handler.getDbusProperty<uint32_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, 123456u);
}

TEST(GetDbusPropertyBranch2, Int64_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{int64_t(-999999)}));
    auto result = handler.getDbusProperty<int64_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, -999999);
}

TEST(GetDbusPropertyBranch2, Uint64_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{uint64_t(1ULL << 40)}));
    auto result = handler.getDbusProperty<uint64_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, 1ULL << 40);
}

TEST(GetDbusPropertyBranch2, Double_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{3.14}));
    auto result = handler.getDbusProperty<double>("/obj", "prop", "iface");
    EXPECT_DOUBLE_EQ(result, 3.14);
}

TEST(GetDbusPropertyBranch2, String_Success)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{std::string("hello")}));
    auto result = handler.getDbusProperty<std::string>("/obj", "prop", "iface");
    EXPECT_EQ(result, "hello");
}

TEST(GetDbusPropertyBranch2, VectorString_Success)
{
    MockDBusHandler2 handler;
    std::vector<std::string> val = {"a", "b", "c"};
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{val}));
    auto result = handler.getDbusProperty<std::vector<std::string>>(
        "/obj", "prop", "iface");
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "a");
}

TEST(GetDbusPropertyBranch2, VectorUint64_Success)
{
    MockDBusHandler2 handler;
    std::vector<uint64_t> val = {1, 2, 3};
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{val}));
    auto result = handler.getDbusProperty<std::vector<uint64_t>>("/obj", "prop",
                                                                 "iface");
    EXPECT_EQ(result.size(), 3u);
}

TEST(GetDbusPropertyBranch2, VectorObjectPath_Success)
{
    MockDBusHandler2 handler;
    std::vector<sdbusplus::object_path> val = {sdbusplus::object_path("/a"),
                                               sdbusplus::object_path("/b")};
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{val}));
    auto result = handler.getDbusProperty<std::vector<sdbusplus::object_path>>(
        "/obj", "prop", "iface");
    EXPECT_EQ(result.size(), 2u);
}

TEST(GetDbusPropertyBranch2, VectorTupleString_Success)
{
    MockDBusHandler2 handler;
    std::vector<std::tuple<std::string, std::string, std::string>> val = {
        {"a", "b", "c"}};
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{val}));
    auto result = handler.getDbusProperty<
        std::vector<std::tuple<std::string, std::string, std::string>>>(
        "/obj", "prop", "iface");
    EXPECT_EQ(result.size(), 1u);
}

// ============================================================================
// getDbusProperty<T> — bad_variant_access when type mismatch
// ============================================================================

TEST(GetDbusPropertyBranch2, TypeMismatch_ThrowsBadVariantAccess)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{std::string("wrong")}));
    EXPECT_THROW(handler.getDbusProperty<uint32_t>("/obj", "prop", "iface"),
                 std::bad_variant_access);
}

// ============================================================================
// tryGetDbusProperty<T> — success and exception paths for multiple types
// Each instantiation covers both try and catch branches.
// ============================================================================

TEST(TryGetDbusPropertyBranch2, Uint8_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<uint8_t>("/obj", "prop", "iface",
                                                      uint8_t(99));
    EXPECT_EQ(result, 99);
}

TEST(TryGetDbusPropertyBranch2, Uint8_Success_ReturnsActual)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{uint8_t(42)}));
    auto result = handler.tryGetDbusProperty<uint8_t>("/obj", "prop", "iface",
                                                      uint8_t(99));
    EXPECT_EQ(result, 42);
}

TEST(TryGetDbusPropertyBranch2, Int16_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<int16_t>("/obj", "prop", "iface",
                                                      int16_t(-1));
    EXPECT_EQ(result, -1);
}

TEST(TryGetDbusPropertyBranch2, Uint16_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<uint16_t>("/obj", "prop", "iface",
                                                       uint16_t(500));
    EXPECT_EQ(result, 500);
}

TEST(TryGetDbusPropertyBranch2, Int32_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<int32_t>("/obj", "prop", "iface",
                                                      int32_t(-999));
    EXPECT_EQ(result, -999);
}

TEST(TryGetDbusPropertyBranch2, Uint32_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<uint32_t>("/obj", "prop", "iface",
                                                       uint32_t(77777));
    EXPECT_EQ(result, 77777u);
}

TEST(TryGetDbusPropertyBranch2, Int64_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<int64_t>("/obj", "prop", "iface",
                                                      int64_t(-1LL));
    EXPECT_EQ(result, -1LL);
}

TEST(TryGetDbusPropertyBranch2, Uint64_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<uint64_t>("/obj", "prop", "iface",
                                                       uint64_t(0));
    EXPECT_EQ(result, 0u);
}

TEST(TryGetDbusPropertyBranch2, Double_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<double>("/obj", "prop", "iface",
                                                     -1.0);
    EXPECT_DOUBLE_EQ(result, -1.0);
}

TEST(TryGetDbusPropertyBranch2, Double_Success_ReturnsActual)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Return(PropertyValue{2.718}));
    auto result = handler.tryGetDbusProperty<double>("/obj", "prop", "iface",
                                                     -1.0);
    EXPECT_DOUBLE_EQ(result, 2.718);
}

TEST(TryGetDbusPropertyBranch2, VectorString_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    std::vector<std::string> defVal = {"default"};
    auto result = handler.tryGetDbusProperty<std::vector<std::string>>(
        "/obj", "prop", "iface", defVal);
    EXPECT_EQ(result, defVal);
}

TEST(TryGetDbusPropertyBranch2, VectorUint64_Exception_ReturnsDefault)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    std::vector<uint64_t> defVal = {0};
    auto result = handler.tryGetDbusProperty<std::vector<uint64_t>>(
        "/obj", "prop", "iface", defVal);
    EXPECT_EQ(result, defVal);
}

// tryGetDbusProperty with default (no explicit defValue)
TEST(TryGetDbusPropertyBranch2, Int32_Exception_DefaultConstructed)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<int32_t>("/obj", "prop", "iface");
    EXPECT_EQ(result, 0); // int32_t() == 0
}

TEST(TryGetDbusPropertyBranch2, String_Exception_DefaultConstructed)
{
    MockDBusHandler2 handler;
    EXPECT_CALL(handler, getDbusPropertyVariant(_, _, _))
        .WillOnce(Throw(TestSdbusException2()));
    auto result = handler.tryGetDbusProperty<std::string>("/obj", "prop",
                                                          "iface");
    EXPECT_EQ(result, ""); // std::string() == ""
}

// ============================================================================
// getPropertyFromCollection<T> — additional type instantiations
// ============================================================================

TEST(GetPropertyFromCollectionBranch2, Bool_Found)
{
    PropertyValuesCollection col = {{"flag", PropertyValue{true}}};
    auto result = getPropertyFromCollection<bool>(col, "flag");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(*result);
}

TEST(GetPropertyFromCollectionBranch2, Bool_NotFound)
{
    PropertyValuesCollection col = {{"flag", PropertyValue{true}}};
    auto result = getPropertyFromCollection<bool>(col, "missing");
    EXPECT_FALSE(result.has_value());
}

TEST(GetPropertyFromCollectionBranch2, Double_Found)
{
    PropertyValuesCollection col = {{"val", PropertyValue{1.5}}};
    auto result = getPropertyFromCollection<double>(col, "val");
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(*result, 1.5);
}

TEST(GetPropertyFromCollectionBranch2, Uint8_Found)
{
    PropertyValuesCollection col = {{"byte", PropertyValue{uint8_t(0xFF)}}};
    auto result = getPropertyFromCollection<uint8_t>(col, "byte");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 0xFF);
}

TEST(GetPropertyFromCollectionBranch2, Int16_Found)
{
    PropertyValuesCollection col = {{"val", PropertyValue{int16_t(-32000)}}};
    auto result = getPropertyFromCollection<int16_t>(col, "val");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -32000);
}

TEST(GetPropertyFromCollectionBranch2, Uint16_Found)
{
    PropertyValuesCollection col = {{"val", PropertyValue{uint16_t(65000)}}};
    auto result = getPropertyFromCollection<uint16_t>(col, "val");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 65000);
}

TEST(GetPropertyFromCollectionBranch2, Int64_Found)
{
    PropertyValuesCollection col = {
        {"val", PropertyValue{int64_t(-999999999LL)}}};
    auto result = getPropertyFromCollection<int64_t>(col, "val");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -999999999LL);
}

TEST(GetPropertyFromCollectionBranch2, VectorString_Found)
{
    std::vector<std::string> vec = {"x", "y"};
    PropertyValuesCollection col = {{"arr", PropertyValue{vec}}};
    auto result = getPropertyFromCollection<std::vector<std::string>>(col,
                                                                      "arr");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 2u);
}

TEST(GetPropertyFromCollectionBranch2, VectorUint64_Found)
{
    std::vector<uint64_t> vec = {10, 20, 30};
    PropertyValuesCollection col = {{"arr", PropertyValue{vec}}};
    auto result = getPropertyFromCollection<std::vector<uint64_t>>(col, "arr");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size(), 3u);
}

// Boundary: key exactly matches first element
TEST(GetPropertyFromCollectionBranch2, FirstElement_Found)
{
    PropertyValuesCollection col = {{"aaa", PropertyValue{uint32_t(1)}},
                                    {"bbb", PropertyValue{uint32_t(2)}},
                                    {"ccc", PropertyValue{uint32_t(3)}}};
    auto result = getPropertyFromCollection<uint32_t>(col, "aaa");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1u);
}

// Boundary: key exactly matches last element
TEST(GetPropertyFromCollectionBranch2, LastElement_Found)
{
    PropertyValuesCollection col = {{"aaa", PropertyValue{uint32_t(1)}},
                                    {"bbb", PropertyValue{uint32_t(2)}},
                                    {"ccc", PropertyValue{uint32_t(3)}}};
    auto result = getPropertyFromCollection<uint32_t>(col, "ccc");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 3u);
}

// Boundary: key between existing keys (not found, but lower_bound != end)
TEST(GetPropertyFromCollectionBranch2, BetweenKeys_NotFound)
{
    PropertyValuesCollection col = {{"aaa", PropertyValue{uint32_t(1)}},
                                    {"ccc", PropertyValue{uint32_t(3)}}};
    auto result = getPropertyFromCollection<uint32_t>(col, "bbb");
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// split() additional edge cases
// ============================================================================

// Delimiter at start and end with trim
TEST(SplitBranch2, DelimiterAtStartAndEnd_WithTrim)
{
    auto result = split(",  hello  , world  ,", ",", " ");
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
}

// Single character string, no delimiter match
TEST(SplitBranch2, SingleChar_NoDelim)
{
    auto result = split("x", ",");
    EXPECT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "x");
}

// All characters are delimiter => empty result
TEST(SplitBranch2, AllDelimiter_EmptyResult)
{
    auto result = split(",,,", ",");
    EXPECT_TRUE(result.empty());
}

// Trim partially removes characters from tokens
TEST(SplitBranch2, TrimPartial)
{
    auto result = split("aahelloa,aworlda", ",", "a");
    EXPECT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], "hello");
    EXPECT_EQ(result[1], "world");
}

// Multi-char delimiter
TEST(SplitBranch2, MultiCharDelim)
{
    auto result = split("foo::bar::baz", "::");
    EXPECT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "foo");
    EXPECT_EQ(result[1], "bar");
    EXPECT_EQ(result[2], "baz");
}

// ============================================================================
// convertBitfieldToVector — various bitfield patterns
// ============================================================================

TEST(ConvertBitfieldToVectorBranch2, EmptyBitfield)
{
    std::vector<uint8_t> bitfield;
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    EXPECT_TRUE(result.empty());
}

TEST(ConvertBitfieldToVectorBranch2, SingleByteAllSet)
{
    std::vector<uint8_t> bitfield = {0xFF};
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    EXPECT_EQ(result.size(), 8u);
    for (auto v : result)
    {
        EXPECT_TRUE(v);
    }
}

TEST(ConvertBitfieldToVectorBranch2, SingleByteAllClear)
{
    std::vector<uint8_t> bitfield = {0x00};
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    EXPECT_EQ(result.size(), 8u);
    for (auto v : result)
    {
        EXPECT_FALSE(v);
    }
}

TEST(ConvertBitfieldToVectorBranch2, SingleByteAlternating)
{
    std::vector<uint8_t> bitfield = {0xAA}; // 10101010
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    EXPECT_EQ(result.size(), 8u);
    // bit 0 = 0, bit 1 = 1, bit 2 = 0, bit 3 = 1, ...
    EXPECT_FALSE(result[0]);
    EXPECT_TRUE(result[1]);
    EXPECT_FALSE(result[2]);
    EXPECT_TRUE(result[3]);
}

TEST(ConvertBitfieldToVectorBranch2, TwoBytesPartial)
{
    std::vector<uint8_t> bitfield = {0x01, 0x80}; // bit0=1, bit15=1
    std::vector<bool> result;
    convertBitfieldToVector(bitfield, result);
    EXPECT_EQ(result.size(), 16u);
    EXPECT_TRUE(result[0]);
    for (size_t i = 1; i < 15; i++)
    {
        EXPECT_FALSE(result[i]);
    }
    EXPECT_TRUE(result[15]);
}

// ============================================================================
// convertMacAddressToString — edge cases
// ============================================================================

TEST(ConvertMacAddressBranch2, ExactLength)
{
    std::vector<uint8_t> mac = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    std::string result;
    convertMacAddressToString(mac.data(), mac.size(), result);
    EXPECT_EQ(result, "aa:bb:cc:dd:ee:ff");
}

TEST(ConvertMacAddressBranch2, LongerThanRequired)
{
    std::vector<uint8_t> mac = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    std::string result;
    convertMacAddressToString(mac.data(), mac.size(), result);
    // Should still work, uses only first 6 bytes
    EXPECT_EQ(result, "01:02:03:04:05:06");
}

TEST(ConvertMacAddressBranch2, TooShort)
{
    std::vector<uint8_t> mac = {0x01, 0x02, 0x03};
    std::string result = "previous";
    convertMacAddressToString(mac.data(), mac.size(), result);
    EXPECT_TRUE(result.empty());
}

TEST(ConvertMacAddressBranch2, AllZeros)
{
    std::vector<uint8_t> mac = {0, 0, 0, 0, 0, 0};
    std::string result;
    convertMacAddressToString(mac.data(), mac.size(), result);
    EXPECT_EQ(result, "00:00:00:00:00:00");
}

// ============================================================================
// convertGuid64ToString — various values
// ============================================================================

TEST(ConvertGuid64Branch2, ZeroGuid)
{
    std::string result;
    convertGuid64ToString(0, result);
    EXPECT_EQ(result, "0000-0000-0000-0000");
}

TEST(ConvertGuid64Branch2, MaxGuid)
{
    std::string result;
    convertGuid64ToString(UINT64_MAX, result);
    EXPECT_EQ(result, "FFFF-FFFF-FFFF-FFFF");
}

TEST(ConvertGuid64Branch2, PartialGuid)
{
    std::string result;
    convertGuid64ToString(0x0001000200030004ULL, result);
    EXPECT_EQ(result, "0001-0002-0003-0004");
}

// ============================================================================
// parseStaticUuid — additional paths
// ============================================================================

// Multi-value path (colon-separated values)
TEST(ParseStaticUuidBranch2, MultipleValues)
{
    uuid_t uuid = "STATIC:514:0:Name:val1:val2:val3";
    uint8_t dt = 0, in = 0, dr = 0;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(rp, "Name");
    EXPECT_EQ(rv.size(), 3u);
    EXPECT_EQ(rv[0], "val1");
    EXPECT_EQ(rv[1], "val2");
    EXPECT_EQ(rv[2], "val3");
}

// Single value path (no colon in value)
TEST(ParseStaticUuidBranch2, SingleValue)
{
    uuid_t uuid = "STATIC:514:0:Prop:SingleVal";
    uint8_t dt = 0, in = 0, dr = 0;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(rp, "Prop");
    EXPECT_EQ(rv.size(), 1u);
    EXPECT_EQ(rv[0], "SingleVal");
}

// Two colon-separated values
TEST(ParseStaticUuidBranch2, TwoValues)
{
    uuid_t uuid = "STATIC:514:5:Attr:first:second";
    uint8_t dt = 0, in = 0, dr = 0;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(in, 5);
    EXPECT_EQ(rp, "Attr");
    EXPECT_EQ(rv.size(), 2u);
    EXPECT_EQ(rv[0], "first");
    EXPECT_EQ(rv[1], "second");
}

// Invalid format: not enough fields
TEST(ParseStaticUuidBranch2, TooFewFields)
{
    uuid_t uuid = "STATIC:514:0";
    uint8_t dt = 0, in = 0, dr = 0;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, -3);
}

// Not starting with STATIC
TEST(ParseStaticUuidBranch2, NotStaticPrefix)
{
    uuid_t uuid = "DYNAMIC:514:0:Prop:Val";
    uint8_t dt = 0, in = 0, dr = 0;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, -3);
}

// Zero combined value
TEST(ParseStaticUuidBranch2, ZeroCombined)
{
    uuid_t uuid = "STATIC:0:0:Prop:Val";
    uint8_t dt = 0xFF, in = 0xFF, dr = 0xFF;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(dt, 0);
    EXPECT_EQ(dr, 0);
    EXPECT_EQ(in, 0);
}

// Max valid combined value (0xFFFF)
TEST(ParseStaticUuidBranch2, MaxCombined)
{
    uuid_t uuid = "STATIC:65535:255:P:V";
    uint8_t dt = 0, in = 0, dr = 0;
    std::string rp;
    std::vector<std::string> rv;
    int rc = parseStaticUuid(uuid, dt, in, dr, rp, rv);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(dt, 0xFF);
    EXPECT_EQ(dr, 0xFF);
    EXPECT_EQ(in, 255);
}

// ============================================================================
// isValidDbusString — UTF-8 boundary: valid 2-byte, 3-byte, 4-byte
// ============================================================================

TEST(IsValidDbusStringBranch2, Valid2ByteMinimal)
{
    // U+0080 = C2 80 (smallest valid 2-byte encoding)
    EXPECT_TRUE(isValidDbusString("\xC2\x80"));
}

TEST(IsValidDbusStringBranch2, Valid3ByteMinimal)
{
    // U+0800 = E0 A0 80 (smallest valid 3-byte encoding)
    EXPECT_TRUE(isValidDbusString("\xE0\xA0\x80"));
}

TEST(IsValidDbusStringBranch2, Valid4ByteMinimal)
{
    // U+10000 = F0 90 80 80 (smallest valid 4-byte encoding)
    EXPECT_TRUE(isValidDbusString("\xF0\x90\x80\x80"));
}

TEST(IsValidDbusStringBranch2, MixedValidUtf8)
{
    // ASCII + 2-byte + 3-byte + 4-byte
    EXPECT_TRUE(isValidDbusString("A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"));
}

TEST(IsValidDbusStringBranch2, InvalidLeadingByte_FE)
{
    EXPECT_FALSE(isValidDbusString("\xFE"));
}

TEST(IsValidDbusStringBranch2, InvalidContinuationByte)
{
    // 3-byte sequence with bad second continuation byte
    EXPECT_FALSE(isValidDbusString("\xE2\x82\x40"));
}

// ============================================================================
// typeName<T>() — additional instantiations for branch coverage
// ============================================================================

TEST(TypeNameBranch2, VoidType)
{
    std::string name = typeName<void>();
    EXPECT_EQ(name, "void");
}

TEST(TypeNameBranch2, ConstVoidPtr)
{
    std::string name = typeName<const void*>();
    EXPECT_EQ(name, "void const*");
}

// ============================================================================
// convertAndScaleDownUint32ToDouble — scale factor 1
// ============================================================================

TEST(ConvertScaleBranch2, ScaleFactor1)
{
    double result = convertAndScaleDownUint32ToDouble(200, 1.0);
    EXPECT_DOUBLE_EQ(result, 200.0);
}

TEST(ConvertScaleBranch2, LargeScaleFactor)
{
    double result = convertAndScaleDownUint32ToDouble(1000000, 1000.0);
    EXPECT_DOUBLE_EQ(result, 1000.0);
}

// ============================================================================
// convertAndScaleDownUint8ToDouble — various values
// ============================================================================

TEST(ConvertScaleBranch2, Uint8Max)
{
    double result = convertAndScaleDownUint8ToDouble(255);
    EXPECT_DOUBLE_EQ(result, 255.0);
}

TEST(ConvertScaleBranch2, Uint8Mid)
{
    double result = convertAndScaleDownUint8ToDouble(128);
    EXPECT_DOUBLE_EQ(result, 128.0);
}

// ============================================================================
// uint64ToDoubleSafeConvert — additional values
// ============================================================================

TEST(SafeConvertBranch2, Uint64Zero)
{
    double result = uint64ToDoubleSafeConvert(0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(SafeConvertBranch2, Uint64One)
{
    double result = uint64ToDoubleSafeConvert(1);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

TEST(SafeConvertBranch2, Uint64MaxSafe)
{
    uint64_t value = MAX_SAFE_INTEGER_IN_DOUBLE;
    double result = uint64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(value));
}

// ============================================================================
// int64ToDoubleSafeConvert — additional values
// ============================================================================

TEST(SafeConvertBranch2, Int64Zero)
{
    double result = int64ToDoubleSafeConvert(0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(SafeConvertBranch2, Int64NegOne)
{
    double result = int64ToDoubleSafeConvert(-1);
    EXPECT_DOUBLE_EQ(result, -1.0);
}

TEST(SafeConvertBranch2, Int64PositiveSafe)
{
    int64_t value = static_cast<int64_t>(MAX_SAFE_INTEGER_IN_DOUBLE);
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(value));
}

TEST(SafeConvertBranch2, Int64NegativeSafe)
{
    int64_t value = -static_cast<int64_t>(MAX_SAFE_INTEGER_IN_DOUBLE);
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(value));
}

// ============================================================================
// Bitfield256 — additional edge cases
// ============================================================================

TEST(Bitfield256Branch2, SetBitZero)
{
    Bitfield256 bf;
    EXPECT_TRUE(bf.setBit(0));
    EXPECT_TRUE(bf.isAnyBitSet());
    std::string bits = bf.getSetBits();
    EXPECT_FALSE(bits.empty());
}

TEST(Bitfield256Branch2, SetBitMax)
{
    Bitfield256 bf;
    EXPECT_TRUE(bf.setBit(255));
    EXPECT_TRUE(bf.isAnyBitSet());
}

TEST(Bitfield256Branch2, SetSameBitTwice)
{
    Bitfield256 bf;
    EXPECT_TRUE(bf.setBit(42));
    EXPECT_FALSE(bf.setBit(42));
}

TEST(Bitfield256Branch2, SetMultipleBits_GetSetBits)
{
    Bitfield256 bf;
    bf.setBit(0);
    bf.setBit(127);
    bf.setBit(255);
    std::string bits = bf.getSetBits();
    EXPECT_FALSE(bits.empty());
    // Should contain commas for multiple bits
    EXPECT_NE(bits.find(","), std::string::npos);
}

// ============================================================================
// convertMsgToString — Rx direction
// ============================================================================

TEST(ConvertMsgToStringBranch2, RxDirection)
{
    std::vector<uint8_t> buffer = {0xAB, 0xCD};
    auto result = convertMsgToString(Rx, buffer, 0x01, 0x02);
    EXPECT_NE(result.find("Rx"), std::string::npos);
}

TEST(ConvertMsgToStringBranch2, TxDirection)
{
    std::vector<uint8_t> buffer = {0x01};
    auto result = convertMsgToString(Tx, buffer, 0x00, 0x00);
    EXPECT_NE(result.find("Tx"), std::string::npos);
}

TEST(ConvertMsgToStringBranch2, EmptyBuffer)
{
    std::vector<uint8_t> buffer;
    auto result = convertMsgToString(Tx, buffer, 0x00, 0x00);
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// makeDBusNameValid — edge cases
// ============================================================================

TEST(MakeDBusNameValidBranch2, AllValid)
{
    std::string result = makeDBusNameValid("Valid_Name.123/path");
    EXPECT_EQ(result, "Valid_Name.123/path");
}

TEST(MakeDBusNameValidBranch2, SpecialCharsReplaced)
{
    std::string result = makeDBusNameValid("name with spaces!");
    EXPECT_EQ(result.find(' '), std::string::npos);
    EXPECT_EQ(result.find('!'), std::string::npos);
}

TEST(MakeDBusNameValidBranch2, EmptyString)
{
    std::string result = makeDBusNameValid("");
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// combineDeviceTypeAndRole / getDeviceTypeAndRole round-trip
// ============================================================================

TEST(DeviceTypeRoleBranch2, RoundTrip)
{
    uint8_t type = 0x12, role = 0x34;
    uint16_t combined = combineDeviceTypeAndRole(type, role);
    uint8_t outType = 0, outRole = 0;
    getDeviceTypeAndRole(combined, &outType, &outRole);
    EXPECT_EQ(outType, type);
    EXPECT_EQ(outRole, role);
}

TEST(DeviceTypeRoleBranch2, OnlyTypeBits)
{
    uint16_t combined = combineDeviceTypeAndRole(0xFF, 0x00);
    EXPECT_EQ(combined, 0x00FF);
    uint8_t outType = 0, outRole = 0;
    getDeviceTypeAndRole(combined, &outType, &outRole);
    EXPECT_EQ(outType, 0xFF);
    EXPECT_EQ(outRole, 0x00);
}

TEST(DeviceTypeRoleBranch2, OnlyRoleBits)
{
    uint16_t combined = combineDeviceTypeAndRole(0x00, 0xFF);
    EXPECT_EQ(combined, 0xFF00);
    uint8_t outType = 0, outRole = 0;
    getDeviceTypeAndRole(combined, &outType, &outRole);
    EXPECT_EQ(outType, 0x00);
    EXPECT_EQ(outRole, 0xFF);
}
