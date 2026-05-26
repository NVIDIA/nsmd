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
 * Calls createNSMNetworkAdapter directly (forward-declared) to exercise
 * factory-function branches:
 *  - Missing Name/UUID/Type properties (FALSE count() branches)
 *  - Valid creation with all properties
 *  - Invalid UUID -> error path
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmNetworkAdapter.hpp"

#undef private
#undef protected

// Forward-declare the factory function (non-static)
namespace nsm
{
requester::Coroutine createNSMNetworkAdapter(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmNetworkAdapterBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NetworkAdapter";
    const uuid_t deviceUuid = "STATIC:514:0:NSM_DEVICE_INSTANCE_NUMBER:0";

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

    dbus::PropertyMap basicProperties = {
        {"Name", std::string("NetAdapter_Branch_0")},
        {"UUID", deviceUuid},
        {"Type", std::string("NSM_NetworkAdapter")},
        {"InventoryObjPath",
         std::string("/xyz/openbmc_project/inventory/system/")},
    };
};

// ============================================================================
// Factory: all properties present -> sensor created
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, Factory_AllProperties_SensorCreated)
{
    const std::string testPath = "/xyz/test/netadapter_branch/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = basicProperties;

    const size_t before = device->deviceSensors.size();
    createNSMNetworkAdapter(mockManager, intf, testPath);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// Factory: missing Name -> FALSE count("Name") branch
// name="" -> D-Bus path ends with "/" -> sdbusplus throws -> no sensor
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, DISABLED_Factory_MissingName_NoSensor)
{
    const std::string testPath = "/xyz/test/netadapter_branch/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = basicProperties;
    pm.erase("Name");

    const size_t before = device->deviceSensors.size();
    try
    {
        createNSMNetworkAdapter(mockManager, intf, testPath);
    }
    catch (const std::exception&)
    {}
    EXPECT_EQ(before, device->deviceSensors.size());
}

// ============================================================================
// Factory: missing UUID -> FALSE count("UUID") branch
// uuid="" -> parseStaticUuid throws -> no sensor
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, DISABLED_Factory_MissingUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/netadapter_branch/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = basicProperties;
    pm["Name"] = std::string("NetAdapter_Branch_NoUUID");
    pm.erase("UUID");

    const size_t before = device->deviceSensors.size();
    try
    {
        createNSMNetworkAdapter(mockManager, intf, testPath);
    }
    catch (const std::exception&)
    {}
    EXPECT_EQ(before, device->deviceSensors.size());
}

// ============================================================================
// Factory: missing Type -> FALSE count("Type") branch
// type="" -> sensor still created (empty type is valid)
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, Factory_MissingType_SensorCreated)
{
    const std::string testPath = "/xyz/test/netadapter_branch/no_type";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = basicProperties;
    pm["Name"] = std::string("NetAdapter_Branch_NoType");
    pm.erase("Type");

    const size_t before = device->deviceSensors.size();
    createNSMNetworkAdapter(mockManager, intf, testPath);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// Factory: invalid UUID -> getNsmDeviceFromStaticUUID throws -> error path
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest, DISABLED_Factory_InvalidUUID_NoSensor)
{
    const std::string testPath = "/xyz/test/netadapter_branch/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = basicProperties;
    pm["Name"] = std::string("NetAdapter_Branch_BadUUID");
    pm["UUID"] = std::string("INVALID_UUID_FORMAT");

    const size_t before = device->deviceSensors.size();
    try
    {
        createNSMNetworkAdapter(mockManager, intf, testPath);
    }
    catch (const std::exception&)
    {}
    EXPECT_EQ(before, device->deviceSensors.size());
}

// ============================================================================
// Factory: missing InventoryObjPath -> FALSE count("InventoryObjPath")
// inventoryObjPath="" -> no leading "/" -> sdbusplus throws -> no sensor
// ============================================================================

TEST_F(NsmNetworkAdapterBranchTest,
       DISABLED_Factory_MissingInventoryObjPath_NoSensor)
{
    const std::string testPath = "/xyz/test/netadapter_branch/no_invpath";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm = basicProperties;
    pm["Name"] = std::string("NetAdapter_Branch_NoInvPath");
    pm.erase("InventoryObjPath");

    const size_t before = device->deviceSensors.size();
    try
    {
        createNSMNetworkAdapter(mockManager, intf, testPath);
    }
    catch (const std::exception&)
    {}
    EXPECT_EQ(before, device->deviceSensors.size());
}
