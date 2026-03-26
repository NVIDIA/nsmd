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

#pragma once
#include <phosphor-logging/lg2.hpp>

#include <coroutine>
#include <exception>
#include <memory>
#include <utility>

namespace requester
{

/** @struct Coroutine
 *
 * A coroutine return_object supports nesting coroutine
 */
struct Coroutine
{
    /** @brief The nested struct named 'promise_type' which is needed for
     * Coroutine struct to be a coroutine return_object.
     */
    struct promise_type
    {
        /** @brief For keeping the parent coroutine handle if any. For the case
         * of nesting co_await coroutine, this handle will be used to resume to
         * continue parent coroutine.
         */
        std::coroutine_handle<> parent_handle;

        /** @brief For holding return value of coroutine
         */
        uint8_t data;

        /** @brief For holding exception if any.
         */
        std::exception_ptr exception;

        bool detached = false;

        /** @brief Get the return object object
         */
        Coroutine get_return_object()
        {
            return {std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        /** @brief The method is called before starting a coroutine. Returning
         * std::suspend_never awaitable to execute coroutine body immediately.
         */
        std::suspend_never initial_suspend()
        {
            return {};
        }

        /** @brief The method is called after coroutine completed to return a
         * customized awaitable object to resume to parent coroutine if any.
         */
        auto final_suspend() noexcept
        {
            struct awaiter
            {
                /** @brief Returning false to make await_suspend to be called.
                 */
                bool await_ready() const noexcept
                {
                    return false;
                }

                /** @brief Do nothing here for customized awaitable object.
                 */
                void await_resume() const noexcept {}

                /** @brief Returning parent coroutine handle here to continue
                 * parent coroutine.
                 */
                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<promise_type> h) noexcept
                {
                    auto parent_handle = h.promise().parent_handle;
                    if (h.promise().detached)
                    {
                        h.destroy();
                    }
                    if (parent_handle)
                    {
                        return parent_handle;
                    }
                    return std::noop_coroutine();
                }
            };
            return awaiter{};
        }

        /** @brief The handler for an exception was thrown in
         * coroutine body.
         */
        void unhandled_exception()
        {
            try
            {
                throw; // Rethrow the current exception to handle it
            }
            catch (const std::exception& e)
            {
                lg2::error("Caught exception:: {HANDLER_EXCEPTION}",
                           "HANDLER_EXCEPTION", e.what());
            }
            catch (...)
            {
                lg2::error("Caught unknown exception in coroutine");
            }
            exception = std::current_exception();
        }

        /** @brief Keeping the value returned by co_return operator
         */
        void return_value(uint8_t value) noexcept
        {
            data = std::move(value);
        }
    };

#ifdef COVERAGE_DISABLE_COROUTINES
    promise_type promise;
#endif // COVERAGE_DISABLE_COROUTINES

    /** @brief Check if the coroutine is done.
     *
     * @return True if the coroutine is done, false otherwise.
     */
    bool done() const noexcept
    {
        return !handle || handle.done();
    }

    /** @brief Called by co_await to check if it needs to be
     * suspened.
     */
    bool await_ready() const noexcept
    {
        return done();
    }

    /** @brief Get the exception if any.
     *
     * @return The exception if any.
     */
    std::exception_ptr exception() const noexcept
    {
        return handle ? handle.promise().exception : nullptr;
    }

    /** @brief Get the data returned by the coroutine.
     *
     * @return The data returned by the coroutine.
     */
    uint8_t data() const noexcept
    {
#ifdef COVERAGE_DISABLE_COROUTINES
        return promise.data; // In coverage mode, use promise directly
#else
        return handle ? std::move(handle.promise().data) : 0;
#endif
    }

    /** @brief Called by co_await operator to get return value when coroutine
     * finished.
     */
    uint8_t await_resume() const noexcept
    {
        return data();
    }

    /** @brief Called when the coroutine itself is being suspended. The
     * recording the parent coroutine handle is for await_suspend() in
     * promise_type::final_suspend to refer.
     */
    bool await_suspend(std::coroutine_handle<> coroutine)
    {
        if (handle)
        {
            handle.promise().parent_handle = coroutine;
        }
        return true;
    }

