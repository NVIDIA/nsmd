/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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

/*
 * sleep_semaphore_mock_test.cpp
 *
 * Branch-coverage tests for common::Sleep and common::CoroutineSemaphore
 * using D1 (mockSdEvent.cpp - link-time sd_event overrides).
 *
 * Sleep branches covered:
 *   1. sd_event_now fails       → rc = NSM_SW_ERROR, await_suspend returns
 * false
 *   2. sd_event_add_time fails  → rc = NSM_SW_ERROR, await_suspend returns
 * false
 *   3. timerEventPriority == NonPriority → different accuracy value selected
 *   4. Both calls succeed       → await_suspend returns true (coroutine
 * suspends)
 *
 * CoroutineSemaphore branches covered:
 *   5. release() with suspended waiter → sd_event_add_defer fires callback
 *      synchronously (D1), waiter is resumed
 *   6. release() with suspended waiter + defer failure → lg2 error path,
 *      waiter remains suspended
 */

#include "common/event.hpp"
#include "coroutine.hpp"
#include "coroutineSemaphore.hpp"
#include "mockSdEvent.hpp"
#include "sleep.hpp"

#include <gtest/gtest.h>

// ============================================================================
// Sleep branch tests (D1 error injection)
// ============================================================================

/**
 * Sleep_SdEventNow_Fails_ReturnsError
 * When sd_event_now returns -1, await_suspend returns false immediately
 * (no suspension) and await_resume returns NSM_SW_ERROR.
 */
TEST(SleepMock, SdEventNow_Fails_ReturnsError)
{
    common::Event event;

    nsm::test::failNextSdEventNow = true;

    auto cr = [&]() -> requester::Coroutine {
        auto rc = co_await common::Sleep(event, 1000, common::Priority);
        co_return rc;
    }();

    EXPECT_TRUE(cr.done());
    EXPECT_EQ(cr.data(), NSM_SW_ERROR);
}

/**
 * Sleep_SdEventAddTime_Fails_ReturnsError
 * When sd_event_add_time returns -1, await_suspend returns false immediately
 * and await_resume returns NSM_SW_ERROR.
 */
TEST(SleepMock, SdEventAddTime_Fails_ReturnsError)
{
    common::Event event;

    nsm::test::failNextSdEventAddTime = true;

    auto cr = [&]() -> requester::Coroutine {
        auto rc = co_await common::Sleep(event, 1000, common::Priority);
        co_return rc;
    }();

    EXPECT_TRUE(cr.done());
    EXPECT_EQ(cr.data(), NSM_SW_ERROR);
}

/**
 * Sleep_NonPriority_Success_CoroutineSuspends
 * With NonPriority and no failure injection, await_suspend sets NonPriority
 * accuracy values and returns true. The coroutine remains suspended (D1 does
 * not fire the timer callback).
 */
TEST(SleepMock, NonPriority_Success_CoroutineSuspends)
{
    common::Event event;

    auto cr = [&]() -> requester::Coroutine {
        co_await common::Sleep(event, 1000, common::NonPriority);
        co_return NSM_SW_SUCCESS;
    }();

    // Coroutine is suspended: await_suspend returned true, no timeout fires
    EXPECT_FALSE(cr.done());
}

/**
 * Sleep_Priority_Success_CoroutineSuspends
 * With Priority and no failure injection, await_suspend returns true and the
 * coroutine remains suspended.
 */
TEST(SleepMock, Priority_Success_CoroutineSuspends)
{
    common::Event event;

    auto cr = [&]() -> requester::Coroutine {
        co_await common::Sleep(event, 1000, common::Priority);
        co_return NSM_SW_SUCCESS;
    }();

    // Coroutine is suspended
    EXPECT_FALSE(cr.done());
}

// ============================================================================
// CoroutineSemaphore contended-path tests (D1 sd_event_add_defer fires inline)
// ============================================================================

/**
 * CoroutineSemaphore_Contended_DeferFires_WaiterResumed
 * When a second coroutine tries to acquire an already-held semaphore, it
 * suspends into the queue. On release(), sd_event_add_defer is called.
 * D1 fires the defer callback synchronously, resuming the waiter in-place.
 */
TEST(CoroutineSemaphoreMock, Contended_DeferFires_WaiterResumed)
{
    common::CoroutineSemaphore sem;

    // First acquire: semaphore starts at 1, try_acquire succeeds,
    // await_ready returns true → coroutine does NOT suspend.
    auto taker = [&]() -> requester::Coroutine {
        co_await sem.acquire(1);
        co_return NSM_SW_SUCCESS;
    }();
    EXPECT_TRUE(taker.done()); // taker ran to completion immediately

    // Second acquire: semaphore is now at 0, try_acquire fails,
    // await_ready returns false → coroutine suspends into the queue.
    auto waiter = [&]() -> requester::Coroutine {
        co_await sem.acquire(2);
        co_return NSM_SW_SUCCESS;
    }();
    EXPECT_FALSE(waiter.done()); // waiter is suspended in the queue

    // release(): D1 fires sd_event_add_defer synchronously.
    // The deferred callback calls handle.resume() on the waiter.
    sem.release();

    EXPECT_TRUE(waiter.done());
    EXPECT_EQ(waiter.data(), NSM_SW_SUCCESS);
}

/**
 * CoroutineSemaphore_Contended_DeferFails_WaiterNotResumed
 * When sd_event_add_defer returns -1, release() logs an error and the waiter
 * remains suspended (the deferred resume never happens).
 */
TEST(CoroutineSemaphoreMock, Contended_DeferFails_WaiterNotResumed)
{
    common::CoroutineSemaphore sem;

    // Take the semaphore
    auto taker = [&]() -> requester::Coroutine {
        co_await sem.acquire(1);
        co_return NSM_SW_SUCCESS;
    }();
    EXPECT_TRUE(taker.done());

    // Waiter suspends in the queue
    auto waiter = [&]() -> requester::Coroutine {
        co_await sem.acquire(2);
        co_return NSM_SW_SUCCESS;
    }();
    EXPECT_FALSE(waiter.done());

    // Fail the defer: release() hits the lg2::error branch
    nsm::test::failNextSdEventAddDefer = true;
    sem.release();

    // Waiter was NOT resumed because defer failed
    EXPECT_FALSE(waiter.done());
}
