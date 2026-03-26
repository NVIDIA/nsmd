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
 * Branch coverage tests for nsmFpgaPort.cpp:
 * - createNsmFpgaPortSensor: unknown type (falls through all if/else if)
 * - createNsmFpgaPortSensor: missing Name, UUID, InventoryObjPath properties
 * - createNsmFpgaPortSensor: NSM_FpgaPort missing Health AND ChasisPowerState
 * - createNsmFpgaPortSensor: NSM_PortInfo missing PortType, PortProtocol,
 *   Priority, DeviceIndex
 * - createNsmFpgaPortSensor: NSM_PortState missing LinkStatus
 * - createNsmFpgaPortSensor: NSM_PCIe missing Priority, DeviceIndex
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "nsmFpgaPort.hpp"

namespace nsm
{
requester::Coroutine createNsmFpgaPortSensor(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

class NsmFpgaPortBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    static constexpr const char* baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    static constexpr const char* portInfoIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortInfo";
    static constexpr const char* portStateIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortState";
    static constexpr const char* pcieIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PCIe";

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:80";
    const std::string sensorName = "FpgaPort_Br3";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/fabr3/fpga/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmFpgaPortBranch3Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmFpgaPortBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Unknown type - falls through all type branches, no sensor added
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_UnknownType_NoSensorAdded)
{
    const std::string path = "/test/fpgabr3/unknown_type";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "unknown_type/";
    pm["Type"] = std::string("NSM_UnknownType");

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    // No sensor added because type does not match any branch
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Missing Name property (FALSE branch for "Name" count)
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_MissingName)
{
    const std::string path = "/test/fpgabr3/missing_name";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // Name omitted
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "missing_name/";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    // Should still create sensor (name defaults to empty string)
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
}

// ============================================================================
// Missing UUID property (FALSE branch for "UUID" count)
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_MissingUUID_NoDevice)
{
    const std::string path = "/test/fpgabr3/missing_uuid";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    // UUID omitted -> empty uuid -> no device found
    pm["InventoryObjPath"] = invPath + "missing_uuid/";
    pm["Type"] = std::string("NSM_FpgaPort");

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Missing InventoryObjPath (FALSE branch for "InventoryObjPath" count)
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_MissingInventoryObjPath)
{
    const std::string path = "/test/fpgabr3/missing_inv";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    // InventoryObjPath omitted -> defaults to empty
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    // Exception from PortIntf constructor with empty path -> caught
    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, baseIntf, path));
}

// ============================================================================
// NSM_FpgaPort: both Health AND ChasisPowerState missing
// Both FALSE branches hit simultaneously
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_FpgaPort_MissingBothHealthAndChassis)
{
    const std::string path = "/test/fpgabr3/no_health_chassis";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "no_health_chassis/";
    pm["Type"] = std::string("NSM_FpgaPort");
    // Both Health and ChasisPowerState omitted -> empty strings -> throws

    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, baseIntf, path));
}

// ============================================================================
// NSM_PortInfo: missing PortType, PortProtocol, Priority, DeviceIndex
// All FALSE branches for optional PortInfo properties
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_PortInfo_MissingAllOptionalProps)
{
    const std::string path = "/test/fpgabr3/portinfo_bare";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portinfo_bare/";
    // DeviceIndex omitted

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    // PortType, PortProtocol, Priority omitted -> all FALSE branches

    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, portInfoIntf, path));
}

// ============================================================================
// NSM_PortState: missing LinkStatus
// FALSE branch for "LinkStatus" count
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_PortState_MissingLinkStatus)
{
    const std::string path = "/test/fpgabr3/portstate_bare";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portstate_bare/";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portStateIntf);
    cur["Type"] = std::string("NSM_PortState");
    // LinkStatus omitted -> empty string -> throws in constructor

    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, portStateIntf, path));
}

// ============================================================================
// NSM_PCIe: missing Priority, DeviceIndex
// FALSE branches for "Priority" and "DeviceIndex" count
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_PCIe_MissingPriorityAndDeviceIndex)
{
    const std::string path = "/test/fpgabr3/pcie_bare";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "pcie_bare/";
    // DeviceIndex omitted

    auto& cur = utils::MockDbusAsync::propertyMap(path, pcieIntf);
    cur["Type"] = std::string("NSM_PCIe");
    // Priority omitted

    createNsmFpgaPortSensor(mockManager, pcieIntf, path);
}

// ============================================================================
// NSM_PortInfo: with all properties present (TRUE branches)
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_PortInfo_AllPropsPresent)
{
    const std::string path = "/test/fpgabr3/portinfo_full";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portinfo_full/";
    base["DeviceIndex"] = uint64_t(1);

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    cur["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortType.UpstreamPort");
    cur["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortProtocol.PCIe");
    cur["Priority"] = true;

    createNsmFpgaPortSensor(mockManager, portInfoIntf, path);
}

// ============================================================================
// NSM_PortState: with LinkStatus present (TRUE branch)
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_PortState_WithLinkStatus)
{
    const std::string path = "/test/fpgabr3/portstate_full";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portstate_full/";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portStateIntf);
    cur["Type"] = std::string("NSM_PortState");
    cur["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState."
        "LinkStatusType.LinkUp");

    createNsmFpgaPortSensor(mockManager, portStateIntf, path);
}

// ============================================================================
// Missing Type property (FALSE branch for "Type" count in current iface)
// ============================================================================
TEST_F(NsmFpgaPortBranch3Test, Factory_MissingType_NoSensorAdded)
{
    const std::string path = "/test/fpgabr3/missing_type";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "missing_type/";
    // Type omitted -> empty string -> no branch matches

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
