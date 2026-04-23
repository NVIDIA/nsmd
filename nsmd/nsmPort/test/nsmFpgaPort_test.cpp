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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "nsmFpgaPort.hpp"
#include "nsmGpuPciePort.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmFpgaPortSensor(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);

}; // namespace nsm

auto bus = sdbusplus::bus::new_default();

TEST(NsmFpgaPort, Constructor)
{
    std::string name = "FpgaPort0";
    std::string type = "NSM_FpgaPort";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";
    std::string chasisState = "xyz.openbmc_project.State.Chassis.PowerState.On";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/port0";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_fpga", "port", "/xyz/openbmc_project/inventory/system/fpga0"});

    NsmFpgaPort fpgaPort(bus, name, type, health, chasisState, associations,
                         inventoryObjPath);

    EXPECT_EQ(fpgaPort.getName(), name);
    EXPECT_EQ(fpgaPort.getType(), type);
}

TEST(NsmFpgaPort, ConstructorWithMultipleAssociations)
{
    std::string name = "FpgaPort1";
    std::string type = "NSM_FpgaPort";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning";
    std::string chasisState =
        "xyz.openbmc_project.State.Chassis.PowerState.Off";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/port1";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_fpga", "port", "/xyz/openbmc_project/inventory/system/fpga1"});
    associations.push_back({"connected_device", "port",
                            "/xyz/openbmc_project/inventory/system/device0"});

    NsmFpgaPort fpgaPort(bus, name, type, health, chasisState, associations,
                         inventoryObjPath);

    EXPECT_EQ(fpgaPort.getName(), name);
    EXPECT_EQ(fpgaPort.getType(), type);
}

TEST(NsmFpgaPort, ConstructorWithEmptyAssociations)
{
    std::string name = "FpgaPort2";
    std::string type = "NSM_FpgaPort";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical";
    std::string chasisState = "xyz.openbmc_project.State.Chassis.PowerState.On";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/port2";

    std::vector<utils::Association> associations; // Empty

    NsmFpgaPort fpgaPort(bus, name, type, health, chasisState, associations,
                         inventoryObjPath);

    EXPECT_EQ(fpgaPort.getName(), name);
    EXPECT_EQ(fpgaPort.getType(), type);
}

TEST(NsmFpgaPort, ConstructorWithDifferentHealthStates)
{
    std::string name = "FpgaPort3";
    std::string type = "NSM_FpgaPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/port3";
    std::vector<utils::Association> associations;

    // Test different health states
    std::vector<std::string> healthStates = {
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical"};
    for (const auto& health : healthStates)
    {
        NsmFpgaPort fpgaPort(bus, name, type, health,
                             "xyz.openbmc_project.State.Chassis.PowerState.On",
                             associations, inventoryObjPath);
        EXPECT_EQ(fpgaPort.getName(), name);
    }
}

TEST(NsmFpgaPort, ConstructorWithDifferentChasisStates)
{
    std::string name = "FpgaPort4";
    std::string type = "NSM_FpgaPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/port4";
    std::vector<utils::Association> associations;

    // Test different chasis states
    std::vector<std::string> chasisStates = {
        "xyz.openbmc_project.State.Chassis.PowerState.On",
        "xyz.openbmc_project.State.Chassis.PowerState.Off"};
    for (const auto& state : chasisStates)
    {
        NsmFpgaPort fpgaPort(
            bus, name, type,
            "xyz.openbmc_project.State.Decorator.Health.HealthType.OK", state,
            associations, inventoryObjPath);
        EXPECT_EQ(fpgaPort.getType(), type);
    }
}

TEST(NsmFpgaPortInfo, Constructor)
{
    std::string name = "FpgaPortInfo0";
    std::string type = "NSM_PortInfo";
    std::string portType =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/portinfo0";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmFpgaPortInfo portInfo(name, type, portType, portProtocol, portInfoIntf);

    EXPECT_EQ(portInfo.getName(), name);
    EXPECT_EQ(portInfo.getType(), type);
}

TEST(NsmFpgaPortInfo, ConstructorWithDifferentPortTypes)
{
    std::string name = "FpgaPortInfo1";
    std::string type = "NSM_PortInfo";
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/portinfo1";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::string> portTypes = {
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.DownstreamPort"};
    for (const auto& portType : portTypes)
    {
        NsmFpgaPortInfo portInfo(name, type, portType, portProtocol,
                                 portInfoIntf);
        EXPECT_EQ(portInfo.getName(), name);
    }
}

TEST(NsmFpgaPortInfo, ConstructorWithDifferentProtocols)
{
    std::string name = "FpgaPortInfo2";
    std::string type = "NSM_PortInfo";
    std::string portType =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/portinfo2";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::string> protocols = {
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.Ethernet",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.NVLink"};
    for (const auto& protocol : protocols)
    {
        NsmFpgaPortInfo portInfo(name, type, portType, protocol, portInfoIntf);
        EXPECT_EQ(portInfo.getType(), type);
    }
}

