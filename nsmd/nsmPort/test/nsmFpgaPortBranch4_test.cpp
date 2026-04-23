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
 * Branch coverage tests for nsmFpgaPort.cpp constructor bodies:
 * - NsmFpgaPort constructor: D-Bus interface creation, health/chassis state
 *   conversion, association loop (empty & multiple associations)
 * - NsmFpgaPortInfo constructor: portType/portProtocol conversion
 * - NsmFpgaPortState constructor: linkStatus conversion
 * - Factory: NSM_FpgaPort with associations via factory path
 * - Factory: NSM_PCIe error path (pcieECCIntfSensorGroup null check - L221-229)
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

class NsmFpgaPortBranch4Test :
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

    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:90";
    const std::string sensorName = "FpgaPort_Br4";
    const std::string invPath =
        "/xyz/openbmc_project/inventory/system/fabr4/fpga";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmFpgaPortBranch4Test() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));

        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmFpgaPortBranch4Test()
    {
        cleanupDeviceSensors(devices);
    }
};

// ============================================================================
// Direct NsmFpgaPort constructor - with associations (loop body L34-38)
// Covers: L12,17,19-22,24-26,29-30,32-34,36-38,40-41
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPort_WithAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/port_ctor_assoc";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_fpga", "port", "/xyz/openbmc_project/inventory/system/fpga0"});
    associations.push_back({"connected_device", "port",
                            "/xyz/openbmc_project/inventory/system/device0"});

    NsmFpgaPort fpgaPort(
        bus, sensorName, "NSM_FpgaPort",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK",
        "xyz.openbmc_project.State.Chassis.PowerState.On", associations,
        objPath);

    EXPECT_EQ(fpgaPort.getName(), sensorName);
    EXPECT_EQ(fpgaPort.getType(), "NSM_FpgaPort");
}

// ============================================================================
// Direct NsmFpgaPort constructor - empty associations (loop not entered)
// Covers: L32 (false branch of for loop)
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPort_EmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/port_ctor_empty";

    std::vector<utils::Association> associations; // empty

    NsmFpgaPort fpgaPort(
        bus, sensorName, "NSM_FpgaPort",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning",
        "xyz.openbmc_project.State.Chassis.PowerState.Off", associations,
        objPath);

    EXPECT_EQ(fpgaPort.getName(), sensorName);
}

// ============================================================================
// Direct NsmFpgaPort constructor - Critical health state
// Covers different health conversion path
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPort_CriticalHealth)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/port_ctor_crit";

    std::vector<utils::Association> associations;

    NsmFpgaPort fpgaPort(
        bus, sensorName, "NSM_FpgaPort",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical",
        "xyz.openbmc_project.State.Chassis.PowerState.On", associations,
        objPath);

    EXPECT_EQ(fpgaPort.getName(), sensorName);
}

// ============================================================================
// Direct NsmFpgaPortInfo constructor
// Covers: L43,48,50-52,54
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPortInfo)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/portinfo_ctor";

    auto portInfoIntfObj = std::make_shared<PortInfoIntf>(bus, objPath.c_str());

    NsmFpgaPortInfo portInfo(sensorName, "NSM_PortInfo",
                             "xyz.openbmc_project.Inventory.Decorator.PortInfo."
                             "PortType.UpstreamPort",
                             "xyz.openbmc_project.Inventory.Decorator.PortInfo."
                             "PortProtocol.PCIe",
                             portInfoIntfObj);

    EXPECT_EQ(portInfo.getName(), sensorName);
    EXPECT_EQ(portInfo.getType(), "NSM_PortInfo");
}

// ============================================================================
// Direct NsmFpgaPortInfo constructor - DownstreamPort type
// Covers different portType conversion path
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPortInfo_DownstreamPort)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/portinfo_ctor_ds";

    auto portInfoIntfObj = std::make_shared<PortInfoIntf>(bus, objPath.c_str());

    NsmFpgaPortInfo portInfo(sensorName, "NSM_PortInfo",
                             "xyz.openbmc_project.Inventory.Decorator.PortInfo."
                             "PortType.DownstreamPort",
                             "xyz.openbmc_project.Inventory.Decorator.PortInfo."
                             "PortProtocol.NVLink",
                             portInfoIntfObj);

    EXPECT_EQ(portInfo.getName(), sensorName);
}

