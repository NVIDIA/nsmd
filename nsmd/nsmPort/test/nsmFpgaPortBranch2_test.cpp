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
 * Branch coverage for createNsmFpgaPortSensor factory in nsmFpgaPort.cpp.
 *
 * Covers:
 * - All 4 type dispatch branches (NSM_FpgaPort, NSM_PortInfo, NSM_PortState,
 *   NSM_PCIe)
 * - count() FALSE branches for optional properties (Health, ChasisPowerState,
 *   PortType, PortProtocol, Priority, DeviceIndex, LinkStatus)
 * - No device found (invalid UUID)
 * - Unknown type (no branch matches)
 * - Base properties failure (early return)
 * - Exception catch branch
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

class NsmFpgaPortFactory2Test :
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

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string sensorName = "FpgaPort_Br2";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/fabr2/fpga/";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmFpgaPortFactory2Test() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmFpgaPortFactory2Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Base properties fail -> early return
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_BasePropertiesFail)
{
    const std::string path = "/test/fpgabr2/no_base";
    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Invalid UUID -> no device
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_InvalidUUID_NoDevice)
{
    const std::string path = "/test/fpgabr2/bad_uuid";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = std::string("INVALID_UUID_STRING");
    pm["InventoryObjPath"] = invPath;
    pm["Type"] = std::string("NSM_FpgaPort");

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// NSM_FpgaPort type - with all properties
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_FpgaPort_AllProps)
{
    const std::string path = "/test/fpgabr2/fpgaport_all";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "fpgaport_all/";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    createNsmFpgaPortSensor(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_FpgaPort - missing Health (FALSE branch)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_FpgaPort_MissingHealth)
{
    const std::string path = "/test/fpgabr2/fpga_no_health";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "fpga_no_health/";
    pm["Type"] = std::string("NSM_FpgaPort");
    // Health omitted -> empty string -> throws in constructor -> caught
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    createNsmFpgaPortSensor(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_FpgaPort - missing ChasisPowerState (FALSE branch)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_FpgaPort_MissingChasisState)
{
    const std::string path = "/test/fpgabr2/fpga_no_chassis";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "fpga_no_chassis/";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    // ChasisPowerState omitted

    createNsmFpgaPortSensor(mockManager, baseIntf, path);
}

// ============================================================================
// NSM_PortInfo type - with all properties
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_PortInfo_AllProps)
{
    const std::string path = "/test/fpgabr2/portinfo_all";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portinfo_all/";
    base["DeviceIndex"] = uint64_t(0);

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    cur["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortType.UpstreamPort");
    cur["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortProtocol.PCIe");
    cur["Priority"] = bool(false);

    createNsmFpgaPortSensor(mockManager, portInfoIntf, path);
}

// ============================================================================
// NSM_PortInfo - missing optional properties (FALSE branches)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_PortInfo_MissingOptionalProps)
{
    const std::string path = "/test/fpgabr2/portinfo_minimal";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portinfo_minimal/";
    // DeviceIndex omitted

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    // PortType, PortProtocol, Priority omitted

    createNsmFpgaPortSensor(mockManager, portInfoIntf, path);
}

// ============================================================================
// NSM_PortState type - with LinkStatus
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_PortState_WithLinkStatus)
{
    const std::string path = "/test/fpgabr2/portstate_ls";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portstate_ls/";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portStateIntf);
    cur["Type"] = std::string("NSM_PortState");
    cur["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState."
        "LinkStatusType.LinkUp");

    createNsmFpgaPortSensor(mockManager, portStateIntf, path);
}

// ============================================================================
// NSM_PortState - missing LinkStatus (FALSE branch)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_PortState_MissingLinkStatus)
{
    const std::string path = "/test/fpgabr2/portstate_no_ls";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "portstate_no_ls/";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portStateIntf);
    cur["Type"] = std::string("NSM_PortState");
    // LinkStatus omitted

    createNsmFpgaPortSensor(mockManager, portStateIntf, path);
}

// ============================================================================
// NSM_PCIe type - with all properties
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_PCIe_AllProps)
{
    const std::string path = "/test/fpgabr2/pcie_all";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "pcie_all/";
    base["DeviceIndex"] = uint64_t(0);

    auto& cur = utils::MockDbusAsync::propertyMap(path, pcieIntf);
    cur["Type"] = std::string("NSM_PCIe");
    cur["Priority"] = bool(true);

    createNsmFpgaPortSensor(mockManager, pcieIntf, path);
}

// ============================================================================
// NSM_PCIe - missing optional properties (FALSE branches)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_PCIe_MissingOptionalProps)
{
    const std::string path = "/test/fpgabr2/pcie_minimal";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "pcie_minimal/";
    // DeviceIndex omitted

    auto& cur = utils::MockDbusAsync::propertyMap(path, pcieIntf);
    cur["Type"] = std::string("NSM_PCIe");
    // Priority omitted

    createNsmFpgaPortSensor(mockManager, pcieIntf, path);
}

// ============================================================================
// Unknown type -> no branch matches
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_UnknownType_NoSensor)
{
    const std::string path = "/test/fpgabr2/unknown_type";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "unknown_type/";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_Unknown_Type");

    const size_t before = gpu->deviceSensors.size() +
                          gpu->roundRobinSensors.size() +
                          gpu->staticSensors.size();
    createNsmFpgaPortSensor(mockManager, portInfoIntf, path);
    EXPECT_EQ(gpu->deviceSensors.size() + gpu->roundRobinSensors.size() +
                  gpu->staticSensors.size(),
              before);
}

// ============================================================================
// Missing Name and UUID (FALSE branches)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_MissingNameAndUUID)
{
    const std::string path = "/test/fpgabr2/no_name_uuid";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    // Name omitted
    // UUID omitted
    base["InventoryObjPath"] = invPath + "no_name_uuid/";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_FpgaPort");

    createNsmFpgaPortSensor(mockManager, portInfoIntf, path);
}

// ============================================================================
// Missing InventoryObjPath (FALSE branch)
// ============================================================================
TEST_F(NsmFpgaPortFactory2Test, Factory_MissingInventoryObjPath)
{
    const std::string path = "/test/fpgabr2/no_inv";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    // InventoryObjPath omitted
    base["Type"] = std::string("NSM_FpgaPort");
    base["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    base["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    createNsmFpgaPortSensor(mockManager, baseIntf, path);
}
