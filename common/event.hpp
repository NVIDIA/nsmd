/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
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

#include <systemd/sd-event.h>

#include <sdeventplus/event.hpp>

namespace common
{

/**
 * @brief sdeventplus::Event subclass with virtual wrappers for selected
 *        libsystemd sd_event_* entry points.
 *
 * Production code calls the default implementations (defined in event.cpp),
 * which forward to the real C API. Unit tests may derive from common::Event
 * and override these methods (e.g. with gmock::NiceMock) instead of using
 * link-time mocks.
 *
 * Free-function overloads below take `const Event*` and are invoked as
 * `sd_event_now(event.get(), ...)` — ADL resolves to the common:: overload
 * when the argument is a pointer to common::Event. For the raw sd_event*
 * handle (e.g. attach_event), cast to sdeventplus::Event& and call get().
 */
class Event : public sdeventplus::Event
{
  public:
    using sdeventplus::Event::Event;

    /** @brief Default constructor — wraps sd_event_default(). */
    Event() : sdeventplus::Event(sdeventplus::Event::get_default()) {}

    /** @brief Converting constructor from sdeventplus::Event. */
    Event(const sdeventplus::Event& other) : sdeventplus::Event(other) {}
    Event(sdeventplus::Event&& other) : sdeventplus::Event(std::move(other)) {}

    virtual ~Event();

    /** @brief Pointer to this object for virtual sd_event_* hook overloads. */
    const Event* get() const noexcept
    {
        return this;
    }

    /** @brief Virtual hook for sd_event_now(). */
    virtual int now(clockid_t clock, uint64_t* usec) const;

    /** @brief Virtual hook for sd_event_add_time(). */
    virtual int addTime(sd_event_source** ret, clockid_t clock, uint64_t usec,
                        uint64_t accuracy, sd_event_time_handler_t callback,
                        void* userdata) const;

    /** @brief Virtual hook for sd_event_add_defer(). */
    virtual int addDefer(sd_event_source** ret, sd_event_handler_t callback,
                         void* userdata) const;

    /** @brief Virtual hook for sd_event_source_unref(). */
    virtual sd_event_source* sourceUnref(sd_event_source* s) const;
};

// ---- Free-function overloads (ADL-discoverable) ----

inline int sd_event_now(const Event* e, clockid_t clock, uint64_t* usec)
{
    return e->now(clock, usec);
}

inline int sd_event_add_time(const Event* e, sd_event_source** ret,
                             clockid_t clock, uint64_t usec, uint64_t accuracy,
                             sd_event_time_handler_t callback, void* userdata)
{
    return e->addTime(ret, clock, usec, accuracy, callback, userdata);
}

inline int sd_event_add_defer(const Event* e, sd_event_source** ret,
                              sd_event_handler_t callback, void* userdata)
{
    return e->addDefer(ret, callback, userdata);
}

inline sd_event_source* sd_event_source_unref(const Event& e,
                                              sd_event_source* s)
{
    return e.sourceUnref(s);
}

} // namespace common
