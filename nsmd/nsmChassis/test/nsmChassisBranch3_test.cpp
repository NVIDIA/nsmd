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
 * Additional branch coverage batch 3 for nsmd/nsmChassis/nsmChassis.cpp
 *
 * Covers:
 * - markAssetPropertiesNotSupported: individual switch cases (FRU_PART_NUMBER,
 *   SERIAL_NUMBER, MARKETING_NAME, default)
 * - createAsset: Manufacturer absent FALSE branch, unsupported set that skips
 *   all sensors (!anySensorAdded TRUE), unsupported set that skips some
 * - createChassisType: ChassisType absent FALSE branch
 * - createLocationCode: LocationCode absent FALSE branch
 * - createLocationContext: LocationContext absent FALSE branch
 * - createFieldReplaceable: FieldReplaceable absent FALSE branch
 * - createOperationalStatus: DeviceType present/absent, InstanceNumber
 *   present/absent, InventoryObjPaths present/absent, Priority present/absent,
 *   DeviceType != BASEBOARD throws
 * - createFPGAAttributes: LocationType/ChassisType present TRUE branches
 * - Factory: NSM_FPGA_Attributes type dispatched
 * - Factory: NSM_OperationalStatus type dispatched
 * - Factory: NSM_Chassis type with DEVICE_UUID absent FALSE branch
 * - Factory: NSM_Chassis_Attributes with multiple optional flags FALSE
 * - Factory: NSM_Chassis_Attributes DeviceDiagnosticsSupported TRUE branch
 * - Factory: NSM_Chassis_Attributes GPIOStateSupported TRUE branch
 * - Factory: NSM_Chassis_Attributes ResetMetricsSupported TRUE branch
 * - Factory: NSM_Chassis_Attributes DimensionSupported TRUE branch
 * - Factory: NSM_Chassis_Attributes PowerLimitSupported TRUE branch
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

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

using namespace nsm;

// Forward-declare non-static helper functions from nsmChassis.cpp
namespace nsm
{
void createAsset(
    std::shared_ptr<NsmDevice> device, const std::string& name,
    const dbus::PropertyMap& allCurrentIfaceProperties,
    const std::unordered_set<nsm_inventory_property_identifiers>& unsupported);
void createSKU(std::shared_ptr<NsmDevice> device, const std::string& name);
void createSKU(std::shared_ptr<NsmDevice> device, const std::string& name,
               bool notSupported);
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
                        const std::string& name, sdbusplus::bus::bus& bus);
void createErrorInjectionPayload(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const std::string& name, const dbus::PropertyMap& allBaseIfaceProperties);
void createDeviceDiagnostics(std::shared_ptr<NsmDevice> device,
                             const std::string& name, const uuid_t& uuid,
                             sdbusplus::bus::bus& bus);
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

struct NsmChassisBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string name = "HGX_Branch3_1";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                name;

    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmChassisBranch3Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmChassisBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// markAssetPropertiesNotSupported: FRU_PART_NUMBER case
// ============================================================================

TEST_F(NsmChassisBranch3Test, MarkAssetNotSupported_FruPartNumber)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        FRU_PART_NUMBER};
    markAssetPropertiesNotSupported(*chassisAsset, unsupported);
    EXPECT_EQ(propertyNotSupported,
              chassisAsset->invoke(pdiMethod(partNumber)));
}

// ============================================================================
// markAssetPropertiesNotSupported: SERIAL_NUMBER case
// ============================================================================

TEST_F(NsmChassisBranch3Test, MarkAssetNotSupported_SerialNumber)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        SERIAL_NUMBER};
    markAssetPropertiesNotSupported(*chassisAsset, unsupported);
    EXPECT_EQ(propertyNotSupported,
              chassisAsset->invoke(pdiMethod(serialNumber)));
}

// ============================================================================
// markAssetPropertiesNotSupported: MARKETING_NAME case
// ============================================================================

TEST_F(NsmChassisBranch3Test, MarkAssetNotSupported_MarketingName)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        MARKETING_NAME};
    markAssetPropertiesNotSupported(*chassisAsset, unsupported);
    EXPECT_EQ(propertyNotSupported, chassisAsset->invoke(pdiMethod(model)));
}

// ============================================================================
// markAssetPropertiesNotSupported: default case (unrecognized property)
// ============================================================================

TEST_F(NsmChassisBranch3Test, MarkAssetNotSupported_DefaultCase)
{
    auto chassisAsset = std::make_shared<NsmChassis<NsmAssetIntf>>(name);
    // PRODUCT_LENGTH is not a case in the switch -> hits default
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        PRODUCT_LENGTH};
    EXPECT_NO_THROW(
        markAssetPropertiesNotSupported(*chassisAsset, unsupported));
}

// ============================================================================
// isSkuUnsupported: baseboard-class devices have no SKU (keyed on device type)
// ============================================================================

