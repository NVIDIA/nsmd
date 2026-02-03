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

#include "base.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "network-ports.h"

#include "utils.hpp"

#include <filesystem>

#define private public
#define protected public

#include "nsmInterface.hpp"
#include "nsmPCIeErrors.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPCIePort.hpp"
#include "nsmPort.hpp"
#include "nsmPortInfo.hpp"

namespace nsm
{
requester::Coroutine createNsmPortSensor(SensorManager& manager,
                                         const std::string& interface,
                                         const std::string& objPath,
                                         bool enableNetworkPortAddresses);
} // namespace nsm

TEST(NsmPortMetrics, GoodTest)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName("dummy_port");
    uint8_t portNum = 1;
    std::string type = "DummyType";
    uint8_t deviceType = 1;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/dummy_device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/dummy/dummy_device/Ports";
    std::vector<utils::Association> associations;
    std::shared_ptr<IBPortIntf> iBPortIntf =
        std::make_shared<IBPortIntf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                associations, parentObjPath, inventoryObjPath,
                                iBPortIntf, portMetricsOem2Intf,
                                portPacketCountersIntf);

    EXPECT_EQ(portTel.portName, pName);
    EXPECT_EQ(portTel.portNumber, portNum);
    EXPECT_EQ(portTel.iBPortIntf, iBPortIntf);
    EXPECT_NE(portTel.portMetricsOem2Intf, nullptr);
    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);

    std::vector<uint8_t> portData{
        0xFF, 0xFF, 0xFF, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }; /*for counter values, 8 bytes each*/
    struct nsm_port_counter_data portTelData = {};
    std::memcpy(&portTelData, portData.data(), sizeof(portData));

    portTel.updateCounterValues(&portTelData);

    EXPECT_EQ(portTel.iBPortIntf->rxPkts(), portTelData.port_rcv_pkts);
    // checking only first and last values for iBPortIntf
    EXPECT_EQ(portTel.iBPortIntf->txWait(), portTelData.xmit_wait);

    EXPECT_EQ(portTel.portMetricsOem2Intf->rxBytes(),
              portTelData.port_rcv_data);
    // checking only first and last values for portMetricsOem2Intf
    EXPECT_EQ(portTel.portMetricsOem2Intf->txBytes(),
              portTelData.port_xmit_data);
}

namespace nsm
{
requester::Coroutine createNsmPCIePort(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath);
};

using namespace nsm;

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

using sdbusplus::server::object_t;
using namespace sdbusplus::server::xyz::openbmc_project;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PortIntf = object_t<Inventory::Item::server::Port>;
using PortStateIntf = object_t<Inventory::Decorator::server::PortState>;
using HealthIntf = object_t<State::Decorator::server::Health>;

struct NsmPCIePortTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    eid_t eid = 0;
    uint8_t instanceId = 0;
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_PCIePort";
    const std::string name = "PCIe_0";
    const std::string objPath =
        chassisInventoryBasePath /
        "HGX_NVLinkManagementNIC_0/NetworkAdapters/NVLinkManagementNIC_0/Ports" /
        name;

    const uuid_t cx7Uuid = "STATIC:2:255:NSM_DEVICE_INSTANCE_NUMBER:255";
    std::shared_ptr<MockNsmDevice> cx7;

    NsmDeviceTable devices;

    NsmPCIePortTest() : SensorManagerTest(devices)
    {
        cx7 = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(cx7Uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(cx7, nullptr);
        EXPECT_EQ(NSM_DEV_ID_PCIE_BRIDGE, cx7->getDeviceType());
    }

    dbus::PropertyMap error = {
        {"UUID", "992b3ec1-e468-f145-8686-badbadbadbad"},
    };
    dbus::PropertyMap basic = {
        {"Name", "PCIe_0"},
        {"Type", "NSM_PCIePort"},
        {"InventoryObjPath", objPath},
        {"UUID", cx7Uuid},
        {"Health", "xyz.openbmc_project.State.Decorator.Health.HealthType.OK"},
        {"PortType",
         "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortType.UpstreamPort"},
        {"PortProtocol",
         "xyz.openbmc_project.Inventory.Decorator.PortInfo.PortProtocol.PCIe"},
        {"LinkState",
         "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStates.Enabled"},
        {"LinkStatus",
         "xyz.openbmc_project.Inventory.Decorator.PortState.LinkStatusType.LinkUp"},
    };

    dbus::PropertyMap associations[1] = {
        {
            {"Forward", "parent_device"},
            {"Backward", "all_states"},
            {"AbsolutePath",
             "/xyz/openbmc_project/inventory/system/chassis/HGX_NVLinkManagementNIC_0/NetworkAdapters/NVLinkManagementNIC_0"},
        },
    };
    const MapperServiceMap serviceMap = {
        {
            {
                "xyz.openbmc_project.NSM",
                {
                    basicIntfName + ".Associations0",
                },
            },
        },
    };
};

