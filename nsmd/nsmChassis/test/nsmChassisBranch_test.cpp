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
 * Branch coverage for nsmd/nsmChassis/nsmChassis.cpp
 *
 * Covers:
 * - createChassisAttributes: FALSE branches when optional properties absent
 *   (LocationType, LocationCode, LocationContext, FieldReplaceable,
 *    ChassisType, PrettyNameForChassis, Priority in createPowerLimit)
 * - createWriteProtect: missing DeviceType → NSM_DEV_ID_UNKNOWN ≠ BASEBOARD
 * - createFPGAAttributes: TRUE branches with LocationType and ChassisType
 * - createOperationalStatus: missing optional properties FALSE branches
 * - createPowerState: missing optional properties FALSE branches
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "nsmChassis.hpp"

using namespace nsm;

namespace nsm
{
requester::Coroutine nsmChassisCreateSensors(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

struct NsmChassisBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string name = "HGX_GPU_Branch_1";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                name;

    // GPU device (NSM_DEV_ID_GPU)
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:4";
    // FPGA/Baseboard device (NSM_DEV_ID_BASEBOARD)
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;
    std::shared_ptr<MockNsmDevice> fpga;

    NsmChassisBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(gpu, nullptr);
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmChassisBranchTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// NSM_Chassis_Attributes: no optional properties → all FALSE branches in
// createChassisAttributes for LocationType, LocationCode, LocationContext,
// FieldReplaceable, ChassisType, PrettyNameForChassis, DimensionSupported,
// PowerLimitSupported, WriteProtectSupported, ResetMetricsSupported,
// GPIOStateSupported, ErrorInjectionSupported, DeviceDiagnosticsSupported
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_NoOptionalProps_AllFalseBranches)
{
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = name;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    // Only "Type" set — all optional properties absent
    // → covers FALSE branches for all if (allCurrentIfaceProperties.count("X"))
    //
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", objPath));
}

// ============================================================================
// NSM_Chassis_Attributes: AssetInformationAvailable=false → createAsset NOT
// called (FALSE branch of the && condition). Also DimensionSupported=false,
// PowerLimitSupported=false → FALSE branches for && conditions
// ============================================================================

TEST_F(NsmChassisBranchTest,
       ChassisAttributes_AssetAvailableFalse_FalseBranches)
{
    const std::string altName = "HGX_GPU_Branch_2";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    // AssetInformationAvailable=false → inner FALSE of && condition
    propertyMapAttributes["AssetInformationAvailable"] = bool(false);
    // DimensionSupported=false → FALSE branch of && for DimensionSupported
    propertyMapAttributes["DimensionSupported"] = bool(false);
    // PowerLimitSupported=false → FALSE branch of && for PowerLimitSupported
    propertyMapAttributes["PowerLimitSupported"] = bool(false);
    // WriteProtectSupported=false → FALSE branch of &&
    propertyMapAttributes["WriteProtectSupported"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createPowerLimit: Priority property present → covers L209 TRUE branch
// createChassisAttributes: LocationContext, FieldReplaceable, PrettyName TRUE
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_WithPriorityAndExtraProps)
{
    const std::string altName = "HGX_GPU_Branch_3";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["PowerLimitSupported"] = bool(true);
    propertyMapAttributes["Priority"] = bool(true); // covers L209 TRUE branch
    // LocationContext present → covers L173 TRUE branch in
    // createLocationContext
    propertyMapAttributes["LocationContext"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.LocationContext"
        ".Contexts.GPU");
    // FieldReplaceable present → covers L191 TRUE branch in
    // createFieldReplaceable
    propertyMapAttributes["FieldReplaceable"] = bool(true);
    // PrettyNameForChassis present → covers L227 TRUE branch in
    // createPrettyName
    propertyMapAttributes["PrettyNameForChassis"] = std::string("GPU SXM 1");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createWriteProtect: DeviceType missing → NSM_DEV_ID_UNKNOWN ≠ BASEBOARD →
// throws std::runtime_error (L249 TRUE branch, L243 FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, WriteProtect_MissingDeviceType_Throws)
{
    const std::string altName = "HGX_GPU_Branch_4";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    // Base property map WITHOUT DeviceType → stays NSM_DEV_ID_UNKNOWN
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;
    // DeviceType deliberately absent → L243 FALSE branch

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["WriteProtectSupported"] = bool(true);
    // createWriteProtect called; no DeviceType → NSM_DEV_ID_UNKNOWN ≠ BASEBOARD
    // → throws (L249 TRUE branch)

    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(
            mockManager, basicIntfName + ".ChassisAttributes", altObjPath),
        std::runtime_error);
}

// ============================================================================
// createWriteProtect: DeviceType=GPU (not BASEBOARD) → throws (L249 TRUE)
// ============================================================================

TEST_F(NsmChassisBranchTest, WriteProtect_GpuDevice_Throws)
{
    const std::string altName = "HGX_GPU_Branch_5";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;
    basePropertyMap["DeviceType"] =
        uint64_t(NSM_DEV_ID_GPU); // GPU not BASEBOARD

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["WriteProtectSupported"] = bool(true);
    // DeviceType=GPU ≠ BASEBOARD → throws

    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(
            mockManager, basicIntfName + ".ChassisAttributes", altObjPath),
        std::runtime_error);
}

// ============================================================================
// createFPGAAttributes: WITH LocationType and ChassisType → TRUE branches
// at L424 and L428 in createFPGAAttributes
// ============================================================================

TEST_F(NsmChassisBranchTest, FpgaAttributes_WithLocationTypeAndChassisType)
{
    const std::string altName = "HGX_FPGA_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_FPGA_Attributes");
    // LocationType present → covers L424 TRUE branch in createFPGAAttributes
    propertyMapAttributes["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    // ChassisType present → covers L428 TRUE branch in createFPGAAttributes
    propertyMapAttributes["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType"
        ".Module");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createOperationalStatus: missing DeviceType → 0 ≠ NSM_DEV_ID_BASEBOARD →
// throws (L448 TRUE). Also covers L443 FALSE (DeviceType absent)
// ============================================================================

TEST_F(NsmChassisBranchTest, OperationalStatus_MissingDeviceType_Throws)
{
    const std::string altName = "HGX_OpStatus_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    // No DeviceType in base props → deviceType=0 ≠ BASEBOARD → throws
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;
    // DeviceType absent → L443 FALSE branch

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".OperationalStatus");
    propertyMap["Type"] = std::string("NSM_OperationalStatus");

    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(
            mockManager, basicIntfName + ".OperationalStatus", altObjPath),
        std::runtime_error);
}