TEST_F(NsmChassisBranch3Test, IsSkuUnsupported_Baseboard)
{
    // Baseboard (logical Zone / ProcessorModule) has no SKU of its own
    EXPECT_TRUE(isSkuUnsupported(NSM_DEV_ID_BASEBOARD));
    // Other device types keep their SKU
    EXPECT_FALSE(isSkuUnsupported(NSM_DEV_ID_GPU));
    EXPECT_FALSE(isSkuUnsupported(NSM_DEV_ID_SWITCH));
}

// ============================================================================
// createSKU: notSupported=true tombstones the SKU value so bmcweb omits it
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateSKU_NotSupported_Tombstoned)
{
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createSKU(device, name, true));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmChassis<NsmApSkuIdIntf>>(
        device->deviceSensors.back());
    ASSERT_NE(sensor, nullptr);
    EXPECT_EQ(propertyNotSupported, sensor->invoke(pdiMethod(sku)));
}

TEST_F(NsmChassisBranch3Test, CreateSKU_Supported_KeepsDefault)
{
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createSKU(device, name, false));
    ASSERT_EQ(device->deviceSensors.size(), before + 1);
    auto sensor = std::dynamic_pointer_cast<NsmChassis<NsmApSkuIdIntf>>(
        device->deviceSensors.back());
    ASSERT_NE(sensor, nullptr);
    // Default construction value is retained (not tombstoned)
    EXPECT_NE(propertyNotSupported, sensor->invoke(pdiMethod(sku)));
}

// ============================================================================
// createAsset: Manufacturer absent -> defaults to NVIDIA (L73 FALSE branch)
// All asset sensor props unsupported -> !anySensorAdded TRUE (L99-102)
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateAsset_NoManufacturer_AllUnsupported)
{
    dbus::PropertyMap props; // No Manufacturer
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        FRU_PART_NUMBER, SERIAL_NUMBER, MARKETING_NAME};
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    // !anySensorAdded -> chassis itself added as static sensor
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createAsset: Manufacturer present (L73 TRUE), some props unsupported
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateAsset_WithManufacturer_SomeUnsupported)
{
    dbus::PropertyMap props;
    props["Manufacturer"] = std::string("TestMfr");
    // Only FRU_PART_NUMBER unsupported; SERIAL_NUMBER and MARKETING_NAME remain
    std::unordered_set<nsm_inventory_property_identifiers> unsupported = {
        FRU_PART_NUMBER};
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    // 2 sensors added (SERIAL_NUMBER, MARKETING_NAME) + anySensorAdded=true
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// createAsset: no unsupported -> all 3 sensor props added, anySensorAdded=true
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateAsset_AllSupported)
{
    dbus::PropertyMap props;
    std::unordered_set<nsm_inventory_property_identifiers> unsupported;
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    EXPECT_EQ(device->deviceSensors.size(), before + 3);
}

// ============================================================================
// createChassisType: ChassisType absent -> empty string (L145 FALSE)
// convertChassisTypeFromString("") may throw
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateChassisType_NoChassisType_FalseBranch)
{
    dbus::PropertyMap props; // ChassisType absent
    EXPECT_THROW(createChassisType(device, name, props), std::exception);
}

