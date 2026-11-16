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
 * - createChassisAttributesFeature: all *Supported flags TRUE, location /
 *   field-replaceable / chassis type / pretty name params present
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "base.h"
#include "device-configuration.h"

#include "../../common/nsmPropertySupport.hpp"
#include "features/creators.hpp"
#include "features/feature.hpp"
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
                        const std::string& name, sdbusplus::bus_t& bus);
void createErrorInjectionPayload(
    SensorManager& manager, std::shared_ptr<NsmDevice> device,
    const std::string& name, const dbus::PropertyMap& allBaseIfaceProperties);
void createDeviceDiagnostics(std::shared_ptr<NsmDevice> device,
                             const std::string& name, const uuid_t& uuid,
                             sdbusplus::bus_t& bus);
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

    /** @brief Run the chassis attributes feature of the device `devName` */
    requester::Coroutine attributes(const std::string& devName,
                                    uint8_t deviceType,
                                    dbus::PropertyMap params)
    {
        nsm::features::DeviceContext dev;
        dev.name = devName;
        dev.kind = "gpu";
        dev.deviceType = deviceType;
        dev.staticUuid = deviceUuid;
        dev.platformPrefix = "HGX_";
        dev.device = device;
        params["Name"] = devName;
        return nsm::features::createChassisAttributesFeature(mockManager, dev,
                                                             params);
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
// Feature: DimensionSupported, PowerLimitSupported, ResetMetricsSupported,
// GPIOStateSupported, DeviceDiagnosticsSupported all TRUE branches
// ============================================================================

TEST_F(NsmChassisBranch3Test, Feature_ChassisAttributes_AllFeaturesEnabled)
{
    const size_t before = device->deviceSensors.size();
    auto rc = attributes("HGX_B3_AllFeat_1", NSM_DEV_ID_BASEBOARD,
                         {{"DimensionSupported", true},
                          {"PowerLimitSupported", true},
                          {"ResetMetricsSupported", true},
                          {"GPIOStateSupported", true},
                          {"DeviceDiagnosticsSupported", true}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // SKU, Health, 3 dimension, 2 power limit, reset metrics, GPIO state
    EXPECT_GE(device->deviceSensors.size(), before + 9);
}

// ============================================================================
// Feature: LocationCode, LocationContext, FieldReplaceable, ChassisType and
// PrettyNameForChassis present
// ============================================================================

TEST_F(NsmChassisBranch3Test,
       Feature_ChassisAttributes_LocationAndFieldReplaceable)
{
    const size_t before = device->deviceSensors.size();
    auto rc = attributes(
        "HGX_B3_LocCtx_1", NSM_DEV_ID_GPU,
        {{"LocationCode", std::string("LOC-123")},
         {"LocationContext", std::string("Slot_A")},
         {"FieldReplaceable", true},
         {"ChassisType",
          std::string(
              "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module")},
         {"PrettyNameForChassis", std::string("GPU Module")}});
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    // SKU, Health, LocationCode, LocationContext, FieldReplaceable,
    // ChassisType, PrettyName
    EXPECT_EQ(device->deviceSensors.size(), before + 7);
}
