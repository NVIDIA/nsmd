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

#include "nsmPCIeRetimer.hpp"

#include "dBusAsyncUtils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <vector>

namespace nsm
{
NsmPCIeRetimerChassis::NsmPCIeRetimerChassis(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type) : NsmObject(name, type)
{
    auto pcieRetimerChaasisBasePath = std::string(chassisInventoryBasePath) +
                                      "/" + name;
    lg2::info("NsmPCIeRetimerChassis: create sensor:{NAME}", "NAME",
              name.c_str());

    // attach all interfaces
    associationDef_ = std::make_unique<AssociationDefinitionsInft>(
        bus, pcieRetimerChaasisBasePath.c_str());

    // handle associations
    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDef_->associations(associations_list);

    asset_ = std::make_unique<AssetIntf>(bus,
                                         pcieRetimerChaasisBasePath.c_str());
    asset_->sku("");
    location_ =
        std::make_unique<LocationIntf>(bus, pcieRetimerChaasisBasePath.c_str());
    location_->locationType(LocationIntf::convertLocationTypesFromString(
        "xyz.openbmc_project.Inventory.Decorator.Location.LocationTypes.Embedded"));
    chassis_ =
        std::make_unique<ChassisIntf>(bus, pcieRetimerChaasisBasePath.c_str());
    chassis_->ChassisIntf::type(ChassisIntf::convertChassisTypeFromString(
        "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.Component"));
}

static requester::Coroutine
    CreatePCIeRetimerChassis(SensorManager& manager,
                             const std::string& interface,
                             const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto type = interface.substr(interface.find_last_of('.') + 1);
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of NsmPCIeRetimerChassis PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        co_return NSM_ERROR;
    }

    auto retimer_chassis =
        std::make_shared<NsmPCIeRetimerChassis>(bus, name, associations, type);
    nsmDevice->deviceSensors.emplace_back(retimer_chassis);

    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    CreatePCIeRetimerChassis,
    "xyz.openbmc_project.Configuration.NSM_PCIeRetimer")

} // namespace nsm
