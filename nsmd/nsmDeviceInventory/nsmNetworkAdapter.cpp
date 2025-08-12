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
#if defined(ENABLE_DEBUG_INFO)
#include "nsmDebugInfo.hpp"
#endif
#if defined(ENABLE_DEBUG_TOKEN)
#include "nsmDebugToken.hpp"
#endif
#if defined(ENABLE_DEBUG_INFO)
#include "nsmEraseTrace.hpp"
#endif
#if defined(ENABLE_ERROR_INJECTION)
#include "nsmErrorInjection/nsmErrorInjectionCommon.hpp"
#endif
#if defined(ENABLE_DEBUG_INFO)
#include "nsmLogInfo.hpp"
#endif

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
    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    std::string name{};
    if (allCurrentIfaceProperties.count("Name"))
    {
        name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }
    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    std::string inventoryObjPath{};
    if (allCurrentIfaceProperties.count("InventoryObjPath"))
    {
        inventoryObjPath = std::get<std::string>(
            allCurrentIfaceProperties.at("InventoryObjPath"));
    }

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);
    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

    auto networkAdapterDI = std::make_shared<NsmNetworkAdapterDI>(
        bus, name, associations, type, inventoryObjPath);
    nsmDevice->getDeviceSensors().emplace_back(networkAdapterDI);

#if defined(ENABLE_DEBUG_TOKEN)
    auto debugTokenObject = std::make_shared<NsmDebugTokenObject>(
        bus, name, associations, type, uuid);
    nsmDevice->addStaticSensor(debugTokenObject);
#endif

#if defined(ENABLE_DEBUG_INFO)
    auto networkAdapterDebugInfoObject = std::make_shared<NsmDebugInfoObject>(
        bus, name, inventoryObjPath, type, uuid, DebugDumpType::Network);
    nsmDevice->addStaticSensor(networkAdapterDebugInfoObject);

    auto networkAdapterEraseTraceObject = std::make_shared<NsmEraseTraceObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterEraseTraceObject);

    auto networkAdapterLogInfoObject = std::make_shared<NsmLogInfoObject>(
        bus, name, inventoryObjPath, type, uuid);
    nsmDevice->addStaticSensor(networkAdapterLogInfoObject);
#endif

#if defined(ENABLE_ERROR_INJECTION)
    createNsmErrorInjectionSensors(manager, nsmDevice,
                                   path(inventoryObjPath) / name);
#endif

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
    auto ntwAdpResetSensor = std::make_shared<NsmNetworkAdapterDIReset>(
        bus, name, type, inventoryObjPath, nsmDevice);
    nsmDevice->getDeviceSensors().push_back(ntwAdpResetSensor);
#endif

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

#if defined(ENABLE_NETWORK_ADAPTER_RESET)
NsmNetworkAdapterDIReset::NsmNetworkAdapterDIReset(
    sdbusplus::bus::bus& bus, const std::string& name, const std::string& type,
    std::string& inventoryObjPath, std::shared_ptr<NsmDevice> device) :
    NsmObject(name, type)
{
    lg2::info("NsmNetworkAdapterDIReset: create sensor:{NAME}", "NAME",
              name.c_str());

    objPath = inventoryObjPath + name;
    resetIntf = std::make_shared<NsmResetDeviceIntf>(bus, objPath.c_str());
    resetIntf->resetType(sdbusplus::common::xyz::openbmc_project::control::
                             Reset::ResetTypes::ForceRestart);
    resetAsyncIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, objPath.c_str(), device);
}
#endif

REGISTER_NSM_CREATION_FUNCTION(
    createNSMNetworkAdapter,
    "xyz.openbmc_project.Configuration.NSM_NetworkAdapter")

} // namespace nsm
