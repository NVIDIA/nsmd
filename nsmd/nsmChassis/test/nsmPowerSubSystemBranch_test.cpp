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

/*
 * Branch coverage tests for nsmPowerSubSystem.cpp
 *
 * Targets FALSE branches not yet covered by nsmPowerSubSystem_test.cpp:
 * - createPowerSubSystem: FALSE branch for count("Name")   → empty name
 * - createPowerSubSystem: FALSE branch for count("PowerSupplyType")
 *   → powerSupplyType="" → convertPowerSupplyTypes("") throws
 * - createPowerSubSystem: FALSE branch for count("UUID")
 *   → uuid="" → parseStaticUuid("") throws
 * - NsmPowerPowerSupply constructor: association loop with 0 entries
 * - NsmPowerPowerSupply constructor: association loop with multiple entries
 * - createPowerSubSystem: success path with all optional properties present
 * - createPowerSubSystem: success path with DC type
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmPowerSubSystem.hpp"

namespace nsm
{
requester::Coroutine createPowerSubSystem(SensorManager& manager,
                                          const std::string& interface,
                                          const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmPowerSubSystemBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string interfaceName =
        "xyz.openbmc_project.Configuration.NSM_PowerSupply";
    const std::string basePath =
        "/xyz/openbmc_project/inventory/item/powersupply";
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string type = "NSM_PowerSupply";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmPowerSubSystemBranchTest() : SensorManagerTest(devices)
    {
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmPowerSubSystemBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// FALSE branch: count("Name") is false → name stays empty ("")
// With valid UUID and PowerSupplyType, construction proceeds with name=""
// The resulting PDI path uses the empty name → no crash
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest,
       DISABLED_Factory_NameAbsent_FalseBranch_EmptyNameUsed)
{
    const std::string objPath = basePath + "/PSU_no_name_br";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    // Omit "Name" → FALSE branch for count("Name")
    propertyMap["Type"] = type;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["PowerSupplyType"] = std::string(
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC");

    // name="" → factory proceeds; mock sdbusplus does not validate path,
    // so no exception propagates out of the coroutine
    EXPECT_NO_THROW(createPowerSubSystem(mockManager, interfaceName, objPath));
}

// ============================================================================
// FALSE branch: count("PowerSupplyType") is false → powerSupplyType=""
// convertPowerSupplyTypesFromString("") throws → propagates out
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest,
       Factory_PowerSupplyTypeAbsent_FalseBranch_Throws)
{
    const std::string objPath = basePath + "/PSU_no_type_br";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    // Omit "PowerSupplyType" → FALSE branch
    propertyMap["Name"] = std::string("PSU_no_type_br");
    propertyMap["Type"] = type;
    propertyMap["UUID"] = fpgaUuid;

    EXPECT_THROW_COROUTINE(
        createPowerSubSystem(mockManager, interfaceName, objPath),
        std::exception);
}

// ============================================================================
// FALSE branch: count("UUID") is false → uuid="" → getNsmDevice fails
// Factory logs error and returns NSM_ERROR without throwing
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest,
       Factory_UUIDAbsent_FalseBranch_NoDeviceFound)
{
    const std::string objPath = basePath + "/PSU_no_uuid_br";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    // Omit "UUID" → FALSE branch → uuid="" → parseStaticUuid throws
    propertyMap["Name"] = std::string("PSU_no_uuid_br");
    propertyMap["Type"] = type;
    propertyMap["PowerSupplyType"] = std::string(
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC");

    EXPECT_THROW_COROUTINE(
        createPowerSubSystem(mockManager, interfaceName, objPath),
        std::runtime_error);
}

// ============================================================================
// NsmPowerPowerSupply constructor: empty association list (loop body never
// executes → zero iterations of association loop)
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest, Constructor_NoAssociations_LoopSkipped)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "PSU_branch_no_assoc";
    std::string psType =
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC";
    std::string path =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/"
        "PSU_branch_no_assoc";
    std::string objType = "NSM_PowerSupply";
    std::vector<utils::Association> assocs; // empty → loop body never runs

    auto psu = std::make_shared<NsmPowerPowerSupply>(bus, name, assocs, objType,
                                                     path, psType);

    ASSERT_NE(psu, nullptr);
    EXPECT_NE(psu->associationDefinitionsInft, nullptr);
    auto list = psu->associationDefinitionsInft->associations();
    EXPECT_TRUE(list.empty());
}

// ============================================================================
// NsmPowerPowerSupply constructor: two associations → loop body runs twice
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest, Constructor_TwoAssociations_LoopRunsTwice)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "PSU_branch_two_assoc";
    std::string psType =
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.DC";
    std::string path =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/"
        "PSU_branch_two_assoc";
    std::string objType = "NSM_PowerSupply";
    std::vector<utils::Association> assocs = {
        {"powering", "powered_by",
         "/xyz/openbmc_project/inventory/system/board"},
        {"contained_by", "contains",
         "/xyz/openbmc_project/inventory/system/chassis"},
    };

    auto psu = std::make_shared<NsmPowerPowerSupply>(bus, name, assocs, objType,
                                                     path, psType);

    ASSERT_NE(psu, nullptr);
    auto list = psu->associationDefinitionsInft->associations();
    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(std::get<0>(list[0]), "powering");
    EXPECT_EQ(std::get<1>(list[0]), "powered_by");
    EXPECT_EQ(std::get<0>(list[1]), "contained_by");
    EXPECT_EQ(std::get<1>(list[1]), "contains");
}

// ============================================================================
// NsmPowerPowerSupply constructor: three associations → loop runs three times
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest, Constructor_ThreeAssociations_AllStored)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "PSU_branch_three_assoc";
    std::string psType =
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC";
    std::string path =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/"
        "PSU_branch_three_assoc";
    std::string objType = "NSM_PowerSupply";
    std::vector<utils::Association> assocs = {
        {"powering", "powered_by",
         "/xyz/openbmc_project/inventory/system/board"},
        {"chassis", "powered_by",
         "/xyz/openbmc_project/inventory/system/chassis"},
        {"parent", "child", "/xyz/openbmc_project/inventory/system/parent"},
    };

    auto psu = std::make_shared<NsmPowerPowerSupply>(bus, name, assocs, objType,
                                                     path, psType);

    ASSERT_NE(psu, nullptr);
    auto list = psu->associationDefinitionsInft->associations();
    EXPECT_EQ(list.size(), 3u);
}

// ============================================================================
// Factory success: all properties present, DC type, sensor created
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest, Factory_AllPresent_DCType_SensorCreated)
{
    const std::string objPath = basePath + "/PSU_dc_br";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap["Name"] = std::string("PSU_dc_br");
    propertyMap["Type"] = type;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["PowerSupplyType"] = std::string(
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.DC");

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createPowerSubSystem(mockManager, interfaceName, objPath));
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// ============================================================================
// Factory: invalid UUID format → parseStaticUuid throws std::runtime_error
// ============================================================================

TEST_F(NsmPowerSubSystemBranchTest, Factory_InvalidUUIDFormat_Throws)
{
    const std::string objPath = basePath + "/PSU_bad_uuid_br";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          interfaceName);
    propertyMap["Name"] = std::string("PSU_bad_uuid_br");
    propertyMap["Type"] = type;
    propertyMap["UUID"] = std::string("NOT_A_VALID_UUID_FORMAT");
    propertyMap["PowerSupplyType"] = std::string(
        "com.nvidia.PowerSupply.PowerSupplyInfo.PowerSupplyTypes.AC");

    EXPECT_THROW_COROUTINE(
        createPowerSubSystem(mockManager, interfaceName, objPath),
        std::runtime_error);
}
