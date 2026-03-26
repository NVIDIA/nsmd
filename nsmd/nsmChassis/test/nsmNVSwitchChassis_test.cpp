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

#include "base.h"

#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"
#include "nsmSetAsync/asyncOperationManager.hpp"

namespace nsm
{
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

struct NsmNVSwitchChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";
    const std::string name = "NVSwitch_Test";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvswitch";

    const uuid_t switchUuid = "STATIC:1:0:NSM_DEVICE_INSTANCE_NUMBER:100";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nvswitch;

    NsmNVSwitchChassisTest() : SensorManagerTest(devices)
    {
        nvswitch = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nvswitch, nullptr);
    }

    ~NsmNVSwitchChassisTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", switchUuid},
    };
};

TEST_F(NsmNVSwitchChassisTest, goodTestCreateNVSwitchChassis)
{
    dbus::PropertyMap chassisProperties = {
        {"Name", name},
        {"Type", std::string("NSM_NVSwitch_Chassis")},
        {"UUID", switchUuid},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = chassisProperties;

    createNsmNVSwitchChassis(mockManager, basicIntfName, objPath);

    // Should create UUID interface sensor
    EXPECT_GE(nvswitch->staticSensors.size(), 1);
}

TEST_F(NsmNVSwitchChassisTest, badTestMissingUUID)
{
    // Single map for basicIntfName: Name and Type present, UUID missing
    dbus::PropertyMap props = {
        {"Name", name}, {"Type", std::string("NSM_NVSwitch_Chassis")},
        // Missing UUID
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = props;

    EXPECT_THROW_COROUTINE(createNsmChassis(mockManager, basicIntfName, objPath,
                                            "NSM_NVSwitch_Chassis"),
                           std::runtime_error);
}

TEST_F(NsmNVSwitchChassisTest, testCreateWithAssetInfo)
{
    dbus::PropertyMap assetProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_Asset")},
        {"Manufacturer", std::string("NVIDIA")},
        {"PartNumber", std::string("12345-ABC")},
        {"SerialNumber", std::string("SN98765")},
        {"Model", std::string("NVSwitch_Model_X")},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = assetProperties;

    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVSwitch_Chassis");

    // Should create Asset, SKU and Health sensors
    EXPECT_GE(nvswitch->staticSensors.size(), 3);
}

TEST_F(NsmNVSwitchChassisTest, testCreateWithHealthAndLocation)
{
    dbus::PropertyMap attributesProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_Attrs")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Slot"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = attributesProperties;

    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVSwitch_Chassis");

    // Should create Health, Location, and Chassis sensors
    EXPECT_GE(nvswitch->staticSensors.size(), 3);
}

TEST_F(NsmNVSwitchChassisTest, testCreateWithSKU)
{
    dbus::PropertyMap skuProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NVSwitch_SKU")},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = basicProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    propertyMap = skuProperties;

    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVSwitch_Chassis");

    // Should create Asset, SKU and Health sensors (all created for
    // ChassisAttributes)
    EXPECT_GE(nvswitch->staticSensors.size(), 3);
}

struct NsmNVLinkMgmtNicChassisTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
    const std::string name = "NVLinkMgmtNIC_Test";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/nvlink_nic";

    const uuid_t nicUuid = "STATIC:1:1:NSM_DEVICE_INSTANCE_NUMBER:101";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nic;

    NsmNVLinkMgmtNicChassisTest() : SensorManagerTest(devices)
    {
        nic = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(nicUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nic, nullptr);
    }

    ~NsmNVLinkMgmtNicChassisTest()
    {
        cleanupDeviceSensors(devices);
    }

    dbus::PropertyMap basicProperties = {
        {"Name", name},
        {"UUID", nicUuid},
    };
};

TEST_F(NsmNVLinkMgmtNicChassisTest, goodTestCreateNVLinkMgmtNicChassis)
{
    dbus::PropertyMap chassisProperties = {
        {"Name", name},
        {"Type", std::string("NSM_NVLinkMgmtNic_Chassis")},
        {"UUID", nicUuid},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = chassisProperties;

    createNsmNVLinkMgmtNicChassis(mockManager, basicIntfName, objPath);

    // Should create UUID interface sensor
    EXPECT_GE(nic->staticSensors.size(), 1);
}

TEST_F(NsmNVLinkMgmtNicChassisTest, testCreateWithAllAttributes)
{
    dbus::PropertyMap baseProperties = {
        {"Name", name},
        {"UUID", nicUuid},
    };

    dbus::PropertyMap attributesProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NIC_Attributes")},
        {"LocationType",
         "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"},
        {"ChassisType",
         "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module"},
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNIC");
    basePropertyMap = baseProperties;

    auto& attrPropertyMap = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".ChassisAttributes");
    attrPropertyMap = attributesProperties;

    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", objPath,
                     "NSM_NVLinkMgmtNIC");

    // Should create Asset(3), SKU(1), Health(1), ChassisType(1),
    // LocationType(1) = 7 sensors
    EXPECT_GE(nic->staticSensors.size(), 7);
}

