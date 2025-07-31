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

#include "nsmPowerSubSystem.hpp"

#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace nsm
{

NsmPowerPowerSupply::NsmPowerPowerSupply(
    sdbusplus::bus::bus& bus, std::string& name,
    const std::vector<utils::Association>& associations, std::string& type,
    std::string& path, std::string& powerSupplyType) : NsmObject(name, type)
{
    // add all interfaces
    associationDefinitionsInft =
        std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    // handle associations
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefinitionsInft->associations(associations_list);
    powerSupplyInfoIntf = std::make_unique<PowerSupplyInfoIntf>(bus,
                                                                path.c_str());
    powerSupplyInfoIntf->powerSupplyType(
        PowerSupplyInfoIntf::convertPowerSupplyTypesFromString(
            powerSupplyType));
    powerSupplyIntf = std::make_unique<PowerSupplyIntf>(bus, path.c_str());
}

static requester::Coroutine CreatePowerSubSystem(SensorManager& manager,
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

    std::string powerSupplyType{};
    if (allCurrentIfaceProperties.count("PowerSupplyType"))
    {
        powerSupplyType = std::get<std::string>(
            allCurrentIfaceProperties.at("PowerSupplyType"));
    }

    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }

    auto type = interface.substr(interface.find_last_of('.') + 1);

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of CreatePowerSubSystem PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto nsmPowerSubSystemPath =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/" +
        name;

    auto fpgaPowerSubSystem = std::make_shared<NsmPowerPowerSupply>(
        bus, name, associations, type, nsmPowerSubSystemPath, powerSupplyType);
    nsmDevice->deviceSensors.emplace_back(fpgaPowerSubSystem);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePowerSubSystem, "xyz.openbmc_project.Configuration.NSM_PowerSupply")
} // namespace nsm
