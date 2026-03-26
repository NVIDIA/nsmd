/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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

/**
 * @file mockSdEvent.cpp
 * @brief Link-time replacements for the sd_event C API functions used by
 *        common::Sleep and common::CoroutineSemaphore.
 *
 * Including this file in a test executable overrides the real libsystemd
 * implementations and provides two capabilities:
 *
 *   1. Error injection for Sleep branch coverage:
 *      - sd_event_now and sd_event_add_time can be made to return -1 via the
 *        flags in mockSdEvent.hpp, exercising the NSM_SW_ERROR branches inside
 *        Sleep::await_suspend.
 *
 *   2. Synchronous sd_event_add_defer for CoroutineSemaphore coverage:
 *      - The deferred callback is fired inline so the contended-path inside
 *        CoroutineSemaphore::release() executes synchronously under gcovr.
 *        This is safe because CoroutineSemaphore::Awaiter::await_suspend
 *        returns void (no UB from resuming a frame mid-await_suspend).
 *
 * NOTE: sd_event_add_time deliberately does NOT fire its callback
 * synchronously. Sleep::await_suspend returns bool; resuming the coroutine
 * handle from within a bool-returning await_suspend would be UB if the frame
 * is destroyed before await_suspend returns true. The error-injection flags
 * (failNextSdEventAddTime) cover the relevant Sleep branches.
 *
 * Link usage: add this file as a source to the test executable in meson.build.
 * If the test also links against a system library that exports these symbols,
 * add '-Wl,--allow-multiple-definition' to link_args.
 */

#include "mockSdEvent.hpp"

#include <systemd/sd-event.h>

namespace nsm::test
{
bool failNextSdEventNow = false;
bool failNextSdEventAddTime = false;
bool failNextSdEventAddDefer = false;
} // namespace nsm::test

extern "C" int sd_event_now(sd_event* /*e*/, clockid_t /*clock*/,
                            uint64_t* usec)
{
    if (nsm::test::failNextSdEventNow)
    {
        nsm::test::failNextSdEventNow = false;
        return -1;
    }
    if (usec)
        *usec = 0;
    return 0;
}

extern "C" int sd_event_add_time(sd_event* /*e*/, sd_event_source** src,
                                 clockid_t /*clock*/, uint64_t /*usec*/,
                                 uint64_t /*accuracy*/,
                                 sd_event_time_handler_t /*callback*/,
                                 void* /*userdata*/)
{
    if (nsm::test::failNextSdEventAddTime)
    {
        nsm::test::failNextSdEventAddTime = false;
        return -1;
    }
    // Do not fire the callback synchronously: Sleep::await_suspend returns
    // bool true after this call. If the coroutine frame were destroyed by an
    // in-place resume before await_suspend returns, that would be UB.
    if (src)
        *src = nullptr;
    return 0;
}

// Called by Sleep::on_timeout to release the event source handle.
extern "C" sd_event_source* sd_event_source_unref(sd_event_source* /*s*/)
{
    return nullptr;
}

// Used by CoroutineSemaphore::release() to defer queue resumption.
// Safe to fire synchronously: Awaiter::await_suspend returns void, so the
// coroutine frame is stable when handle.resume() is called from here.
extern "C" int sd_event_add_defer(sd_event* /*e*/, sd_event_source** src,
                                  sd_event_handler_t callback, void* userdata)
{
    if (nsm::test::failNextSdEventAddDefer)
    {
        nsm::test::failNextSdEventAddDefer = false;
        return -1;
    }
    if (src)
        *src = nullptr;
    if (callback)
        callback(nullptr, userdata);
    return 0;
}
