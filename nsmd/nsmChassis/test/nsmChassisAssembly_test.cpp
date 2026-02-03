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
NsmDeviceTable devices;
std::shared_ptr<MockNsmDeviceBase> gpu;
std::shared_ptr<MockNsmDeviceBase> fpga;
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

    //  NiceMock<MockSensorManager> mockManager{devices};

    NsmChassisAssemblyTest() : SensorManagerTest(devices)
    {
        if (fpga)
        {
            fpga->staticSensors.clear();
            fpga->deviceSensors.clear();
            fpga->roundRobinSensors.clear();
            fpga->prioritySensors.clear();
        }
        if (gpu)
        {
            gpu->staticSensors.clear();
            gpu->deviceSensors.clear();
            gpu->roundRobinSensors.clear();
            gpu->prioritySensors.clear();
        }
    }

    const PropertyValuesCollection error = {
        {"Type", "NSM_GPU_ChasisAssembly"},
        {"UUID", "a3b0bdf6-8661-4d8e-8268-0e59415f2076"},
    };
    const PropertyValuesCollection basic = {
        {"ChassisName", chassisName},    {"Name", name},
        {"Type", "NSM_ChassisAssembly"}, {"UUID", gpuUuid},
        {"AssemblyType", "Device"},
    };
    const PropertyValuesCollection chassisAttributes = {
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
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    propertyMap["ChassisName"] =
        std::get<std::string>(get(basic, "ChassisName").second);
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);

    // Set up propertyMap with the invalid type to ensure it's used
    propertyMap["Type"] = std::get<std::string>(get(error, "Type").second);

    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_EQ(1, devices.size());
    gpu = dynamic_pointer_cast<MockNsmDeviceBase>(devices[0]);
    EXPECT_EQ(1, gpu->deviceSensors.size());
    EXPECT_EQ(1, gpu->roundRobinSensors.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["ChassisName"] =
        std::get<std::string>(get(basic, "ChassisName").second);
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);
    propertyMap["AssemblyType"] =
        std::get<std::string>(get(basic, "AssemblyType").second);

    // First call: NSM_ChassisAssembly
    propertyMap["Type"] = std::get<std::string>(get(basic, "Type").second);
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);

    // Second call: NSM_Chassis_Attributes
    propertyMap["Type"] =
        std::get<std::string>(get(chassisAttributes, "Type").second);
    propertyMap["PhysicalContext"] =
        std::get<std::string>(get(chassisAttributes, "PhysicalContext").second);
    propertyMap["LocationType"] =
        std::get<std::string>(get(chassisAttributes, "LocationType").second);
    nsmChassisAssemblyCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath);
    EXPECT_EQ(1, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(8, gpu->staticSensors.size());
    EXPECT_EQ(0, gpu->roundRobinSensors.size());
    EXPECT_EQ(8, gpu->deviceSensors.size());

    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(
                           gpu->deviceSensors[0]));

    // Check asset sensors (indices 1-4: partNumber, serialNumber, model,
    // buildDate)
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

    // Check area sensor (index 6)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
                           gpu->deviceSensors[6]));
    EXPECT_EQ(AreaIntf::PhysicalContextType::GPU,
              dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
                  gpu->deviceSensors[6])
                  ->invoke(pdiMethod(physicalContext)));

    // Check location sensor (index 7)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu->deviceSensors[7]));
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu->deviceSensors[7])
                  ->invoke(pdiMethod(locationType)));
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateStaticSensors)
{
    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["ChassisName"] =
        std::get<std::string>(get(basic, "ChassisName").second);
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = std::get<uuid_t>(get(basic, "UUID").second);
    propertyMap["AssemblyType"] =
        std::get<std::string>(get(basic, "AssemblyType").second);

    // Set up interface-specific properties for Chassis Asset and Health
    propertyMap["Type"] =
        std::get<std::string>(get(chassisAttributes, "Type").second);
    propertyMap["Name"] =
        std::get<std::string>(get(chassisAttributes, "Name").second);
    nsmChassisAssemblyCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath);
    EXPECT_EQ(1, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(5, gpu->staticSensors.size());
    EXPECT_EQ(0, gpu->roundRobinSensors.size());
    EXPECT_EQ(5, gpu->deviceSensors.size());

    // Check asset sensors (indices 0-3: partNumber, serialNumber, model,
    // buildDate)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[0]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[1]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[2]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
                           gpu->deviceSensors[3]));

    // Check health sensor (index 4)
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu->deviceSensors[4]));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu->deviceSensors[4])
                  ->invoke(pdiMethod(health)));
}

TEST_F(NsmChassisAssemblyTest, badTestNoDevideFound)
{
    // Clear all sensors from previous tests to ensure isolation

    auto& propertyMap = utils::MockDbusAsync::getPropertyMap();
    propertyMap.clear();

    // Set up base properties with INVALID UUID that doesn't match any device
    const uuid_t invalidUuid =
        "a3b0bdf6-8661-4d8e-8268-0e59415f2076"; // Different from gpuUuid and
                                                // fpgaUuid
    propertyMap["ChassisName"] =
        std::get<std::string>(get(basic, "ChassisName").second);
    propertyMap["Name"] = std::get<std::string>(get(basic, "Name").second);
    propertyMap["UUID"] = invalidUuid; // Invalid UUID as uuid_t type
    propertyMap["AssemblyType"] =
        std::get<std::string>(get(basic, "AssemblyType").second);

    // Set up interface-specific properties
    propertyMap["Type"] =
        std::get<std::string>(get(chassisAttributes, "Type").second);
    nsmChassisAssemblyCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath);

    EXPECT_EQ(1, devices.size());
    EXPECT_EQ(0, gpu->prioritySensors.size());
    EXPECT_EQ(0, gpu->roundRobinSensors.size());
    EXPECT_EQ(0, gpu->deviceSensors.size());
}