TEST(NsmFpgaPortState, Constructor)
{
    std::string name = "FpgaPortState0";
    std::string type = "NSM_PortState";
    std::string linkStatus =
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/portstate0";

    NsmFpgaPortState portState(bus, name, type, linkStatus, inventoryObjPath);

    EXPECT_EQ(portState.getName(), name);
    EXPECT_EQ(portState.getType(), type);
}

TEST(NsmFpgaPortState, ConstructorWithDifferentLinkStates)
{
    std::string name = "FpgaPortState1";
    std::string type = "NSM_PortState";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/fpga/portstate1";

    std::vector<std::string> linkStates = {
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp",
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkDown",
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.NoLink"};
    for (const auto& linkStatus : linkStates)
    {
        NsmFpgaPortState portState(bus, name, type, linkStatus,
                                   inventoryObjPath);
        EXPECT_EQ(portState.getName(), name);
    }
}

TEST(NsmFpgaPortState, ConstructorWithMultipleInstances)
{
    std::string type = "NSM_PortState";
    std::string linkStatus =
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp";

    for (int i = 0; i < 3; i++)
    {
        std::string name = "FpgaPortState" + std::to_string(i);
        std::string inventoryObjPath =
            "/xyz/openbmc_project/inventory/system/fpga/portstate" +
            std::to_string(i);

        NsmFpgaPortState portState(bus, name, type, linkStatus,
                                   inventoryObjPath);
        EXPECT_EQ(portState.getName(), name);
        EXPECT_EQ(portState.getType(), type);
    }
}

struct NsmFpgaPortFactoryTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t fpgaUuid = "STATIC:3:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> fpga;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;

    NsmFpgaPortFactoryTest() :
        SensorManagerTest(devices),
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        objServer(std::make_shared<sdbusplus::asio::object_server>(systemBus))
    {
        ON_CALL(mockManager, getObjServer())
            .WillByDefault(ReturnRef(*objServer));

        fpga = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(fpgaUuid));
        EXPECT_NE(fpga, nullptr);
    }

    ~NsmFpgaPortFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/port0";
    const std::string name = "FpgaPort0";
    const std::string type = "NSM_FpgaPort";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ports/FPGA_Port_0";
    const std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";
    const std::string chasisState =
        "xyz.openbmc_project.State.Chassis.PowerState.On";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["UUID"] = fpgaUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["Type"] = type;
    propertyMap["Health"] = health;
    propertyMap["ChasisPowerState"] = chasisState;

    dbus::PropertyMap association = {
        {"Forward", "parent_fpga"},
        {"Backward", "port"},
        {"AbsolutePath", "/xyz/openbmc_project/inventory/system/fpga0"}};
    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, interface + ".Associations0");
    propertyMapAssociation0 = association;

    createNsmFpgaPortSensor(mockManager, interface, objPath);

    EXPECT_EQ(2, fpga->deviceSensors.size());
    auto port = std::dynamic_pointer_cast<NsmFpgaPort>(fpga->deviceSensors[1]);
    EXPECT_NE(nullptr, port);
    EXPECT_EQ(name, port->getName());
    EXPECT_EQ(type, port->getType());
}

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorInvalidUUID)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/port_invalid";
    const std::string name = "FpgaPortInvalid";
    const std::string type = "NSM_FpgaPort";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ports/FPGA_Port_Invalid";
    const uuid_t invalidUuid = "INVALID:UUID:DOES:NOT:EXIST";

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    propertyMap["Name"] = name;
    propertyMap["UUID"] = invalidUuid;
    propertyMap["InventoryObjPath"] = inventoryObjPath;
    propertyMap["Type"] = type;

    createNsmFpgaPortSensor(mockManager, interface, objPath);

    // Should have only the automatic first sensor
    EXPECT_EQ(1, fpga->deviceSensors.size());
}

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorPortInfoSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortInfo";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/portinfo_factory";
    const std::string name = "FpgaPortInfoFactory";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ports/FPGA_PortInfoFactory";

    // Base interface properties (read by coGetCachedBaseProperties)
    auto& baseMap = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_FpgaPort");
    baseMap["Name"] = name;
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};

    // Type-specific interface properties (read by coGetAllDbusProperty)
    auto& typeMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    typeMap["Type"] = std::string("NSM_PortInfo");
    typeMap["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    typeMap["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    typeMap["Priority"] = bool{false};

    createNsmFpgaPortSensor(mockManager, interface, objPath);

    // 1 auto device sensor + 1 NsmPCIePortInfoGroup1 via addSensor
    EXPECT_EQ(2, fpga->deviceSensors.size());
    auto portInfo = std::dynamic_pointer_cast<NsmPCIePortInfoGroup1>(
        fpga->deviceSensors[1]);
    EXPECT_NE(nullptr, portInfo);
    EXPECT_EQ(name, portInfo->getName());
    EXPECT_EQ(std::string("NSM_PortInfo"), portInfo->getType());
}

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorPortStateSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortState";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/portstate_factory";
    const std::string name = "FpgaPortStateFactory";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ports/FPGA_PortStateFactory";

    // Base interface properties
    auto& baseMap = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_FpgaPort");
    baseMap["Name"] = name;
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;

    // Type-specific interface properties
    auto& typeMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    typeMap["Type"] = std::string("NSM_PortState");
    typeMap["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");

    createNsmFpgaPortSensor(mockManager, interface, objPath);

    // 1 auto device sensor + 1 portStateSensor
    EXPECT_EQ(2, fpga->deviceSensors.size());
    auto portState =
        std::dynamic_pointer_cast<NsmFpgaPortState>(fpga->deviceSensors[1]);
    EXPECT_NE(nullptr, portState);
    EXPECT_EQ(name, portState->getName());
    EXPECT_EQ(std::string("NSM_PortState"), portState->getType());
}

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorPCIeSuccess)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PCIe";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/pcie_factory";
    const std::string name = "FpgaPCIeFactory";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ports/FPGA_PCIeFactory";

    // Base interface properties
    auto& baseMap = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_FpgaPort");
    baseMap["Name"] = name;
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};

    // Type-specific interface properties
    auto& typeMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    typeMap["Type"] = std::string("NSM_PCIe");
    typeMap["Priority"] = bool{false};

    createNsmFpgaPortSensor(mockManager, interface, objPath);

    // 1 auto device sensor + 3 PCIe ECC groups (Group2, Group3, Group4)
    // all go through addSensorBase which pushes to deviceSensors
    EXPECT_EQ(4, fpga->deviceSensors.size());
}

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorExceptionCatch)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortInfo";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/portinfo_bad";
    const std::string name = "FpgaPortInfoBad";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ports/FPGA_PortInfoBad";

    // Base interface properties (valid)
    auto& baseMap = utils::MockDbusAsync::propertyMap(
        objPath, "xyz.openbmc_project.Configuration.NSM_FpgaPort");
    baseMap["Name"] = name;
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};

    // Type-specific properties with invalid PortType to trigger exception
    // in NsmFpgaPortInfo constructor via convertPortTypeFromString
    auto& typeMap = utils::MockDbusAsync::propertyMap(objPath, interface);
    typeMap["Type"] = std::string("NSM_PortInfo");
    typeMap["PortType"] = std::string("invalid.PortType.NotInEnum");
    typeMap["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    typeMap["Priority"] = bool{false};

    // Exception caught inside createNsmFpgaPortSensor, should not propagate
    EXPECT_NO_THROW(createNsmFpgaPortSensor(mockManager, interface, objPath));

    // No new sensor added — exception was caught and co_return NSM_ERROR //

    EXPECT_EQ(1, fpga->deviceSensors.size());
}

