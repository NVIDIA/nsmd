/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <sdbusplus/bus.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public
#include "nsmNumericSensor/nsmThresholdValue.hpp"
#undef private
#undef protected

using ::testing::_;
using ::testing::Return;

using namespace nsm;

// Fixture for threshold value tests
class NsmThresholdValueTest : public ::testing::Test
{
  protected:
    sdbusplus::bus::bus bus = sdbusplus::bus::new_default();
    const std::string testName = "TestSensor";
    const std::string testType = "TestType";
};

// ============================================================================
// NsmThresholdValueWarningLow Tests
// ============================================================================

TEST_F(NsmThresholdValueTest, WarningLowConstructorInitializesCorrectly)
{
    // Test that constructor properly initializes base class and interface
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningLow sensor(testName, testType, mockIntf);

    // Verify basic construction (no crash)
    EXPECT_EQ(sensor.intf, mockIntf);
}

TEST_F(NsmThresholdValueTest, WarningLowUpdateReadingCallsInterfaceMethod)
{
    // Test updateReading() calls warningLow() on interface
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningLow sensor(testName, testType, mockIntf);

    const double testValue = 42.5;
    const uint64_t timestamp = 1234567890;

    // Call updateReading and verify interface method is called
    sensor.updateReading(testValue, timestamp);

    // Verify by checking the dbus property was set
    EXPECT_EQ(mockIntf->warningLow(), testValue);
}

TEST_F(NsmThresholdValueTest, WarningLowUpdateReadingWithZeroValue)
{
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningLow sensor(testName, testType, mockIntf);

    sensor.updateReading(0.0, 0);
    EXPECT_EQ(mockIntf->warningLow(), 0.0);
}

TEST_F(NsmThresholdValueTest, WarningLowUpdateReadingWithNegativeValue)
{
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningLow sensor(testName, testType, mockIntf);

    sensor.updateReading(-25.5, 0);
    EXPECT_EQ(mockIntf->warningLow(), -25.5);
}

// ============================================================================
// NsmThresholdValueWarningHigh Tests
// ============================================================================

TEST_F(NsmThresholdValueTest, WarningHighConstructorInitializesCorrectly)
{
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningHigh sensor(testName, testType, mockIntf);

    EXPECT_EQ(sensor.intf, mockIntf);
}

TEST_F(NsmThresholdValueTest, WarningHighUpdateReadingCallsInterfaceMethod)
{
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningHigh sensor(testName, testType, mockIntf);

    const double testValue = 85.3;
    sensor.updateReading(testValue, 0);

    EXPECT_EQ(mockIntf->warningHigh(), testValue);
}

TEST_F(NsmThresholdValueTest, WarningHighUpdateReadingWithLargeValue)
{
    auto mockIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueWarningHigh sensor(testName, testType, mockIntf);

    sensor.updateReading(999999.99, 0);
    EXPECT_EQ(mockIntf->warningHigh(), 999999.99);
}

// ============================================================================
// NsmThresholdValueCriticalLow Tests
// ============================================================================

TEST_F(NsmThresholdValueTest, CriticalLowConstructorInitializesCorrectly)
{
    auto mockIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueCriticalLow sensor(testName, testType, mockIntf);

    EXPECT_EQ(sensor.intf, mockIntf);
}

TEST_F(NsmThresholdValueTest, CriticalLowUpdateReadingCallsInterfaceMethod)
{
    auto mockIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueCriticalLow sensor(testName, testType, mockIntf);

    const double testValue = 10.5;
    sensor.updateReading(testValue, 0);

    EXPECT_EQ(mockIntf->criticalLow(), testValue);
}

TEST_F(NsmThresholdValueTest, CriticalLowUpdateReadingWithMinValue)
{
    auto mockIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueCriticalLow sensor(testName, testType, mockIntf);

    sensor.updateReading(-std::numeric_limits<double>::max(), 0);
    EXPECT_EQ(mockIntf->criticalLow(), -std::numeric_limits<double>::max());
}

// ============================================================================
// NsmThresholdValueCriticalHigh Tests
// ============================================================================

TEST_F(NsmThresholdValueTest, CriticalHighConstructorInitializesCorrectly)
{
    auto mockIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueCriticalHigh sensor(testName, testType, mockIntf);

    EXPECT_EQ(sensor.intf, mockIntf);
}

TEST_F(NsmThresholdValueTest, CriticalHighUpdateReadingCallsInterfaceMethod)
{
    auto mockIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueCriticalHigh sensor(testName, testType, mockIntf);

    const double testValue = 95.7;
    sensor.updateReading(testValue, 0);

    EXPECT_EQ(mockIntf->criticalHigh(), testValue);
}

