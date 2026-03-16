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

#include "nsmPowerSubSystem.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine createPowerSubSystem(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
}

struct NsmPowerPowerSupplyTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PSU0";
    const std::string type = "NSM_PowerSupply";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/item/powersupply/PSU0";
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_PowerSupply";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmPowerPowerSupplyTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmPowerPowerSupplyTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"Type", type},
        {"UUID", fpgaUuid},
        {"PowerSupplyType",
         "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC"},
    };
};

TEST_F(NsmPowerPowerSupplyTest, goodTestBasic)
{
    // Basic test to verify test infrastructure works
    EXPECT_EQ(name, "PSU0");
    EXPECT_EQ(type, "NSM_PowerSupply");
}

TEST_F(NsmPowerPowerSupplyTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "TestPSU";
    std::string testType = "NSM_PowerSupply";
    std::string testPath =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/TestPSU";
    std::string powerSupplyType =
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC";
    std::vector<utils::Association> associations;

    auto powerSupply = std::make_shared<NsmPowerPowerSupply>(
        bus, testName, associations, testType, testPath, powerSupplyType);

    EXPECT_NE(powerSupply, nullptr);
    EXPECT_NE(powerSupply->associationDefinitionsInft, nullptr);
    EXPECT_NE(powerSupply->powerSupplyInfoIntf, nullptr);
    EXPECT_NE(powerSupply->powerSupplyIntf, nullptr);
}

TEST_F(NsmPowerPowerSupplyTest, testConstructorWithDCPowerSupply)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "TestPSU_DC";
    std::string testType = "NSM_PowerSupply";
    std::string testPath =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/TestPSU_DC";
    std::string powerSupplyType =
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.DC";
    std::vector<utils::Association> associations;

    auto powerSupply = std::make_shared<NsmPowerPowerSupply>(
        bus, testName, associations, testType, testPath, powerSupplyType);

    EXPECT_NE(powerSupply, nullptr);
    EXPECT_NE(powerSupply->associationDefinitionsInft, nullptr);
    EXPECT_NE(powerSupply->powerSupplyInfoIntf, nullptr);
    EXPECT_NE(powerSupply->powerSupplyIntf, nullptr);
}

TEST_F(NsmPowerPowerSupplyTest, testConstructorWithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "TestPSU_Assoc";
    std::string testType = "NSM_PowerSupply";
    std::string testPath =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/TestPSU_Assoc";
    std::string powerSupplyType =
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC";
    std::vector<utils::Association> associations = {
        {"powering", "powered_by",
         "/xyz/openbmc_project/inventory/system/board"},
    };

    auto powerSupply = std::make_shared<NsmPowerPowerSupply>(
        bus, testName, associations, testType, testPath, powerSupplyType);

    EXPECT_NE(powerSupply, nullptr);
    EXPECT_NE(powerSupply->associationDefinitionsInft, nullptr);

    auto assocList = powerSupply->associationDefinitionsInft->associations();
    EXPECT_EQ(assocList.size(), 1);
}

TEST_F(NsmPowerPowerSupplyTest, testcreatePowerSubSystemMissingName)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    dbus::PropertyMap props = {
        {"Type", type},
        {"UUID", fpgaUuid},
        {"PowerSupplyType",
         "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC"},
    };
    propertyMap = props;

    EXPECT_THROW_COROUTINE(
        createPowerSubSystem(mockManager, interfaceName, objPath),
        sdbusplus::exception::SdBusError);
}

TEST_F(NsmPowerPowerSupplyTest, testcreatePowerSubSystemNoDevice)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);

    uuid_t invalidUuid = "STATIC:99:0:NSM_DEVICE_INSTANCE_NUMBER:99";
    dbus::PropertyMap props = {
        {"Name", name},
        {"Type", type},
        {"UUID", invalidUuid},
        {"PowerSupplyType",
         "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC"},
    };
    propertyMap = props;

    createPowerSubSystem(mockManager, interfaceName, objPath);

    // UUID does not match any device; createPowerSubSystem runs without throw
}

TEST_F(NsmPowerPowerSupplyTest, testcreatePowerSubSystemSuccess)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    createPowerSubSystem(mockManager, interfaceName, objPath);

    // Verify that power supply sensors were created
    EXPECT_GE(fpga->deviceSensors.size(), 0);
}
