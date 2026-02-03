/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#define private public
#define protected public

#include "base.h"

#include "nsmReset.hpp"

#include <gtest/gtest.h>

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmResetSensor(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);

}; // namespace nsm

struct NsmResetTest : public ::testing::Test, public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmResetTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmResetTest()
    {
        if (gpu)
        {
            gpu->staticSensors.clear();
        }
    }
};

TEST_F(NsmResetTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "GpuReset0";
    std::string type = "NSM_GpuReset";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/control/processor/reset0";
    uint8_t deviceIndex = 0;

    auto reset = std::make_shared<NsmReset>(bus, name, type, inventoryObjPath,
                                            gpu, deviceIndex);

    EXPECT_NE(reset, nullptr);
    EXPECT_EQ(reset->getName(), name);
    EXPECT_EQ(reset->getType(), type);
}

TEST_F(NsmResetTest, testConstructorWithDifferentDeviceIndices)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "GpuReset";
    std::string type = "NSM_GpuReset";

    std::vector<uint8_t> deviceIndices = {0, 1, 2, 3};
    for (const auto& deviceIndex : deviceIndices)
    {
        std::string inventoryObjPath =
            "/xyz/openbmc_project/control/processor/reset" +
            std::to_string(deviceIndex);

        auto reset = std::make_shared<NsmReset>(
            bus, name, type, inventoryObjPath, gpu, deviceIndex);
        EXPECT_NE(reset, nullptr);
        EXPECT_EQ(reset->getName(), name);
    }
}

TEST_F(NsmResetTest, testConstructorWithDifferentNames)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string type = "NSM_GpuReset";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/control/processor/reset";
    uint8_t deviceIndex = 0;

    std::vector<std::string> names = {"Reset0", "Reset1", "GpuReset"};
    for (const auto& name : names)
    {
        auto reset = std::make_shared<NsmReset>(
            bus, name, type, inventoryObjPath, gpu, deviceIndex);
        EXPECT_EQ(reset->getName(), name);
    }
}

TEST_F(NsmResetTest, testConstructorWithMultipleDevices)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "GpuReset";
    std::string type = "NSM_GpuReset";
    std::string inventoryObjPath1 =
        "/xyz/openbmc_project/control/processor/reset_gpu0";
    std::string inventoryObjPath2 =
        "/xyz/openbmc_project/control/processor/reset_gpu1";
    uint8_t deviceIndex = 0;

    const uuid_t gpu2Uuid = "STATIC:0:1:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto gpu2 = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(gpu2Uuid));
    EXPECT_NE(gpu2, nullptr);

    auto reset1 = std::make_shared<NsmReset>(bus, name, type, inventoryObjPath1,
                                             gpu, deviceIndex);
    auto reset2 = std::make_shared<NsmReset>(bus, name, type, inventoryObjPath2,
                                             gpu2, deviceIndex);

    EXPECT_NE(reset1, nullptr);
    EXPECT_NE(reset2, nullptr);
    EXPECT_NE(reset1.get(), reset2.get());
}

TEST_F(NsmResetTest, testConstructorWithNullDevice)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "GpuReset";
    std::string type = "NSM_GpuReset";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/control/processor/reset_null";
    uint8_t deviceIndex = 0;

    std::shared_ptr<NsmDevice> nullDevice = nullptr;

    auto reset = std::make_shared<NsmReset>(bus, name, type, inventoryObjPath,
                                            nullDevice, deviceIndex);
    EXPECT_NE(reset, nullptr);
}

TEST_F(NsmResetTest, testMultipleResets)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string type = "NSM_GpuReset";
    uint8_t deviceIndex = 0;

    for (int i = 0; i < 3; i++)
    {
        std::string name = "GpuReset" + std::to_string(i);
        std::string inventoryObjPath =
            "/xyz/openbmc_project/control/processor/reset" + std::to_string(i);

        auto reset = std::make_shared<NsmReset>(
            bus, name, type, inventoryObjPath, gpu, deviceIndex);
        EXPECT_EQ(reset->getName(), name);
        EXPECT_EQ(reset->getType(), type);
    }
}

TEST_F(NsmResetTest, CreateNsmResetSensorSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_GpuReset";
    const std::string objPath = "/xyz/openbmc_project/control/processor/reset0";
    const std::string name = "GpuReset0";
    const std::string type = "NSM_GpuReset";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/control/processor/reset";
    const uint64_t instanceNumber = 0;
    const uint64_t deviceIndex = 0;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["Type"] = type;
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["InstanceNumber"] = instanceNumber;
    propertyMap["DeviceIndex"] = deviceIndex;

    createNsmResetSensor(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    auto reset = std::dynamic_pointer_cast<NsmReset>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, reset);
    EXPECT_EQ(name, reset->getName());
    EXPECT_EQ(type, reset->getType());
}

TEST_F(NsmResetTest, CreateNsmResetSensorDifferentDeviceIndex)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_GpuReset";
    const std::string objPath = "/xyz/openbmc_project/control/processor/reset1";
    const std::string name = "GpuReset1";
    const std::string type = "NSM_GpuReset";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/control/processor/reset";
    const uint64_t instanceNumber = 1;
    const uint64_t deviceIndex = 1;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["Type"] = type;
    propertyMap["UUID"] = gpuUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["InstanceNumber"] = instanceNumber;
    propertyMap["DeviceIndex"] = deviceIndex;

    createNsmResetSensor(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    auto reset = std::dynamic_pointer_cast<NsmReset>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, reset);
    EXPECT_EQ(name, reset->getName());
}

TEST_F(NsmResetTest, CreateNsmResetSensorInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_GpuReset";
    const std::string objPath =
        "/xyz/openbmc_project/control/processor/reset_invalid";
    const std::string name = "GpuResetInvalid";
    const std::string type = "NSM_GpuReset";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/control/processor/reset";
    const uint64_t instanceNumber = 99;
    const uint64_t deviceIndex = 0;
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["Type"] = type;
    propertyMap["UUID"] = invalidUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["InstanceNumber"] = instanceNumber;
    propertyMap["DeviceIndex"] = deviceIndex;

    createNsmResetSensor(mockManager, interface, objPath);

    // Should have only the automatic first sensor, not the reset
    EXPECT_EQ(1, gpu->deviceSensors.size());
    auto reset = std::dynamic_pointer_cast<NsmReset>(gpu->deviceSensors[0]);
    EXPECT_EQ(nullptr, reset);
}
