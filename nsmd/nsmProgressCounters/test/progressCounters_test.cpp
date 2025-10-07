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

#include "base.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "commonMock.hpp"
#include "counterProducer.hpp"
#include "nsmDevice.hpp"
#include "progressCounterType.hpp"
#include "progressCounters.hpp"

using namespace ::testing;
using namespace nsm;

class ProgressCountersTest : public Test
{
  protected:
    uuid_t uuid = "00000000-0000-0000-0000-000000000001";
    MockNsmDeviceBase mockDevice{1, 1, "MCTP_UUID", uuid, 1};
    ProgressCounters progressCounters{mockDevice};
};

// Test constructor
TEST_F(ProgressCountersTest, Constructor)
{
    // Verify initial state
    EXPECT_EQ(progressCounters.dumpIteration, 0);
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.startTime, 0);
    EXPECT_EQ(progressCounters.lastUpdateTime, 0);

    // Verify counters are initialized to zero
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

#ifdef NVIDIA_PROGRESS_COUNTER

// Test increment with PollingType
TEST_F(ProgressCountersTest, IncrementWithPollingType)
{
    uint64_t currentTime = 1000000; // 1 second in microseconds
    uint8_t rc = NSM_SUCCESS;

    // Test Priority polling
    progressCounters.increment(PollingType::Priority, rc, currentTime);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        1);
    EXPECT_EQ(progressCounters.totalCount, 1);
    EXPECT_EQ(progressCounters.startTime, currentTime);
    EXPECT_EQ(progressCounters.lastUpdateTime, currentTime);
}

// Test increment with PollingType - GpuPerformanceMonitoring
TEST_F(ProgressCountersTest, IncrementWithPollingTypeGPM)
{
    uint64_t currentTime = 2000000; // 2 seconds in microseconds
    uint8_t rc = NSM_SUCCESS;

    progressCounters.increment(PollingType::GpuPerformanceMonitoring, rc,
                               currentTime);

    EXPECT_EQ(progressCounters.counters[static_cast<uint32_t>(
                  ProgressCounterType::GpuPerformanceMonitoring)],
              1);
    EXPECT_EQ(progressCounters.totalCount, 1);
}

// Test increment with ProgressCounterType and NSM_SUCCESS
TEST_F(ProgressCountersTest, IncrementWithProgressCounterTypeSuccess)
{
    uint64_t currentTime = 3000000; // 3 seconds in microseconds
    uint8_t rc = NSM_SUCCESS;

    progressCounters.increment(ProgressCounterType::RoundRobin, rc,
                               currentTime);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        1);
    EXPECT_EQ(progressCounters.totalCount, 1);
    EXPECT_EQ(progressCounters.startTime, currentTime);
    EXPECT_EQ(progressCounters.lastUpdateTime, currentTime);
}

// Test increment with ProgressCounterType and NSM_SW_ERROR_TIMEOUT
TEST_F(ProgressCountersTest, IncrementWithProgressCounterTypeTimeout)
{
    uint64_t currentTime = 4000000; // 4 seconds in microseconds
    uint8_t rc = NSM_SW_ERROR_TIMEOUT;

    // Try to increment RoundRobin, but it should increment Timeout instead
    progressCounters.increment(ProgressCounterType::RoundRobin, rc,
                               currentTime);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Timeout)],
        1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        0);
    EXPECT_EQ(progressCounters.totalCount, 1);
}

// Test increment with ProgressCounterType and error return code
TEST_F(ProgressCountersTest, IncrementWithProgressCounterTypeError)
{
    uint64_t currentTime = 5000000; // 5 seconds in microseconds
    uint8_t rc = NSM_ERROR; // Any error code other than SUCCESS or TIMEOUT

    // Try to increment Static, but it should increment Error instead
    progressCounters.increment(ProgressCounterType::Static, rc, currentTime);

    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Error)],
              1);
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Static)],
              0);
    EXPECT_EQ(progressCounters.totalCount, 1);
}

// Test multiple increments
TEST_F(ProgressCountersTest, MultipleIncrements)
{
    uint64_t startTime = 1000000;
    uint8_t rc = NSM_SUCCESS;

    // Increment different counter types
    progressCounters.increment(ProgressCounterType::Priority, rc, startTime);
    progressCounters.increment(ProgressCounterType::Priority, rc,
                               startTime + 100000);
    progressCounters.increment(ProgressCounterType::RoundRobin, rc,
                               startTime + 200000);
    progressCounters.increment(ProgressCounterType::Static, rc,
                               startTime + 300000);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        2);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        1);
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Static)],
              1);
    EXPECT_EQ(progressCounters.totalCount, 4);
    EXPECT_EQ(progressCounters.startTime, startTime);
    EXPECT_EQ(progressCounters.lastUpdateTime, startTime + 300000);
}

