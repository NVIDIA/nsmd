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
 * - createChassisAttributesFeature: FALSE/TRUE branches of the optional
 *   attribute params (LocationType, LocationCode, LocationContext,
 *   FieldReplaceable, ChassisType, PrettyNameForChassis, Priority) and the
 *   *Supported flags; empty LocationCode is dropped
 * - createWriteProtect via the feature: DeviceType GPU throws
 * - createChassisFeature / createChassisAssociationsFeature
 * - createFPGAAttributes: TRUE branches with LocationType and ChassisType
 * - createOperationalStatus / createPowerState: optional properties
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmChassis.hpp"

using namespace nsm;

// Forward-declare non-static helper functions from nsmChassis.cpp
namespace nsm
{
void createFPGAAttributes(std::shared_ptr<NsmDevice> device,
                          const std::string& name,
                          const dbus::PropertyMap& allCurrentIfaceProperties);
void createOperationalStatus(std::shared_ptr<NsmDevice> device,
                             const std::string& name,
                             const dbus::PropertyMap& allCurrentIfaceProperties,
                             const dbus::PropertyMap& allBaseIfaceProperties);
void createPowerState(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties,
                      const dbus::PropertyMap& allBaseIfaceProperties);
} // namespace nsm

struct NsmChassisBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "HGX_GPU_Branch_1";

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

    /** @brief Run a chassis feature creator for the device `devName` */
    requester::Coroutine runFeature(nsm::features::CreatorFn creator,
                                    const std::string& devName,
                                    const uuid_t& uuid, uint8_t deviceType,
                                    dbus::PropertyMap params,
                                    bool withDevice = true)
    {
        nsm::features::DeviceContext dev;
        dev.name = devName;
        dev.kind = "gpu";
        dev.deviceType = deviceType;
        dev.staticUuid = uuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = mockManager.getNsmDeviceFromStaticUUID(uuid);
        }
        params.try_emplace("Name", devName);
        return creator(mockManager, dev, params);
    }

    /** @brief Chassis attributes feature of a GPU chassis */
    requester::Coroutine gpuAttributes(const std::string& devName,
                                       dbus::PropertyMap params)
    {
        return runFeature(nsm::features::createChassisAttributesFeature,
                          devName, gpuUuid, NSM_DEV_ID_GPU, std::move(params));
    }
};

// ============================================================================
// ChassisAttributes: no optional params → only SKU and Health
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_NoOptionalProps_AllFalseBranches)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes(name, {});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // createSKU + createHealth
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// ChassisAttributes: AssetInformationAvailable/Dimension/PowerLimit/
// WriteProtect present but false → FALSE branches of the && conditions
// ============================================================================