// ============================================================================
// Direct NsmFpgaPortState constructor
// Covers: L56,61,63-66,68
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPortState_LinkUp)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/portstate_ctor_up";

    NsmFpgaPortState portState(
        bus, sensorName, "NSM_PortState",
        "xyz.openbmc_project.Inventory.Decorator.PortState."
        "LinkStatusType.LinkUp",
        objPath);

    EXPECT_EQ(portState.getName(), sensorName);
    EXPECT_EQ(portState.getType(), "NSM_PortState");
}

// ============================================================================
// Direct NsmFpgaPortState constructor - LinkDown
// Covers different linkStatus conversion path
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPortState_LinkDown)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/portstate_ctor_down";

    NsmFpgaPortState portState(
        bus, sensorName, "NSM_PortState",
        "xyz.openbmc_project.Inventory.Decorator.PortState."
        "LinkStatusType.LinkDown",
        objPath);

    EXPECT_EQ(portState.getName(), sensorName);
}

// ============================================================================
// Direct NsmFpgaPortState constructor - NoLink
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Constructor_NsmFpgaPortState_NoLink)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string objPath = invPath + "/portstate_ctor_nolink";

    NsmFpgaPortState portState(
        bus, sensorName, "NSM_PortState",
        "xyz.openbmc_project.Inventory.Decorator.PortState."
        "LinkStatusType.NoLink",
        objPath);

    EXPECT_EQ(portState.getName(), sensorName);
}

// ============================================================================
// Factory: NSM_FpgaPort with associations (covers association loop via factory)
// Covers: L122-141 including co_await utils::coGetAssociations
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_FpgaPort_WithAssociations)
{
    const std::string path = "/test/fpgabr4/fpgaport_assoc";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "/fpgaport_assoc";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    // Set up association properties
    dbus::PropertyMap association0 = {
        {"Forward", std::string("parent_fpga")},
        {"Backward", std::string("port")},
        {"AbsolutePath",
         std::string("/xyz/openbmc_project/inventory/system/fpga0")}};
    auto& assocMap = utils::MockDbusAsync::propertyMap(
        path, std::string(baseIntf) + ".Associations0");
    assocMap = association0;

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_FpgaPort with multiple associations
// Covers: L34-38 loop body executed multiple times
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_FpgaPort_MultipleAssociations)
{
    const std::string path = "/test/fpgabr4/fpgaport_multi_assoc";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "/fpgaport_multi_assoc";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    // First association
    dbus::PropertyMap association0 = {
        {"Forward", std::string("parent_fpga")},
        {"Backward", std::string("port")},
        {"AbsolutePath",
         std::string("/xyz/openbmc_project/inventory/system/fpga0")}};
    auto& assocMap0 = utils::MockDbusAsync::propertyMap(
        path, std::string(baseIntf) + ".Associations0");
    assocMap0 = association0;

    // Second association
    dbus::PropertyMap association1 = {
        {"Forward", std::string("connected_device")},
        {"Backward", std::string("port")},
        {"AbsolutePath",
         std::string("/xyz/openbmc_project/inventory/system/device0")}};
    auto& assocMap1 = utils::MockDbusAsync::propertyMap(
        path, std::string(baseIntf) + ".Associations1");
    assocMap1 = association1;

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_FpgaPort with no associations (empty associations list)
// Covers: L34 for-loop false branch (no iterations)
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_FpgaPort_NoAssociations)
{
    const std::string path = "/test/fpgabr4/fpgaport_no_assoc";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "/fpgaport_no_assoc";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    // No association sub-interfaces registered -> empty associations vector

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, baseIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_PortInfo via factory (covers NsmFpgaPortInfo ctor through
// factory path)
// Covers: L43,48,50-52,54 via factory + L170-181
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_PortInfo_FullProps)
{
    const std::string path = "/test/fpgabr4/portinfo_full";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "/portinfo_full";
    base["DeviceIndex"] = uint64_t(2);

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    cur["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortType.UpstreamPort");
    cur["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortProtocol.PCIe");
    cur["Priority"] = true;

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, portInfoIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_PortState via factory (covers NsmFpgaPortState ctor through
// factory path)
// Covers: L56,61,63-66,68 via factory + L192-194
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_PortState_FullProps)
{
    const std::string path = "/test/fpgabr4/portstate_full";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "/portstate_full";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portStateIntf);
    cur["Type"] = std::string("NSM_PortState");
    cur["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState."
        "LinkStatusType.LinkUp");

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, portStateIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_PCIe via factory with priority=true (covers L232-236)
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_PCIe_WithPriority)
{
    const std::string path = "/test/fpgabr4/pcie_priority";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "/pcie_priority";
    base["DeviceIndex"] = uint64_t(3);

    auto& cur = utils::MockDbusAsync::propertyMap(path, pcieIntf);
    cur["Type"] = std::string("NSM_PCIe");
    cur["Priority"] = true;

    const size_t before = gpu->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, pcieIntf, path);
    EXPECT_GT(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: NSM_PCIe via factory with priority=false (roundRobin path)
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_PCIe_NoPriority)
{
    const std::string path = "/test/fpgabr4/pcie_no_priority";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "/pcie_no_priority";
    base["DeviceIndex"] = uint64_t(4);

    auto& cur = utils::MockDbusAsync::propertyMap(path, pcieIntf);
    cur["Type"] = std::string("NSM_PCIe");
    cur["Priority"] = false;

    const size_t rrBefore = gpu->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, pcieIntf, path);
    EXPECT_GT(gpu->roundRobinSensors.size(), rrBefore);
}

