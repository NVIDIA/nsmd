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

#include "libnsm/base.h"

#include <sdeventplus/event.hpp>

#include <coroutine>

namespace common
{
class TimerAwaiter
{
  public:
    /** @brief Called by co_await to check if it needs to be
     * suspened.
     */
    bool await_ready() const noexcept
    {
        return false;
    }

    /** @brief Called by co_await operator to get return void when coroutine
     * finished.
     */
    nsm_sw_codes await_resume() const noexcept
    {
        return rc;
    }

    /** @brief Called to suspend the coroutine and register the timer in the
     * event loop. */
    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        this->handle = handle; // Store the handle to resume later
        return start();
    }

    TimerAwaiter() = default;
    TimerAwaiter& operator=(const TimerAwaiter& other) = delete;

    /** @brief Constructor to initialize the awaitable object.
     *
     * @param time Duration of the timer in milliseconds.
     */
    TimerAwaiter(uint64_t time) :
        durationInUsec(time * 1000), // Convert mSec to uSec
        rc(NSM_SW_SUCCESS)           // Initialize the result code to success
    {}
    ~TimerAwaiter()
    {
        if (handle && handle.done())
        {
            handle.destroy();
        }
    }

    /** @brief Starts the timer. */
    bool start()
    {
        isRunning = isExpired = false;
        uint64_t now;
        if (sd_event_now(event.get(), CLOCK_MONOTONIC, &now) < 0)
        {
            rc = NSM_SW_ERROR; // Failure scenario handling
            return false;      // Don't suspend on failure
        }

        if (sd_event_add_time(
                event.get(), &eventSource, CLOCK_MONOTONIC,
                now + durationInUsec,  // Timer expiration in microseconds
                0,                     // Accuracy
                &TimerAwaiter::resume, // Callback function
                this) < 0)
        {
            rc = NSM_SW_ERROR; // Handle errors from adding the timer
            return false;      // Don't suspend on failure
        }

        sd_event_source_set_priority(eventSource, SD_EVENT_PRIORITY_NORMAL);
        startTime = now;
        isRunning = true;
        return true; // Coroutine suspended successfully
    }

    /** @brief Stops the timer to resume the coroutine. */
    bool stop()
    {
        if (isRunning)
        {
            sd_event_source_unref(eventSource); // Cancel the timer event source
            if (sd_event_add_time(event.get(), &eventSource, CLOCK_MONOTONIC,
                                  0,            // Call the callback immediately
                                  0,
                                  &TimerAwaiter::resume, // Callback function
                                  this) < 0)
            {
                rc = NSM_SW_ERROR; // Handle errors from adding the timer
                return false;
            }
            sd_event_source_set_priority(eventSource, SD_EVENT_PRIORITY_NORMAL);
        }
        return true;
    }

    /** @brief Checks if the timer expired. */
    bool expired() const
    {
        return isExpired;
    }

    /** @brief Checks if the timer is running. */
    bool running() const
    {
        return isRunning;
    }

  private:
    /** @brief Handle to resume the suspended coroutine. */
    std::coroutine_handle<> handle;

    /** @brief Flag to indicate if the timer is running. */
    bool isRunning = false;
    /** @brief Flag to indicate if the timer is expired. */
    bool isExpired = false;

    /** @brief Pointer to the event source created by `sd_event_add_time`. */
    sd_event_source* eventSource = nullptr;

    /** @brief Reference to the event loop where the timer is registered. */
    const sdeventplus::Event event = sdeventplus::Event::get_default();

    /** @brief Start time of the timer. */
    uint64_t startTime = 0;

    /** @brief Duration for which the coroutine should call the timer callback,
     * in microseconds.
     */
    uint64_t durationInUsec;

    /** @brief Result code, which can be used as a return value after
     * resumption. */
    nsm_sw_codes rc;

    /** @brief Resumes the coroutine and sets the result code.
     */
    void resume(uint64_t now)
    {
        if (isRunning)
        {
            isRunning = false;
            isExpired = now >= (startTime + durationInUsec);
            rc = NSM_SW_SUCCESS;
            sd_event_source_unref(eventSource);
            handle.resume(); // Resume the coroutine
        }
    }

    /** @brief Timer callback that resumes the coroutine.
     * This is called by the event loop when the timeout occurs.
     */
    static int resume(sd_event_source*, uint64_t now, void* userdata)
    {
        auto timer = static_cast<TimerAwaiter*>(userdata);
        timer->resume(now);
        return 0;
    }
};
} // namespace common
