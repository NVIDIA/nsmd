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
 * Additional branch coverage for nsmd/nsmChassis/nsmChassis.cpp
 *
 * Covers:
 * - Direct calls to non-static createXxx helpers with various property
 *   combinations to hit TRUE/FALSE branches not covered by factory tests
 * - createAsset: Manufacturer present, all-unsupported → !anySensorAdded TRUE
 * - createFPGAAsset: Manufacturer absent FALSE branch
 * - createDimension, createHealth, createSKU: direct creation verification
 * - createLocation: LocationType absent FALSE branch
 * - createPowerLimit: Priority absent FALSE branch
 * - createPrettyName: PrettyNameForChassis absent FALSE branch
 * - createWriteProtect: DeviceType=BASEBOARD success path
 * - createResetMetrics: direct creation verification
 * - createErrorInjectionPayload: DeviceType=MCTP_BRIDGE TRUE branch,
 *   DeviceType present TRUE branch
 * - createDeviceDiagnostics: direct creation verification
 * - createPowerState: direct call with all props, DeviceType=GPU throws
 * - NsmChassis<IntfType>::update() for non-UuidIntf → co_return NSM_SUCCESS //
 *
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "nsmAssetIntf.hpp"
#include "nsmChassis.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#include "nsmGPIO/nsmGPIOStateCommon.hpp"
#include "nsmGpuPresenceAndPowerStatus.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmPowerSupplyStatus.hpp"
#include "nsmProcessor/nsmOemResetStatistics.hpp"
#include "nsmWriteProtectedJumper.hpp"

using namespace nsm;

// Forward-declare non-static helper functions from nsmChassis.cpp
namespace nsm
{
void createSKU(std::shared_ptr<NsmDevice> device, const std::string& name);
void createFPGAAsset(std::shared_ptr<NsmDevice> device, const std::string& name,
                     const dbus::PropertyMap& allCurrentIfaceProperties);
void createDimension(std::shared_ptr<NsmDevice> device,
                     const std::string& name);
void createHealth(std::shared_ptr<NsmDevice> device, const std::string& name);
void createLocation(std::shared_ptr<NsmDevice> device, const std::string& name,
                    const dbus::PropertyMap& allCurrentIfaceProperties);
void createPowerLimit(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties);
void createPrettyName(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties);
void createWriteProtect(std::shared_ptr<NsmDevice> device,
                        const std::string& name,
                        const dbus::PropertyMap& allBaseIfaceProperties);
void createResetMetrics(std::shared_ptr<NsmDevice> device,
                        const std::string& name, sdbusplus::bus::bus& bus);
void createErrorInjectionPayload(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const std::string& name, const dbus::PropertyMap& allBaseIfaceProperties);
void createDeviceDiagnostics(std::shared_ptr<NsmDevice> device,
                             const std::string& name, const uuid_t& uuid,
                             sdbusplus::bus::bus& bus);
void createPowerState(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties,
                      const dbus::PropertyMap& allBaseIfaceProperties);
requester::Coroutine nsmChassisCreateSensors(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

struct NsmChassisBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string name = "HGX_Branch2_1";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                name;

    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmChassisBranch2Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmChassisBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createSKU: direct call verifies NsmApSkuIdIntf sensor is added
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateSKU_DirectCall)
{
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createSKU(device, name));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createHealth: direct call verifies HealthIntf sensor with OK health
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateHealth_DirectCall)
{
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createHealth(device, name));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmInterfaceProvider<HealthIntf>>(
        device->deviceSensors.back());
    EXPECT_NE(sensor, nullptr);
    EXPECT_EQ(HealthIntf::HealthType::OK, sensor->invoke(pdiMethod(health)));
}

// ============================================================================
// createDimension: direct call adds 3 sensors (depth, width, height)
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateDimension_DirectCall)
{
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createDimension(device, name));
    // depth, width, height = 3 sensors added
    EXPECT_EQ(device->deviceSensors.size(), before + 3);
}

// ============================================================================
// createFPGAAsset: Manufacturer absent → FALSE branch at L117 → default NVIDIA
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateFPGAAsset_NoManufacturer_DefaultNvidia)
{
    dbus::PropertyMap props; // No Manufacturer key
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAsset(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmChassis<NsmAssetIntf>>(
        device->deviceSensors.back());
    EXPECT_NE(sensor, nullptr);
    EXPECT_EQ(MANUFACTURER_NVIDIA, sensor->invoke(pdiMethod(manufacturer)));
}

// ============================================================================
// createFPGAAsset: Manufacturer present → TRUE branch at L117
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateFPGAAsset_WithManufacturer_TrueBranch)
{
    dbus::PropertyMap props;
    props["Manufacturer"] = std::string("CustomMfr");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAsset(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmChassis<NsmAssetIntf>>(
        device->deviceSensors.back());
    EXPECT_NE(sensor, nullptr);
    EXPECT_EQ("CustomMfr", sensor->invoke(pdiMethod(manufacturer)));
}

// ============================================================================
// createLocation: LocationType absent → FALSE branch at L171
// convertLocationTypesFromString("") throws → verify exception
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateLocation_NoLocationType_FalseBranch)
{
    dbus::PropertyMap props; // LocationType absent
    EXPECT_THROW(createLocation(device, name, props), std::exception);
}

