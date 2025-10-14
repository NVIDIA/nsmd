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

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmEnergy.hpp"
#include "nsmNumericSensor.hpp"
#include "nsmNumericSensorValue_mock.hpp"
#include "nsmPower.hpp"
#include "nsmTemp.hpp"

class NsmNumericSensorValueAggregateTest : public ::testing::Test
{
  protected:
    sdbusplus::bus::bus& bus = utils::DBusHandler::getBus();
    std::string sensorName = "test_sensor";
    std::string sensorType = "test_type";
    std::vector<utils::Association> associations = {
        {"chassis", "all_sensors",
         "/xyz/openbmc_project/inventory/test_device"}};
    std::string physicalContext = "GPU";
    double maxAllowableValue = std::numeric_limits<double>::infinity();
    std::string readingBasis = "Headroom";
    std::string description = "test sensor description";
};

// Test NsmTemp with single DbusValue element
TEST_F(NsmNumericSensorValueAggregateTest, NsmTempSingleDbusValue)
{
    nsm::NsmTemp tempSensor{bus,
                            sensorName,
                            sensorType,
                            1,
                            associations,
                            associations[0].absolutePath,
                            physicalContext,
                            nullptr,
                            maxAllowableValue,
                            &readingBasis,
                            &description};

    auto aggregate = tempSensor.getSensorValueObject();
    ASSERT_NE(aggregate, nullptr);

    // Find the DbusValue element
    nsm::NsmNumericSensorDbusValue* dbusValue = nullptr;
    for (const auto& obj : aggregate->objects)
    {
        dbusValue = dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get());
        if (dbusValue)
            break;
    }
    ASSERT_NE(dbusValue, nullptr);

    const double reading1 = 45.5;
    const double reading2 = 50.0;

    // First update should always work
    aggregate->updateReading(reading1);

    // Check that nextUpdateTimestamp was updated in the DbusValue
    EXPECT_GT(dbusValue->nextUpdateTimestamp, 0);
    uint64_t firstTimestamp = dbusValue->nextUpdateTimestamp;

    // Second update immediately (within 1000ms) should not update DbusValue
    aggregate->updateReading(reading2);

    // nextUpdateTimestamp should not change
    EXPECT_EQ(dbusValue->nextUpdateTimestamp, firstTimestamp);

    // Wait for > 1000ms and try again
    std::this_thread::sleep_for(std::chrono::milliseconds(1001));

    aggregate->updateReading(reading2);

    // Now nextUpdateTimestamp should be updated
    EXPECT_GT(dbusValue->nextUpdateTimestamp, firstTimestamp);
}

// Test NsmPower with multiple elements including DbusValue
TEST_F(NsmNumericSensorValueAggregateTest, NsmPowerMultipleElements)
{
    // Create NsmPower sensor
    nsm::NsmPower powerSensor{bus,
                              "power_sensor",
                              sensorType,
                              1,
                              1, // averaging interval
                              associations,
                              associations[0].absolutePath,
                              physicalContext,
                              nullptr,
                              maxAllowableValue,
                              &readingBasis,
                              &description};

    auto aggregate = powerSensor.getSensorValueObject();
    ASSERT_NE(aggregate, nullptr);

    // NsmPower can have multiple elements:
    // 1. NsmNumericSensorShmem (if NVIDIA_SHMEM) - NOT a DbusValue
    // 2. NsmNumericSensorDbusValueTimestamp - IS a DbusValue
    // 3. NsmNumericSensorCompositeChildValue (if configured) - NOT a DbusValue
    // Note: In this test without config, expect 1-2 elements depending on
    // NVIDIA_SHMEM
#ifdef NVIDIA_SHMEM
    EXPECT_GE(aggregate->objects.size(), 2);
#else
    EXPECT_GE(aggregate->objects.size(), 1);
#endif

    // Find the DbusValue element
    nsm::NsmNumericSensorDbusValue* dbusValue = nullptr;
    size_t dbusValueCount = 0;
    for (const auto& obj : aggregate->objects)
    {
        auto* dv = dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get());
        if (dv)
        {
            dbusValue = dv;
            dbusValueCount++;
        }
    }
    // Verify exactly 1 element can be dynamically cast to
    // NsmNumericSensorDbusValue
    EXPECT_EQ(dbusValueCount, 1);
    ASSERT_NE(dbusValue, nullptr);

    const double reading1 = 150.5;
    const double reading2 = 175.0;

    // First update should work for all elements
    aggregate->updateReading(reading1);

    EXPECT_GT(dbusValue->nextUpdateTimestamp, 0);
    uint64_t firstTimestamp = dbusValue->nextUpdateTimestamp;

    // Second update immediately (within 1000ms) should not update DbusValue
    aggregate->updateReading(reading2);

    // nextUpdateTimestamp should not change
    EXPECT_EQ(dbusValue->nextUpdateTimestamp, firstTimestamp);

    // Wait for > 1000ms and try again
    std::this_thread::sleep_for(std::chrono::milliseconds(1001));

    aggregate->updateReading(reading2);

    // Now nextUpdateTimestamp should be updated
    EXPECT_GT(dbusValue->nextUpdateTimestamp, firstTimestamp);
}

