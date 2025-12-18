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

#include "counterProducer.hpp"
#include "nsmDevice.hpp"
#include "progressCounterType.hpp"
#include "progressCounters.hpp"

using namespace ::testing;
using namespace nsm;

class ProgressCountersTest : public Test
{
  protected:
    ProgressCounters progressCounters{1};
};

// Test constructor
TEST_F(ProgressCountersTest, Constructor)
{
    // Verify initial state
    EXPECT_EQ(progressCounters.dumpIteration, 0);
    EXPECT_EQ(progressCounters.totalCount, 0);
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
    uint8_t rc = NSM_SUCCESS;

    // Test Priority polling
    progressCounters.increment(PollingType::Priority, rc);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::Priority)],
        1);
    EXPECT_EQ(progressCounters.totalCount, 1);
    EXPECT_EQ(progressCounters.lastUpdateTime, 0); // Not set until flush/update
}

// Test increment with PollingType - GpuPerformanceMonitoring
TEST_F(ProgressCountersTest, IncrementWithPollingTypeGPM)
{
    uint8_t rc = NSM_SUCCESS;

    progressCounters.increment(PollingType::GpuPerformanceMonitoring, rc);

    EXPECT_EQ(progressCounters.counters[static_cast<uint32_t>(
                  ProgressCounterType::GpuPerformanceMonitoring)],
              1);
    EXPECT_EQ(progressCounters.totalCount, 1);
}

// Test increment with ProgressCounterType and NSM_SUCCESS
TEST_F(ProgressCountersTest, IncrementWithProgressCounterTypeSuccess)
{
    uint8_t rc = NSM_SUCCESS;

    progressCounters.increment(ProgressCounterType::RoundRobin, rc);

    EXPECT_EQ(
        progressCounters
            .counters[static_cast<uint32_t>(ProgressCounterType::RoundRobin)],
        1);
    EXPECT_EQ(progressCounters.totalCount, 1);
    EXPECT_EQ(progressCounters.lastUpdateTime, 0); // Not set until flush/update
}

// Test increment with ProgressCounterType and NSM_SW_ERROR_TIMEOUT
TEST_F(ProgressCountersTest, IncrementWithProgressCounterTypeTimeout)
{
    uint8_t rc = NSM_SW_ERROR_TIMEOUT;

    // Try to increment RoundRobin, but it should increment Timeout instead
    progressCounters.increment(ProgressCounterType::RoundRobin, rc);

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
    uint8_t rc = NSM_ERROR; // Any error code other than SUCCESS or TIMEOUT

    // Try to increment Static, but it should increment Error instead
    progressCounters.increment(ProgressCounterType::Static, rc);

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
    uint8_t rc = NSM_SUCCESS;

    // Increment different counter types
    progressCounters.increment(ProgressCounterType::Priority, rc);
    progressCounters.increment(ProgressCounterType::Priority, rc);
    progressCounters.increment(ProgressCounterType::RoundRobin, rc);
    progressCounters.increment(ProgressCounterType::Static, rc);

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
    EXPECT_EQ(progressCounters.lastUpdateTime, 0); // Not set until flush/update
}

// Test updateCounters
TEST_F(ProgressCountersTest, UpdateCounters)
{
    uint64_t currentTime = 6000000;
    uint8_t rc = NSM_SUCCESS;

    // Add some counters
    progressCounters.increment(ProgressCounterType::Priority, rc);
    progressCounters.increment(ProgressCounterType::RoundRobin, rc);

    EXPECT_EQ(progressCounters.totalCount, 2);
    EXPECT_EQ(progressCounters.dumpIteration, 0);

    // Manually call updateCounters with time parameter
    progressCounters.updateCounters(currentTime);

    // After update, counters should be reset
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.lastUpdateTime, currentTime);
    EXPECT_EQ(progressCounters.dumpIteration, 1);

    // All counters should be zero
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

// Test flushIfNeeded - count threshold exceeded
TEST_F(ProgressCountersTest, flushIfNeededCountExceeded)
{
    uint8_t rc = NSM_SUCCESS;

    // Increment counters until threshold is reached (threshold uses >=)
    // Auto-flush happens when totalCount >= countThreshold
    for (uint32_t i = 0; i < SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD; i++)
    {
        progressCounters.increment(ProgressCounterType::Priority, rc);
    }

    // After reaching threshold, counters should be auto-reset by increment()
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.dumpIteration, 1);

    // All counters should be zero after dump
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

