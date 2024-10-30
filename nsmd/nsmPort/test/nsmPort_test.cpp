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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "network-ports.h"

#include "utils.hpp"

#define private public
#define protected public

#include "nsmPCIeErrors.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPCIePort.hpp"
#include "nsmPort.hpp"
#include "nsmPortInfo.hpp"

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

    nsm::NsmPortMetrics portTel(bus, pName, portNum, type, deviceType,
                                associations, parentObjPath, inventoryObjPath);

    EXPECT_EQ(portTel.portName, pName);
    EXPECT_EQ(portTel.portNumber, portNum);
    EXPECT_NE(portTel.iBPortIntf, nullptr);
    EXPECT_NE(portTel.portMetricsOem2Intf, nullptr);
    EXPECT_NE(portTel.associationDefinitionsIntf, nullptr);

    std::vector<uint8_t> portData{
        0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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
        0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    }; /*for counter values, 8 bytes each*/
    auto portTelData =
        reinterpret_cast<nsm_port_counter_data*>(portData.data());

    portTel.updateCounterValues(portTelData);

    EXPECT_EQ(portTel.iBPortIntf->rxPkts(), portTelData->port_rcv_pkts);
    // checking only first and last values for iBPortIntf
    EXPECT_EQ(portTel.iBPortIntf->txWait(), portTelData->xmit_wait);

    EXPECT_EQ(portTel.portMetricsOem2Intf->rxBytes(),
              portTelData->port_rcv_data);
    // checking only first and last values for portMetricsOem2Intf
    EXPECT_EQ(portTel.portMetricsOem2Intf->txBytes(),
              portTelData->port_xmit_data);
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

    const uuid_t cx7Uuid = "992b3ec1-e468-f145-8686-409009062aa8";

    NsmDeviceTable devices{
        {std::make_shared<NsmDevice>((uint8_t)NSM_DEV_ID_PCIE_BRIDGE,
                                     instanceId)},
    };
    NsmDevice& cx7 = *devices[0];

    void SetUp() override
    {
        cx7.uuid = cx7Uuid;
    }

    NiceMock<MockSensorManager> mockManager{devices};

    const PropertyValuesCollection error = {
        {"UUID", "992b3ec1-e468-f145-8686-badbadbadbad"},
    };
    const PropertyValuesCollection basic = {
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

    const PropertyValuesCollection associations[1] = {
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
    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "InventoryObjPath"));
    values.push(objPath, get(error, "UUID"));

    createNsmPCIePort(mockManager, basicIntfName, objPath);
    EXPECT_EQ(0, cx7.prioritySensors.size());
    EXPECT_EQ(0, cx7.roundRobinSensors.size());
    EXPECT_EQ(0, cx7.deviceSensors.size());
}

TEST_F(NsmPCIePortTest, goodTestCreateDeviceSensors)
{
    utils::MockDbusAsync::getServiceMap() = serviceMap;

    auto& values = utils::MockDbusAsync::getValues();
    values.push(objPath, get(basic, "InventoryObjPath"));
    values.push(objPath, get(basic, "UUID"));
    values.push(objPath, get(associations[0], "Forward"));
    values.push(objPath, get(associations[0], "Backward"));
    values.push(objPath, get(associations[0], "AbsolutePath"));
    values.push(objPath, get(basic, "Health"));
    values.push(objPath, get(basic, "PortType"));
    values.push(objPath, get(basic, "PortProtocol"));
    values.push(objPath, get(basic, "LinkState"));
    values.push(objPath, get(basic, "LinkStatus"));

    createNsmPCIePort(mockManager, basicIntfName, objPath);

    EXPECT_EQ(0, cx7.prioritySensors.size());
    EXPECT_EQ(4, cx7.roundRobinSensors.size());
    EXPECT_EQ(8, cx7.deviceSensors.size());

    auto sensors = 0;
    auto associationsObject =
        dynamic_pointer_cast<NsmPCIePort<AssociationDefinitionsInft>>(
            cx7.deviceSensors[sensors++]);
    auto healthObject = dynamic_pointer_cast<NsmPCIePort<HealthIntf>>(
        cx7.deviceSensors[sensors++]);
    auto portObject = dynamic_pointer_cast<NsmPCIePort<PortIntf>>(
        cx7.deviceSensors[sensors++]);
    auto portStateObject = dynamic_pointer_cast<NsmPCIePort<PortStateIntf>>(
        cx7.deviceSensors[sensors++]);
    auto pcieLinkSpeed =
        dynamic_pointer_cast<NsmPCIeLinkSpeed<NsmPortInfoIntf>>(
            cx7.deviceSensors[sensors++]);
    auto pcieErrorsGroup2 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7.deviceSensors[sensors++]);
    auto pcieErrorsGroup3 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7.deviceSensors[sensors++]);
    auto pcieErrorsGroup4 =
        dynamic_pointer_cast<NsmPCIeErrors>(cx7.deviceSensors[sensors++]);

    EXPECT_EQ(sensors, cx7.deviceSensors.size());
    EXPECT_NE(nullptr, associationsObject);
    EXPECT_NE(nullptr, healthObject);
    EXPECT_NE(nullptr, portObject);
    EXPECT_NE(nullptr, portStateObject);
    EXPECT_NE(nullptr, pcieLinkSpeed);
    EXPECT_NE(nullptr, pcieErrorsGroup2);
    EXPECT_NE(nullptr, pcieErrorsGroup3);
    EXPECT_NE(nullptr, pcieErrorsGroup4);

    EXPECT_EQ(1, associationsObject->pdi().associations().size());
    EXPECT_EQ(
        get<std::string>(basic, "Health"),
        HealthIntf::convertHealthTypeToString(healthObject->pdi().health()));
    EXPECT_EQ(
        get<std::string>(basic, "PortType"),
        PortInfoIntf::convertPortTypeToString(pcieLinkSpeed->pdi().type()));
    EXPECT_EQ(get<std::string>(basic, "PortProtocol"),
              PortInfoIntf::convertPortProtocolToString(
                  pcieLinkSpeed->pdi().protocol()));
    EXPECT_EQ(get<std::string>(basic, "LinkState"),
              PortStateIntf::convertLinkStatesToString(
                  portStateObject->pdi().linkState()));
    EXPECT_EQ(get<std::string>(basic, "LinkStatus"),
              PortStateIntf::convertLinkStatusTypeToString(
                  portStateObject->pdi().linkStatus()));
    EXPECT_EQ(GROUP_ID_2, pcieErrorsGroup2->groupId);
    EXPECT_EQ(GROUP_ID_3, pcieErrorsGroup3->groupId);
    EXPECT_EQ(GROUP_ID_4, pcieErrorsGroup4->groupId);

    EXPECT_CALL(mockManager, SendRecvNsmMsg)
        .Times(cx7.roundRobinSensors.size())
        .WillRepeatedly(mockSendRecvNsmMsg());
    for (size_t i = 0; i < cx7.roundRobinSensors.size(); i++)
    {
        cx7.roundRobinSensors[i]->update(mockManager, eid).detach();
    }
}