TEST_F(NsmPCIePortTest, badTestCreateDeviceSensors)
{
    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties with INVALID UUID that doesn't match any device
    const uuid_t invalidUuid =
        "a3b0bdf6-8661-4d8e-8268-0e59415f2076"; // From error collection
    propertyMap["InventoryObjPath"] = basic["InventoryObjPath"];
    propertyMap["UUID"] = invalidUuid;          // Invalid UUID as uuid_t type

    EXPECT_THROW_COROUTINE(
        createNsmPCIePort(mockManager, basicIntfName, objPath),
        std::runtime_error);
}

TEST_F(NsmPCIePortTest, goodTestCreateDeviceSensors)
{
    utils::MockDbusAsync::serviceMap() = serviceMap;

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);

    // Set up base properties that coGetCachedBaseProperties needs
    propertyMap["UUID"] = basic["UUID"];

    // Set up interface-specific properties
    propertyMap["InventoryObjPath"] = basic["InventoryObjPath"];
    propertyMap["Health"] = basic["Health"];
    propertyMap["PortType"] = basic["PortType"];
    propertyMap["PortProtocol"] = basic["PortProtocol"];
    propertyMap["LinkState"] = basic["LinkState"];
    propertyMap["LinkStatus"] = basic["LinkStatus"];

    auto& propertyMapAssociation0 = utils::MockDbusAsync::propertyMap(
        objPath, basicIntfName + ".Associations0");
    propertyMapAssociation0 = associations[0];

    createNsmPCIePort(mockManager, basicIntfName, objPath);

    EXPECT_EQ(0, cx7->prioritySensors.size());
    EXPECT_EQ(5, cx7->roundRobinSensors.size());
    EXPECT_EQ(9, cx7->deviceSensors.size());

    auto sensors = 1; // Skip msgTypes sensor added by initMsgTypesSensor()
    auto associationsObject =
        dynamic_pointer_cast<NsmPCIePort<AssociationDefinitionsInft>>(
            cx7->deviceSensors[sensors++]);
    auto healthObject = dynamic_pointer_cast<NsmPCIePort<HealthIntf>>(
        cx7->deviceSensors[sensors++]);
    auto portObject = dynamic_pointer_cast<NsmPCIePort<PortIntf>>(
        cx7->deviceSensors[sensors++]);
    auto portStateObject = dynamic_pointer_cast<NsmPCIePort<PortStateIntf>>(
        cx7->deviceSensors[sensors++]);
    auto pcieLinkSpeed =
        dynamic_pointer_cast<NsmPCIeLinkSpeed<NsmPortInfoIntf>>(
            cx7->deviceSensors[sensors++]);
    auto pcieErrorsGroup2 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7->deviceSensors[sensors++]);
    auto pcieErrorsGroup3 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7->deviceSensors[sensors++]);
    auto pcieErrorsGroup4 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7->deviceSensors[sensors++]);

    EXPECT_EQ(sensors, cx7->deviceSensors.size());
    EXPECT_NE(nullptr, associationsObject);
    EXPECT_NE(nullptr, healthObject);
    EXPECT_NE(nullptr, portObject);
    EXPECT_NE(nullptr, portStateObject);
    EXPECT_NE(nullptr, pcieLinkSpeed);
    EXPECT_NE(nullptr, pcieErrorsGroup2);
    EXPECT_NE(nullptr, pcieErrorsGroup3);
    EXPECT_NE(nullptr, pcieErrorsGroup4);

    EXPECT_EQ(1, associationsObject->invoke(pdiMethod(associations)).size());
    EXPECT_EQ(std::get<std::string>(basic["Health"]),
              HealthIntf::convertHealthTypeToString(
                  healthObject->invoke(pdiMethod(health))));
    EXPECT_EQ(std::get<std::string>(basic["PortType"]),
              PortInfoIntf::convertPortTypeToString(
                  pcieLinkSpeed->invoke(pdiMethod(type))));
    EXPECT_EQ(std::get<std::string>(basic["PortProtocol"]),
              PortInfoIntf::convertPortProtocolToString(
                  pcieLinkSpeed->invoke(pdiMethod(protocol))));
    EXPECT_EQ(std::get<std::string>(basic["LinkState"]),
              PortStateIntf::convertLinkStatesToString(
                  portStateObject->invoke(pdiMethod(linkState))));
    EXPECT_EQ(std::get<std::string>(basic["LinkStatus"]),
              PortStateIntf::convertLinkStatusTypeToString(
                  portStateObject->invoke(pdiMethod(linkStatus))));
    EXPECT_EQ(GROUP_ID_2, pcieErrorsGroup2->groupId);
    EXPECT_EQ(GROUP_ID_3, pcieErrorsGroup3->groupId);
    EXPECT_EQ(GROUP_ID_4, pcieErrorsGroup4->groupId);

    EXPECT_CALL(*cx7, sensorIO)
        .Times(cx7->roundRobinSensors.size())
        .WillRepeatedly(mockSensorIO(NSM_SUCCESS));
    for (size_t i = 0; i < cx7->roundRobinSensors.size(); i++)
    {
        cx7->roundRobinSensors[i]->update(cx7).detach();
    }
}

