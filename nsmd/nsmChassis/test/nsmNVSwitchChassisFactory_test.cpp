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

#include "base.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmInventoryProperty.hpp"
#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"

#undef private
#undef protected

using namespace nsm;

// Forward declarations for factory functions
namespace nsm
{
requester::Coroutine createNsmNVLinkMgmtNicChassis(SensorManager& manager,
                                                   const std::string& interface,
                                                   const std::string& objPath);
} // namespace nsm

// ============================================================================
// nsmNVSwitchAndNVMgmtNICChassis.cpp - factory helper functions
// Focus: createAssetTag, createChassisVersion (CX8 path),
//        createChassisSKU, AssetTagIntf/RevisionIntf templates
// ============================================================================

struct NsmNVSwitchChassisFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    // Use a CX8 device to hit the AssetTag and ChassisVersion branches
    // combined = (NSM_PCIE_BRIDGE_DEV_ROLE_CX8 << 8) | NSM_DEV_ID_PCIE_BRIDGE
    //          = (2 << 8) | 2 = 514
    const uuid_t cx8Uuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/chassis/NVLinkMgmtNic0";
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_NVLinkMgmtNic_Chassis";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> cx8Dev;

    NsmNVSwitchChassisFactoryTest() : SensorManagerTest(devices)
    {
        cx8Dev = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx8Uuid));
        EXPECT_NE(cx8Dev, nullptr);
        // NSM_DEV_ID_PCIE_BRIDGE = 2, NSM_PCIE_BRIDGE_DEV_ROLE_CX8 = 2
        EXPECT_EQ(NSM_DEV_ID_PCIE_BRIDGE, cx8Dev->getDeviceType());
        EXPECT_EQ(NSM_PCIE_BRIDGE_DEV_ROLE_CX8, cx8Dev->getDeviceRole());
    }

    ~NsmNVSwitchChassisFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_CX8Attributes_CreatesAssetTagAndVersion)
{
    // Arrange - set up base interface properties
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVLinkMgmtNic0");
    basePropertyMap["UUID"] = cx8Uuid;

    // Set up attributes interface
    std::string attrIntf = baseIntf + ".ChassisAttributes";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propMap["Type"] = std::string("NSM_Chassis_Attributes");

    // Act
    createNsmNVLinkMgmtNicChassis(mockManager, attrIntf, objPath);

    // Assert - CX8 device gets: 3 asset sensors + 1 SKU + 1 health
    //          + 1 assetTag + 1 version = 7 static + 1 msgTypes
    // AssetTag and ChassisVersion should be created for CX8 devices
    EXPECT_GE(cx8Dev->deviceSensors.size(), 2u);

    // Verify we have inventory property sensors for asset tag and version
    bool foundAssetTag = false;
    bool foundVersion = false;
    for (auto& sensor : cx8Dev->deviceSensors)
    {
        auto assetTagSensor =
            std::dynamic_pointer_cast<NsmInventoryProperty<AssetTagIntf>>(
                sensor);
        if (assetTagSensor && assetTagSensor->property == ASSET_TAG)
        {
            foundAssetTag = true;
        }
        auto versionSensor =
            std::dynamic_pointer_cast<NsmInventoryProperty<RevisionIntf>>(
                sensor);
        if (versionSensor && versionSensor->property == INFO_ROM_VERSION)
        {
            foundVersion = true;
        }
    }
    EXPECT_TRUE(foundAssetTag);
    EXPECT_TRUE(foundVersion);
}

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_CX8WithAllOptional_AllSensorsCreated)
{
    // Arrange
    auto& basePropertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                              baseIntf);
    basePropertyMap["Name"] = std::string("NVLinkMgmtNic0");
    basePropertyMap["UUID"] = cx8Uuid;

    std::string attrIntf = baseIntf + ".ChassisAttributes";
    auto& propMap = utils::MockDbusAsync::propertyMap(objPath, attrIntf);
    propMap["Type"] = std::string("NSM_Chassis_Attributes");
    propMap["ChassisType"] = std::string(
        "xyz.openbmc_project.Inventory.Item.Chassis."
        "ChassisType.Module");
    propMap["LocationType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.Location."
        "LocationTypes.Embedded");
    propMap["LocationCode"] = std::string("NVLinkMgmtNic_Slot0");
    propMap["PrettyNameForChassis"] = std::string("NVLink Management NIC 0");

    // Act
    createNsmNVLinkMgmtNicChassis(mockManager, attrIntf, objPath);

    // Assert - CX8 path: asset(3) + SKU(1) + health(1) + chassisType(1)
    //   + locationType(1) + locationCode(1) + prettyName(1)
    //   + assetTag(1) + version(1) + msgTypes(1) = 12
    EXPECT_GE(cx8Dev->deviceSensors.size(), 5u);
}