// Test NsmEnergy with single DbusValue element
TEST_F(NsmNumericSensorValueAggregateTest, NsmEnergySingleDbusValue)
{
    nsm::NsmEnergy energySensor{bus,
                                sensorName,
                                sensorType,
                                1,
                                associations,
                                associations[0].absolutePath,
                                physicalContext,
                                nullptr,
                                maxAllowableValue,
                                &readingBasis,
                                &description};

    auto aggregate = energySensor.getSensorValueObject();
    ASSERT_NE(aggregate, nullptr);

    // Find the DbusValue element
    nsm::NsmNumericSensorDbusValue* dbusValue = nullptr;
    for (const auto& obj : aggregate->objects)
    {
        dbusValue = dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get());
        if (dbusValue)
            break;
    }
    ASSERT_NE(dbusValue, nullptr);

    const double reading1 = 1000.0;
    const double reading2 = 2000.0;

    // First update should always work
    aggregate->updateReading(reading1);

    EXPECT_GT(dbusValue->nextUpdateTimestamp, 0);
    uint64_t firstTimestamp = dbusValue->nextUpdateTimestamp;

    // Second update immediately (within 1000ms) should not update DbusValue
    aggregate->updateReading(reading2);

    // nextUpdateTimestamp should not change
    EXPECT_EQ(dbusValue->nextUpdateTimestamp, firstTimestamp);

    // Wait for > 1000ms and try again
    std::this_thread::sleep_for(std::chrono::milliseconds(1001));

    aggregate->updateReading(reading2);

    // Now nextUpdateTimestamp should be updated
    EXPECT_GT(dbusValue->nextUpdateTimestamp, firstTimestamp);
}

// Test mixed elements: DbusValue and non-DbusValue (mock)
TEST_F(NsmNumericSensorValueAggregateTest, MixedDbusValueAndNonDbusValue)
{
    auto mockValue = std::make_unique<MockNsmNumericSensorValue>();
    auto mockValuePtr = mockValue.get();

    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>(
        std::make_unique<nsm::NsmNumericSensorDbusValue>(
            bus, "mixed_sensor_dbus", "power", nsm::SensorUnit::Watts,
            associations, physicalContext, nullptr, maxAllowableValue,
            &readingBasis, &description),
        std::move(mockValue));

    EXPECT_EQ(aggregate->objects.size(), 2);

    const double reading = 100.0;

    // Mock (non-DbusValue) should be called every time
    EXPECT_CALL(*mockValuePtr, updateReading(reading, testing::Gt(0))).Times(1);

    aggregate->updateReading(reading);

    // Immediately call again - mock should still be called
    EXPECT_CALL(*mockValuePtr, updateReading(reading, testing::Gt(0))).Times(1);

    aggregate->updateReading(reading);
}

// Test timing threshold boundary at exactly 1000ms
TEST_F(NsmNumericSensorValueAggregateTest, TimingThresholdBoundary)
{
    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>(
        std::make_unique<nsm::NsmNumericSensorDbusValue>(
            bus, "threshold_sensor", "power", nsm::SensorUnit::Watts,
            associations, physicalContext, nullptr, maxAllowableValue,
            &readingBasis, &description));

    // Find the DbusValue element
    nsm::NsmNumericSensorDbusValue* dbusValue = nullptr;
    for (const auto& obj : aggregate->objects)
    {
        dbusValue = dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get());
        if (dbusValue)
            break;
    }
    ASSERT_NE(dbusValue, nullptr);

    const double reading = 75.0;

    // First update
    aggregate->updateReading(reading);
    uint64_t firstTimestamp = dbusValue->nextUpdateTimestamp;
    EXPECT_GT(firstTimestamp, 0);

    // Update before nextUpdateTimestamp should NOT trigger update
    // We simulate this by manually setting nextUpdateTimestamp to future
    auto currentTime = utils::getCurrentSteadyClockTimestamp();
    dbusValue->nextUpdateTimestamp = currentTime + 1000;
    uint64_t futureTimestamp = dbusValue->nextUpdateTimestamp;

    aggregate->updateReading(reading);

    // nextUpdateTimestamp should NOT be updated (timestamp <
    // nextUpdateTimestamp)
    EXPECT_EQ(dbusValue->nextUpdateTimestamp, futureTimestamp);

    // Update at or after nextUpdateTimestamp SHOULD trigger update
    dbusValue->nextUpdateTimestamp = currentTime - 1;
    uint64_t pastTimestamp = dbusValue->nextUpdateTimestamp;

    aggregate->updateReading(reading);

    // nextUpdateTimestamp should be updated now (should be > current time)
    EXPECT_GT(dbusValue->nextUpdateTimestamp, pastTimestamp);
    EXPECT_GT(dbusValue->nextUpdateTimestamp, currentTime);
}

