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

#include "nsmPCIeRetimer.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createPCIeRetimerChassis(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);

}; // namespace nsm

struct NsmPCIeRetimerChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PCIeRetimer0";
    const std::string type = "NSM_PCIeRetimer";

    NsmDeviceTable devices;
    std::shared_ptr<NsmPCIeRetimerChassis> retimerChassis;

    NsmPCIeRetimerChassisTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<utils::Association> associations;
        associations.push_back(
            {"chassis", "contained_by",
             "/xyz/openbmc_project/inventory/system/chassis/HGX"});

        retimerChassis = std::make_shared<NsmPCIeRetimerChassis>(
            bus, name, associations, type);

        EXPECT_NE(retimerChassis, nullptr);
        EXPECT_EQ(retimerChassis->getName(), name);
        EXPECT_EQ(retimerChassis->getType(), type);
    }
};

TEST_F(NsmPCIeRetimerChassisTest, goodTestConstructor)
{
    EXPECT_NE(retimerChassis->associationDef_, nullptr);
    EXPECT_NE(retimerChassis->asset_, nullptr);
    EXPECT_NE(retimerChassis->apSkuId_, nullptr);
    EXPECT_NE(retimerChassis->location_, nullptr);
    EXPECT_NE(retimerChassis->chassis_, nullptr);
    EXPECT_NE(retimerChassis->health_, nullptr);
}

TEST_F(NsmPCIeRetimerChassisTest, goodTestApSkuIdInitialization)
{
    // Test that sku is initialized to empty string
    EXPECT_EQ(retimerChassis->apSkuId_->sku(), "");
}

TEST_F(NsmPCIeRetimerChassisTest, goodTestLocationInitialization)
{
    // Test that location type is set to Embedded
    auto locationType = retimerChassis->location_->locationType();
    EXPECT_EQ(locationType, LocationIntf::LocationTypes::Embedded);
}

TEST_F(NsmPCIeRetimerChassisTest, goodTestChassisTypeInitialization)
{
    // Test that chassis type is set to Component
    auto chassisType = retimerChassis->chassis_->type();
    EXPECT_EQ(chassisType, ChassisIntf::ChassisType::Component);
}

TEST_F(NsmPCIeRetimerChassisTest, goodTestHealthInitialization)
{
    // Test that health is initialized to OK
    auto health = retimerChassis->health_->health();
    EXPECT_EQ(health, HealthIntf::HealthType::OK);
}

TEST_F(NsmPCIeRetimerChassisTest, goodTestAssociations)
{
    // Test that associations are properly set
    auto associations = retimerChassis->associationDef_->associations();
    EXPECT_EQ(associations.size(), 1);
    EXPECT_EQ(std::get<0>(associations[0]), "chassis");
    EXPECT_EQ(std::get<1>(associations[0]), "contained_by");
}

TEST_F(NsmPCIeRetimerChassisTest, testMultipleAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> multipleAssociations;
    multipleAssociations.push_back(
        {"chassis", "contained_by",
         "/xyz/openbmc_project/inventory/system/chassis/HGX"});
    multipleAssociations.push_back(
        {"parent", "child", "/xyz/openbmc_project/inventory/system"});

    auto retimer = std::make_shared<NsmPCIeRetimerChassis>(
        bus, "PCIeRetimer1", multipleAssociations, "NSM_PCIeRetimer");

    EXPECT_NE(retimer, nullptr);
    auto associations = retimer->associationDef_->associations();
    EXPECT_EQ(associations.size(), 2);
}

TEST_F(NsmPCIeRetimerChassisTest, testDifferentNames)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "contained_by",
         "/xyz/openbmc_project/inventory/system/chassis/HGX"});

    std::vector<std::string> names = {"Retimer0", "Retimer1", "PCIeRetimer_A"};
    for (const auto& testName : names)
    {
        auto retimer = std::make_shared<NsmPCIeRetimerChassis>(
            bus, testName, associations, "NSM_PCIeRetimer");
        EXPECT_EQ(retimer->getName(), testName);
    }
}

TEST_F(NsmPCIeRetimerChassisTest, testEmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> emptyAssociations;

    auto retimer = std::make_shared<NsmPCIeRetimerChassis>(
        bus, "PCIeRetimer_Empty", emptyAssociations, "NSM_PCIeRetimer");

    EXPECT_NE(retimer, nullptr);
    auto associations = retimer->associationDef_->associations();
    EXPECT_EQ(associations.size(), 0);
}

TEST_F(NsmPCIeRetimerChassisTest, testMultipleInstances)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "contained_by",
         "/xyz/openbmc_project/inventory/system/chassis/HGX"});

    auto retimer1 = std::make_shared<NsmPCIeRetimerChassis>(
        bus, "PCIeRetimer1", associations, "NSM_PCIeRetimer");
    auto retimer2 = std::make_shared<NsmPCIeRetimerChassis>(
        bus, "PCIeRetimer2", associations, "NSM_PCIeRetimer");

    EXPECT_NE(retimer1, nullptr);
    EXPECT_NE(retimer2, nullptr);
    EXPECT_NE(retimer1.get(), retimer2.get());
    EXPECT_NE(retimer1->getName(), retimer2->getName());
}

TEST_F(NsmPCIeRetimerChassisTest, testDifferentTypes)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "contained_by",
         "/xyz/openbmc_project/inventory/system/chassis/HGX"});

    std::vector<std::string> types = {"NSM_PCIeRetimer", "Type1", "Type2"};
    for (const auto& testType : types)
    {
        auto retimer = std::make_shared<NsmPCIeRetimerChassis>(
            bus, "PCIeRetimer_TypeTest", associations, testType);
        EXPECT_EQ(retimer->getType(), testType);
    }
}

struct NsmPCIeRetimerFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t retimerUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIeRetimerFactoryTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(retimerUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmPCIeRetimerFactoryTest, CreatePCIeRetimerChassisSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/PCIeRetimer0";
    const std::string name = "PCIeRetimer0";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["UUID"] = retimerUuid;

    dbus::PropertyMap association = {
        {"Forward", "chassis"},
        {"Backward", "contained_by"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/chassis/HGX"}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    createPCIeRetimerChassis(mockManager, interface, objPath);

    EXPECT_EQ(2, gpu->deviceSensors.size());
    auto retimer =
        std::dynamic_pointer_cast<NsmPCIeRetimerChassis>(gpu->deviceSensors[1]);
    EXPECT_NE(nullptr, retimer);
    EXPECT_EQ(name, retimer->getName());
}

TEST_F(NsmPCIeRetimerFactoryTest, CreatePCIeRetimerChassisInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/PCIeRetimer_invalid";
    const std::string name = "PCIeRetimerInvalid";
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["UUID"] = invalidUuid;

    EXPECT_THROW_COROUTINE(
        createPCIeRetimerChassis(mockManager, interface, objPath),
        std::runtime_error);

    // Should have only the automatic first sensor
    EXPECT_EQ(1, gpu->deviceSensors.size());
}