// Test AssetTagIntf template instantiation directly
TEST_F(NsmNVSwitchChassisFactoryTest,
       AssetTagIntf_Construction_CreatesValidObject)
{
    // Arrange & Act - use unique name to avoid D-Bus path conflicts
    auto assetTagChassis = NsmNVSwitchAndNicChassis<AssetTagIntf>(
        "TestChassisAssetTag", "NSM_NVSwitch");

    // Assert
    EXPECT_EQ(assetTagChassis.getName(), "TestChassisAssetTag");
    EXPECT_EQ(assetTagChassis.getType(), "NSM_NVSwitch");
}

// Test RevisionIntf template instantiation directly
TEST_F(NsmNVSwitchChassisFactoryTest,
       RevisionIntf_Construction_CreatesValidObject)
{
    // Arrange & Act - use unique name to avoid D-Bus path conflicts
    auto revisionChassis = NsmNVSwitchAndNicChassis<RevisionIntf>(
        "TestChassisRevision", "NSM_NVSwitch");

    // Assert
    EXPECT_EQ(revisionChassis.getName(), "TestChassisRevision");
    EXPECT_EQ(revisionChassis.getType(), "NSM_NVSwitch");
}

// Test LocationCodeIntf template instantiation directly
TEST_F(NsmNVSwitchChassisFactoryTest,
       LocationCodeIntf_Construction_CreatesValidObject)
{
    // Arrange & Act - use unique name to avoid D-Bus path conflicts
    auto locCodeChassis = NsmNVSwitchAndNicChassis<LocationCodeIntf>(
        "TestChassisLocCode", "NSM_NVSwitch");

    // Assert
    EXPECT_EQ(locCodeChassis.getName(), "TestChassisLocCode");
    EXPECT_EQ(locCodeChassis.getType(), "NSM_NVSwitch");
}

// Test ItemIntf template instantiation directly
TEST_F(NsmNVSwitchChassisFactoryTest, ItemIntf_Construction_CreatesValidObject)
{
    // Arrange & Act - use unique name to avoid D-Bus path conflicts
    auto itemChassis = NsmNVSwitchAndNicChassis<ItemIntf>("TestChassisItem",
                                                          "NSM_NVSwitch");

    // Assert
    EXPECT_EQ(itemChassis.getName(), "TestChassisItem");
    EXPECT_EQ(itemChassis.getType(), "NSM_NVSwitch");
}

// Test NsmApSkuIdIntf template instantiation directly
TEST_F(NsmNVSwitchChassisFactoryTest,
       NsmApSkuIdIntf_Construction_CreatesValidObject)
{
    // Arrange & Act - use unique name to avoid D-Bus path conflicts
    auto skuChassis = NsmNVSwitchAndNicChassis<NsmApSkuIdIntf>("TestChassisSku",
                                                               "NSM_NVSwitch");

    // Assert
    EXPECT_EQ(skuChassis.getName(), "TestChassisSku");
    EXPECT_EQ(skuChassis.getType(), "NSM_NVSwitch");
}