// ============================================================================
// createOperationalStatus: DeviceType=BASEBOARD, missing InstanceNumber,
// InventoryObjPaths, Priority → covers FALSE branches at L455, L462, L469
// ============================================================================

TEST_F(NsmChassisBranchTest,
       OperationalStatus_BaseboardMissingOptional_FalseBranches)
{
    const std::string altName = "HGX_OpStatus_Branch_2";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    // InstanceNumber and Priority absent → FALSE branches at L455, L469

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".OperationalStatus");
    propertyMap["Type"] = std::string("NSM_OperationalStatus");
    // InventoryObjPaths required to avoid empty-interfaces throw
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/sensor"};

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".OperationalStatus", altObjPath));
}

// ============================================================================
// createPowerState: DeviceType=BASEBOARD, missing InstanceNumber,
// InventoryObjPaths, Priority → covers FALSE branches at L500, L507, L514
// ============================================================================

TEST_F(NsmChassisBranchTest, PowerState_BaseboardMissingOptional_FalseBranches)
{
    const std::string altName = "HGX_PowerState_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    // InstanceNumber and Priority absent → FALSE branches at L500, L514

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = std::string("NSM_PowerState");
    // InventoryObjPaths required to avoid empty-interfaces throw
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/sensor"};

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".PowerState", altObjPath));
}

// ============================================================================
// createPowerState: DeviceType missing → 0 ≠ BASEBOARD → throws (L493 TRUE)
// ============================================================================

TEST_F(NsmChassisBranchTest, PowerState_MissingDeviceType_Throws)
{
    const std::string altName = "HGX_PowerState_Branch_2";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;
    // DeviceType absent → deviceType=0 ≠ BASEBOARD → throws

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = std::string("NSM_PowerState");

    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                                altObjPath),
        std::runtime_error);
}

