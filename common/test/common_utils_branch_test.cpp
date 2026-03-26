/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

// Branch Coverage Tests for common/utils.cpp
// Focus: Bitfield operations, conversions, validations

#include "utils.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <sdbusplus/exception.hpp>

#include <gtest/gtest.h>

using namespace utils;

// Branch Coverage Tests for isValidDbusString
// Note: isValidDbusString validates UTF-8 encoding, not DBus naming rules

TEST(CommonUtilsBranch, IsValidDbusStringEmptyString)
{
    bool result = isValidDbusString("");
    EXPECT_TRUE(result); // Empty string is valid UTF-8
}

TEST(CommonUtilsBranch, IsValidDbusStringStartsWithDigit)
{
    bool result = isValidDbusString("1InvalidName");
    EXPECT_TRUE(
        result); // Valid UTF-8 (ASCII), function doesn't check naming rules
}

TEST(CommonUtilsBranch, IsValidDbusStringContainsInvalidChar)
{
    bool result = isValidDbusString("Invalid-Name");
    EXPECT_TRUE(
        result); // Hyphen is valid UTF-8, function doesn't check naming rules
}

TEST(CommonUtilsBranch, IsValidDbusStringValidName)
{
    bool result = isValidDbusString("ValidName123");
    EXPECT_TRUE(result);
}

// Branch Coverage Tests for Bitfield256 class

TEST(CommonUtilsBranch, Bitfield256SetBitValid)
{
    Bitfield256 bf;
    bool result = bf.setBit(100);
    EXPECT_TRUE(result);
    EXPECT_TRUE(bf.isAnyBitSet());
}

