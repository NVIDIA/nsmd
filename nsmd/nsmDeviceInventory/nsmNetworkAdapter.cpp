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

#include "nsmNetworkAdapter.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmDebugInfo.hpp"
#include "nsmDebugToken.hpp"
#include "nsmEraseTrace.hpp"
#include "nsmErrorInjectionCommon.hpp"
#include "nsmLogInfo.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmNetworkAdapterDI::NsmNetworkAdapterDI(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& inventoryObjPath) :
    NsmObject(name, type)
{
    auto objPath = inventoryObjPath + name;
    lg2::info("NsmNetworkAdapterDI: {NAME}", "NAME", name.c_str());

    associationDefIntf =
        std::make_unique<AssociationDefinitionsInft>(bus, objPath.c_str());
    pcieDeviceIntf = std::make_unique<PCIeDeviceIntf>(bus, objPath.c_str());
    networkInterfaceIntf =
        std::make_unique<NetworkInterfaceIntf>(bus, objPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefIntf->associations(associations_list);
}

static requester::Coroutine
    createNSMNetworkAdapter(SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());
    auto type = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Type", interface.c_str());
    auto inventoryObjPath = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "InventoryObjPath", interface.c_str());
    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto nsmDevice = manager.getNsmDevice(uuid);

    auto networkAdapterDI = std::make_shared<NsmNetworkAdapterDI>(
        bus, name, associations, type, inventoryObjPath);
    nsmDevice->deviceSensors.emplace_back(networkAdapterDI);

    auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
        bus, name, associations, type, uuid);
    nsmDevice->addStaticSensor(debugTokenObject);

    auto networkAdapterDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterDebugInfoObject);

    auto networkAdapterEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterEraseTraceObject);

    auto networkAdapterLogInfoObject = std::make_shared<NsmLogInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterLogInfoObject);

    createNsmErrorInjectionSensors(manager, nsmDevice,
                                   path(inventoryObjPath) / name);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNSMNetworkAdapter,
    "xyz.openbmc_project.Configuration.NSM_NetworkAdapter")

} // namespace nsm
