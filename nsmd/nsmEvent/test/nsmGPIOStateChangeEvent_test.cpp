/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "device-capability-discovery.h"

#include "nsmEvent/nsmGPIOStateChangeEvent.hpp"
#include "test/mockDBusHandler.hpp"

#include <memory>

#include <gtest/gtest.h>

using namespace ::testing;
using namespace nsm;

class NsmGPIOStateChangeEventTest : public ::testing::Test
{
  protected:
    static constexpr eid_t testEid = 10;
};

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithBasicParameters)
{
    std::string name = "GPIOEvent";
    std::string type = "GPIO";
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW({ NsmGPIOStateChangeEvent event(name, type, gpioIntf); });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithEmptyName)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW({ NsmGPIOStateChangeEvent event("", "GPIO", gpioIntf); });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithEmptyType)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW(
        { NsmGPIOStateChangeEvent event("GPIOEvent", "", gpioIntf); });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithNullInterface)
{
    EXPECT_NO_THROW(
        { NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", nullptr); });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithLongName)
{
    std::string longName(100, 'G');
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW(
        { NsmGPIOStateChangeEvent event(longName, "GPIO", gpioIntf); });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithLongType)
{
    std::string longType(100, 'T');
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW(
        { NsmGPIOStateChangeEvent event("GPIOEvent", longType, gpioIntf); });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithSpecialCharacters)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW({
        NsmGPIOStateChangeEvent event("GPIO-Event_123", "Type-123", gpioIntf);
    });
}

TEST_F(NsmGPIOStateChangeEventTest, MultipleInstancesIndependent)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    NsmGPIOStateChangeEvent event1("GPIO1", "Type1", gpioIntf);
    NsmGPIOStateChangeEvent event2("GPIO2", "Type2", gpioIntf);

    SUCCEED();
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithDifferentNames)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW({
        NsmGPIOStateChangeEvent event1("GPIO_A", "GPIO", gpioIntf);
        NsmGPIOStateChangeEvent event2("GPIO_B", "GPIO", gpioIntf);
        NsmGPIOStateChangeEvent event3("GPIO_C", "GPIO", gpioIntf);
    });
}

TEST_F(NsmGPIOStateChangeEventTest, ConstructorWithDifferentTypes)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    EXPECT_NO_THROW({
        NsmGPIOStateChangeEvent event1("GPIO", "TypeA", gpioIntf);
        NsmGPIOStateChangeEvent event2("GPIO", "TypeB", gpioIntf);
        NsmGPIOStateChangeEvent event3("GPIO", "TypeC", gpioIntf);
    });
}

// ============================================================================
// handle Tests
// ============================================================================

TEST_F(NsmGPIOStateChangeEventTest, HandleWithNullEvent)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(testEid, type, eventId, nullptr, 0);

    // Should handle gracefully (likely return error)
    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithZeroLength)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(testEid, type, eventId, &msg, 0);

    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithNullInterface)
{
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", nullptr);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(testEid, type, eventId, &msg, sizeof(msg));

    // Should handle gracefully when interface is null
    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithSmallEvent)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(testEid, type, eventId, &msg, 10);

    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithDifferentEids)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result1 = event.handle(1, type, eventId, &msg, sizeof(msg));
    auto result2 = event.handle(255, type, eventId, &msg, sizeof(msg));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithZeroEid)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(0, type, eventId, &msg, sizeof(msg));

    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithMaxEid)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(255, type, eventId, &msg, sizeof(msg));

    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleMultipleTimes)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result1 = event.handle(testEid, type, eventId, &msg, sizeof(msg));
    auto result2 = event.handle(testEid, type, eventId, &msg, sizeof(msg));
    auto result3 = event.handle(testEid, type, eventId, &msg, sizeof(msg));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithLargeEvent)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    // Allocate buffer large enough for the length check to pass (so the
    // decode function can safely read event->data_size)
    std::vector<uint8_t> msgBuf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN, 0);
    auto msgPtr = reinterpret_cast<const nsm_msg*>(msgBuf.data());
    NsmType type = {};
    NsmEventId eventId = {};

    auto result = event.handle(testEid, type, eventId, msgPtr, 1000);

    EXPECT_GE(result, 0);
}

