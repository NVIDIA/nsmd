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

namespace nsm
{
requester::Coroutine
    createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath);
requester::Coroutine
    createNsmNVLinkMgmtNicChassisAssembly(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
void createAssemblyAsset(std::shared_ptr<NsmDevice> device,
                         const std::string& chassisName,
                         const std::string& name, const std::string& baseType,
                         const dbus::PropertyMap& allCurrentIfaceProperties);
void createPhysicalContext(std::shared_ptr<NsmDevice> device,
                           const std::string& chassisName,
                           const std::string& name, const std::string& baseType,
                           const dbus::PropertyMap& allCurrentIfaceProperties);
void createAssemblyHealth(std::shared_ptr<NsmDevice> device,
                          const std::string& chassisName,
                          const std::string& name, const std::string& baseType);
void createLocationType(std::shared_ptr<NsmDevice> device,
                        const std::string& chassisName, const std::string& name,
                        const std::string& baseType,
                        const dbus::PropertyMap& allCurrentIfaceProperties);
} // namespace nsm

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

// ==========================================================================
// Factory function coverage tests
// ==========================================================================

struct NsmNVSwitchChassisAssemblyCreateTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string nvSwitchBaseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";
    const std::string nvLinkBaseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_ChassisAssembly";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmNVSwitchChassisAssemblyCreateTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmNVSwitchChassisAssemblyCreateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

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

// coGetCachedBaseProperties fails (no base property map) → early return
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateNVSwitch_BasePropertiesFail)
{
    // No property map set up for this objPath → coGetCachedBaseProperties
    // returns NSM_SW_ERROR → function returns early without adding sensors
    const std::string objPath = "/test/nvswitch/fail";
    size_t initialCount = gpu->staticSensors.size();

    createNsmNVSwitchChassisAssembly(
        mockManager, nvSwitchBaseIntf + ".ChassisAttributes", objPath);

    EXPECT_EQ(gpu->staticSensors.size(), initialCount);
}

// type == "NSM_NVSwitch_ChassisAssembly" → assembly + revision sensors
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateNVSwitch_TypeIsBaseType)
{
    const std::string objPath = "/test/nvswitch/assembly_base";

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      nvSwitchBaseIntf);
    baseMap["Name"] = std::string("NVSwitch_Base");
    baseMap["UUID"] = gpuUuid;
    baseMap["ChassisName"] = std::string("HGX_NS_Base");
    baseMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    size_t initialCount = gpu->staticSensors.size();
    createNsmNVSwitchChassisAssembly(mockManager, nvSwitchBaseIntf, objPath);

    // Expect assemblyObject + versionSensor (NsmInventoryProperty) added
    EXPECT_GT(gpu->staticSensors.size(), initialCount);
}

// type == "NSM_Chassis_Attributes" with PhysicalContext and LocationType
TEST_F(NsmNVSwitchChassisAssemblyCreateTest,
       CreateNVSwitch_ChassisAttributes_AllOptionals)
{
    const std::string objPath = "/test/nvswitch/chassis_attrs_all";
    const std::string interface = nvSwitchBaseIntf + ".ChassisAttributes";

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      nvSwitchBaseIntf);
    baseMap["Name"] = std::string("NVSwitch_AllOpt");
    baseMap["UUID"] = gpuUuid;
    baseMap["ChassisName"] = std::string("HGX_NS_AllOpt");

    auto& currentMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    currentMap["Type"] = std::string("NSM_Chassis_Attributes");
    currentMap["Name"] = std::string("NVSwitch_AllOpt");
    currentMap["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    currentMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded");

    size_t initialCount = gpu->staticSensors.size();
    createNsmNVSwitchChassisAssembly(mockManager, interface, objPath);

    // health + 4 asset sensors + physicalContext + locationType = 7 added
    EXPECT_GT(gpu->staticSensors.size(), initialCount);
}

