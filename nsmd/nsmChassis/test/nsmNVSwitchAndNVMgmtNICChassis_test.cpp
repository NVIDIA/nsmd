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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"

namespace nsm
{
requester::Coroutine createNsmChassis(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath,
                                      const std::string baseType);
} // namespace nsm

using namespace nsm;

struct NsmNVSwitchChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NVSwitch0";
    const std::string type = "NSM_NVSwitch";

    NsmDeviceTable devices;

    NsmNVSwitchChassisTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmNVSwitchChassisTest, goodTestAssetChassis)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(name, type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestChassisIntf)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(name, type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestHealthIntf)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(name,
                                                                          type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestLocationIntf)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<LocationIntf>>(name, type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestUuidIntf)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(name,
                                                                        type);
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), name);
    EXPECT_EQ(chassis->getType(), type);
}

TEST_F(NsmNVSwitchChassisTest, goodTestMultipleInstances)
{
    // Test creating multiple chassis instances
    auto chassis1 = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(
        "NVSwitch0", type);
    auto chassis2 = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(
        "NVSwitch1", type);
    auto chassis3 = std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(
        "NVSwitch2", type);

    EXPECT_NE(chassis1, nullptr);
    EXPECT_NE(chassis2, nullptr);
    EXPECT_NE(chassis3, nullptr);

    EXPECT_EQ(chassis1->getName(), "NVSwitch0");
    EXPECT_EQ(chassis2->getName(), "NVSwitch1");
    EXPECT_EQ(chassis3->getName(), "NVSwitch2");
}

TEST_F(NsmNVSwitchChassisTest, goodTestNVMgmtNIC)
{
    const std::string nicName = "NVMgmtNIC0";
    auto nic = std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(nicName,
                                                                        type);
    EXPECT_NE(nic, nullptr);
    EXPECT_EQ(nic->getName(), nicName);
}

// update() for non-UuidIntf: constexpr-else branch -> co_return NSM_SUCCESS //
// immediately (line 59-61 in
// nsmNVSwitchAndNVMgmtNICChassis.cpp). nsmDevice parameter is not accessed at
// all.
TEST_F(NsmNVSwitchChassisTest, Update_NonUuidIntf_ReturnsImmediately)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<NsmAssetIntf>>(name, type);
    ASSERT_NE(chassis, nullptr);

    // NsmAssetIntf != UuidIntf -> constexpr-else -> co_return NSM_SUCCESS
    // No co_await, no device access; passing nullptr is safe.
    auto coro = chassis->update(nullptr);
    // Coroutine completes synchronously with no side-effects
}

// update() for ChassisIntf (another non-UuidIntf): same constexpr-else path.
TEST_F(NsmNVSwitchChassisTest, Update_ChassisIntf_ReturnsImmediately)
{
    auto chassis =
        std::make_shared<NsmNVSwitchAndNicChassis<ChassisIntf>>(name, type);
    ASSERT_NE(chassis, nullptr);

    auto coro = chassis->update(nullptr);
}

// update() for HealthIntf (non-UuidIntf): same constexpr-else path.
TEST_F(NsmNVSwitchChassisTest, Update_HealthIntf_ReturnsImmediately)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<HealthIntf>>(name,
                                                                          type);
    ASSERT_NE(chassis, nullptr);

    auto coro = chassis->update(nullptr);
}

// createNsmChassis: coGetCachedBaseProperties failure -> co_return NSM_SW_ERROR
// at lines 197-200. Triggered by not registering the base
// interface property map, so MockDbusAsync::findPropertyMap returns nullptr ->
// NSM_SW_ERROR.
TEST_F(NsmNVSwitchChassisTest, CreateNsmChassis_BasePropertiesFail)
{
    // Use a unique path; do NOT register the base interface property map.
    const std::string missingPath = "/xyz/test/nvswitch/basefail_unique";
    const std::string attrIntf =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.ChassisAttributes";

    // Only set the attribute interface - NOT the base ("NSM_NVSwitch_Chassis")
    auto& attrMap = utils::MockDbusAsync::propertyMap(missingPath, attrIntf);
    attrMap["Type"] = std::string("NSM_Chassis_Attributes");

    // createNsmChassis looks for baseInterface =
    //   "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis"
    // which has no property map -> coGetCachedBaseProperties returns
    // NSM_SW_ERROR -> co_return rc immediately (no sensor created, no
    // exception).
    createNsmChassis(mockManager, attrIntf, missingPath,
                     "NSM_NVSwitch_Chassis");
    // If we reach here, the early-return path executed correctly.
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmChassis
// =============================================================================

static const std::string kNvSwBaseType = "NSM_NVSwitch_Chassis";
static const std::string kNvSwBaseIntf =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";
static const std::string kNvSwAttrIntf =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.ChassisAttributes";
// NVSwitch: NSM_DEV_ID_SWITCH=1, role=0 -> n1=1, instance=9
static const uuid_t kNvSwUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:9";

struct NsmNVSwitchChassisFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvsw;

    NsmNVSwitchChassisFactoryTest() : SensorManagerTest(devices)
    {
        nvsw = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kNvSwUuid));
        EXPECT_NE(nvsw, nullptr);
    }

    ~NsmNVSwitchChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// Type absent -> type="" -> neither type==baseType nor
