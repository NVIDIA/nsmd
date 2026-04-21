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

#include "nsmAsioPCIeDeviceInterface.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{

std::shared_ptr<NsmAsioPCIeDeviceInterface>
    NsmAsioPCIeDeviceInterface::createSinglePortDevice(
        sdbusplus::asio::object_server& objServer,
        const std::string& objectPath, const std::string& deviceType,
        const std::vector<uint64_t>& functionIds)
{
    auto intf = std::make_shared<NsmAsioPCIeDeviceInterface>(
        objServer, objectPath, deviceType, functionIds);

    if (!intf->initialize())
    {
        lg2::error("Failed to initialize PCIeDevice interface: path={PATH}",
                   "PATH", objectPath);
        return nullptr;
    }

    return intf;
}

NsmAsioPCIeDeviceInterface::NsmAsioPCIeDeviceInterface(
    sdbusplus::asio::object_server& objServer, const std::string& objectPath,
    const std::string& deviceType, const std::vector<uint64_t>& functionIds) :
    NsmAsioInterfaceBase(objServer, objectPath,
                         "xyz.openbmc_project.Inventory.Item.PCIeDevice"),
    functionIds(functionIds)
{
    if (!dbusInterface)
    {
        return;
    }

    dbusInterface->register_property(
        "DeviceType",
        PCIeDeviceServer::convertDeviceTypesToString(
            PCIeDeviceServer::convertDeviceTypesFromString(deviceType)));
    dbusInterface->register_property("PCIeType",
                                     PCIeDeviceServer::convertPCIeTypesToString(
                                         PCIeDeviceServer::PCIeTypes::Gen1));
    dbusInterface->register_property("MaxPCIeType",
                                     PCIeDeviceServer::convertPCIeTypesToString(
                                         PCIeDeviceServer::PCIeTypes::Gen1));
    dbusInterface->register_property("GenerationInUse",
                                     PCIeSlotServer::convertGenerationsToString(
                                         PCIeSlotServer::Generations::Gen1));
    dbusInterface->register_property("LanesInUse", size_t(0));
    dbusInterface->register_property("MaxLanes", size_t(0));
    dbusInterface->register_property("GenerationSupported",
                                     PCIeSlotServer::convertGenerationsToString(
                                         PCIeSlotServer::Generations::Unknown));

    for (auto& id : functionIds)
    {
        std::string prefix = "Function" + std::to_string(id);
        dbusInterface->register_property(prefix + "ClassCode", std::string(""));
        dbusInterface->register_property(prefix + "DeviceClass",
                                         std::string(""));
        dbusInterface->register_property(prefix + "DeviceId", std::string(""));
        dbusInterface->register_property(prefix + "FunctionType",
                                         std::string(""));
        dbusInterface->register_property(prefix + "RevisionId",
                                         std::string(""));
        dbusInterface->register_property(prefix + "SubsystemId",
                                         std::string(""));
        dbusInterface->register_property(prefix + "SubsystemVendorId",
                                         std::string(""));
        dbusInterface->register_property(prefix + "VendorId", std::string(""));
    }
}

bool NsmAsioPCIeDeviceInterface::initialize()
{
    if (!dbusInterface)
    {
        lg2::error("Cannot initialize PCIeDevice interface: null: path={PATH}",
                   "PATH", objectPath);
        return false;
    }
    return dbusInterface->initialize();
}

void NsmAsioPCIeDeviceInterface::updateLinkSpeed(
    const std::string& pcieType, const std::string& generationInUse,
    const std::string& maxPcieType, size_t lanesInUse, size_t maxLanes)
{
    if (!dbusInterface)
    {
        return;
    }
    dbusInterface->set_property("PCIeType", pcieType);
    dbusInterface->set_property("GenerationInUse", generationInUse);
    dbusInterface->set_property("MaxPCIeType", maxPcieType);
    dbusInterface->set_property("LanesInUse", lanesInUse);
    dbusInterface->set_property("MaxLanes", maxLanes);
}

void NsmAsioPCIeDeviceInterface::updateFunction(
    uint8_t functionId, const std::string& vendorId,
    const std::string& deviceId, const std::string& classCode,
    const std::string& revisionId, const std::string& functionType,
    const std::string& deviceClass, const std::string& subsystemVendorId,
    const std::string& subsystemId)
{
    if (!dbusInterface)
    {
        return;
    }
    std::string prefix = "Function" + std::to_string(functionId);
    dbusInterface->set_property(prefix + "VendorId", vendorId);
    dbusInterface->set_property(prefix + "DeviceId", deviceId);
    dbusInterface->set_property(prefix + "ClassCode", classCode);
    dbusInterface->set_property(prefix + "RevisionId", revisionId);
    dbusInterface->set_property(prefix + "FunctionType", functionType);
    dbusInterface->set_property(prefix + "DeviceClass", deviceClass);
    dbusInterface->set_property(prefix + "SubsystemVendorId",
                                subsystemVendorId);
    dbusInterface->set_property(prefix + "SubsystemId", subsystemId);
}

} // namespace nsm