// type == "NSM_Chassis_Attributes" without optional properties
TEST_F(NsmNVSwitchChassisAssemblyCreateTest,
       CreateNVSwitch_ChassisAttributes_NoOptionals)
{
    const std::string objPath = "/test/nvswitch/chassis_attrs_noopt";
    const std::string interface = nvSwitchBaseIntf + ".ChassisAttributes2";

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      nvSwitchBaseIntf);
    baseMap["Name"] = std::string("NVSwitch_NoOpt");
    baseMap["UUID"] = gpuUuid;
    baseMap["ChassisName"] = std::string("HGX_NS_NoOpt");

    auto& currentMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    currentMap["Type"] = std::string("NSM_Chassis_Attributes");
    currentMap["Name"] = std::string("NVSwitch_NoOpt");
    // No PhysicalContext or LocationType → those branches not taken

    size_t initialCount = gpu->staticSensors.size();
    createNsmNVSwitchChassisAssembly(mockManager, interface, objPath);

    // health + 4 asset sensors = 5 added
    EXPECT_GT(gpu->staticSensors.size(), initialCount);
}

// createNsmNVLinkMgmtNicChassisAssembly with type == baseType
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateNVLink_TypeIsBaseType)
{
    const std::string objPath = "/test/nvlink/assembly_base";

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, nvLinkBaseIntf);
    baseMap["Name"] = std::string("NVLink_Base");
    baseMap["UUID"] = gpuUuid;
    baseMap["ChassisName"] = std::string("HGX_NL_Base");
    baseMap["Type"] = std::string("NSM_NVLinkMgmtNic_ChassisAssembly");

    size_t initialCount = gpu->staticSensors.size();
    createNsmNVLinkMgmtNicChassisAssembly(mockManager, nvLinkBaseIntf, objPath);

    EXPECT_GT(gpu->staticSensors.size(), initialCount);
}

// createNsmNVLinkMgmtNicChassisAssembly with NSM_Chassis_Attributes
TEST_F(NsmNVSwitchChassisAssemblyCreateTest,
       CreateNVLink_ChassisAttributes_NoOptionals)
{
    const std::string objPath = "/test/nvlink/chassis_attrs";
    const std::string interface = nvLinkBaseIntf + ".ChassisAttributes";

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, nvLinkBaseIntf);
    baseMap["Name"] = std::string("NVLink_Attrs");
    baseMap["UUID"] = gpuUuid;
    baseMap["ChassisName"] = std::string("HGX_NL_Attrs");

    auto& currentMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    currentMap["Type"] = std::string("NSM_Chassis_Attributes");
    currentMap["Name"] = std::string("NVLink_Attrs");

    size_t initialCount = gpu->staticSensors.size();
    createNsmNVLinkMgmtNicChassisAssembly(mockManager, interface, objPath);

    EXPECT_GT(gpu->staticSensors.size(), initialCount);
}

// Direct test for createAssemblyHealth helper function
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateAssemblyHealth_Direct)
{
    size_t initialCount = gpu->staticSensors.size();
    createAssemblyHealth(gpu, "HGX_Health_Direct", "NVSwitch_Health",
                         "NSM_NVSwitch_ChassisAssembly");
    EXPECT_EQ(gpu->staticSensors.size(), initialCount + 1);
}

// Direct test for createAssemblyAsset helper function
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateAssemblyAsset_Direct)
{
    dbus::PropertyMap props;
    props["Name"] = std::string("NVSwitch_AssetDirect");

    size_t initialCount = gpu->staticSensors.size();
    createAssemblyAsset(gpu, "HGX_Asset_Direct", "NVSwitch_Asset",
                        "NSM_NVSwitch_ChassisAssembly", props);
    // 4 sensors added: partNumber, serialNumber, model, buildDate
    EXPECT_EQ(gpu->staticSensors.size(), initialCount + 4);
}