TEST_F(NsmChassisBranchTest,
       ChassisAttributes_AssetAvailableFalse_FalseBranches)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_GPU_Branch_2",
                            {{"AssetInformationAvailable", false},
                             {"DimensionSupported", false},
                             {"PowerLimitSupported", false},
                             {"WriteProtectSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// createPowerLimit with Priority; LocationContext, FieldReplaceable,
// PrettyNameForChassis TRUE branches
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_WithPriorityAndExtraProps)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes(
        "HGX_GPU_Branch_3",
        {{"PowerLimitSupported", true},
         {"Priority", true},
         {"LocationContext",
          std::string("xyz.openbmc_project.Inventory.Decorator.LocationContext"
                      ".Contexts.GPU")},
         {"FieldReplaceable", true},
         {"PrettyNameForChassis", std::string("GPU SXM 1")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // SKU, Health, LocationContext, FieldReplaceable, PrettyName and the two
    // power limit properties
    EXPECT_EQ(gpu->deviceSensors.size(), before + 7);
}

// ============================================================================
// createWriteProtect: DeviceType=GPU (not BASEBOARD) → throws
// ============================================================================

TEST_F(NsmChassisBranchTest, WriteProtect_GpuDevice_Throws)
{
    EXPECT_THROW_COROUTINE(
        gpuAttributes("HGX_GPU_Branch_5", {{"WriteProtectSupported", true}}),
        std::runtime_error);
}

// ============================================================================
// createFPGAAttributes: WITH LocationType and ChassisType → TRUE branches
// ============================================================================

TEST_F(NsmChassisBranchTest, FpgaAttributes_WithLocationTypeAndChassisType)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    props["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType"
        ".Module");

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAttributes(fpga, "HGX_FPGA_Branch_1", props));
    // FPGA asset, SKU, Location, ChassisType, Health
    EXPECT_EQ(fpga->deviceSensors.size(), before + 5);
}

// ============================================================================
// createOperationalStatus: missing DeviceType → 0 ≠ NSM_DEV_ID_BASEBOARD →
// throws
// ============================================================================

TEST_F(NsmChassisBranchTest, OperationalStatus_MissingDeviceType_Throws)
{
    dbus::PropertyMap props;
    dbus::PropertyMap baseProps;
    EXPECT_THROW(
        createOperationalStatus(gpu, "HGX_OpStatus_Branch_1", props, baseProps),
        std::runtime_error);
}

// ============================================================================
// createOperationalStatus: DeviceType=BASEBOARD, missing InstanceNumber and
// Priority → FALSE branches
// ============================================================================

TEST_F(NsmChassisBranchTest,
       OperationalStatus_BaseboardMissingOptional_FalseBranches)
{
    const std::string altName = "HGX_OpStatus_Branch_2";
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    dbus::PropertyMap props;
    // InventoryObjPaths required to avoid empty-interfaces throw
    props["InventoryObjPaths"] = std::vector<std::string>{
        std::string(chassisInventoryBasePath) + "/" + altName + "/sensor"};

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createOperationalStatus(fpga, altName, props, baseProps));
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: DeviceType=BASEBOARD, missing InstanceNumber and
// Priority → FALSE branches
// ============================================================================

TEST_F(NsmChassisBranchTest, PowerState_BaseboardMissingOptional_FalseBranches)
{
    const std::string altName = "HGX_PowerState_Branch_1";
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    dbus::PropertyMap props;
    props["InventoryObjPaths"] = std::vector<std::string>{
        std::string(chassisInventoryBasePath) + "/" + altName + "/sensor"};

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(fpga, altName, props, baseProps));
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: DeviceType missing → 0 ≠ BASEBOARD → throws
// ============================================================================

TEST_F(NsmChassisBranchTest, PowerState_MissingDeviceType_Throws)
{
    dbus::PropertyMap props;
    dbus::PropertyMap baseProps;
    EXPECT_THROW(
        createPowerState(gpu, "HGX_PowerState_Branch_2", props, baseProps),
        std::runtime_error);
}

// ============================================================================
// ChassisAttributes: DimensionSupported=true → createDimension called
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_DimensionSupported_True)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_Dim_Branch_1", {{"DimensionSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // SKU, Health and depth/width/height
    EXPECT_EQ(gpu->deviceSensors.size(), before + 5);
}

