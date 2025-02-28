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
#include "nsmAssetIntf.hpp"
#include "nsmChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

namespace nsm
{
requester::Coroutine
    nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath);
};

using namespace nsm;

struct NsmChassisAssemblyTest : public testing::Test, public utils::DBusTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";
    const std::string chassisName = "HGX_GPU_SXM_1";
    const std::string name = "Assembly1";
    const std::string objPath = chassisInventoryBasePath / chassisName / name;

    const uuid_t gpuUuid = "992b3ec1-e468-f145-8686-409009062aa8";
    const uuid_t fpgaUuid = "992b3ec1-e464-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>(gpuUuid)},
        {std::make_shared<NsmDevice>(fpgaUuid)},
    };
    NsmDevice& gpu = *devices[0];
    NsmDevice& fpga = *devices[1];

    NiceMock<MockSensorManager> mockManager{devices};

    const PropertyValuesCollection error = {
        {"Type", "NSM_GPU_ChasisAssembly"},
        {"UUID", "a3b0bdf6-8661-4d8e-8268-0e59415f2076"},
    };
    const PropertyValuesCollection basic = {
        {"ChassisName", chassisName},    {"Name", name},
        {"Type", "NSM_ChassisAssembly"}, {"UUID", gpuUuid},
        {"DeviceAssembly", true},
    };
    const PropertyValuesCollection area = {
        {"Type", "NSM_Area"},
        {"PhysicalContext",
         "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU"},
    };
    const PropertyValuesCollection asset = {
        {"Type", "NSM_Asset"},
        {"Name", "HGX_GPU_SXM_1"},
        {"Vendor", "NVIDIA"},
    };
    const PropertyValuesCollection health = {
        {"Type", "NSM_Health"},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
    };
    const PropertyValuesCollection location = {
        {"Type", "NSM_Location"},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
    };
};

TEST_F(NsmChassisAssemblyTest, badTestCreateDeviceSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(error, "Type"));
    values.push(objPath, get(basic, "UUID"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);

    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
TEST_F(NsmChassisAssemblyTest, goodTestCreateDeviceSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(basic, "Type"));
    values.push(objPath, get(basic, "UUID"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(area, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(area, "PhysicalContext"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Area",
                                    objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(health, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(health, "Health"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Health",
                                    objPath);
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(location, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(location, "LocationType"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Location",
                                    objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(4, gpu.roundRobinSensors.size());
    EXPECT_EQ(4, gpu.deviceSensors.size());
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AssemblyIntf>>(
                           gpu.deviceSensors[0]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
                           gpu.deviceSensors[1]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                           gpu.deviceSensors[2]));
    EXPECT_NE(nullptr, dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                           gpu.deviceSensors[3]));

    EXPECT_EQ(AreaIntf::PhysicalContextType::GPU,
              dynamic_pointer_cast<NsmInterfaceProvider<AreaIntf>>(
                  gpu.deviceSensors[1])
                  ->invoke(pdiMethod(physicalContext)));
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu.deviceSensors[2])
                  ->invoke(pdiMethod(health)));
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu.deviceSensors[3])
                  ->invoke(pdiMethod(locationType)));
}

TEST_F(NsmChassisAssemblyTest, goodTestCreateStaticSensors)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(asset, "Type"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(asset, "Vendor"));
    values.push(objPath, get(asset, "Name"));
    values.push(objPath, get(basic, "DeviceAssembly"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Asset",
                                    objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(4, gpu.roundRobinSensors.size());
    EXPECT_EQ(4, gpu.deviceSensors.size());
    for (int i = 0; i < 4; i++)
    {
        auto& sensor = gpu.deviceSensors[i];
        auto inventorySensor =
            dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(sensor);
        EXPECT_NE(nullptr, inventorySensor);
    }
    auto partNumber = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu.deviceSensors[0]);
    EXPECT_EQ(DEVICE_PART_NUMBER, partNumber->property);
    auto model = dynamic_pointer_cast<NsmInventoryProperty<NsmAssetIntf>>(
        gpu.deviceSensors[2]);
    EXPECT_EQ(get<std::string>(asset, "Vendor"),
              model->invoke(pdiMethod(manufacturer)));
    EXPECT_EQ(get<std::string>(asset, "Name"), model->invoke(pdiMethod(name)));
}

TEST_F(NsmChassisAssemblyTest, badTestNoDevideFound)
{
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "ChassisName"));
    values.push(objPath, get(basic, "Name"));
    values.push(objPath, get(asset, "Type"));
    values.push(objPath, get(error, "UUID"));
    nsmChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Asset",
                                    objPath);

    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
