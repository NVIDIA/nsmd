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
 * Branch coverage tests for nsmd/nsmDeviceInventory/nsmNetworkAdapter.cpp
 *
 * Calls nsm::features::createNetworkAdapterFeature directly to exercise
 * creator branches:
 *  - Valid creation with all params
 *  - No ParentChassisAssociationPaths / empty entry → no association
 *  - No NsmDevice → error path
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmNetworkAdapter.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmNetworkAdapterBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string chassisPath =
        "/xyz/openbmc_project/inventory/system/chassis/HGX_CX_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmNetworkAdapterBranchTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmNetworkAdapterBranchTest() override
    {
        cleanupDeviceSensors(devices);
    }

    features::DeviceContext makeDev(bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "HGX_CX_0";
        dev.kind = "cx_nic";
        dev.staticUuid = deviceUuid;
        dev.platformPrefix = "HGX_";
        dev.chassisPath = chassisPath;
        if (withDevice)
        {
            dev.device = device;
        }
        return dev;
    }

    std::shared_ptr<NsmNetworkAdapterDI> findAdapter() const
    {
        for (const auto& sensor : device->deviceSensors)
        {
            if (auto adapter =
                    std::dynamic_pointer_cast<NsmNetworkAdapterDI>(sensor))
            {
                return adapter;
            }
        }
        return nullptr;
    }

    dbus::PropertyMap basicParams = {
        {"Name", std::string("NetAdapter_Branch_0")},
        {"ParentChassisAssociationPaths",
         std::vector<std::string>{"chassis/HGX_CX_0"}},
    };
};

// ============================================================================
// All params present -> adapter created with parent_chassis association
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, Feature_AllParams_SensorCreated)
{
    const size_t before = device->deviceSensors.size();
    auto rc = features::createNetworkAdapterFeature(mockManager, makeDev(),
                                                    basicParams);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_GT(device->deviceSensors.size(), before);

    auto adapter = findAdapter();
    ASSERT_NE(adapter, nullptr);
    EXPECT_EQ(adapter->getName(), "NetAdapter_Branch_0");
    EXPECT_EQ(adapter->getType(), "NSM_NetworkAdapter");
    const auto& assocs = adapter->associationDefIntf->associations();
    ASSERT_EQ(assocs.size(), 1u);
    EXPECT_EQ(std::get<0>(assocs[0]), "parent_chassis");
    EXPECT_EQ(std::get<1>(assocs[0]), "network_adapters");
    EXPECT_EQ(std::get<2>(assocs[0]), chassisPath);
}

// ============================================================================
// No ParentChassisAssociationPaths -> adapter without associations
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, Feature_NoAssociationParam_NoAssociation)
{
    auto params = basicParams;
    params["Name"] = std::string("NetAdapter_Branch_NoAssoc");
    params.erase("ParentChassisAssociationPaths");

    auto rc = features::createNetworkAdapterFeature(mockManager, makeDev(),
                                                    params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto adapter = findAdapter();
    ASSERT_NE(adapter, nullptr);
    EXPECT_TRUE(adapter->associationDefIntf->associations().empty());
}

// ============================================================================
// Empty association entry -> no association
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, Feature_EmptyAssociationEntry_Skipped)
{
    auto params = basicParams;
    params["Name"] = std::string("NetAdapter_Branch_EmptyAssoc");
    params["ParentChassisAssociationPaths"] = std::vector<std::string>{""};

    auto rc = features::createNetworkAdapterFeature(mockManager, makeDev(),
                                                    params);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    auto adapter = findAdapter();
    ASSERT_NE(adapter, nullptr);
    EXPECT_TRUE(adapter->associationDefIntf->associations().empty());
}

// ============================================================================
// No NsmDevice -> NSM_ERROR, nothing created
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, Feature_NullDevice_ReturnsError)
{
    const size_t before = device->deviceSensors.size();
    auto rc = features::createNetworkAdapterFeature(mockManager, makeDev(false),
                                                    basicParams);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(device->deviceSensors.size(), before);
}
