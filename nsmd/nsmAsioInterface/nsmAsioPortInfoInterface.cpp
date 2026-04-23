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

#include "nsmAsioPortInfoInterface.hpp"

#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/PortInfo/server.hpp>

namespace nsm
{

using PortInfoServer =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::PortInfo;

std::unique_ptr<NsmAsioPortInfoInterface>
    NsmAsioPortInfoInterface::createSinglePortDevice(
        sdbusplus::asio::object_server& objServer,
        const std::string& objectPath, const std::string& portType,
        const std::string& portProtocol)
{
    auto intf = std::make_unique<NsmAsioPortInfoInterface>(
        objServer, objectPath, portType, portProtocol);

    if (!intf->initialize())
    {
        lg2::error("Failed to initialize PortInfo interface: path={PATH}",
                   "PATH", objectPath);
        return nullptr;
    }

    return intf;
}

NsmAsioPortInfoInterface::NsmAsioPortInfoInterface(
    sdbusplus::asio::object_server& objServer, const std::string& objectPath,
    const std::string& portType, const std::string& portProtocol) :
    NsmAsioInterfaceBase(objServer, objectPath,
                         "xyz.openbmc_project.Inventory.Decorator.PortInfo")
{
    if (!dbusInterface)
    {
        return;
    }

    dbusInterface->register_property(
        "Type", PortInfoServer::convertPortTypeToString(
                    PortInfoServer::convertPortTypeFromString(portType)));
    dbusInterface->register_property(
        "Protocol",
        PortInfoServer::convertPortProtocolToString(
            PortInfoServer::convertPortProtocolFromString(portProtocol)));
    dbusInterface->register_property("CurrentSpeed", double(0));
    dbusInterface->register_property("MaxSpeed", double(0));
    dbusInterface->register_property("TargetSpeed",
                                     std::numeric_limits<double>::quiet_NaN());
}

bool NsmAsioPortInfoInterface::initialize()
{
    if (!dbusInterface)
    {
        lg2::error("Cannot initialize PortInfo interface: null: path={PATH}",
                   "PATH", objectPath);
        return false;
    }
    return dbusInterface->initialize();
}

void NsmAsioPortInfoInterface::updateSpeeds(double current, double max,
                                            double target)
{
    if (!dbusInterface)
    {
        return;
    }
    currentSpeed = current;
    dbusInterface->set_property("CurrentSpeed", current);
    dbusInterface->set_property("MaxSpeed", max);
    dbusInterface->set_property("TargetSpeed", target);
}

} // namespace nsm