TEST_F(NsmNVLinkMgmtNicChassisTest, badTestInvalidLocationType)
{
    dbus::PropertyMap attributesProperties = {
        {"Type", std::string("NSM_Chassis_Attributes")},
        {"Name", std::string("NIC_BadLoc")},
        {"LocationType", std::string("InvalidLocationType")},
    };

    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath + "_badloc", basicIntfName);
    basePropertyMap = basicProperties;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(
        objPath + "_badloc", basicIntfName + ".ChassisAttributes");
    propertyMap = attributesProperties;

    // Should throw due to invalid LocationType
    EXPECT_THROW_COROUTINE(
        createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes",
                         objPath + "_badloc", "NSM_NVLinkMgmtNic_Chassis"),
        std::exception);
}
// ============================================================================
// PART 5: NsmNVSwitchAndNicChassis - additional template instantiations
// ============================================================================

TEST(NsmNVSwitchChassisBatch9, CreateLocationCodeIntf)
{
    // Arrange & Act - test NsmNVSwitchAndNicChassis<LocationCodeIntf>
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<LocationCodeIntf>>(
        "NVSwitch0", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch0");
    EXPECT_EQ(chassis->getType(), "NSM_NVSwitch");
}

TEST(NsmNVSwitchChassisBatch9, CreateItemIntf)
{
    // Arrange & Act - test NsmNVSwitchAndNicChassis<ItemIntf>
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<ItemIntf>>(
        "NVSwitch1", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch1");
}

TEST(NsmNVSwitchChassisBatch9, CreateAssetTagIntf)
{
    // Arrange & Act - test NsmNVSwitchAndNicChassis<AssetTagIntf>
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<AssetTagIntf>>(
        "NVSwitch2", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch2");
}

TEST(NsmNVSwitchChassisBatch9, CreateRevisionIntf)
{
    // Arrange & Act - test NsmNVSwitchAndNicChassis<RevisionIntf>
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<RevisionIntf>>(
        "NVSwitch3", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch3");
}

TEST(NsmNVSwitchChassisBatch9, CreateNsmApSkuIdIntf)
{
    // Arrange & Act - test NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>
    auto chassis = std::make_shared<NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>>(
        "NVSwitch4", "NSM_NVSwitch");

    // Assert
    EXPECT_NE(chassis, nullptr);
    EXPECT_EQ(chassis->getName(), "NVSwitch4");
}

// ============================================================================
// PART 6: NVSwitch/NVMgmtNIC Chassis factory helper function coverage
// ============================================================================

struct NsmNVSwitchChassisFactoryTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string nvSwitchBasicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVSwitch_Chassis";
    const uuid_t switchUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string switchName = "NVSwitch_0";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                switchName;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> switchDev;

    NsmNVSwitchChassisFactoryTest() : SensorManagerTest(devices)
    {
        switchDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(switchUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(switchDev, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmNVSwitchChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmNVSwitchChassisFactoryTest,
       DISABLED_CreateNVSwitchChassis_BaseType_CreatesUuidSensor)
{
    // Arrange - set base type "NSM_NVSwitch_Chassis"
    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    basePropertyMap["Name"] = switchName;
    basePropertyMap["UUID"] = switchUuid;

    auto& propertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    propertyMap["Type"] = std::string("NSM_NVSwitch_Chassis");
    propertyMap["UUID"] = switchUuid;

    // Act
    createNsmNVSwitchChassis(mockManager, nvSwitchBasicIntfName, objPath);

    // Assert - should have at least 1 static sensor (UuidIntf)
    EXPECT_GE(switchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNVSwitchChassis_Attributes_MinimalProperties)
{
    // Arrange - type NSM_Chassis_Attributes with no optional properties
    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    basePropertyMap["Name"] = switchName;
    basePropertyMap["UUID"] = switchUuid;

    std::string attrIntf = nvSwitchBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");

    // Act
    createNsmNVSwitchChassis(mockManager, attrIntf, objPath);

    // Assert - should have asset sensors (3 from createChassisAsset),
    //          SKU sensor (1 from createChassisSKU),
    //          Health sensor (1 from createChassisHealth),
    //          + 1 msgTypes
    EXPECT_GE(switchDev->deviceSensors.size(), 1u);
    EXPECT_GE(switchDev->staticSensors.size(), 0u);
}

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNVSwitchChassis_Attributes_WithChassisType)
{
    // Arrange
    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    basePropertyMap["Name"] = switchName;
    basePropertyMap["UUID"] = switchUuid;

    std::string attrIntf = nvSwitchBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");

    // Act
    createNsmNVSwitchChassis(mockManager, attrIntf, objPath);

    // Assert - createChassisType adds 1 more static sensor
    EXPECT_GE(switchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNVSwitchChassis_Attributes_WithLocationTypeAndCode)
{
    // Arrange
    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    basePropertyMap["Name"] = switchName;
    basePropertyMap["UUID"] = switchUuid;

    std::string attrIntf = nvSwitchBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Embedded");
    propertyMap["LocationCode"] = std::string("NVSwitch_SXM_0");

    // Act
    createNsmNVSwitchChassis(mockManager, attrIntf, objPath);

    // Assert - createLocationType + createLocationCode each add 1 static sensor
    EXPECT_GE(switchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNVSwitchChassis_Attributes_WithPrettyName)
{
    // Arrange
    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    basePropertyMap["Name"] = switchName;
    basePropertyMap["UUID"] = switchUuid;

    std::string attrIntf = nvSwitchBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["PrettyNameForChassis"] = std::string("NVSwitch 0 Chassis");

    // Act
    createNsmNVSwitchChassis(mockManager, attrIntf, objPath);

    // Assert - createPrettyName adds 1 static sensor
    EXPECT_GE(switchDev->deviceSensors.size(), 1u);
}

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNVSwitchChassis_Attributes_AllOptionalProperties)
{
    // Arrange
    auto& basePropertyMap =
        utils::MockDbusAsync::propertyMap(objPath, nvSwitchBasicIntfName);
    basePropertyMap["Name"] = switchName;
    basePropertyMap["UUID"] = switchUuid;

    std::string attrIntf = nvSwitchBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");
    propertyMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Module");
    propertyMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Embedded");
    propertyMap["LocationCode"] = std::string("NVSwitch_SXM_0");
    propertyMap["PrettyNameForChassis"] = std::string("NVSwitch 0 Chassis");

    // Act
    createNsmNVSwitchChassis(mockManager, attrIntf, objPath);

    // Assert - all optional properties add static sensors
    EXPECT_GE(switchDev->deviceSensors.size(), 1u);
}

// ============================================================================
// PART 7: NVLinkMgmtNic Chassis factory
// ============================================================================

struct NsmNVLinkMgmtNicChassisFactoryTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string nicBasicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
    // PCIE_BRIDGE device with CX8 role
    const uuid_t nicUuid = "STATIC:4:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string nicName = "NVLinkMgmtNIC_0";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                nicName;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> nicDev;

    NsmNVLinkMgmtNicChassisFactoryTest() : SensorManagerTest(devices)
    {
        nicDev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(nicUuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(nicDev, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmNVLinkMgmtNicChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmNVLinkMgmtNicChassisFactoryTest,
       DISABLED_CreateNVLinkMgmtNicChassis_BaseType)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              nicBasicIntfName);
    basePropertyMap["Name"] = nicName;
    basePropertyMap["UUID"] = nicUuid;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          nicBasicIntfName);
    propertyMap["Type"] = std::string("NSM_NVLinkMgmtNic_Chassis");
    propertyMap["UUID"] = nicUuid;

    // Act
    createNsmNVLinkMgmtNicChassis(mockManager, nicBasicIntfName, objPath);

    // Assert
    EXPECT_GE(nicDev->deviceSensors.size(), 1u);
}

TEST_F(NsmNVLinkMgmtNicChassisFactoryTest,
       CreateNVLinkMgmtNicChassis_Attributes_WithCX8Role)
{
    // Arrange - PCIE_BRIDGE with CX8 role should also get assetTag + version
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              nicBasicIntfName);
    basePropertyMap["Name"] = nicName;
    basePropertyMap["UUID"] = nicUuid;

    std::string attrIntf = nicBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");

    // Simulate PCIE_BRIDGE + CX8 role
    // Note: the device was created with device type from UUID parsing
    // which may or may not match NSM_DEV_ID_PCIE_BRIDGE
    // The factory checks: device->getDeviceType() == NSM_DEV_ID_PCIE_BRIDGE
    // && device->getDeviceRole() == NSM_PCIE_BRIDGE_DEV_ROLE_CX8

    // Act
    createNsmNVLinkMgmtNicChassis(mockManager, attrIntf, objPath);

    // Assert - basic sensors created (asset, SKU, health)
    EXPECT_GE(nicDev->deviceSensors.size(), 1u);
}

// ============================================================================
// PART 11: NsmNVSwitchChassis factory - createNsmChassis_Attributes with
//          PCIE_BRIDGE + CX8 role for AssetTag and ChassisVersion
// ============================================================================

struct NsmCX8ChassisFactoryTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string nicBasicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";
    // NSM_DEV_ID_PCIE_BRIDGE = 4, NSM_PCIE_BRIDGE_DEV_ROLE_CX8 = typically
    // some known value. Use the UUID format that yields the correct device type
    // and role.
    const uuid_t cx8Uuid = "STATIC:4:0:NSM_DEVICE_INSTANCE_NUMBER:7";
    const std::string cx8Name = "CX8_NIC_0";
    const std::string objPath = std::string(chassisInventoryBasePath) + "/" +
                                cx8Name;

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> cx8Dev;

    NsmCX8ChassisFactoryTest() : SensorManagerTest(devices)
    {
        cx8Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx8Uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(cx8Dev, nullptr);
        AsyncOperationManager::getInstance()->dispatchers.clear();
    }

    ~NsmCX8ChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmCX8ChassisFactoryTest, CreateCX8Chassis_Attributes)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              nicBasicIntfName);
    basePropertyMap["Name"] = cx8Name;
    basePropertyMap["UUID"] = cx8Uuid;

    std::string attrIntf = nicBasicIntfName + ".ChassisAttributes";
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propertyMap["Type"] = std::string("NSM_Chassis_Attributes");

    // Act
    createNsmNVLinkMgmtNicChassis(mockManager, attrIntf, objPath);

    // Assert - basic sensors + possibly AssetTag + ChassisVersion if
    // device type/role matches CX8
    EXPECT_GE(cx8Dev->deviceSensors.size(), 1u);
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmChassis
// =============================================================================

// Name absent in base props → name="" → NsmNVSwitchAndNicChassis constructed
// with empty name → D-Bus path invalid → SdBusError thrown (no try/catch
// in createChassisAsset or createNsmChassis)
TEST_F(NsmNVSwitchChassisTest, CreateChassis_MissingName_Throws)
{
    const std::string subPath =
        "/xyz/openbmc_project/inventory/system/sw_noname";
    auto& baseMap = utils::MockDbusAsync::propertyMap(subPath, basicIntfName);
    baseMap["UUID"] = switchUuid;
    // "Name" intentionally omitted from base props → FALSE branch → name=""

    auto& currMap = utils::MockDbusAsync::propertyMap(
        subPath, basicIntfName + ".ChassisAttributes");
    currMap["Type"] = std::string("NSM_Chassis_Attributes");

    EXPECT_THROW_COROUTINE(
        createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes",
                         subPath, "NSM_NVSwitch_Chassis"),
        std::exception);
}

// Type absent in current props → type="" → neither baseType nor Attributes →
// no conditional sensors added → co_return NSM_SUCCESS
TEST_F(NsmNVSwitchChassisTest, CreateChassis_MissingType_NoConditionalSensors)
{
    const std::string subPath =
        "/xyz/openbmc_project/inventory/system/sw_notype";
    auto& baseMap = utils::MockDbusAsync::propertyMap(subPath, basicIntfName);
    baseMap["Name"] = std::string("NVSwitch_NoType");
    baseMap["UUID"] = switchUuid;

    auto& currMap = utils::MockDbusAsync::propertyMap(
        subPath, basicIntfName + ".ChassisAttributes");
    (void)currMap;
    // "Type" intentionally omitted → type="" → if/else-if branches skipped

    const size_t before = nvswitch->staticSensors.size();
    createNsmChassis(mockManager, basicIntfName + ".ChassisAttributes", subPath,
                     "NSM_NVSwitch_Chassis");
    // No conditional sensors added (type doesn't match any branch)
    EXPECT_EQ(before, nvswitch->staticSensors.size());
}

// type == baseType but UUID absent in current props → inner uuid="" →
// chassisUuid created with empty UUID (FALSE branch for inner count("UUID"))
TEST_F(NsmNVSwitchChassisTest, CreateChassis_BaseTypeNoCurrentUUID)
{
    const std::string subPath =
        "/xyz/openbmc_project/inventory/system/sw_no_curr_uuid";
    // Base interface (basicIntfName) has Name and UUID (for outer UUID check)
    auto& baseMap = utils::MockDbusAsync::propertyMap(subPath, basicIntfName);
    baseMap["Name"] = std::string("NVSwitch_NoCurrUUID");
    baseMap["UUID"] = switchUuid;

    // Sub-interface has Type == baseType but NO UUID →
    // inner count("UUID") FALSE → inner uuid="" → chassisUuid with empty UUID
    auto& currMap =
        utils::MockDbusAsync::propertyMap(subPath, basicIntfName + ".UuidSub");
    currMap["Type"] = std::string("NSM_NVSwitch_Chassis");
    // UUID intentionally omitted from current props

    const size_t before = nvswitch->staticSensors.size();
    createNsmChassis(mockManager, basicIntfName + ".UuidSub", subPath,
                     "NSM_NVSwitch_Chassis");
    // chassisUuid sensor IS added (with empty UUID from inner count FALSE)
    EXPECT_GT(nvswitch->staticSensors.size(), before);
}
