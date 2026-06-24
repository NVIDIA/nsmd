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

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "counterProducer.hpp"

using namespace ::testing;

class CountersTest : public Test
{};
using CountersArray = nsm::CountersArray<uint32_t, nsm::PollingCountersSize>;
using CounterDataRow = nsm::CountersDataRow<uint32_t, nsm::PollingCountersSize>;
using DeviceCounterDumpObject =
    nsm::DeviceCounterDumpObject<uint32_t, nsm::PollingCountersSize,
                                 SENSOR_PROGRESS_COUNTERS_MEMFD_SIZE>;

// Tests for CounterDataRow size
TEST_F(CountersTest, CounterDataRowSize)
{
    CountersArray counters{};
    counters[0] = 10;
    counters[1] = 20;
    counters[2] = 30;
    size_t rowSize = sizeof(CounterDataRow);

    // key (4 bytes) + timestamp (8 bytes) + 10 uint32_t counters (40 bytes) =
    // 52 bytes
    size_t expected = sizeof(uint32_t) + sizeof(uint64_t) +
                      sizeof(CountersArray);
    EXPECT_EQ(rowSize, expected);
}

// Test DeviceCounterDumpObject
TEST_F(CountersTest, DeviceCounterDumpObjectCreation)
{
    // Create a device counter dump object
    DeviceCounterDumpObject deviceData("/tmp/test_42");

    EXPECT_GT(deviceData.maxRows,
              0); // maxRows is now constexpr and should be > 0

    // Test that maxRows is consistent across instances
    DeviceCounterDumpObject deviceData2("/tmp/test_43");
    EXPECT_EQ(deviceData.maxRows, deviceData2.maxRows);
}

// Verify shm is accessible by name after construction
TEST_F(CountersTest, DeviceCounterDumpObjectShmAccessible)
{
    DeviceCounterDumpObject deviceData("/tmp/test_getfd");
    int fd = shm_open("/nsm_tmp_test_getfd", O_RDONLY, 0);
    EXPECT_GE(fd, 0);
    if (fd >= 0)
    {
        close(fd);
    }
}

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCounters)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray counters{};
    counters[0] = 10;
    counters[1] = 20;

    // First update should succeed
    bool result = deviceData.updateCounters({0, 1000, counters});
    EXPECT_TRUE(result);

    // maxRows should be calculated and > 0
    EXPECT_GT(deviceData.maxRows, 0);
}

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCountersMultipleIterations)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray counters{};
    counters[0] = 10;

    // Update counters for multiple iterations
    bool result1 = deviceData.updateCounters({0, 1000, counters});
    EXPECT_TRUE(result1);

    bool result2 = deviceData.updateCounters({1, 2000, counters});
    EXPECT_TRUE(result2);

    bool result3 = deviceData.updateCounters({2, 3000, counters});
    EXPECT_TRUE(result3);
}

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCountersFailureEmptyCounters)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray emptyCounters{}; // All zeros

    // Update with empty counters should fail (validation rejects all-zero
    // counters)
    bool result = deviceData.updateCounters({0, 1000, emptyCounters});

    EXPECT_FALSE(result);
}

TEST_F(CountersTest,
       DeviceCounterDumpObjectUpdateCountersFailureAllZeroCounters)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray allZeroCounters{};
    // Explicitly set all to zero to test std::ranges::all_of
    for (auto& counter : allZeroCounters)
    {
        counter = 0;
    }

    // Update with all zero counters should fail (validation rejects all-zero
    // counters)
    bool result = deviceData.updateCounters({0, 1000, allZeroCounters});

    EXPECT_FALSE(result);
}

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCountersLargeTimestamps)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray counters{};
    counters[0] = 10;

    // Test with large timestamp values
    uint64_t largelastUpdateTime = UINT64_MAX - 1000;

    bool result = deviceData.updateCounters({0, largelastUpdateTime, counters});

    EXPECT_TRUE(result);
}

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCountersLargeDumpIteration)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray counters{};
    counters[0] = 10;

    // First write to initialize the file
    bool initResult = deviceData.updateCounters({0, 500, counters});
    EXPECT_TRUE(initResult);

    // Test with large dump iteration (key)
    uint32_t largeIteration = deviceData.maxRows * 10;

    // This should succeed - the key will be modulo'd by maxRows
    bool result = deviceData.updateCounters({largeIteration, 1000, counters});

    EXPECT_TRUE(result);
}

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCountersManyCounters)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    // Create counters using all array elements
    CountersArray manyCounters{};
    for (int i = 0; i < 10; ++i)
    {
        manyCounters[i] = uint32_t(i);
    }

    bool result = deviceData.updateCounters({0, 1000, manyCounters});

    EXPECT_TRUE(result);
}

TEST_F(CountersTest, DeviceCounterDumpObjectRotationLogic)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");
    CountersArray counters{};
    counters[0] = 10;

    // Test rotation logic: key % maxRows
    for (uint32_t i = 0; i < 2 * deviceData.maxRows; i++)
    {
        bool result = deviceData.updateCounters({i, i * 1000, counters});
        EXPECT_TRUE(result) << "Failed at iteration " << i;
    }
}

