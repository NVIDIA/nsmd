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

#include <phosphor-logging/lg2.hpp>

#include <concepts>
#include <utility>

namespace common
{

/** @brief A callable that returns a coroutine. */
template <typename T>
concept CoroutineFactory = requires(T& fn) {
                               { fn() } -> std::same_as<requester::Coroutine>;
                           };

/** @brief A callable that does the work synchronously. */
template <typename T>
concept VoidCallable = requires(T& fn) {
                           { fn() } -> std::same_as<void>;
                       };

/** @brief Either form spawnDetached accepts. */
template <typename T>
concept Spawnable = CoroutineFactory<T> || VoidCallable<T>;

namespace detail
{

/**
 * @brief Run a factory's coroutine and keep the factory alive.
 *
 * A coroutine lambda's frame points at its closure. It does not copy it. So
 * the closure has to outlive the coroutine, not just the call that created it.
 * Taking it by value stores it in this frame, which lives until the coroutine
 * has finished.
 */
template <CoroutineFactory Factory>
requester::Coroutine runOwned(Factory factory)
{
    co_await factory();

    // coverity[missing_return]
    co_return NSM_SW_SUCCESS;
}

template <Spawnable Fn>
int startSpawned(sd_event_source*, void* userdata)
{
    auto* held = static_cast<Fn*>(userdata);
    if constexpr (CoroutineFactory<Fn>)
    {
        runOwned(std::move(*held)).detach();
    }
    else
    {
        (*held)();
    }
    delete held;
    return 0;
}

} // namespace detail

/**
 * @brief Start background work on the next event loop iteration.
 *
 * requester::Coroutine starts eagerly. Calling one and detaching it runs the
 * body inside the caller, up to the first suspension. A body that never
 * suspends runs to completion there, before the caller returns.
 *
 * Taking a callable instead of a coroutine defers the call itself. Write the
 * work inline as a coroutine:
 *
 *   common::spawnDetached(event, [this, mode]() -> requester::Coroutine {
 *       co_await doWork(mode);
 *       co_return NSM_SW_SUCCESS;
 *   });
 *
 * or return one, or do the work synchronously and return nothing:
 *
 *   common::spawnDetached(event, [this, mode] { return doWork(mode); });
 *   common::spawnDetached(event, [this] { republish(); });
 *
 * The callable runs once. It outlives this call and any coroutine it returns,
 * so captures by value are safe after a suspension. Nothing joins the result.
 * Use AsyncScope when completion has to be awaited.
 */
template <Spawnable Fn>
void spawnDetached(const common::Event& event, Fn fn)
{
    auto* held = new Fn(std::move(fn));
    if (sd_event_add_defer(event.get(), nullptr, &detail::startSpawned<Fn>,
                           held) < 0)
    {
        lg2::error("spawnDetached: failed to defer, starting inline");
        detail::startSpawned<Fn>(nullptr, held);
    }
}

/** @brief Start background work on the default event loop. */
template <Spawnable Fn>
void spawnDetached(Fn fn)
{
    spawnDetached(common::Event{}, std::move(fn));
}

} // namespace common
