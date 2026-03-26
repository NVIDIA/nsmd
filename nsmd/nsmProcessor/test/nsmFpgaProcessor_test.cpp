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
#include "nsmGpuOperationalStatus.hpp"
#include "nsmNumericSensor/nsmAltitudePressure.hpp"
#include "nsmNumericSensor/nsmThreshold.hpp"

#include <sdbusplus/bus.hpp>

#include <limits>

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

// ============================================================================
// addSensor<T> instantiation coverage
// ============================================================================

TEST_F(NsmFpgaProcessorFactoryTest, AddSensorNsmGpuOperationalStatus)
{
    auto& dbus = utils::DBusHandler::getBus();
    std::string invPath = "/xyz/openbmc_project/inventory/test/gpu_op_status";
    auto sensor = std::make_shared<NsmGpuOperationalStatus>(
        dbus, "GpuOpStatus_AS", "NSM_Processor", invPath);
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmFpgaProcessorFactoryTest, AddSensorNsmThreshold)
{
    auto sensorValue = std::make_shared<NsmNumericSensorValueAggregate>();
    auto sensor = std::make_shared<NsmThreshold>(
        "Threshold_AS", "NSM_Threshold", uint8_t(1), sensorValue);
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

TEST_F(NsmFpgaProcessorFactoryTest, AddSensorNsmAltitudePressure)
{
    auto& dbus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    std::string physicalContext = "GPU";
    auto sensor = std::make_shared<NsmAltitudePressure>(
        dbus, "AltitudePressure_AS", "altitude", associations, physicalContext,
        nullptr, 1000.0, std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN());
    size_t before = fpga->deviceSensors.size();
    fpga->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// createNsmFpgaProcessorSensor: base interface not registered at unique path →
// coGetCachedBaseProperties returns error → co_return rc (early return //
// at line 61 in nsmFpgaProcessor.cpp).
TEST_F(NsmFpgaProcessorFactoryTest,
       CreateFpgaProcessor_BasePropertiesFail_NoSensor)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";
    const std::string uniquePath = "/xyz/test/fpgaproc/base_fail_unique";
    // Register a sub-interface only; leave FPGA_PROCESSOR_INTERFACE absent.
    auto& other = utils::MockDbusAsync::propertyMap(uniquePath,
                                                    baseIntf + ".Sub");
    other["Type"] = std::string("NSM_FpgaProcessor");

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, baseIntf + ".Sub", uniquePath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// =============================================================================
// Branch coverage: factory property-count and type-check branches
// =============================================================================

struct NsmFpgaProcessorFactoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmFpgaProcessorFactoryBranchTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmFpgaProcessorFactoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap validProps = {
        {"Name", std::string("FPGA_Branch")},
        {"UUID", fpgaUuid},
        {"Type", std::string("NSM_FpgaProcessor")},
        {"InventoryObjPath",
         std::string("/xyz/test/fpgaproc/branch/FPGA_Branch")},
        {"LocationType",
         std::string("xyz.openbmc_project.Inventory.Decorator.Location."
                     "LocationTypes.Embedded")},
        {"FpgaType",
         std::string("xyz.openbmc_project.Inventory.Decorator.FpgaType."
                     "FPGAType.Discrete")},
        {"Health",
         std::string("xyz.openbmc_project.State.Decorator.Health.HealthType."
                     "OK")},
    };
};

// type != "NSM_FpgaProcessor" → FALSE branch of if(type=="NSM_FpgaProcessor")
// → no sensor created, no exception
TEST_F(NsmFpgaProcessorFactoryBranchTest, TypeMismatch_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/type_mismatch";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm["Type"] = std::string("NSM_OtherType"); // not NSM_FpgaProcessor

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "Name" → FALSE branch for count("Name") → name="" but
// inventoryObjPath is valid → sensor IS created
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingName_SensorCreated)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("Name");
    pm["InventoryObjPath"] =
        std::string("/xyz/test/fpgaproc/branch/FPGA_NoName"); // unique path

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// Missing "InventoryObjPath" → inventoryObjPath="" → invalid D-Bus path →
// exception caught by factory try-catch → no sensor
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingInventoryObjPath_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_invpath";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("InventoryObjPath");

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "LocationType" → locationType="" → invalid enum conversion →
// exception caught by factory try-catch → no sensor
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingLocationType_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_loctype";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("LocationType");

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "FpgaType" → fpgaType="" → invalid enum conversion →
// exception caught by factory try-catch → no sensor
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingFpgaType_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_fpgatype";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("FpgaType");

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "Health" → health="" → invalid enum conversion →
// exception caught by factory try-catch → no sensor
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingHealth_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_health";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("Health");

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "UUID" → count("UUID") FALSE → uuid="" →
// parseStaticUuid("") throws inside getNsmDeviceFromStaticUUID →
// caught by factory try-catch → no sensor
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("UUID"); // uuid="" → parseStaticUuid throws → caught

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "Type" → count("Type") FALSE → type="" →
// if (type == "NSM_FpgaProcessor") FALSE → no sensor created
TEST_F(NsmFpgaProcessorFactoryBranchTest, MissingType_NoSensor)
{
    const std::string testPath = "/xyz/test/fpgaproc/missing_type";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = validProps;
    pm.erase("Type"); // type="" → if(type=="NSM_FpgaProcessor") FALSE

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, intf, testPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}
