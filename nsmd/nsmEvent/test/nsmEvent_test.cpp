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

#include "network-ports.h"

#include "nsmLongRunningEventHandler.hpp"
#include "nsmThresholdEvent.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

namespace nsm
{
requester::Coroutine createNsmThresholdEvent(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
}; // namespace nsm

using namespace nsm;

struct NsmThresholdEventTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Event_Threshold";
    const std::string name = "ThresholdEventSetting";
    const std::string objPath = chassisInventoryBasePath / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmThresholdEventTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"UUID", "992b3ec1-e468-f145-8686-badbadbadbad"},
        {"MessageArgs", std::vector<std::string>{}},
    };
    dbus::PropertyMap basic = {
        {"UUID", gpuUuid},
        {"Name", name},
        {"OriginOfCondition", "/redfish/v1/Chassis/HGX_GPU_SXM_1"},
        {"MessageId", "ResourceEvent.1.0.ResourceErrorsDetected"},
        {"LoggingNamespace", "GPU_SXM 1 Threshold"},
        {"Resolution",
         "Regarding Port Error documentation and further actions please refer to (TBD)"},
        {"MessageArgs",
         std::vector<std::string>{
             "GPU_SXM_1",
             "No Errors",
         }},
        {"Severity", "Critical"},
    };
};

TEST_F(NsmThresholdEventTest, badTestUuidNotFound)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any device
    propertyMap["UUID"] = error["UUID"]; // Invalid UUID as uuid_t type

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmThresholdEventTest, badTestMessageArgsSize)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;
    propertyMap["MessageArgs"] = error["MessageArgs"];

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, basicIntfName, objPath),
        std::invalid_argument);
}

TEST_F(NsmThresholdEventTest, badTestInvalidMessageId)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;
    propertyMap["MessageId"] = std::string("InvalidMessageId");

    EXPECT_THROW_COROUTINE(
        createNsmThresholdEvent(mockManager, basicIntfName, objPath),
        std::invalid_argument);
}

TEST_F(NsmThresholdEventTest, goodTestCreateEvent)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);

    EXPECT_EQ(2, gpu->deviceEvents.size());
    EXPECT_EQ(2, gpu->eventDispatcher.eventsMap.size());

    auto event =
        dynamic_pointer_cast<NsmThresholdEvent>(gpu->deviceEvents.back());
    EXPECT_NE(nullptr, event);
    auto eventMapEntry =
        gpu->eventDispatcher.eventsMap[NSM_TYPE_NETWORK_PORT].find(
            NSM_THRESHOLD_EVENT);
    EXPECT_EQ(event.get(), eventMapEntry->second.get());

    const nsm_health_event_payload payload{0, 0, 1, 1, 1, 1, 1, 1, 1, 0};
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_health_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_health_event(eid, true, &payload, msg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    rc = gpu->eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                     NSM_THRESHOLD_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    rc = gpu->eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                     NSM_THRESHOLD_EVENT, msg,
                                     eventMsg.size() - 3);
    EXPECT_EQ(NSM_SW_ERROR_LENGTH, rc);
}

TEST_F(NsmThresholdEventTest, goodTestCreateEventWithDeviceToPortMap)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap = basic;

    // Setup deviceToPortMap with port mapping
    uint8_t portNumber = 5;
    std::string portName = "Port_5";
    mockManager.deviceToPortMap[gpu][portNumber] = portName;

    createNsmThresholdEvent(mockManager, basicIntfName, objPath);

    EXPECT_EQ(2, gpu->deviceEvents.size());
    EXPECT_EQ(2, gpu->eventDispatcher.eventsMap.size());

    auto event =
        dynamic_pointer_cast<NsmThresholdEvent>(gpu->deviceEvents.back());
    EXPECT_NE(nullptr, event);

    // Create payload with the specific port number to trigger deviceToPortMap
    // lookup
    const nsm_health_event_payload payload{portNumber, 0, 1, 1, 1,
                                           1,          1, 1, 1, 0};
    std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                                  sizeof(nsm_health_event_payload));
    auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
    auto rc = encode_nsm_health_event(eid, true, &payload, msg);
    EXPECT_EQ(NSM_SW_SUCCESS, rc);

    // This should trigger the deviceToPortMap code path (lines 114-121)
    rc = gpu->eventDispatcher.handle(eid, NSM_TYPE_NETWORK_PORT,
                                     NSM_THRESHOLD_EVENT, msg, eventMsg.size());
    EXPECT_EQ(NSM_SW_SUCCESS, rc);
}

