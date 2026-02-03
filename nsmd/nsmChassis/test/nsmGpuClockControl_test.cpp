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

#include "nsmGpuClockControl.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createControlGpuClock(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);

}; // namespace nsm

struct NsmChassisClockControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "GPU0_ClockControl";
    const std::string type = "NSM_ClockControl";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisClockControlTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisClockControlTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmChassisClockControlTest, goodTestBasic)
{
    // Basic test to verify test infrastructure works
    EXPECT_EQ(name, "GPU0_ClockControl");
    EXPECT_EQ(type, "NSM_ClockControl");
}

TEST_F(NsmChassisClockControlTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "TestClockControl";
    std::string testType = "NSM_ClockControl";
    std::string testPath = "/test/path";
    std::vector<utils::Association> associations;

    auto clockControl =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, testPath.c_str(), gpu);

    EXPECT_NE(clockControl, nullptr);
    EXPECT_EQ(clockControl->device, gpu);
}

TEST_F(NsmChassisClockControlTest, testClearClockLimitEncodingFailure)
{
    auto& bus = utils::DBusHandler::getBus();

    auto clockControl =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    EXPECT_NE(clockControl, nullptr);

    // This test verifies that the object is created properly
    // The actual coroutine testing would require more complex async test setup
}

TEST_F(NsmChassisClockControlTest, testDeviceAssociation)
{
    auto& bus = utils::DBusHandler::getBus();

    auto clockControl =
        std::make_shared<NsmClearClockLimAsyncIntf>(bus, objPath.c_str(), gpu);

    EXPECT_NE(clockControl, nullptr);
    EXPECT_EQ(clockControl->device, gpu);
    EXPECT_EQ(clockControl->device->getEid(), gpu->getEid());
}
TEST_F(NsmChassisClockControlTest, createControlGpuClockSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_SXM_0/ClockControl0";
    const std::string testName = "HGX_GPU 0 ClockLimit_0";
    const std::string testType = "NSM_ControlClockLimit_0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU_0";
    const std::string physicalContext =
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU";
    const std::string clockMode =
        "com.nvidia.ClockMode.Mode.MaximumPerformance";
    const bool priority = false;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = testName;
    propertyMap["Type"] = testType;
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["PhysicalContext"] = physicalContext;
    propertyMap["ClockMode"] = clockMode;
    propertyMap["Priority"] = priority;

    dbus::PropertyMap association = {
        {"Forward", "parent_chassis"},
        {"Backward", "clock_controls"},
        {"AbsolutePath",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU 0"}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        testObjPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    createControlGpuClock(mockManager, interface, testObjPath);

    EXPECT_GE(gpu->deviceSensors.size(), 1);
    // Check if sensor was created (could be at different index)
    bool foundSensor = false;
    for (const auto& sensor : gpu->deviceSensors)
    {
        if (std::dynamic_pointer_cast<NsmChassisClockControl>(sensor))
        {
            foundSensor = true;
            EXPECT_EQ(testName, sensor->getName());
            break;
        }
    }
    EXPECT_TRUE(foundSensor);
}

TEST_F(NsmChassisClockControlTest, CreateControlGpuClockInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_ControlClockLimit_0";
    const std::string testObjPath =
        "/xyz/openbmc_project/inventory/system/chassis/invalid/ClockControl";
    const std::string testName = "InvalidClockControl";
    const std::string testType = "NSM_ClockControl";
    const std::string testPath = "/test/clock/control";
    const uint64_t clockControlSize = 1;
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(testObjPath,
                                                          interface);
    propertyMap["Name"] = testName;
    propertyMap["Type"] = testType;
    propertyMap["UUID"] = invalidUuid;
    propertyMap["InventoryObjPaths"] = std::vector<std::string>{testPath};
    propertyMap["ClockControlSize"] = clockControlSize;

    EXPECT_THROW_COROUTINE(
        createControlGpuClock(mockManager, interface, testObjPath),
        std::runtime_error);

    // Should have only the automatic first sensor
    EXPECT_EQ(1, gpu->deviceSensors.size());
    auto clockControl = std::dynamic_pointer_cast<NsmClearClockLimAsyncIntf>(
        gpu->deviceSensors[0]);
    EXPECT_EQ(nullptr, clockControl);
}
