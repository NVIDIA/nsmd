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
 * Branch coverage for nsmFpgaProcessor.cpp:
 * - NsmFpgaProcessor constructor with various parameters
 * - createNsmFpgaProcessorSensor factory: base fail, invalid UUID,
 *   NSM_FpgaProcessor type, missing properties, unknown type
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
struct NsmFpgaProcessorBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaProcessor";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmFpgaProcessorBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmFpgaProcessorBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ===========================================================================
// NsmFpgaProcessor constructor
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Constructor_AllParams)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "FPGA_Ctor";
    std::string type = "NSM_FpgaProcessor";
    std::string invPath = "/xyz/openbmc_project/inventory/system/fpga_ctor";
    std::vector<utils::Association> assocs;
    assocs.push_back({"parent", "child", "/xyz/openbmc_project/system/gpu0"});
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    NsmFpgaProcessor sensor(bus, name, type, invPath, assocs, fpgaType,
                            locationType, health);
    EXPECT_EQ(sensor.getName(), name);
}

TEST_F(NsmFpgaProcessorBranchTest, Constructor_EmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string name = "FPGA_Empty";
    std::string type = "NSM_FpgaProcessor";
    std::string invPath =
        "/xyz/openbmc_project/inventory/system/fpga_empty_assoc";
    std::vector<utils::Association> assocs;
    std::string fpgaType =
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete";
    std::string locationType =
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";

    NsmFpgaProcessor sensor(bus, name, type, invPath, assocs, fpgaType,
                            locationType, health);
    EXPECT_EQ(sensor.getName(), name);
}

// ===========================================================================
// Factory: base properties fail
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Factory_BasePropertiesFail)
{
    const std::string path = "/test/fpga_proc_br/no_base";
    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: invalid UUID -> no device
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Factory_InvalidUUID)
{
    const std::string path = "/test/fpga_proc_br/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_BadUUID");
    pm["UUID"] = std::string("STATIC:99:99:BAD:0");
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_bad");
    pm["Type"] = std::string("NSM_FpgaProcessor");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: NSM_FpgaProcessor type with all properties
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Factory_FpgaProcessor_AllProps)
{
    const std::string path = "/test/fpga_proc_br/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_AllProps");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_all/");
    pm["Type"] = std::string("NSM_FpgaProcessor");
    pm["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Embedded");
    pm["FpgaType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.FpgaType.FPGAType.Discrete");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: missing optional properties (FALSE branches)
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Factory_FpgaProcessor_MissingOptional)
{
    const std::string path = "/test/fpga_proc_br/missing_opt";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_MissOpt");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_missopt/");
    pm["Type"] = std::string("NSM_FpgaProcessor");
    // LocationType, FpgaType, Health omitted -> FALSE branches

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}

// ===========================================================================
// Factory: unknown type -> no sensor created
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Factory_UnknownType)
{
    const std::string path = "/test/fpga_proc_br/unknown_type";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = std::string("FPGA_Unknown");
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/fpga_unknown/");
    pm["Type"] = std::string("NSM_Unknown_Type");

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ===========================================================================
// Factory: missing Name, UUID, InventoryObjPath (FALSE branches)
// ===========================================================================
TEST_F(NsmFpgaProcessorBranchTest, Factory_MissingBaseProps)
{
    const std::string path = "/test/fpga_proc_br/missing_base";
    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // Name, UUID, InventoryObjPath omitted
    pm["Type"] = std::string("NSM_FpgaProcessor");

    createNsmFpgaProcessorSensor(mockManager, baseIntf, path);
}
