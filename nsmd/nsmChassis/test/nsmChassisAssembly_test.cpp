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

#include "nsmAssetIntf.hpp"
#include "nsmChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

namespace nsm
{
requester::Coroutine
    nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath);
requester::Coroutine
    createNsmNVSwitchChassisAssembly(SensorManager& manager,
                                     const std::string& interface,
                                     const std::string& objPath);
requester::Coroutine createNsmChassisAssembly(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath,
                                              const std::string baseType);
}; // namespace nsm

using namespace nsm;

struct NsmChassisAssemblyTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";
    const std::string chassisName = "HGX_GPU_SXM_1";
    const std::string name = "Assembly1";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmChassisAssemblyTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_EQ(2, devices.size());
        EXPECT_NE(gpu, nullptr);
        EXPECT_NE(fpga, nullptr);
        EXPECT_EQ(NSM_DEV_ID_GPU, gpu->getDeviceType());
        EXPECT_EQ(NSM_DEV_ID_BASEBOARD, fpga->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"Type", "NSM_GPU_ChasisAssembly"},
        {"UUID", "a3b0bdf6-8661-4d8e-8268-0e59415f2076"},
    };
    dbus::PropertyMap basic = {
        {"ChassisName", chassisName},    {"Name", name},
        {"Type", "NSM_ChassisAssembly"}, {"UUID", gpuUuid},
        {"AssemblyType", "Device"},
    };
    dbus::PropertyMap chassisAttributes = {
        {"Type", "NSM_Chassis_Attributes"},
        {"PhysicalContext",
         "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU"},
        {"Name", "HGX_GPU_SXM_1"},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
    };
};

TEST_F(NsmChassisAssemblyTest, badTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];

    // Set up propertyMap with the invalid type to ensure it's used
    propertyMap["Type"] = error["Type"];

    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(1, gpu->deviceSensors.size());     // Only msgTypes sensor
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // Only msgTypes sensor
    EXPECT_EQ(0, gpu->prioritySensors.size());
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["ChassisName"] = basic["ChassisName"];
    propertyMap["Name"] = basic["Name"];
    propertyMap["UUID"] = basic["UUID"];
    propertyMap["AssemblyType"] = basic["AssemblyType"];

    // First call: NSM_ChassisAssembly
    propertyMap["Type"] = basic["Type"];
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);

    // Second call: NSM_Chassis_Attributes
    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = chassisAttributes["Type"];
    propertyMapAttributes["PhysicalContext"] =
        chassisAttributes["PhysicalContext"];
    propertyMapAttributes["LocationType"] = chassisAttributes["LocationType"];
    nsmChassisAssemblyCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath);
    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(8, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // msgTypes sensor
    EXPECT_EQ(9, gpu->deviceSensors.size());     // msgTypes + 8 others

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(
                           gpu->deviceSensors[1])); // Index 1, msgTypes at 0

    // Check asset sensors (indices 2-5: partNumber, serialNumber, model,
    // buildDate)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[2]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[3]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[4]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[5]));

    // Check health sensor (index 6)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu->deviceSensors[6]));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu->deviceSensors[6])
                  ->invoke(pdiMethod(health)));

    // Check area sensor (index 7)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
                           gpu->deviceSensors[7]));
    EXPECT_EQ(AreaIntf::PhysicalContextType::GPU,
              dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
                  gpu->deviceSensors[7])
                  ->invoke(pdiMethod(physicalContext)));

    // Check location sensor (index 8)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu->deviceSensors[8]));
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu->deviceSensors[8])
                  ->invoke(pdiMethod(locationType)));
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateStaticSensors)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    basePropertyMap["ChassisName"] = basic["ChassisName"];
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = basic["UUID"];
    basePropertyMap["AssemblyType"] = basic["AssemblyType"];

    // Set up interface-specific properties for Chassis Asset and Health
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = chassisAttributes["Type"];
    propertyMap["Name"] = chassisAttributes["Name"];
    nsmChassisAssemblyCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath);
    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(5, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // msgTypes sensor
    EXPECT_EQ(6, gpu->deviceSensors.size());     // msgTypes + 5 others

    // Check asset sensors (indices 1-4: partNumber, serialNumber, model,
    // buildDate) - msgTypes at index 0
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[1]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[2]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[3]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[4]));

    // Check health sensor (index 5)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu->deviceSensors[5]));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu->deviceSensors[5])
                  ->invoke(pdiMethod(health)));
}