TEST_F(NsmThresholdValueTest, CriticalHighUpdateReadingWithMaxValue)
{
    auto mockIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueCriticalHigh sensor(testName, testType, mockIntf);

    sensor.updateReading(std::numeric_limits<double>::max(), 0);
    EXPECT_EQ(mockIntf->criticalHigh(), std::numeric_limits<double>::max());
}

// ============================================================================
// NsmThresholdValueHardShutdownLow Tests
// ============================================================================

TEST_F(NsmThresholdValueTest, HardShutdownLowConstructorInitializesCorrectly)
{
    auto mockIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueHardShutdownLow sensor(testName, testType, mockIntf);

    EXPECT_EQ(sensor.intf, mockIntf);
}

TEST_F(NsmThresholdValueTest, HardShutdownLowUpdateReadingCallsInterfaceMethod)
{
    auto mockIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueHardShutdownLow sensor(testName, testType, mockIntf);

    const double testValue = 5.0;
    sensor.updateReading(testValue, 0);

    EXPECT_EQ(mockIntf->hardShutdownLow(), testValue);
}

TEST_F(NsmThresholdValueTest, HardShutdownLowUpdateReadingWithVerySmallValue)
{
    auto mockIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueHardShutdownLow sensor(testName, testType, mockIntf);

    sensor.updateReading(0.0001, 0);
    EXPECT_NEAR(mockIntf->hardShutdownLow(), 0.0001, 0.00001);
}

// ============================================================================
// NsmThresholdValueHardShutdownHigh Tests
// ============================================================================

TEST_F(NsmThresholdValueTest, HardShutdownHighConstructorInitializesCorrectly)
{
    auto mockIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueHardShutdownHigh sensor(testName, testType, mockIntf);

    EXPECT_EQ(sensor.intf, mockIntf);
}

TEST_F(NsmThresholdValueTest, HardShutdownHighUpdateReadingCallsInterfaceMethod)
{
    auto mockIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueHardShutdownHigh sensor(testName, testType, mockIntf);

    const double testValue = 100.0;
    sensor.updateReading(testValue, 0);

    EXPECT_EQ(mockIntf->hardShutdownHigh(), testValue);
}

TEST_F(NsmThresholdValueTest, HardShutdownHighUpdateReadingWithVeryLargeValue)
{
    auto mockIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test");

    NsmThresholdValueHardShutdownHigh sensor(testName, testType, mockIntf);

    sensor.updateReading(1e6, 0);
    EXPECT_EQ(mockIntf->hardShutdownHigh(), 1e6);
}

// ============================================================================
// Edge Case Tests - All threshold types
// ============================================================================

TEST_F(NsmThresholdValueTest, AllTypesHandleTimestampParameter)
{
    // Verify that timestamp parameter is accepted (even though it's unused)
    auto warningIntf = std::make_shared<ThresholdWarningIntf>(
        bus, "/xyz/openbmc_project/sensors/test1");
    auto criticalIntf = std::make_shared<ThresholdCriticalIntf>(
        bus, "/xyz/openbmc_project/sensors/test2");
    auto shutdownIntf = std::make_shared<ThresholdHardShutdownIntf>(
        bus, "/xyz/openbmc_project/sensors/test3");

    NsmThresholdValueWarningLow warnLow(testName, testType, warningIntf);
    NsmThresholdValueWarningHigh warnHigh(testName, testType, warningIntf);
    NsmThresholdValueCriticalLow critLow(testName, testType, criticalIntf);
    NsmThresholdValueCriticalHigh critHigh(testName, testType, criticalIntf);
    NsmThresholdValueHardShutdownLow shutLow(testName, testType, shutdownIntf);
    NsmThresholdValueHardShutdownHigh shutHigh(testName, testType,
                                               shutdownIntf);

    const uint64_t timestamp = 9999999999;

    // All should accept timestamp without error
    warnLow.updateReading(1.0, timestamp);
    warnHigh.updateReading(2.0, timestamp);
    critLow.updateReading(3.0, timestamp);
    critHigh.updateReading(4.0, timestamp);
    shutLow.updateReading(5.0, timestamp);
    shutHigh.updateReading(6.0, timestamp);

    EXPECT_EQ(warningIntf->warningLow(), 1.0);
    EXPECT_EQ(warningIntf->warningHigh(), 2.0);
    EXPECT_EQ(criticalIntf->criticalLow(), 3.0);
    EXPECT_EQ(criticalIntf->criticalHigh(), 4.0);
    EXPECT_EQ(shutdownIntf->hardShutdownLow(), 5.0);
    EXPECT_EQ(shutdownIntf->hardShutdownHigh(), 6.0);
}