// ============================================================================
// ChassisAttributes: ResetMetricsSupported=true → createResetMetrics called
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_ResetMetricsSupported_True)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_Reset_Branch_1",
                            {{"ResetMetricsSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 3);
}

// ============================================================================
// ChassisAttributes: GPIOStateSupported=true → createNsmGPIOStateSensors
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_GPIOStateSupported_True)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_GPIO_Branch_1",
                            {{"GPIOStateSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// ChassisAttributes: ErrorInjectionSupported=true on a GPU →
// createErrorInjectionPayload called, not MCTP_BRIDGE → nothing added
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_ErrorInjectionSupported_True)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_ErrInj_Branch_1",
                            {{"ErrorInjectionSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// ChassisAttributes: DeviceDiagnosticsSupported=true →
// createDeviceDiagnostics called (when ENABLE_DEBUG_INFO)
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_DeviceDiagnosticsSupported_True)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_Diag_Branch_1",
                            {{"DeviceDiagnosticsSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// ChassisAttributes: AssetInformationAvailable=true → createAsset called
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_AssetInformationAvailable_True)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_Asset_Branch_1",
                            {{"AssetInformationAvailable", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// createOperationalStatus: InstanceNumber and Priority=true TRUE branches
// ============================================================================

TEST_F(NsmChassisBranchTest,
       OperationalStatus_WithInstanceNumberAndPriority_TrueBranches)
{
    const std::string altName = "HGX_OpStatus_Branch_3";
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(1);
    dbus::PropertyMap props;
    props["InventoryObjPaths"] = std::vector<std::string>{
        std::string(chassisInventoryBasePath) + "/" + altName + "/sensor"};
    props["Priority"] = bool(true);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createOperationalStatus(fpga, altName, props, baseProps));
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);
}

// ============================================================================
// createOperationalStatus: InventoryObjPaths absent → empty interfaces →
// NsmInterfaces constructor throws
// ============================================================================

TEST_F(NsmChassisBranchTest,
       OperationalStatus_NoInventoryObjPaths_EmptyInterfaces)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    dbus::PropertyMap props;
    EXPECT_THROW(createOperationalStatus(fpga, "HGX_OpStatus_Branch_4", props,
                                         baseProps),
                 std::runtime_error);
}

// ============================================================================
// createPowerState: InstanceNumber and Priority=true TRUE branches
// ============================================================================

TEST_F(NsmChassisBranchTest,
       PowerState_WithInstanceNumberAndPriority_TrueBranches)
{
    const std::string altName = "HGX_PowerState_Branch_3";
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(1);
    dbus::PropertyMap props;
    props["InventoryObjPaths"] = std::vector<std::string>{
        std::string(chassisInventoryBasePath) + "/" + altName + "/sensor"};
    props["Priority"] = bool(true);

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(fpga, altName, props, baseProps));
    EXPECT_EQ(fpga->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: InventoryObjPaths absent → empty interfaces → throws
// ============================================================================

TEST_F(NsmChassisBranchTest, PowerState_NoInventoryObjPaths_EmptyInterfaces)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    dbus::PropertyMap props;
    EXPECT_THROW(
        createPowerState(fpga, "HGX_PowerState_Branch_4", props, baseProps),
        std::runtime_error);
}

// ============================================================================
// createChassisFeature: UUID and MCTP UUID objects from the NsmDevice
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisFeature_AddsUuidSensors)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = runFeature(nsm::features::createChassisFeature,
                         "HGX_Chassis_Branch_1", gpuUuid, NSM_DEV_ID_GPU, {});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GE(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// createChassisFeature: no Name param → params.at throws
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisFeature_MissingName_Throws)
{
    nsm::features::DeviceContext dev;
    dev.name = "HGX_Chassis_NoName";
    dev.staticUuid = gpuUuid;
    dev.device = gpu;
    const dbus::PropertyMap params;
    EXPECT_THROW_COROUTINE(
        nsm::features::createChassisFeature(mockManager, dev, params),
        std::exception);
}

// ============================================================================
// createChassisFeature: no NsmDevice → NSM_ERROR
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisFeature_NoDevice_ReturnsError)
{
    auto rc = runFeature(nsm::features::createChassisFeature,
                         "HGX_Chassis_NoUUID", gpuUuid, NSM_DEV_ID_GPU, {},
                         false);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_ERROR);
}

