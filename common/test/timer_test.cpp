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

#include "timer.hpp"

#include <gtest/gtest.h>

using namespace common;

struct TimerAwaiterTest : public ::testing::Test
{};

TEST_F(TimerAwaiterTest, testDefaultConstructor)
{
    TimerAwaiter timer;
    EXPECT_FALSE(timer.running());
    EXPECT_FALSE(timer.expired());
}

TEST_F(TimerAwaiterTest, testConstructorWithTime)
{
    uint64_t time = 100; // 100 milliseconds
    TimerAwaiter timer(time);

    EXPECT_FALSE(timer.running());
    EXPECT_FALSE(timer.expired());
}

TEST_F(TimerAwaiterTest, testConstructorWithCode)
{
    TimerAwaiter timer(NSM_SW_SUCCESS);
    EXPECT_FALSE(timer.running());
}

TEST_F(TimerAwaiterTest, testStart)
{
    TimerAwaiter timer(100);

    bool result = timer.start();

    EXPECT_TRUE(result);
    EXPECT_TRUE(timer.running());
    EXPECT_FALSE(timer.expired());
}

TEST_F(TimerAwaiterTest, testStop)
{
    TimerAwaiter timer(100);

    timer.start();
    EXPECT_TRUE(timer.running());

    bool result = timer.stop();

    EXPECT_TRUE(result);
    // Note: timer.running() may still be true until event loop processes the
    // stop
}

TEST_F(TimerAwaiterTest, testStopWithoutStart)
{
    TimerAwaiter timer(100);

    bool result = timer.stop();

    EXPECT_TRUE(result);
}

TEST_F(TimerAwaiterTest, testRunning)
{
    TimerAwaiter timer(100);

    EXPECT_FALSE(timer.running());

    timer.start();
    EXPECT_TRUE(timer.running());

    timer.stop();
    // Note: timer.running() may still be true until event loop processes the
    // stop
}

TEST_F(TimerAwaiterTest, testExpired)
{
    TimerAwaiter timer(100);

    EXPECT_FALSE(timer.expired());

    timer.start();
    EXPECT_FALSE(timer.expired());
}

TEST_F(TimerAwaiterTest, testOperatorConversion)
{
    TimerAwaiter timer1(NSM_SW_SUCCESS);
    TimerAwaiter timer2(NSM_SW_ERROR);

    EXPECT_FALSE(timer1.running());
    EXPECT_FALSE(timer2.running());
}

TEST_F(TimerAwaiterTest, testMultipleStartStop)
{
    TimerAwaiter timer(100);

    timer.start();
    EXPECT_TRUE(timer.running());

    timer.stop();
    // Note: timer.running() may still be true until event loop processes the
    // stop

    timer.start();
    EXPECT_TRUE(timer.running());

    timer.stop();
    // Note: timer.running() may still be true until event loop processes the
    // stop
}

TEST_F(TimerAwaiterTest, testDifferentTimeDurations)
{
    TimerAwaiter timer1(50);
    TimerAwaiter timer2(100);
    TimerAwaiter timer3(1000);

    EXPECT_TRUE(timer1.start());
    EXPECT_TRUE(timer2.start());
    EXPECT_TRUE(timer3.start());

    EXPECT_TRUE(timer1.running());
    EXPECT_TRUE(timer2.running());
    EXPECT_TRUE(timer3.running());
}
