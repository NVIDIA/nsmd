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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmNVSwitchAndNVMgmtNICChassisAssembly.hpp"

namespace nsm
{
requester::Coroutine
    createNsmNVSwitchChassisAssembly(SensorManager& manager,
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

// ============================================================================
// createNVLinkMgmtNicChassisAssemblyFeature
// These tests cover createAssemblyAsset, createAssemblyHealth,
// createPhysicalContext, createLocationType through the feature creator.
// ============================================================================

struct NsmNVLinkMgmtNicChassisAssemblyFeatureTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string chassisName = "HGX_NVLinkMgmt_0";
    const uuid_t deviceUuid = "STATIC:2:0:NSM_DEVICE_INSTANCE_NUMBER:10";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;
    nsm::features::DeviceContext dev;

    NsmNVLinkMgmtNicChassisAssemblyFeatureTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
        dev.name = chassisName;
        dev.kind = "cx_nic";
        dev.staticUuid = deviceUuid;
        dev.platformPrefix = "HGX_";
        dev.device = device;
    }

    ~NsmNVLinkMgmtNicChassisAssemblyFeatureTest()
    {
        cleanupDeviceSensors(devices);
    }

    requester::Coroutine createAssembly(const dbus::PropertyMap& params)
    {
        return nsm::features::createNVLinkMgmtNicChassisAssemblyFeature(
            mockManager, dev, params);
    }
};

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest,
       CreateAssembly_WithPhysicalContextAndLocationType)
{
    dbus::PropertyMap params{
        {"Name", std::string("NVLinkMgmtNic0_Asm")},
        {"PhysicalContext",
         std::string("xyz.openbmc_project.Inventory.Decorator.Area."
                     "PhysicalContextType.Backplane")},
        {"LocationType", std::string("xyz.openbmc_project.Inventory.Decorator."
                                     "Location.LocationTypes.Embedded")}};

    const size_t staticBefore = device->staticSensors.size();
    auto rc = createAssembly(params);
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);

    // Assembly + revision + health + 4 asset + area + location
    EXPECT_EQ(device->staticSensors.size(), staticBefore + 9);
    size_t areas = 0, locations = 0;
    for (const auto& sensor : device->staticSensors)
    {
        areas += dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(sensor)
                     ? 1
                     : 0;
        locations +=
            dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(sensor)
                ? 1
                : 0;
    }
    EXPECT_EQ(areas, 1u);
    EXPECT_EQ(locations, 1u);
}

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest, CreateAssembly_NoOptionals)
{
    const size_t staticBefore = device->staticSensors.size();
    auto rc = createAssembly({{"Name", std::string("NVLink_Attrs")}});
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);

    // Assembly + revision + health + 4 asset
    EXPECT_EQ(device->staticSensors.size(), staticBefore + 7);
    auto assembly = dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(
        device->staticSensors[staticBefore]);
    ASSERT_NE(assembly, nullptr);
    EXPECT_EQ(assembly->getName(), "Assembly0");
    EXPECT_EQ(assembly->getType(), "NSM_NVLinkMgmtNic_ChassisAssembly");
}

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest,
       CreateAssembly_IndexesPerDevice)
{
    auto rc0 = createAssembly({{"Name", std::string("First")}});
    auto rc1 = createAssembly({{"Name", std::string("Second")}});
    EXPECT_EQ(rc0.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(rc1.data(), NSM_SW_SUCCESS);

    std::vector<std::string> names;
    for (const auto& sensor : device->staticSensors)
    {
        if (dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(sensor))
        {
            names.push_back(sensor->getName());
        }
    }
    EXPECT_EQ((std::vector<std::string>{"Assembly0", "Assembly1"}), names);
}

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest,
       CreateAssembly_MissingName_Rejected)
{
    const size_t staticBefore = device->staticSensors.size();
    auto rc = createAssembly({});
    EXPECT_EQ(rc.data(), NSM_SW_ERROR_DATA);
    EXPECT_EQ(device->staticSensors.size(), staticBefore);
    // The rejected assembly did not take an index
    EXPECT_EQ(dev.nextIndex("Assembly"), 0u);
}

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest,
       CreateAssembly_NonStringParam_Rejected)
{
    const size_t staticBefore = device->staticSensors.size();
    auto rc = createAssembly({{"Name", std::string("NVLink_Bad")},
                              {"PhysicalContext", uint64_t{1}}});
    EXPECT_EQ(rc.data(), NSM_SW_ERROR_DATA);
    EXPECT_EQ(device->staticSensors.size(), staticBefore);
}

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest,
       CreateAssembly_InvalidLocationType_Throws)
{
    const size_t staticBefore = device->staticSensors.size();
    EXPECT_THROW_COROUTINE(
        createAssembly({{"Name", std::string("NVLink_BadLoc")},
                        {"LocationType", std::string("NotALocation")}}),
        std::exception);
    EXPECT_EQ(device->staticSensors.size(), staticBefore);
}

TEST_F(NsmNVLinkMgmtNicChassisAssemblyFeatureTest,
       CreateAssembly_NoNsmDevice_Fails)
{
    dev.device = nullptr;
    auto rc = createAssembly({{"Name", std::string("NVLink_NoDevice")}});
    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
    EXPECT_FALSE(rc.exception());
}

// ============================================================================
// Legacy NSM_NVSwitch_ChassisAssembly creator (still registered)
// ============================================================================

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
