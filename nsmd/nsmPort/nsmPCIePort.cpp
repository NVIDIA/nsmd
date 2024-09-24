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

#include "nsmPCIePort.hpp"

#include "nsmPriorityMapping.h"

#include "dBusAsyncUtils.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPCIeErrors.hpp"
#include "nsmPCIeLinkSpeed.hpp"
#include "nsmPortInfo.hpp"

#include <xyz/openbmc_project/Association/Definitions/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortState/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Port/server.hpp>
#include <xyz/openbmc_project/State/Decorator/Health/server.hpp>

namespace nsm
{

using sdbusplus::server::object_t;
using namespace sdbusplus::server::xyz::openbmc_project;
using AssociationDefinitionsInft = object_t<Association::server::Definitions>;
using PortIntf = object_t<Inventory::Item::server::Port>;
using PortStateIntf = object_t<Inventory::Decorator::server::PortState>;
using HealthIntf = object_t<State::Decorator::server::Health>;

requester::Coroutine createNsmPCIePort(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath)
{
    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto device = manager.getNsmDevice(uuid);

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto health = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Health", interface.c_str());
    auto portType = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PortType", interface.c_str());
    auto portProtocol = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PortProtocol", interface.c_str());
    auto linkState = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "LinkState", interface.c_str());
    auto linkStatus = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "LinkStatus", interface.c_str());

    auto associationsObject =
        std::make_shared<NsmPCIePort<AssociationDefinitionsInft>>(
            inventoryObjPath);
    auto healthObject =
        std::make_shared<NsmPCIePort<HealthIntf>>(inventoryObjPath);
    auto portObject = std::make_shared<NsmPCIePort<PortIntf>>(inventoryObjPath);
    auto portStateObject =
        std::make_shared<NsmPCIePort<PortStateIntf>>(inventoryObjPath);
    auto portInfoObject = NsmPCIePort<NsmPortInfoIntf>(inventoryObjPath);
    auto pcieLinkSpeed =
        std::make_shared<NsmPCIeLinkSpeed<NsmPortInfoIntf>>(portInfoObject, 0);
    auto portPCIeEccObject = NsmPCIePort<PCIeEccIntf>(inventoryObjPath);
    auto pcieErrorsGroup2 = std::make_shared<NsmPCIeErrors>(portPCIeEccObject,
                                                            0, GROUP_ID_2);
    auto pcieErrorsGroup3 = std::make_shared<NsmPCIeErrors>(portPCIeEccObject,
                                                            0, GROUP_ID_3);
    auto pcieErrorsGroup4 = std::make_shared<NsmPCIeErrors>(portPCIeEccObject,
                                                            0, GROUP_ID_4);

    associationsObject->pdi().associations(
        utils::getAssociations(associations));
    healthObject->pdi().health(HealthIntf::convertHealthTypeFromString(health));
    portInfoObject.pdi().type(
        PortInfoIntf::convertPortTypeFromString(portType));
    portInfoObject.pdi().protocol(
        PortInfoIntf::convertPortProtocolFromString(portProtocol));
    portStateObject->pdi().linkState(
        PortStateIntf::convertLinkStatesFromString(linkState));
    portStateObject->pdi().linkStatus(
        PortStateIntf::convertLinkStatusTypeFromString(linkStatus));

    device->deviceSensors.emplace_back(associationsObject);
    device->deviceSensors.emplace_back(healthObject);
    device->deviceSensors.emplace_back(portObject);
    device->deviceSensors.emplace_back(portStateObject);
    device->addSensor(pcieLinkSpeed, PCIE_PORT_LINK_SPEED_PRIORITY);
    device->addSensor(pcieErrorsGroup2, PCIE_PORT_ERRORS_PRIORITY);
    device->addSensor(pcieErrorsGroup3, PCIE_PORT_ERRORS_PRIORITY);
    device->addSensor(pcieErrorsGroup4, PCIE_PORT_ERRORS_PRIORITY);

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(createNsmPCIePort,
                               "xyz.openbmc_project.Configuration.NSM_PCIePort")

} // namespace nsm
