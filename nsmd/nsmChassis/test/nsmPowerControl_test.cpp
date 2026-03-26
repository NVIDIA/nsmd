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

#include "nsmObjectFactory.hpp"
#include "nsmPowerControl.hpp"

namespace nsm
{
requester::Coroutine createControlGpuPower(SensorManager& manager,
                                           const std::string& interface,
                                           const std::string& objPath);
} // namespace nsm

using namespace nsm;

struct NsmPowerControlTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "PowerControl";
    const std::string type = "NSM_PowerControl";
    const std::string path = "/xyz/openbmc_project/control/power_cap/HGX";
    const std::string physicalContext = "SystemBoard";

    NsmDeviceTable devices;
    std::shared_ptr<NsmPowerControl> powerControl;

    NsmPowerControlTest() : SensorManagerTest(devices) {}

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        std::vector<utils::Association> associations;
        associations.push_back(
            {"chassis", "power_control",
             "/xyz/openbmc_project/inventory/system/chassis/HGX"});
        std::string typeCopy = type;
        powerControl = std::make_shared<NsmPowerControl>(
            bus, name, associations, typeCopy, path, physicalContext);
        EXPECT_NE(powerControl, nullptr);
        EXPECT_EQ(powerControl->getName(), name);
        EXPECT_EQ(powerControl->getType(), type);
    }
};

TEST_F(NsmPowerControlTest, goodTestConstructor)
{
    EXPECT_NE(powerControl->decoratorAreaIntf, nullptr);
    EXPECT_NE(powerControl->associationDefinitionsInft, nullptr);
    EXPECT_NE(powerControl->powerModeIntf, nullptr);
    EXPECT_NE(powerControl->clearPowerCapAsyncIntf, nullptr);
}

TEST_F(NsmPowerControlTest, goodTestPowerCapEnable)
{
    EXPECT_TRUE(powerControl->powerCapEnable());
}

TEST_F(NsmPowerControlTest, goodTestGetMaxPowerCapValue)
{
    // Verify getter is callable (no throw)
    (void)powerControl->maxPowerCapValue();
}

TEST_F(NsmPowerControlTest, goodTestGetMinPowerCapValue)
{
    // Verify getter is callable (no throw)
    (void)powerControl->minPowerCapValue();
}

TEST_F(NsmPowerControlTest, goodTestSetPowerCap)
{
    uint32_t powerCap = 5000;
    powerControl->powerCap(powerCap);
    EXPECT_EQ(powerControl->powerCap(), powerCap);
}

TEST_F(NsmPowerControlTest, goodTestPowerMode)
{
    using Mode = sdbusplus::common::xyz::openbmc_project::control::power::Mode;
    EXPECT_EQ(powerControl->powerModeIntf->powerMode(), Mode::PowerMode::OEM);
}

TEST_F(NsmPowerControlTest, goodTestPhysicalContext)
{
    using AreaIntf =
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area;
    EXPECT_EQ(powerControl->decoratorAreaIntf->physicalContext(),
              AreaIntf::PhysicalContextType::SystemBoard);
}

// Test for createControlGpuPower function
TEST_F(NsmPowerControlTest, goodTestCreateControlGpuPower)
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_ControlTotalGPUPower";
    const std::string controlName = "ControlTotalGPUPower";
    const std::string objPath = "/xyz/openbmc_project/control/power_cap/" +
                                controlName;
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    propertyMap["Name"] = controlName;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["PhysicalContext"] = std::string("SystemBoard");

    auto fpga = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
    EXPECT_NE(fpga, nullptr);

    size_t initialSensorCount = fpga->deviceSensors.size();
    createControlGpuPower(mockManager, basicIntfName, objPath);

    EXPECT_GT(fpga->deviceSensors.size(), initialSensorCount);
}

// =============================================================================
// createControlGpuPower factory branch coverage
// =============================================================================

struct NsmControlGpuPowerFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_ControlTotalGPUPower";
    const std::string basePath =
        "/xyz/openbmc_project/control/power_cap/factory";
    const std::string controlName = "FactoryPowerControl";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmControlGpuPowerFactoryTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmControlGpuPowerFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", controlName},
        {"UUID", fpgaUuid},
        {"PhysicalContext", std::string("SystemBoard")},
    };
};