TEST_F(CountersTest, DeviceCounterDumpObjectFileSizeAndDataIntegrity)
{
    DeviceCounterDumpObject deviceData("/tmp/test_1");

    // Prepare test data
    const uint32_t numIterations = 5;
    std::vector<CountersArray> testCounters(numIterations);
    std::vector<uint32_t> testKeys(numIterations);
    std::vector<uint64_t> testTimestamps(numIterations);

    // Generate test data
    for (uint32_t i = 0; i < numIterations; i++)
    {
        testKeys[i] = i;
        testTimestamps[i] = i * 1000;
        testCounters[i][0] = 10 + i; // Different counter values
        testCounters[i][1] = 20 + i;
        testCounters[i][2] = 30 + i;
    }

    // Update counters multiple times
    for (uint32_t i = 0; i < numIterations; i++)
    {
        bool result = deviceData.updateCounters(
            {testKeys[i], testTimestamps[i], testCounters[i]});
        EXPECT_TRUE(result) << "Failed to update counters at iteration " << i;
    }

    // Test file size using the file descriptor
    size_t fileSize = deviceData.fd.getFileSize();
    size_t expectedMinSize = numIterations * sizeof(CounterDataRow);
    EXPECT_GE(fileSize, expectedMinSize)
        << "File size too small. Expected at least " << expectedMinSize
        << " bytes, got " << fileSize;

    // Test data integrity by reading back the data
    uint32_t maxRows = deviceData.maxRows;
    for (uint32_t i = 0; i < numIterations; i++)
    {
        uint32_t expectedRow = testKeys[i] % maxRows;
        // Read the data from the calculated row using helper function
        CounterDataRow readData;
        deviceData.fd.read(expectedRow * sizeof(CounterDataRow),
                           reinterpret_cast<uint8_t*>(&readData),
                           sizeof(CounterDataRow));

        // Verify the data matches what we wrote
        EXPECT_EQ(readData.key, testKeys[i])
            << "Key mismatch at iteration " << i;
        EXPECT_EQ(readData.timestamp, testTimestamps[i])
            << "Timestamp mismatch at iteration " << i;

        // Verify counter values
        for (size_t j = 0; j < testCounters[i].size(); j++)
        {
            EXPECT_EQ(readData.counters[j], testCounters[i][j])
                << "Counter " << j << " mismatch at iteration " << i;
        }
    }

    // Test that rotation logic works correctly
    for (uint32_t i = 0; i < numIterations; i++)
    {
        uint32_t expectedRow = testKeys[i] % maxRows;
        EXPECT_LT(expectedRow, maxRows)
            << "Rotation logic failed for key " << testKeys[i];
    }
}

// updateCounters() write failure path: invalidate the fd after construction
// so that lseek() inside CustomFD::write() fails → fd.write() returns false
// → "if (!fd.write(...))" TRUE branch → throw → catch → return false. //

TEST_F(CountersTest, DeviceCounterDumpObjectUpdateCountersWriteFailure)
{
    DeviceCounterDumpObject deviceData("/tmp/test_write_fail");
    CountersArray counters{};
    counters[0] = 42; // non-zero, passes the all_of check

    // Invalidate the underlying file descriptor so that the write() will fail
    // (lseek on -1 returns EBADF → CustomFD::write() returns false).
    close(deviceData.fd.fd);
    deviceData.fd.fd = -1;

    bool result = deviceData.updateCounters({0, 1000, counters});
    EXPECT_FALSE(result);
}

TEST_F(CountersTest, DeviceCounterDumpObjectRotationWithLargeKeys)
{
    DeviceCounterDumpObject deviceData("/tmp/test_2");

    uint32_t maxRows = deviceData.maxRows;
    CountersArray testCounters{};
    testCounters[0] = 100;
    testCounters[1] = 200;
    testCounters[2] = 300;

    // Test with keys that are larger than maxRows to test rotation
    std::vector<uint32_t> testKeys = {
        0, 1, maxRows, maxRows + 1, maxRows * 2, maxRows * 2 + 1};
    std::vector<uint32_t> expectedRows = {0, 1, 0, 1,
                                          0, 1}; // Expected rows after rotation

    // Write data with large keys
    for (size_t i = 0; i < testKeys.size(); i++)
    {
        bool result =
            deviceData.updateCounters({testKeys[i], i * 1000, testCounters});
        EXPECT_TRUE(result)
            << "Failed to update counters for key " << testKeys[i];
    }

    // Verify file size using the file descriptor
    size_t fileSize = deviceData.fd.getFileSize();

    // Calculate expected file size based on rotation logic
    // Since we have keys that map to rows 0 and 1 only, we expect at least 2
    // rows
    size_t uniqueRows = 2; // Keys map to rows 0 and 1 only
    size_t expectedMinSize = uniqueRows * sizeof(CounterDataRow);

    EXPECT_GE(fileSize, expectedMinSize)
        << "File size too small after writing large keys. Expected at least "
        << expectedMinSize << " bytes, got " << fileSize;

    // Read back data and verify rotation worked correctly
    for (size_t i = 0; i < testKeys.size(); i++)
    {
        uint32_t expectedRow = testKeys[i] % maxRows;
        EXPECT_EQ(expectedRow, expectedRows[i])
            << "Rotation calculation wrong for key " << testKeys[i];

        CounterDataRow readData;
        deviceData.fd.read(expectedRow * sizeof(CounterDataRow),
                           reinterpret_cast<uint8_t*>(&readData),
                           sizeof(CounterDataRow));

        // Verify the data (note: due to rotation, some data might be
        // overwritten) We'll check that the last written data for each row is
        // correct
        if (expectedRow == 0) // Keys 0, maxRows, maxRows*2 all map to row 0
        {
            // The last key that maps to row 0 should be maxRows*2
            EXPECT_EQ(readData.key, maxRows * 2) << "Wrong key in row 0";
        }
        else if (expectedRow ==
                 1) // Keys 1, maxRows+1, maxRows*2+1 all map to row 1
        {
            // The last key that maps to row 1 should be maxRows*2+1
            EXPECT_EQ(readData.key, maxRows * 2 + 1) << "Wrong key in row 1";
        }
    }
}