// Test with all mock objects (no DbusValue)
TEST_F(NsmNumericSensorValueAggregateTest, AllNonDbusValues)
{
    auto mockValue1 = std::make_unique<MockNsmNumericSensorValue>();
    auto mockValue1Ptr = mockValue1.get();
    auto mockValue2 = std::make_unique<MockNsmNumericSensorValue>();
    auto mockValue2Ptr = mockValue2.get();

    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>(
        std::move(mockValue1), std::move(mockValue2));

    EXPECT_EQ(aggregate->objects.size(), 2);

    const double reading = 42.0;

    // Both mocks should be called every time, regardless of timing
    EXPECT_CALL(*mockValue1Ptr, updateReading(reading, testing::Gt(0)))
        .Times(1);
    EXPECT_CALL(*mockValue2Ptr, updateReading(reading, testing::Gt(0)))
        .Times(1);

    aggregate->updateReading(reading);

    // Call again immediately - both should still be called
    EXPECT_CALL(*mockValue1Ptr, updateReading(reading, testing::Gt(0)))
        .Times(1);
    EXPECT_CALL(*mockValue2Ptr, updateReading(reading, testing::Gt(0)))
        .Times(1);

    aggregate->updateReading(reading);
}

// Test append method - appending additional elements to aggregate
TEST_F(NsmNumericSensorValueAggregateTest, AppendMethod)
{
    // Create NsmPower sensor (has 1 or 2 elements depending on NVIDIA_SHMEM)
    nsm::NsmPower powerSensor{bus,
                              "power_append_sensor",
                              sensorType,
                              1,
                              1, // averaging interval
                              associations,
                              associations[0].absolutePath,
                              physicalContext,
                              nullptr,
                              maxAllowableValue,
                              &readingBasis,
                              &description};

    auto aggregate = powerSensor.getSensorValueObject();
    ASSERT_NE(aggregate, nullptr);

    size_t initialSize = aggregate->objects.size();

    // Test appending additional element
    aggregate->append(std::make_unique<nsm::NsmNumericSensorDbusValue>(
        bus, "power_sensor_append_2", "power", nsm::SensorUnit::Watts,
        associations, physicalContext, nullptr, maxAllowableValue,
        &readingBasis, &description));

    EXPECT_EQ(aggregate->objects.size(), initialSize + 1);

    // After appending, we should have 2 DbusValue elements now
    // (1 from NsmPower + 1 we just appended)
    size_t dbusValueCount = 0;
    for (const auto& obj : aggregate->objects)
    {
        if (dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get()))
        {
            dbusValueCount++;
        }
    }
    EXPECT_EQ(dbusValueCount, 2);

    const double reading = 200.0;

    // First update should work
    aggregate->updateReading(reading);

    // Find any DbusValue to check timestamp
    nsm::NsmNumericSensorDbusValue* dbusValue = nullptr;
    for (const auto& obj : aggregate->objects)
    {
        dbusValue = dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get());
        if (dbusValue)
            break;
    }
    ASSERT_NE(dbusValue, nullptr);

    EXPECT_GT(dbusValue->nextUpdateTimestamp, 0);
    uint64_t firstTimestamp = dbusValue->nextUpdateTimestamp;

    // Immediate second update should not update nextUpdateTimestamp
    aggregate->updateReading(reading);
    EXPECT_EQ(dbusValue->nextUpdateTimestamp, firstTimestamp);
}

// Test empty aggregate
TEST_F(NsmNumericSensorValueAggregateTest, EmptyAggregate)
{
    auto aggregate = std::make_shared<nsm::NsmNumericSensorValueAggregate>();

    EXPECT_EQ(aggregate->objects.size(), 0);

    const double reading = 100.0;

    // Should not crash with empty objects
    aggregate->updateReading(reading);
}

// Test multiple consecutive updates with proper delays
TEST_F(NsmNumericSensorValueAggregateTest, MultipleUpdatesWithDelays)
{
    nsm::NsmTemp tempSensor{bus,
                            "multi_update_sensor",
                            sensorType,
                            1,
                            associations,
                            associations[0].absolutePath,
                            physicalContext,
                            nullptr,
                            maxAllowableValue,
                            &readingBasis,
                            &description};

    auto aggregate = tempSensor.getSensorValueObject();
    ASSERT_NE(aggregate, nullptr);

    // Find the DbusValue element
    nsm::NsmNumericSensorDbusValue* dbusValue = nullptr;
    for (const auto& obj : aggregate->objects)
    {
        dbusValue = dynamic_cast<nsm::NsmNumericSensorDbusValue*>(obj.get());
        if (dbusValue)
            break;
    }
    ASSERT_NE(dbusValue, nullptr);

    std::vector<double> readings = {25.0, 30.0, 35.0};
    std::vector<uint64_t> timestamps;

    // First update
    aggregate->updateReading(readings[0]);
    timestamps.push_back(dbusValue->nextUpdateTimestamp);
    EXPECT_GT(timestamps[0], 0);

    // Update immediately - should not change timestamp
    aggregate->updateReading(readings[1]);
    EXPECT_EQ(dbusValue->nextUpdateTimestamp, timestamps[0]);

    // Wait and update - should change timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    aggregate->updateReading(readings[2]);
    timestamps.push_back(dbusValue->nextUpdateTimestamp);
    EXPECT_GT(timestamps[1], timestamps[0]);
}
