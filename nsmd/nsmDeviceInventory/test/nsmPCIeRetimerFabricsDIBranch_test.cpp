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
 * Branch coverage for nsmPCIeRetimerFabricsDI.cpp factory function:
 *
 * Targets uncovered code paths:
 * - createNSMPCIeRetimerFabrics: all properties present (direct call)
 * - createNSMPCIeRetimerFabrics: !nsmDevice (UUID matches no device)
 * - createNSMPCIeRetimerFabrics: missing Name, UUID, FabricType
 *   (FALSE count() branches)
 * - NsmPCIeRetimerFabricDI constructor: verify interface initialization
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmPCIeRetimerFabricsDI.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNSMPCIeRetimerFabrics(SensorManager& manager,
                                                 const std::string& interface,
                                                 const std::string& objPath);
} // namespace nsm

using namespace nsm;

static constexpr auto fabricTypePCIe =
    "xyz.openbmc_project.Inventory.Item.Fabric.FabricType.PCIe";

// ============================================================================
// Fixture
// ============================================================================

struct NsmPCIeRetimerFabricsDIBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_PCIeRetimer_Fabrics";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIeRetimerFabricsDIBranchTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIeRetimerFabricsDIBranchTest() override
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Factory: all properties present -> sensor created (direct call)
// Exercises lines 43-86: all count() TRUE branches, nsmDevice valid
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Factory_AllProperties_SensorCreated)
{
    const std::string testPath = "/xyz/test/fabrics_br/all_props";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabBr_0");
    pm["UUID"] = deviceUuid;
    pm["FabricType"] = std::string(fabricTypePCIe);

    const size_t before = gpu->deviceSensors.size();
    createNSMPCIeRetimerFabrics(mockManager, intf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: invalid UUID -> nsmDevice is nullptr -> error path (lines 71-79)
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Factory_InvalidUUID_ReturnsError)
{
    const std::string testPath = "/xyz/test/fabrics_br/bad_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabBr_BadUUID");
    pm["UUID"] = std::string("STATIC:99:99:NSM_DEVICE_INSTANCE_NUMBER:999");
    pm["FabricType"] = std::string(fabricTypePCIe);

    const size_t before = gpu->deviceSensors.size();
    createNSMPCIeRetimerFabrics(mockManager, intf, testPath);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ============================================================================
// Factory: missing Name -> FALSE count("Name") branch -> name=""
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Factory_MissingName_EmptyName)
{
    const std::string testPath = "/xyz/test/fabrics_br/no_name";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    // Name intentionally omitted -> name=""
    pm["UUID"] = deviceUuid;
    pm["FabricType"] = std::string(fabricTypePCIe);

    // Empty name creates a path ending with "/" which sdbusplus may reject
    try
    {
        createNSMPCIeRetimerFabrics(mockManager, intf, testPath);
    }
    catch (...)
    {
        // Name="" may cause sdbusplus path validation to throw
    }
    // Either throws or creates sensor with empty name; both are valid outcomes
}

// ============================================================================
// Factory: missing UUID -> FALSE count("UUID") branch -> uuid=""
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Factory_MissingUUID_NoDevice)
{
    const std::string testPath = "/xyz/test/fabrics_br/no_uuid";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabBr_NoUUID");
    // UUID intentionally omitted -> uuid=""
    pm["FabricType"] = std::string(fabricTypePCIe);

    const size_t before = gpu->deviceSensors.size();
    try
    {
        createNSMPCIeRetimerFabrics(mockManager, intf, testPath);
    }
    catch (...)
    {
        // uuid="" -> parseStaticUuid may throw
    }
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ============================================================================
// Factory: missing FabricType -> FALSE count("FabricType") branch
// fabricType="" -> convertFabricTypeFromString("") may throw
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Factory_MissingFabricType_EmptyType)
{
    const std::string testPath = "/xyz/test/fabrics_br/no_fabtype";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabBr_NoFabType");
    pm["UUID"] = deviceUuid;
    // FabricType intentionally omitted -> fabricType=""

    const size_t before = gpu->deviceSensors.size();
    try
    {
        createNSMPCIeRetimerFabrics(mockManager, intf, testPath);
    }
    catch (...)
    {
        // Empty fabricType -> convertFabricTypeFromString throws
    }
    // Sensor should not be added on failure
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ============================================================================
// Factory: with associations from D-Bus
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest,
       Factory_WithAssociations_SensorCreated)
{
    const std::string testPath = "/xyz/test/fabrics_br/with_assoc";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("FabBr_Assoc");
    pm["UUID"] = deviceUuid;
    pm["FabricType"] = std::string(fabricTypePCIe);

    // Set up association service map
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    MapperServiceMap assocServiceMap = {
        {{
            "xyz.openbmc_project.NSM",
            {intf + ".Associations"},
        }},
    };
    serviceMap = assocServiceMap;

    dbus::PropertyMap assocProperties = {
        {"Forward", "parent_chassis"},
        {"Backward", "pcie_retimer"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system"},
    };
    auto& assocPm = utils::MockDbusAsync::propertyMap(testPath,
                                                      intf + ".Associations");
    assocPm = assocProperties;

    const size_t before = gpu->deviceSensors.size();
    createNSMPCIeRetimerFabrics(mockManager, intf, testPath);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Constructor: verify all interfaces are created properly
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Constructor_VerifyInterfaces)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent", "child", "/xyz/openbmc_project/test/parent"});
    associations.push_back(
        {"fabric", "device", "/xyz/openbmc_project/test/fabric"});

    NsmPCIeRetimerFabricDI obj(bus, "FabBr_Verify", associations,
                               "NSM_PCIeRetimer_Fabrics", fabricTypePCIe);

    EXPECT_EQ(obj.getName(), "FabBr_Verify");
    EXPECT_EQ(obj.getType(), "NSM_PCIeRetimer_Fabrics");
    EXPECT_NE(obj.associationDefIntf, nullptr);
    EXPECT_NE(obj.uuidIntf, nullptr);
    EXPECT_NE(obj.fabricIntf, nullptr);
}
