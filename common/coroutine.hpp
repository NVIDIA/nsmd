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
        }

        /** @brief Keeping the value returned by co_return operator
         */
        void return_value(uint8_t value) noexcept
        {
            data = std::move(value);
        }
    };

    /** @brief Called by co_await to check if it needs to be
     * suspened.
     */
    bool await_ready() const noexcept
    {
        return handle.done();
    }

    /** @brief Called by co_await operator to get return value when coroutine
     * finished.
     */
    uint8_t await_resume() const noexcept
    {
        return std::move(handle.promise().data);
    }

    /** @brief Called when the coroutine itself is being suspended. The
     * recording the parent coroutine handle is for await_suspend() in
     * promise_type::final_suspend to refer.
     */
    bool await_suspend(std::coroutine_handle<> coroutine)
    {
        handle.promise().parent_handle = coroutine;
        return true;
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
    mutable std::coroutine_handle<promise_type> handle;
};
} // namespace requester
