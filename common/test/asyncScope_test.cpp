// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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
 * These tests double as the usage examples for AsyncScope: each one is the
 * shape a caller fanning out over devices would write. Tasks finish
 * asynchronously through common::Sleep and the loop is driven with
 * event.run(), so the timing is real rather than mocked.
 */

#include "asyncScope.hpp"
#include "sleep.hpp"

#include <chrono>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace
{

/** What one task reports back, defaulting to failure until it says otherwise.
 */
struct Outcome
{
    bool ran = false;
    uint8_t code = 0xFF;
};

/** One participant: what it returns, when, and whether it throws instead. */
struct Step
{
    uint8_t code;
    std::chrono::milliseconds delay;
    bool byThrowing;
};

/**
 * A task that finishes after a delay and records its own outcome, so tasks can
 * complete out of order while results stay tied to their slot.
 */
static requester::Coroutine runStep(const common::Event& event, Step step,
                                    Outcome& outcome)
{
    co_await common::Sleep(
        event,
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(step.delay)
                .count()),
        common::NonPriority);

    if (step.byThrowing)
    {
        throw std::runtime_error("device fell off the bus");
    }

    outcome.ran = true;
    outcome.code = step.code;
    // coverity[missing_return]
    co_return step.code;
}

/** A task that records its outcome without ever suspending. */
static requester::Coroutine runNow(uint8_t code, Outcome& outcome)
{
    outcome.ran = true;
    outcome.code = code;
    // coverity[missing_return]
    co_return code;
}

/** Drive the loop until the fan-out has finished. */
static void runUntilDone(common::Event& event,
                         const requester::Coroutine& fanOut)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    while (!fanOut.done() && std::chrono::steady_clock::now() < deadline)
    {
        event.run(std::chrono::microseconds(1000));
    }
    ASSERT_TRUE(fanOut.done()) << "AsyncScope::join never resumed";
}

/** Drive the loop for a while, for work nothing is waiting on. */
static void runFor(common::Event& event, std::chrono::milliseconds duration)
{
    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline)
    {
        event.run(std::chrono::microseconds(1000));
    }
}

struct AsyncScopeTest : public ::testing::Test
{
    common::Event event;
};

/**
 * The call site under test: spawn every task, then await the scope draining.
 */
