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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#define private public
#define protected public

#include "nsmServiceReadyInterface.hpp"

using namespace nsm;

// A test bus; sdbusplus::bus::new_default() creates a real bus connection.
// The ServiceReadyIntf D-Bus objects created during tests are registered on
// this bus.  In the Docker test environment a D-Bus session is available.
static auto bus = sdbusplus::bus::new_default();

// =============================================================================
// Direct-instance tests (bypass the singleton to avoid cross-test ordering
// dependencies and cover isStateEnabled/setStateEnabled/setStateStarting).
// =============================================================================

// NsmServiceReadyIntf private constructor is accessible via #define private
// public.  Each test creates its own instance at a unique D-Bus path.
TEST(NsmServiceReadyIntfDirect, DefaultState_IsStarting_NotEnabled)
{
    NsmDeviceTable devices;
    NsmServiceReadyIntf inst(bus, "/test/svc_rdy/default_state", devices);

    // Constructor sets state to Starting → isStateEnabled() must return false
    EXPECT_FALSE(inst.isStateEnabled());
}

TEST(NsmServiceReadyIntfDirect, SetStateEnabled_IsStateEnabled_ReturnsTrue)
{
    NsmDeviceTable devices;
    NsmServiceReadyIntf inst(bus, "/test/svc_rdy/set_enabled", devices);

    inst.setStateEnabled();
    EXPECT_TRUE(inst.isStateEnabled());
}

TEST(NsmServiceReadyIntfDirect, SetStateStarting_AfterEnabled_ReturnsFalse)
{
    NsmDeviceTable devices;
    NsmServiceReadyIntf inst(bus, "/test/svc_rdy/set_starting", devices);

    inst.setStateEnabled();
    EXPECT_TRUE(inst.isStateEnabled());

    inst.setStateStarting();
    EXPECT_FALSE(inst.isStateEnabled());
}

// =============================================================================
// Singleton tests (getInstance, initialize, double-initialize).
// Each test manipulates the static instance pointer directly.
// Tests within this suite run in declaration order (TEST not TEST_F), so the
// binary-unique singleton lifecycle is handled explicitly.
// =============================================================================

// getInstance() when the singleton is not yet initialized must throw
// std::runtime_error (line 42-43 in nsmServiceReadyInterface.hpp).
TEST(NsmServiceReadyIntfSingleton,
     GetInstance_NotInitialized_ThrowsRuntimeError)
{
    auto* saved = NsmServiceReadyIntf::instance;
    NsmServiceReadyIntf::instance = nullptr;

    EXPECT_THROW(NsmServiceReadyIntf::getInstance(), std::runtime_error);

    NsmServiceReadyIntf::instance = saved; // restore for following tests
}

// initialize() when instance == nullptr must succeed (lines 57-58).
TEST(NsmServiceReadyIntfSingleton, Initialize_WhenNull_Succeeds)
{
    NsmServiceReadyIntf::instance = nullptr;
    NsmDeviceTable devices;

    EXPECT_NO_THROW(NsmServiceReadyIntf::initialize(
        bus, "/test/svc_rdy/singleton", devices));
    EXPECT_NE(NsmServiceReadyIntf::instance, nullptr);
}

// initialize() a second time (while instance != nullptr) must throw
// std::logic_error (lines 52-55).
TEST(NsmServiceReadyIntfSingleton,
     Initialize_AlreadyInitialized_ThrowsLogicError)
{
    // instance was set by the previous test; calling initialize() again throws
    NsmDeviceTable devices;
    EXPECT_THROW(NsmServiceReadyIntf::initialize(bus, "/test/svc_rdy/duplicate",
                                                 devices),
                 std::logic_error);
}

// getInstance() when the singleton has been initialized must return the
// instance and NOT throw (line 45).
TEST(NsmServiceReadyIntfSingleton, GetInstance_WhenInitialized_ReturnsRef)
{
    // instance was set by Initialize_WhenNull_Succeeds
    ASSERT_NE(NsmServiceReadyIntf::instance, nullptr);
    EXPECT_NO_THROW((void)NsmServiceReadyIntf::getInstance());
    EXPECT_EQ(&NsmServiceReadyIntf::getInstance(),
              NsmServiceReadyIntf::instance);
}

// State operations through the singleton path.
TEST(NsmServiceReadyIntfSingleton, Singleton_StateOperations)
{
    ASSERT_NE(NsmServiceReadyIntf::instance, nullptr);
    auto& inst = NsmServiceReadyIntf::getInstance();

    // Set enabled and verify
    inst.setStateEnabled();
    EXPECT_TRUE(inst.isStateEnabled());

    // Set starting and verify
    inst.setStateStarting();
    EXPECT_FALSE(inst.isStateEnabled());
}