// ============================================================================
// createChassisAttributes: DimensionSupported=true → createDimension called
// (covers L374/375 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_DimensionSupported_True)
{
    const std::string altName = "HGX_Dim_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["DimensionSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: ResetMetricsSupported=true → createResetMetrics
// called (covers L389/390 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_ResetMetricsSupported_True)
{
    const std::string altName = "HGX_Reset_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["ResetMetricsSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: GPIOStateSupported=true → createNsmGPIOStateSensors
// called (covers L394/395 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_GPIOStateSupported_True)
{
    const std::string altName = "HGX_GPIO_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["GPIOStateSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: ErrorInjectionSupported=true →
// createErrorInjectionPayload called; DeviceType absent → not MCTP_BRIDGE →
// inner if false (covers L401/402 TRUE branch, L314 FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_ErrorInjectionSupported_True)
{
    const std::string altName = "HGX_ErrInj_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;
    // DeviceType absent → NSM_DEV_ID_UNKNOWN ≠ NSM_DEV_ID_MCTP_BRIDGE

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["ErrorInjectionSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: DeviceDiagnosticsSupported=true →
// createDeviceDiagnostics called (covers L407/409 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_DeviceDiagnosticsSupported_True)
{
    const std::string altName = "HGX_Diag_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["DeviceDiagnosticsSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: AssetInformationAvailable=true → createAsset called
// (covers L338/339-342 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_AssetInformationAvailable_True)
{
    const std::string altName = "HGX_Asset_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    // AssetInformationAvailable=true → createAsset called
    propertyMapAttributes["AssetInformationAvailable"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createOperationalStatus: InstanceNumber present (TRUE branch at L455) and
// Priority=true (TRUE branch at L469)
// ============================================================================

TEST_F(NsmChassisBranchTest,
       OperationalStatus_WithInstanceNumberAndPriority_TrueBranches)
{
    const std::string altName = "HGX_OpStatus_Branch_3";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    basePropertyMap["InstanceNumber"] = uint64_t(1); // TRUE branch at L455

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".OperationalStatus");
    propertyMap["Type"] = std::string("NSM_OperationalStatus");
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/sensor"};
    propertyMap["Priority"] = bool(true); // TRUE branch at L469

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".OperationalStatus", altObjPath));
}

// ============================================================================
// createOperationalStatus: InventoryObjPaths absent → empty dbus::Interfaces
// (FALSE branch at L462 → inventoryObjPaths stays empty)
// ============================================================================

TEST_F(NsmChassisBranchTest,
       OperationalStatus_NoInventoryObjPaths_EmptyInterfaces)
{
    const std::string altName = "HGX_OpStatus_Branch_4";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".OperationalStatus");
    propertyMap["Type"] = std::string("NSM_OperationalStatus");
    // InventoryObjPaths absent → L462 FALSE → inventoryObjPaths stays empty
    // NsmInterfaces constructor then throws because interfaces cannot be empty

    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(
            mockManager, basicIntfName + ".OperationalStatus", altObjPath),
        std::runtime_error);
}

// ============================================================================
// createPowerState: InstanceNumber present (TRUE branch at L500) and
// Priority=true (TRUE branch at L514)
// ============================================================================

TEST_F(NsmChassisBranchTest,
       PowerState_WithInstanceNumberAndPriority_TrueBranches)
{
    const std::string altName = "HGX_PowerState_Branch_3";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    basePropertyMap["InstanceNumber"] = uint64_t(1); // TRUE branch at L500

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = std::string("NSM_PowerState");
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/sensor"};
    propertyMap["Priority"] = bool(true); // TRUE branch at L514

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".PowerState", altObjPath));
}

// ============================================================================
// createPowerState: InventoryObjPaths absent → empty dbus::Interfaces
// (FALSE branch at L507 → inventoryObjPaths stays empty)
// ============================================================================

TEST_F(NsmChassisBranchTest, PowerState_NoInventoryObjPaths_EmptyInterfaces)
{
    const std::string altName = "HGX_PowerState_Branch_4";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = std::string("NSM_PowerState");
    // InventoryObjPaths absent → L507 FALSE → inventoryObjPaths stays empty
    // NsmInterfaces constructor then throws because interfaces cannot be empty

    EXPECT_THROW_COROUTINE(
        nsmChassisCreateSensors(mockManager, basicIntfName + ".PowerState",
                                altObjPath),
        std::runtime_error);
}

// ============================================================================
// NSM_Chassis type: DEVICE_UUID absent → covers L608 FALSE branch
// ============================================================================

TEST_F(NsmChassisBranchTest, Chassis_MissingDeviceUUID_FalseBranch)
{
    const std::string altName = "HGX_Chassis_Branch_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = std::string("NSM_Chassis");
    // DEVICE_UUID absent → covers L608 FALSE branch

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".Chassis", altObjPath));
}

// ============================================================================
// Factory: Name absent in allBaseIfaceProperties → L569 FALSE → name=""
// ============================================================================