// ============================================================================
// Factory: exception in NsmFpgaPort constructor (invalid health string)
// Covers: L240-246 catch branch
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_FpgaPort_InvalidHealth_ExceptionCaught)
{
    const std::string path = "/test/fpgabr4/fpga_bad_health";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "/fpga_bad_health";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] = std::string("invalid.health.enum.string");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, baseIntf, path));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: exception in NsmFpgaPortState constructor (invalid linkStatus)
// Covers: L240-246 catch branch via PortState path
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_PortState_InvalidLinkStatus_Exception)
{
    const std::string path = "/test/fpgabr4/portstate_bad_ls";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "/portstate_bad_ls";

    auto& cur = utils::MockDbusAsync::propertyMap(path, portStateIntf);
    cur["Type"] = std::string("NSM_PortState");
    cur["LinkStatus"] = std::string("invalid.link.status.string");

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, portStateIntf, path));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: exception in NsmFpgaPortInfo constructor (invalid portType)
// Covers: L240-246 catch branch via PortInfo path
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test, Factory_PortInfo_InvalidPortType_Exception)
{
    const std::string path = "/test/fpgabr4/portinfo_bad_pt";

    auto& base = utils::MockDbusAsync::propertyMap(path, baseIntf);
    base["Name"] = sensorName;
    base["UUID"] = gpuUuid;
    base["InventoryObjPath"] = invPath + "/portinfo_bad_pt";
    base["DeviceIndex"] = uint64_t(0);

    auto& cur = utils::MockDbusAsync::propertyMap(path, portInfoIntf);
    cur["Type"] = std::string("NSM_PortInfo");
    cur["PortType"] = std::string("invalid.port.type.string");
    cur["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo."
        "PortProtocol.PCIe");
    cur["Priority"] = false;

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, portInfoIntf, path));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}

// ============================================================================
// Factory: exception in NsmFpgaPort constructor (invalid chassis state)
// Covers: L240-246 catch branch via invalid chassis conversion
// ============================================================================
TEST_F(NsmFpgaPortBranch4Test,
       Factory_FpgaPort_InvalidChassisState_ExceptionCaught)
{
    const std::string path = "/test/fpgabr4/fpga_bad_chassis";

    auto& pm = utils::MockDbusAsync::propertyMap(path, baseIntf);
    pm["Name"] = sensorName;
    pm["UUID"] = gpuUuid;
    pm["InventoryObjPath"] = invPath + "/fpga_bad_chassis";
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    pm["ChasisPowerState"] = std::string("invalid.chassis.state.string");

    const size_t before = gpu->deviceSensors.size();
    EXPECT_NO_THROW_COROUTINE(
        createNsmFpgaPortSensor(mockManager, baseIntf, path));
    EXPECT_EQ(gpu->deviceSensors.size(), before);
}