// Test updateCounters
TEST_F(ProgressCountersTest, UpdateCounters)
{
    uint64_t currentTime = 6000000;
    uint8_t rc = NSM_SUCCESS;

    // Add some counters
    progressCounters.increment(ProgressCounterType::Priority, rc, currentTime);
    progressCounters.increment(ProgressCounterType::RoundRobin, rc,
                               currentTime);

    EXPECT_EQ(progressCounters.totalCount, 2);
    EXPECT_EQ(progressCounters.dumpIteration, 0);

    // Manually call updateCounters
    progressCounters.updateCounters();

    // After update, counters should be reset
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.startTime, 0);
    EXPECT_EQ(progressCounters.lastUpdateTime, 0);
    EXPECT_EQ(progressCounters.dumpIteration, 1);

    // All counters should be zero
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }

    // DeviceCounterDumpObject should be created
    EXPECT_NE(progressCounters.deviceCounterDumpObject, nullptr);
}

// Test updateCountersIfNeeded - count threshold exceeded
TEST_F(ProgressCountersTest, UpdateCountersIfNeededCountExceeded)
{
    uint64_t currentTime = 7000000;
    uint8_t rc = NSM_SUCCESS;

    // Increment counters until threshold is exceeded (threshold uses >)
    // Need THRESHOLD + 1 increments to trigger dump
    for (uint32_t i = 0; i <= SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD;
         i++)
    {
        progressCounters.increment(ProgressCounterType::Priority, rc,
                                   currentTime + i * 1000);
    }

    // After exceeding threshold, counters should be reset
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.dumpIteration, 1);

    // All counters should be zero after dump
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

// Test updateCountersIfNeeded - time threshold exceeded
TEST_F(ProgressCountersTest, UpdateCountersIfNeededTimeExceeded)
{
    uint64_t startTime = 8000000;
    uint8_t rc = NSM_SUCCESS;

    // First increment to set startTime
    progressCounters.increment(ProgressCounterType::Priority, rc, startTime);

    EXPECT_EQ(progressCounters.totalCount, 1);
    EXPECT_EQ(progressCounters.startTime, startTime);

    // Increment with time that exceeds threshold
    uint64_t endTime = startTime + SENSOR_PROGRESS_COUNTERS_DUMP_TIME_THRESHOLD;
    progressCounters.increment(ProgressCounterType::Priority, rc, endTime);

    // After exceeding time threshold, counters should be reset
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.dumpIteration, 1);

    // All counters should be zero after dump
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

// Test updateCountersIfNeeded - neither threshold exceeded
TEST_F(ProgressCountersTest, UpdateCountersIfNeededNoThreshold)
{
    uint64_t startTime = 9000000;
    uint8_t rc = NSM_SUCCESS;

    // Increment a few times without exceeding thresholds
    uint32_t incrementCount = SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD / 2;
    for (uint32_t i = 0; i < incrementCount; i++)
    {
        progressCounters.increment(ProgressCounterType::Priority, rc,
                                   startTime + i * 1000);
    }

    // Counters should NOT be reset
    EXPECT_EQ(progressCounters.totalCount, incrementCount);
    EXPECT_EQ(progressCounters.dumpIteration, 0);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        incrementCount);
}

// Test all ProgressCounterType enum values
TEST_F(ProgressCountersTest, AllCounterTypes)
{
    uint64_t currentTime = 10000000;
    uint8_t rc = NSM_SUCCESS;

    // Test each counter type (except EnumCount which is just for sizing)
    progressCounters.increment(ProgressCounterType::Priority, rc, currentTime);
    progressCounters.increment(ProgressCounterType::GpuPerformanceMonitoring,
                               rc, currentTime + 1000);
    progressCounters.increment(ProgressCounterType::LongRunning, rc,
                               currentTime + 2000);
    progressCounters.increment(ProgressCounterType::Static, rc,
                               currentTime + 3000);
    progressCounters.increment(ProgressCounterType::RoundRobin, rc,
                               currentTime + 4000);
    progressCounters.increment(ProgressCounterType::PriorityTimeExceeded, rc,
                               currentTime + 5000);
    progressCounters.increment(ProgressCounterType::PostPatch, rc,
                               currentTime + 6000);
    progressCounters.increment(ProgressCounterType::Event, rc,
                               currentTime + 7000);
    progressCounters.increment(ProgressCounterType::Error, rc,
                               currentTime + 8000);
    progressCounters.increment(ProgressCounterType::Timeout, rc,
                               currentTime + 9000);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        1);
    EXPECT_EQ(progressCounters.counters[static_cast<uint32_t>(
                  ProgressCounterType::GpuPerformanceMonitoring)],
              1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::LongRunning)],
        1);
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Static)],
              1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        1);
    EXPECT_EQ(progressCounters.counters[static_cast<uint32_t>(
                  ProgressCounterType::PriorityTimeExceeded)],
              1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::PostPatch)],
        1);
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Event)],
              1);
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Error)],
              1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Timeout)],
        1);
    EXPECT_EQ(progressCounters.totalCount, 10);
}

