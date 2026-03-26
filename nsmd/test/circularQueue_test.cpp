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

#include <gtest/gtest.h>

#define private public
#define protected public

#include "circularQueue.hpp"

using namespace nsm;

struct CircularQueueTest : public ::testing::Test
{};

TEST_F(CircularQueueTest, goodTestConstructor)
{
    CircularQueue<int> queue;
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);
}

TEST_F(CircularQueueTest, goodTestPush)
{
    CircularQueue<int> queue;

    queue.push(1);
    EXPECT_EQ(queue.size(), 1);
    EXPECT_FALSE(queue.empty());

    queue.push(2);
    queue.push(3);
    EXPECT_EQ(queue.size(), 3);
}

TEST_F(CircularQueueTest, goodTestPushMove)
{
    CircularQueue<std::string> queue;

    std::string str1 = "test1";
    queue.push(std::move(str1));
    EXPECT_EQ(queue.size(), 1);

    queue.push(std::string("test2"));
    EXPECT_EQ(queue.size(), 2);
}

TEST_F(CircularQueueTest, goodTestCurrent)
{
    CircularQueue<int> queue;
    queue.push(10);
    queue.push(20);
    queue.push(30);

    EXPECT_EQ(queue.current(), 10);
    queue.next();
    EXPECT_EQ(queue.current(), 20);
    queue.next();
    EXPECT_EQ(queue.current(), 30);
}

TEST_F(CircularQueueTest, goodTestNext)
{
    CircularQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    EXPECT_EQ(queue.current(), 1);
    queue.next();
    EXPECT_EQ(queue.current(), 2);
    queue.next();
    EXPECT_EQ(queue.current(), 3);
    queue.next(); // Should wrap around
    EXPECT_EQ(queue.current(), 1);
}

TEST_F(CircularQueueTest, goodTestCircularBehavior)
{
    CircularQueue<int> queue;
    queue.push(100);
    queue.push(200);

    EXPECT_EQ(queue.current(), 100);
    queue.next();
    EXPECT_EQ(queue.current(), 200);
    queue.next(); // Wrap around
    EXPECT_EQ(queue.current(), 100);
    queue.next();
    EXPECT_EQ(queue.current(), 200);
}

TEST_F(CircularQueueTest, badTestCurrentOnEmpty)
{
    CircularQueue<int> queue;
    EXPECT_THROW(queue.current(), std::runtime_error);
}

TEST_F(CircularQueueTest, goodTestNextOnEmpty)
{
    CircularQueue<int> queue;
    // Should not throw, just set index to 0
    queue.next();
    EXPECT_EQ(queue.currentIndex, 0);
}

TEST_F(CircularQueueTest, goodTestSingleElement)
{
    CircularQueue<int> queue;
    queue.push(42);

    EXPECT_EQ(queue.current(), 42);
    queue.next();
    EXPECT_EQ(queue.current(), 42); // Should stay at the same element
    queue.next();
    EXPECT_EQ(queue.current(), 42);
}

TEST_F(CircularQueueTest, goodTestIterators)
{
    CircularQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    int sum = 0;
    for (auto it = queue.begin(); it != queue.end(); ++it)
    {
        sum += *it;
    }
    EXPECT_EQ(sum, 6);

    // Test const iterators
    int constSum = 0;
    for (auto it = queue.cbegin(); it != queue.cend(); ++it)
    {
        constSum += *it;
    }
    EXPECT_EQ(constSum, 6);
}

TEST_F(CircularQueueTest, goodTestRangeBasedFor)
{
    CircularQueue<int> queue;
    queue.push(10);
    queue.push(20);
    queue.push(30);

    int sum = 0;
    for (const auto& value : queue)
    {
        sum += value;
    }
    EXPECT_EQ(sum, 60);
}