TEST(NsmPortMetrics, TestNullCounters)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName("test_port");
    uint8_t portNum = 2;
    std::string type = "TestType";
    uint8_t deviceType = 2;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/test/device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/test/device/Ports";
    std::vector<utils::Association> associations;
    std::shared_ptr<IBPortIntf> iBPortIntf =
        std::make_shared<IBPortIntf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());
    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                associations, parentObjPath, inventoryObjPath,
                                iBPortIntf, portMetricsOem2Intf,
                                portPacketCountersIntf);

    // Test with null pointer - should not crash
    portTel.updateCounterValues(nullptr);

    // Counters should remain at initial values
    EXPECT_EQ(portTel.iBPortIntf->rxPkts(), 0);
}

TEST(NsmPortMetrics, TestEmptyAssociations)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string pName("empty_assoc_port");
    uint8_t portNum = 3;
    std::string type = "EmptyType";
    uint8_t deviceType = 3;
    std::string parentObjPath =
        "/xyz/openbmc_project/inventory/system/empty/device";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/empty/device/Ports";

    // Empty associations vector
    std::vector<utils::Association> emptyAssociations;
    std::shared_ptr<IBPortIntf> iBPortIntf =
        std::make_shared<IBPortIntf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortMetricsOem2Intf> portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    std::shared_ptr<PortPacketCountersIntf> portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                emptyAssociations, parentObjPath,
                                inventoryObjPath, iBPortIntf,
                                portMetricsOem2Intf, portPacketCountersIntf);

    EXPECT_EQ(portTel.portName, pName);
    EXPECT_EQ(portTel.portNumber, portNum);
    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);
}

TEST(NsmPCIeErrors, Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PCIeErrors";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    uint8_t deviceIndex = 1;
    uint8_t groupId = GROUP_ID_2;

    nsm::NsmPCIeErrors errors(provider, deviceIndex, groupId);

    EXPECT_EQ(errors.getName(), name);
    EXPECT_EQ(errors.getType(), type);
}

