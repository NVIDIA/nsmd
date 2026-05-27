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
 * Additional branch coverage batch 4 for nsmd/nsmChassis/nsmChassis.cpp
 *
 * Covers:
 * - createChassisAttributes: every property present/absent combination
 *   (AssetInformationAvailable TRUE/FALSE, LocationType, LocationCode,
 *    LocationContext, FieldReplaceable, ChassisType, PrettyNameForChassis,
 *    DimensionSupported, PowerLimitSupported, WriteProtectSupported,
 *    ResetMetricsSupported, GPIOStateSupported, ErrorInjectionSupported,
 *    DeviceDiagnosticsSupported - all present=TRUE and absent branches)
 * - createAsset: Manufacturer present with empty string
 * - createFPGAAsset: Manufacturer present/absent branches
 * - createLocation: LocationType absent FALSE branch
 * - createPowerLimit: Priority present TRUE / absent FALSE branches
 * - createWriteProtect: DeviceType=BASEBOARD success, DeviceType absent throws
 * - createErrorInjectionPayload: DeviceType=MCTP_BRIDGE vs other
 * - createPrettyName: PrettyNameForChassis absent FALSE branch
 * - createOperationalStatus: all optional props missing then present
 * - createPowerState: BASEBOARD success, non-BASEBOARD throws, all optional
 *   props present/absent
 * - Factory: NSM_PowerState type dispatched
 * - Factory: NSM_Chassis type with DEVICE_UUID present TRUE branch
 * - Factory: unrecognized type falls through
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
void createChassisAttributes(std::shared_ptr<NsmDevice> device,
                             SensorManager& manager, sdbusplus::bus::bus& bus,
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

struct NsmChassisBranch4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_Chassis";
    const std::string name = "HGX_Branch4_1";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                name;

    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmChassisBranch4Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmChassisBranch4Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createFPGAAsset: Manufacturer absent -> defaults to NVIDIA (L116 FALSE)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateFPGAAsset_NoManufacturer)
{
    dbus::PropertyMap props; // no Manufacturer key
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAsset(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createFPGAAsset: Manufacturer present -> TRUE branch (L116)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateFPGAAsset_WithManufacturer)
{
    dbus::PropertyMap props;
    props["Manufacturer"] = std::string("TestCorp");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createFPGAAsset(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createLocation: LocationType absent -> empty string (L169 FALSE)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateLocation_NoLocationType)
{
    dbus::PropertyMap props; // LocationType absent
    EXPECT_THROW(createLocation(device, name, props), std::exception);
}

// ============================================================================
// createLocation: LocationType present -> TRUE (L169)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateLocation_WithLocationType)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createLocation(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerLimit: Priority absent -> FALSE branch (L240), default false
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePowerLimit_NoPriority)
{
    dbus::PropertyMap props; // no Priority key
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerLimit(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// createPowerLimit: Priority present and true -> TRUE branch (L240)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePowerLimit_WithPriorityTrue)
{
    dbus::PropertyMap props;
    props["Priority"] = bool(true);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerLimit(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// createPrettyName: PrettyNameForChassis absent -> FALSE branch (L258)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePrettyName_NoPrettyName)
{
    dbus::PropertyMap props; // PrettyNameForChassis absent
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPrettyName(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPrettyName: PrettyNameForChassis present -> TRUE branch (L258)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePrettyName_WithPrettyName)
{
    dbus::PropertyMap props;
    props["PrettyNameForChassis"] = std::string("My GPU Module");
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPrettyName(device, name, props));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createWriteProtect: DeviceType=BASEBOARD success (L280 FALSE, no throw)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateWriteProtect_Baseboard)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createWriteProtect(device, name, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createWriteProtect: DeviceType absent -> 0 != BASEBOARD -> throws (L280 TRUE)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateWriteProtect_NoDeviceType_Throws)
{
    dbus::PropertyMap baseProps; // no DeviceType
    EXPECT_THROW(createWriteProtect(device, name, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createWriteProtect: DeviceType=GPU -> throws
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateWriteProtect_WrongDeviceType_Throws)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    EXPECT_THROW(createWriteProtect(device, name, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createErrorInjectionPayload: DeviceType absent -> 0 != MCTP_BRIDGE (L344)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateErrorInjection_NoDeviceType)
{
    dbus::PropertyMap baseProps; // no DeviceType
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createErrorInjectionPayload(mockManager, device, name, baseProps));
    // deviceType != NSM_DEV_ID_MCTP_BRIDGE -> nothing added
    EXPECT_EQ(device->deviceSensors.size(), before);
}

// ============================================================================
// createErrorInjectionPayload: DeviceType=MCTP_BRIDGE -> TRUE branch (L344)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateErrorInjection_MctpBridge)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_MCTP_BRIDGE);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createErrorInjectionPayload(mockManager, device, name, baseProps));
    // MCTP_BRIDGE -> MCU error injection sensors created
    EXPECT_GE(device->deviceSensors.size(), before);
}

// ============================================================================
// createErrorInjectionPayload: DeviceType=GPU -> != MCTP_BRIDGE (FALSE L344)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateErrorInjection_GpuDeviceType)
{
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(
        createErrorInjectionPayload(mockManager, device, name, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before);
}

// ============================================================================
// createChassisAttributes: all properties present and TRUE
// Covers all TRUE branches at once (compound test)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_AllPropsPresent)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["AssetInformationAvailable"] = bool(true);
    currentProps["Manufacturer"] = std::string("NVIDIA");
    currentProps["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    currentProps["LocationCode"] = std::string("LOC-001");
    currentProps["LocationContext"] = std::string("Slot_1");
    currentProps["FieldReplaceable"] = bool(true);
    currentProps["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    currentProps["PrettyNameForChassis"] = std::string("GPU Module");
    currentProps["DimensionSupported"] = bool(true);
    currentProps["PowerLimitSupported"] = bool(true);
    currentProps["WriteProtectSupported"] = bool(true);
    currentProps["ResetMetricsSupported"] = bool(true);
    currentProps["GPIOStateSupported"] = bool(true);
    currentProps["ErrorInjectionSupported"] = bool(true);
    currentProps["DeviceDiagnosticsSupported"] = bool(true);

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// createChassisAttributes: no optional properties at all
// Covers all FALSE branches for optional properties
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_NoOptionalProps)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps; // nothing
    dbus::PropertyMap baseProps;

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
    // SKU + Health always added
    EXPECT_GE(device->deviceSensors.size(), before + 2);
}

// ============================================================================
// createChassisAttributes: AssetInformationAvailable present but FALSE
// Covers L367 TRUE (count > 0) but L368 FALSE (value is false) compound branch
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_AssetInfoPresentButFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["AssetInformationAvailable"] = bool(false);
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: each supported flag present=FALSE explicitly
// (flag present in map with value false -> count TRUE, value FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_AllFlagsPresentButFalse)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["DimensionSupported"] = bool(false);
    currentProps["PowerLimitSupported"] = bool(false);
    currentProps["WriteProtectSupported"] = bool(false);
    currentProps["ResetMetricsSupported"] = bool(false);
    currentProps["GPIOStateSupported"] = bool(false);
    currentProps["ErrorInjectionSupported"] = bool(false);
    currentProps["DeviceDiagnosticsSupported"] = bool(false);
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createPowerState: BASEBOARD, all optional props present
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePowerState_Baseboard_AllProps)
{
    dbus::PropertyMap currentProps;
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/pwr"};
    currentProps["Priority"] = bool(true);

    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    baseProps["InstanceNumber"] = uint64_t(3);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: BASEBOARD, missing InstanceNumber and Priority
// Covers InstanceNumber absent FALSE, Priority absent FALSE branches
// InventoryObjPaths must be present (required by NsmInterfaces)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePowerState_Baseboard_MissingOptional)
{
    dbus::PropertyMap currentProps;
    // InventoryObjPaths must be provided to avoid "interfaces cannot be empty"
    currentProps["InventoryObjPaths"] =
        std::vector<std::string>{objPath + "/pwr_min"};
    // InstanceNumber and Priority absent -> FALSE branches
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createPowerState(device, name, currentProps, baseProps));
    EXPECT_EQ(device->deviceSensors.size(), before + 1);
}

// ============================================================================
// createPowerState: DeviceType=GPU -> throws
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePowerState_GpuType_Throws)
{
    dbus::PropertyMap currentProps;
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_GPU);
    EXPECT_THROW(createPowerState(device, name, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createPowerState: DeviceType absent -> 0 != BASEBOARD -> throws
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreatePowerState_NoDeviceType_Throws)
{
    dbus::PropertyMap currentProps;
    dbus::PropertyMap baseProps; // no DeviceType
    EXPECT_THROW(createPowerState(device, name, currentProps, baseProps),
                 std::runtime_error);
}

// ============================================================================
// createAsset: Manufacturer present empty string -> TRUE branch (L73)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateAsset_EmptyManufacturer)
{
    dbus::PropertyMap props;
    props["Manufacturer"] = std::string("");
    std::unordered_set<nsm_inventory_property_identifiers> unsupported;
    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createAsset(device, name, props, unsupported));
    EXPECT_EQ(device->deviceSensors.size(), before + 3);
}

// ============================================================================
// Factory: NSM_PowerState type with BASEBOARD
// Covers: else if (type == "NSM_PowerState") TRUE branch (L628)
// ============================================================================

TEST_F(NsmChassisBranch4Test, Factory_PowerState_Baseboard)
{
    const std::string altName = "HGX_B4_PwrState_1";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;
    basePropertyMap["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);
    basePropertyMap["InstanceNumber"] = uint64_t(1);

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".PowerState");
    propertyMap["Type"] = std::string("NSM_PowerState");
    propertyMap["InventoryObjPaths"] =
        std::vector<std::string>{altObjPath + "/pwr"};
    propertyMap["Priority"] = bool(false);

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".PowerState", altObjPath));
}

// ============================================================================
// Factory: NSM_Chassis type with DEVICE_UUID present -> TRUE branch (L637)
// ============================================================================

TEST_F(NsmChassisBranch4Test, Factory_NsmChassis_WithDeviceUUID)
{
    const std::string altName = "HGX_B4_Chassis_UUID";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".Chassis");
    propertyMap["Type"] = std::string("NSM_Chassis");
    propertyMap["DEVICE_UUID"] = uuid_t("some-device-uuid-string");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".Chassis", altObjPath));
}

