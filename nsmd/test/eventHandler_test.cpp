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

#include "eventHandler.hpp"
#include "eventManager.hpp"

using namespace nsm;

class TestEventHandler : public EventHandler
{
  public:
    TestEventHandler()
    {
        // Register a test handler
        handlers[1] = [this](eid_t eid, NsmType type, NsmEventId eventId,
                             const nsm_msg* /*event*/, size_t /*eventLen*/) {
            handledEventId = eventId;
            handledEid = eid;
            handledType = type;
            handlerCalled = true;
        };
    }

    uint8_t nsmType() override
    {
        return NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    }

    void unsupportedEvent(uint8_t eid, const nsm_msg* /*event*/,
                          size_t /*eventLen*/) override
    {
        unsupportedEventCalled = true;
        unsupportedEventEid = eid;
    }

    bool handlerCalled = false;
    bool unsupportedEventCalled = false;
    NsmEventId handledEventId = 0;
    eid_t handledEid = 0;
    NsmType handledType = 0;
    eid_t unsupportedEventEid = 0;
};

struct EventHandlerTest : public Test, public utils::DBusTest
{
    std::unique_ptr<TestEventHandler> handler;

    void SetUp() override
    {
        handler = std::make_unique<TestEventHandler>();
    }
};

TEST_F(EventHandlerTest, testHandleSupportedEvent)
{
    eid_t testEid = 10;
    NsmType testType = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    NsmEventId testEventId = 1;

    // Create a minimal NSM event message
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + sizeof(nsm_event));
    auto event = reinterpret_cast<nsm_msg*>(eventData.data());
    event->hdr.nvidia_msg_type = testType;
    event->hdr.instance_id = 0;

    handler->handle(testEid, testType, testEventId, event, eventData.size());

    EXPECT_TRUE(handler->handlerCalled);
    EXPECT_EQ(handler->handledEventId, testEventId);
    EXPECT_EQ(handler->handledEid, testEid);
    EXPECT_EQ(handler->handledType, testType);
    EXPECT_FALSE(handler->unsupportedEventCalled);
}

TEST_F(EventHandlerTest, testHandleUnsupportedEvent)
{
    eid_t testEid = 20;
    NsmType testType = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    NsmEventId testEventId = 99; // Unregistered event ID

    // Create a minimal NSM event message
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + sizeof(nsm_event) + 2);
    auto event = reinterpret_cast<nsm_msg*>(eventData.data());
    event->hdr.nvidia_msg_type = testType;
    event->hdr.instance_id = 0;
    event->payload[0] = 0;
    event->payload[1] = testEventId;

    handler->handle(testEid, testType, testEventId, event, eventData.size());

    EXPECT_FALSE(handler->handlerCalled);
    EXPECT_TRUE(handler->unsupportedEventCalled);
    EXPECT_EQ(handler->unsupportedEventEid, testEid);
}

TEST_F(EventHandlerTest, testNsmType)
{
    EXPECT_EQ(handler->nsmType(), NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY);
}

struct EventManagerTest : public Test, public utils::DBusTest
{
    EventManager manager;

    void SetUp() override {}
};

TEST_F(EventManagerTest, testRegisterHandler)
{
    auto handler = std::make_unique<TestEventHandler>();
    NsmType testType = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;

    manager.registerHandler(testType, std::move(handler));

    EXPECT_EQ(manager.evenTypeHandlers.size(), 1);
    EXPECT_NE(manager.evenTypeHandlers.find(testType),
              manager.evenTypeHandlers.end());
}

TEST_F(EventManagerTest, testHandleWithRegisteredHandler)
{
    auto testHandler = std::make_unique<TestEventHandler>();
    auto* handlerPtr = testHandler.get();
    NsmType testType = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    NsmEventId testEventId = 1;
    eid_t testEid = 15;

    manager.registerHandler(testType, std::move(testHandler));

    // Create a minimal NSM event message
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + sizeof(nsm_event));
    auto event = reinterpret_cast<nsm_msg*>(eventData.data());
    event->hdr.nvidia_msg_type = testType;
    event->hdr.instance_id = 5;
    auto eventPayload = reinterpret_cast<nsm_event*>(event->payload);
    eventPayload->ackr = 0; // No acknowledgement required

    auto response = manager.handle(testEid, testType, testEventId, event,
                                   eventData.size());

    EXPECT_TRUE(handlerPtr->handlerCalled);
    EXPECT_FALSE(response.has_value());
}

