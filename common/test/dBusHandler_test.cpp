/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Tests for production dBusHandler.cpp awaitable implementations:
 *   - coGetDbusPropertyBase::await_ready() / await_suspend()
 *   - coGetServiceMap::await_ready() / await_suspend()
 *   - coGetAllDbusProperty::await_ready() / await_suspend()
 *   - coLogEvent::await_ready() / await_suspend()
 *
 * Unlike dbus_async_utils_test.cpp (which uses the mock dBusHandler),
 * these tests compile production dBusHandler.cpp directly to exercise
 * the real async_method_call paths.
 *
 * All await_ready() methods in production return false unconditionally.
 * All await_suspend() methods queue an async D-Bus call and return true.
 * The D-Bus call will fail with an error (service not found in the test
 * environment), and the error callback will fire while the awaitable
 * object is still alive on the stack, preventing the SEGV that occurs
 * when the coroutine is destroyed before the callback fires.
 */

#include "dBusAsyncUtils.hpp"
#include "utils.hpp"

#include <chrono>
#include <coroutine>

#include <gtest/gtest.h>

using namespace utils;

// Drain the static asio io_context so that pending D-Bus callbacks fire
// while awaitables are still alive on the stack.
static void drainIoContext(int ms = 200)
{
    utils::DBusHandler::getAsioConnection()->get_io_context().run_for(
        std::chrono::milliseconds(ms));
}

// ============================================================================
// coGetDbusPropertyBase (via coGetDbusProperty<std::string>)
// ============================================================================

TEST(DBusHandlerTest, coGetDbusProperty_AwaitReady_ReturnsFalse)
{
    // Production await_ready() always returns false (unlike mock)
    coGetDbusProperty<std::string> obj("/obj", "Prop", "com.test.Iface");
    EXPECT_FALSE(obj.await_ready());
}

TEST(DBusHandlerTest, coGetDbusProperty_AwaitSuspend_ReturnsTrueQueuesCall)
{
    // await_suspend() queues async D-Bus Get call and returns true (suspend)
    coGetDbusProperty<std::string> obj("/obj", "Prop", "com.test.Iface",
                                       "com.test.Service");
    EXPECT_TRUE(obj.await_suspend(std::noop_coroutine()));

    // Drain io_context: D-Bus error comes back, callback fires, obj is alive.
    // On error, resetRetValue() is called → string stays empty.
    drainIoContext();
    EXPECT_EQ(obj.await_resume(), "");
}

TEST(DBusHandlerTest, coGetDbusProperty_Bool_AwaitSuspend_DrainsSafely)
{
    coGetDbusProperty<bool> obj("/obj", "Flag", "com.test.Iface",
                                "com.test.Service");
    EXPECT_FALSE(obj.await_ready());
    EXPECT_TRUE(obj.await_suspend(std::noop_coroutine()));
    drainIoContext();
    // resetRetValue() on error → bool() == false
    EXPECT_FALSE(obj.await_resume());
}

// ============================================================================
// coGetServiceMap
// ============================================================================

TEST(DBusHandlerTest, coGetServiceMap_Constructor_StoresFields)
{
    dbus::Interfaces ifaces{"com.A", "com.B"};
    coGetServiceMap sm("/test/path", ifaces);
    EXPECT_EQ(sm.objectPath, "/test/path");
    EXPECT_EQ(sm.ifaceList, ifaces);
    EXPECT_TRUE(sm.await_resume().empty());
}

TEST(DBusHandlerTest, coGetServiceMap_AwaitReady_ReturnsFalse)
{
    coGetServiceMap sm("/test/path", dbus::Interfaces{});
    EXPECT_FALSE(sm.await_ready());
}

TEST(DBusHandlerTest, coGetServiceMap_AwaitSuspend_ReturnsTrueQueuesCall)
{
    // await_suspend() queues async ObjectMapper.GetObject and returns true
    coGetServiceMap sm("/no/such/path", dbus::Interfaces{});
    EXPECT_TRUE(sm.await_suspend(std::noop_coroutine()));

    // Drain io_context: error callback fires while sm is alive on the stack.
    // On error, map is not updated → still empty.
    drainIoContext();
    EXPECT_TRUE(sm.await_resume().empty());
}

// NOTE: "DestroyedBeforeCallback" scenario (outer awaitable destroyed while
// the D-Bus async call is still pending) requires an aliveMarker to be stored
// inside the struct, which is intentionally not done.  In production the
// awaitables are always used inside a co_await expression so the outer
// coroutine frame remains alive until the callback completes.

// ============================================================================
// coGetAllDbusProperty
// ============================================================================

TEST(DBusHandlerTest, coGetAllDbusProperty_Constructor_StoresFields)
{
    coGetAllDbusProperty gadp("com.Svc", "/obj/path", "com.Iface");
    EXPECT_EQ(gadp.service, "com.Svc");
    EXPECT_EQ(gadp.objectPath, "/obj/path");
    EXPECT_EQ(gadp.interface, "com.Iface");
    EXPECT_TRUE(gadp.await_resume().empty());
}

TEST(DBusHandlerTest, coGetAllDbusProperty_Constructor_DefaultInterface)
{
    coGetAllDbusProperty gadp("com.Svc", "/obj/path");
    EXPECT_EQ(gadp.service, "com.Svc");
    EXPECT_EQ(gadp.objectPath, "/obj/path");
    EXPECT_EQ(gadp.interface, "");
    EXPECT_TRUE(gadp.await_resume().empty());
}

TEST(DBusHandlerTest, coGetAllDbusProperty_AwaitReady_ReturnsFalse)
{
    coGetAllDbusProperty gadp("com.Svc", "/obj/path", "com.Iface");
    EXPECT_FALSE(gadp.await_ready());
}

TEST(DBusHandlerTest, coGetAllDbusProperty_AwaitSuspend_ReturnsTrueQueuesCall)
{
    // await_suspend() queues async DBus.Properties.GetAll and returns true
    coGetAllDbusProperty gadp("com.test.Service", "/no/such/path",
                              "com.test.Iface");
    EXPECT_TRUE(gadp.await_suspend(std::noop_coroutine()));

    // Drain io_context: error callback fires while gadp is alive on the stack.
    // On error, property map is not updated → still empty.
    drainIoContext();
    EXPECT_TRUE(gadp.await_resume().empty());
}

// ============================================================================
// coLogEvent
// ============================================================================

TEST(DBusHandlerTest, coLogEvent_AwaitReady_ReturnsFalse)
{
    Level lvl = Level::Error;
    std::map<std::string, std::string> data{{"key", "val"}};
    coLogEvent evt("com.test.Service", "MessageId.Test", lvl, data);
    EXPECT_FALSE(evt.await_ready());
}

TEST(DBusHandlerTest, coLogEvent_AwaitResume_DefaultIsFalse)
{
    Level lvl = Level::Warning;
    std::map<std::string, std::string> data{};
    coLogEvent evt("com.test.Service", "MsgId", lvl, data);
    // success member is value-initialised to false
    EXPECT_FALSE(evt.await_resume());
}

TEST(DBusHandlerTest, coLogEvent_AwaitSuspend_ReturnsTrueQueuesCall)
{
    // await_suspend() queues async Logging.Create call and returns true
    Level lvl = Level::Warning;
    std::map<std::string, std::string> data{};
    coLogEvent evt("com.test.Logging", "MessageId.Test", lvl, data);
    EXPECT_TRUE(evt.await_suspend(std::noop_coroutine()));

    // Drain io_context: error callback fires while evt is alive on the stack.
    // On error, success remains false.
    drainIoContext();
    EXPECT_FALSE(evt.await_resume());
}
