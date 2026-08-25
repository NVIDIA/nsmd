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

#pragma once

#include "libnsm/base.h"

#include "common/coroutine.hpp"
#include "common/event.hpp"
#include "common/spawn.hpp"

#include <phosphor-logging/lg2.hpp>

#include <coroutine>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace common
{

/** @brief Return the message held by an exception_ptr. */
inline std::string describeException(std::exception_ptr thrown) noexcept
{
    if (!thrown)
    {
        return "no exception";
    }
    try
    {
        std::rethrow_exception(thrown);
    }
    catch (const std::exception& e)
    {
        return e.what();
    }
    catch (...)
    {
        return "unknown exception";
    }
}

namespace detail
{

/**
 * @brief State shared by the scope and its in-flight tasks for one round.
 *
 * Destroyed when the last task finishes after join(), resuming the joiner.
 */
struct ScopeState
{
    explicit ScopeState(const common::Event& event) : event(event) {}

    ~ScopeState()
    {
        resume();
    }

    ScopeState(const ScopeState&) = delete;
    ScopeState& operator=(const ScopeState&) = delete;
    ScopeState(ScopeState&&) = delete;
    ScopeState& operator=(ScopeState&&) = delete;

    const common::Event event;

    std::coroutine_handle<> waiter = nullptr;

    // Deferred to the loop: this runs from a destructor during coroutine
    // frame teardown, where resuming inline is unsafe.
    void resume()
    {
        auto handle = std::exchange(waiter, nullptr);
        if (!handle)
        {
            return;
        }

        spawnDetached(event, [handle] {
            if (!handle.done())
            {
                handle.resume();
            }
        });
    }
};

/** @brief Await a task, holding @p state until it finishes or throws. */
inline requester::Coroutine
    adopt(requester::Coroutine task,
          [[maybe_unused]] std::shared_ptr<ScopeState> state)
{
    static_cast<void>(co_await task);

    if (auto thrown = task.exception())
    {
        lg2::error("AsyncScope: task threw: {ERROR}", "ERROR",
                   describeException(thrown));
    }

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

} // namespace detail

/**
 * @brief Run detached coroutines and wait for all of them to finish.
 *
 *   common::AsyncScope scope(event);
 *   for (size_t i = 0; i < devices.size(); ++i)
 *   {
 *       scope.spawn([&, i]() -> requester::Coroutine {
 *           results[i] = co_await setOne(devices[i]);
 *           co_return NSM_SW_SUCCESS;
 *       });
 *   }
 *   co_await scope.join();
 *
 * Tasks start together on the next loop iteration, so their requests overlap.
 * Results are not collected; tasks write to caller-owned storage. A scope is
 * reusable: each join() waits for tasks spawned since the previous one.
 */
class AsyncScope
{
  public:
    /** @brief Defaults to the loop from sd_event_default(). */
    explicit AsyncScope(const common::Event& event = common::Event()) :
        event(event)
    {}

    AsyncScope(const AsyncScope&) = delete;
    AsyncScope& operator=(const AsyncScope&) = delete;
    AsyncScope(AsyncScope&&) = delete;
    AsyncScope& operator=(AsyncScope&&) = delete;

    /**
     * @brief Start a task; the next join() waits for it, even if not started.
     *
     * Takes a callable (as spawnDetached does) because a coroutine would
     * already be running when passed.
     */
    template <Spawnable Fn>
    void spawn(Fn fn)
    {
#ifdef COVERAGE_DISABLE_COROUTINES
        // Coverage builds never resume suspended frames, so run inline.
        fn();
#else
        if (!state)
        {
            state = std::make_shared<detail::ScopeState>(event);
        }

        spawnDetached(event, [fn = std::move(fn), ref = state]() mutable {
            if constexpr (CoroutineFactory<Fn>)
            {
                return detail::adopt(fn(), ref);
            }
            else
            {
                fn();
            }
        });
#endif
    }

    struct JoinAwaiter
    {
        AsyncScope& scope;

        bool await_ready() const noexcept
        {
            // Only the scope's own reference left: every task finished.
            return !scope.state || scope.state.use_count() == 1;
        }

        void await_suspend(std::coroutine_handle<> handle) const noexcept
        {
            scope.state->waiter = handle;

            // The last task to finish destroys the state, resuming us.
            scope.state.reset();
        }

        void await_resume() const noexcept {}
    };

    /** @brief Wait for every task spawned since the last join. */
    JoinAwaiter join()
    {
        return JoinAwaiter{*this};
    }

  private:
    const common::Event event;

    // Null between rounds; created by the first spawn().
    std::shared_ptr<detail::ScopeState> state;
};

} // namespace common