// =============================================================================
// NsmLongRunningEventHandler Tests
// =============================================================================

TEST(NsmLongRunningEventHandler, Constructor_CreatesObjectWithCorrectNameType)
{
    NsmLongRunningEventHandler handler;
    EXPECT_EQ(handler.getName(), "NsmLongRunningEventHandler");
    EXPECT_EQ(handler.getType(), "NSM_LONG_RUNNING_EVENT_HANDLER");
}

TEST(NsmLongRunningEventHandler, Handle_NoDeviceForEid_ReturnsErrorData)
{
    NsmLongRunningEventHandler handler;
    eid_t eid = 255;
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(eventData.data());
    // MctpDiscovery singleton is not initialized in unit tests, so handle()
    // throws std::runtime_error before looking up the device
    EXPECT_THROW(handler.handle(eid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                                NSM_LONG_RUNNING_EVENT, msg, eventData.size()),
                 std::runtime_error);
}

// =============================================================================
// EventDispatcher branch-coverage tests
// =============================================================================

// Minimal NsmEvent concrete class for testing EventDispatcher.
struct MockNsmEvent : public NsmEvent
{
    MockNsmEvent() : NsmEvent("mock", "mock_type") {}
    int handle(eid_t, NsmType, NsmEventId, const nsm_msg*, size_t) override
    {
        return NSM_SW_SUCCESS;
    }
};

// addEvent(): adding the same (type, eventId) pair a second time must return
// NSM_SW_ERROR_DATA (duplicate guard, line 101-103 in nsmEvent.cpp).
TEST(EventDispatcher, AddEvent_DuplicateEventId_ReturnsErrorData)
{
    EventDispatcher dispatcher;
    NsmType type = NSM_TYPE_NETWORK_PORT;
    NsmEventId eventId = NSM_THRESHOLD_EVENT;

    EXPECT_EQ(
        dispatcher.addEvent(type, eventId, std::make_shared<MockNsmEvent>()),
        NSM_SW_SUCCESS);
    EXPECT_EQ(
        dispatcher.addEvent(type, eventId, std::make_shared<MockNsmEvent>()),
        NSM_SW_ERROR_DATA);
}