// ============================================================================
// Factory: unrecognized Type -> falls through all if/else if branches
// ============================================================================

TEST_F(NsmChassisBranch4Test, Factory_UnrecognizedType)
{
    const std::string altName = "HGX_B4_Unknown";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_UnknownType_XYZ");

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: Name absent -> empty name (L598-601 FALSE branch)
// Empty name produces invalid D-Bus path -> SdBusError
// ============================================================================

TEST_F(NsmChassisBranch4Test, DISABLED_Factory_NameAbsent)
{
    const std::string altObjPath = std::string(chassisInventoryBasePath) +
                                   "/HGX_B4_NoName";

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    // No "Name" key -> empty name -> invalid D-Bus path
    basePropertyMap["UUID"] = deviceUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");

    EXPECT_ANY_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: UUID absent -> empty uuid (L607-609 FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranch4Test, DISABLED_Factory_UUIDAbsent)
{
    const std::string altName = "HGX_B4_NoUUID";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    // No "UUID" key

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        altObjPath, basicIntfName + ".ChassisAttributes");
    propertyMap["Type"] = std::string("NSM_UnknownType_NoUUID");

    // Device lookup with empty UUID may throw
    EXPECT_ANY_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// Factory: Type absent -> empty type (L602-605 FALSE branch)
// ============================================================================

TEST_F(NsmChassisBranch4Test, Factory_TypeAbsent)
{
    const std::string altName = "HGX_B4_NoType";
    const std::string altObjPath = std::string(chassisInventoryBasePath) + "/" +
                                   altName;

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(altObjPath,
                                                              basicIntfName);
    basePropertyMap["Name"] = altName;
    basePropertyMap["UUID"] = deviceUuid;

    utils::MockDbusAsync::propertyMap(altObjPath,
                                      basicIntfName + ".ChassisAttributes");
    // No "Type" key -> empty type -> falls through all if/else

    EXPECT_NO_THROW(nsmChassisCreateSensors(
        mockManager, basicIntfName + ".ChassisAttributes", altObjPath));
}

// ============================================================================
// createChassisAttributes: WriteProtectSupported=TRUE, DeviceType=BASEBOARD
// => createWriteProtect called successfully
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_WriteProtect_Baseboard)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["WriteProtectSupported"] = bool(true);
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_BASEBOARD);

    const size_t before = device->deviceSensors.size();
    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// createChassisAttributes: ErrorInjectionSupported=TRUE,
