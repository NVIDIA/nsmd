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

#include "features/creators.hpp"

using namespace nsm;

#ifdef ENABLE_DEBUG_TOKEN

struct NsmDebugTokenObjectFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    size_t staticBefore = 0;

    NsmDebugTokenObjectFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
        staticBefore = gpu->staticSensors.size();
    }

    ~NsmDebugTokenObjectFactoryTest()
    {
        mockManager.debugTokenList.clear();
        cleanupDeviceSensors(devices);
    }

    features::DeviceContext makeDev(bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "HGX_GPU_SXM_0";
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return dev;
    }

    requester::Coroutine create(const dbus::PropertyMap& params,
                                bool withDevice = true)
    {
        return features::createDebugTokenFeature(mockManager,
                                                 makeDev(withDevice), params);
    }

    dbus::PropertyMap basicParams = {
        {"ChassisName", std::string("HGX_GPU_SXM_0")},
    };
};

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateDebugTokenWithNICType)
{
    dbus::PropertyMap params = basicParams;
    params["DebugTokenType"] = std::vector<std::string>{"NIC"};

    auto rc = create(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // NIC debug token is a static sensor and not in the unified list
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 1);
    EXPECT_TRUE(mockManager.debugTokenList.empty());
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateDebugTokenWithUnifiedType)
{
    dbus::PropertyMap params = basicParams;
    params["DebugTokenType"] = std::vector<std::string>{"Unified"};

    const size_t rrBefore = gpu->roundRobinSensors.size();
    auto rc = create(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // Unified debug token is polled round robin and listed in the manager
    EXPECT_EQ(mockManager.debugTokenList.size(), 1u);
    EXPECT_EQ(gpu->roundRobinSensors.size(), rrBefore + 1);
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateDebugTokenWithBothTypes)
{
    dbus::PropertyMap params = basicParams;
    params["DebugTokenType"] = std::vector<std::string>{"NIC", "Unified"};

    auto rc = create(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 1);
    EXPECT_EQ(mockManager.debugTokenList.size(), 1u);
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateDebugTokenMissingTokenType)
{
    auto rc = create(basicParams);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    // No DebugTokenType → nothing created
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
    EXPECT_TRUE(mockManager.debugTokenList.empty());
}

TEST_F(NsmDebugTokenObjectFactoryTest, testCreateDebugTokenNullDevice)
{
    dbus::PropertyMap params = basicParams;
    params["DebugTokenType"] = std::vector<std::string>{"NIC", "Unified"};

    auto rc = create(params, /*withDevice=*/false);
    EXPECT_EQ(rc.data(), NSM_ERROR);

    EXPECT_EQ(gpu->staticSensors.size(), staticBefore);
    EXPECT_TRUE(mockManager.debugTokenList.empty());
}

TEST_F(NsmDebugTokenObjectFactoryTest, testMultipleDebugTokens)
{
    dbus::PropertyMap params1 = basicParams;
    params1["DebugTokenType"] = std::vector<std::string>{"Unified"};

    dbus::PropertyMap params2 = basicParams;
    params2["ChassisName"] = std::string("HGX_GPU_SXM_1");
    params2["DebugTokenType"] = std::vector<std::string>{"NIC"};

    auto rc1 = create(params1);
    auto rc2 = create(params2);
    EXPECT_EQ(rc1.data(), NSM_SUCCESS);
    EXPECT_EQ(rc2.data(), NSM_SUCCESS);

    // Both should have been created
    EXPECT_EQ(mockManager.debugTokenList.size(), 1u);
    EXPECT_EQ(gpu->staticSensors.size(), staticBefore + 1);
}

// DebugTokenDeviceType param with "Unified" token type
TEST_F(NsmDebugTokenObjectFactoryTest, testCreateDebugTokenWithDeviceType)
{
    dbus::PropertyMap params = basicParams;
    params["DebugTokenType"] = std::vector<std::string>{"Unified"};
    params["DebugTokenDeviceType"] = std::string("GPU");

    auto rc = create(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(mockManager.debugTokenList.size(), 1u);
}

#endif // ENABLE_DEBUG_TOKEN