// type=="NSM_Chassis_Attributes" -> no conditional block runs -> no sensors
// added
TEST_F(NsmNVSwitchChassisFactoryTest, CreateNsmChassis_MissingType_NoSensors)
{
    const std::string objPath = "/xyz/test/nvswitch/factory/no_type";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    baseMap["Name"] = std::string("TestSwitch");
    baseMap["UUID"] = kNvSwUuid;
    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwAttrIntf);
    (void)currMap; // intentionally empty -> type="" -> no conditional block

    const size_t before = nvsw->staticSensors.size();
    createNsmChassis(mockManager, kNvSwAttrIntf, objPath, kNvSwBaseType);
    EXPECT_EQ(before, nvsw->staticSensors.size());
}

// UUID absent from base props -> uuid="" -> getNsmDeviceFromStaticUUID throws
// Only runs in coverage build: in real-coroutine mode the sub-interface
// property map is not registered -> coGetAllDbusProperty suspends forever ->
// coroutine never completes -> EXPECT_THROW_COROUTINE sees no exception.
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmNVSwitchChassisFactoryTest, CreateNsmChassis_MissingUUID_Throws)
{
    const std::string objPath = "/xyz/test/nvswitch/factory/no_uuid";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    baseMap["Name"] = std::string("TestSwitch");
    // "UUID" intentionally omitted -> uuid="" -> parseStaticUuid throws

    EXPECT_THROW_COROUTINE(
        createNsmChassis(mockManager, kNvSwAttrIntf, objPath, kNvSwBaseType),
        std::exception);
}
#endif // COVERAGE_DISABLE_COROUTINES

// Name absent + type==baseType -> NsmNVSwitchAndNicChassis<UuidIntf>("","..")
// -> D-Bus path ends with "/" -> sdbusplus throws
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_MissingName_BaseTypeMatch_Throws)
{
    const std::string objPath = "/xyz/test/nvswitch/factory/no_name_base";
    // When interface == baseInterface both reads use the same property map
    auto& props = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    props["UUID"] = kNvSwUuid;
    props["Type"] = kNvSwBaseType; // type == baseType -> TRUE block
    // "Name" intentionally omitted -> name="" -> path ends with "/" -> throws

    EXPECT_THROW_COROUTINE(
        createNsmChassis(mockManager, kNvSwBaseIntf, objPath, kNvSwBaseType),
        std::exception);
}

// type==baseType path with valid Name+UUID -> UuidIntf sensor created (TRUE
// block)
TEST_F(NsmNVSwitchChassisFactoryTest, CreateNsmChassis_BaseTypeMatch_Success)
{
    const std::string objPath = "/xyz/test/nvswitch/factory/base_type_match";
    // interface == baseInterface -> both reads use the same property map
    auto& props = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    props["Name"] = std::string("SwitchUUID");
    props["UUID"] = kNvSwUuid;
    props["Type"] = kNvSwBaseType; // type == baseType -> enters TRUE block

    const size_t before = nvsw->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kNvSwBaseIntf, objPath, kNvSwBaseType));
    EXPECT_GT(nvsw->staticSensors.size(), before);
}