// ============================================================================
// ChassisAttributes: LocationType and LocationCode TRUE branches
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_LocationTypeAndCode_TrueBranches)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes(
        "HGX_GPU_LocType",
        {{"LocationType",
          std::string("xyz.openbmc_project.Inventory.Decorator.Location"
                      ".LocationTypes.Embedded")},
         {"LocationCode", std::string("U2")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // SKU, Health, Location, LocationCode
    EXPECT_EQ(gpu->deviceSensors.size(), before + 4);
}

// ============================================================================
// ChassisAttributes: empty LocationCode (EM default) is dropped → no
// LocationCode object
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_EmptyLocationCode_Dropped)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_GPU_EmptyLocCode",
                            {{"LocationCode", std::string("")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// ChassisAttributes: ChassisType TRUE branch
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_ChassisType_TrueBranch)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes(
        "HGX_GPU_ChassisTypeBranch",
        {{"ChassisType",
          std::string(
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 3);
}

// ============================================================================
// ChassisAttributes: ResetMetrics/GPIO/ErrorInj/DeviceDiag present but false
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAttributes_MoreFeaturesFalse_FalseBranches)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = gpuAttributes("HGX_GPU_MoreFalse",
                            {{"ResetMetricsSupported", false},
                             {"GPIOStateSupported", false},
                             {"ErrorInjectionSupported", false},
                             {"DeviceDiagnosticsSupported", false}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before + 2);
}

// ============================================================================
// createFPGAAttributes: Manufacturer present → createFPGAAsset TRUE branch
// ============================================================================

TEST_F(NsmChassisBranchTest, FpgaAttributes_WithManufacturer_TrueBranch)
{
    dbus::PropertyMap props;
    props["Manufacturer"] = std::string("NVIDIA Custom");

    const size_t before = fpga->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAttributes(fpga, "HGX_FPGA_WithMfr", props));
    // FPGA asset, SKU, Health
    EXPECT_EQ(fpga->deviceSensors.size(), before + 3);
}

// ============================================================================
// createChassisAssociationsFeature: parent and bridging associations
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAssociations_ParentAndBridging)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = runFeature(
        nsm::features::createChassisAssociationsFeature,
        "HGX_Chassis_WithAssoc", gpuUuid, NSM_DEV_ID_GPU,
        {{"ParentChassisAssociationPaths",
          std::vector<std::string>{"chassis/HGX_ProcessorModule_0", ""}},
         {"BridgingChassisAssociationPath",
          std::string("chassis/HGX_Bridge_0")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(gpu->deviceSensors.size(), before + 1);

    auto assoc = std::dynamic_pointer_cast<
        NsmInterfaceProvider<AssociationDefinitionsInft>>(
        gpu->deviceSensors.back());
    ASSERT_NE(assoc, nullptr);
    const auto links = assoc->invoke(pdiMethod(associations));
    ASSERT_EQ(links.size(), 2u);
    EXPECT_EQ(std::get<0>(links[0]), "parent_chassis");
    EXPECT_EQ(std::get<1>(links[0]), "all_chassis");
    EXPECT_EQ(std::get<2>(links[0]), std::string(inventorySystemBasePath) +
                                         "/chassis/HGX_ProcessorModule_0");
    EXPECT_EQ(std::get<0>(links[1]), "bridged_chassis");
    EXPECT_EQ(std::get<1>(links[1]), "bridging_chassis");
    EXPECT_EQ(std::get<2>(links[1]),
              std::string(inventorySystemBasePath) + "/chassis/HGX_Bridge_0");
}

// ============================================================================
// createChassisAssociationsFeature: no targets → no object
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAssociations_NoTargets_NoObject)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = runFeature(nsm::features::createChassisAssociationsFeature,
                         "HGX_Chassis_NoAssoc", gpuUuid, NSM_DEV_ID_GPU,
                         {{"BridgingChassisAssociationPath", std::string("")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// createChassisAssociationsFeature: no NsmDevice → NSM_ERROR
// ============================================================================

TEST_F(NsmChassisBranchTest, ChassisAssociations_NoDevice_ReturnsError)
{
    auto rc = runFeature(nsm::features::createChassisAssociationsFeature,
                         "HGX_Chassis_AssocNoDev", gpuUuid, NSM_DEV_ID_GPU,
                         {{"ParentChassisAssociationPaths",
                           std::vector<std::string>{"chassis/HGX_Parent"}}},
                         false);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_ERROR);
}