TEST_F(NsmFpgaPortFactoryTest, CreateNsmFpgaPortSensorBasePropertiesNotFound)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort.PortInfo";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/fpga/portinfo_no_base";

    // Do NOT register the base (FPGA_PORT_INTERFACE) property map.
    // coGetCachedBaseProperties will return NSM_SW_ERROR (non-success),
    // exercising the early-return branch at line 82-84.

    createNsmFpgaPortSensor(mockManager, interface, objPath);

    // rc != NSM_SUCCESS → co_return rc; no sensor added
    EXPECT_EQ(1, fpga->deviceSensors.size());
}

// =============================================================================
// FALSE-branch coverage: count() checks in createNsmFpgaPortSensor
// =============================================================================

// NSM_FpgaPort + Health absent → health="" → convertHealthTypeFromString("")
// throws InvalidEnumString → caught in try/catch → no sensor added
TEST_F(NsmFpgaPortFactoryTest, NSMFpgaPort_MissingHealth_Throws_NoSensor)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string objPath = "/xyz/test/fpga/port_no_health";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/port_no_health";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Name"] = std::string("FpgaPort_NoHealth");
    pm["UUID"] = fpgaUuid;
    pm["InventoryObjPath"] = inventoryObjPath;
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["ChasisPowerState"] =
        std::string("xyz.openbmc_project.State.Chassis.PowerState.On");
    // "Health" intentionally omitted → FALSE branch → health="" → throws

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// NSM_FpgaPort + ChasisPowerState absent → chasisState="" →
// convertPowerStateFromString("") throws → caught → no sensor added
TEST_F(NsmFpgaPortFactoryTest, NSMFpgaPort_MissingChasisPowerState_Throws)
{
    const std::string interface =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string objPath = "/xyz/test/fpga/port_no_chasis";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/port_no_chasis";
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Name"] = std::string("FpgaPort_NoChasis");
    pm["UUID"] = fpgaUuid;
    pm["InventoryObjPath"] = inventoryObjPath;
    pm["Type"] = std::string("NSM_FpgaPort");
    pm["Health"] =
        std::string("xyz.openbmc_project.State.Decorator.Health.HealthType.OK");
    // "ChasisPowerState" omitted → FALSE branch → chasisState="" → throws

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// NSM_PortInfo + PortType absent → portType="" → convertPortTypeFromString("")
// throws → caught → no sensor added
TEST_F(NsmFpgaPortFactoryTest, NSMPortInfo_MissingPortType_Throws_NoSensor)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortInfo";
    const std::string objPath = "/xyz/test/fpga/portinfo_no_porttype";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/portinfo_no_porttype";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPortInfo_NoPortType");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = bool{false};
    // "PortType" omitted → portType="" → convertPortTypeFromString throws

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// NSM_PortInfo + PortProtocol absent → portProtocol="" →
// NsmFpgaPortInfo constructor throws → caught → no sensor added
TEST_F(NsmFpgaPortFactoryTest, NSMPortInfo_MissingPortProtocol_Throws_NoSensor)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortInfo";
    const std::string objPath = "/xyz/test/fpga/portinfo_no_portproto";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/portinfo_no_portproto";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPortInfo_NoProto");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["Priority"] = bool{false};
    // "PortProtocol" omitted → portProtocol="" → throws

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// NSM_PortInfo + Priority absent → priority=false (FALSE branch) →
// pcieECCGroup1 added to roundRobinSensors; portInfoSensor to deviceSensors
TEST_F(NsmFpgaPortFactoryTest, NSMPortInfo_MissingPriority_RoundRobinSensor)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortInfo";
    const std::string objPath = "/xyz/test/fpga/portinfo_no_priority";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/portinfo_no_priority";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPortInfo_NoPriority");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    // "Priority" omitted → priority=false → roundRobin for pcieECCGroup1

    const size_t rrBefore = fpga->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_GT(fpga->roundRobinSensors.size(), rrBefore);
}

