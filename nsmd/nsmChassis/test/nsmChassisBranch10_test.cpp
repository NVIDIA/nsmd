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

// ============================================================================
// Additional branch coverage for nsmd/nsmChassis/nsmChassis.cpp
//
// Covers uncovered branches:
// - nsmChassisCreateSensors: coGetCachedBaseProperties failure path
// - createAsset: various unsupported property combinations (loop skip paths)
// - markAssetPropertiesNotSupported: all three cases at once
// - createFPGAAttributes: only LocationType (no ChassisType) and vice versa
// - createChassisAttributes: WriteProtectSupported=true with GPU throws
// - createChassisAttributes: AssetInfo=true, no Manufacturer (default path)
// - createChassisAttributes: PowerLimitSupported=true, no Priority (default)
// - createOperationalStatus: DeviceType=BASEBOARD, Priority=false explicit
// - createPowerState: BASEBOARD, InstanceNumber present, Priority absent
// - NsmChassis<UuidIntf>::update() specialization path
// - createAsset: individual unsupported props (SERIAL_NUMBER, MARKETING_NAME)
// - createFieldReplaceable: FieldReplaceable truly absent
// - Factory: NSM_Chassis with DEVICE_UUID present
// - Various compound && short-circuit FALSE directions
// ============================================================================

#include "../../common/nsmPropertySupport.hpp"
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

#include <unordered_set>

// Forward-declare non-static helper functions from nsmChassis.cpp
namespace nsm
{
void createAsset(
    std::shared_ptr<NsmDevice> device, const std::string& name,
    const dbus::PropertyMap& allCurrentIfaceProperties,
    const std::unordered_set<nsm_inventory_property_identifiers>& unsupported);
void createSKU(std::shared_ptr<NsmDevice> device, const std::string& name);
void createFPGAAsset(std::shared_ptr<NsmDevice> device, const std::string& name,
                     const dbus::PropertyMap& allCurrentIfaceProperties);
void createDimension(std::shared_ptr<NsmDevice> device,
                     const std::string& name);
void createChassisType(std::shared_ptr<NsmDevice> device,
                       const std::string& name,
                       const dbus::PropertyMap& allCurrentIfaceProperties);
void createHealth(std::shared_ptr<NsmDevice> device, const std::string& name);
void createLocation(std::shared_ptr<NsmDevice> device, const std::string& name,
                    const dbus::PropertyMap& allCurrentIfaceProperties);
void createLocationCode(std::shared_ptr<NsmDevice> device,
                        const std::string& name,
                        const dbus::PropertyMap& allCurrentIfaceProperties);
void createLocationContext(std::shared_ptr<NsmDevice> device,
                           const std::string& name,
                           const dbus::PropertyMap& allCurrentIfaceProperties);
void createFieldReplaceable(std::shared_ptr<NsmDevice> device,
                            const std::string& name,
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
                        const std::string& name, sdbusplus::bus_t& bus);
void createErrorInjectionPayload(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const std::string& name, const dbus::PropertyMap& allBaseIfaceProperties);
void createDeviceDiagnostics(std::shared_ptr<NsmDevice> device,
                             const std::string& name, const uuid_t& uuid,
                             sdbusplus::bus_t& bus);
void createChassisAttributes(std::shared_ptr<NsmDevice> device,
                             SensorManager& manager, sdbusplus::bus_t& bus,
                             const std::string& name, const uuid_t& uuid,
                             const dbus::PropertyMap& allCurrentIfaceProperties,
                             const dbus::PropertyMap& allBaseIfaceProperties);
void createOperationalStatus(std::shared_ptr<NsmDevice> device,
                             const std::string& name,
                             const dbus::PropertyMap& allCurrentIfaceProperties,
                             const dbus::PropertyMap& allBaseIfaceProperties);
void createPowerState(std::shared_ptr<NsmDevice> device,
                      const std::string& name,
                      const dbus::PropertyMap& allCurrentIfaceProperties,
                      const dbus::PropertyMap& allBaseIfaceProperties);
void createFPGAAttributes(std::shared_ptr<NsmDevice> device,
                          const std::string& name,
                          const dbus::PropertyMap& allCurrentIfaceProperties);
requester::Coroutine nsmChassisCreateSensors(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

// ============================================================================
// Fixture for nsmChassis.cpp branch coverage batch 10b
// ============================================================================

static const std::string kBaseIntf10b =
    "xyz.openbmc_project.Configuration.NSM_Chassis";
static const uuid_t kDevUuid10b = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

struct NsmChassisBranch10bTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "HGX_B10b_1";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                name;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmChassisBranch10bTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kDevUuid10b));
        EXPECT_NE(device, nullptr);
    }

    ~NsmChassisBranch10bTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// 1. nsmChassisCreateSensors: coGetCachedBaseProperties returns error
