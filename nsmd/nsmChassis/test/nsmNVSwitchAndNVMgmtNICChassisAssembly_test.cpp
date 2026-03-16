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

#include "nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

using namespace nsm;

// Import types from the namespace
using AssemblyIntf = nsm::AssemblyIntf;
using HealthIntf = nsm::HealthIntf;
using AreaIntf = nsm::AreaIntf;
using LocationIntf = nsm::LocationIntf;

struct NsmNVSwitchChassisAssemblyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_Chassis";
    const std::string name = "NVSwitch0_Assembly";
    const std::string type = "NSM_NVSwitch_Assembly";

    NsmDeviceTable devices;

    NsmNVSwitchChassisAssemblyTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestConstructor)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
    EXPECT_EQ(assembly->getType(), type);
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestNVSwitchAssembly)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "NVSwitch0", type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), "NVSwitch0");
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestNVMgmtNICAssembly)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "NVMgmtNIC0", type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), "NVMgmtNIC0");
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestMultipleAssemblies)
{
    auto assembly1 =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "Assembly0", type);
    auto assembly2 =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "Assembly1", type);
    auto assembly3 =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AssemblyIntf>>(
            chassisName, "Assembly2", type);

    EXPECT_NE(assembly1, nullptr);
    EXPECT_NE(assembly2, nullptr);
    EXPECT_NE(assembly3, nullptr);

    EXPECT_EQ(assembly1->getName(), "Assembly0");
    EXPECT_EQ(assembly2->getName(), "Assembly1");
    EXPECT_EQ(assembly3->getName(), "Assembly2");
}

// TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestDifferentTypes) - Disabled due
// to DBus path conflict

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestWithHealthIntf)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<HealthIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestWithAreaIntf)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<AreaIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
}

TEST_F(NsmNVSwitchChassisAssemblyTest, goodTestWithLocationIntf)
{
    auto assembly =
        std::make_shared<NsmNVSwitchAndNicChassisAssembly<LocationIntf>>(
            chassisName, name, type);
    EXPECT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), name);
}

namespace nsm
{
requester::Coroutine
    createNsmNVLinkMgmtNicChassisAssembly(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

// ============================================================================
// Factory tests: createNsmNVLinkMgmtNicChassisAssembly
// These tests cover createAssemblyAsset, createAssemblyHealth,
// createPhysicalContext, createLocationType, and the top-level factory.
// ============================================================================

struct NsmNVLinkMgmtNicChassisAssemblyFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string baseIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvlink_asm_factory";
    const uuid_t deviceUuid = "STATIC:2:0:NSM_DEVICE_INSTANCE_NUMBER:10";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmNVLinkMgmtNicChassisAssemblyFactoryTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmNVLinkMgmtNicChassisAssemblyFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFactoryTest,
       CreateChassisAttributes_WithPhysicalContextAndLocationType)
{
    const std::string currentIntf = baseIntfName + ".ChassisAttributes";

    // Set up base properties
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              baseIntfName);
    basePropertyMap["Name"] = std::string("NVLinkMgmtNic0_Asm");
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["ChassisName"] = std::string("HGX_NVLinkMgmt_0");

    // Set up attributes properties
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, currentIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["Name"] = std::string("NVLinkMgmtNic0_Asm");
    propertyMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area."
        "PhysicalContextType.Backplane");
    propertyMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Embedded");

    size_t staticBefore = device->staticSensors.size();
    createNsmNVLinkMgmtNicChassisAssembly(mockManager, currentIntf, objPath);

    // Should have created Health + Asset sensors + PhysicalContext + Location
    EXPECT_GT(device->staticSensors.size(), staticBefore);
}
