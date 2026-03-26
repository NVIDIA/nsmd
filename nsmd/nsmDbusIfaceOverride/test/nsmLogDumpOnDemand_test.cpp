/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

// Include standard/sdbusplus/gtest headers BEFORE defining private/protected
// macros. GCC 15 rejects re-declarations of private nested structs inside
// template bodies (e.g. std::any) when the 'private' keyword is redefined.
// Pre-including these headers ensures they are processed without the macro.
#include <sdbusplus/bus.hpp>

#include <any>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmLogDumpOnDemand.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

// Fixture that resets the NsmLogDumpTracker singleton instance pointer
// before/after each test so branches can be exercised independently.
struct NsmLogDumpTrackerTest : public ::testing::Test
{
    void SetUp() override
    {
        NsmLogDumpTracker::instance = nullptr;
    }

    void TearDown() override
    {
        NsmLogDumpTracker::instance = nullptr;
    }
};

// getInstance() when instance == nullptr must throw std::runtime_error //

TEST_F(NsmLogDumpTrackerTest, GetInstance_NotInitialized_Throws)
{
    EXPECT_THROW(NsmLogDumpTracker::getInstance(), std::runtime_error);
}

// initialize() when instance == nullptr must succeed and getInstance() returns
// a valid reference (no throw, TRUE branch of `if (!instance)` in getInstance)
TEST_F(NsmLogDumpTrackerTest, Initialize_ThenGetInstance_Succeeds)
{
    ASSERT_NO_THROW(NsmLogDumpTracker::initialize(bus, "/test/logdump/first"));
    EXPECT_NO_THROW(NsmLogDumpTracker::getInstance());
    // Returned reference must be the same object on repeated calls
    NsmLogDumpTracker& ref1 = NsmLogDumpTracker::getInstance();
    NsmLogDumpTracker& ref2 = NsmLogDumpTracker::getInstance();
    EXPECT_EQ(&ref1, &ref2);
}

// initialize() when instance != nullptr must throw std::logic_error //
// (TRUE branch of `if (instance)` in initialize)
TEST_F(NsmLogDumpTrackerTest, Initialize_WhenAlreadyInitialized_Throws)
{
    // First initialization sets instance
    ASSERT_NO_THROW(NsmLogDumpTracker::initialize(bus, "/test/logdump/first"));
    EXPECT_NE(NsmLogDumpTracker::instance, nullptr);

    // Second initialization without resetting must throw
    EXPECT_THROW(NsmLogDumpTracker::initialize(bus, "/test/logdump/second"),
                 std::logic_error);
}