TEST(CommonUtilsBranch, Bitfield256IsAnyBitSetEmpty)
{
    Bitfield256 bf;
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(CommonUtilsBranch, Bitfield256IsAnyBitSetAfterSet)
{
    Bitfield256 bf;
    bf.setBit(50);
    EXPECT_TRUE(bf.isAnyBitSet());
}

TEST(CommonUtilsBranch, Bitfield256ClearAfterSet)
{
    Bitfield256 bf;
    bf.setBit(10);
    bf.setBit(20);
    bf.clear();
    EXPECT_FALSE(bf.isAnyBitSet());
}

TEST(CommonUtilsBranch, Bitfield256GetSetBitsEmpty)
{
    Bitfield256 bf;
    std::string result = bf.getSetBits();
    EXPECT_TRUE(result.empty());
}

TEST(CommonUtilsBranch, Bitfield256GetSetBitsMultiple)
{
    Bitfield256 bf;
    bf.setBit(5);
    bf.setBit(10);
    std::string result = bf.getSetBits();
    EXPECT_FALSE(result.empty());
}

// Branch Coverage Tests for double conversion functions

TEST(CommonUtilsBranch, ConvertAndScaleDownUint32ZeroValue)
{
    double result = convertAndScaleDownUint32ToDouble(0, 100.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(CommonUtilsBranch, ConvertAndScaleDownUint32MaxValue)
{
    double result = convertAndScaleDownUint32ToDouble(UINT32_MAX, 1.0);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(UINT32_MAX));
}

TEST(CommonUtilsBranch, ConvertAndScaleDownUint8ZeroValue)
{
    double result = convertAndScaleDownUint8ToDouble(0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(CommonUtilsBranch, Uint64ToDoubleSafeConvertMaxSafeValue)
{
    uint64_t value = (1ULL << 53);
    double result = uint64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(value));
}

TEST(CommonUtilsBranch, Uint64ToDoubleSafeConvertExceedsMax)
{
    uint64_t value = (1ULL << 54);
    double result = uint64ToDoubleSafeConvert(value);
    EXPECT_LE(result, static_cast<double>(1ULL << 53));
}

TEST(CommonUtilsBranch, Int64ToDoubleSafeConvertPositiveMax)
{
    int64_t value = (1LL << 53);
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_DOUBLE_EQ(result, static_cast<double>(value));
}

TEST(CommonUtilsBranch, Int64ToDoubleSafeConvertNegativeMin)
{
    int64_t value = -(1LL << 53);
    double result = int64ToDoubleSafeConvert(value);
    // Use 1LL (signed) to get correct negative value; 1ULL would wrap around
    EXPECT_DOUBLE_EQ(result, static_cast<double>(-((1LL << 53) - 1)));
}

TEST(CommonUtilsBranch, Int64ToDoubleSafeConvertExceedsPositiveMax)
{
    int64_t value = (1LL << 54);
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_LE(result, static_cast<double>(1LL << 53));
}

TEST(CommonUtilsBranch, Int64ToDoubleSafeConvertExceedsNegativeMin)
{
    int64_t value = -(1LL << 54);
    double result = int64ToDoubleSafeConvert(value);
    EXPECT_GE(result, static_cast<double>(-(1LL << 53)));
}

// Branch Coverage Tests for combineDeviceTypeAndRole
// Note: Function combines as (role << 8) | type (role in high byte, type in low
// byte)

TEST(CommonUtilsBranch, CombineDeviceTypeAndRoleZeroValues)
{
    uint16_t result = combineDeviceTypeAndRole(0, 0);
    EXPECT_EQ(result, 0);
}

TEST(CommonUtilsBranch, CombineDeviceTypeAndRoleMaxValues)
{
    uint16_t result = combineDeviceTypeAndRole(0xFF, 0xFF);
    EXPECT_EQ(result, 0xFFFF);
}

TEST(CommonUtilsBranch, CombineDeviceTypeAndRoleMixedValues)
{
    uint16_t result = combineDeviceTypeAndRole(0x12, 0x34);
    EXPECT_EQ(result, 0x3412); // (0x34 << 8) | 0x12 = 0x3412
}

// Branch Coverage Tests for getDeviceTypeAndRole
// Note: Function extracts type from low byte, role from high byte

TEST(CommonUtilsBranch, GetDeviceTypeAndRoleValid)
{
    uint8_t deviceType = 0;
    uint8_t deviceRole = 0;
    getDeviceTypeAndRole(0xABCD, &deviceType, &deviceRole);
    EXPECT_EQ(deviceType, 0xCD); // Low byte
    EXPECT_EQ(deviceRole, 0xAB); // High byte
}

TEST(CommonUtilsBranch, GetDeviceTypeAndRoleZeroCombined)
{
    uint8_t deviceType = 0xFF;
    uint8_t deviceRole = 0xFF;
    getDeviceTypeAndRole(0, &deviceType, &deviceRole);
    EXPECT_EQ(deviceType, 0);
    EXPECT_EQ(deviceRole, 0);
}

// ============================================================================
// Branch coverage: readFdToBuffer / writeBufferToFd error paths
// ============================================================================

// Line 763: lseek fails on a pipe → readFdToBuffer throws
TEST(CommonUtilsBranch, ReadFdToBuffer_LseekFails_Throws)
{
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::vector<uint8_t> buffer;
    EXPECT_THROW(utils::readFdToBuffer(pipefd[0], buffer), std::runtime_error);

    close(pipefd[0]);
    close(pipefd[1]);
}

// Lines 779-780: lseek OK (regular file), but read() returns EBADF on
// a write-only fd → readFdToBuffer throws
TEST(CommonUtilsBranch, ReadFdToBuffer_ReadFails_Throws)
{
    char tmpPath[] = "/tmp/utils_branch_test_XXXXXX";
    int tmpFd = mkstemp(tmpPath);
    ASSERT_GE(tmpFd, 0);
    const uint8_t data[] = {0xAA, 0xBB, 0xCC};
    ASSERT_EQ(::write(tmpFd, data, sizeof(data)), (ssize_t)sizeof(data));
    close(tmpFd);

    // Reopen write-only: lseek and fstat succeed, but read() fails with EBADF
    int fd = open(tmpPath, O_WRONLY);
    unlink(tmpPath);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buffer;
    EXPECT_THROW(utils::readFdToBuffer(fd, buffer), std::runtime_error);

    close(fd);
}

// Line 797: lseek fails on a pipe → writeBufferToFd throws
TEST(CommonUtilsBranch, WriteBufferToFd_LseekFails_Throws)
{
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);

    std::vector<uint8_t> buffer = {1, 2, 3};
    EXPECT_THROW(utils::writeBufferToFd(pipefd[1], buffer), std::runtime_error);

    close(pipefd[0]);
    close(pipefd[1]);
}

// Lines 806-807: lseek OK (regular file), but write() returns EBADF on
// a read-only fd with non-empty buffer → writeBufferToFd throws
TEST(CommonUtilsBranch, WriteBufferToFd_WriteFails_Throws)
{
    char tmpPath[] = "/tmp/utils_branch_write_XXXXXX";
    int tmpFd = mkstemp(tmpPath);
    ASSERT_GE(tmpFd, 0);
    close(tmpFd);

    // Reopen read-only: lseek succeeds, write() fails with EBADF
    int fd = open(tmpPath, O_RDONLY);
    unlink(tmpPath);
    ASSERT_GE(fd, 0);

    std::vector<uint8_t> buffer = {1, 2, 3};
    EXPECT_THROW(utils::writeBufferToFd(fd, buffer), std::runtime_error);

    close(fd);
}

// Test for IDBusHandler::tryGetDbusProperty catch block (L269-271 in utils.hpp)
// Create a concrete subclass of IDBusHandler that throws sdbusplus::exception
// from getDbusPropertyVariant to cover the catch block.

struct ThrowingSdBusHandler : public IDBusHandler
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
        // Throw a concrete sdbusplus exception to exercise the catch block
        // in tryGetDbusProperty
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

// L269-271: tryGetDbusProperty catch block
// When getDbusPropertyVariant throws sdbusplus::exception, returns defValue
TEST(CommonUtilsBranch, TryGetDbusProperty_SdBusException_ReturnsDefault)
{
    ThrowingSdBusHandler handler;
    const std::string defValue = "default";
    auto result = handler.tryGetDbusProperty<std::string>("path", "prop",
                                                          "intf", defValue);
    EXPECT_EQ(result, defValue);
}
