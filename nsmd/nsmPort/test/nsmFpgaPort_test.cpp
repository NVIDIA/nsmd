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

    NsmFpgaPortFactoryTest() : SensorManagerTest(devices)
    {
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
