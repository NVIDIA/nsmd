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

#include "nsmGpuChassisAssembly.hpp"
#include "nsmInventoryProperty.hpp"

namespace nsm
{
void nsmGpuChassisAssemblyCreateSensors(SensorManager& manager,
                                        const std::string& interface,
                                        const std::string& objPath);
};

using namespace nsm;

struct NsmGpuChassisAssemblyTest : public testing::Test, public utils::DBusTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_GPU_ChassisAssembly";
    const std::string chassisName = "HGX_GPU_SXM_1";
    const std::string name = "Assembly1";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/" + chassisName + "/" +
        name;

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
        {"ChassisName", chassisName},
        {"Name", name},
        {"Type", "NSM_GPU_ChassisAssembly"},
        {"UUID", gpuUuid},
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

TEST_F(NsmGpuChassisAssemblyTest, badTestCreateDeviceSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(error, "Type")))
        .WillOnce(Return(get(basic, "UUID")));
    EXPECT_NO_THROW(nsmGpuChassisAssemblyCreateSensors(mockManager,
                                                       basicIntfName, objPath));
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}
TEST_F(NsmGpuChassisAssemblyTest, goodTestCreateDeviceSensors)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(basic, "Type")))
        .WillOnce(Return(get(basic, "UUID")));
    nsmGpuChassisAssemblyCreateSensors(mockManager, basicIntfName, objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(area, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(area, "PhysicalContext")));
    nsmGpuChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Area",
                                       objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(health, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(health, "Health")));
    nsmGpuChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Health",
                                       objPath);
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(location, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(location, "LocationType")));
    nsmGpuChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Location",
                                       objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
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
                  ->pdi()
                  .physicalContext());
    EXPECT_EQ(HealthIntf::HealthType::OK,
              dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
                  gpu.deviceSensors[2])
                  ->pdi()
                  .health());
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
                  gpu.deviceSensors[3])
                  ->pdi()
                  .locationType());
}

TEST_F(NsmGpuChassisAssemblyTest, goodTestCreateStaticSensors)
{
    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(4)
        .WillRepeatedly(
            [](eid_t, Request&, const nsm_msg**,
               size_t*) -> requester::Coroutine { co_return NSM_SUCCESS; });
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(asset, "Type")))
        .WillOnce(Return(get(basic, "UUID")))
        .WillOnce(Return(get(asset, "Vendor")))
        .WillOnce(Return(get(asset, "Name")));
    nsmGpuChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Asset",
                                       objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(4, gpu.deviceSensors.size());
    for (int i = 0; i < 4; i++)
    {
        auto& sensor = gpu.deviceSensors[i];
        auto inventorySensor =
            dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(sensor);
        EXPECT_NE(nullptr, inventorySensor);
    }
    auto sensor = dynamic_pointer_cast<NsmInventoryProperty<AssetIntf>>(
        gpu.deviceSensors[2]);
    EXPECT_EQ(get<std::string>(asset, "Vendor"), sensor->pdi().manufacturer());
    EXPECT_EQ(get<std::string>(asset, "Name"), sensor->pdi().name());
}

TEST_F(NsmGpuChassisAssemblyTest, badTestNoDevideFound)
{
    EXPECT_CALL(mockDBus, getDbusPropertyVariant)
        .WillOnce(Return(get(basic, "ChassisName")))
        .WillOnce(Return(get(basic, "Name")))
        .WillOnce(Return(get(asset, "Type")))
        .WillOnce(Return(get(error, "UUID")))
        .WillOnce(Return(get(asset, "Vendor")))
        .WillOnce(Return(get(asset, "Name")));
    nsmGpuChassisAssemblyCreateSensors(mockManager, basicIntfName + ".Asset",
                                       objPath);
    EXPECT_EQ(0, fpga.prioritySensors.size());
    EXPECT_EQ(0, fpga.roundRobinSensors.size());
    EXPECT_EQ(0, fpga.deviceSensors.size());
    EXPECT_EQ(0, gpu.prioritySensors.size());
    EXPECT_EQ(0, gpu.roundRobinSensors.size());
    EXPECT_EQ(0, gpu.deviceSensors.size());
}