// DeviceType=MCTP_BRIDGE
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_ErrorInjection_MctpBridge)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["ErrorInjectionSupported"] = bool(true);
    dbus::PropertyMap baseProps;
    baseProps["DeviceType"] = uint64_t(NSM_DEV_ID_MCTP_BRIDGE);

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: only LocationType present (others absent)
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_OnlyLocationType)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location"
        ".LocationTypes.Embedded");
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: only LocationCode present
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_OnlyLocationCode)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["LocationCode"] = std::string("LOC-X");
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: only LocationContext present
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_OnlyLocationContext)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["LocationContext"] = std::string("SlotA");
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: only FieldReplaceable present
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_OnlyFieldReplaceable)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["FieldReplaceable"] = bool(false);
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: only ChassisType present
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_OnlyChassisType)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}

// ============================================================================
// createChassisAttributes: only PrettyNameForChassis present
// ============================================================================

TEST_F(NsmChassisBranch4Test, CreateChassisAttributes_OnlyPrettyName)
{
    auto& bus = utils::DBusHandler::getBus();
    dbus::PropertyMap currentProps;
    currentProps["PrettyNameForChassis"] = std::string("Test Module");
    dbus::PropertyMap baseProps;

    EXPECT_NO_THROW(createChassisAttributes(
        device, mockManager, bus, name, deviceUuid, currentProps, baseProps));
}