static requester::Coroutine fanOut(const common::Event& event,
                                   const std::vector<Step>& steps,
                                   std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope(event);
    for (size_t i = 0; i < steps.size(); ++i)
    {
        scope.spawn([&event, &steps, &outcomes, i] {
            return runStep(event, steps[i], outcomes[i]);
        });
    }
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// join() must wait for the slowest task, not the first to finish, and each
// task's result must stay in its own slot.
TEST_F(AsyncScopeTest, JoinWaitsForEveryTaskWhateverTheOrder)
{
    // Reversed delays: the first task spawned finishes last.
    const std::vector<Step> steps{
        {10, std::chrono::milliseconds(30), false},
        {20, std::chrono::milliseconds(20), false},
        {30, std::chrono::milliseconds(10), false},
    };
    std::vector<Outcome> outcomes(steps.size());

    auto task = fanOut(event, steps, outcomes);
    runUntilDone(event, task);

    for (size_t i = 0; i < outcomes.size(); ++i)
    {
        EXPECT_TRUE(outcomes[i].ran) << "task " << i << " had not finished";
        EXPECT_EQ(outcomes[i].code, steps[i].code);
    }
}

// A task that throws still releases the scope, so join() cannot hang; its
// slot keeps the failure-shaped default rather than looking like a success.
TEST_F(AsyncScopeTest, AThrowingTaskStillReleasesTheScope)
{
    const std::vector<Step> steps{
        {10, std::chrono::milliseconds(10), false},
        {20, std::chrono::milliseconds(5), true},
    };
    std::vector<Outcome> outcomes(steps.size());

    auto task = fanOut(event, steps, outcomes);
    runUntilDone(event, task);

    EXPECT_TRUE(outcomes[0].ran);
    EXPECT_EQ(outcomes[0].code, 10);

    EXPECT_FALSE(outcomes[1].ran);
    EXPECT_EQ(outcomes[1].code, 0xFF);
}

/** Spawns a task that finishes immediately and one that does not. */
static requester::Coroutine mixedFanOut(const common::Event& event,
                                        std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope(event);
    scope.spawn([&outcomes] { return runNow(1, outcomes[0]); });
    scope.spawn([&event, &outcomes] {
        return runStep(event, Step{2, std::chrono::milliseconds(20), false},
                       outcomes[1]);
    });
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// The first task is done the moment it starts, taking the count down while the
// second is still running. join() must wait for the whole round.
TEST_F(AsyncScopeTest, ATaskThatFinishesFirstDoesNotEndTheJoin)
{
    std::vector<Outcome> outcomes(2);

    auto task = mixedFanOut(event, outcomes);
    EXPECT_FALSE(task.done()) << "the join returned with a task outstanding";

    runUntilDone(event, task);
    EXPECT_TRUE(outcomes[0].ran);
    EXPECT_TRUE(outcomes[1].ran);
}

/** Uses one scope for two rounds of work. */
static requester::Coroutine twoRounds(const common::Event& event,
                                      std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope(event);

    scope.spawn([&event, &outcomes] {
        return runStep(event, Step{1, std::chrono::milliseconds(10), false},
                       outcomes[0]);
    });
    co_await scope.join();

    // The second round has not been spawned yet, so the first join waited for
    // its own round and nothing else.
    EXPECT_TRUE(outcomes[0].ran);
    EXPECT_FALSE(outcomes[1].ran);

    scope.spawn([&event, &outcomes] {
        return runStep(event, Step{2, std::chrono::milliseconds(10), false},
                       outcomes[1]);
    });
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// A scope is reusable: each join waits for whatever is outstanding at the
// time, so spawning again afterwards is ordinary use rather than an error.
TEST_F(AsyncScopeTest, AScopeCanBeJoinedThenReused)
{
    std::vector<Outcome> outcomes(2);

    auto task = twoRounds(event, outcomes);
    runUntilDone(event, task);

    EXPECT_TRUE(outcomes[0].ran);
    EXPECT_TRUE(outcomes[1].ran);
}

/** Fans out over tasks that never suspend. */
static requester::Coroutine fanOutImmediate(const common::Event& event,
                                            const std::vector<uint8_t>& codes,
                                            std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope(event);
    for (size_t i = 0; i < codes.size(); ++i)
    {
        scope.spawn(
            [&codes, &outcomes, i] { return runNow(codes[i], outcomes[i]); });
    }
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// A round with nothing in it has nothing to wait for, which await_ready()
// catches without the loop having to run.
TEST_F(AsyncScopeTest, JoiningAnEmptyRoundDoesNotSuspend)
{
    std::vector<Outcome> none;

    auto task = fanOutImmediate(event, {}, none);
    EXPECT_TRUE(task.done());
}

// Spawning defers the start, so even tasks that never suspend are outstanding
// until the loop turns: the join must not read the round as already drained.
TEST_F(AsyncScopeTest, TasksThatNeverSuspendStillNeedTheLoopToTurn)
{
    const std::vector<uint8_t> codes{1, 2};
    std::vector<Outcome> outcomes(codes.size());

    auto task = fanOutImmediate(event, codes, outcomes);
    EXPECT_FALSE(task.done());

    runUntilDone(event, task);
    EXPECT_EQ(outcomes[0].code, 1);
    EXPECT_EQ(outcomes[1].code, 2);
}

/** Fans out with the work written inline rather than called through. */
static requester::Coroutine fanOutInline(const common::Event& event,
                                         std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope(event);
    for (size_t i = 0; i < outcomes.size(); ++i)
    {
        // Captured by value, so these live in the closure rather than in this
        // frame, and are read after a suspension.
        scope.spawn(
            [&event, out = &outcomes[i],
             code = static_cast<uint8_t>(i + 1)]() -> requester::Coroutine {
            co_await common::Sleep(event, 5000, common::NonPriority);
            out->ran = true;
            out->code = code;
            // coverity[missing_return]
            co_return code;
        });
    }
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// A coroutine lambda's frame points at its closure rather than copying it, so
// spawning must keep the closure alive for as long as the coroutine runs, not
// merely until it starts.
TEST_F(AsyncScopeTest, WorkCanBeWrittenInlineAsACoroutineLambda)
{
    std::vector<Outcome> outcomes(2);

    auto task = fanOutInline(event, outcomes);
    runUntilDone(event, task);

    EXPECT_EQ(outcomes[0].code, 1);
    EXPECT_EQ(outcomes[1].code, 2);
}

/** Fans out on the default event loop, taking no Event. */
static requester::Coroutine fanOutOnDefaultEvent(std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope;
    for (size_t i = 0; i < outcomes.size(); ++i)
    {
        scope.spawn([out = &outcomes[i], code = static_cast<uint8_t>(i + 1)] {
            out->ran = true;
            out->code = code;
        });
    }
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// A scope built without an Event uses sd_event_default(), which is the loop the
// fixture's own Event refers to, so the fixture can still drive it.
TEST_F(AsyncScopeTest, AScopeCanUseTheDefaultEventLoop)
{
    std::vector<Outcome> outcomes(2);

    auto task = fanOutOnDefaultEvent(outcomes);
    EXPECT_FALSE(task.done());

    runUntilDone(event, task);
    EXPECT_EQ(outcomes[0].code, 1);
    EXPECT_EQ(outcomes[1].code, 2);
}

/** Fans out over synchronous work, which needs no coroutine at all. */
static requester::Coroutine fanOutVoid(const common::Event& event,
                                       std::vector<Outcome>& outcomes)
{
    common::AsyncScope scope(event);
    for (size_t i = 0; i < outcomes.size(); ++i)
    {
        scope.spawn([out = &outcomes[i], code = static_cast<uint8_t>(i + 1)] {
            out->ran = true;
            out->code = code;
        });
    }
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

// A function returning nothing is finished when it returns, so the whole round
// completes on the turn of the loop that starts it.
TEST_F(AsyncScopeTest, WorkCanBeAPlainFunctionReturningNothing)
{
    std::vector<Outcome> outcomes(2);

    auto task = fanOutVoid(event, outcomes);
    EXPECT_FALSE(task.done());

    runUntilDone(event, task);
    EXPECT_EQ(outcomes[0].code, 1);
    EXPECT_EQ(outcomes[1].code, 2);
}

/** Spawns a callable whose call operator changes its own captures. */
static requester::Coroutine fanOutMutable(const common::Event& event,
                                          Outcome& outcome)
{
    common::AsyncScope scope(event);
    scope.spawn([&outcome, calls = uint8_t{0}]() mutable {
        ++calls;
        outcome.ran = true;
        outcome.code = calls;
    });
    co_await scope.join();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

TEST_F(AsyncScopeTest, WorkCanBeAMutableCallable)
{
    Outcome outcome;

    auto task = fanOutMutable(event, outcome);
    runUntilDone(event, task);
    EXPECT_TRUE(outcome.ran);
    EXPECT_EQ(outcome.code, 1);
}

// Fails if the shared state is ever collapsed onto the caller's frame: the
// task, which has not even started yet, would report through a dangling
// pointer.
TEST_F(AsyncScopeTest, DroppingTheScopeWithoutJoiningIsSafe)
{
    std::vector<Outcome> outcomes(1);

    {
        common::AsyncScope scope(event);
        scope.spawn([this, &outcomes] {
            return runStep(event, Step{1, std::chrono::milliseconds(5), false},
                           outcomes[0]);
        });
    }

    runFor(event, std::chrono::milliseconds(200));
    EXPECT_TRUE(outcomes[0].ran);
}

} // namespace
