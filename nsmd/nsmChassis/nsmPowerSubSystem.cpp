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
    std::string& path, std::string& powerSupplyType) :
    NsmObject(name, type)
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
    powerSupplyInfoIntf = std::make_unique<PowerSupplyInfoIntf>(bus, path.c_str());
    powerSupplyInfoIntf->powerSupplyType(PowerSupplyInfoIntf::convertPowerSupplyTypesFromString(powerSupplyType));
    powerSupplyIntf = std::make_unique<PowerSupplyIntf>(bus, path.c_str());
}

static void CreatePowerSubSystem(SensorManager& manager,
                                  const std::string& interface,
                                  const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto powerSupplyType = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "PowerSupplyType", interface.c_str());

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto associations = utils::getAssociations(objPath,
                                               interface + ".Associations");

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of CreatePowerSubSystem PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto nsmPowerSubSystemPath =
        "/xyz/openbmc_project/inventory/system/PowerSubsystem/PowerSupplies/" + name;

    auto fpgaPowerSubSystem = std::make_shared<NsmPowerPowerSupply>(
        bus, name, associations, type, nsmPowerSubSystemPath, powerSupplyType);
    nsmDevice->deviceSensors.emplace_back(fpgaPowerSubSystem);
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePowerSubSystem,
    "xyz.openbmc_project.Configuration.NSM_PowerSupply")
} // namespace nsm