TEST(NsmPCIeErrors, GenRequestMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PCIeErrors";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    uint8_t deviceIndex = 2;
    uint8_t groupId = GROUP_ID_3;

    nsm::NsmPCIeErrors errors(provider, deviceIndex, groupId);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = errors.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST(NsmPCIeErrors, HandleResponseMsg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::filesystem::path path = "/xyz/openbmc_project/inventory/test/device";
    std::string name = "PCIeErrors";
    std::string type = "NSM_PCIeErrors";

    auto pcieEccIntf = std::make_shared<PCIeEccIntf>(bus, path.c_str());
    NsmInterfaceProvider<PCIeEccIntf> provider(name, type, path, pcieEccIntf);

    uint8_t deviceIndex = 1;
    uint8_t groupId = GROUP_ID_2;

    nsm::NsmPCIeErrors errors(provider, deviceIndex, groupId);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_scalar_group_telemetry_v1_resp) +
            sizeof(nsm_query_scalar_group_telemetry_group_2),
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    nsm_query_scalar_group_telemetry_group_2 data = {};
    data.correctable_errors = 5;
    data.non_fatal_errors = 2;
    data.fatal_errors = 1;

    uint8_t rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, cc, reason_code, &data, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = errors.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify values were set
    EXPECT_EQ(pcieEccIntf->ceCount(), 5);
    EXPECT_EQ(pcieEccIntf->nonfeCount(), 2);
    EXPECT_EQ(pcieEccIntf->feCount(), 1);
}

TEST_F(NsmPCIePortTest, TearDown)
{
    devices.clear();
    ::testing::Mock::VerifyAndClearExpectations(&mockManager);
}

struct NsmPortSensorCreateTestFixture :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string basicIntfName =
        "xyz.openbmc_project.Configuration.NSM_NVLink";
    const std::string objPath =
        "/xyz/openbmc_project/inventory/system/gpu/port_create";
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:20";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmPortSensorCreateTestFixture() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmPortSensorCreateTestFixture()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmPortSensorCreateTestFixture, goodTestCreatePortSensor)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink")},
        {"UUID", gpuUuid},
        {"Count", uint64_t(4)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0")},
        {"Priority", false},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    // Mock empty topology
    auto& serviceMap = utils::MockDbusAsync::serviceMap();
    serviceMap.clear();

    createNsmPortSensor(mockManager, basicIntfName, objPath, false);

    // Expect multiple sensors per port (Status, Characteristics, Metrics)
    EXPECT_GE(gpu->deviceSensors.size(), 4);
}

TEST_F(NsmPortSensorCreateTestFixture, badTestMissingUUID)
{
    using namespace nsm;

    dbus::PropertyMap properties = {
        {"Name", std::string("NVLink")},
        {"Count", uint64_t(4)},
        {"DeviceType", uint64_t(NSM_DEV_ID_GPU)},
        {"ParentObjPath",
         std::string("/xyz/openbmc_project/inventory/system/gpu0")},
    };

    auto& propertyMap = utils::MockDbusAsync::propertyMap(objPath,
                                                          basicIntfName);
    propertyMap = properties;

    EXPECT_THROW_COROUTINE(
        createNsmPortSensor(mockManager, basicIntfName, objPath, false),
        std::runtime_error);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusUpdate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_0";
    uint8_t portNum = 1;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/nvlink_0";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Mock port status response - LinkUp
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());

    auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_PORTSTATE_UP,
                                            NSM_PORTSTATUS_ENABLED, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));

    portStatus.update(gpu);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortCharacteristicsUpdate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_1";
    uint8_t portNum = 2;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/nvlink_1";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    // Mock port characteristics response
    Response portCharResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

    nsm_port_characteristics_data data = {};
    data.nv_port_line_rate_mbps = 100000;   // 100 Gbps
    data.nv_port_data_rate_kbps = 90000000; // 90 Gbps
    data.status_lane_info = 0x04;           // 4 lanes
    data.port_status.port_down_reason_code =
        NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN;

    auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // handleResponseMsg is called directly, not through sensorIO
    auto result = portChar.handleResponseMsg(msg, portCharResp.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortMetricsUpdate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Metrics";
    uint8_t portNum = 4;
    std::string type = "NSM_NVLink";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath = "/xyz/openbmc_project/inventory/system/gpu3";
    std::string inventoryObjPath = parentObjPath + "/Ports/nvlink_metrics";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics portMetrics(bus, portName, portNum, type, deviceType,
                               associations, parentObjPath, inventoryObjPath,
                               iBPortIntf, portMetricsOem2Intf,
                               portPacketCountersIntf);

    // Mock port telemetry counter response
    Response portTelResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             PORT_COUNTER_TELEMETRY_MAX_DATA_SIZE,
                         0);
    auto msg = reinterpret_cast<nsm_msg*>(portTelResp.data());

    nsm_port_counter_data data = {};
    data.supported_counter.port_rcv_pkts = 1;
    data.port_rcv_pkts = 1000;
    data.supported_counter.port_xmit_pkts = 1;
    data.port_xmit_pkts = 2000;
    data.supported_counter.port_rcv_errors = 1;
    data.port_rcv_errors = 5;

    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = portMetrics.handleResponseMsg(msg, portTelResp.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);

    EXPECT_EQ(iBPortIntf->rxPkts(), 1000);
    EXPECT_EQ(iBPortIntf->txPkts(), 2000);
    EXPECT_EQ(iBPortIntf->rxErrors(), 5);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusVariousStates)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_States";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/states";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    // Test various port states
    std::vector<std::pair<uint8_t, uint8_t>> portStates = {
        {NSM_PORTSTATE_DOWN, NSM_PORTSTATUS_DISABLED},
        {NSM_PORTSTATE_UP, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_TRAINING, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_TRAINING_FAILURE, NSM_PORTSTATUS_ENABLED},
        {NSM_PORTSTATE_POLLING, NSM_PORTSTATUS_ENABLED}};

    for (size_t i = 0; i < portStates.size(); i++)
    {
        uint8_t portNum = i + 1;
        NsmPortStatus portStatus(bus, portName, portNum, "NSM_NVLink",
                                 portMetricsOem3Intf, inventoryObjPath);

        Response portStatusResp(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());

        auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                                portStates[i].first,
                                                portStates[i].second, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        EXPECT_CALL(*gpu, sensorIO)
            .WillOnce(mockSensorIO(portStatusResp, Response{}));

        portStatus.update(gpu);
    }
}

