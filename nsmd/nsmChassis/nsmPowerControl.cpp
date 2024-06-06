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

#include "nsmPowerControl.hpp"
#include "nsmd/nsmProcessor/nsmProcessor.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace nsm
{

NsmPowerControl::NsmPowerControl(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations, std::string& type,
    const std::string& path, const std::string& physicalContext) :
    NsmObject(name, type),
    PowerCapIntf(bus, path.c_str()), ClearPowerCapIntf(bus, path.c_str())
{
    decoratorAreaIntf = std::make_unique<DecoratorAreaIntf>(bus, path.c_str());
    decoratorAreaIntf->physicalContext(
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area::
            convertPhysicalContextTypeFromString(
                "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType." +
                physicalContext));
    // add all interfaces
    associationDefinitionsInft =
        std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    powerModeIntf = std::make_unique<PowerModeIntf>(bus, path.c_str());
    powerModeIntf->powerMode(Mode::PowerMode::MaximumPerformance);
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
}

// customer set for power cap
uint32_t NsmPowerControl::powerCap(uint32_t power_limit)
{
	if (power_limit > maxPowerCapValue() ||
	    power_limit < minPowerCapValue()) {
		throw sdbusplus::xyz::openbmc_project::Common::Device::Error::
		    WriteFailure();
	}
	SensorManager &manager = SensorManager::getInstance();
	auto size = manager.powerCapList.size();
	for (const auto &powerCapSensor : manager.powerCapList) {
		powerCapSensor->getPowerCapIntf()->powerCap(power_limit / size);
	}
	return PowerCapIntf::powerCap();
}

// called when individual gpu processor is updated
void NsmPowerControl::updatePowerCapValue(const std::string &childName,
					  uint32_t value)
{
	powerCapChildValues[childName] = value;
	uint32_t totalValue = 0;

	for (const auto &pair : powerCapChildValues) {
		totalValue += pair.second;
	}
	// calling parent powercap to initialize the value on dbus
	PowerCapIntf::powerCap(totalValue);
}

// custom get for maxPowerValue
uint32_t NsmPowerControl::maxPowerCapValue() const
{
    SensorManager& manager = SensorManager::getInstance();
    uint32_t valueTotal = 0;
    for (const auto& powerCapSensor : manager.maxPowerCapList)
    {
        valueTotal += powerCapSensor->getMaxPowerCapIntf()
                          ->PowerCapIntf::maxPowerCapValue();
    }
    return valueTotal;
}

// custom get fir minPowerValue
uint32_t NsmPowerControl::minPowerCapValue() const
{
    SensorManager& manager = SensorManager::getInstance();
    uint32_t valueTotal = 0;
    for (const auto& powerCapSensor : manager.minPowerCapList)
    {
        valueTotal += powerCapSensor->getMinPowerCapIntf()
                          ->PowerCapIntf::minPowerCapValue();
    }
    return valueTotal;
}

// custom get for defaultPowerCap
uint32_t NsmPowerControl::defaultPowerCap() const
{
    SensorManager& manager = SensorManager::getInstance();
    uint32_t valueTotal = 0;
    for (const auto& powerCapSensor : manager.defaultPowerCapList)
    {
        valueTotal += powerCapSensor->getDefaultPowerCapIntf()
                          ->ClearPowerCapIntf::defaultPowerCap();
    }
    return valueTotal;
}

int32_t NsmPowerControl::clearPowerCap()
{
    SensorManager& manager = SensorManager::getInstance();
    for (const auto& powerCapSensor : manager.defaultPowerCapList)
    {
        powerCapSensor->getDefaultPowerCapIntf()->clearPowerCap();
    }
    return 0;
}

static void CreateControlGpuPower(SensorManager& manager,
                                  const std::string& interface,
                                  const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto type = interface.substr(interface.find_last_of('.') + 1);

    auto associations = utils::getAssociations(objPath,
                                               interface + ".Associations");
    auto physicalContext = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "PhysicalContext", interface.c_str());

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of CreateFPGATotalGPUPower PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto nsmFPGAControlTotalGPUPowerPath =
        "/xyz/openbmc_project/inventory/system/chassis/power/control/" + name;

    auto fpgaControlTotalGpuPower = std::make_shared<NsmPowerControl>(
        bus, name, associations, type, nsmFPGAControlTotalGPUPowerPath, physicalContext);
    nsmDevice->deviceSensors.emplace_back(fpgaControlTotalGpuPower);
    manager.objectPathToSensorMap[nsmFPGAControlTotalGPUPowerPath] =
        fpgaControlTotalGpuPower;
}

REGISTER_NSM_CREATION_FUNCTION(
    CreateControlGpuPower,
    "xyz.openbmc_project.Configuration.NSM_ControlTotalGPUPower")
} // namespace nsm