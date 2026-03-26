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
 * Branch coverage for nsmNVSwitchAndNVMgmtNICChassis.cpp
 *
 * Covers:
 * - createChassisAsset: exercises helper directly
 * - createChassisType: with valid ChassisType property
 * - createChassisHealth: exercises helper directly
 * - createLocationType: with/without LocationType property
 * - createLocationCode: with valid LocationCode property
 * - createPrettyName: with valid PrettyNameForChassis property
 * - createAssetTag: exercises helper directly
 * - createChassisVersion: exercises helper directly
 * - createChassisSKU: exercises helper directly
 * - NsmNVSwitchAndNicChassis<UuidIntf>::update() error path
 * - createNsmChassis factory: missing Name/UUID/Type branches
 * - createNsmNVSwitchChassis / createNsmNVLinkMgmtNicChassis wrappers
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

// Forward-declare non-static helpers from nsmNVSwitchAndNVMgmtNICChassis.cpp
namespace nsm
{
void createChassisAsset(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType);
void createChassisType(std::shared_ptr<NsmDevice> device, std::string& name,
                       const std::string& baseType,
                       dbus::PropertyMap& allCurrentIfaceProperties);
void createChassisHealth(std::shared_ptr<NsmDevice> device, std::string& name,
                         const std::string& baseType);
void createLocationType(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType,
                        dbus::PropertyMap& allCurrentIfaceProperties);
void createLocationCode(std::shared_ptr<NsmDevice> device, std::string& name,
                        const std::string& baseType,
                        dbus::PropertyMap& allCurrentIfaceProperties);
void createPrettyName(std::shared_ptr<NsmDevice> device, std::string& name,
                      const std::string& baseType,
                      dbus::PropertyMap& allCurrentIfaceProperties);
void createAssetTag(std::shared_ptr<NsmDevice> device, std::string& name,
                    const std::string& baseType);
void createChassisVersion(std::shared_ptr<NsmDevice> device, std::string& name,
                          const std::string& baseType);
void createChassisSKU(std::shared_ptr<NsmDevice> device, std::string& name,
                      const std::string& baseType);
requester::Coroutine createNsmChassis(SensorManager& manager,
                                      const std::string& interface,
                                      const std::string& objPath,
                                      const std::string baseType);
requester::Coroutine createNsmNVSwitchChassis(SensorManager& manager,
                                              const std::string& interface,
                                              const std::string& objPath);
requester::Coroutine createNsmNVLinkMgmtNicChassis(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Direct helper function tests
// ============================================================================

// NVSwitch: NSM_DEV_ID_SWITCH=1, role=0 -> combined=1, instance=0
static const uuid_t kDevUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:0";
static const std::string kBaseType = "NSM_NVSwitch_Chassis";
static const std::string kBaseIntf =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";
static const std::string kAttrIntf =
    "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis.ChassisAttributes";

struct NsmNVSwitchChassisBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> dev;
    std::string switchName = "NVSwitch_B2";

    NsmNVSwitchChassisBranch2Test() : SensorManagerTest(devices)
    {
        dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(kDevUuid));
        EXPECT_NE(dev, nullptr);
    }

    ~NsmNVSwitchChassisBranch2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// createChassisAsset: exercises the helper directly
// Adds 3 static sensors (partNumber, serialNumber, model)
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateChassisAsset_DirectCall)
{
    const size_t before = dev->staticSensors.size();
    createChassisAsset(dev, switchName, kBaseType);
    // partNumber + serialNumber + model = 3 sensors
    EXPECT_EQ(dev->staticSensors.size(), before + 3);
}

// ============================================================================
// createChassisType: valid ChassisType property (TRUE branch)
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateChassisType_ValidProperty)
{
    dbus::PropertyMap props;
    props["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");

    const size_t before = dev->staticSensors.size();
    createChassisType(dev, switchName, kBaseType, props);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createChassisHealth: exercises helper directly
// Adds 1 static sensor
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateChassisHealth_DirectCall)
{
    const size_t before = dev->staticSensors.size();
    createChassisHealth(dev, switchName, kBaseType);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createLocationType: with LocationType property present (TRUE branch)
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateLocationType_WithProperty)
{
    dbus::PropertyMap props;
    props["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");

    const size_t before = dev->staticSensors.size();
    createLocationType(dev, switchName, kBaseType, props);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createLocationType: LocationType absent from props (FALSE branch at L115)
// The helper itself always creates a sensor; the count() check is in the
// factory. Here we exercise the helper with an empty LocationType value.
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateLocationType_EmptyValue)
{
    dbus::PropertyMap props;
    // LocationType present but empty -> convertLocationTypesFromString("")
    // will throw due to invalid enum string
    props["LocationType"] = std::string("");
    EXPECT_THROW(createLocationType(dev, switchName, kBaseType, props),
                 std::exception);
}