TEST_F(NsmPortSensorCreateTestFixture, testPortCharacteristicsUpdateError)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Error";
    uint8_t portNum = 5;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/error";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    // Mock error response
    Response portCharResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

    nsm_port_characteristics_data data = {};
    auto rc = encode_query_port_characteristics_resp(0, NSM_ERROR, 0x5678,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = portChar.handleResponseMsg(msg, portCharResp.size());
    EXPECT_EQ(result, NSM_ERROR);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortMetricsCounterValues)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_Counters";
    uint8_t portNum = 6;
    std::string type = "NSM_NVLink";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath = "/xyz/openbmc_project/inventory/system/gpu4";
    std::string inventoryObjPath = parentObjPath + "/Ports/counters";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics portMetrics(bus, portName, portNum, type, deviceType,
                               associations, parentObjPath, inventoryObjPath,
                               iBPortIntf, portMetricsOem2Intf,
                               portPacketCountersIntf);

    // Need larger buffer for port counter data (contains many uint64_t fields)
    Response portTelResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                             sizeof(nsm_port_counter_data),
                         0);
    auto msg = reinterpret_cast<nsm_msg*>(portTelResp.data());

    nsm_port_counter_data data = {};
    // Set all counters
    data.supported_counter.port_rcv_pkts = 1;
    data.port_rcv_pkts = 5000;
    data.supported_counter.port_xmit_pkts = 1;
    data.port_xmit_pkts = 6000;
    data.supported_counter.port_malformed_pkts = 1;
    data.port_malformed_pkts = 10;
    data.supported_counter.vl15_dropped = 1;
    data.vl15_dropped = 2;
    data.supported_counter.port_rcv_errors = 1;
    data.port_rcv_errors = 15;
    data.supported_counter.port_xmit_discard = 1;
    data.port_xmit_discard = 8;
    data.supported_counter.symbol_ber = 1;
    data.symbol_ber = 0x12345678;
    data.supported_counter.link_error_recovery_counter = 1;
    data.link_error_recovery_counter = 3;
    data.supported_counter.link_downed_counter = 1;
    data.link_downed_counter = 1;

    auto rc = encode_get_port_telemetry_counter_resp(0, NSM_SUCCESS, ERR_NULL,
                                                     &data, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto result = portMetrics.handleResponseMsg(msg, portTelResp.size());
    EXPECT_EQ(result, NSM_SW_SUCCESS);

    EXPECT_EQ(iBPortIntf->rxPkts(), 5000);
    EXPECT_EQ(iBPortIntf->txPkts(), 6000);
    EXPECT_EQ(iBPortIntf->malformedPkts(), 10);
    EXPECT_EQ(iBPortIntf->vL15DroppedPkts(), 2);
    EXPECT_EQ(iBPortIntf->rxErrors(), 15);
    EXPECT_EQ(iBPortIntf->txDiscardPkts(), 8);
    EXPECT_EQ(iBPortIntf->linkErrorRecoveryCounter(), 3);
    EXPECT_EQ(iBPortIntf->linkDownCount(), 1);
}

