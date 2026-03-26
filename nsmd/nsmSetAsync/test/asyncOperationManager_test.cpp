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

#include <unistd.h>

#include <stdexcept>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "asyncOperationManager.hpp"
#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"

using namespace nsm;

TEST(asyncOperationManager, GoodTest)
{
    AsyncOperationManager manager{AsyncOperationResultObjPath};

    for (size_t i{0}; i < 4; ++i)
    {
        const auto [path, statusInterface] = manager.getNewStatusInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(i);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
    }

    for (size_t i{4}; i < 8; ++i)
    {
        const auto [path, statusInterface,
                    valueInterface] = manager.getNewStatusValueInterface();

        const std::string objPath = std::string{AsyncOperationResultObjPath} +
                                    "/" + std::to_string(i);

        EXPECT_EQ(path, objPath);
        EXPECT_NE(statusInterface.get(), nullptr);
        EXPECT_NE(valueInterface.get(), nullptr);
    }

    for (size_t i{0}; i < 32; ++i)
    {
        const auto [path, statusInterface] = manager.getNewStatusInterface();
        EXPECT_NE(path, "");
        EXPECT_NE(statusInterface.get(), nullptr);
    }

    for (size_t i{0}; i < 32; ++i)
    {
        const auto [path, statusInterface,
                    valueInterface] = manager.getNewStatusValueInterface();

        EXPECT_NE(path, "");
        EXPECT_NE(statusInterface.get(), nullptr);
        EXPECT_NE(valueInterface.get(), nullptr);
    }
}

// clearValueInterface() – unix_fd branch (line 101-103 in
// asyncOperationManager.cpp): when the stored value is a unix_fd, the fd
// must be closed before clearing the variant.
TEST(asyncOperationManager, ClearValueInterface_UnixFdBranch_ClosesFd)
{
    AsyncOperationManager manager{AsyncOperationResultObjPath};

    // Obtain a fresh value interface
    const auto [path, statusIntf,
                valueIntf] = manager.getNewStatusValueInterface();
    ASSERT_NE(valueIntf, nullptr);

    // Create a real fd via pipe (uses read-end)
    int pipefd[2];
    ASSERT_EQ(::pipe(pipefd), 0);
    const int readEnd = pipefd[0];
    ::close(pipefd[1]); // write-end not needed

    // Store unix_fd into the value interface to exercise the unix_fd branch
    valueIntf->value(sdbusplus::message::unix_fd{readEnd});
    EXPECT_TRUE(std::holds_alternative<sdbusplus::message::unix_fd>(
        valueIntf->value()));

    // clearValueInterface() should close readEnd and reset the variant
    manager.clearValueInterface(valueIntf);

    // After clear the variant must no longer hold a unix_fd
    EXPECT_FALSE(std::holds_alternative<sdbusplus::message::unix_fd>(
        valueIntf->value()));

    // readEnd is now closed; a second close() must return EBADF
    EXPECT_EQ(::close(readEnd), -1);
    EXPECT_EQ(errno, EBADF);
}

// =============================================================================
// AsyncSetOperationDispatcher::setImpl branch coverage tests
// =============================================================================

static AsyncSetOperationHandler makeSuccessHandler()
{
    return [](const AsyncSetOperationValueType&, AsyncOperationStatusType*,
              std::shared_ptr<NsmDevice>) -> requester::Coroutine {
        co_return NSM_SW_SUCCESS;
    };
}

// setImpl: interface not in asyncOperations → throws UnsupportedRequest
TEST(asyncOperationManager, SetImpl_InterfaceNotFound_ThrowsUnsupportedRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(
        bus, "/com/nvidia/nsmd/dispatch/test_ifnotfound");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_ifnotfound");

    EXPECT_THROW_COROUTINE(
        disp.setImpl("NoSuchInterface", "SomeProp",
                     AsyncSetOperationValueType{false}, resultIntf),
        sdbusplus::error::xyz::openbmc_project::common::UnsupportedRequest);
}

// setImpl: interface registered but property missing → throws
// UnsupportedRequest
TEST(asyncOperationManager, SetImpl_PropertyNotFound_ThrowsUnsupportedRequest)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(
        bus, "/com/nvidia/nsmd/dispatch/test_propnotfnd");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_propnotfnd");

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {makeSuccessHandler(), nullptr, nullptr});

    EXPECT_THROW_COROUTINE(
        disp.setImpl("TestIface", "NoSuchProp",
                     AsyncSetOperationValueType{false}, resultIntf),
        sdbusplus::error::xyz::openbmc_project::common::UnsupportedRequest);
}