    Coroutine() : handle(nullptr) {}
    Coroutine(std::coroutine_handle<promise_type> h) : handle(h) {}
    Coroutine(const Coroutine&) = delete;
    Coroutine& operator=(const Coroutine&) = delete;
    Coroutine(Coroutine&& other) noexcept
#ifdef COVERAGE_DISABLE_COROUTINES
        :
        promise({.parent_handle = {},
                 .data = std::exchange(other.promise.data, 0),
                 .exception = {},
                 .detached = false})
#else
        : handle(std::exchange(other.handle, {}))
#endif
    {}
    Coroutine& operator=(Coroutine&& other) noexcept
    {
        if (this != &other)
        {
#ifndef COVERAGE_DISABLE_COROUTINES
            if (handle)
            {
                handle.destroy();
            }
            handle = std::exchange(other.handle, {});
#else
            promise.data = other.promise.data;
            other.promise.data = 0;
#endif
        }
        return *this;
    }

    ~Coroutine()
    {
        if (handle && handle.done())
        {
            handle.destroy();
        }
    }

    void detach()
    {
        if (!handle)
        {
            return;
        }

        if (handle.done())
        {
            handle.destroy();
        }
        else
        {
            handle.promise().detached = true;
        }
        handle = nullptr;
    }

    /** @brief Assigned by promise_type::get_return_object to keep coroutine
     * handle itself.
     */
    mutable std::coroutine_handle<promise_type> handle = nullptr;

    /** @brief Assign a new coroutine to the handle.
     *
     * @param handle The handle to assign the coroutine to.
     * @param task The task to assign to the handle.
     * @return True if the coroutine was assigned, false otherwise.
     */
    template <typename Task>
    static bool assign(std::coroutine_handle<>& handle, Task&& task)
    {
        // Coroutine still running, return false
        if (handle && !handle.done())
        {
            return false;
        }

        // Destroy the old coroutine if it exists and is done
        if (handle && handle.done())
        {
            handle.destroy();
        }
        handle = nullptr;

        auto co = task();
        if (!co.handle)
        {
            co.handle = nullptr;
            return true; // no new coroutine
        }

        // Destroy the new coroutine if it's already finished
        if (co.handle.done())
        {
            co.handle.destroy();
            co.handle = nullptr;
            return true;
        }

        // Assign the new coroutine
        handle = co.handle;
        co.handle = nullptr; // transfer ownership, prevent ~Coroutine() from
                             // destroying the suspended frame
        return true;
    }

#ifdef COVERAGE_DISABLE_COROUTINES

    template <typename T>
    Coroutine(T&& value)
    {
        promise.data = static_cast<uint8_t>(value);
        // Don't create handle in coverage mode - it would be invalid
        // handle = nullptr is already set by default initialization
    }
    // Constrain conversion operator to arithmetic types only
    // to prevent unwanted conversions (e.g., to std::source_location)
    template <typename T>
        requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
    operator T() const
    {
        return static_cast<T>(data());
    }

    explicit operator bool() const
    {
        return data() != 0;
    }

    template <typename T>
    bool operator==(T other) const
    {
        return data() == static_cast<uint8_t>(other);
    }

    template <typename T>
    bool operator!=(T other) const
    {
        return !(*this == other);
    }
#endif // COVERAGE_DISABLE_COROUTINES
};
} // namespace requester
#ifdef COVERAGE_DISABLE_COROUTINES
namespace lg2::details
{
// Custom log_convert for Coroutine - treat it as unsigned 8-bit value
template <log_flags... Fs>
inline auto log_convert(const char* h, log_flag<Fs...> f,
                        const requester::Coroutine& rc)
{
    // Convert Coroutine to uint64_t (like other unsigned integrals in lg2)
    // and add appropriate flags
    return std::make_tuple(h, (f | unsigned_val | field8).value,
                           static_cast<uint64_t>(rc.data()));
}
} // namespace lg2::details
#endif // COVERAGE_DISABLE_COROUTINES