// Test all PollingType enum values
TEST_F(ProgressCountersTest, AllPollingTypes)
{
    uint64_t currentTime = 11000000;
    uint8_t rc = NSM_SUCCESS;

    // Test each polling type
    progressCounters.increment(PollingType::Priority, rc, currentTime);
    progressCounters.increment(PollingType::GpuPerformanceMonitoring, rc,
                               currentTime + 1000);
    progressCounters.increment(PollingType::LongRunning, rc,
                               currentTime + 2000);
    progressCounters.increment(PollingType::Static, rc, currentTime + 3000);
    progressCounters.increment(PollingType::RoundRobin, rc, currentTime + 4000);

    // PollingType enums map directly to ProgressCounterType enums
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        1);
    EXPECT_EQ(progressCounters.counters[static_cast<uint32_t>(
                  ProgressCounterType::GpuPerformanceMonitoring)],
              1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::LongRunning)],
        1);
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Static)],
              1);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        1);
    EXPECT_EQ(progressCounters.totalCount, 5);
}

// Test multiple dump iterations
TEST_F(ProgressCountersTest, MultipleDumpIterations)
{
    uint64_t currentTime = 12000000;
    uint8_t rc = NSM_SUCCESS;

    // First batch - exceed count threshold
    for (uint32_t i = 0; i <= SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD;
         i++)
    {
        progressCounters.increment(ProgressCounterType::Priority, rc,
                                   currentTime + i * 1000);
    }

    EXPECT_EQ(progressCounters.dumpIteration, 1);
    EXPECT_EQ(progressCounters.totalCount, 0);

    // Second batch - exceed count threshold again
    for (uint32_t i = 0; i <= SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD;
         i++)
    {
        progressCounters.increment(ProgressCounterType::RoundRobin, rc,
                                   currentTime + 100000 + i * 1000);
    }

    EXPECT_EQ(progressCounters.dumpIteration, 2);
    EXPECT_EQ(progressCounters.totalCount, 0);

    // Third batch - exceed count threshold again
    for (uint32_t i = 0; i <= SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD;
         i++)
    {
        progressCounters.increment(ProgressCounterType::Event, rc,
                                   currentTime + 200000 + i * 1000);
    }

    EXPECT_EQ(progressCounters.dumpIteration, 3);
    EXPECT_EQ(progressCounters.totalCount, 0);
}

// Test mixed success, timeout and error return codes
TEST_F(ProgressCountersTest, MixedReturnCodes)
{
    uint64_t currentTime = 13000000;

    // Increment with different return codes
    progressCounters.increment(ProgressCounterType::Priority, NSM_SUCCESS,
                               currentTime);
    progressCounters.increment(ProgressCounterType::Priority, NSM_SUCCESS,
                               currentTime + 1000);
    progressCounters.increment(ProgressCounterType::RoundRobin,
                               NSM_SW_ERROR_TIMEOUT, currentTime + 2000);
    progressCounters.increment(ProgressCounterType::Static, NSM_ERROR,
                               currentTime + 3000);
    progressCounters.increment(ProgressCounterType::Event, 0x10,
                               currentTime + 4000); // Some other error code

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        2);
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        0); // Should not increment due to timeout
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Static)],
              0); // Should not increment due to error
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Event)],
              0); // Should not increment due to error
    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Timeout)],
        1); // Should increment once for timeout
    EXPECT_EQ(progressCounters
                  .counters[static_cast<uint32_t>(ProgressCounterType::Error)],
              2); // Should increment twice for errors
    EXPECT_EQ(progressCounters.totalCount, 5);
}

#else

// If NVIDIA_PROGRESS_COUNTER is not defined, test that increment does nothing
TEST_F(ProgressCountersTest, IncrementDisabledFeature)
{
    uint64_t currentTime = 1000000;
    uint8_t rc = NSM_SUCCESS;

    // Try to increment
    progressCounters.increment(ProgressCounterType::Priority, rc, currentTime);

    // Nothing should change
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.startTime, 0);
    EXPECT_EQ(progressCounters.lastUpdateTime, 0);

    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

#endif // NVIDIA_PROGRESS_COUNTER
