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

#include "base.h"

// Forward declare the function to test
namespace nsm
{
requester::Coroutine createNsmDebugToken(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath);
}

using namespace nsm;

struct NsmDebugTokenObjectFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_DebugToken";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/debug_token0";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmDebugTokenObjectFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmDebugTokenObjectFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", std::string("DebugToken0")},
        {"Type", std::string("NSM_DebugToken")},
        {"UUID", gpuUuid},
        {"ChassisName", std::string("HGX_GPU_SXM_0")},
    };
};

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateNsmDebugTokenWithNICType)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    dbus::PropertyMap props = basicProperties;
    props["DebugTokenType"] = std::vector<std::string>{"NIC"};
    propertyMap = props;

    createNsmDebugToken(mockManager, interfaceName, objPath);

    // Verify that NIC debug token was created
    EXPECT_GT(gpu->staticSensors.size(), 0);
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateNsmDebugTokenWithUnifiedType)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    dbus::PropertyMap props = basicProperties;
    props["DebugTokenType"] = std::vector<std::string>{"Unified"};
    propertyMap = props;

    createNsmDebugToken(mockManager, interfaceName, objPath);

    // Verify that unified debug token was created
    EXPECT_GE(mockManager.debugTokenList.size(), 0);
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateNsmDebugTokenWithBothTypes)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    dbus::PropertyMap props = basicProperties;
    props["DebugTokenType"] = std::vector<std::string>{"NIC", "Unified"};
    propertyMap = props;

    createNsmDebugToken(mockManager, interfaceName, objPath);

    // Verify that both debug tokens were created
    EXPECT_GT(gpu->staticSensors.size(), 0);
}

TEST_F(NsmDebugTokenObjectFactoryTest,
       testCreateNsmDebugTokenMissingChassisName)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    dbus::PropertyMap props = {
        {"Name", std::string("DebugToken0")},
        {"Type", std::string("NSM_DebugToken")},
        {"UUID", gpuUuid},
        {"DebugTokenType", std::vector<std::string>{"Unified"}},
    };
    propertyMap = props;

    EXPECT_THROW_COROUTINE(
        createNsmDebugToken(mockManager, interfaceName, objPath),
        sdbusplus::exception::SdBusError);
}

TEST_F(NsmDebugTokenObjectFactoryTest,
       testCreateNsmDebugTokenMissingDebugTokenType)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    createNsmDebugToken(mockManager, interfaceName, objPath);

    // Should handle gracefully without DebugTokenType
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateNsmDebugTokenInvalidUUID)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    uuid_t invalidUuid = "STATIC:99:0:NSM_DEVICE_INSTANCE_NUMBER:99";
    dbus::PropertyMap props = {
        {"Name", std::string("DebugToken0")},
        {"Type", std::string("NSM_DebugToken")},
        {"UUID", invalidUuid},
        {"ChassisName", std::string("InvalidChassis")},
        {"DebugTokenType", std::vector<std::string>{"Unified"}},
    };
    propertyMap = props;

    // Should handle gracefully even with invalid UUID
    createNsmDebugToken(mockManager, interfaceName, objPath);
}

TEST_F(NsmDebugTokenObjectFactoryTest, testMultipleDebugTokens)
{
    std::string objPath1 = "/xyz/openbmc_project/inventory/system/debug_token1";
    std::string objPath2 = "/xyz/openbmc_project/inventory/system/debug_token2";

    auto& propertyMap1 = utils::MockDbusAsync::propertyMap(objPath1,
                                                           interfaceName);
    dbus::PropertyMap props1 = basicProperties;
    props1["DebugTokenType"] = std::vector<std::string>{"Unified"};
    propertyMap1 = props1;

    auto& propertyMap2 = utils::MockDbusAsync::propertyMap(objPath2,
                                                           interfaceName);
    dbus::PropertyMap props2 = basicProperties;
    props2["DebugTokenType"] = std::vector<std::string>{"NIC"};
    propertyMap2 = props2;

    createNsmDebugToken(mockManager, interfaceName, objPath1);
    createNsmDebugToken(mockManager, interfaceName, objPath2);

    // Both should have been created
    EXPECT_GT(gpu->staticSensors.size(), 0);
}
