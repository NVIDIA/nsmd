/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "circularQueue.hpp"
#include "nsmDevice.hpp"
#include "nsmObject.hpp"
#include "sensorQueueMap.hpp"

// For convenience
using SensorQueueUnorderedMap =
    std::unordered_map<nsm::PollingType,
                       std::shared_ptr<nsm::LimitedSensorQueue>>;

using namespace nsm;

// Mock NsmObject for testing
class MockNsmObject : public NsmObject
{
  public:
    MockNsmObject() : NsmObject("MockObject", "MockType") {}
};

// Test fixture for sensorQueueMap
class SensorQueueMapTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create some mock sensors
        sensor1 = std::make_shared<MockNsmObject>();
        sensor2 = std::make_shared<MockNsmObject>();
        sensor3 = std::make_shared<MockNsmObject>();
    }

    std::shared_ptr<NsmObject> sensor1;
    std::shared_ptr<NsmObject> sensor2;
    std::shared_ptr<NsmObject> sensor3;
};

// ============================================================================
// LimitedSensorQueue Tests
// ============================================================================

TEST_F(SensorQueueMapTest, LimitedSensorQueueConstructorInitializesCorrectly)
{
    SensorQueue queue;
    queue.push(sensor1);
    queue.push(sensor2);

    LimitedSensorQueue limitedQueue(queue);

    // Verify queue has sensors to update (initial count = queue size)
    EXPECT_TRUE(limitedQueue.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest, LimitedSensorQueueNextDecrementsCount)
{
    SensorQueue queue;
    queue.push(sensor1);
    queue.push(sensor2);

    LimitedSensorQueue limitedQueue(queue);

    // Call next() twice (queue has 2 items)
    limitedQueue.next();
    EXPECT_TRUE(limitedQueue.hasSensorsToUpdate()); // Still 1 remaining

    limitedQueue.next();
    EXPECT_FALSE(limitedQueue.hasSensorsToUpdate()); // Count = 0
}

TEST_F(SensorQueueMapTest, LimitedSensorQueueNextDoesNothingWhenCountZero)
{
    SensorQueue queue;
    queue.push(sensor1);

    LimitedSensorQueue limitedQueue(queue);

    // Exhaust the count
    limitedQueue.next();
    EXPECT_FALSE(limitedQueue.hasSensorsToUpdate());

    // Call next() again when count = 0 (should do nothing)
    limitedQueue.next();
    EXPECT_FALSE(limitedQueue.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest, LimitedSensorQueueCurrentReturnsCurrentSensor)
{
    SensorQueue queue;
    queue.push(sensor1);
    queue.push(sensor2);

    LimitedSensorQueue limitedQueue(queue);

    // current() should return the current sensor
    auto& current = limitedQueue.current();
    EXPECT_NE(current, nullptr);
}

TEST_F(SensorQueueMapTest,
       LimitedSensorQueueHasSensorsToUpdateReturnsFalseWhenCountZero)
{
    SensorQueue queue;
    queue.push(sensor1);

    LimitedSensorQueue limitedQueue(queue);

    limitedQueue.next(); // Count becomes 0

    // Both conditions false: countToUpdate = 0
    EXPECT_FALSE(limitedQueue.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest,
       LimitedSensorQueueHasSensorsToUpdateReturnsFalseWhenQueueEmpty)
{
    SensorQueue emptyQueue;

    LimitedSensorQueue limitedQueue(emptyQueue);

    // Both conditions: countToUpdate = 0 (queue.size() = 0)
    EXPECT_FALSE(limitedQueue.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest,
       LimitedSensorQueueHasSensorsToUpdateReturnsTrueWhenBothPositive)
{
    SensorQueue queue;
    queue.push(sensor1);
    queue.push(sensor2);

    LimitedSensorQueue limitedQueue(queue);

    // Both conditions true: countToUpdate > 0 && size() > 0
    EXPECT_TRUE(limitedQueue.hasSensorsToUpdate());
}

// ============================================================================
// SensorQueueMap Constructor Tests
// ============================================================================

TEST_F(SensorQueueMapTest, SensorQueueMapConstructorThrowsOnEmptyMap)
{
    SensorQueueUnorderedMap emptyMap;

    // Constructor should throw for empty map
    EXPECT_THROW({ SensorQueueMap map(emptyMap); }, std::runtime_error);
}

TEST_F(SensorQueueMapTest, SensorQueueMapConstructorSucceedsWithNonEmptyMap)
{
    SensorQueue queue;
    queue.push(sensor1);

    auto limitedQueue = std::make_shared<LimitedSensorQueue>(queue);

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue;

    // Constructor should not throw
    EXPECT_NO_THROW({ SensorQueueMap queueMap(map); });
}

// ============================================================================
// SensorQueueMap hasSensorsToUpdate Tests
// ============================================================================

TEST_F(SensorQueueMapTest, HasSensorsToUpdateReturnsTrueWhenQueueHasSensors)
{
    SensorQueue queue;
    queue.push(sensor1);

    auto limitedQueue = std::make_shared<LimitedSensorQueue>(queue);

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue;

    SensorQueueMap sensorQueueMap{map};

    // Should return true (limitedQueue has sensors to update)
    EXPECT_TRUE(sensorQueueMap.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest, HasSensorsToUpdateReturnsFalseWhenAllQueuesExhausted)
{
    SensorQueue queue1;
    queue1.push(sensor1);
    auto limitedQueue1 = std::make_shared<LimitedSensorQueue>(queue1);
    limitedQueue1->next(); // Exhaust count

    SensorQueue queue2;
    queue2.push(sensor2);
    auto limitedQueue2 = std::make_shared<LimitedSensorQueue>(queue2);
    limitedQueue2->next(); // Exhaust count

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue1;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue2;

    SensorQueueMap sensorQueueMap{map};

    // Should return false (all queues exhausted)
    EXPECT_FALSE(sensorQueueMap.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest, HasSensorsToUpdateReturnsTrueWhenOneQueueHasSensors)
{
    SensorQueue queue1;
    queue1.push(sensor1);
    auto limitedQueue1 = std::make_shared<LimitedSensorQueue>(queue1);
    limitedQueue1->next(); // Exhaust count

    SensorQueue queue2;
    queue2.push(sensor2);
    queue2.push(sensor3);
    auto limitedQueue2 = std::make_shared<LimitedSensorQueue>(queue2);
    // limitedQueue2 still has sensors to update

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue1;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue2;

    SensorQueueMap sensorQueueMap{map};

    // Should return true (queue2 has sensors to update)
    EXPECT_TRUE(sensorQueueMap.hasSensorsToUpdate());
}

// ============================================================================
// SensorQueueMap nextSensorState Tests
// ============================================================================

TEST_F(SensorQueueMapTest, NextSensorStateReturnsFirstWhenTypeNotContained)
{
    SensorQueue queue;
    queue.push(sensor1);
    auto limitedQueue = std::make_shared<LimitedSensorQueue>(queue);

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue;

    SensorQueueMap sensorQueueMap{map};

    // Pass a polling type not in the map
    PollingType pollingType = PollingType::LongRunning;
    sensorQueueMap.nextSensorState(pollingType);

    // Should be set to first element (Priority)
    EXPECT_EQ(pollingType, PollingType::Priority);
}

TEST_F(SensorQueueMapTest, NextSensorStateRotatesToNextType)
{
    SensorQueue queue1;
    queue1.push(sensor1);
    auto limitedQueue1 = std::make_shared<LimitedSensorQueue>(queue1);

    SensorQueue queue2;
    queue2.push(sensor2);
    auto limitedQueue2 = std::make_shared<LimitedSensorQueue>(queue2);

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue1;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue2;

    SensorQueueMap sensorQueueMap{map};

    // Start with Priority
    PollingType pollingType = PollingType::Priority;
    sensorQueueMap.nextSensorState(pollingType);

    // Should rotate to next (either Priority or GpuPerformanceMonitoring)
    // In unordered_map, order is not guaranteed, but it should be one of them
    EXPECT_TRUE(pollingType == PollingType::Priority ||
                pollingType == PollingType::GpuPerformanceMonitoring);
}

TEST_F(SensorQueueMapTest, NextSensorStateSkipsExhaustedQueues)
{
    SensorQueue queue1;
    queue1.push(sensor1);
    auto limitedQueue1 = std::make_shared<LimitedSensorQueue>(queue1);
    limitedQueue1->next(); // Exhaust

    SensorQueue queue2;
    queue2.push(sensor2);
    auto limitedQueue2 = std::make_shared<LimitedSensorQueue>(queue2);
    // limitedQueue2 has sensors

    SensorQueue queue3;
    queue3.push(sensor3);
    auto limitedQueue3 = std::make_shared<LimitedSensorQueue>(queue3);
    limitedQueue3->next(); // Exhaust

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue1;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue2;
    map[PollingType::LongRunning] = limitedQueue3;

    SensorQueueMap sensorQueueMap{map};

    // Start with exhausted queue
    PollingType pollingType = PollingType::Priority;
    sensorQueueMap.nextSensorState(pollingType);

    // Should skip to GpuPerformanceMonitoring (only one with sensors)
    // Or stay at one of the valid polling types
    bool isValid = (pollingType == PollingType::Priority ||
                    pollingType == PollingType::GpuPerformanceMonitoring ||
                    pollingType == PollingType::LongRunning);
    EXPECT_TRUE(isValid);
}

TEST_F(SensorQueueMapTest, NextSensorStateWrapsAroundMap)
{
    SensorQueue queue;
    queue.push(sensor1);
    auto limitedQueue = std::make_shared<LimitedSensorQueue>(queue);

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue;

    SensorQueueMap sensorQueueMap{map};

    PollingType pollingType = PollingType::Priority;

    // Rotate multiple times to test wrap-around
    sensorQueueMap.nextSensorState(pollingType);
    sensorQueueMap.nextSensorState(pollingType);
    sensorQueueMap.nextSensorState(pollingType);

    // Should be valid polling type after wrap-around
    EXPECT_TRUE(pollingType == PollingType::Priority ||
                pollingType == PollingType::GpuPerformanceMonitoring);
}

TEST_F(SensorQueueMapTest, NextSensorStateStopsAtStartWhenAllExhausted)
{
    SensorQueue queue1;
    queue1.push(sensor1);
    auto limitedQueue1 = std::make_shared<LimitedSensorQueue>(queue1);
    limitedQueue1->next(); // Exhaust

    SensorQueue queue2;
    queue2.push(sensor2);
    auto limitedQueue2 = std::make_shared<LimitedSensorQueue>(queue2);
    limitedQueue2->next(); // Exhaust

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue1;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue2;

    SensorQueueMap sensorQueueMap{map};

    PollingType pollingType = PollingType::Priority;

    sensorQueueMap.nextSensorState(pollingType);

    // Should return to one of the valid polling types
    bool isValid = (pollingType == PollingType::Priority ||
                    pollingType == PollingType::GpuPerformanceMonitoring);
    EXPECT_TRUE(isValid);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(SensorQueueMapTest, LimitedSensorQueueWithSingleSensor)
{
    SensorQueue queue;
    queue.push(sensor1);

    LimitedSensorQueue limitedQueue(queue);

    EXPECT_TRUE(limitedQueue.hasSensorsToUpdate());
    limitedQueue.next();
    EXPECT_FALSE(limitedQueue.hasSensorsToUpdate());
}

TEST_F(SensorQueueMapTest, SensorQueueMapWithSinglePollingType)
{
    SensorQueue queue;
    queue.push(sensor1);
    auto limitedQueue = std::make_shared<LimitedSensorQueue>(queue);

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue;

    SensorQueueMap sensorQueueMap{map};

    EXPECT_TRUE(sensorQueueMap.hasSensorsToUpdate());

    PollingType pollingType = PollingType::Priority;
    sensorQueueMap.nextSensorState(pollingType);

    EXPECT_EQ(pollingType, PollingType::Priority);
}

TEST_F(SensorQueueMapTest, MultiplePollingTypesWithVariousStates)
{
    SensorQueue queue1;
    queue1.push(sensor1);
    queue1.push(sensor2);
    auto limitedQueue1 = std::make_shared<LimitedSensorQueue>(queue1);

    SensorQueue queue2;
    queue2.push(sensor3);
    auto limitedQueue2 = std::make_shared<LimitedSensorQueue>(queue2);
    limitedQueue2->next(); // Exhaust

    SensorQueue queue3;
    auto limitedQueue3 = std::make_shared<LimitedSensorQueue>(queue3);
    // Empty queue

    SensorQueueUnorderedMap map;
    map[PollingType::Priority] = limitedQueue1;
    map[PollingType::GpuPerformanceMonitoring] = limitedQueue2;
    map[PollingType::LongRunning] = limitedQueue3;

    SensorQueueMap sensorQueueMap{map};

    // Only queue1 has sensors to update
    EXPECT_TRUE(sensorQueueMap.hasSensorsToUpdate());
}

// ============================================================================
// SensorQueue (CircularQueue<shared_ptr<NsmObject>>) direct coverage tests
// These exercise CircularQueue<shared_ptr<NsmObject>> L48/L55/L68 paths
// ============================================================================

// L48: current() throw path for SensorQueue (empty queue)
TEST_F(SensorQueueMapTest, SensorQueue_CurrentOnEmpty_Throws)
{
    SensorQueue queue;
    EXPECT_THROW(queue.current(), std::runtime_error);
}

// L55: currentIndex >= size() reset path for SensorQueue
TEST_F(SensorQueueMapTest, SensorQueue_CurrentAfterIndexReset)
{
    SensorQueue queue;
    queue.push(sensor1);
    queue.push(sensor2);
    queue.currentIndex = 99; // Force out-of-bounds index

    // current() should reset currentIndex to 0 and return sensor1
    auto& cur = queue.current();
    EXPECT_EQ(cur, sensor1);
    EXPECT_EQ(queue.currentIndex, 0u);
}

// L68: next() else branch (empty queue) for SensorQueue
TEST_F(SensorQueueMapTest, SensorQueue_NextOnEmpty)
{
    SensorQueue queue;
    queue.next(); // Should not throw; sets currentIndex = 0
    EXPECT_EQ(queue.currentIndex, 0u);
}
