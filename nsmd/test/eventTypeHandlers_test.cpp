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

using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-capability-discovery.h"
#include "network-ports.h"
#include "platform-environmental.h"

#include "eventTypeHandlers.hpp"

using namespace nsm;

struct EventType0HandlerTest : public Test, public utils::DBusTest
{
    std::unique_ptr<EventType0Handler> handler;

    void SetUp() override
    {
        handler = std::make_unique<EventType0Handler>();
    }
};

TEST_F(EventType0HandlerTest, testConstructor)
{
    EXPECT_NE(handler, nullptr);
}

TEST_F(EventType0HandlerTest, testNsmType)
{
    EXPECT_EQ(handler->nsmType(), NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
}

TEST_F(EventType0HandlerTest, testDelegatedEvents)
{
    // Verify that rediscovery event is registered
    EXPECT_NE(handler->handlers.find(NSM_REDISCOVERY_EVENT),
              handler->handlers.end());

    // Verify that long running event is registered
    EXPECT_NE(handler->handlers.find(NSM_LONG_RUNNING_EVENT),
              handler->handlers.end());
}

struct EventType1HandlerTest : public Test, public utils::DBusTest
{
    std::unique_ptr<EventType1Handler> handler;

    void SetUp() override
    {
        handler = std::make_unique<EventType1Handler>();
    }
};

TEST_F(EventType1HandlerTest, testConstructor)
{
    EXPECT_NE(handler, nullptr);
}

TEST_F(EventType1HandlerTest, testNsmType)
{
    EXPECT_EQ(handler->nsmType(), NSM_TYPE_NETWORK_PORT);
}

TEST_F(EventType1HandlerTest, testDelegatedEvents)
{
    // Verify that threshold event is registered
    EXPECT_NE(handler->handlers.find(NSM_THRESHOLD_EVENT),
              handler->handlers.end());

    // Verify that fabric manager state event is registered
    EXPECT_NE(handler->handlers.find(NSM_FABRIC_MANAGER_STATE_EVENT),
              handler->handlers.end());
}

struct EventType3HandlerTest : public Test, public utils::DBusTest
{
    std::unique_ptr<EventType3Handler> handler;

    void SetUp() override
    {
        handler = std::make_unique<EventType3Handler>();
    }
};

TEST_F(EventType3HandlerTest, testConstructor)
{
    EXPECT_NE(handler, nullptr);
}

TEST_F(EventType3HandlerTest, testNsmType)
{
    EXPECT_EQ(handler->nsmType(), NSM_TYPE_PLATFORM_ENVIRONMENTAL);
}

TEST_F(EventType3HandlerTest, testDelegatedEvents)
{
    // Verify that XID event is registered
    EXPECT_NE(handler->handlers.find(NSM_XID_EVENT), handler->handlers.end());

    // Verify that reset required event is registered
    EXPECT_NE(handler->handlers.find(NSM_RESET_REQUIRED_EVENT),
              handler->handlers.end());
}

// DelegatingEventHandler::enableDelegation() - duplicate handler
// → NSM_SW_ERROR_DATA
TEST_F(EventType0HandlerTest, EnableDelegation_Duplicate_ReturnsError)
{
    // NSM_REDISCOVERY_EVENT is already registered by the constructor.
    // Calling enableDelegation() again with the same id must fail.
    int rc = handler->enableDelegation(NSM_REDISCOVERY_EVENT);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}
