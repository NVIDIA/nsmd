/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "common/event.hpp"

#include <gmock/gmock.h>

namespace common::test
{

class MockEvent : public common::Event
{
  public:
    using common::Event::Event;

    MOCK_METHOD(int, now, (clockid_t clock, uint64_t* usec), (const, override));
    MOCK_METHOD(int, addTime,
                (sd_event_source * *ret, clockid_t clock, uint64_t usec,
                 uint64_t accuracy, sd_event_time_handler_t callback,
                 void* userdata),
                (const, override));
    MOCK_METHOD(int, addDefer,
                (sd_event_source * *ret, sd_event_handler_t callback,
                 void* userdata),
                (const, override));
    MOCK_METHOD(sd_event_source*, sourceUnref, (sd_event_source * s),
                (const, override));
};

} // namespace common::test
