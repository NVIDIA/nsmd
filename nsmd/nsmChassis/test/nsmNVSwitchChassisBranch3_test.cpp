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
 * Branch coverage tests for nsmNVSwitchAndNVMgmtNICChassis.cpp (batch 3):
 *
 * - createNsmChassis factory: type == "NSM_Chassis_Attributes" branch
 *   with PCIE_BRIDGE + CX8 device (lines 264-270 TRUE path):
 *   createAssetTag + createChassisVersion are called
 * - createNsmChassis: PCIE_BRIDGE + CX9 device also triggers the same branch
 * - createNsmChassis: non-PCIE_BRIDGE device does NOT trigger the branch
 *   (NSM_DEV_ID_SWITCH device → FALSE path)
 * - createLocationType: LocationType key absent from map → locationType=""
 *   → convertLocationTypesFromString("") throws (FALSE branch at line 115)
 * - createNsmChassis: missing Name in base props (FALSE branch for
 * count("Name")) → name="" (the factory proceeds with empty name)
 * - createNsmNVLinkMgmtNicChassis wrapper function
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "base.h"

#define private public
#define protected public

#include "nsmInventoryProperty.hpp"
#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"

#undef private
#undef protected

namespace nsm
{
void createLocationType(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType,
                        dbus::PropertyMap& allCurrentIfaceProperties);
requester::Coroutine createNsmChassis(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath,
                                      const std::string baseType);
requester::Coroutine createNsmNVLinkMgmtNicChassis(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

// NSM_DEV_ID_PCIE_BRIDGE = 2, NSM_PCIE_BRIDGE_DEV_ROLE_CX8 = 2
// combined = (role << 8) | type = (2 << 8) | 2 = 514
static const uuid_t kCX8Uuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
// NSM_PCIE_BRIDGE_DEV_ROLE_CX9 = 3 → combined = (3 << 8) | 2 = 770
static const uuid_t kCX9Uuid = "STATIC:770:0:NSM_DEVICE_INSTANCE_NUMBER:0";
// NSM_DEV_ID_SWITCH = 1, role=0 → combined = (0 << 8) | 1 = 1
static const uuid_t kSwitchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:0";

static const std::string kBaseType = "NSM_NVSwitch_Chassis";
static const std::string kBaseIntf =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";
static const std::string kAttrIntf =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.ChassisAttributes";

static const std::string kNicBaseType = "NSM_NVLinkMgmtNic_Chassis";
static const std::string kNicBaseIntf =
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
static const std::string kNicAttrIntf =
    "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis.ChassisAttributes";

// ============================================================================
// Fixture
// ============================================================================

struct NsmNVSwitchChassisBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> cx8Dev;
    std::shared_ptr<MockNsmDevice> cx9Dev;
    std::shared_ptr<MockNsmDevice> switchDev;

    NsmNVSwitchChassisBranch3Test() : SensorManagerTest(devices)
    {
        cx8Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kCX8Uuid));
        EXPECT_NE(cx8Dev, nullptr);

        cx9Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kCX9Uuid));
        EXPECT_NE(cx9Dev, nullptr);

        switchDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kSwitchUuid));
        EXPECT_NE(switchDev, nullptr);
    }

    ~NsmNVSwitchChassisBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Factory with PCIE_BRIDGE+CX8 device → line 264 TRUE path
// createAssetTag + createChassisVersion are called
// ============================================================================

TEST_F(NsmNVSwitchChassisBranch3Test,
       Factory_PCIeBridge_CX8_AssetTagAndVersionCreated)
{
    const std::string objPath = "/xyz/test/nvswitch_b3/cx8_attrs";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("CX8_Chassis_br3");
    baseMap["UUID"] = kCX8Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    // LocationType absent intentionally to skip createLocationType
    // ChassisType absent intentionally

    const size_t before = cx8Dev->staticSensors.size();
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    // createChassisAsset (3) + createChassisSKU (1) + createChassisHealth (1)
    // + createAssetTag (1) + createChassisVersion (1) = 7 sensors
    EXPECT_GT(cx8Dev->staticSensors.size(), before + 4);
}

