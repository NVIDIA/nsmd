/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Branch coverage tests for nsmd/nsmPort/nsmPCIePort.cpp
 *
 * Covers createNsmPCIePort factory function:
 *   - TRUE/FALSE branches for each optional property:
 *     InventoryObjPath, UUID, Health, PortType, PortProtocol,
 *     LinkState, LinkStatus
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <sdbusplus/exception.hpp>

using namespace ::testing;

#define private public
#define protected public

#include "nsmPCIePort.hpp"

namespace nsm
{
requester::Coroutine createNsmPCIePort(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Test fixture
// ============================================================================

struct NsmPCIePortTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf = "xyz.openbmc_project.Configuration.NSM_PCIePort";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/pcie/port0";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/pcie/port0/port";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPCIePortTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPCIePortTest()
    {
        cleanupDeviceSensors(devices);
    }

    // Set all required properties in the property map
    void setAllProperties(dbus::PropertyMap& pm,
                          bool withInventoryObjPath = true,
                          bool withUUID = true, bool withHealth = true,
                          bool withPortType = true,
                          bool withPortProtocol = true,
                          bool withLinkState = true, bool withLinkStatus = true)
    {
        if (withInventoryObjPath)
        {
            pm["InventoryObjPath"] = inventoryObjPath;
        }
        if (withUUID)
        {
            pm["UUID"] = gpuUuid;
        }
        if (withHealth)
        {
            pm["Health"] = std::string(
                "xyz.openbmc_project.State.Decorator.Health"
                ".HealthType.OK");
        }
        if (withPortType)
        {
            pm["PortType"] = std::string(
                "xyz.openbmc_project.Inventory.Decorator.PortInfo"
                ".PortType.BidirectionalPort");
        }
        if (withPortProtocol)
        {
            pm["PortProtocol"] = std::string(
                "xyz.openbmc_project.Inventory.Decorator.PortInfo"
                ".PortProtocol.PCIe");
        }
        if (withLinkState)
        {
            pm["LinkState"] = std::string(
                "xyz.openbmc_project.Inventory.Decorator.PortState"
                ".LinkStates.Enabled");
        }
        if (withLinkStatus)
        {
            pm["LinkStatus"] = std::string(
                "xyz.openbmc_project.Inventory.Decorator.PortState"
                ".LinkStatusType.LinkUp");
        }
    }
};

// ============================================================================
// All properties present → all 7 TRUE branches covered
// ============================================================================

TEST_F(NsmPCIePortTest, AllPropertiesPresent_CreatesSensors)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm);

    size_t deviceSensorsBefore = gpu->deviceSensors.size();
    // PCIE_PORT_LINK_SPEED_PRIORITY and PCIE_PORT_ERRORS_PRIORITY are false,
    // so addSensor sends to roundRobinSensors
    size_t roundRobinBefore = gpu->roundRobinSensors.size();

    createNsmPCIePort(mockManager, intf, objPath);

    // addDeviceSensors adds 4 sensors (associations, health, port, portState)
    // addSensor also pushes to deviceSensors: 4 more (linkSpeed + 3 errors)
    EXPECT_EQ(gpu->deviceSensors.size(), deviceSensorsBefore + 8);
    // PCIE_PORT_*_PRIORITY = false → addSensor uses RoundRobin
    EXPECT_EQ(gpu->roundRobinSensors.size(), roundRobinBefore + 4);
}

// ============================================================================
// InventoryObjPath absent → FALSE branch for that if-check
// (inventoryObjPath stays ""; sdbusplus rejects empty D-Bus path → throws)
// ============================================================================

TEST_F(NsmPCIePortTest, InventoryObjPathAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/false);

    // FALSE branch executed (inventoryObjPath stays ""), then D-Bus object
    // creation at "" fails with an invalid-argument exception
    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           std::exception);
}

// ============================================================================
// UUID absent → FALSE branch for UUID if-check
// (uuid stays ""; parseStaticUuid("") fails → throws std::runtime_error)
// ============================================================================

TEST_F(NsmPCIePortTest, UUIDAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/true, /*withUUID=*/false);

    // FALSE branch executed (uuid stays ""), then
    // getNsmDeviceFromStaticUUID("") calls parseStaticUuid("") which throws
    // std::runtime_error
    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           std::runtime_error);
}

// ============================================================================
// Health absent → FALSE branch; empty string → convert throws
// ============================================================================

TEST_F(NsmPCIePortTest, HealthAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/true, /*withUUID=*/true,
                     /*withHealth=*/false);

    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           sdbusplus::exception::InvalidEnumString);
}

// ============================================================================
// PortType absent → FALSE branch; empty string → convert throws
// ============================================================================

TEST_F(NsmPCIePortTest, PortTypeAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/true, /*withUUID=*/true,
                     /*withHealth=*/true, /*withPortType=*/false);

    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           sdbusplus::exception::InvalidEnumString);
}

// ============================================================================
// PortProtocol absent → FALSE branch; empty string → convert throws
// ============================================================================

TEST_F(NsmPCIePortTest, PortProtocolAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/true, /*withUUID=*/true,
                     /*withHealth=*/true, /*withPortType=*/true,
                     /*withPortProtocol=*/false);

    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           sdbusplus::exception::InvalidEnumString);
}

// ============================================================================
// LinkState absent → FALSE branch; empty string → convert throws
// ============================================================================

TEST_F(NsmPCIePortTest, LinkStateAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/true, /*withUUID=*/true,
                     /*withHealth=*/true, /*withPortType=*/true,
                     /*withPortProtocol=*/true, /*withLinkState=*/false);

    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           sdbusplus::exception::InvalidEnumString);
}

// ============================================================================
// LinkStatus absent → FALSE branch; empty string → convert throws
// ============================================================================

TEST_F(NsmPCIePortTest, LinkStatusAbsent_FalseBranch_Throws)
{
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, intf);
    setAllProperties(pm, /*withInventoryObjPath=*/true, /*withUUID=*/true,
                     /*withHealth=*/true, /*withPortType=*/true,
                     /*withPortProtocol=*/true, /*withLinkState=*/true,
                     /*withLinkStatus=*/false);

    EXPECT_THROW_COROUTINE(createNsmPCIePort(mockManager, intf, objPath),
                           sdbusplus::exception::InvalidEnumString);
}
