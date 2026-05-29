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
 * Regression test for the bug where ~Coroutine() used
 *   if (handle && handle.done())
 * which skipped handle.destroy() for suspended coroutines, leaking the
 * heap-allocated coroutine frame. Valgrind reports this as "definitely
 * lost" bytes when the old (buggy) condition is used.
 *
 * The fix — removing the handle.done() guard — is safe because:
 * - NsmDevice::~NsmDevice() calls task.detach() before ~Coroutine() runs,
 *   transferring ownership to the event loop (handle set to nullptr).
 * - deviceTask holds shared_ptr<NsmDevice> by value, so ~NsmDevice() is
 *   only entered when use_count drops to zero, which cannot happen while
 *   the coroutine frame is live.
 */
#ifndef COVERAGE_DISABLE_COROUTINES
TEST(CoroutineDestructorTest, DestroySuspendedHandle_NoMemoryLeak)
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
#endif // COVERAGE_DISABLE_COROUTINES

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
 *
 * Not applicable in COVERAGE_DISABLE_COROUTINES mode: co_await is a no-op
 * so suspendedCoroutine() completes immediately and there is no suspended
 * handle to destroy. The coroutine.hpp move-assignment has a separate
 * #else branch for coverage mode that transfers promise.data instead.
 */
#ifndef COVERAGE_DISABLE_COROUTINES
TEST(CoroutineDestructorTest, MoveAssignment_DestroysOldSuspendedHandle)
#else
TEST(CoroutineDestructorTest,
     DISABLED_MoveAssignment_DestroysOldSuspendedHandle)
#endif
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

/**
 * Tests for Coroutine::assign().
 *
 * Regression coverage for bug 5749651: assign() called with a task that has
 * no co_await (completes immediately) caused a double-destroy crash:
 *   1. assign() called co.handle.destroy() on the done frame.
 *   2. co.handle was not nulled → dangling pointer.
 *   3. ~Coroutine() checked handle.done() on freed memory (UB).
 *   4. If UB returned true → second handle.destroy() → SIGABRT (double-free).
 * Fix: co.handle = nullptr after every explicit destroy inside assign().
 */

// Immediate task (no co_await): verifies no double-free (bug 5749651).
// If assign() omits `co.handle = nullptr` after destroy, this crashes with
// SIGABRT under ASAN/valgrind or sporadically in production.
TEST(CoroutineAssignTest, ImmediateTask_NoCrashAndHandleNull)
{
    std::coroutine_handle<> handle{};
    bool result = Coroutine::assign(handle, []() -> Coroutine { co_return 0; });
    EXPECT_TRUE(result);
    // Immediately-done frame is destroyed inside assign(); handle stays null.
    EXPECT_EQ(handle, nullptr);
}

// Two consecutive immediate tasks: no accumulated state between calls.
TEST(CoroutineAssignTest, ImmediateTaskTwice_BothSucceed)
{
    std::coroutine_handle<> handle{};
    EXPECT_TRUE(Coroutine::assign(handle, []() -> Coroutine { co_return 0; }));
    EXPECT_EQ(handle, nullptr);
    EXPECT_TRUE(Coroutine::assign(handle, []() -> Coroutine { co_return 0; }));
    EXPECT_EQ(handle, nullptr);
}

#ifndef COVERAGE_DISABLE_COROUTINES
// Suspended task: handle transferred to caller, not done.
TEST(CoroutineAssignTest, SuspendedTask_HandleSetAndNotDone)
{
    std::coroutine_handle<> handle{};
    bool result = Coroutine::assign(handle, []() -> Coroutine {
        co_await SuspendForever{};
        co_return 0;
    });
    EXPECT_TRUE(result);
    EXPECT_NE(handle, nullptr);
    EXPECT_FALSE(handle.done());
    handle.destroy();
}

// assign() while a coroutine is still suspended must return false and leave
// the existing handle untouched.
TEST(CoroutineAssignTest, AssignWhileRunning_ReturnsFalse)
{
    std::coroutine_handle<> handle{};
    Coroutine::assign(handle, []() -> Coroutine {
        co_await SuspendForever{};
        co_return 0;
    });
    ASSERT_NE(handle, nullptr);
    ASSERT_FALSE(handle.done());

    bool result = Coroutine::assign(handle, []() -> Coroutine { co_return 0; });
    EXPECT_FALSE(result);
    // Original handle must be unchanged.
    EXPECT_NE(handle, nullptr);
    EXPECT_FALSE(handle.done());

    handle.destroy();
}
#endif // COVERAGE_DISABLE_COROUTINES

} // namespace requester
