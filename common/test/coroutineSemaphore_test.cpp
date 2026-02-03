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

#include "coroutineSemaphore.hpp"

#include <gtest/gtest.h>

using namespace common;

struct CoroutineSemaphoreTest : public ::testing::Test
{
    CoroutineSemaphore semaphore;
};

TEST_F(CoroutineSemaphoreTest, testConstructor)
{
    CoroutineSemaphore sem;
    EXPECT_NO_THROW({ [[maybe_unused]] auto awaiter = sem.acquire(1); });
}

TEST_F(CoroutineSemaphoreTest, testAcquire)
{
    int eid = 10;
    auto awaiter = semaphore.acquire(eid);

    EXPECT_EQ(awaiter.eid, eid);
    EXPECT_EQ(&awaiter.semaphore, &semaphore);
}

TEST_F(CoroutineSemaphoreTest, testMultipleAcquire)
{
    auto awaiter1 = semaphore.acquire(1);
    auto awaiter2 = semaphore.acquire(2);

    EXPECT_EQ(awaiter1.eid, 1);
    EXPECT_EQ(awaiter2.eid, 2);
}

TEST_F(CoroutineSemaphoreTest, testRelease)
{
    [[maybe_unused]] auto awaiter = semaphore.acquire(1);
    EXPECT_NO_THROW(semaphore.release());
}

TEST_F(CoroutineSemaphoreTest, testAcquireReleaseCycle)
{
    // Acquire
    [[maybe_unused]] auto awaiter1 = semaphore.acquire(1);

    // Release
    semaphore.release();

    // Acquire again
    auto awaiter2 = semaphore.acquire(2);

    EXPECT_EQ(awaiter2.eid, 2);
}

TEST_F(CoroutineSemaphoreTest, testAwaiterConstructor)
{
    int testEid = 42;
    int testId = 123;

    CoroutineSemaphore::Awaiter awaiter(semaphore, testEid, testId);

    EXPECT_EQ(awaiter.eid, testEid);
    EXPECT_EQ(awaiter.awaiterId, testId);
    EXPECT_EQ(&awaiter.semaphore, &semaphore);
}

TEST_F(CoroutineSemaphoreTest, testMultipleReleases)
{
    semaphore.release();
    semaphore.release();

    auto awaiter = semaphore.acquire(1);
    EXPECT_EQ(awaiter.eid, 1);
}
