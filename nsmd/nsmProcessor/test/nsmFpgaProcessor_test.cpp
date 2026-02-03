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
using namespace ::testing;

#include "nsmFpgaProcessor.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmFpgaProcessorSensor(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath);

}; // namespace nsm

auto bus = sdbusplus::bus::new_default();

TEST(NsmFpgaProcessor, Constructor)
{
    std::string name = "FPGA0";
    std::string type = "NSM_FpgaProcessor";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor0";
    std::vector<utils::Association> associations;
    associations.push_back({"parent_chassis", "fpga",
                            "/xyz/openbmc_project/inventory/system/chassis"});
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath, associations,
                              fpgaType, locationType, health);

    EXPECT_EQ(fpgaProc.getName(), name);
    EXPECT_EQ(fpgaProc.getType(), type);
}

TEST(NsmFpgaProcessor, ConstructorWithMultipleAssociations)
{
    std::string name = "FPGA1";
    std::string type = "NSM_FpgaProcessor";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor1";
    std::vector<utils::Association> associations;
    associations.push_back({"parent_chassis", "fpga",
                            "/xyz/openbmc_project/inventory/system/chassis"});
    associations.push_back({"connected_port", "fpga",
                            "/xyz/openbmc_project/inventory/system/port0"});
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning";

    NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath, associations,
                              fpgaType, locationType, health);

    EXPECT_EQ(fpgaProc.getName(), name);
    EXPECT_EQ(fpgaProc.getType(), type);
}

TEST(NsmFpgaProcessor, ConstructorWithEmptyAssociations)
{
    std::string name = "FPGA2";
    std::string type = "NSM_FpgaProcessor";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor2";
    std::vector<utils::Association> associations; // Empty
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Integrated";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical";

    NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath, associations,
                              fpgaType, locationType, health);

    EXPECT_EQ(fpgaProc.getName(), name);
    EXPECT_EQ(fpgaProc.getType(), type);
}

TEST(NsmFpgaProcessor, ConstructorWithDifferentFpgaTypes)
{
    std::string name = "FPGA3";
    std::string type = "NSM_FpgaProcessor";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor3";
    std::vector<utils::Association> associations;
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    std::vector<std::string> fpgaTypes = {
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete",
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Integrated"};

    for (const auto& fpgaType : fpgaTypes)
    {
        NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath,
                                  associations, fpgaType, locationType, health);
        EXPECT_EQ(fpgaProc.getName(), name);
    }
}

TEST(NsmFpgaProcessor, ConstructorWithDifferentLocationTypes)
{
    std::string name = "FPGA4";
    std::string type = "NSM_FpgaProcessor";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor4";
    std::vector<utils::Association> associations;
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    std::vector<std::string> locationTypes = {
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded",
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot",
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Socket"};

    for (auto& locationType : locationTypes)
    {
        NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath,
                                  associations, fpgaType, locationType, health);
        EXPECT_EQ(fpgaProc.getType(), type);
    }
}

TEST(NsmFpgaProcessor, ConstructorWithDifferentHealthStates)
{
    std::string name = "FPGA5";
    std::string type = "NSM_FpgaProcessor";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor5";
    std::vector<utils::Association> associations;
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";

    std::vector<std::string> healthStates = {
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical"};

    for (auto& health : healthStates)
    {
        NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath,
                                  associations, fpgaType, locationType, health);
        EXPECT_EQ(fpgaProc.getName(), name);
    }
}

TEST(NsmFpgaProcessor, ConstructorWithMultipleInstances)
{
    std::string type = "NSM_FpgaProcessor";
    std::vector<utils::Association> associations;
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    for (int i = 0; i < 3; i++)
    {
        std::string name = "FPGA" + std::to_string(i);
        std::string inventoryObjPath =
            "/xyz/openbmc_project/inventory/system/fpga/processor" +
            std::to_string(i);

        NsmFpgaProcessor fpgaProc(bus, name, type, inventoryObjPath,
                                  associations, fpgaType, locationType, health);
        EXPECT_EQ(fpgaProc.getName(), name);
        EXPECT_EQ(fpgaProc.getType(), type);
    }
}

struct NsmFpgaProcessorFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmFpgaProcessorFactoryTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmFpgaProcessorFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmFpgaProcessorFactoryTest, CreateNsmFpgaProcessorSensorSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor0";
    const std::string name = "FPGA0";
    const std::string type = "NSM_FpgaProcessor";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/processors/FPGA_0";
    const std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    const std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded";
    const std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    // All properties in one propertyMap (base + current combined)
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["Type"] = type;
    propertyMap["FpgaType"] = fpgaType;
    propertyMap["LocationType"] = locationType;
    propertyMap["Health"] = health;

    // Add association
    dbus::PropertyMap association = {
        {"Forward", "parent_chassis"},
        {"Backward", "all_processors"},
        {"AbsolutePath",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_FPGA_0"}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    createNsmFpgaProcessorSensor(mockManager, interface, objPath);

    EXPECT_EQ(2, fpga->deviceSensors.size());
    auto processor =
        std::dynamic_pointer_cast<NsmFpgaProcessor>(fpga->deviceSensors[1]);
    EXPECT_NE(nullptr, processor);
    EXPECT_EQ(name, processor->getName());
    EXPECT_EQ(type, processor->getType());
}

TEST_F(NsmFpgaProcessorFactoryTest, CreateNsmFpgaProcessorSensorInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/processor_invalid";
    const std::string name = "FPGAInvalid";
    const std::string type = "NSM_FpgaProcessor";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/processors/FPGA_Invalid";
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    // All properties in one propertyMap
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["UUID"] = invalidUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["Type"] = type;

    createNsmFpgaProcessorSensor(mockManager, interface, objPath);

    // Should have only the automatic first sensor
    EXPECT_EQ(1, fpga->deviceSensors.size());
}