TEST_F(NsmPortSensorCreateTestFixture, testGetBitErrorRate)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_BER";
    uint8_t portNum = 7;
    std::string type = "NSM_NVLink";
    uint8_t deviceType = NSM_DEV_ID_GPU;
    std::string parentObjPath = "/xyz/openbmc_project/inventory/system/gpu5";
    std::string inventoryObjPath = parentObjPath + "/Ports/ber";
    std::vector<utils::Association> associations;

    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    NsmPortMetrics portMetrics(bus, portName, portNum, type, deviceType,
                               associations, parentObjPath, inventoryObjPath,
                               iBPortIntf, portMetricsOem2Intf,
                               portPacketCountersIntf);

    // Test BER calculation with different values
    uint64_t berValue = 0x0A0305; // magnitude=10, coef_float=3, coef=5
    double ber = portMetrics.getBitErrorRate(berValue);
    EXPECT_GT(ber, 0.0);

    // Test with zero value
    ber = portMetrics.getBitErrorRate(0);
    EXPECT_EQ(ber, 0.0);
}

TEST_F(NsmPortSensorCreateTestFixture, testPortStatusDownLockWithRuntimeError)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DownLock";
    uint8_t portNum = 8;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/downlock";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    // Mock port status response - DOWN_LOCK state
    Response portStatusResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(portStatusResp.data());

    auto rc = encode_query_port_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                            NSM_PORTSTATE_DOWN_LOCK,
                                            NSM_PORTSTATUS_ENABLED, msg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Mock port characteristics response with error (use encode_reason_code
    // directly)
    Response portCharResp((sizeof(struct nsm_msg_hdr) +
                           sizeof(struct nsm_common_non_success_resp)),
                          0);
    auto charMsg = reinterpret_cast<nsm_msg*>(portCharResp.data());

    rc = encode_reason_code(NSM_ERROR, 0x1234, NSM_QUERY_PORT_CHARACTERISTICS,
                            charMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    testing::InSequence seq;
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portStatusResp, Response{}));
    EXPECT_CALL(*gpu, sensorIO)
        .WillOnce(mockSensorIO(portCharResp, Response{}));

    auto result = portStatus.update(gpu);
    // Force coroutine to complete by accessing data
    auto returnCode = result.data();
    EXPECT_EQ(returnCode, NSM_SW_SUCCESS);

    // Runtime error should be set after port characteristics error
    EXPECT_TRUE(portMetricsOem3Intf->runtimeError());
}

TEST_F(NsmPortSensorCreateTestFixture,
       testPortCharacteristicsLinkDownReasonCodes)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_LinkDown";
    uint8_t portNum = 9;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/linkdown";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());
    auto iBPortIntf = std::make_shared<IBPortIntf>(bus,
                                                   inventoryObjPath.c_str());

    NsmPortCharacteristics portChar(bus, portName, portNum, type,
                                    portMetricsOem3Intf, iBPortIntf,
                                    inventoryObjPath);

    // Test various link down reason codes
    std::vector<uint32_t> reasonCodes = {
        NSM_PORT_DOWN_REASON_CODE_NO_LINK_DOWN,
        NSM_PORT_DOWN_REASON_CODE_HI_SER_BER,
        NSM_PORT_DOWN_REASON_CODE_BLOCK_LOCK_LOSS,
        NSM_PORT_DOWN_REASON_CODE_ALIGNMENT_LOSS,
        NSM_PORT_DOWN_REASON_CODE_FEC_SYNC_LOSS,
        NSM_PORT_DOWN_REASON_CODE_CABLE_UNPLUGGED,
        NSM_PORT_DOWN_REASON_CODE_THERMAL_SHUTDOWN};

    for (auto reasonCode : reasonCodes)
    {
        Response portCharResp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_query_port_characteristics_resp),
                              0);
        auto msg = reinterpret_cast<nsm_msg*>(portCharResp.data());

        nsm_port_characteristics_data data = {};
        data.nv_port_line_rate_mbps = 50000;
        data.nv_port_data_rate_kbps = 45000000;
        data.status_lane_info = 0x02;
        data.port_status.port_down_reason_code = reasonCode;

        auto rc = encode_query_port_characteristics_resp(0, NSM_SUCCESS,
                                                         ERR_NULL, &data, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);

        auto result = portChar.handleResponseMsg(msg, portCharResp.size());
        EXPECT_EQ(result, NSM_SW_SUCCESS);
    }
}