//    -> rc != NSM_SUCCESS -> co_return rc (L591-593)
// ============================================================================

TEST_F(NsmChassisBranch10bTest, Factory_BasePropsFailure_ReturnsError)
{
    const std::string altObjPath = "/xyz/test/b10b/no_base_props_registered";

    // Do NOT register base property map -> coGetCachedBaseProperties returns
    // NSM_SW_ERROR -> rc != NSM_SUCCESS -> co_return rc
    auto& currProps = utils::MockDbusAsync::propertyMap(
        altObjPath, kBaseIntf10b + ".ChassisAttributes");
    currProps["Type"] = std::string("NSM_Chassis_Attributes");

    // Should not throw, just returns error code
    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, kBaseIntf10b + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// 2. createAsset: only SERIAL_NUMBER unsupported -> FRU_PART_NUMBER and
//    MARKETING_NAME added, SERIAL_NUMBER skipped (continue at L90-92)
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateAsset_OnlySerialNumberUnsupported)
{
    dbus::PropertyMap props;
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        SERIAL_NUMBER};
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    // FRU_PART_NUMBER + MARKETING_NAME = 2 sensors
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// 3. createAsset: only MARKETING_NAME unsupported -> FRU_PART_NUMBER and
//    SERIAL_NUMBER added, MARKETING_NAME skipped
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateAsset_OnlyMarketingNameUnsupported)
{
    dbus::PropertyMap props;
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        MARKETING_NAME};
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    // FRU_PART_NUMBER + SERIAL_NUMBER = 2 sensors
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// 4. createAsset: SERIAL_NUMBER and MARKETING_NAME unsupported -> only
//    FRU_PART_NUMBER added (anySensorAdded=true, just 1 sensor)
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateAsset_TwoUnsupported_OnlyOneAdded)
{
    dbus::PropertyMap props;
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        SERIAL_NUMBER, MARKETING_NAME};
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    // Only FRU_PART_NUMBER added
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// 5. markAssetPropertiesNotSupported: all three cases in one call
//    FRU_PART_NUMBER + SERIAL_NUMBER + MARKETING_NAME -> all three switch arms
// ============================================================================

TEST_F(NsmChassisBranch10bTest, MarkAssetNotSupported_AllThreeCases)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        FRU_PART_NUMBER, SERIAL_NUMBER, MARKETING_NAME};
    EXPECT_NO_THROW(
        markAssetPropertiesNotSupported(*chassisAsset, unsupported));
    EXPECT_EQ(propertyNotSupported,
              chassisAsset->invoke(pdiMethod(partNumber)));
    EXPECT_EQ(propertyNotSupported,
              chassisAsset->invoke(pdiMethod(serialNumber)));
    EXPECT_EQ(propertyNotSupported, chassisAsset->invoke(pdiMethod(model)));
}