// ============================================================================
// createLocationCode: with valid LocationCode property
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateLocationCode_ValidProperty)
{
    dbus::PropertyMap props;
    props["LocationCode"] = std::string("U1-S0");

    const size_t before = dev->staticSensors.size();
    createLocationCode(dev, switchName, kBaseType, props);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createPrettyName: with valid PrettyNameForChassis property
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreatePrettyName_ValidProperty)
{
    dbus::PropertyMap props;
    props["PrettyNameForChassis"] = std::string("NVSwitch Chassis 0");

    const size_t before = dev->staticSensors.size();
    createPrettyName(dev, switchName, kBaseType, props);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createAssetTag: exercises helper directly
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateAssetTag_DirectCall)
{
    const size_t before = dev->staticSensors.size();
    createAssetTag(dev, switchName, kBaseType);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createChassisVersion: exercises helper directly
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateChassisVersion_DirectCall)
{
    const size_t before = dev->staticSensors.size();
    createChassisVersion(dev, switchName, kBaseType);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// createChassisSKU: exercises helper directly
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateChassisSKU_DirectCall)
{
    const size_t before = dev->staticSensors.size();
    createChassisSKU(dev, switchName, kBaseType);
    EXPECT_EQ(dev->staticSensors.size(), before + 1);
}

// ============================================================================
// NsmNVSwitchAndNicChassis<UuidIntf>::update() error path
// MctpDiscovery::getInstance() and getDeviceUUID call sequence;
// in coverage build the coroutine runs synchronously and the UUID will
// not be set -> rc != NSM_SW_SUCCESS -> co_return NSM_ERROR path taken. //

// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Update_UuidIntf_ErrorPath)
{
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<UuidIntf>>(
        switchName, kBaseType);
    ASSERT_NE(chassis, nullptr);

    // update() for UuidIntf calls MctpDiscovery::getInstance() which is not
    // initialized in test environment -> throws
    EXPECT_THROW_COROUTINE(chassis->update(dev), std::exception);
}

// ============================================================================
// Factory: createNsmChassis with base properties fail (no props registered)
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_BasePropertiesFail)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/base_fail";
    // Do NOT register base interface property map
    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    // coGetCachedBaseProperties returns NSM_SW_ERROR -> early return
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    // No crash, no sensors added
}

// ============================================================================
// Factory: missing Name in base props -> name="" -> createChassisAsset
// constructs NsmNVSwitchAndNicChassis with empty name -> D-Bus path invalid
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_MissingName_Throws)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/no_name";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["UUID"] = kDevUuid;
    // "Name" intentionally omitted

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    EXPECT_THROW_COROUTINE(
        createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType),
        std::exception);
}

// ============================================================================
// Factory: missing UUID in base props -> uuid="" -> parseStaticUuid throws
// ============================================================================
#ifdef COVERAGE_DISABLE_COROUTINES
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_MissingUUID_Throws)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/no_uuid";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("NoUUID_Switch");
    // "UUID" intentionally omitted

    EXPECT_THROW_COROUTINE(
        createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType),
        std::exception);
}
#endif

// ============================================================================
// Factory: missing Type in current props -> type="" -> no conditional block
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_MissingType_NoSensors)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/no_type";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("NoType_Switch");
    baseMap["UUID"] = kDevUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    (void)currMap; // intentionally empty

    const size_t before = dev->staticSensors.size();
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    EXPECT_EQ(before, dev->staticSensors.size());
}

// ============================================================================
// Factory: type == baseType with valid Name+UUID -> UuidIntf sensor created
// Inner UUID present in current props (TRUE branch for inner count("UUID"))
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_BaseTypeMatch_WithInnerUUID)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/base_type_with_uuid";
    auto& props = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    props["Name"] = std::string("Switch_InnerUUID");
    props["UUID"] = kDevUuid;
    props["Type"] = kBaseType;

    const size_t before = dev->staticSensors.size();
    createNsmChassis(mockManager, kBaseIntf, objPath, kBaseType);
    EXPECT_GT(dev->staticSensors.size(), before);
}