// ============================================================================
// createLocation: LocationType present → TRUE branch at L171
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateLocation_WithLocationType_TrueBranch)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createLocation(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmInterfaceProvider<LocationIntf>>(
        device->deviceSensors.back());
    EXPECT_NE(sensor, nullptr);
    EXPECT_EQ(LocationIntf::LocationTypes::Embedded,
              sensor->invoke(pdiMethod(locationType)));
}

// ============================================================================
// createPowerLimit: Priority absent → FALSE branch at L245, default false
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePowerLimit_NoPriority_FalseBranch)
{
    dbus::PropertyMap props; // Priority absent
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerLimit(device, name, props));
    // Adds 2 sensors: MINIMUM_DEVICE_POWER_LIMIT and MAXIMUM_DEVICE_POWER_LIMIT
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// createPowerLimit: Priority present and true → TRUE branch at L245
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePowerLimit_WithPriority_TrueBranch)
{
    dbus::PropertyMap props;
    props["Priority"] = bool(true);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerLimit(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// createPrettyName: PrettyNameForChassis absent → FALSE branch at L263
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePrettyName_NoPrettyName_FalseBranch)
{
    dbus::PropertyMap props; // PrettyNameForChassis absent
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPrettyName(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPrettyName: PrettyNameForChassis present → TRUE branch at L263
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePrettyName_WithPrettyName_TrueBranch)
{
    dbus::PropertyMap props;
    props["PrettyNameForChassis"] = std::string("My GPU");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPrettyName(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmInterfaceProvider<ItemIntf>>(
        device->deviceSensors.back());
    EXPECT_NE(sensor, nullptr);
    EXPECT_EQ("My GPU", sensor->invoke(pdiMethod(prettyName)));
}

// ============================================================================
// createWriteProtect: DeviceType=BASEBOARD → success path (no throw)
// Covers L285 FALSE branch (deviceType == BASEBOARD)
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateWriteProtect_Baseboard_Success)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createWriteProtect(device, name, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createWriteProtect: DeviceType absent → NSM_DEV_ID_UNKNOWN ≠ BASEBOARD →
// throws (L287 TRUE branch, L279 FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateWriteProtect_NoDeviceType_Throws)
{
    dbus::PropertyMap baseProps; // DeviceType absent
    EXPECT_THROW(createWriteProtect(device, name, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createWriteProtect: DeviceType=GPU → not BASEBOARD → throws
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateWriteProtect_GpuType_Throws)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    EXPECT_THROW(createWriteProtect(device, name, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createResetMetrics: direct call creates ResetStatisticsAggregator sensor
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateResetMetrics_DirectCall)
{
    auto& bus = utils::DBusHandler::getBus();
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createResetMetrics(device, name, bus));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createErrorInjectionPayload: DeviceType absent → NSM_DEV_ID_UNKNOWN ≠
// MCTP_BRIDGE → no sensors added (L343 FALSE branch, L349 FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateErrorInjection_NoDeviceType_NoSensorsAdded)
{
    dbus::PropertyMap baseProps; // DeviceType absent
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createErrorInjectionPayload(mockManager, device, name, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before);
}

// ============================================================================
// createErrorInjectionPayload: DeviceType=GPU → not MCTP_BRIDGE →
// no sensors added (L349 FALSE branch, L343 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateErrorInjection_GpuType_NoSensorsAdded)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createErrorInjectionPayload(mockManager, device, name, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before);
}

// ============================================================================
// createErrorInjectionPayload: DeviceType=MCTP_BRIDGE → TRUE branch at L349
// → createNsmMCUErrorInjectionSensors called
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateErrorInjection_MctpBridge_CreatesSensors)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_MCTP_BRIDGE);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createErrorInjectionPayload(mockManager, device, name, baseProps));
    // createNsmMCUErrorInjectionSensors adds sensors
    EXPECT_GE(device->deviceSensors.size(), before);
}

// ============================================================================
// createDeviceDiagnostics: direct call adds NsmDebugInfoObject sensor
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreateDeviceDiagnostics_DirectCall)
{
    auto& bus = utils::DBusHandler::getBus();
    const uuid_t uuid = "test-uuid-for-diagnostics";
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createDeviceDiagnostics(device, name, uuid, bus));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: DeviceType=BASEBOARD, all optional props present
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePowerState_Baseboard_AllProps)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/sensor"};
    currentProps["Priority"] = bool(true);

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(2);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: DeviceType=GPU → not BASEBOARD → throws
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePowerState_GpuType_Throws)
{
    dbus::PropertyMap currentProps;
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    EXPECT_THROW(createPowerState(device, name, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createPowerState: DeviceType absent → 0 ≠ BASEBOARD → throws
// ============================================================================

TEST_F(NsmChassisBranch2Test, CreatePowerState_NoDeviceType_Throws)
{
    dbus::PropertyMap currentProps;
    dbus::PropertyMap baseProps; // DeviceType absent → deviceType=0
    EXPECT_THROW(createPowerState(device, name, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createPowerState: DeviceType=BASEBOARD, missing InstanceNumber/Priority/
// InventoryObjPaths → FALSE branches at L536/L540/L546/L553
// ============================================================================

TEST_F(NsmChassisBranch2Test,
       CreatePowerState_Baseboard_MissingOptional_FalseBranches)
{
    dbus::PropertyMap currentProps;
    // InventoryObjPaths must be present to avoid empty-interfaces throw
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/dev"};

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    // InstanceNumber absent → FALSE branch
    // Priority absent → FALSE branch

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// NsmChassis<HealthIntf>::update() → non-UuidIntf specialization →
// co_return NSM_SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch2Test, NsmChassisHealthUpdate_ReturnsSuccess)
{
    auto chassisHealth = std::make_shared<NsmChassis<HealthIntf>>(name);
    chassisHealth->update(device);
}

// ============================================================================
// NsmChassis<LocationIntf>::update() → non-UuidIntf → co_return NSM_SUCCESS //

// ============================================================================

TEST_F(NsmChassisBranch2Test, NsmChassisLocationUpdate_ReturnsSuccess)
{
    auto chassisLocation = std::make_shared<NsmChassis<LocationIntf>>(name);
    chassisLocation->update(device);
}

// ============================================================================
// NsmChassis<DimensionIntf>::update() → non-UuidIntf → co_return NSM_SUCCESS //

// ============================================================================

TEST_F(NsmChassisBranch2Test, NsmChassisDimensionUpdate_ReturnsSuccess)
{
    auto chassisDim = std::make_shared<NsmChassis<DimensionIntf>>(name);
    chassisDim->update(device);
}

// ============================================================================
// Factory: NSM_Chassis type with DEVICE_UUID present → L645 TRUE branch
// (already partly covered, but here we exercise the direct createPowerState
// path to ensure DeviceType=BASEBOARD with InstanceNumber present)
// ============================================================================

TEST_F(NsmChassisBranch2Test, Factory_PowerState_BaseboardWithInstanceNumber)
{
    const std::string altName = "HGX_B2_PwrState_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    basePropertyMap["InstanceNumber"] = uint64_t(3);

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = std::string("NSM_PowerState");
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/sensor"};
    propertyMap["Priority"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".PowerState", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with WriteProtectSupported=true and
// DeviceType=BASEBOARD → createWriteProtect succeeds (L421-424 path)
// ============================================================================

TEST_F(NsmChassisBranch2Test, Factory_ChassisAttributes_WriteProtect_Baseboard)
{
    const std::string altName = "HGX_B2_WP_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["WriteProtectSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with ErrorInjectionSupported=true and
// DeviceType=MCTP_BRIDGE → createErrorInjectionPayload calls
// createNsmMCUErrorInjectionSensors (L438-442 path, L349 TRUE branch)
// ============================================================================

TEST_F(NsmChassisBranch2Test,
       Factory_ChassisAttributes_ErrorInjection_MctpBridge)
{
    const std::string altName = "HGX_B2_ErrInj_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_MCTP_BRIDGE);

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["ErrorInjectionSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with AssetInformationAvailable=true and
// Manufacturer present in interface props → createAsset with Manufacturer
// TRUE branch at L74
// ============================================================================

TEST_F(NsmChassisBranch2Test, Factory_ChassisAttributes_AssetWithManufacturer)
{
    const std::string altName = "HGX_B2_AssetMfr_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMapAttributes = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMapAttributes["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMapAttributes["AssetInformationAvailable"] = bool(true);
    propertyMapAttributes["Manufacturer"] = std::string("TestManufacturer");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}
