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

/*
 * Tests for nsmd/nsmEvent/nsmLongRunningEventHandler.cpp
 *
 * NsmLongRunningEventHandler::handle() calls
 * mctp::MctpDiscovery::getInstance() which throws "MctpDiscovery instance is
 * not initialized yet" in the unit test environment. The !nsmDevice branch
 * is therefore blocked (the exception exits before the if-check is reached).
 *
 * See BLOCKED_TESTS.md for the blocked handle() branch-coverage tests.
 */

#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmEvent/nsmLongRunningEventHandler.hpp"

using namespace nsm;

// =============================================================================
// Constructor test
// =============================================================================

TEST(NsmLongRunningEventHandler, Constructor_DefaultState)
{
    NsmLongRunningEventHandler handler;
    EXPECT_EQ(handler.getName(), "NsmLongRunningEventHandler");
    EXPECT_EQ(handler.getType(), "NSM_LONG_RUNNING_EVENT_HANDLER");
}

// handle(): MctpDiscovery::getInstance() is not initialised in unit tests,
// so it throws std::runtime_error before the !nsmDevice check is reached.
// Exercising handle() entry improves line coverage for the function.
TEST(NsmLongRunningEventHandler, Handle_MctpDiscoveryNotInit_ThrowsRuntimeError)
{
    NsmLongRunningEventHandler handler;
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(eventData.data());

    EXPECT_THROW(
        handler.handle(0, NSM_TYPE_NETWORK_PORT, 0, msg, eventData.size()),
        std::runtime_error);
}