TEST_F(NsmChassisBranchTest, Factory_MissingName_FalseBranch)
{
    const std::string altName = "HGX_Chassis_NoName";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    // Name absent → L569 FALSE → name stays ""
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = std::string("NSM_Chassis");

    // name="" → NsmChassis ctor with empty name builds invalid D-Bus path →
    // throws sdbusplus::exception::SdBusError
    EXPECT_THROW_COROUTINE(nsmChassisCreateSensors(mockManager,
                                                   basicIntfName + ".Chassis",
                                                   altObjPath),
                           std::exception);
}

// ============================================================================
// Factory: Type absent in allCurrentIfaceProperties → L573 FALSE → type="" →
// no type branch taken → returns NSM_SUCCESS
// ============================================================================

TEST_F(NsmChassisBranchTest, Factory_MissingType_FalseBranch)
{
    const std::string altName = "HGX_Chassis_NoType";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    // Type absent in current interface → L573 FALSE → type="" → no branch
    (void)utils::MockDbusAsync::propertyMap(altObjPath,
                                            basicIntfName + ".NoType");

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".NoType", altObjPath));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: UUID absent in allBaseIfaceProperties → L577 FALSE → uuid="" →
// getNsmDeviceFromStaticUUID("") throws std::runtime_error
// ============================================================================

TEST_F(NsmChassisBranchTest, Factory_MissingUUID_ThrowsRuntimeError)
{
    const std::string altName = "HGX_Chassis_NoUUID";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    // UUID absent → L577 FALSE → uuid="" → getNsmDeviceFromStaticUUID throws

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = std::string("NSM_Chassis");

    EXPECT_THROW_COROUTINE(nsmChassisCreateSensors(mockManager,
                                                   basicIntfName + ".Chassis",
                                                   altObjPath),
                           std::runtime_error);
}

// ============================================================================
// createChassisAttributes: LocationType and LocationCode TRUE branches (L346,
// L350 in createChassisAttributes, different from FPGA path)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_LocationTypeAndCode_TrueBranches)
{
    const std::string altName = "HGX_GPU_LocType";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    // LocationType present → covers L346 TRUE branch in createChassisAttributes
    propertyMapAttributes["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    // LocationCode present → covers L350 TRUE branch
    propertyMapAttributes["LocationCode"] = std::string("U2");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: ChassisType TRUE branch (L362)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_ChassisType_TrueBranch)
{
    const std::string altName = "HGX_GPU_ChassisTypeBranch";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    // ChassisType present → covers L362 TRUE branch in createChassisAttributes
    propertyMapAttributes["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: ResetMetrics/GPIO/ErrorInj/DeviceDiag present but
// set to FALSE → covers the second operand of && being false (B3 branches)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_MoreFeaturesFalse_FalseBranches)
{
    const std::string altName = "HGX_GPU_MoreFalse";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    // Property present but value=false → covers the && second-operand FALSE
    // branch for each of these optional features
    propertyMapAttributes["ResetMetricsSupported"] = bool(false);
    propertyMapAttributes["GPIOStateSupported"] = bool(false);
    propertyMapAttributes["ErrorInjectionSupported"] = bool(false);
    propertyMapAttributes["DeviceDiagnosticsSupported"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createFPGAAsset: Manufacturer present → covers L78 TRUE branch
// ============================================================================

TEST_F(NsmChassisBranchTest, FpgaAttributes_WithManufacturer_TrueBranch)
{
    const std::string altName = "HGX_FPGA_WithMfr";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = fpgaUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".FPGAAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_FPGA_Attributes");
    // Manufacturer present → covers L78 TRUE branch in createFPGAAsset
    propertyMapAttributes["Manufacturer"] = std::string("NVIDIA Custom");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".FPGAAttributes", altObjPath));
}

// ============================================================================
// NSM_Chassis with non-empty associations → L623 TRUE branch
// ============================================================================

TEST_F(NsmChassisBranchTest, Chassis_WithAssociations_TrueBranch)
{
    const std::string altName = "HGX_Chassis_WithAssoc";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    const MapperServiceMap assocServiceMap = {{{
        "xyz.openbmc_project.NSM",
        {basicIntfName + ".Associations"},
    }}};
    utils::MockDbusAsync::serviceMap() = assocServiceMap;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = gpuUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = std::string("NSM_Chassis");

    // Provide association so associations.empty() = false → L623 TRUE
    auto& assocProps = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Associations");
    assocProps["Forward"] = std::string("chassis");
    assocProps["Backward"] = std::string("all_components");
    assocProps["AbsolutePath"] = altObjPath;

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".Chassis", altObjPath));
}
