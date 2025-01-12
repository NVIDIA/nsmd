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

#include "common/globals.hpp"

#include <systemd/sd-event.h>

#include <sdbusplus/timer.hpp>
#include <sdeventplus/event.hpp>
#include <sdeventplus/source/event.hpp>

#include <coroutine>

namespace common
{
enum TimerEventPriority
{
    Priority,
    NonPriority
};

/** @struct Sleep
 *
 * An awaitable object needed by co_await operator to sleep for a certain
 * duration and suspend the current coroutine context
 * e.g. rc = co_await Sleep(event, duration);
 */
struct Sleep
{
    /** @brief Pointer to the event source created by `sd_event_add_time`. */
    sd_event_source* eventSource = nullptr;

    /** @brief Reference to the event loop where the timer is registered. */
    const sdeventplus::Event& event;

    /** @brief Duration for which the coroutine should sleep, in microseconds.
     */
    uint64_t durationInUsec;

    /** @brief Boolean to adjust priority. */
    TimerEventPriority timerEventPriority;

    /** @brief Result code, which can be used as a return value after
     * resumption. */
    nsm_sw_codes rc;

    /** @brief Handle to resume the suspended coroutine. */
    std::coroutine_handle<> resumeHandle;

    /** @brief Timer callback that resumes the coroutine.
     * This is called by the event loop when the timeout occurs.
     */
    static int on_timeout(sd_event_source* /* source */, uint64_t /* usec */,
                          void* userdata)
    {
        /* TODO: See if the event source can be reused.*/
        auto* sleep = static_cast<Sleep*>(userdata);
        sd_event_source_unref(sleep->eventSource);
        sleep->resumeHandle.resume(); // Resume the coroutine
        return 0;                     // Success
    }

    /** @brief The coroutine will always suspend, so `await_ready` returns
     * false. */
    bool await_ready() const noexcept
    {
        return false;
    }

    /** @brief Called to suspend the coroutine and register the timer in the
     * event loop. */
    bool await_suspend(std::coroutine_handle<> handle) noexcept
    {
        resumeHandle = handle; // Store the handle to resume later

        uint64_t now;
        if (sd_event_now(event.get(), CLOCK_MONOTONIC, &now) < 0)
        {
            rc = NSM_SW_ERROR; // Failure scenario handling
            return false;      // Don't suspend on failure
        }

        int64_t priority = SD_EVENT_SOURCE_MAX_PRIORITY;
        uint64_t accuracyInUsec = 1; // Accuracy of 1us

        if (timerEventPriority == NonPriority)
        {
            priority = SD_EVENT_SOURCE_NORMAL_PRIORITY;
            accuracyInUsec = 0;
        }

        if (sd_event_add_time(
                event.get(), &eventSource, CLOCK_MONOTONIC,
                now + durationInUsec, // Timer expiration in microseconds
                accuracyInUsec,       // Accuracy
                &Sleep::on_timeout,   // Callback function
                this) <               // Pass `this` to access the object
            0)
        {
            rc = NSM_SW_ERROR; // Handle errors from adding the timer
            return false;      // Don't suspend on failure
        }

        sd_event_source_set_priority(eventSource, priority);

        return true; // Coroutine suspended successfully
    }

    /** @brief Called after the awaitable is complete to get the return value.
     */
    nsm_sw_codes await_resume() const noexcept
    {
        return rc;
    }

    /** @brief Constructor to initialize the awaitable object.
     *
     * @param event Reference to the event loop to which the timer will be
     * added.
     * @param durationInMsec Duration of the sleep in microseconds.
     */
    Sleep(const sdeventplus::Event& event, uint64_t durationInUsec,
          TimerEventPriority priority) :
        event(event),
        durationInUsec(durationInUsec), timerEventPriority(priority),
        rc(NSM_SW_SUCCESS) // Initialize the result code to success
    {}
};
} // namespace common
