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

#include "nsmGpuPciePort.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

using namespace nsm;

namespace nsm
{
requester::Coroutine createNsmGpuPcieSensor(SensorManager& manager,
                                            const std::string& interface,
                                            const std::string& objPath);

}; // namespace nsm

auto bus = sdbusplus::bus::new_default();

TEST(NsmGpuPciePort, Constructor)
{
    std::string name = "GpuPciePort0";
    std::string type = "NSM_GPU_PCIe";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK";
    std::string chasisState = "xyz.openbmc_project.State.Chassis.PowerState.On";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port0";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_gpu", "port", "/xyz/openbmc_project/inventory/system/gpu0"});

    NsmGpuPciePort gpuPort(bus, name, type, health, chasisState, associations,
                           inventoryObjPath);

    EXPECT_EQ(gpuPort.getName(), name);
    EXPECT_EQ(gpuPort.getType(), type);
}

TEST(NsmGpuPciePort, ConstructorWithMultipleAssociations)
{
    std::string name = "GpuPciePort1";
    std::string type = "NSM_GPU_PCIe";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning";
    std::string chasisState =
        "xyz.openbmc_project.State.Chassis.PowerState.Off";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port1";

    std::vector<utils::Association> associations;
    associations.push_back(
        {"parent_gpu", "port", "/xyz/openbmc_project/inventory/system/gpu1"});
    associations.push_back({"connected_device", "port",
                            "/xyz/openbmc_project/inventory/system/device1"});

    NsmGpuPciePort gpuPort(bus, name, type, health, chasisState, associations,
                           inventoryObjPath);

    EXPECT_EQ(gpuPort.getName(), name);
    EXPECT_EQ(gpuPort.getType(), type);
}

TEST(NsmGpuPciePort, ConstructorWithEmptyAssociations)
{
    std::string name = "GpuPciePort2";
    std::string type = "NSM_GPU_PCIe";
    std::string health =
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical";
    std::string chasisState = "xyz.openbmc_project.State.Chassis.PowerState.On";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port2";

    std::vector<utils::Association> associations; // Empty

    NsmGpuPciePort gpuPort(bus, name, type, health, chasisState, associations,
                           inventoryObjPath);

    EXPECT_EQ(gpuPort.getName(), name);
    EXPECT_EQ(gpuPort.getType(), type);
}

TEST(NsmGpuPciePort, ConstructorWithDifferentHealthStates)
{
    std::string name = "GpuPciePort3";
    std::string type = "NSM_GPU_PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port3";
    std::vector<utils::Association> associations;

    std::vector<std::string> healthStates = {
        "xyz.openbmc_project.State.Decorator.Health.HealthType.OK",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Warning",
        "xyz.openbmc_project.State.Decorator.Health.HealthType.Critical"};
    for (const auto& health : healthStates)
    {
        NsmGpuPciePort gpuPort(
            bus, name, type, health,
            "xyz.openbmc_project.State.Chassis.PowerState.On", associations,
            inventoryObjPath);
        EXPECT_EQ(gpuPort.getName(), name);
    }
}

TEST(NsmGpuPciePort, ConstructorWithDifferentChasisStates)
{
    std::string name = "GpuPciePort4";
    std::string type = "NSM_GPU_PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/port4";
    std::vector<utils::Association> associations;

    std::vector<std::string> chasisStates = {
        "xyz.openbmc_project.State.Chassis.PowerState.On",
        "xyz.openbmc_project.State.Chassis.PowerState.Off"};
    for (const auto& state : chasisStates)
    {
        NsmGpuPciePort gpuPort(
            bus, name, type,
            "xyz.openbmc_project.State.Decorator.Health.HealthType.OK", state,
            associations, inventoryObjPath);
        EXPECT_EQ(gpuPort.getType(), type);
    }
}

TEST(NsmGpuPciePortInfo, Constructor)
{
    std::string name = "GpuPciePortInfo0";
    std::string type = "NSM_PortInfo";
    std::string portType =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/portinfo0";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    NsmGpuPciePortInfo portInfo(name, type, portType, portProtocol,
                                portInfoIntf);

    EXPECT_EQ(portInfo.getName(), name);
    EXPECT_EQ(portInfo.getType(), type);
}

TEST(NsmGpuPciePortInfo, ConstructorWithDifferentPortTypes)
{
    std::string name = "GpuPciePortInfo1";
    std::string type = "NSM_PortInfo";
    std::string portProtocol =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/portinfo1";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::string> portTypes = {
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.DownstreamPort"};
    for (const auto& portType : portTypes)
    {
        NsmGpuPciePortInfo portInfo(name, type, portType, portProtocol,
                                    portInfoIntf);
        EXPECT_EQ(portInfo.getName(), name);
    }
}

TEST(NsmGpuPciePortInfo, ConstructorWithDifferentProtocols)
{
    std::string name = "GpuPciePortInfo2";
    std::string type = "NSM_PortInfo";
    std::string portType =
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/pcie/portinfo2";

    auto portInfoIntf =
        std::make_shared<PortInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::string> protocols = {
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.Ethernet",
        "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.NVLink"};
    for (const auto& protocol : protocols)
    {
        NsmGpuPciePortInfo portInfo(name, type, portType, protocol,
                                    portInfoIntf);
        EXPECT_EQ(portInfo.getType(), type);
    }
}