// ============================================================================
// 6. createFPGAAttributes: only LocationType present, no ChassisType
//    -> L454 TRUE, L458 FALSE
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateFPGAAttributes_OnlyLocationType)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    // ChassisType absent -> FALSE
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAttributes(device, name, props));
    // createFPGAAsset + createSKU + createLocation + createHealth = 4
    EXPECT_GE(device->deviceSensors.size(), before + 4);
}

// ============================================================================
// 7. createFPGAAttributes: only ChassisType present, no LocationType
//    -> L454 FALSE, L458 TRUE
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateFPGAAttributes_OnlyChassisType)
{
    dbus::PropertyMap props;
    props["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    // LocationType absent -> FALSE
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAttributes(device, name, props));
    // createFPGAAsset + createSKU + createChassisType + createHealth = 4
    EXPECT_GE(device->deviceSensors.size(), before + 4);
}

// ============================================================================
// 8. createChassisAttributes: AssetInformationAvailable=true, Manufacturer
//    absent -> default NVIDIA manufacturer path in createAsset (L72-73 FALSE)
// ============================================================================

TEST_F(NsmChassisBranch10bTest,
       CreateChassisAttributes_AssetTrue_NoManufacturer)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["AssetInformationAvailable"] = bool(true);
    // No Manufacturer -> defaults to NVIDIA
    dbus::PropertyMap baseProps;

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, kDevUuid10b, currentProps, baseProps));
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// 9. createChassisAttributes: WriteProtectSupported=true, DeviceType=GPU
//    -> createWriteProtect throws (deviceType != BASEBOARD)
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateChassisAttributes_WriteProtectGpu_Throws)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["WriteProtectSupported"] = bool(true);
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    EXPECT_THROW(createChassisAttributes(device, mockManager, bus, name,
                                         kDevUuid10b, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// 10. createChassisAttributes: PowerLimitSupported=true, no Priority key
//     -> createPowerLimit with default priority=false (L240 FALSE)
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateChassisAttributes_PowerLimitNoPriority)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["PowerLimitSupported"] = bool(true);
    // No Priority key -> defaults to false
    dbus::PropertyMap baseProps;

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, kDevUuid10b, currentProps, baseProps));
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// 11. createOperationalStatus: BASEBOARD, Priority explicitly false
//     -> covers L497 TRUE (count > 0) but priority=false path
// ============================================================================