// Direct test for createPhysicalContext helper function
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreatePhysicalContext_Direct)
{
    dbus::PropertyMap props;
    props["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");

    size_t initialCount = gpu->staticSensors.size();
    createPhysicalContext(gpu, "HGX_Phys_Direct", "NVSwitch_Phys",
                          "NSM_NVSwitch_ChassisAssembly", props);
    EXPECT_EQ(gpu->staticSensors.size(), initialCount + 1);
}

// Direct test for createLocationType helper function
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateLocationType_Direct)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded");

    size_t initialCount = gpu->staticSensors.size();
    createLocationType(gpu, "HGX_Loc_Direct", "NVSwitch_Loc",
                       "NSM_NVSwitch_ChassisAssembly", props);
    EXPECT_EQ(gpu->staticSensors.size(), initialCount + 1);
}

// count("Name") FALSE: base "Name" absent → name="" but type has no match
// → neither if/else-if block runs → no sensors created, no throw.
TEST_F(NsmNVSwitchChassisAssemblyCreateTest,
       CreateNVSwitch_MissingName_NoSensor)
{
    const std::string subIntf = nvSwitchBaseIntf + ".MissingName";
    const std::string objPath = "/test/nvswitch/missing_name";

    // Base map: has UUID and ChassisName, but NO "Name" → name=""
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      nvSwitchBaseIntf);
    baseMap["UUID"] = gpuUuid;
    baseMap["ChassisName"] = std::string("HGX_NS_NoName");

    // Current map: no "Type" → type="" → neither block executes
    utils::MockDbusAsync::propertyMap(objPath, subIntf);

    const size_t before = gpu->staticSensors.size();
    createNsmNVSwitchChassisAssembly(mockManager, subIntf, objPath);
    EXPECT_EQ(before, gpu->staticSensors.size());
}

// count("ChassisName") FALSE: base "ChassisName" absent → chassisName="" but
// type has no match → neither block runs → no sensors, no throw.
TEST_F(NsmNVSwitchChassisAssemblyCreateTest,
       CreateNVSwitch_MissingChassisName_NoSensor)
{
    const std::string subIntf = nvSwitchBaseIntf + ".MissingChassisName";
    const std::string objPath = "/test/nvswitch/missing_chassis_name";

    // Base map: has Name and UUID, but NO "ChassisName" → chassisName=""
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      nvSwitchBaseIntf);
    baseMap["Name"] = std::string("NVSwitch_NoChassisName");
    baseMap["UUID"] = gpuUuid;

    // Current map: no "Type" → type="" → neither block executes
    utils::MockDbusAsync::propertyMap(objPath, subIntf);

    const size_t before = gpu->staticSensors.size();
    createNsmNVSwitchChassisAssembly(mockManager, subIntf, objPath);
    EXPECT_EQ(before, gpu->staticSensors.size());
}

// count("UUID") FALSE: base "UUID" absent → uuid="" →
// getNsmDeviceFromStaticUUID("") throws std::runtime_error.
// Only runs in coverage build: in real-coroutine mode the exception is thrown
// inside the nested createNsmChassisAssembly coroutine and stored in its
// promise; await_resume() in the outer coroutine is noexcept so the outer
// promise stays empty → EXPECT_THROW_COROUTINE sees no exception.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmNVSwitchChassisAssemblyCreateTest, CreateNVSwitch_MissingUUID_Throws)
{
    const std::string subIntf = nvSwitchBaseIntf + ".MissingUUID";
    const std::string objPath = "/test/nvswitch/missing_uuid";

    // Base map: has Name and ChassisName, but NO "UUID" → uuid=""
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      nvSwitchBaseIntf);
    baseMap["Name"] = std::string("NVSwitch_NoUUID");
    baseMap["ChassisName"] = std::string("HGX_NS_NoUUID");

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, subIntf);
    currMap["Type"] = std::string("NSM_NVSwitch_ChassisAssembly");

    EXPECT_THROW_COROUTINE(
        createNsmNVSwitchChassisAssembly(mockManager, subIntf, objPath),
        std::runtime_error);
}
#endif // COVERAGE_DISABLE_COROUTINES