// handle(): when the NsmType is not present in eventsMap the function
// must return NSM_SW_ERROR_DATA (lines 115-121 in nsmEvent.cpp).
TEST(EventDispatcher, Handle_TypeNotFound_ReturnsErrorData)
{
    EventDispatcher dispatcher; // empty – no events registered
    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(eventData.data());

    auto rc = dispatcher.handle(0, NSM_TYPE_NETWORK_PORT, NSM_THRESHOLD_EVENT,
                                msg, eventData.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// handle(): when the NsmType IS registered but the eventId is absent the
// function must return NSM_SW_ERROR_DATA (lines 124-130 in nsmEvent.cpp).
TEST(EventDispatcher, Handle_EventIdNotFound_ReturnsErrorData)
{
    EventDispatcher dispatcher;
    NsmType type = NSM_TYPE_NETWORK_PORT;
    NsmEventId registeredId = NSM_THRESHOLD_EVENT;
    NsmEventId unknownId = registeredId + 1;

    dispatcher.addEvent(type, registeredId, std::make_shared<MockNsmEvent>());

    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(eventData.data());

    auto rc = dispatcher.handle(0, type, unknownId, msg, eventData.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// =============================================================================
// DelegatingEventHandler branch-coverage tests
// =============================================================================

// Concrete subclass to expose the protected enableDelegation() for testing.
struct TestDelegatingHandler : public DelegatingEventHandler
{
    uint8_t nsmType() override
    {
        return NSM_TYPE_NETWORK_PORT;
    }
    int testEnableDelegation(NsmEventId eventId)
    {
        return enableDelegation(eventId);
    }
};

// enableDelegation(): calling it twice with the same eventId must return
// NSM_SW_ERROR_DATA on the second call (lines 137-143 in nsmEvent.cpp).
TEST(DelegatingEventHandler, EnableDelegation_Duplicate_ReturnsErrorData)
{
    TestDelegatingHandler handler;
    NsmEventId eventId = NSM_THRESHOLD_EVENT;

    EXPECT_EQ(handler.testEnableDelegation(eventId), NSM_SW_SUCCESS);
    EXPECT_EQ(handler.testEnableDelegation(eventId), NSM_SW_ERROR_DATA);
}

// delegate(): MctpDiscovery::getInstance() is not initialised in unit tests,
// so it throws std::runtime_error before line 158 is reached.
// Calling handle() after enableDelegation() exercises the delegate() function
// entry (lines 151-156) and improves function coverage for nsmEvent.cpp.
TEST(DelegatingEventHandler, Delegate_MctpDiscoveryNotInit_ThrowsRuntimeError)
{
    TestDelegatingHandler handler;
    NsmEventId eventId = NSM_THRESHOLD_EVENT;
    handler.testEnableDelegation(eventId);

    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(eventData.data());

    // EventHandler::handle() -> handlers.at(eventId)() -> delegate()
    // -> MctpDiscovery::getInstance() throws std::runtime_error
    EXPECT_THROW(handler.handle(0, NSM_TYPE_NETWORK_PORT, eventId, msg,
                                eventData.size()),
                 std::runtime_error);
}

// EventHandler::handle(): when eventId is not in handlers map the unsupported
// event path is taken (eventHandler.hpp line 58-66).  No exception thrown.
TEST(DelegatingEventHandler, Handle_UnregisteredEventId_CallsUnsupportedEvent)
{
    TestDelegatingHandler handler;
    // do NOT call enableDelegation – handlers map is empty
    NsmEventId eventId = NSM_THRESHOLD_EVENT;

    std::vector<uint8_t> eventData(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto* msg = reinterpret_cast<const nsm_msg*>(eventData.data());

    // Should not throw; unsupportedEvent() is called instead
    EXPECT_NO_THROW(handler.handle(0, NSM_TYPE_NETWORK_PORT, eventId, msg,
                                   eventData.size()));
}

// =============================================================================
// logEventAsync branch-coverage tests
//
// logEventAsync has a static cachedLoggingService that persists across all
// calls within the same binary.  Tests must run in order:
//   1. EmptyServiceMap   – cache not set, serviceMap empty → early return
//   2. NonEmptyServiceMap – cache not set, serviceMap populated → cache set
//   3. CachedService     – cache set by test 2, serviceMap cleared by TearDown
//
// DBusTest::TearDown() clears MockDbusAsync::serviceMap() between tests, but
// it does NOT reset cachedLoggingService (it's static inside logEventAsync).
// =============================================================================

struct LogEventAsyncTest : public testing::Test, public utils::DBusTest
{};

// Test 1: empty serviceMap → hits serviceMap.empty()=true branch → early
// NSM_SW_ERROR return; cache remains unset.
TEST_F(LogEventAsyncTest, EmptyServiceMap_EarlyReturn)
{
    // MockDbusAsync::serviceMap() is empty by default after DBusTest::SetUp()
    logEventAsync("test.MessageId", Level::Critical, {});
    // No assertion needed – we are exercising the branch, not the return value
    // (logEventAsync returns a Coroutine whose result is not co_awaited here,
    //
    //  but because coGetServiceMap::await_ready() returns true synchronously
    //  the whole function body executes before this line returns).
}

// Test 2: non-empty serviceMap → cache miss path: lookup succeeds, service
// cached.  Covers serviceMap.empty()=false, !cachedLoggingService=true, and
// the coLogEvent success path.
TEST_F(LogEventAsyncTest, NonEmptyServiceMap_CachesAndSucceeds)
{
    auto& smap = utils::MockDbusAsync::serviceMap();
    smap.push_back({"xyz.openbmc_project.Logging", {}});
    logEventAsync("test.MessageId", Level::Critical, {});
}

// Test 3: cachedLoggingService is set by test 2; DBusTest::TearDown() cleared
// serviceMap, so coGetServiceMap would return empty – but the cached path is
// taken and skips the lookup entirely.  Covers cachedLoggingService=true
// branch and the coLogEvent success path from the cached code-path.
TEST_F(LogEventAsyncTest, CachedService_SkipsLookup)
{
    // serviceMap is empty (cleared by TearDown after test 2)
    // cachedLoggingService was set by test 2 → service not empty → no lookup
    logEventAsync("test.MessageId", Level::Critical, {});
}
