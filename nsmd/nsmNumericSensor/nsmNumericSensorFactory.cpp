/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
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



#include "nsmNumericSensorFactory.hpp"

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{

CreationFunction NumericSensorFactory::getCreationFunction()
{
    return std::bind_front(&NumericSensorFactory::make, this);
}

void NumericSensorFactory::make(SensorManager& manager,
                                const std::string& interface,
                                const std::string& objPath)

{
    auto& bus = utils::DBusHandler::getBus();

    auto uuid = utils::DBusHandler().getDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    NumericSensorInfo info{};

    info.name = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    info.name = utils::makeDBusNameValid(info.name);

    info.type = interface.substr(interface.find_last_of('.') + 1);

    info.sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "SensorId", interface.c_str());

    info.priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());

    info.aggregated = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Aggregated", interface.c_str());

    info.associations =
        utils::getAssociations(objPath, interface + ".Associations");

    for (const auto& association : info.associations)
    {
        if (association.forward == "chassis")
        {
            info.chassis_association = association.absolutePath;
            break;
        }
    }

    if (info.chassis_association.empty())
    {
        lg2::error(
            "Association Property of Numeric Sensor PDI has no chassis association. "
            "Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
            "NAME", info.name, "TYPE", info.type, "OBJPATH", objPath);

        return;
    }

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of Numeric Sensor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", info.name, "TYPE", info.type);
        return;
    }

    std::shared_ptr<NsmNumericAggregator> aggregator{};
    // Check if Aggregator object for the NSM Command already exists.
    if (info.aggregated)
    {
        aggregator = nsmDevice->findAggregatorByType(info.type);
        if (aggregator)
        {
            // If existing Aggregator has low priority and this NSM Command has
            // high priority, update the existing Aggregator's priority to
            // high, remove it from round-robin queue, and place it in priority
            // queue.
            if (info.priority && !aggregator->priority)
            {
                aggregator->priority = true;
                std::erase(nsmDevice->roundRobinSensors, aggregator);
                nsmDevice->prioritySensors.push_back(aggregator);
            }
        }
        else
        {
            aggregator = builder->makeAggregator(info);
            nsmDevice->sensorAggregators.push_back(aggregator);
            lg2::info(
                "Created NSM Sensor Aggregator : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", info.name, "TYPE", info.type);

            if (info.priority)
            {
                nsmDevice->prioritySensors.push_back(aggregator);
            }
            else
            {
                nsmDevice->roundRobinSensors.push_back(aggregator);
            }
        }
    }

    auto sensor = builder->makeSensor(interface, objPath, bus, info);
    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", info.name, "TYPE", info.type);

    nsmDevice->deviceSensors.emplace_back(sensor);

    if (info.aggregated)
    {
        auto rc = aggregator->addSensor(info.sensorId,
                                        sensor->getSensorValueObject());
        if (rc == NSM_SW_SUCCESS)
        {
            lg2::info(
                "Added NSM Sensor to Aggregator : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", info.name, "TYPE", info.type);
        }
        else
        {
            lg2::error(
                "Failed to add NSM Sensor to Aggregator : RC = {RC}, UUID={UUID}, Name={NAME}, Type={TYPE}",
                "RC", rc, "UUID", uuid, "NAME", info.name, "TYPE", info.type);
        }
    }
    else
    {
        if (info.priority)
        {
            nsmDevice->prioritySensors.emplace_back(sensor);
        }
        else
        {
            nsmDevice->roundRobinSensors.emplace_back(sensor);
        }
    }
}
}; // namespace nsm
