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
 * Tests for nsmd/nsmEvent/nsmResetRequiredEvent.cpp
 *
 *   - NsmResetRequiredEvent constructor (no messageArgs)
 *   - NsmResetRequiredEvent constructor (one messageArg)
 *   - NsmResetRequiredEvent constructor (multiple messageArgs joined with
 * comma)
 *   - NsmResetRequiredEvent constructor (errorId key found → adds EventId)
 *   - NsmResetRequiredEvent constructor (impactedComponent → adds DEVICE_NAME)
 *   - NsmResetRequiredEvent::handle (decode failure → returns error)
 *   - NsmResetRequiredEvent::handle (success → returns NSM_SW_SUCCESS)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "platform-environmental.h"

#include "nsmEvent/nsmResetRequiredEvent.hpp"

using namespace nsm;

// Build a valid NsmEventInfo for NsmResetRequiredEvent
static NsmEventInfo makeResetRequiredInfo()
{
    NsmEventInfo info{};
    info.messageId = "ResourceEvent.1.0.ResourceChanged";
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.loggingNamespace = "GPU_Reset";
    info.resolution = "Reboot the system";
    info.messageArgs = {"GPU_1"};
    info.severity = Level::Critical;
    return info;
}

// Encode a reset required event into a buffer
static std::vector<uint8_t> buildResetRequiredEvent()
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_nsm_reset_required_event(0, false, msg);
    return buf;
}

// =============================================================================
// Constructor tests
// =============================================================================

TEST(NsmResetRequiredEvent, Constructor_NoMessageArgs_EmptyMessageArgs)
{
    auto info = makeResetRequiredInfo();
    info.messageArgs.clear();
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.getName(), "reset_event");
    EXPECT_EQ(event.getType(), "NSM_ResetRequired");
    EXPECT_EQ(event.messageArgs, "");
}

TEST(NsmResetRequiredEvent, Constructor_OneMessageArg_MessageArgsSet)
{
    auto info = makeResetRequiredInfo();
    info.messageArgs = {"GPU_1"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
}

TEST(NsmResetRequiredEvent, Constructor_MultipleMessageArgs_JoinedWithComma)
{
    auto info = makeResetRequiredInfo();
    info.messageArgs = {"GPU_1", "arg2", "arg3"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.messageArgs, "GPU_1,arg2,arg3");
}

TEST(NsmResetRequiredEvent, Constructor_ErrorIdFound_AddsEventId)
{
    auto info = makeResetRequiredInfo();
    // getEventErrorId looks for "ResetRequired" key in pairs
    info.errorId = {"ResetRequired", "ERR-1234", "OtherKey", "OTHER-999"};
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto it = event.eventData.find("xyz.openbmc_project.Logging.Entry.EventId");
    ASSERT_NE(it, event.eventData.end());
    EXPECT_EQ(it->second, "ERR-1234");
}

TEST(NsmResetRequiredEvent, Constructor_ImpactedComponent_AddsDeviceName)
{
    auto info = makeResetRequiredInfo();
    info.impactedComponent = "GPU_1_Component";
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto it = event.eventData.find("DEVICE_NAME");
    ASSERT_NE(it, event.eventData.end());
    EXPECT_EQ(it->second, "GPU_1_Component");
}

TEST(NsmResetRequiredEvent, Constructor_NoImpactedComponent_NoDeviceName)
{
    auto info = makeResetRequiredInfo();
    info.impactedComponent = "";
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    EXPECT_EQ(event.eventData.count("DEVICE_NAME"), 0u);
}

// =============================================================================
// handle – decode failure (buffer too small)
// =============================================================================

TEST(NsmResetRequiredEvent, Handle_ShortBuffer_ReturnsError)
{
    auto info = makeResetRequiredInfo();
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle – success: decoded event, async logging detached
// =============================================================================

TEST(NsmResetRequiredEvent, Handle_ValidEvent_ReturnsSuccess)
{
    auto info = makeResetRequiredInfo();
    NsmResetRequiredEvent event("reset_event", "NSM_ResetRequired", info);

    auto buf = buildResetRequiredEvent();
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
