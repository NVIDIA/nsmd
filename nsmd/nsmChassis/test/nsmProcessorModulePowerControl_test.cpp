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

#include "nsmProcessorModulePowerControl.hpp"

using namespace nsm;

struct NsmProcessorModulePowerControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "ProcessorModulePowerControl";
    const std::string type = "NSM_ProcessorModulePowerControl";
    const std::string path =
        "/xyz/openbmc_project/control/processor_module_power_cap/GPU0";

    NsmDeviceTable devices;
    std::shared_ptr<NsmProcessorModulePowerControl> powerControl;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::shared_ptr<NsmClearPowerCapIntf> clearPowerCapIntf;

    NsmProcessorModulePowerControlTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();

        std::vector<std::tuple<std::string, std::string, std::string>>
            associations_list;
        associations_list.push_back(
            {"chassis", "power_control",
             "/xyz/openbmc_project/inventory/system/chassis/HGX"});

        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
        clearPowerCapIntf = std::make_shared<NsmClearPowerCapIntf>(bus, path);

        powerControl = std::make_shared<NsmProcessorModulePowerControl>(
            bus, name, type, powerCapIntf, clearPowerCapIntf, path,
            associations_list);

        EXPECT_NE(powerControl, nullptr);
        EXPECT_EQ(powerControl->getName(), name);
        EXPECT_EQ(powerControl->getType(), type);
    }
};

TEST_F(NsmProcessorModulePowerControlTest, goodTestConstructor)
{
    EXPECT_NE(powerControl->powerCapIntf, nullptr);
    EXPECT_NE(powerControl->clearPowerCapIntf, nullptr);
    EXPECT_NE(powerControl->associationDefinitionsIntf, nullptr);
    EXPECT_NE(powerControl->decoratorAreaIntf, nullptr);
    EXPECT_TRUE(powerControl->powerCapIntf->powerCapEnable());
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestPhysicalContext)
{
    auto physicalCtx = powerControl->decoratorAreaIntf->physicalContext();
    EXPECT_EQ(physicalCtx,
              DecoratorAreaIntf::PhysicalContextType::GPUSubsystem);
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestClearPowerCapIntf)
{
    EXPECT_NE(clearPowerCapIntf, nullptr);
    EXPECT_EQ(clearPowerCapIntf->clearPowerCap(), 0);
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestPowerCapSettings)
{
    // Test setting power cap
    uint32_t powerCap = 400;
    powerCapIntf->powerCap(powerCap);
    EXPECT_EQ(powerCapIntf->powerCap(), powerCap);

    // Test power cap enable
    powerCapIntf->powerCapEnable(false);
    EXPECT_FALSE(powerCapIntf->powerCapEnable());

    powerCapIntf->powerCapEnable(true);
    EXPECT_TRUE(powerCapIntf->powerCapEnable());
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestMinMaxPowerCap)
{
    // Test min power cap
    uint32_t minPowerCap = 200;
    powerCapIntf->minPowerCapValue(minPowerCap);
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), minPowerCap);

    // Test max power cap
    uint32_t maxPowerCap = 600;
    powerCapIntf->maxPowerCapValue(maxPowerCap);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), maxPowerCap);
}

TEST_F(NsmProcessorModulePowerControlTest, goodTestAssociations)
{
    auto associations =
        powerControl->associationDefinitionsIntf->associations();
    EXPECT_EQ(associations.size(), 1);
    EXPECT_EQ(std::get<0>(associations[0]), "chassis");
    EXPECT_EQ(std::get<1>(associations[0]), "power_control");
}

TEST_F(NsmProcessorModulePowerControlTest, testGenRequestMsg)
{
    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto result = powerControl->genRequestMsg(eid, instanceId);

    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0);
}

TEST_F(NsmProcessorModulePowerControlTest, testGenRequestMsgMultipleCalls)
{
    eid_t eid = 10;

    auto result1 = powerControl->genRequestMsg(eid, 1);
    auto result2 = powerControl->genRequestMsg(eid, 2);
    auto result3 = powerControl->genRequestMsg(eid, 3);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmProcessorModulePowerControlTest, testGenRequestMsgWithDifferentEids)
{
    auto result1 = powerControl->genRequestMsg(10, 1);
    auto result2 = powerControl->genRequestMsg(20, 1);
    auto result3 = powerControl->genRequestMsg(255, 1);

    EXPECT_TRUE(result1.has_value());
    EXPECT_TRUE(result2.has_value());
    EXPECT_TRUE(result3.has_value());
}

TEST_F(NsmProcessorModulePowerControlTest, testClearPowerCapIntfDefaultPowerCap)
{
    // Test default power cap value
    uint32_t defaultCap = 500;
    clearPowerCapIntf->defaultPowerCap(defaultCap);
    EXPECT_EQ(clearPowerCapIntf->defaultPowerCap(), defaultCap);
}

TEST_F(NsmProcessorModulePowerControlTest, testPowerCapIntfBoundaryValues)
{
    // Test with minimum value
    powerCapIntf->minPowerCapValue(100);
    EXPECT_EQ(powerCapIntf->minPowerCapValue(), 100);

    // Test with maximum value
    powerCapIntf->maxPowerCapValue(1000);
    EXPECT_EQ(powerCapIntf->maxPowerCapValue(), 1000);

    // Test power cap within range
    powerCapIntf->powerCap(500);
    EXPECT_EQ(powerCapIntf->powerCap(), 500);
}