// NSM_PortState + LinkStatus absent → linkStatus="" →
// convertLinkStatusTypeFromString("") throws → caught → no sensor added
TEST_F(NsmFpgaPortFactoryTest, NSMPortState_MissingLinkStatus_Throws_NoSensor)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortState";
    const std::string objPath = "/xyz/test/fpga/portstate_no_linkstatus";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/portstate_no_linkstatus";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPortState_NoLink");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortState");
    // "LinkStatus" omitted → linkStatus="" → convertLinkStatusTypeFromString
    // throws InvalidEnumString → caught → no sensor

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before, fpga->deviceSensors.size());
}

// NSM_PCIe + Priority absent → priority=false (FALSE branch) →
// pcieECCGroup2/3/4 added to roundRobinSensors
TEST_F(NsmFpgaPortFactoryTest, NSMPCIe_MissingPriority_RoundRobinSensors)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PCIe";
    const std::string objPath = "/xyz/test/fpga/pcie_no_priority";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/pcie_no_priority";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPCIe_NoPriority");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PCIe");
    // "Priority" omitted → priority=false → roundRobin for all PCIe groups

    const size_t rrBefore = fpga->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_GT(fpga->roundRobinSensors.size(), rrBefore);
}

// NSM_PortInfo + DeviceIndex absent from base → L164 FALSE → deviceIndex=0
TEST_F(NsmFpgaPortFactoryTest, NSMPortInfo_MissingDeviceIndex_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortInfo";
    const std::string objPath = "/xyz/test/fpga/portinfo_no_devidx";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/portinfo_no_devidx";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPortInfo_NoDevIdx");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    // "DeviceIndex" omitted → L164 FALSE → deviceIndex stays 0
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortInfo");
    pm["PortType"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort");
    pm["PortProtocol"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe");
    pm["Priority"] = bool{false};

    const size_t rrBefore = fpga->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_GT(fpga->roundRobinSensors.size(), rrBefore);
}

// NSM_PCIe + DeviceIndex absent from base → L205 FALSE → deviceIndex=0
TEST_F(NsmFpgaPortFactoryTest, NSMPCIe_MissingDeviceIndex_FalseBranch)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PCIe";
    const std::string objPath = "/xyz/test/fpga/pcie_no_devidx";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/pcie_no_devidx";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPCIe_NoDevIdx");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    // "DeviceIndex" omitted → L205 FALSE → deviceIndex stays 0
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PCIe");
    pm["Priority"] = bool{false};

    const size_t rrBefore = fpga->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_GT(fpga->roundRobinSensors.size(), rrBefore);
}