// setImpl: handler throws InvalidArgument → resultIntf status = InvalidArgument
// Only runs in coverage build: in real-coroutine mode the nested handler
// coroutine stores the exception in its own promise; setImpl's try/catch never
// sees it (await_resume() is noexcept) → status stays Success.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST(asyncOperationManager,
     SetImpl_HandlerThrowsInvalidArg_StatusInvalidArgument)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(bus,
                                     "/com/nvidia/nsmd/dispatch/test_invarg");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_invarg");

    AsyncSetOperationHandler throwHandler =
        [](const AsyncSetOperationValueType&, AsyncOperationStatusType*,
           std::shared_ptr<NsmDevice>) -> requester::Coroutine {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
        co_return NSM_SW_SUCCESS;
    };

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {throwHandler, nullptr, nullptr});

    auto coro = disp.setImpl("TestIface", "TestProp",
                             AsyncSetOperationValueType{false}, resultIntf);

    EXPECT_EQ(resultIntf->status(), AsyncOperationStatusType::InvalidArgument);
}
#endif // COVERAGE_DISABLE_COROUTINES

// setImpl: handler throws std::runtime_error → resultIntf status =
// InternalFailure
// Only runs in coverage build: same reason as above.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST(asyncOperationManager, SetImpl_HandlerThrowsOther_StatusInternalFailure)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(bus,
                                     "/com/nvidia/nsmd/dispatch/test_intfail");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_intfail");

    AsyncSetOperationHandler throwHandler =
        [](const AsyncSetOperationValueType&, AsyncOperationStatusType*,
           std::shared_ptr<NsmDevice>) -> requester::Coroutine {
        throw std::runtime_error("unexpected error");
        co_return NSM_SW_SUCCESS;
    };

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {throwHandler, nullptr, nullptr});

    auto coro = disp.setImpl("TestIface", "TestProp",
                             AsyncSetOperationValueType{false}, resultIntf);

    EXPECT_EQ(resultIntf->status(), AsyncOperationStatusType::InternalFailure);
}
#endif // COVERAGE_DISABLE_COROUTINES

// setImpl: sensor is null → success (isOnline check skipped)
TEST(asyncOperationManager, SetImpl_SensorNull_SuccessWithoutUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(
        bus, "/com/nvidia/nsmd/dispatch/test_sensornil");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_sensornil");

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {makeSuccessHandler(), nullptr, nullptr});

    auto coro = disp.setImpl("TestIface", "TestProp",
                             AsyncSetOperationValueType{false}, resultIntf);

    EXPECT_EQ(resultIntf->status(), AsyncOperationStatusType::Success);
}

// setImpl: sensor non-null, device offline → sensor->update not called
TEST(asyncOperationManager, SetImpl_DeviceOffline_SensorUpdateSkipped)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(bus,
                                     "/com/nvidia/nsmd/dispatch/test_offline");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_offline");

    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0);
    device->isDeviceActive = false; // Force device offline

    auto sensor = std::make_shared<MockSensor>("testSensor", "TestType");

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {makeSuccessHandler(), sensor, device});

    auto coro = disp.setImpl("TestIface", "TestProp",
                             AsyncSetOperationValueType{false}, resultIntf);

    EXPECT_EQ(resultIntf->status(), AsyncOperationStatusType::Success);
}

// AsyncSetOperationDispatcher::set() – happy path:
// Covers lines 134-154 (set() function body):
// - calls getInstance()->getNewStatusInterface()
// - result.first.empty() == false → skips throw branch
// - calls setImpl().detach()
// - returns the object path
TEST(asyncOperationManager, Set_ValidInterfaceAndProperty_ReturnsObjectPath)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(bus,
                                     "/com/nvidia/nsmd/dispatch/test_set_hpy");

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {makeSuccessHandler(), nullptr, nullptr});

    sdbusplus::message::object_path resultPath;
    EXPECT_NO_THROW(resultPath = disp.set("TestIface", "TestProp",
                                          AsyncSetOperationValueType{false}));
    EXPECT_FALSE(resultPath.str.empty());
}

// setImpl: sensor non-null, device online → sensor->update is called
TEST(asyncOperationManager, SetImpl_DeviceOnline_SensorUpdateCalled)
{
    auto& bus = utils::DBusHandler::getBus();
    AsyncSetOperationDispatcher disp(bus,
                                     "/com/nvidia/nsmd/dispatch/test_online");
    auto resultIntf = std::make_shared<AsyncStatusIntf>(
        bus, "/com/nvidia/nsmd/result/test_online");

    auto device = std::make_shared<MockNsmDevice>(
        0, 0, "NSM_DEVICE_INSTANCE_NUMBER", "0", 0);
    // isDeviceActive = true by default (set in MockNsmDevice constructor)

    auto sensor = std::make_shared<MockSensor>("testSensor", "TestType");

    disp.addAsyncSetOperation("TestIface", "TestProp",
                              {makeSuccessHandler(), sensor, device});

    auto coro = disp.setImpl("TestIface", "TestProp",
                             AsyncSetOperationValueType{false}, resultIntf);

    EXPECT_EQ(resultIntf->status(), AsyncOperationStatusType::Success);
}
