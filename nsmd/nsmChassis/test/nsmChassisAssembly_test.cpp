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
#include "features/feature.hpp"
#include "nsmAssetIntf.hpp"
#include "nsmChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

namespace nsm
{
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
    const std::string chassisName = "HGX_GPU_SXM_1";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> fpga;
    // One device context per test, so assembly indexes start at 0
    nsm::features::DeviceContext dev;

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

        dev.name = chassisName;
        dev.kind = "gpu";
        dev.staticUuid = gpuUuid;
        dev.platformPrefix = "HGX_";
        dev.device = gpu;
    }

    const std::string physicalContextGpu =
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU";
    const std::string locationEmbedded =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";

    /** @brief Run the ChassisAssembly feature on the GPU device */
    requester::Coroutine createAssembly(const dbus::PropertyMap& params)
    {
        return nsm::features::createChassisAssemblyFeature(mockManager, dev,
                                                           params);
    }
};

// =============================================================================
// createChassisAssemblyFeature
// =============================================================================

TEST_F(NsmChassisAssemblyTest, goodTestCreateAssemblyFeatureAllAttributes)
{
    dbus::PropertyMap params = {
        {"Name", std::string("HGX_GPU_SXM_1")},
        {"AssemblyType", std::string("Device")},
        {"PhysicalContext", physicalContextGpu},
        {"LocationType", locationEmbedded},
    };

    auto rc = createAssembly(params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_FALSE(rc.exception());

    EXPECT_EQ(2, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(8, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // msgTypes sensor
    EXPECT_EQ(9, gpu->deviceSensors.size());     // msgTypes + 8 others

    auto assembly = dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(
        gpu->deviceSensors[1]); // Index 1, msgTypes at 0
    ASSERT_NE(nullptr, assembly);
    EXPECT_EQ("Assembly0", assembly->getName());

    // Asset sensors (indices 2-5: partNumber, serialNumber, model, buildDate)
    for (size_t i = 2; i <= 5; ++i)
    {
        EXPECT_NE(nullptr,
                  dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                      gpu->deviceSensors[i]));
    }

    // Health sensor (index 6)
    auto healthObj = dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
        gpu->deviceSensors[6]);
    ASSERT_NE(nullptr, healthObj);
    EXPECT_EQ(HealthIntf::HealthType::OK, healthObj->invoke(pdiMethod(health)));

    // Area sensor (index 7)
    auto areaObj = dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
        gpu->deviceSensors[7]);
    ASSERT_NE(nullptr, areaObj);
    EXPECT_EQ(AreaIntf::PhysicalContextType::GPU,
              areaObj->invoke(pdiMethod(physicalContext)));

    // Location sensor (index 8)
    auto locationObj = dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
        gpu->deviceSensors[8]);
    ASSERT_NE(nullptr, locationObj);
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              locationObj->invoke(pdiMethod(locationType)));
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateAssemblyFeatureAssetAndHealthOnly)
{
    // No PhysicalContext / LocationType: Assembly + 4 asset + health
    auto rc = createAssembly({{"Name", std::string("HGX_GPU_SXM_1")},
                              {"AssemblyType", std::string("Device")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);

    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(6, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size()); // msgTypes sensor
    EXPECT_EQ(7, gpu->deviceSensors.size());     // msgTypes + 6 others

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(
                           gpu->deviceSensors[1]));
    for (size_t i = 2; i <= 5; ++i)
    {
        EXPECT_NE(nullptr,
                  dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                      gpu->deviceSensors[i]));
    }
    auto healthObj = dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
        gpu->deviceSensors[6]);
    ASSERT_NE(nullptr, healthObj);
    EXPECT_EQ(HealthIntf::HealthType::OK, healthObj->invoke(pdiMethod(health)));
}