// Unknown type (not FpgaPort/PortInfo/PortState/PCIe) → L196 else-if FALSE →
// no sensor created; execution falls through the else-if chain
TEST_F(NsmFpgaPortFactoryTest, NSMUnknownType_NoSensor)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortInfo";
    const std::string objPath = "/xyz/test/fpga/unknown_type";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/unknown_type";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPort_Unknown");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    baseMap["DeviceIndex"] = uint64_t{0};
    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_Unknown"); // triggers FALSE for all else-ifs

    const size_t before = fpga->deviceSensors.size() +
                          fpga->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before,
              fpga->deviceSensors.size() + fpga->roundRobinSensors.size());
}

// ============================================================================
// FALSE-branch coverage: count() checks on allBaseIfaceProperties
// ============================================================================

// Name absent in base → L89 FALSE → name="" → sensors still created
TEST_F(NsmFpgaPortFactoryTest, Factory_MissingName_SensorsCreated)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortState";
    const std::string objPath = "/xyz/test/fpga/missing_name";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/missing_name_inv";
    // Name absent → L89 FALSE → name stays ""
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortState");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");

    const size_t before = fpga->deviceSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_GT(fpga->deviceSensors.size(), before);
}

// UUID absent in base → L94 FALSE → uuid="" → getNsmDeviceFromStaticUUID
// throws std::runtime_error → caught at L240 → co_return NSM_ERROR //