TEST_F(EventManagerTest, testHandleWithAcknowledgement)
{
    auto testHandler = std::make_unique<TestEventHandler>();
    NsmType testType = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    NsmEventId testEventId = 1;
    eid_t testEid = 15;
    uint8_t instanceId = 7;

    manager.registerHandler(testType, std::move(testHandler));

    // Create a minimal NSM event message with ack required
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + sizeof(nsm_event));
    auto event = reinterpret_cast<nsm_msg*>(eventData.data());
    event->hdr.nvidia_msg_type = testType;
    event->hdr.instance_id = instanceId;
    auto eventPayload = reinterpret_cast<nsm_event*>(event->payload);
    eventPayload->ackr = 1; // Acknowledgement required

    auto response = manager.handle(testEid, testType, testEventId, event,
                                   eventData.size());

    EXPECT_TRUE(response.has_value());
    EXPECT_GT(response->size(), 0);
}

TEST_F(EventManagerTest, testHandleUnregisteredType)
{
    NsmType testType = NSM_TYPE_NETWORK_PORT;
    NsmEventId testEventId = 1;
    eid_t testEid = 25;

    // Create a minimal NSM event message
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + sizeof(nsm_event));
    auto event = reinterpret_cast<nsm_msg*>(eventData.data());
    event->hdr.nvidia_msg_type = testType;

    auto response = manager.handle(testEid, testType, testEventId, event,
                                   eventData.size());

    EXPECT_FALSE(response.has_value());
}

TEST_F(EventManagerTest, testAckEvent)
{
    uint8_t instanceId = 10;
    uint8_t nsmType = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    uint8_t eventId = 5;

    auto ackMsg = manager.ackEvent(instanceId, nsmType, eventId);

    EXPECT_TRUE(ackMsg.has_value());
    EXPECT_EQ(ackMsg->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_event_ack));

    // Verify the ack message structure
    auto msg = reinterpret_cast<const nsm_msg*>(ackMsg->data());
    EXPECT_EQ(msg->hdr.instance_id, instanceId);
    EXPECT_EQ(msg->hdr.nvidia_msg_type, nsmType);
}

TEST_F(EventManagerTest, testMultipleHandlerRegistration)
{
    auto handler0 = std::make_unique<TestEventHandler>();
    auto handler1 = std::make_unique<TestEventHandler>();

    manager.registerHandler(NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                            std::move(handler0));
    manager.registerHandler(NSM_TYPE_NETWORK_PORT, std::move(handler1));

    EXPECT_EQ(manager.evenTypeHandlers.size(), 2);
}

// =============================================================================
// Base EventHandler::unsupportedEvent() coverage tests
//
// TestEventHandler overrides unsupportedEvent(), so the base D-Bus logging
// implementation (lines 71-113 in eventHandler.hpp) is never reached by the
// tests above.  The class below does NOT override unsupportedEvent(), so the
// base implementation runs.  In unit tests without a real D-Bus daemon,
// getService() throws a std::exception that is caught on line 95, exercising
// the exception path (lines 95-100) and the surrounding code.
// =============================================================================

class BaseUnsupportedEventHandler : public EventHandler
{
  public:
    uint8_t nsmType() override
    {
        return NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    }
    // unsupportedEvent() NOT overridden → base D-Bus logging implementation
    // used
};

TEST_F(EventHandlerTest, testBaseUnsupportedEvent_ExceptionPath)
{
    BaseUnsupportedEventHandler baseHandler;

    // Build a minimal NSM event message; payload[1] holds the event ID.
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + sizeof(nsm_event) + 2,
                                   0);
    auto* event = reinterpret_cast<nsm_msg*>(eventData.data());
    event->hdr.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    event->payload[1] = 42; // unregistered eventId

    // handle() dispatches to base unsupportedEvent() because eventId=42 is
    // not in handlers.  The base implementation tries getService() which
    // throws in unit-test environment; the catch block (lines 95-100) handles
    // it so handle() must NOT propagate the exception.
    EXPECT_NO_THROW(baseHandler.handle(5, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                       42, event, eventData.size()));
}