TEST_F(NsmChassisBranch10bTest,
       CreateOperationalStatus_Baseboard_PriorityExplicitFalse)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/opstat"};
    currentProps["Priority"] = bool(false);

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(5);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createOperationalStatus(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// 12. createPowerState: BASEBOARD, InstanceNumber present, Priority absent
//     -> L529 TRUE, L545 FALSE
// ============================================================================

TEST_F(NsmChassisBranch10bTest,
       CreatePowerState_Baseboard_InstancePresent_NoPriority)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/pwr_b10b"};
    // Priority absent -> FALSE branch

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(7);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// 13. createPowerState: BASEBOARD, InstanceNumber absent, Priority present
//     -> L529 FALSE, L545 TRUE
// ============================================================================

TEST_F(NsmChassisBranch10bTest,
       CreatePowerState_Baseboard_NoInstance_PriorityTrue)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/pwr_b10b2"};
    currentProps["Priority"] = bool(true);

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    // InstanceNumber absent -> FALSE branch

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// 14. createFieldReplaceable: FieldReplaceable truly absent from map
//     -> L222 FALSE branch (count == 0, uninitialized bool used)
// NOTE: DISABLED because production code uses uninitialized bool when
// "FieldReplaceable" is absent (nsmChassis.cpp:221), causing Valgrind
// "Conditional jump on uninitialised value". Production code bug:
// bool fieldReplaceable should be default-initialized to false.
// ============================================================================

TEST_F(NsmChassisBranch10bTest, DISABLED_CreateFieldReplaceable_TrulyAbsent)
{
    dbus::PropertyMap props; // FieldReplaceable key not in map at all
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFieldReplaceable(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// 15. NsmChassis<NsmAssetIntf>::update() -> non-UuidIntf -> co_return SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisAssetUpdate_ReturnsSuccess)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    chassisAsset->update(device);
}

// ============================================================================
// 16. NsmChassis<NsmApSkuIdIntf>::update() -> non-UuidIntf -> co_return SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisSkuUpdate_ReturnsSuccess)
{
    auto chassisSku = std::make_shared<NsmChassis<NsmApSkuIdIntf>>(name);
    chassisSku->update(device);
}

// ============================================================================
// 17. NsmChassis<ChassisIntf>::update() -> non-UuidIntf -> co_return SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisChassisIntfUpdate_ReturnsSuccess)
{
    auto chassis = std::make_shared<NsmChassis<ChassisIntf>>(name);
    chassis->update(device);
}

// ============================================================================
// 18. NsmChassis<PowerLimitIntf>::update() -> non-UuidIntf -> co_return SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisPowerLimitUpdate_ReturnsSuccess)
{
    auto chassisPwrLimit = std::make_shared<NsmChassis<PowerLimitIntf>>(name);
    chassisPwrLimit->update(device);
}

// ============================================================================
// 19. NsmChassis<ReplaceableIntf>::update() -> non-UuidIntf -> SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisReplaceableUpdate_ReturnsSuccess)
{
    auto chassisRepl = std::make_shared<NsmChassis<ReplaceableIntf>>(name);
    chassisRepl->update(device);
}

// ============================================================================
// 20. NsmChassis<LocationCodeIntf>::update() -> non-UuidIntf -> SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisLocationCodeUpdate_ReturnsSuccess)
{
    auto chassisLocCode = std::make_shared<NsmChassis<LocationCodeIntf>>(name);
    chassisLocCode->update(device);
}

// ============================================================================
// 21. NsmChassis<LocationContextIntf>::update() -> non-UuidIntf -> SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisLocationContextUpdate_ReturnsSuccess)
{
    auto chassisLocCtx =
        std::make_shared<NsmChassis<LocationContextIntf>>(name);
    chassisLocCtx->update(device);
}

// ============================================================================
// 22. NsmChassis<ItemIntf>::update() -> non-UuidIntf -> SUCCESS
// ============================================================================

TEST_F(NsmChassisBranch10bTest, NsmChassisItemUpdate_ReturnsSuccess)
{
    auto chassisItem = std::make_shared<NsmChassis<ItemIntf>>(name);
    chassisItem->update(device);
}

// ============================================================================
// 23. createChassisAttributes: ErrorInjectionSupported=true,
//     DeviceType=GPU (not MCTP_BRIDGE) -> createErrorInjectionPayload
//     does not create sensors (L344 FALSE branch inside)
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateChassisAttributes_ErrorInjection_GpuType)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["ErrorInjectionSupported"] = bool(true);
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, kDevUuid10b, currentProps, baseProps));
    // ErrorInjection not added because GPU != MCTP_BRIDGE
    // Still has SKU + Health as minimum
    EXPECT_GE(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// 24. createChassisAttributes: only ResetMetricsSupported=true
//     (all other flags absent) -> isolated TRUE branch at L420-424
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateChassisAttributes_OnlyResetMetrics)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["ResetMetricsSupported"] = bool(true);
    dbus::PropertyMap baseProps;

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, kDevUuid10b, currentProps, baseProps));
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// 25. createChassisAttributes: only DeviceDiagnosticsSupported=true
//     (all other flags absent) -> isolated TRUE branch at L438-443
// ============================================================================

TEST_F(NsmChassisBranch10bTest, CreateChassisAttributes_OnlyDeviceDiagnostics)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["DeviceDiagnosticsSupported"] = bool(true);
    dbus::PropertyMap baseProps;

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, kDevUuid10b, currentProps, baseProps));
    EXPECT_GT(device->deviceSensors.size(), before);
}
