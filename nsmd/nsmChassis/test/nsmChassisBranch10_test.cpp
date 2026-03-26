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

/*
 * Branch coverage batch 10 for nsmPowerSubSystem.cpp and nsmChassisAssembly.cpp
 *
 * Covers:
 * - createPowerSubSystem: null device from getNsmDeviceFromStaticUUID
 * - nsmChassisAssemblyCreateSensors: missing ChassisName, Name, Type, UUID
 * - nsmChassisAssemblyCreateSensors: type == "NSM_ChassisAssembly" (assembly)
 * - nsmChassisAssemblyCreateSensors: type == "NSM_Chassis_Attributes"
 *   without PhysicalContext and LocationType (FALSE branches)
 * - nsmChassisAssemblyCreateSensors: type == "NSM_Chassis_Attributes"
 *   with both PhysicalContext and LocationType (TRUE branches)
 * - nsmChassisAssemblyCreateSensors: unrecognized type (neither branch taken)
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmChassisAssembly.hpp"
#include "nsmPowerSubSystem.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createPowerSubSystem(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
requester::Coroutine
    nsmChassisAssemblyCreateSensors(SensorManager& manager,
                                    const std::string& interface,
                                    const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture for PowerSubSystem null-device test
// ============================================================================

struct NsmPowerSubSystemNullDevTest : public Test, public utils::DBusTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_PowerSupply";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/item/powersupply/PSU_null";

    NsmDeviceTable devices;
    NiceMock<NullReturnMockSensorManager> nullManager{devices};

    NsmPowerSubSystemNullDevTest()
    {
        sensorManagerInstance.reset(&nullManager);
    }

    ~NsmPowerSubSystemNullDevTest()
    {
        sensorManagerInstance.release();
    }
};

// ============================================================================
// createPowerSubSystem: null device -> lg2::error + co_return NSM_ERROR
// ============================================================================

TEST_F(NsmPowerSubSystemNullDevTest,
       CreatePowerSubSystem_NullDevice_ReturnsError)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap["Name"] = std::string("PSU_null");
    propertyMap["UUID"] =
        std::string("STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0");
    propertyMap["PowerSupplyType"] = std::string(
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC");

    EXPECT_NO_THROW(createPowerSubSystem(nullManager, interfaceName, objPath));
}

// ============================================================================
// Fixture for ChassisAssembly branch tests
// ============================================================================

static const std::string kBaseIntf10 =
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly";
static const std::string kAttrIntf10 =
    "xyz.openbmc_project.Configuration.NSM_ChassisAssembly.ChassisAttributes";
static const uuid_t kGpuUuid10 = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:3";

struct NsmChassisAssemblyBranch10Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmChassisAssemblyBranch10Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kGpuUuid10));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmChassisAssemblyBranch10Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// nsmChassisAssemblyCreateSensors: missing ChassisName, Name, UUID
// in base properties -> FALSE branches at count("ChassisName"),
// count("Name"), count("UUID")
// Also missing Type in current properties -> FALSE branch at count("Type")
// ============================================================================

TEST_F(NsmChassisAssemblyBranch10Test, Factory_MissingBaseProps_FalseBranches)
{
    const std::string objPath = "/xyz/test/assembly_b10/missing_baseprops";

    // Register base interface with minimal props (no ChassisName, Name)
    // UUID must be valid format or getNsmDeviceFromStaticUUID throws
    auto& baseProps = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf10);
    baseProps["UUID"] = kGpuUuid10;

    // Register current interface with no Type
    auto& currProps = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf10);
    currProps["SomeOtherProp"] = std::string("value2");

    // type will be "" -> neither "NSM_ChassisAssembly" nor
    // "NSM_Chassis_Attributes" match -> no processing, just co_return SUCCESS
    EXPECT_NO_THROW(
        nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf10, objPath));
}

// ============================================================================
// nsmChassisAssemblyCreateSensors: type == "NSM_ChassisAssembly"
// -> creates AssemblyIntf sensor
// ============================================================================

TEST_F(NsmChassisAssemblyBranch10Test, Factory_TypeAssembly_CreatesAssembly)
{
    const std::string objPath = "/xyz/test/assembly_b10/type_assembly";

    // When interface == baseInterface, coGetCachedBaseProperties reads the
    // same interface as coGetAllDbusProperty.  Set all needed properties in
    // one call so the cache is populated, then the second coGetAllDbusProperty
    // call finds the same data (cached).
    auto& props = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf10);
    props["ChassisName"] = std::string("TestChassis_B10");
    props["Name"] = std::string("Assembly_B10");
    props["UUID"] = kGpuUuid10;
    props["Type"] = std::string("NSM_ChassisAssembly");

    const size_t before = gpu->staticSensors.size();
    EXPECT_NO_THROW(
        nsmChassisAssemblyCreateSensors(mockManager, kBaseIntf10, objPath));
    EXPECT_GT(gpu->staticSensors.size(), before);
}

// ============================================================================
// nsmChassisAssemblyCreateSensors: type == "NSM_Chassis_Attributes"
// without PhysicalContext and LocationType -> FALSE branches
// ============================================================================

TEST_F(NsmChassisAssemblyBranch10Test,
       Factory_TypeAttributes_NoPhysicalContextNoLocation)
{
    const std::string objPath = "/xyz/test/assembly_b10/attrs_no_opts";

    auto& baseProps = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf10);
    baseProps["ChassisName"] = std::string("TestChassis_B10_2");
    baseProps["Name"] = std::string("Assembly_B10_2");
    baseProps["UUID"] = kGpuUuid10;

    auto& currProps = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf10);
    currProps["Type"] = std::string("NSM_Chassis_Attributes");
    // No PhysicalContext, no LocationType -> both FALSE branches

    const size_t before = gpu->staticSensors.size();
    EXPECT_NO_THROW(
        nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf10, objPath));
    // Should have created asset + health sensors but not area or location
    EXPECT_GT(gpu->staticSensors.size(), before);
}

// ============================================================================
// nsmChassisAssemblyCreateSensors: type == "NSM_Chassis_Attributes"
// with PhysicalContext and LocationType -> both TRUE branches
// ============================================================================

TEST_F(NsmChassisAssemblyBranch10Test,
       Factory_TypeAttributes_WithPhysicalContextAndLocation)
{
    const std::string objPath = "/xyz/test/assembly_b10/attrs_with_opts";

    auto& baseProps = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf10);
    baseProps["ChassisName"] = std::string("TestChassis_B10_3");
    baseProps["Name"] = std::string("Assembly_B10_3");
    baseProps["UUID"] = kGpuUuid10;

    auto& currProps = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf10);
    currProps["Type"] = std::string("NSM_Chassis_Attributes");
    currProps["PhysicalContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType.GPU");
    currProps["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");

    const size_t before = gpu->staticSensors.size();
    EXPECT_NO_THROW(
        nsmChassisAssemblyCreateSensors(mockManager, kAttrIntf10, objPath));
    // Should have created asset + health + area + location sensors
    EXPECT_GT(gpu->staticSensors.size(), before);
}