TEST_F(NsmFpgaPortFactoryTest, Factory_MissingUUID_CatchesException)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortState";
    const std::string objPath = "/xyz/test/fpga/missing_uuid";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/missing_uuid_inv";
    // UUID absent → L94 FALSE → uuid="" → throws → caught
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPort_NoUUID");
    baseMap["InventoryObjPath"] = inventoryObjPath;

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortState");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");

    // Exception caught inside factory, should not propagate
    EXPECT_NO_THROW(createNsmFpgaPortSensor(mockManager, interface, objPath));
}

// InventoryObjPath absent in base → L104 FALSE → inventoryObjPath="" →
// NsmFpgaPortState ctor with "" → throws SdBusError → caught at L240
TEST_F(NsmFpgaPortFactoryTest, Factory_MissingInventoryObjPath_CatchesException)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortState";
    const std::string objPath = "/xyz/test/fpga/missing_inv_path";
    // InventoryObjPath absent → L104 FALSE → inventoryObjPath="" → throws
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPort_NoInvPath");
    baseMap["UUID"] = fpgaUuid;
    // InventoryObjPath intentionally omitted

    auto& pm = utils::MockDbusAsync::propertyMap(objPath, interface);
    pm["Type"] = std::string("NSM_PortState");
    pm["LinkStatus"] = std::string(
        "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp");

    // Exception caught inside factory, should not propagate
    EXPECT_NO_THROW(createNsmFpgaPortSensor(mockManager, interface, objPath));
}

// Type absent in current iface → L99 FALSE → type="" → no type branch taken
// → factory returns NSM_SUCCESS without creating any sensor
TEST_F(NsmFpgaPortFactoryTest, Factory_MissingType_NoSensors)
{
    const std::string baseIntf =
        "xyz.openbmc_project.Configuration.NSM_FpgaPort";
    const std::string interface = baseIntf + ".PortState";
    const std::string objPath = "/xyz/test/fpga/missing_type";
    const std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/test/fpga/missing_type_inv";
    auto& baseMap = utils::MockDbusAsync::propertyMap(objPath, baseIntf);
    baseMap["Name"] = std::string("FpgaPort_NoType");
    baseMap["UUID"] = fpgaUuid;
    baseMap["InventoryObjPath"] = inventoryObjPath;
    // Type absent → L99 FALSE → type="" → no type branch taken
    (void)utils::MockDbusAsync::propertyMap(objPath, interface);

    const size_t before = fpga->deviceSensors.size() +
                          fpga->roundRobinSensors.size();
    createNsmFpgaPortSensor(mockManager, interface, objPath);
    EXPECT_EQ(before,
              fpga->deviceSensors.size() + fpga->roundRobinSensors.size());
}
