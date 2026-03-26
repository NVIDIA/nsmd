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

#include "common/event.hpp"

namespace common
{

Event::~Event() = default;

int Event::now(clockid_t clock, uint64_t* usec) const
{
    return ::sd_event_now(static_cast<const sdeventplus::Event*>(this)->get(),
                          clock, usec);
}

int Event::addTime(sd_event_source** ret, clockid_t clock, uint64_t usec,
                   uint64_t accuracy, sd_event_time_handler_t callback,
                   void* userdata) const
{
    return ::sd_event_add_time(
        static_cast<const sdeventplus::Event*>(this)->get(), ret, clock, usec,
        accuracy, callback, userdata);
}

int Event::addDefer(sd_event_source** ret, sd_event_handler_t callback,
                    void* userdata) const
{
    return ::sd_event_add_defer(
        static_cast<const sdeventplus::Event*>(this)->get(), ret, callback,
        userdata);
}

sd_event_source* Event::sourceUnref(sd_event_source* s) const
{
    return ::sd_event_source_unref(s);
}

} // namespace common
