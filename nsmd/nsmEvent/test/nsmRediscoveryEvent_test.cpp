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
 * Tests for nsmd/nsmEvent/nsmRediscoveryEvent.cpp
 *
 *   - NsmRediscoveryEvent constructor (no messageArgs)
 *   - NsmRediscoveryEvent constructor (one messageArg)
 *   - NsmRediscoveryEvent constructor (multiple messageArgs joined with comma)
 *   - NsmRediscoveryEvent constructor (logging=false does not change eventData)
 *   - NsmRediscoveryEvent::handle (decode failure → returns error)
 *
 * NOTE: NsmRediscoveryEvent::handle() success path requires
 * mctp::MctpDiscovery to be initialized (deleted constructor singleton),
 * so only the decode-failure path is tested here.
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "device-capability-discovery.h"

#include "nsmEvent/nsmRediscoveryEvent.hpp"

using namespace nsm;

// Build a valid NsmEventInfo for NsmRediscoveryEvent
static NsmEventInfo makeRediscoveryInfo(bool logging = false)
{
    NsmEventInfo info{};
    info.messageId = "ResourceEvent.1.0.ResourceChanged";
    info.originOfCondition =
        "/xyz/openbmc_project/inventory/system/chassis/GPU_1";
    info.loggingNamespace = "GPU_Rediscovery";
    info.resolution = "No action required";
    info.messageArgs = {"GPU_1"};
    info.severity = Level::Informational;
    info.logging = logging;
    return info;
}

// =============================================================================
// Constructor tests
// =============================================================================

TEST(NsmRediscoveryEvent, Constructor_NoMessageArgs_EmptyMessageArgs)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs.clear();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.getName(), "rediscovery");
    EXPECT_EQ(event.getType(), "NSM_Rediscovery");
    EXPECT_EQ(event.messageArgs, "");
}

TEST(NsmRediscoveryEvent, Constructor_OneMessageArg_MessageArgsSet)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"GPU_1"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "GPU_1");
}

TEST(NsmRediscoveryEvent, Constructor_MultipleMessageArgs_JoinedWithComma)
{
    auto info = makeRediscoveryInfo();
    info.messageArgs = {"arg1", "arg2", "arg3"};
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.messageArgs, "arg1,arg2,arg3");
}

TEST(NsmRediscoveryEvent, Constructor_EventDataContainsExpectedKeys)
{
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    EXPECT_EQ(event.eventData.count("REDFISH_ORIGIN_OF_CONDITION"), 1u);
    EXPECT_EQ(event.eventData.count("REDFISH_MESSAGE_ARGS"), 1u);
    EXPECT_EQ(event.eventData.count("REDFISH_MESSAGE_ID"), 1u);
    EXPECT_EQ(event.eventData.count("namespace"), 1u);
    EXPECT_EQ(
        event.eventData.count("xyz.openbmc_project.Logging.Entry.Resolution"),
        1u);
}

// =============================================================================
// handle – decode failure (buffer too small)
// =============================================================================

TEST(NsmRediscoveryEvent, Handle_ShortBuffer_ReturnsError)
{
    auto info = makeRediscoveryInfo();
    NsmRediscoveryEvent event("rediscovery", "NSM_Rediscovery", info);

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = event.handle(5, {}, {}, msg, buf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}