TEST_F(NsmProcessorModulePowerControlTest, testMultipleAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string path2 =
        "/xyz/openbmc_project/control/processor_module_power_cap/GPU1";

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list2;
    associations_list2.push_back(
        {"chassis", "power_control",
         "/xyz/openbmc_project/inventory/system/chassis/HGX_GPU1"});
    associations_list2.push_back(
        {"system", "controlled_by", "/xyz/openbmc_project/inventory/system"});

    auto powerCapIntf2 = std::make_shared<PowerCapIntf>(bus, path2.c_str());
    auto clearPowerCapIntf2 = std::make_shared<NsmClearPowerCapIntf>(bus,
                                                                     path2);

    auto powerControl2 = std::make_shared<NsmProcessorModulePowerControl>(
        bus, "ProcessorModulePowerControl2", type, powerCapIntf2,
        clearPowerCapIntf2, path2, associations_list2);

    auto associations =
        powerControl2->associationDefinitionsIntf->associations();
    EXPECT_EQ(associations.size(), 2);
}

TEST_F(NsmProcessorModulePowerControlTest,
       testPatchPowerLimitInProgressInitialState)
{
    EXPECT_FALSE(powerControl->patchPowerLimitInProgress);
}

TEST_F(NsmProcessorModulePowerControlTest, testGetNameAndType)
{
    EXPECT_EQ(powerControl->getName(), name);
    EXPECT_EQ(powerControl->getType(), type);
}

TEST_F(NsmProcessorModulePowerControlTest, testPowerCapEnableToggle)
{
    powerCapIntf->powerCapEnable(true);
    EXPECT_TRUE(powerCapIntf->powerCapEnable());

    powerCapIntf->powerCapEnable(false);
    EXPECT_FALSE(powerCapIntf->powerCapEnable());

    powerCapIntf->powerCapEnable(true);
    EXPECT_TRUE(powerCapIntf->powerCapEnable());
}

TEST_F(NsmProcessorModulePowerControlTest, testPowerCapValueRange)
{
    // Set min and max
    powerCapIntf->minPowerCapValue(200);
    powerCapIntf->maxPowerCapValue(800);

    // Test value within range
    powerCapIntf->powerCap(400);
    EXPECT_EQ(powerCapIntf->powerCap(), 400);

    powerCapIntf->powerCap(600);
    EXPECT_EQ(powerCapIntf->powerCap(), 600);
}

TEST_F(NsmProcessorModulePowerControlTest, testClearPowerCapIntfPath)
{
    EXPECT_EQ(powerControl->path, path);
}

struct NsmModulePowerLimitTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<PowerCapIntf> powerCapIntf;
    std::string name = "ModulePowerLimit";
    std::string type = "NSM_ModulePowerLimit";
    std::string path = "/xyz/openbmc_project/test/power_limit";

    NsmModulePowerLimitTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        powerCapIntf = std::make_shared<PowerCapIntf>(bus, path.c_str());
    }
};

TEST_F(NsmModulePowerLimitTest, testConstructorMaximumPowerLimit)
{
    uint8_t propertyId = MAXIMUM_MODULE_POWER_LIMIT;
    std::string testName = "MaxPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, propertyId, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->propertyId, propertyId);
    EXPECT_EQ(powerLimit->propertyName, "MAXIMUM_MODULE_POWER_LIMIT");
}

TEST_F(NsmModulePowerLimitTest, testConstructorMinimumPowerLimit)
{
    uint8_t propertyId = MINIMUM_MODULE_POWER_LIMIT;
    std::string testName = "MinPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, propertyId, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->propertyId, propertyId);
    EXPECT_EQ(powerLimit->propertyName, "MINIMUM_MODULE_POWER_LIMIT");
}

TEST_F(NsmModulePowerLimitTest, testConstructorUnknownProperty)
{
    uint8_t propertyId = 99; // Unknown property ID
    std::string testName = "UnknownPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto powerLimit = std::make_shared<NsmModulePowerLimit>(
        testName, testType, propertyId, powerCapIntf);

    EXPECT_NE(powerLimit, nullptr);
    EXPECT_EQ(powerLimit->propertyId, propertyId);
    EXPECT_EQ(powerLimit->propertyName, "");
}

TEST_F(NsmModulePowerLimitTest, testMultiplePowerLimitInstances)
{
    std::string maxName = "MaxPowerLimit";
    std::string minName = "MinPowerLimit";
    std::string testType = "NSM_ModulePowerLimit";

    auto maxPowerLimit = std::make_shared<NsmModulePowerLimit>(
        maxName, testType, MAXIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    auto minPowerLimit = std::make_shared<NsmModulePowerLimit>(
        minName, testType, MINIMUM_MODULE_POWER_LIMIT, powerCapIntf);

    EXPECT_NE(maxPowerLimit, nullptr);
    EXPECT_NE(minPowerLimit, nullptr);
    EXPECT_NE(maxPowerLimit->propertyName, minPowerLimit->propertyName);
}
