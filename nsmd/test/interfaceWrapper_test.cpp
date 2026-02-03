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
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "interfaceWrapper.hpp"

#include <xyz/openbmc_project/Sensor/Value/server.hpp>

using namespace nsm;
using ValueIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Sensor::server::Value>;

struct InterfaceWrapperTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string path = "/xyz/openbmc_project/sensors/test";

    NsmDeviceTable devices;

    InterfaceWrapperTest() : SensorManagerTest(devices) {}
};

TEST_F(InterfaceWrapperTest, goodTestConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto wrapper = std::make_shared<InterfaceWrapper<ValueIntf>>(bus,
                                                                 path.c_str());

    EXPECT_NE(wrapper, nullptr);
}

TEST_F(InterfaceWrapperTest, goodTestGetInterface)
{
    auto& bus = utils::DBusHandler::getBus();
    auto wrapper = std::make_shared<InterfaceWrapper<ValueIntf>>(bus,
                                                                 path.c_str());

    auto intf = wrapper->getInterface();
    EXPECT_NE(intf, nullptr);
}

TEST_F(InterfaceWrapperTest, goodTestInterfaceAccess)
{
    auto& bus = utils::DBusHandler::getBus();
    auto wrapper = std::make_shared<InterfaceWrapper<ValueIntf>>(bus,
                                                                 path.c_str());

    auto intf = wrapper->getInterface();
    EXPECT_NE(intf, nullptr);

    // Test setting and getting value
    intf->value(42.0);
    EXPECT_EQ(intf->value(), 42.0);
}

TEST_F(InterfaceWrapperTest, goodTestMultipleWrappers)
{
    auto& bus = utils::DBusHandler::getBus();

    auto wrapper1 = std::make_shared<InterfaceWrapper<ValueIntf>>(bus,
                                                                  "/path1");
    auto wrapper2 = std::make_shared<InterfaceWrapper<ValueIntf>>(bus,
                                                                  "/path2");

    EXPECT_NE(wrapper1, nullptr);
    EXPECT_NE(wrapper2, nullptr);

    auto intf1 = wrapper1->getInterface();
    auto intf2 = wrapper2->getInterface();

    EXPECT_NE(intf1, nullptr);
    EXPECT_NE(intf2, nullptr);
    EXPECT_NE(intf1, intf2);
}

TEST_F(InterfaceWrapperTest, goodTestGetInterfaceOnObjectPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto& manager = SensorManager::getInstance();

    std::string objPath = "/test/sensor/path";

    // First call should create new interface
    auto intf1 = getInterfaceOnObjectPath<ValueIntf>(objPath, manager, bus,
                                                     path.c_str());
    EXPECT_NE(intf1, nullptr);

    // Second call should return same interface
    auto intf2 = getInterfaceOnObjectPath<ValueIntf>(objPath, manager, bus,
                                                     path.c_str());
    EXPECT_NE(intf2, nullptr);
    EXPECT_EQ(intf1, intf2);
}

TEST_F(InterfaceWrapperTest, goodTestGetInterfaceOnMultiplePaths)
{
    auto& bus = utils::DBusHandler::getBus();
    auto& manager = SensorManager::getInstance();

    auto intf1 = getInterfaceOnObjectPath<ValueIntf>("/path1", manager, bus,
                                                     "/dbus/path1");
    auto intf2 = getInterfaceOnObjectPath<ValueIntf>("/path2", manager, bus,
                                                     "/dbus/path2");

    EXPECT_NE(intf1, nullptr);
    EXPECT_NE(intf2, nullptr);
    EXPECT_NE(intf1, intf2);
}
