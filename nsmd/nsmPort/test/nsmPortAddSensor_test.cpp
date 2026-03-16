/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Coverage targets:
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeECCGroup1> (nsmDevice.hpp)
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeECCGroup2>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeECCGroup3>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeECCGroup4>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeECCGroup5>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeECCGroup8>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPCIeLaneManager>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmPort>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmGetPortECCCounters>
 *   - void nsm::NsmDevice::addSensor<nsm::NsmNetworkAddressAggregator>
 *   - void nsm::NsmDevice::addSensor<nsm::EthPortTelemetryAggregator>
 *
 * Each test creates a concrete sensor of the target type and calls
 * device->addSensor(sensor, PollingType::RoundRobin) to instantiate the
 * corresponding addSensor<T> template in nsmDevice.hpp.
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "network-ports.h"
#include "pci-links.h"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPort.hpp"
#include "nsmRetimerPort.hpp"

using namespace nsm;

static auto& bus = utils::DBusHandler::getBus();

static const std::string objPath =
    "/xyz/openbmc_project/inventory/system/port_addsensor";

// ============================================================================
// Fixture: uses DBusTest + SensorManagerTest for mock D-Bus bus access
// ============================================================================

struct NsmPortAddSensorTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;

    NsmPortAddSensorTest() : SensorManagerTest(devices) {}

    ~NsmPortAddSensorTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<MockNsmDevice> makeDevice()
    {
        return std::make_shared<MockNsmDevice>(NSM_DEV_ID_PCIE_BRIDGE, 0,
                                               "MCTP_EID", "12", 0);
    }
};

// ============================================================================
// addSensor<NsmPCIeECCGroup1>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeECCGroup1)
{
    auto device = makeDevice();
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus, objPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeECCGroup1>(
        "group1", "NSM_PCIeRetimer", objPath, portInfoIntf, portWidthIntf,
        uint8_t(1));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPCIeECCGroup2>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeECCGroup2)
{
    auto device = makeDevice();
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeECCGroup2>(
        "group2", "NSM_PCIeRetimer", objPath, pcieEccIntf, uint8_t(1));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPCIeECCGroup3>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeECCGroup3)
{
    auto device = makeDevice();
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeECCGroup3>(
        "group3", "NSM_PCIeRetimer", objPath, pcieEccIntf, uint8_t(1));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPCIeECCGroup4>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeECCGroup4)
{
    auto device = makeDevice();
    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeECCGroup4>(
        "group4", "NSM_PCIeRetimer", objPath, pcieEccIntf, uint8_t(1));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPCIeECCGroup5>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeECCGroup5)
{
    auto device = makeDevice();
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeECCGroup5>(
        "group5", "NSM_PCIeRetimer", objPath, portMetricsOem2Intf, uint8_t(1));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPCIeECCGroup8>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeECCGroup8)
{
    auto device = makeDevice();
    auto laneErrorIntf = std::make_shared<LaneErrorIntf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeECCGroup8>(
        "group8", "NSM_PCIeRetimer", laneErrorIntf, uint8_t(1), objPath);
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPCIeLaneManager>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPCIeLaneManager)
{
    auto device = makeDevice();
    auto portInfoIntf = std::make_shared<PortInfoIntf>(bus, objPath.c_str());
    auto portWidthIntf = std::make_shared<PortWidthIntf>(bus, objPath.c_str());
    auto sensor = std::make_shared<NsmPCIeLaneManager>(
        "laneManager", "NSM_PCIeRetimer", objPath, portInfoIntf, portWidthIntf,
        NSM_PORT_TYPE_UPSTREAM, uint8_t(0), uint8_t(1));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmPort>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmPort)
{
    auto device = makeDevice();
    std::string portName = "retimer_port_as";
    std::string type = "NSM_PCIeRetimer";
    std::vector<utils::Association> associations;
    auto sensor = std::make_shared<NsmPort>(bus, portName, type, associations,
                                            objPath);
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmGetPortECCCounters>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmGetPortECCCounters)
{
    auto device = makeDevice();
    auto sensor = std::make_shared<NsmGetPortECCCounters>(
        bus, "portECC", "NSM_Port", objPath, uint8_t(0));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<NsmNetworkAddressAggregator>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorNsmNetworkAddressAggregator)
{
    auto device = makeDevice();
    const std::string nodeGuidPath = objPath + "/node_guid";
    const std::string macPath = objPath + "/mac";
    const std::string permMacPath = objPath + "/perm_mac";
    auto sensor = std::make_shared<NsmNetworkAddressAggregator>(
        bus, "netAddr", "NSM_Port", objPath, nodeGuidPath, macPath, permMacPath,
        uint16_t(0));
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}

// ============================================================================
// addSensor<EthPortTelemetryAggregator>
// ============================================================================

TEST_F(NsmPortAddSensorTest, AddSensorEthPortTelemetryAggregator)
{
    auto device = makeDevice();
    std::string portName = "EthPort_AS";
    std::string type = "NSM_EthPort";
    std::string ethObjPath = objPath + "/eth";
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, ethObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, ethObjPath.c_str());
    auto sensor = std::make_shared<EthPortTelemetryAggregator>(
        bus, portName, uint16_t(1), type, ethObjPath, portMetricsOem2Intf,
        portPacketCountersIntf);
    size_t before = device->deviceSensors.size();
    device->addSensor(sensor, PollingType::RoundRobin);
    EXPECT_GT(device->deviceSensors.size(), before);
}