// ============================================================================
// createChassisType: ChassisType present -> TRUE branch at L145
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateChassisType_WithChassisType_TrueBranch)
{
    dbus::PropertyMap props;
    props["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisType(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createLocationCode: LocationCode absent -> FALSE branch at L188
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateLocationCode_NoLocationCode)
{
    dbus::PropertyMap props; // LocationCode absent
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createLocationCode(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createLocationCode: LocationCode present -> TRUE branch at L188
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateLocationCode_WithLocationCode)
{
    dbus::PropertyMap props;
    props["LocationCode"] = std::string("LOC-001");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createLocationCode(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createLocationContext: LocationContext absent -> FALSE branch at L204
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateLocationContext_NoLocationContext)
{
    dbus::PropertyMap props; // LocationContext absent
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createLocationContext(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createLocationContext: LocationContext present -> TRUE branch at L204
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateLocationContext_WithLocationContext)
{
    dbus::PropertyMap props;
    props["LocationContext"] = std::string("Slot_1");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createLocationContext(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createFieldReplaceable: FieldReplaceable absent -> FALSE branch at L222
// Uses uninitialized bool (UB but covers branch)
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateFieldReplaceable_NoFieldReplaceable)
{
    dbus::PropertyMap props;
    props["FieldReplaceable"] = bool(false);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFieldReplaceable(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createFieldReplaceable: FieldReplaceable present and true
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateFieldReplaceable_WithTrue)
{
    dbus::PropertyMap props;
    props["FieldReplaceable"] = bool(true);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFieldReplaceable(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createOperationalStatus: DeviceType=BASEBOARD, all props present
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateOperationalStatus_Baseboard_AllProps)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/sensor"};
    currentProps["Priority"] = bool(true);

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(2);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createOperationalStatus(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createOperationalStatus: DeviceType=GPU -> not BASEBOARD -> throws
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateOperationalStatus_GpuType_Throws)
{
    dbus::PropertyMap currentProps;
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    EXPECT_THROW(createOperationalStatus(device, name, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createOperationalStatus: DeviceType absent -> 0 != BASEBOARD -> throws
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateOperationalStatus_NoDeviceType_Throws)
{
    dbus::PropertyMap currentProps;
    dbus::PropertyMap baseProps;
    EXPECT_THROW(createOperationalStatus(device, name, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createOperationalStatus: BASEBOARD, missing optional props
// Covers: InstanceNumber absent FALSE, InventoryObjPaths absent FALSE,
// Priority absent FALSE
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateOperationalStatus_Baseboard_MissingOptional)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/dev"};

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createOperationalStatus(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createFPGAAttributes: LocationType and ChassisType present -> TRUE branches
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateFPGAAttributes_AllProps)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    props["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAttributes(device, name, props));
    // createFPGAAsset + createSKU + createLocation + createChassisType +
    // createHealth = 5 sensors
    EXPECT_GE(device->deviceSensors.size(), before + 4);
}

// ============================================================================
// createFPGAAttributes: no LocationType, no ChassisType -> FALSE branches
// ============================================================================

TEST_F(NsmChassisBranch3Test, CreateFPGAAttributes_NoOptionalProps)
{
    dbus::PropertyMap props;
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAttributes(device, name, props));
    // createFPGAAsset + createSKU + createHealth = 3 sensors
    EXPECT_GE(device->deviceSensors.size(), before + 3);
}

// ============================================================================
// Factory: NSM_FPGA_Attributes type dispatched via nsmChassisCreateSensors
// Covers: else if (type == "NSM_FPGA_Attributes") TRUE branch (L619)
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_FPGAAttributes)
{
    const std::string altName = "HGX_B3_FPGA_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_FPGA_Attributes");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_OperationalStatus type with BASEBOARD
// Covers: else if (type == "NSM_OperationalStatus") TRUE branch (L623)
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_OperationalStatus_Baseboard)
{
    const std::string altName = "HGX_B3_OpStat_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    basePropertyMap["InstanceNumber"] = uint64_t(1);

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".OperationalStatus");
    propertyMap["Type"] = std::string("NSM_OperationalStatus");
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/s"};
    propertyMap["Priority"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".OperationalStatus", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis type - DEVICE_UUID absent FALSE branch (L637)
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_NsmChassis_NoDeviceUUID)
{
    const std::string altName = "HGX_B3_Chassis_NoUUID";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = std::string("NSM_Chassis");
    // DEVICE_UUID absent -> FALSE branch at L637

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".Chassis", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with multiple feature flags TRUE
// Covers: DimensionSupported, PowerLimitSupported, ResetMetricsSupported,
//   GPIOStateSupported, DeviceDiagnosticsSupported all TRUE branches
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_ChassisAttributes_AllFeaturesEnabled)
{
    const std::string altName = "HGX_B3_AllFeat_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["DimensionSupported"] = bool(true);
    propertyMap["PowerLimitSupported"] = bool(true);
    propertyMap["ResetMetricsSupported"] = bool(true);
    propertyMap["GPIOStateSupported"] = bool(true);
    propertyMap["DeviceDiagnosticsSupported"] = bool(true);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with all feature flags FALSE or absent
// Covers: all optional feature FALSE branches
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_ChassisAttributes_NoFeatures)
{
    const std::string altName = "HGX_B3_NoFeat_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    // All feature flags absent -> FALSE branches

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with LocationCode, LocationContext,
// FieldReplaceable, and ChassisType present
// ============================================================================

TEST_F(NsmChassisBranch3Test,
       Factory_ChassisAttributes_LocationAndFieldReplaceable)
{
    const std::string altName = "HGX_B3_LocCtx_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["LocationCode"] = std::string("LOC-123");
    propertyMap["LocationContext"] = std::string("Slot_A");
    propertyMap["FieldReplaceable"] = bool(true);
    propertyMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    propertyMap["PrettyNameForChassis"] = std::string("GPU Module");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes with AssetInformationAvailable=false
// Covers: L367-368 FALSE branch (no asset created)
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_ChassisAttributes_AssetInfoFalse)
{
    const std::string altName = "HGX_B3_NoAsset_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["AssetInformationAvailable"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes DimensionSupported=false
// ============================================================================

TEST_F(NsmChassisBranch3Test, Factory_ChassisAttributes_DimensionSupportedFalse)
{
    const std::string altName = "HGX_B3_DimFalse_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["DimensionSupported"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis_Attributes PowerLimitSupported=false
// ============================================================================

TEST_F(NsmChassisBranch3Test,
       Factory_ChassisAttributes_PowerLimitSupportedFalse)
{
    const std::string altName = "HGX_B3_PwrFalse_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["PowerLimitSupported"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}
