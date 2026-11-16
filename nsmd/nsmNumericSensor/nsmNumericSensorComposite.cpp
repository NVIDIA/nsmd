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

#include "nsmNumericSensorFeatures.hpp"
#include "nsmPower.hpp"
#include "sensorManager.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>
#include <optional>
#include <vector>

namespace nsm
{
NsmNumericSensorComposite::NsmNumericSensorComposite(
    sdbusplus::bus_t& bus, const std::string& name,
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

    // Composite: publish only Implementation on Sensor.Type
    // (readingBasis=nullptr — no per-physical-measurement reading basis).
    sensorTypeAsioIntf = NsmAsioSensorTypeInterface::create(
        SensorManager::getInstance().getObjServer(), path, &implementation,
        /* readingBasis = */ nullptr);

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

    auto timestamp = utils::getCurrentSteadyClockTimestamp();
#ifdef NVIDIA_SHMEM
    if (shmemSensor)
    {
        shmemSensor->updateReading(totalValue, timestamp);
    }
#endif
    if (timestamp >= nextUpdateTimestamp || nextUpdateTimestamp == 0)
    {
        valueIntf->value(totalValue);
        NsmNumericSensorDbusValue::calculateNextUpdateTimestamp(
            timestamp, nextUpdateTimestamp);
    }
}

} // namespace nsm

namespace nsm::features
{

requester::Coroutine
    createNumericCompositeSensorFeature(SensorManager& manager,
                                        const DeviceContext& dev,
                                        const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("Numeric composite sensor of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }
    auto& bus = utils::DBusHandler::getBus();
    const auto name = std::get<std::string>(params.at("Name"));
    const auto sensorType = std::get<std::string>(params.at("SensorType"));
    const auto path = "/xyz/openbmc_project/sensors/" + sensorType + "/" + name;
    const auto chassis = utils::makeDBusNameValid(
        (chassisInventoryBasePath /
         std::get<std::string>(params.at("ChassisName")))
            .string());
    // Members declare their own total_power links (e.g. the GPU Processor
    // TotalPowerAssociationEnabled), so only the chassis link is here
    std::vector<utils::Association> associations{
        {"chassis", "all_sensors", chassis}};
#ifdef NVIDIA_SHMEM
    auto shmemSensor = std::make_unique<NsmNumericSensorShmem>(
        name, sensorType, chassis,
        std::make_unique<SMBPBIPowerSMBusSensorBytesConverter>());
#endif
    const std::string type = "NSM_NumericCompositeSensor";
    auto sensor = std::make_shared<NsmNumericSensorComposite>(
        bus, name, associations, type, path,
        std::get<std::string>(params.at("PhysicalContext")),
        std::get<std::string>(params.at("Implementation"))
#ifdef NVIDIA_SHMEM
            ,
        std::move(shmemSensor)
#endif
    );
    dev.device->addDeviceSensors(sensor);
    manager.objectPathToSensorMap[path] = sensor;
    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", dev.staticUuid, "NAME", name, "TYPE", type);
    co_return NSM_SUCCESS;
}

} // namespace nsm::features
