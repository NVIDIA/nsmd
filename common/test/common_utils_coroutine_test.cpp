/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "../coroutine.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace requester
{

/**
 * An awaiter that suspends a coroutine indefinitely (never resumes it
 * automatically). Used to create a coroutine that is suspended mid-flight
 * so we can test what happens when its Coroutine RAII wrapper is destroyed
 * before the coroutine completes.
 */
struct SuspendForever
{
    bool await_ready() const noexcept
    {
        return false;
    }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    void await_resume() const noexcept {}
};

/** Coroutine that suspends itself and never resumes. */
static Coroutine suspendedCoroutine()
{
    co_await SuspendForever{};
    co_return 0;
}

/** Coroutine that completes immediately without suspending. */
static Coroutine completedCoroutine()
{
    co_return 0;
}

/**
 * Test: destructor must destroy a suspended (not-done) coroutine handle.
 *
 * DISABLED: The ~Coroutine() fix (removing handle.done() check) was reverted
 * because destroying a suspended coroutine whose handle is also held by an
 * async callback (e.g. MCTP response handler) causes use-after-free when
 * the callback later tries to resume the destroyed handle. The old code
 * leaks the frame, which is safer than crashing. A proper fix requires a
 * cancellation mechanism or shared ownership of the coroutine handle.
 *
 * Original regression test for the bug where ~Coroutine() used
 *   if (handle && handle.done())
 * which skipped handle.destroy() for suspended coroutines, leaking the
 * heap-allocated coroutine frame. Valgrind reports this as "definitely
 * lost" bytes when the old (buggy) condition is used.
 */
TEST(CoroutineDestructorTest, DISABLED_DestroySuspendedHandle_NoMemoryLeak)
{
    {
        auto coro = suspendedCoroutine();
        // Coroutine is suspended at co_await SuspendForever{} — handle is
        // valid and handle.done() == false.
        EXPECT_FALSE(coro.done());
        // ~Coroutine() must call handle.destroy() here regardless of done().
    }
    // If the destructor used `if (handle && handle.done())`, valgrind would
    // report the coroutine frame as definitely lost at this point.
}

/**
 * Test: destructor must also destroy a completed coroutine handle.
 *
 * Sanity-check that the fix does not break the normal (already-done) path.
 */
TEST(CoroutineDestructorTest, DestroyCompletedHandle_NoMemoryLeak)
{
    {
        auto coro = completedCoroutine();
        EXPECT_TRUE(coro.done());
        // ~Coroutine() destroys the handle here as well.
    }
}

/**
 * Test: move assignment must destroy the old handle before replacing it.
 *
 * Verifies that operator=(Coroutine&&) calls handle.destroy() on the
 * previously held (suspended) handle so it is not leaked.
 */
TEST(CoroutineDestructorTest, MoveAssignment_DestroysOldSuspendedHandle)
{
#ifdef COVERAGE_DISABLE_COROUTINES
    // In coverage mode co_await is a no-op, so suspendedCoroutine() completes
    // immediately and done() returns true. The test is only meaningful with
    // real coroutines.
    GTEST_SKIP() << "Skipped: coroutine suspension not available in coverage";
#endif
    auto coro = suspendedCoroutine();
    EXPECT_FALSE(coro.done());

    // Move-assign a new (completed) coroutine; the old suspended handle must
    // be destroyed and must not leak.
    coro = completedCoroutine();
    EXPECT_TRUE(coro.done());
}

} // namespace requester
