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
 * Branch coverage batch 2 for nsmFpgaProcessor.cpp
 *
 * Targets remaining uncovered branches:
 * - createNsmFpgaProcessorSensor: missing Name property
 * - createNsmFpgaProcessorSensor: missing UUID property
 * - createNsmFpgaProcessorSensor: missing Type property
 * - createNsmFpgaProcessorSensor: missing InventoryObjPath property
 * - createNsmFpgaProcessorSensor: null device from getNsmDeviceFromStaticUUID
 * - createNsmFpgaProcessorSensor: exception catch block
 * - createNsmFpgaProcessorSensor: coGetCachedBaseProperties failure
 * - createNsmFpgaProcessorSensor: missing LocationType, FpgaType, Health
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmFpgaProcessor.hpp"

namespace nsm
{
requester::Coroutine createNsmFpgaProcessorSensor(SensorManager& manager,
                                                  const std::string& interface,
                                                  const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================
struct NsmFpgaProcessorBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmFpgaProcessorBranch2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmFpgaProcessorBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ===========================================================================
// Factory: coGetCachedBaseProperties failure (no properties set)
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_BasePropertiesFail)
{
    const std::string path = "/test/fpga_br2/no_base";
    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: missing Name property -> name is empty string
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_MissingName)
{
    const std::string path = "/test/fpga_br2/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_no_name");
    pm["Type"] = std::string("NSM_FpgaProcessor");
    pm["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    pm["FpgaType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: missing UUID property -> uuid is empty, device lookup fails
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_MissingUUID)
{
    const std::string path = "/test/fpga_br2/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_NoUUID");
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_no_uuid");
    pm["Type"] = std::string("NSM_FpgaProcessor");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: missing Type property -> type is empty
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_MissingType)
{
    const std::string path = "/test/fpga_br2/no_type";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_NoType");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_no_type");

    // type is empty -> won't match "NSM_FpgaProcessor" -> no sensor created
    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: missing InventoryObjPath -> inventoryObjPath is empty
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_MissingInventoryObjPath)
{
    const std::string path = "/test/fpga_br2/no_invpath";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_NoInvPath");
    pm["UUID"] = gpuUuid;
    pm["Type"] = std::string("NSM_FpgaProcessor");
    pm["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    pm["FpgaType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: null device from getNsmDeviceFromStaticUUID
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_NullDevice)
{
    const std::string path = "/test/fpga_br2/null_dev";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_NullDev");
    pm["UUID"] = std::string("STATIC:99:99:NONEXISTENT:0");
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_null_dev");
    pm["Type"] = std::string("NSM_FpgaProcessor");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: valid UUID, type=NSM_FpgaProcessor but missing optional properties
// (LocationType, FpgaType, Health) -> empty strings passed to constructor
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_MissingOptionalProps)
{
    const std::string path = "/test/fpga_br2/no_optional";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_NoOptional");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_no_opt");
    pm["Type"] = std::string("NSM_FpgaProcessor");

    // Missing LocationType, FpgaType, Health -> they default to empty strings
    // This will likely throw in the constructor due to invalid enum conversion
    // which is caught by the exception handler
    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: full success path with all properties
// ===========================================================================
TEST_F(NsmFpgaProcessorBranch2Test, Factory_FullSuccess)
{
    const std::string path = "/test/fpga_br2/full_ok";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_FullOK");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_full_ok");
    pm["Type"] = std::string("NSM_FpgaProcessor");
    pm["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    pm["FpgaType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}