// type=="NSM_Chassis_Attributes" with all optional props missing:
// count("ChassisType"), count("LocationType"), count("LocationCode"),
// count("PrettyNameForChassis") -> all FALSE -> those helpers not called.
// createChassisAsset/SKU/Health still run -> sensors created.
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_Attributes_AllOptionalMissing)
{
    const std::string objPath = "/xyz/test/nvswitch/factory/attr_no_optional";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    baseMap["Name"] = std::string("Switch_NoOpt");
    baseMap["UUID"] = kNvSwUuid;
    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    // "ChassisType", "LocationType", "LocationCode", "PrettyNameForChassis"
    // intentionally omitted -> all 4 inner FALSE branches taken

    const size_t before = nvsw->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kNvSwAttrIntf, objPath, kNvSwBaseType));
    // createChassisAsset(3) + createChassisSKU(1) + createChassisHealth(1) = 5
    EXPECT_GT(nvsw->staticSensors.size(), before);
}

// type=="NSM_Chassis_Attributes" with all optional props present:
// count("ChassisType"), count("LocationType"), count("LocationCode"),
// count("PrettyNameForChassis") -> all TRUE -> all helpers called.
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_Attributes_AllOptionalPresent)
{
    const std::string objPath = "/xyz/test/nvswitch/factory/attr_all_optional";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    baseMap["Name"] = std::string("Switch_AllOpt");
    baseMap["UUID"] = kNvSwUuid;
    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    currMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component");
    currMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    currMap["LocationCode"] = std::string("U1");
    currMap["PrettyNameForChassis"] = std::string("NVSwitch_Pretty");

    const size_t before = nvsw->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kNvSwAttrIntf, objPath, kNvSwBaseType));
    // Asset(3)+SKU(1)+Health(1)+ChassisType(1)+LocationType(1)
    // +LocationCode(1)+PrettyName(1) = 9
    EXPECT_GT(nvsw->staticSensors.size(), before);
}

// =============================================================================
// TRUE-branch coverage: device->getDeviceType()==NSM_DEV_ID_PCIE_BRIDGE &&
//                       device->getDeviceRole()==NSM_PCIE_BRIDGE_DEV_ROLE_CX8
// (line 264-265 in nsmNVSwitchAndNVMgmtNICChassis.cpp)
// NSM_DEV_ID_PCIE_BRIDGE=2, NSM_PCIE_BRIDGE_DEV_ROLE_CX8=2
// combined = (role<<8)|type = (2<<8)|2 = 514
// UUID = "STATIC:514:0::"
// =============================================================================

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_PcieBridgeCX8Device_CreatesAssetTagAndVersion)
{
    // PCIE_BRIDGE (type=2) + CX8 role (role=2): combined = (2<<8)|2 = 514
    // UUID format: STATIC:{combined}:{instance}:{propName}:{propValue}
    static const uuid_t kPcieBridgeCx8Uuid =
        "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath = "/xyz/test/nvswitch/factory/pcie_cx8";

    auto cx8dev = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(kPcieBridgeCx8Uuid));
    ASSERT_NE(cx8dev, nullptr);

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    baseMap["Name"] = std::string("PcieBridgeCX8");
    baseMap["UUID"] = kPcieBridgeCx8Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    // No optional properties -> all count() checks FALSE
    // But device type==PCIE_BRIDGE && role==CX8 -> L264-265 TRUE branch
    // -> createAssetTag + createChassisVersion called

    const size_t before = cx8dev->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kNvSwAttrIntf, objPath, kNvSwBaseType));
    // createAssetTag adds 1 + createChassisVersion adds 1 = 2 static sensors
    EXPECT_GT(cx8dev->staticSensors.size(), before);
}

// CX9 device role: exercises the second operand of the || condition
// (device->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX9)
// NSM_PCIE_BRIDGE_DEV_ROLE_CX9=3, NSM_DEV_ID_PCIE_BRIDGE=2
// combined = (3<<8)|2 = 770
TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_PcieBridgeCX9Device_CreatesAssetTagAndVersion)
{
    // PCIE_BRIDGE (type=2) + CX9 role (role=3): combined = (3<<8)|2 = 770
    static const uuid_t kPcieBridgeCx9Uuid =
        "STATIC:770:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath = "/xyz/test/nvswitch/factory/pcie_cx9";

    auto cx9dev = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(kPcieBridgeCx9Uuid));
    ASSERT_NE(cx9dev, nullptr);

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwBaseIntf);
    baseMap["Name"] = std::string("PcieBridgeCX9");
    baseMap["UUID"] = kPcieBridgeCx9Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kNvSwAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = cx9dev->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kNvSwAttrIntf, objPath, kNvSwBaseType));
    // CX9 also triggers createAssetTag + createChassisVersion -> sensors added
    EXPECT_GT(cx9dev->staticSensors.size(), before);
}