// Test flushIfNeeded - time threshold exceeded
TEST_F(ProgressCountersTest, flushIfNeededTimeExceeded)
{
    uint64_t lastUpdateTime = 8000000;
    uint8_t rc = NSM_SUCCESS;

    // Increment some counters
    progressCounters.increment(ProgressCounterType::Priority, rc);
    progressCounters.increment(ProgressCounterType::Priority, rc);

    EXPECT_EQ(progressCounters.totalCount, 2);

    // Manually flush to set lastUpdateTime
    progressCounters.updateCounters(lastUpdateTime);
    EXPECT_EQ(progressCounters.dumpIteration, 1);
    EXPECT_EQ(progressCounters.lastUpdateTime, lastUpdateTime);
    EXPECT_EQ(progressCounters.totalCount, 0);

    // Add more counters
    progressCounters.increment(ProgressCounterType::Priority, rc);
    EXPECT_EQ(progressCounters.totalCount, 1);

    // Now flush with time that exceeds threshold
    uint64_t endTime = lastUpdateTime +
                       SENSOR_PROGRESS_COUNTERS_DUMP_TIME_THRESHOLD;
    bool flushed = progressCounters.flushIfNeeded(endTime);
    EXPECT_TRUE(flushed);

    // After exceeding time threshold, counters should be reset
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.dumpIteration, 2);

    // All counters should be zero after dump
    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

// Test flushIfNeeded - neither threshold exceeded
TEST_F(ProgressCountersTest, flushIfNeededNoThreshold)
{
    uint64_t lastUpdateTime = 9000000;
    uint8_t rc = NSM_SUCCESS;

    // Increment a few times without exceeding thresholds
    uint32_t incrementCount = SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD / 2;
    for (uint32_t i = 0; i < incrementCount; i++)
    {
        progressCounters.increment(ProgressCounterType::Priority, rc);
    }

    // Manually call flushIfNeeded - should not flush
    bool flushed = progressCounters.flushIfNeeded(lastUpdateTime +
                                                  (incrementCount - 1) * 1000);
    EXPECT_FALSE(flushed);

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
    uint8_t rc = NSM_SUCCESS;

    // Test each counter type (except EnumCount which is just for sizing)
    progressCounters.increment(ProgressCounterType::Priority, rc);
    progressCounters.increment(ProgressCounterType::GpuPerformanceMonitoring,
                               rc);
    progressCounters.increment(ProgressCounterType::LongRunning, rc);
    progressCounters.increment(ProgressCounterType::Static, rc);
    progressCounters.increment(ProgressCounterType::RoundRobin, rc);
    progressCounters.increment(ProgressCounterType::PriorityTimeExceeded, rc);
    progressCounters.increment(ProgressCounterType::PostPatch, rc);
    progressCounters.increment(ProgressCounterType::Event, rc);
    progressCounters.increment(ProgressCounterType::Error, rc);
    progressCounters.increment(ProgressCounterType::Timeout, rc);

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
    uint8_t rc = NSM_SUCCESS;

    // Test each polling type
    progressCounters.increment(PollingType::Priority, rc);
    progressCounters.increment(PollingType::GpuPerformanceMonitoring, rc);
    progressCounters.increment(PollingType::LongRunning, rc);
    progressCounters.increment(PollingType::Static, rc);
    progressCounters.increment(PollingType::RoundRobin, rc);

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
    uint8_t rc = NSM_SUCCESS;

    // First batch - reach count threshold (auto-flush via increment())
    for (uint32_t i = 0; i < SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD; i++)
    {
        progressCounters.increment(ProgressCounterType::Priority, rc);
    }

    EXPECT_EQ(progressCounters.dumpIteration, 1);
    EXPECT_EQ(progressCounters.totalCount, 0);

    // Second batch - reach count threshold again (auto-flush via increment())
    for (uint32_t i = 0; i < SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD; i++)
    {
        progressCounters.increment(ProgressCounterType::RoundRobin, rc);
    }

    EXPECT_EQ(progressCounters.dumpIteration, 2);
    EXPECT_EQ(progressCounters.totalCount, 0);

    // Third batch - reach count threshold again (auto-flush via increment())
    for (uint32_t i = 0; i < SENSOR_PROGRESS_COUNTERS_DUMP_COUNT_THRESHOLD; i++)
    {
        progressCounters.increment(ProgressCounterType::Event, rc);
    }

    EXPECT_EQ(progressCounters.dumpIteration, 3);
    EXPECT_EQ(progressCounters.totalCount, 0);
}

// Test mixed success, timeout and error return codes
TEST_F(ProgressCountersTest, MixedReturnCodes)
{
    // Increment with different return codes
    progressCounters.increment(ProgressCounterType::Priority, NSM_SUCCESS);
    progressCounters.increment(ProgressCounterType::Priority, NSM_SUCCESS);
    progressCounters.increment(ProgressCounterType::RoundRobin,
                               NSM_SW_ERROR_TIMEOUT);
    progressCounters.increment(ProgressCounterType::Static, NSM_ERROR);
    progressCounters.increment(ProgressCounterType::Event,
                               0x10); // Some other error code

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
    uint8_t rc = NSM_SUCCESS;

    // Try to increment
    progressCounters.increment(ProgressCounterType::Priority, rc);

    // Nothing should change
    EXPECT_EQ(progressCounters.totalCount, 0);
    EXPECT_EQ(progressCounters.lastUpdateTime, 0);

    for (const auto& counter : progressCounters.counters)
    {
        EXPECT_EQ(counter, 0);
    }
}

#endif // NVIDIA_PROGRESS_COUNTER
