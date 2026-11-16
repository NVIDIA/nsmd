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

#include "features/creators.hpp"
#include "features/feature.hpp"
#include "nsmInventoryProperty.hpp"
#include "nsmNVSwitchAndNVMgmtNICChassis.hpp"

#undef private
#undef protected

using namespace nsm;

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
    const std::string chassisName = "NVLinkMgmtNic0";

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

    /** @brief Run the NVLinkMgmtNic chassis attributes feature on the CX8 */
    requester::Coroutine createAttributes(dbus::PropertyMap params)
    {
        nsm::features::DeviceContext dev;
        dev.name = chassisName;
        dev.kind = "cx_nic";
        dev.staticUuid = cx8Uuid;
        dev.platformPrefix = "HGX_";
        dev.device = cx8Dev;
        params["Name"] = chassisName;
        return nsm::features::createNVLinkMgmtNicChassisAttributesFeature(
            mockManager, dev, params);
    }
};

TEST_F(NsmNVSwitchChassisFactoryTest,
       CreateNsmChassis_CX8Attributes_CreatesAssetTagAndVersion)
{
    // Act
    const size_t before = cx8Dev->staticSensors.size();
    auto rc = createAttributes({});
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);

    // Assert - CX8 device gets: 3 asset sensors + 1 SKU + 1 health
    //          + 1 assetTag + 1 version = 7 static
    // AssetTag and ChassisVersion should be created for CX8 devices
    EXPECT_EQ(cx8Dev->staticSensors.size(), before + 7);

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
    // Act
    const size_t before = cx8Dev->staticSensors.size();
    auto rc = createAttributes(
        {{"ChassisType", std::string("xyz.openbmc_project.Inventory.Item."
                                     "Chassis.ChassisType.Module")},
         {"LocationType", std::string("xyz.openbmc_project.Inventory."
                                      "Decorator.Location.LocationTypes."
                                      "Embedded")},
         {"LocationCode", std::string("NVLinkMgmtNic_Slot0")},
         {"PrettyNameForChassis", std::string("NVLink Management NIC 0")}});
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);

    // Assert - CX8 path: asset(3) + SKU(1) + health(1) + chassisType(1)
    //   + locationType(1) + locationCode(1) + prettyName(1)
    //   + assetTag(1) + version(1) = 11 static
    EXPECT_EQ(cx8Dev->staticSensors.size(), before + 11);
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