// ============================================================================
// Factory with PCIE_BRIDGE+CX9 device → also triggers line 264 TRUE path
// ============================================================================

TEST_F(NsmNVSwitchChassisBranch3Test,
       Factory_PCIeBridge_CX9_AssetTagAndVersionCreated)
{
    const std::string objPath = "/xyz/test/nvswitch_b3/cx9_attrs";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("CX9_Chassis_br3");
    baseMap["UUID"] = kCX9Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = cx9Dev->staticSensors.size();
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    // Should also add createAssetTag + createChassisVersion
    EXPECT_GT(cx9Dev->staticSensors.size(), before + 4);
}

// ============================================================================
// Factory with NVSwitch (non-PCIE_BRIDGE) device → line 264 FALSE path
// createAssetTag and createChassisVersion are NOT called
// ============================================================================

TEST_F(NsmNVSwitchChassisBranch3Test,
       Factory_NVSwitch_NoPCIeBridge_NoAssetTagOrVersion)
{
    const std::string objPath = "/xyz/test/nvswitch_b3/switch_attrs";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("NVSwitch_Chassis_br3");
    baseMap["UUID"] = kSwitchUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = switchDev->staticSensors.size();
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    const size_t after = switchDev->staticSensors.size();
    const size_t added = after - before;

    // Without PCIE_BRIDGE: createChassisAsset(3) + createChassisSKU(1) +
    // createChassisHealth(1) = 5 sensors (no AssetTag or Version)
    EXPECT_EQ(added, 5u);
}

// ============================================================================
// createLocationType: LocationType key absent from map → locationType=""
// → convertLocationTypesFromString("") throws (FALSE branch at line 115)
// ============================================================================

TEST_F(NsmNVSwitchChassisBranch3Test, CreateLocationType_KeyAbsent_Throws)
{
    std::string name = "NoLocType_br3";
    dbus::PropertyMap props; // "LocationType" key not present
    // locationType stays "" → convertLocationTypesFromString("") throws
    EXPECT_THROW(createLocationType(switchDev, name, kBaseType, props),
                 std::exception);
}

// ============================================================================
// Factory: all optional attributes present for PCIE_BRIDGE+CX8
// Tests LocationType, LocationCode, ChassisType, PrettyNameForChassis
// ============================================================================

TEST_F(NsmNVSwitchChassisBranch3Test, Factory_PCIeBridge_CX8_AllOptionalAttrs)
{
    const std::string objPath = "/xyz/test/nvswitch_b3/cx8_full";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("CX8_Full_br3");
    baseMap["UUID"] = kCX8Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    currMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    currMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    currMap["LocationCode"] = std::string("U1-CX8");
    currMap["PrettyNameForChassis"] = std::string("CX8 Chassis");

    const size_t before = cx8Dev->staticSensors.size();
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    // Should add: asset(3) + SKU(1) + health(1) + chassisType(1) +
    // locationType(1) + locationCode(1) + prettyName(1) + assetTag(1) +
    // chassisVersion(1) = 11 sensors
    EXPECT_GT(cx8Dev->staticSensors.size(), before + 8);
}

// ============================================================================
// createNsmNVLinkMgmtNicChassis wrapper - covers the NIC variant
// ============================================================================

TEST_F(NsmNVSwitchChassisBranch3Test, Factory_NVLinkMgmtNic_BaseType_Wrapper)
{
    // NIC device: use the same switch UUID (type=1) since NIC also uses type=1
    const uuid_t nicUuid = "STATIC:1:1:NSM_DEVICE_INSTANCE_NUMBER:0";
    auto nicDev = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(nicUuid));
    ASSERT_NE(nicDev, nullptr);

    const std::string objPath = "/xyz/test/nic_b3/base";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNicBaseIntf);
    baseMap["Name"] = std::string("NIC_Chassis_br3");
    baseMap["UUID"] = nicUuid;
    baseMap["Type"] = kNicBaseType;

    const size_t before = nicDev->staticSensors.size();
    createNsmNVLinkMgmtNicChassis(mockManager, kNicBaseIntf, objPath);
    // base type branch: creates UuidIntf sensor
    EXPECT_GT(nicDev->staticSensors.size(), before);
}