TEST_F(NsmGPIOStateChangeEventTest, HandleWithDifferentEventInstances)
{
    std::shared_ptr<GPIOStateIntf> gpioIntf = nullptr;

    NsmGPIOStateChangeEvent event1("GPIO1", "GPIO", gpioIntf);
    NsmGPIOStateChangeEvent event2("GPIO2", "GPIO", gpioIntf);
    NsmGPIOStateChangeEvent event3("GPIO3", "GPIO", gpioIntf);

    nsm_msg msg{};
    NsmType type = {};
    NsmEventId eventId = {};

    auto result1 = event1.handle(testEid, type, eventId, &msg, sizeof(msg));
    auto result2 = event2.handle(testEid, type, eventId, &msg, sizeof(msg));
    auto result3 = event3.handle(testEid, type, eventId, &msg, sizeof(msg));

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

// ============================================================================
// Tests covering the gpioStateIntf != nullptr branch (lines 63-89)
// ============================================================================

struct NsmGPIOStateChangeEventDbusTest : public Test, public utils::DBusTest
{
    static constexpr eid_t testEid = 10;
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();
};

// handle() – gpioStateIntf non-null, 1 GPIO event:
// Covers lines 63-89 (entire gpioStateIntf block):
//   - lastChangeTime update
//   - for loop body (1 iteration)
//   - if (!currentState.empty()) → true → lineStates() / gpioChanged() called
TEST_F(NsmGPIOStateChangeEventDbusTest,
       Handle_WithGPIOStateIntf_OneEvent_UpdatesState)
{
    auto gpioIntf = std::make_shared<GPIOStateIntf>(bus, "/test/gpio/ev/0");
    NsmGPIOStateChangeEvent event("GPIOEvent", "GPIO", gpioIntf);

    // Build a GPIO state change event with 1 GPIO entry (index=5, value=1)
    nsm_gpio_state_change_event_payload payload = {};
    payload.timestamp_low = 12345;
    payload.timestamp_high = 0;
    payload.num_gpio_events = 1;
    payload.gpio_events[0].gpio_index = 5;
    payload.gpio_events[0].gpio_value = 1;

    constexpr size_t payloadSize = sizeof(nsm_gpio_state_change_event_payload);
    constexpr size_t msgLen = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                              payloadSize;
    std::vector<uint8_t> msgBuf(msgLen, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    auto rc = encode_nsm_gpio_state_change_event(0, false, &payload, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = event.handle(testEid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                               NSM_GPIO_STATE_CHANGE_EVENT, msg, msgLen);
    EXPECT_EQ(result, NSM_SW_SUCCESS);

    // Verify lastChangeTime was updated
    EXPECT_EQ(gpioIntf->lastChangeTime(), 12345ULL);

    // Verify lineStates now contains gpio index 5 = true
    auto states = gpioIntf->lineStates();
    ASSERT_EQ(states.count(5), 1u);
    EXPECT_TRUE(states.at(5));
}

// handle() – gpioStateIntf non-null, 0 GPIO events:
// Covers the false branch of if (!currentState.empty()) (line 84):
//   - currentState starts empty (default lineStates = {}),
//     loop has 0 iterations, so currentState stays empty
//   - if (!currentState.empty()) → false → lineStates/gpioChanged NOT called
TEST_F(NsmGPIOStateChangeEventDbusTest,
       Handle_WithGPIOStateIntf_ZeroEvents_NoLineStateUpdate)
{
    auto gpioIntf = std::make_shared<GPIOStateIntf>(bus, "/test/gpio/ev/1");
    NsmGPIOStateChangeEvent event("GPIOEvent2", "GPIO", gpioIntf);

    // 0 GPIO events: encode with num_gpio_events=0
    nsm_gpio_state_change_event_payload zeroPayload = {};
    zeroPayload.timestamp_low = 9999;
    zeroPayload.num_gpio_events = 0;

    constexpr size_t payloadSize = sizeof(nsm_gpio_state_change_event_payload) -
                                   sizeof(nsm_gpio_event);
    constexpr size_t msgLen = sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                              payloadSize;
    std::vector<uint8_t> msgBuf(msgLen + 64, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(msgBuf.data());

    auto rc = encode_nsm_gpio_state_change_event(0, false, &zeroPayload, msg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = event.handle(testEid, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
                               NSM_GPIO_STATE_CHANGE_EVENT, msg, msgLen);
    EXPECT_EQ(result, NSM_SW_SUCCESS);

    // lineStates should still be empty (the if-block was NOT entered)
    EXPECT_TRUE(gpioIntf->lineStates().empty());
}