// ============================================================================
// Factory: type == baseType without inner UUID (FALSE for inner count("UUID"))
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_BaseTypeMatch_WithoutInnerUUID)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/base_type_no_inner_uuid";
    // Base interface has Name + UUID + Type
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("Switch_NoInnerUUID");
    baseMap["UUID"] = kDevUuid;

    // Sub-interface has Type == baseType but no UUID
    auto& currMap = utils::MockDbusAsync::propertyMap(objPath,
                                                      kBaseIntf + ".Sub");
    currMap["Type"] = kBaseType;
    // "UUID" intentionally omitted from current props

    const size_t before = dev->staticSensors.size();
    createNsmChassis(mockManager, kBaseIntf + ".Sub", objPath, kBaseType);
    EXPECT_GT(dev->staticSensors.size(), before);
}

// ============================================================================
// Factory: type == "NSM_Chassis_Attributes" with all optional props present
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_Attributes_AllPresent)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/attr_all";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("Switch_AllOpt");
    baseMap["UUID"] = kDevUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    currMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component");
    currMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes."
        "Embedded");
    currMap["LocationCode"] = std::string("U1");
    currMap["PrettyNameForChassis"] = std::string("Pretty_Switch");

    const size_t before = dev->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType));
    // Asset(3)+SKU(1)+Health(1)+ChassisType(1)+LocationType(1)
    // +LocationCode(1)+PrettyName(1) = 9
    EXPECT_GE(dev->staticSensors.size(), before + 9);
}

// ============================================================================
// Factory: type == "NSM_Chassis_Attributes" with all optional props absent
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_Attributes_NonePresent)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/attr_none";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("Switch_NoOpt");
    baseMap["UUID"] = kDevUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");
    // All optional props omitted

    const size_t before = dev->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType));
    // Asset(3)+SKU(1)+Health(1) = 5
    EXPECT_EQ(dev->staticSensors.size(), before + 5);
}

// ============================================================================
// Factory: PCIE_BRIDGE + CX8 role -> createAssetTag + createChassisVersion
// combined = (2<<8)|2 = 514
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_PcieBridgeCX8_AssetTagAndVersion)
{
    static const uuid_t kCx8Uuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath = "/xyz/test/nvswitch_b2/pcie_cx8";

    auto cx8dev = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(kCx8Uuid));
    ASSERT_NE(cx8dev, nullptr);
    EXPECT_EQ(NSM_DEV_ID_PCIE_BRIDGE, cx8dev->getDeviceType());
    EXPECT_EQ(NSM_PCIE_BRIDGE_DEV_ROLE_CX8, cx8dev->getDeviceRole());

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("PcieBridge_CX8");
    baseMap["UUID"] = kCx8Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = cx8dev->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType));
    // Asset(3)+SKU(1)+Health(1)+AssetTag(1)+Version(1) = 7
    EXPECT_GE(cx8dev->staticSensors.size(), before + 7);
}

// ============================================================================
// Factory: PCIE_BRIDGE + CX9 role -> createAssetTag + createChassisVersion
// combined = (3<<8)|2 = 770
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_PcieBridgeCX9_AssetTagAndVersion)
{
    static const uuid_t kCx9Uuid = "STATIC:770:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath = "/xyz/test/nvswitch_b2/pcie_cx9";

    auto cx9dev = std::dynamic_pointer_cast<MockNsmDevice>(
        mockManager.getNsmDeviceFromStaticUUID(kCx9Uuid));
    ASSERT_NE(cx9dev, nullptr);

    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("PcieBridge_CX9");
    baseMap["UUID"] = kCx9Uuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = cx9dev->staticSensors.size();
    EXPECT_NO_THROW(
        createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType));
    EXPECT_GE(cx9dev->staticSensors.size(), before + 7);
}

// ============================================================================
// Factory: non-PCIE_BRIDGE device -> AssetTag and Version NOT created
// (FALSE branch at L264 in nsmNVSwitchAndNVMgmtNICChassis.cpp)
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, Factory_NonPcieBridge_NoAssetTagOrVersion)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/non_pcie";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    baseMap["Name"] = std::string("NonPcie_Switch");
    baseMap["UUID"] = kDevUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(objPath, kAttrIntf);
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    const size_t before = dev->staticSensors.size();
    createNsmChassis(mockManager, kAttrIntf, objPath, kBaseType);
    // Asset(3)+SKU(1)+Health(1) = 5, NO AssetTag or Version
    EXPECT_EQ(dev->staticSensors.size(), before + 5);
}

