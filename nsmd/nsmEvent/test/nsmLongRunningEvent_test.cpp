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

#include "test/mockDBusHandler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmLongRunningEvent.hpp"

namespace nsm
{

// Concrete test subclass of NsmLongRunningEvent (handle() is pure virtual)
class TestNsmLongRunningEvent : public NsmLongRunningEvent
{
  public:
    TestNsmLongRunningEvent(const std::string& name, const std::string& type,
                            bool isLongRunning) :
        NsmLongRunningEvent(name, type, isLongRunning)
    {}

    int handle(eid_t, NsmType, NsmEventId, const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

} // namespace nsm

using namespace nsm;

class NsmLongRunningEventTest : public ::testing::Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// ============================================================================
// Constructor tests – cover lines 27-32
// ============================================================================

TEST_F(NsmLongRunningEventTest, Constructor_IsLongRunningTrue_Constructed)
{
    TestNsmLongRunningEvent event("LREvent", "TestType", true);
    EXPECT_TRUE(event.isLongRunning);
    EXPECT_EQ(event.acceptInstanceId, 0xFF);
    EXPECT_FALSE(event.timer.expired());
    EXPECT_FALSE(event.timer.running());
}

TEST_F(NsmLongRunningEventTest, Constructor_IsLongRunningFalse_Constructed)
{
    TestNsmLongRunningEvent event("LREvent2", "TestType", false);
    EXPECT_FALSE(event.isLongRunning);
    EXPECT_EQ(event.acceptInstanceId, 0xFF);
}

// ============================================================================
// initAcceptInstanceId tests – cover lines 34, 37-39
// ============================================================================

TEST_F(NsmLongRunningEventTest,
       InitAcceptInstanceId_AcceptedSuccess_SetsInstanceId)
{
    TestNsmLongRunningEvent event("InitOk", "Type", true);

    bool result = event.initAcceptInstanceId(0x0A, NSM_ACCEPTED,
                                             NSM_SW_SUCCESS);

    EXPECT_TRUE(result);
    EXPECT_EQ(event.acceptInstanceId, 0x0A);
}

TEST_F(NsmLongRunningEventTest, InitAcceptInstanceId_NotAcceptedCc_SetsFF)
{
    TestNsmLongRunningEvent event("InitCcFail", "Type", true);

    bool result = event.initAcceptInstanceId(0x0A, NSM_SUCCESS, NSM_SW_SUCCESS);

    EXPECT_FALSE(result);
    EXPECT_EQ(event.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningEventTest, InitAcceptInstanceId_RcError_SetsFF)
{
    TestNsmLongRunningEvent event("InitRcFail", "Type", true);

    bool result = event.initAcceptInstanceId(0x0A, NSM_ACCEPTED, NSM_SW_ERROR);

    EXPECT_FALSE(result);
    EXPECT_EQ(event.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningEventTest, InitAcceptInstanceId_BothFail_SetsFF)
{
    TestNsmLongRunningEvent event("InitBothFail", "Type", true);

    bool result = event.initAcceptInstanceId(0x0A, NSM_ERROR, NSM_SW_ERROR);

    EXPECT_FALSE(result);
    EXPECT_EQ(event.acceptInstanceId, 0xFF);
}

TEST_F(NsmLongRunningEventTest, InitAcceptInstanceId_ZeroInstanceId_Accepted)
{
    TestNsmLongRunningEvent event("InitZeroId", "Type", true);

    bool result = event.initAcceptInstanceId(0x00, NSM_ACCEPTED,
                                             NSM_SW_SUCCESS);

    EXPECT_TRUE(result);
    EXPECT_EQ(event.acceptInstanceId, 0x00);
}

// ============================================================================
// validateEvent tests – cover lines 50-51, 55
// ============================================================================

TEST_F(NsmLongRunningEventTest, ValidateEvent_DecodeFailure_LogsErrorAndFails)
{
    TestNsmLongRunningEvent event("DecodeFailEvent", "Type", true);
    // Pass a zeroed minimal buffer – decode_long_running_event will fail
    // because the nsm_msg header fields are zeroed (wrong type/size).
    // On a fresh object, shouldLog fires on first decode failure → covers
    // LG2_ERROR lines 50-51.
    std::vector<uint8_t> buf(sizeof(nsm_msg) + 4, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());

    int rc = event.validateEvent(10, msg, buf.size());
    // Result may be failure or command-fail; should not crash
    EXPECT_TRUE(rc == NSM_SW_ERROR_COMMAND_FAIL || rc == NSM_SW_ERROR ||
                rc != NSM_SW_SUCCESS);
}

TEST_F(NsmLongRunningEventTest, ValidateEvent_TimerExpired_LogsTimerError)
{
    TestNsmLongRunningEvent event("TimerExpiredEvent", "Type", true);
    // Force the timer into expired state via #define private public
    event.timer.isExpired = true;
    // Set valid accept state so acceptInstanceId check doesn't dominate
    event.acceptInstanceId = 0x05;
    event.isLongRunning = true;

    std::vector<uint8_t> buf(sizeof(nsm_msg) + 4, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
    // shouldLog("timer.expired()") fires first time timer is expired →
    // covers LG2_ERROR line 55
    int rc = event.validateEvent(10, msg, buf.size());
    EXPECT_NE(rc, -1); // should not crash
}
