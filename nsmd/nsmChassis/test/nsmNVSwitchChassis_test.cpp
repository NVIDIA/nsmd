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
    dbus::PropertyMap chassisProperties = {
        {"Type", std::string("NSM_NVSwitch_Chassis")},
        // Missing UUID
    };

    dbus::PropertyMap baseProps = {
        {"Name", name},
        // Missing UUID
    };

    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              basicIntfName);
    basePropertyMap = baseProps;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = chassisProperties;

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
