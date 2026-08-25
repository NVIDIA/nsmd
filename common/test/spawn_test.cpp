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
 * These tests double as the usage examples for spawnDetached, driving a real
 * event loop so that the deferral being tested is the real one.
 */

#include "sleep.hpp"
#include "spawn.hpp"

#include <chrono>
#include <string>

#include <gtest/gtest.h>

namespace
{

/** Drive the loop until the spawned work has reported, or give up. */
static void runUntil(common::Event& event, const bool& done)
{
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    while (!done && std::chrono::steady_clock::now() < deadline)
    {
        event.run(std::chrono::microseconds(1000));
    }
    ASSERT_TRUE(done) << "spawned work never ran";
}

struct SpawnDetachedTest : public ::testing::Test
{
    common::Event event;
};

// Nothing runs inside the call, whether or not the work would suspend.
TEST_F(SpawnDetachedTest, WorkStartsOnlyOnceTheCallerHasReturned)
{
    bool synchronous = false;
    bool suspending = false;

    common::spawnDetached(event, [&synchronous] { synchronous = true; });
    common::spawnDetached(event, [this, &suspending]() -> requester::Coroutine {
        co_await common::Sleep(event, 5000, common::NonPriority);
        suspending = true;
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    });

    EXPECT_FALSE(synchronous);
    EXPECT_FALSE(suspending);

    runUntil(event, suspending);
    EXPECT_TRUE(synchronous);
}

// Without an Event the work goes to sd_event_default(), which is the loop the
// fixture's own Event refers to, so the fixture can still drive it.
TEST_F(SpawnDetachedTest, WorkCanGoToTheDefaultEventLoop)
{
    bool ran = false;

    common::spawnDetached([&ran] { ran = true; });
    EXPECT_FALSE(ran);

    runUntil(event, ran);
}

// A coroutine lambda's frame points at its closure rather than copying it, so
// dropping the closure once the coroutine has started leaves every capture
// dangling across the first suspension.
TEST_F(SpawnDetachedTest, ACoroutineLambdaKeepsItsCapturesAcrossASuspension)
{
    bool done = false;

    common::spawnDetached(event,
                          [this, &done, name = std::string("connectx"),
                           delay = uint64_t{5000}]() -> requester::Coroutine {
        co_await common::Sleep(event, delay, common::NonPriority);

        // Read only after suspending, so a lost closure shows up here.
        EXPECT_EQ(name, "connectx");
        done = true;
        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    });

    runUntil(event, done);
}

} // namespace
