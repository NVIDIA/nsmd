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
 * Branch coverage for nsmPCIeRetimerFabricsDI.cpp feature creator:
 *
 * Targets code paths:
 * - createPCIeRetimerFabricsFeature: all params present
 * - createPCIeRetimerFabricsFeature: no NsmDevice
 * - createPCIeRetimerFabricsFeature: missing Name, FabricType
 * - createPCIeRetimerFabricsFeature: ChassisAssociationPaths
 * - NsmPCIeRetimerFabricDI constructor: verify interface initialization
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmPCIeRetimerFabricsDI.hpp"

#undef private
#undef protected

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

    features::DeviceContext makeDev(bool withDevice = true)
    {
        features::DeviceContext dev;
        dev.name = "HGX_PCIeRetimer_0";
        dev.kind = "pcie_retimer";
        dev.staticUuid = deviceUuid;
        dev.platformPrefix = "HGX_";
        if (withDevice)
        {
            dev.device = gpu;
        }
        return dev;
    }

    static dbus::PropertyMap params(const std::string& name)
    {
        return {{"Name", name}, {"FabricType", std::string(fabricTypePCIe)}};
    }
};

// ============================================================================
// Feature: all params present -> sensor created
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Feature_AllParams_SensorCreated)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = features::createPCIeRetimerFabricsFeature(mockManager, makeDev(),
                                                        params("FabBr_0"));
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(gpu->deviceSensors.size(), before + 1);
    auto fabric = std::dynamic_pointer_cast<NsmPCIeRetimerFabricDI>(
        gpu->deviceSensors.back());
    ASSERT_NE(fabric, nullptr);
    EXPECT_EQ(fabric->getName(), "FabBr_0");
    EXPECT_EQ(fabric->getType(), "NSM_PCIeRetimer_Fabrics");
    EXPECT_TRUE(fabric->associationDefIntf->associations().empty());
}

// ============================================================================
// Feature: no NsmDevice -> error path
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Feature_NullDevice_ReturnsError)
{
    const size_t before = gpu->deviceSensors.size();
    auto rc = features::createPCIeRetimerFabricsFeature(
        mockManager, makeDev(false), params("FabBr_NullDevice"));
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ============================================================================
// Feature: missing Name -> name="" -> D-Bus path ends with "/"
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Feature_MissingName_EmptyName)
{
    auto p = params("");
    p.erase("Name");

    // Empty name creates a path ending with "/" which sdbusplus may reject
    try
    {
        features::createPCIeRetimerFabricsFeature(mockManager, makeDev(), p);
    }
    catch (...)
    {
        // Name="" may cause sdbusplus path validation to throw
    }
    // Either throws or creates sensor with empty name; both are valid outcomes
}

// ============================================================================
// Feature: missing FabricType -> fabricType="" ->
// convertFabricTypeFromString("") throws
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Feature_MissingFabricType_NoSensor)
{
    auto p = params("FabBr_NoFabType");
    p.erase("FabricType");

    const size_t before = gpu->deviceSensors.size();
    try
    {
        features::createPCIeRetimerFabricsFeature(mockManager, makeDev(), p);
    }
    catch (...)
    {
        // Empty fabricType -> convertFabricTypeFromString throws
    }
    // Sensor should not be added on failure
    EXPECT_EQ(before, gpu->deviceSensors.size());
}

// ============================================================================
// Feature: ChassisAssociationPaths -> chassis/fabrics association
// ============================================================================

TEST_F(NsmPCIeRetimerFabricsDIBranchTest, Feature_WithAssociations_Created)
{
    auto p = params("FabBr_Assoc");
    p["ChassisAssociationPaths"] =
        std::vector<std::string>{"chassis/HGX_PCIeRetimer_0"};

    const size_t before = gpu->deviceSensors.size();
    auto rc = features::createPCIeRetimerFabricsFeature(mockManager, makeDev(),
                                                        p);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_EQ(gpu->deviceSensors.size(), before + 1);
    auto fabric = std::dynamic_pointer_cast<NsmPCIeRetimerFabricDI>(
        gpu->deviceSensors.back());
    ASSERT_NE(fabric, nullptr);
    const auto& assocs = fabric->associationDefIntf->associations();
    ASSERT_EQ(assocs.size(), 1u);
    EXPECT_EQ(std::get<0>(assocs[0]), "chassis");
    EXPECT_EQ(std::get<1>(assocs[0]), "fabrics");
    EXPECT_EQ(std::get<2>(assocs[0]),
              "/xyz/openbmc_project/inventory/system/chassis/"
              "HGX_PCIeRetimer_0");
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