// Full properties (Name+UUID+PhysicalContext) → sensor created (all TRUE
// branches)
TEST_F(NsmControlGpuPowerFactoryTest, Factory_FullProperties_SensorCreated)
{
    const std::string testPath = basePath + "/full";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    propertyMap = basicProperties;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// Missing "Name" → FALSE branch for count("Name") → empty name → invalid
// D-Bus path (trailing slash) → exception caught by createObjects → no sensor
TEST_F(NsmControlGpuPowerFactoryTest, Factory_MissingName_NoSensor)
{
    const std::string testPath = basePath + "/noname";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("Name");
    propertyMap = props;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing "PhysicalContext" → FALSE branch for count("PhysicalContext") →
// empty string → invalid enum conversion → exception → no sensor
TEST_F(NsmControlGpuPowerFactoryTest, Factory_MissingPhysicalContext_NoSensor)
{
    const std::string testPath = basePath + "/nophysical";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props["Name"] =
        std::string("FactoryPCNoPhysical"); // unique name avoids path collision
    props.erase("PhysicalContext");
    propertyMap = props;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// Missing UUID → parseStaticUuid throws → caught by createObjects → no sensor
TEST_F(NsmControlGpuPowerFactoryTest, Factory_MissingUUID_NoSensor)
{
    const std::string testPath = basePath + "/nouuid";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(testPath,
                                                          interfaceName);
    dbus::PropertyMap props = basicProperties;
    props.erase("UUID");
    propertyMap = props;

    const size_t before = fpga->deviceSensors.size();
    NsmObjectFactory::instance().createObjects(mockManager, interfaceName,
                                               testPath);

    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// =============================================================================
// NsmPowerControl::setPowerCap branch coverage
// =============================================================================

// Non-uint32_t value → !powerLimit branch → throws InvalidArgument
TEST_F(NsmPowerControlTest, SetPowerCap_WrongType_ThrowsInvalidArgument)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("not-a-number");

    EXPECT_THROW_COROUTINE(
        powerControl->setPowerCap(value, &status, nullptr),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// uint32_t value > maxPowerCapValue() (=0 when list empty) → out-of-range
// branch → InvalidArgument status, no throw
TEST_F(NsmPowerControlTest, SetPowerCap_OutOfRange_SetsInvalidArgument)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t(100); // > max=0

    powerControl->setPowerCap(value, &status, nullptr);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// uint32_t value == 0 and powerCapList empty → range check passes, loop
// iterates 0 times → returns Success (loop-body branch NOT taken)
TEST_F(NsmPowerControlTest, SetPowerCap_ZeroValue_EmptyList_Success)
{
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = uint32_t(0); // 0 <= max=0 and 0 >= min=0

    powerControl->setPowerCap(value, &status, nullptr);

    // Status stays Success (no device failures)
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

// =============================================================================
// NsmChassisClearPowerCapAsyncIntf::clearPowerCap branch coverage
// =============================================================================

// Happy path: AsyncOperationManager allocates a slot → non-empty objPath
TEST_F(NsmPowerControlTest, ClearPowerCapAsync_HappyPath_ReturnsObjectPath)
{
    auto objPath = powerControl->clearPowerCapAsyncIntf->clearPowerCap();
    EXPECT_FALSE(std::string(objPath).empty());
}

// =============================================================================
// NsmPowerControl::updatePowerCapValue branch coverage
// =============================================================================

// Single child → powerCapChildValues has one entry → totalValue = child value
TEST_F(NsmPowerControlTest, UpdatePowerCapValue_SingleChild_UpdatesTotal)
{
    powerControl->updatePowerCapValue("GPU0", 1500);
    EXPECT_EQ(powerControl->powerCap(), uint32_t(1500));
    EXPECT_TRUE(powerControl->powerCapEnable());
}

// Two children → totalValue = sum of both → powerCap updated accordingly
TEST_F(NsmPowerControlTest, UpdatePowerCapValue_TwoChildren_SumsValues)
{
    powerControl->updatePowerCapValue("GPU0", 1000);
    powerControl->updatePowerCapValue("GPU1", 2000);
    EXPECT_EQ(powerControl->powerCap(), uint32_t(3000));
}

// Updating same child twice → replaces old value → totalValue correct
TEST_F(NsmPowerControlTest, UpdatePowerCapValue_SameChildUpdated_ReplacesValue)
{
    powerControl->updatePowerCapValue("GPU0", 1000);
    powerControl->updatePowerCapValue("GPU0", 500);
    EXPECT_EQ(powerControl->powerCap(), uint32_t(500));
}

// =============================================================================
// NsmPowerControl::clearPowerCap (the int32_t legacy override) returns 0
// =============================================================================

TEST_F(NsmPowerControlTest, ClearPowerCap_ReturnsZero)
{
    EXPECT_EQ(powerControl->clearPowerCap(), 0);
}

// =============================================================================
// NsmPowerControl::defaultPowerCap with empty defaultPowerCapList
// =============================================================================

TEST_F(NsmPowerControlTest, DefaultPowerCap_EmptyList_ReturnsZero)
{
    // SensorManager::defaultPowerCapList is empty in test fixture
    EXPECT_EQ(powerControl->defaultPowerCap(), uint32_t(0));
}

// =============================================================================
// NsmPowerControl::maxPowerCapValue and minPowerCapValue with empty lists
// return 0 (already partially tested; here we assert the exact return value)
// =============================================================================

TEST_F(NsmPowerControlTest, MaxPowerCapValue_EmptyList_ReturnsZero)
{
    EXPECT_EQ(powerControl->maxPowerCapValue(), uint32_t(0));
}

TEST_F(NsmPowerControlTest, MinPowerCapValue_EmptyList_ReturnsZero)
{
    EXPECT_EQ(powerControl->minPowerCapValue(), uint32_t(0));
}