TEST_F(NsmChassisAssemblyTest, goodTestAssemblyTypeBoardPartNumber)
{
    auto rc = createAssembly({{"Name", std::string("Board Assembly")},
                              {"AssemblyType", std::string("Board")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // Assembly, partNumber, serialNumber, model, buildDate, health
    EXPECT_EQ(6, gpu->staticSensors.size());
}

TEST_F(NsmChassisAssemblyTest, goodTestAssemblyTypeFRUPartNumber)
{
    auto rc = createAssembly({{"Name", std::string("FRU Assembly")},
                              {"AssemblyType", std::string("FRU")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(6, gpu->staticSensors.size());
}

TEST_F(NsmChassisAssemblyTest, goodTestAssemblyTypeInvalidDefaultsToBoard)
{
    // Unknown AssemblyType: logged, defaults to BOARD_PART_NUMBER
    auto rc = createAssembly({{"Name", std::string("Invalid Assembly")},
                              {"AssemblyType", std::string("InvalidType")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(6, gpu->staticSensors.size());
}

TEST_F(NsmChassisAssemblyTest, goodTestAssemblyMissingPhysicalContext)
{
    // Asset + Health + Location created, Area NOT created
    auto rc = createAssembly({{"Name", std::string("HGX_GPU_SXM_1")},
                              {"LocationType", locationEmbedded}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(7, gpu->staticSensors.size());
    for (const auto& sensor : gpu->deviceSensors)
    {
        EXPECT_EQ(nullptr,
                  dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(sensor));
    }
}

TEST_F(NsmChassisAssemblyTest, goodTestAssemblyMissingLocationType)
{
    // Asset + Health + Area created, Location NOT created
    auto rc = createAssembly({{"Name", std::string("HGX_GPU_SXM_1")},
                              {"PhysicalContext", physicalContextGpu}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(7, gpu->staticSensors.size());
    for (const auto& sensor : gpu->deviceSensors)
    {
        EXPECT_EQ(
            nullptr,
            dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(sensor));
    }
}

TEST_F(NsmChassisAssemblyTest, goodTestAssemblyIndexesPerDevice)
{
    // Each assembly of a device takes the next "Assembly" index
    auto rc0 = createAssembly({{"Name", std::string("First")}});
    auto rc1 = createAssembly({{"Name", std::string("Second")}});
    EXPECT_EQ(rc0.data(), NSM_SUCCESS);
    EXPECT_EQ(rc1.data(), NSM_SUCCESS);
    // 4 static sensors per assembly when no optional attributes are given
    EXPECT_EQ(8, gpu->staticSensors.size());

    std::vector<std::string> names;
    for (const auto& sensor : gpu->deviceSensors)
    {
        if (dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(sensor))
        {
            names.push_back(sensor->getName());
        }
    }
    EXPECT_EQ((std::vector<std::string>{"Assembly0", "Assembly1"}), names);
}

TEST_F(NsmChassisAssemblyTest, badTestAssemblyNonStringParam)
{
    // A non-string attribute fails before the index is taken
    auto rc = createAssembly({{"Name", uint64_t{1}}});
    EXPECT_EQ(rc.data(), NSM_SW_ERROR_DATA);
    EXPECT_EQ(0, gpu->staticSensors.size());
    EXPECT_EQ(1, gpu->deviceSensors.size()); // msgTypes sensor

    // The failed assembly left no gap in the numbering
    rc = createAssembly({{"Name", std::string("Next")}});
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_GT(gpu->deviceSensors.size(), 1u);
    EXPECT_EQ("Assembly0", gpu->deviceSensors[1]->getName());
}

TEST_F(NsmChassisAssemblyTest, badTestAssemblyInvalidPhysicalContext)
{
    // An unknown enum string throws before any object is created
    EXPECT_THROW_COROUTINE(
        createAssembly({{"Name", std::string("HGX_GPU_SXM_1")},
                        {"PhysicalContext", std::string("NotAContext")}}),
        std::exception);
    EXPECT_EQ(0, gpu->staticSensors.size());
}

TEST_F(NsmChassisAssemblyTest, badTestAssemblyInvalidLocationType)
{
    EXPECT_THROW_COROUTINE(
        createAssembly({{"Name", std::string("HGX_GPU_SXM_1")},
                        {"LocationType", std::string("NotALocation")}}),
        std::exception);
    EXPECT_EQ(0, gpu->staticSensors.size());
}

// =============================================================================
// Legacy NSM_NVSwitch_ChassisAssembly creators (still registered)
// =============================================================================

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
