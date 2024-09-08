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

#include "nsmNumericSensorComposite.hpp"

#include "nsmPower.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace nsm
{
NsmNumericSensorComposite::NsmNumericSensorComposite(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::vector<utils::Association>& associations,
    const std::string& type, const std::string& path,
    const std::string& physicalContext, const std::string& implementation
#ifdef NVIDIA_SHMEM
    ,
    std::unique_ptr<NsmNumericSensorShmem> shmemSensor
#endif
    ) :
    NsmObject(name, type)
#ifdef NVIDIA_SHMEM
    ,
    shmemSensor(std::move(shmemSensor))
#endif
{
    // add all interfaces
    decoratorAreaIntf = std::make_unique<DecoratorAreaIntf>(bus, path.c_str());
    decoratorAreaIntf->physicalContext(
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area::
            convertPhysicalContextTypeFromString(
                "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType." +
                physicalContext));

    typeIntf = std::make_unique<TypeIntf>(bus, path.c_str());
    typeIntf->implementation(
        sdbusplus::common::xyz::openbmc_project::sensor::Type::
            convertImplementationTypeFromString(
                "xyz.openbmc_project.Sensor.Type.ImplementationType." +
                implementation));
    associationDefinitionsInft =
        std::make_unique<AssociationDefinitionsInft>(bus, path.c_str());
    valueIntf = std::make_unique<ValueIntf>(bus, path.c_str());
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
    valueIntf->unit(SensorUnit::Watts);
    valueIntf->value(std::numeric_limits<double>::quiet_NaN());
}

void NsmNumericSensorComposite::updateCompositeReading(std::string childName,
                                                       double value)
{
    childValues[childName] = value;
    bool hasNaN = false;
    double totalValue = 0.0;

    for (const auto& pair : childValues)
    {
        if (std::isnan(pair.second))
        {
            hasNaN = true;
            break;
        }
        totalValue += pair.second;
    }

    if (hasNaN)
    {
        totalValue = std::nan("");
    }

#ifdef NVIDIA_SHMEM
    if (shmemSensor)
    {
        shmemSensor->updateReading(totalValue);
    }
#endif
    valueIntf->value(totalValue);
}

static requester::Coroutine
    CreateFPGATotalGPUPower(SensorManager& manager,
                            const std::string& interface,
                            const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();
    auto name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    auto sensorType = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "SensorType", interface.c_str());

    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    auto type = interface.substr(interface.find_last_of('.') + 1);

    std::vector<utils::Association> associations{};
    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      associations);

    auto physicalContext = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PhysicalContext", interface.c_str());
    auto implementation = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Implementation", interface.c_str());
#ifdef NVIDIA_SHMEM
    std::string chassis_association;
    for (const auto& association : associations)
    {
        if (association.forward == "chassis")
        {
            chassis_association = association.absolutePath;
            break;
        }
    }

    if (chassis_association.empty())
    {
        lg2::error(
            "Association Property of TotalPower Sensor PDI has no chassis association. "
            "Name={NAME}, Type={TYPE}",
            "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }
#endif
    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of CreateFPGATotalGPUPower PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto nsmFPGATotalGPUPowerSensorPath = "/xyz/openbmc_project/sensors/" +
                                          sensorType + "/" + name;
#ifdef NVIDIA_SHMEM
    auto shmemSensor = std::make_unique<NsmNumericSensorShmem>(
        name, sensorType, chassis_association,
        std::make_unique<SMBPBIPowerSMBusSensorBytesConverter>());
#endif
    auto fpgaTotalGpuPower = std::make_shared<NsmNumericSensorComposite>(
        bus, name, associations, type, nsmFPGATotalGPUPowerSensorPath,
        physicalContext, implementation
#ifdef NVIDIA_SHMEM
        ,
        std::move(shmemSensor)
#endif
    );
    nsmDevice->deviceSensors.emplace_back(fpgaTotalGpuPower);
    manager.objectPathToSensorMap[nsmFPGATotalGPUPowerSensorPath] =
        fpgaTotalGpuPower;
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    CreateFPGATotalGPUPower,
    "xyz.openbmc_project.Configuration.NSM_NumericCompositeSensor")
} // namespace nsm
