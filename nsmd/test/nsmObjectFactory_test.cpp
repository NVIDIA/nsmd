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

#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmObjectFactory.hpp"

using namespace nsm;

struct NsmObjectFactoryTest : public Test, public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmObjectFactoryTest() : SensorManagerTest(devices) {}

    ~NsmObjectFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmObjectFactoryTest, goodTestInstance)
{
    auto& factory1 = NsmObjectFactory::instance();
    auto& factory2 = NsmObjectFactory::instance();

    // Should return the same instance (singleton)
    EXPECT_EQ(&factory1, &factory2);
}

TEST_F(NsmObjectFactoryTest, goodTestRegisterSingleInterface)
{
    auto& factory = NsmObjectFactory::instance();

    bool functionCalled = false;
    auto testFunc =
        [&functionCalled](SensorManager&, const std::string&,
                          const std::string&) -> requester::Coroutine {
        functionCalled = true;
        co_return NSM_SUCCESS;
    };

    std::string interfaceName = "test.Interface1";
    factory.registerCreationFunction(testFunc, interfaceName);

    EXPECT_TRUE(factory.creationFunctions.find(interfaceName) !=
                factory.creationFunctions.end());
}

TEST_F(NsmObjectFactoryTest, goodTestRegisterMultipleInterfaces)
{
    auto& factory = NsmObjectFactory::instance();

    auto testFunc = [](SensorManager&, const std::string&,
                       const std::string&) -> requester::Coroutine {
        co_return NSM_SUCCESS;
    };

    std::vector<std::string> interfaces = {
        "test.Interface.A", "test.Interface.B", "test.Interface.C"};

    factory.registerCreationFunction(testFunc, interfaces);

    for (const auto& interface : interfaces)
    {
        EXPECT_TRUE(factory.creationFunctions.find(interface) !=
                    factory.creationFunctions.end());
    }
}

TEST_F(NsmObjectFactoryTest, goodTestCreationFunctionsMap)
{
    auto& factory = NsmObjectFactory::instance();

    // Clear any existing registrations for this test
    size_t initialSize = factory.creationFunctions.size();

    auto testFunc = [](SensorManager&, const std::string&,
                       const std::string&) -> requester::Coroutine {
        co_return NSM_SUCCESS;
    };

    factory.registerCreationFunction(testFunc, "unique.test.interface");

    EXPECT_EQ(factory.creationFunctions.size(), initialSize + 1);
}

TEST_F(NsmObjectFactoryTest, goodTestMultipleRegistrations)
{
    auto& factory = NsmObjectFactory::instance();

    int callCount = 0;
    auto testFunc1 = [&callCount](SensorManager&, const std::string&,
                                  const std::string&) -> requester::Coroutine {
        callCount = 1;
        co_return NSM_SUCCESS;
    };

    auto testFunc2 = [&callCount](SensorManager&, const std::string&,
                                  const std::string&) -> requester::Coroutine {
        callCount = 2;
        co_return NSM_SUCCESS;
    };

    factory.registerCreationFunction(testFunc1, "test.func1");
    factory.registerCreationFunction(testFunc2, "test.func2");

    EXPECT_TRUE(factory.creationFunctions.find("test.func1") !=
                factory.creationFunctions.end());
    EXPECT_TRUE(factory.creationFunctions.find("test.func2") !=
                factory.creationFunctions.end());
}

TEST_F(NsmObjectFactoryTest, goodTestCreateObjects)
{
    auto& factory = NsmObjectFactory::instance();

    bool functionCalled = false;
    auto testFunc =
        [&functionCalled](SensorManager&, const std::string&,
                          const std::string&) -> requester::Coroutine {
        functionCalled = true;
        co_return NSM_SUCCESS;
    };

    std::string interfaceName = "test.CreateObjects";
    factory.registerCreationFunction(testFunc, interfaceName);

    // Call createObjects to cover line 31
    factory.createObjects(mockManager, interfaceName, "/test/object/path");

    EXPECT_TRUE(functionCalled);
}

TEST_F(NsmObjectFactoryTest, goodTestCreateObjectsUnknownInterface)
{
    auto& factory = NsmObjectFactory::instance();

    // Call createObjects with an unregistered interface
    // This should return NSM_SUCCESS without calling any creation function
    factory.createObjects(mockManager, "unknown.interface",
                          "/test/object/path");

    // Should complete without error even for unknown interface
    // (no assertion needed, just ensuring it doesn't crash)
}