TEST_F(CircularQueueTest, goodTestCurrentAfterSizeChange)
{
    CircularQueue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);

    queue.next();
    queue.next(); // currentIndex = 2
    EXPECT_EQ(queue.current(), 3);

    // Manually simulate size reduction (as if elements were removed)
    queue.currentIndex = 5; // Simulate invalid index

    // Should reset to 0 when current() is called
    EXPECT_EQ(queue.current(), 1);
    EXPECT_EQ(queue.currentIndex, 0);
}

TEST_F(CircularQueueTest, goodTestStringQueue)
{
    CircularQueue<std::string> queue;
    queue.push("first");
    queue.push("second");
    queue.push("third");

    EXPECT_EQ(queue.current(), "first");
    queue.next();
    EXPECT_EQ(queue.current(), "second");
    queue.next();
    EXPECT_EQ(queue.current(), "third");
    queue.next();
    EXPECT_EQ(queue.current(), "first");
}

// Cover currentIndex >= size() TRUE branch for CircularQueue<std::string>
// (same path as goodTestCurrentAfterSizeChange but for the string
// instantiation)
TEST_F(CircularQueueTest, goodTestCurrentAfterSizeChange_String)
{
    CircularQueue<std::string> queue;
    queue.push("alpha");
    queue.push("beta");
    queue.currentIndex = 10; // Force invalid index

    // current() should detect currentIndex >= size(), reset to 0, return
    // "alpha"
    EXPECT_EQ(queue.current(), "alpha");
    EXPECT_EQ(queue.currentIndex, 0u);
}

// Cover next() empty-queue path for CircularQueue<std::string>
TEST_F(CircularQueueTest, goodTestNextOnEmpty_String)
{
    CircularQueue<std::string> queue;
    queue.next();
    EXPECT_EQ(queue.currentIndex, 0u);
}

// Exercise CircularQueue<double> to add a third template instantiation,
// covering the push/current/next branches for that type.
TEST_F(CircularQueueTest, goodTestDoubleQueue_WrapAround)
{
    CircularQueue<double> queue;
    queue.push(1.1);
    queue.push(2.2);
    queue.push(3.3);

    EXPECT_DOUBLE_EQ(queue.current(), 1.1);
    queue.next();
    EXPECT_DOUBLE_EQ(queue.current(), 2.2);
    queue.next();
    EXPECT_DOUBLE_EQ(queue.current(), 3.3);
    queue.next(); // wrap
    EXPECT_DOUBLE_EQ(queue.current(), 1.1);
}

// Cover currentIndex >= size() TRUE path for CircularQueue<double>
TEST_F(CircularQueueTest, goodTestCurrentAfterSizeChange_Double)
{
    CircularQueue<double> queue;
    queue.push(9.9);
    queue.currentIndex = 99; // Force invalid index
    EXPECT_DOUBLE_EQ(queue.current(), 9.9);
    EXPECT_EQ(queue.currentIndex, 0u);
}

// Cover push() rvalue overload for int (ensures move-path branch is taken)
TEST_F(CircularQueueTest, goodTestPushRvalue_Int)
{
    CircularQueue<int> queue;
    int val = 42;
    queue.push(std::move(val));
    EXPECT_EQ(queue.current(), 42);
}

// Cover line 48: throw path for empty CircularQueue<std::string> //

TEST_F(CircularQueueTest, badTestCurrentOnEmpty_String)
{
    CircularQueue<std::string> queue;
    EXPECT_THROW(queue.current(), std::runtime_error);
}

// Cover line 48: throw path for empty CircularQueue<double> //

TEST_F(CircularQueueTest, badTestCurrentOnEmpty_Double)
{
    CircularQueue<double> queue;
    EXPECT_THROW(queue.current(), std::runtime_error);
}

// Cover line 68: next() empty path for CircularQueue<double>
TEST_F(CircularQueueTest, goodTestNextOnEmpty_Double)
{
    CircularQueue<double> queue;
    queue.next();
    EXPECT_EQ(queue.currentIndex, 0u);
}