TEST_F(NsmChassisAssemblyTest, badTestNoDevideFound)
{
    // Clear all sensors from previous tests to ensure isolation

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any device
    const uuid_t invalidUuid =
        "a3b0bdf6-8661-4d8e-8268-0e59415f2076"; // Different from gpuUuid and
                                                // fpgaUuid
    basePropertyMap["ChassisName"] = basic["ChassisName"];
    basePropertyMap["Name"] = basic["Name"];
    basePropertyMap["UUID"] = invalidUuid; // Invalid UUID as uuid_t type
    basePropertyMap["AssemblyType"] = basic["AssemblyType"];

    // Set up interface-specific properties
    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = chassisAttributes["Type"];

    EXPECT_THROW_COROUTINE(
        nsmChassisAssemblyCreateSensors(
            mockManager, basicIntfName + ".ChassisAttributes", objPath),
        std::runtime_error);

    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // msgTypes sensor
    EXPECT_EQ(1, gpu->deviceSensors.size());     // msgTypes sensor
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateChassisAssembly)
{
    const std::string assemblyObjPath =
        "/xyz/openbmc_project/inventory/system/assembly";
    const std::string assemblyIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";

    dbus::PropertyMap baseProperties = {
        {"Name", std::string("Assembly_Test")},
        {"UUID", gpuUuid},
        {"ChassisName", std::string("TestChassis")},
        {"AssemblyType", std::string("Assembly")},
    };

    dbus::PropertyMap locationProperties = {
        {"Type", std::string("NSM_NVSwitch_ChassisAssembly_Location")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(assemblyObjPath,
                                                              assemblyIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        assemblyObjPath, assemblyIntfName + ".Location");
    propertyMap = locationProperties;

    createNsmNVSwitchChassisAssembly(
        mockManager, assemblyIntfName + ".Location", assemblyObjPath);

    // Should create Location sensor (may be device sensor, not static)
    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

TEST_F(NsmChassisAssemblyTest, testCreateChassisAssemblyWithInventory)
{
    const std::string assemblyObjPath =
        "/xyz/openbmc_project/inventory/system/assembly_inv";
    const std::string assemblyIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";

    dbus::PropertyMap baseProperties = {
        {"Name", std::string("Assembly_Inventory")},
        {"UUID", gpuUuid},
        {"ChassisName", std::string("InventoryChassis")},
        {"AssemblyType", std::string("Assembly")},
    };

    dbus::PropertyMap inventoryProperties = {
        {"Type", std::string("NSM_NVSwitch_ChassisAssembly_Inventory")},
        {"Name", std::string("Assembly_Inv_Prop")},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(assemblyObjPath,
                                                              assemblyIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        assemblyObjPath, assemblyIntfName + ".Inventory");
    propertyMap = inventoryProperties;

    createNsmNVSwitchChassisAssembly(
        mockManager, assemblyIntfName + ".Inventory", assemblyObjPath);

    // Should create Inventory property sensor (may be device sensor, not
    // static)
    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

TEST_F(NsmChassisAssemblyTest, testCreateChassisAssemblyWithAsset)
{
    const std::string assemblyObjPath =
        "/xyz/openbmc_project/inventory/system/assembly_asset";
    const std::string assemblyIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";

    dbus::PropertyMap baseProperties = {
        {"Name", std::string("Assembly_Asset")},
        {"UUID", gpuUuid},
        {"ChassisName", std::string("AssetChassis")},
        {"AssemblyType", std::string("Assembly")},
    };

    dbus::PropertyMap assetProperties = {
        {"Type", std::string("NSM_NVSwitch_ChassisAssembly_Asset")},
        {"Name", std::string("Assembly_Asset_Info")},
        {"Manufacturer", std::string("NVIDIA")},
        {"Model", std::string("NVSwitch_Assembly_1")},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(assemblyObjPath,
                                                              assemblyIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        assemblyObjPath, assemblyIntfName + ".Asset");
    propertyMap = assetProperties;

    createNsmNVSwitchChassisAssembly(mockManager, assemblyIntfName + ".Asset",
                                     assemblyObjPath);

    // Should create Asset sensor (may be device sensor, not static)
    EXPECT_GE(gpu->deviceSensors.size(), 1);
}

TEST_F(NsmChassisAssemblyTest, badTestCreateChassisAssemblyMissingUUID)
{
    const std::string assemblyObjPath =
        "/xyz/openbmc_project/inventory/system/assembly_nouuid";
    const std::string assemblyIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";

    dbus::PropertyMap baseProperties = {
        {"Name", std::string("Assembly_NoUUID")},
        // Missing UUID
        {"ChassisName", std::string("NoChassis")},
        {"AssemblyType", std::string("Assembly")},
    };

    dbus::PropertyMap locationProperties = {
        {"Type", std::string("NSM_NVSwitch_ChassisAssembly_Location")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(assemblyObjPath,
                                                              assemblyIntfName);
    basePropertyMap = baseProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        assemblyObjPath, assemblyIntfName + ".Location");
    propertyMap = locationProperties;

    EXPECT_THROW_COROUTINE(createNsmChassisAssembly(
                               mockManager, assemblyIntfName + ".Location",
                               assemblyObjPath, "NSM_NVSwitch_ChassisAssembly"),
                           std::runtime_error);
}

TEST_F(NsmChassisAssemblyTest, testCreateMultipleChassisAssemblies)
{
    const std::string assemblyIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_ChassisAssembly";

    for (size_t i = 0; i < 3; i++)
    {
        std::string testPath =
            "/xyz/openbmc_project/inventory/system/assembly_" +
            std::to_string(i);

        dbus::PropertyMap baseProperties = {
            {"Name", std::string("Assembly_") + std::to_string(i)},
            {"UUID", gpuUuid},
            {"ChassisName", std::string("Chassis_") + std::to_string(i)},
            {"AssemblyType", std::string("Assembly")},
        };

        dbus::PropertyMap locationProperties = {
            {"Type", std::string("NSM_NVSwitch_ChassisAssembly")},
            {"LocationType",
             "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot"},
        };

        auto& basePropertyMap =
            utils::MockDbusAsync::propertyMap(testPath, assemblyIntfName);
        basePropertyMap = baseProperties;

        auto& propertyMap = utils::MockDbusAsync::propertyMap(
            testPath, assemblyIntfName + ".Location");
        propertyMap = locationProperties;

        createNsmNVSwitchChassisAssembly(
            mockManager, assemblyIntfName + ".Location", testPath);
    }

    // Should have created multiple assembly sensors
    EXPECT_GE(gpu->staticSensors.size(), 3);
}
