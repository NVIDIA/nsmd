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

#pragma once

/**
 * @file mockSdEvent.hpp
 * @brief Error-injection flags for the sd_event link-time mock
 * (mockSdEvent.cpp).
 *
 * Include this header in test files that need to control the behaviour of
 * sd_event_now, sd_event_add_time, or sd_event_add_defer.
 *
 * Usage:
 *   nsm::test::failNextSdEventNow = true;    // next sd_event_now  → returns -1
 *   nsm::test::failNextSdEventAddTime = true; // next sd_event_add_time →
 * returns -1 nsm::test::failNextSdEventAddDefer = true; // next
 * sd_event_add_defer → returns -1
 *
 * Each flag is automatically reset to false after the next call to the
 * corresponding function, regardless of whether the call was a success or a
 * failure.
 */

namespace nsm::test
{
extern bool failNextSdEventNow;
extern bool failNextSdEventAddTime;
extern bool failNextSdEventAddDefer;
} // namespace nsm::test