// ============================================================================
// createNsmNVSwitchChassis wrapper: calls createNsmChassis with correct type
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateNsmNVSwitchChassis_Wrapper)
{
    const std::string objPath = "/xyz/test/nvswitch_b2/wrapper_sw";
    auto& props = utils::MockDbusAsync::propertyMap(objPath, kBaseIntf);
    props["Name"] = std::string("WrapperSwitch");
    props["UUID"] = kDevUuid;
    props["Type"] = kBaseType;

    const size_t before = dev->staticSensors.size();
    createNsmNVSwitchChassis(mockManager, kBaseIntf, objPath);
    EXPECT_GT(dev->staticSensors.size(), before);
}

// ============================================================================
// createNsmNVLinkMgmtNicChassis wrapper
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, CreateNsmNVLinkMgmtNicChassis_Wrapper)
{
    static const std::string nicBaseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
    const std::string objPath = "/xyz/test/nvswitch_b2/wrapper_nic";
    auto& props = utils::MockDbusAsync::propertyMap(objPath, nicBaseIntf);
    props["Name"] = std::string("WrapperNic");
    props["UUID"] = kDevUuid;
    props["Type"] = std::string("NSM_NVLinkMgmtNic_Chassis");

    const size_t before = dev->staticSensors.size();
    createNsmNVLinkMgmtNicChassis(mockManager, nicBaseIntf, objPath);
    EXPECT_GT(dev->staticSensors.size(), before);
}

// ============================================================================
// NsmInventoryProperty<NsmAssetIntf> genRequestMsg/handleResponseMsg
// via sensor created by createChassisAsset (DEVICE_PART_NUMBER)
// ============================================================================
TEST_F(NsmNVSwitchChassisBranch2Test, InventoryProperty_GenRequestMsg_Success)
{
    createChassisAsset(dev, switchName, kBaseType);
    ASSERT_GE(dev->staticSensors.size(), 1u);

    // First static sensor is partNumber (DEVICE_PART_NUMBER)
    auto sensor = std::dynamic_pointer_cast<NsmSensor>(dev->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    auto request = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(request.has_value());
}

TEST_F(NsmNVSwitchChassisBranch2Test,
       InventoryProperty_GenRequestMsg_EncodeFail)
{
    createChassisAsset(dev, switchName, kBaseType);
    ASSERT_GE(dev->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(dev->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);
    auto request = sensor->genRequestMsg(0, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST_F(NsmNVSwitchChassisBranch2Test,
       InventoryProperty_HandleResponseMsg_Success)
{
    createChassisAsset(dev, switchName, kBaseType);
    ASSERT_GE(dev->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(dev->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Build a valid get_inventory_information response
    const std::string partNum = "900-12345-0001-000";
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_resp) +
            partNum.size(),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_inventory_information_resp(
        0, NSM_SUCCESS, ERR_NULL, partNum.size(),
        reinterpret_cast<const uint8_t*>(partNum.data()), responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor->handleResponseMsg(responseMsg, responseData.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST_F(NsmNVSwitchChassisBranch2Test,
       InventoryProperty_HandleResponseMsg_ErrorCC)
{
    createChassisAsset(dev, switchName, kBaseType);
    ASSERT_GE(dev->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(dev->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Build error CC response (minimum 7 bytes to avoid valgrind issues)
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    responseMsg->payload[1] = NSM_ERROR;

    auto rc = sensor->handleResponseMsg(responseMsg, response.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmNVSwitchChassisBranch2Test,
       InventoryProperty_HandleResponseMsg_DecodeFail)
{
    createChassisAsset(dev, switchName, kBaseType);
    ASSERT_GE(dev->staticSensors.size(), 1u);

    auto sensor = std::dynamic_pointer_cast<NsmSensor>(dev->staticSensors[0]);
    ASSERT_NE(sensor, nullptr);

    // Short buffer -> decode fail
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    // payload[1] = 0 (NSM_SUCCESS) but buffer too short for full response

    auto rc = sensor->handleResponseMsg(responseMsg, sizeof(nsm_msg_hdr));
    EXPECT_NE(rc, NSM_SUCCESS);
}