TEST_F(NsmPortSensorCreateTestFixture, testNetworkAddressAggregatorEthernet)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_Eth";
    std::string type = "NSM_Port";
    std::string objPath = "/xyz/openbmc_project/inventory/system/port_eth";
    std::string nodeGuidObjPath = objPath + "/Infiniband_Node_Guid";
    std::string ethernetMacAddressObjPath = objPath + "/Ethernet_MAC_Address";
    std::string permanentMacAddressObjPath = objPath + "/Permanent_MAC_Address";
    uint16_t portNumber = 1;

    NsmNetworkAddressAggregator networkAddr(
        bus, portName, type, objPath, nodeGuidObjPath,
        ethernetMacAddressObjPath, permanentMacAddressObjPath, portNumber);

    EXPECT_EQ(networkAddr.getName(), portName);
    EXPECT_EQ(networkAddr.portNumber, portNumber);
}

TEST_F(NsmPortSensorCreateTestFixture, testGetPortECCCountersRequest)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "Port_ECC";
    std::string type = "NSM_Port";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/port_ecc";
    uint8_t portNumber = 2;

    NsmGetPortECCCounters eccCounters(bus, portName, type, inventoryObjPath,
                                      portNumber);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = eccCounters.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req));
}

TEST_F(NsmPortSensorCreateTestFixture, testEthPortTelemetryAggregator)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "EthPort";
    uint16_t portNumber = 3;
    std::string type = "NSM_EthPort";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/ethport";

    auto portMetricsOem2Intf =
        std::make_shared<PortMetricsOem2Intf>(bus, inventoryObjPath.c_str());
    auto portPacketCountersIntf =
        std::make_shared<PortPacketCountersIntf>(bus, inventoryObjPath.c_str());

    EthPortTelemetryAggregator ethPort(bus, portName, portNumber, type,
                                       inventoryObjPath, portMetricsOem2Intf,
                                       portPacketCountersIntf);

    EXPECT_EQ(ethPort.getName(), portName);
    EXPECT_EQ(ethPort.portNumber, portNumber);
    EXPECT_NE(ethPort.ethPortIntf, nullptr);
}

TEST_F(NsmPortSensorCreateTestFixture, badTestPortStatusUpdateDecodeError)
{
    using namespace nsm;

    auto& bus = utils::DBusHandler::getBus();
    std::string portName = "NVLink_DecodeErr";
    uint8_t portNum = 10;
    std::string type = "NSM_NVLink";
    std::string inventoryObjPath =
        "/xyz/openbmc_project/inventory/system/gpu/ports/decodeerr";

    auto portMetricsOem3Intf =
        std::make_shared<PortMetricsOem3Intf>(bus, inventoryObjPath.c_str());

    NsmPortStatus portStatus(bus, portName, portNum, type, portMetricsOem3Intf,
                             inventoryObjPath);

    auto mockGpu = std::make_shared<MockNsmDevice>();

    // Mock invalid response with minimal valid size
    Response badResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + sizeof(uint16_t), 0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    encode_reason_code(NSM_ERROR, 0x9999, NSM_QUERY_PORT_STATUS, badMsg);

    EXPECT_CALL(*mockGpu, sensorIO).WillOnce(mockSensorIO(badResp, Response{}));

    portStatus.update(mockGpu);
}
