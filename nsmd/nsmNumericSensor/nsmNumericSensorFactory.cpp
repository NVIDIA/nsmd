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

#include "nsmNumericSensorFactory.hpp"

#include "features/associations.hpp"
#include "globals.hpp"
#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPeakPower.hpp"
#include "nsmThresholdFactory.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include <telemetry_mrd_producer.hpp>

#include <algorithm>
#endif

namespace nsm
{

void NumericSensorFactory::resolveAssociations(NumericSensorInfo& info)
{
    for (const auto& association : info.associations)
    {
        if (association.forward == "chassis")
        {
            info.chassis_association = association.absolutePath;
            break;
        }
    }

    // add primary_temperature_sensor association for primary temperature sensor
    if (info.type == "NSM_Temp" && info.sensorId == 0 &&
        !info.chassis_association.empty())
    {
        bool hasPrimaryTempAssoc = false;
        for (const auto& assoc : info.associations)
        {
            if (assoc.backward == "primary_temperature_sensor")
            {
                hasPrimaryTempAssoc = true;
                break;
            }
        }
        if (!hasPrimaryTempAssoc)
        {
            utils::Association primaryTempAssoc;
            primaryTempAssoc.forward = "chassis";
            primaryTempAssoc.backward = "primary_temperature_sensor";
            primaryTempAssoc.absolutePath = info.chassis_association;
            info.associations.push_back(primaryTempAssoc);
            lg2::info("Added primary_temperature_sensor association for {NAME}",
                      "NAME", info.name);
        }
    }
}

std::optional<NumericSensorInfo> NumericSensorFactory::makeFeatureSensorInfo(
    const features::DeviceContext& dev, const dbus::PropertyMap& params,
    const std::string& type)
{
    using features::param;

    NumericSensorInfo info{};
    info.name =
        utils::makeDBusNameValid(std::get<std::string>(params.at("Name")));
    info.type = type;
    info.sensorId = param<uint64_t>(params, "SensorId").value_or(0);
    info.priority = param<bool>(params, "Priority").value_or(false);
    info.aggregated = param<bool>(params, "Aggregated").value_or(false);
    info.physicalContext =
        param<std::string>(params, "PhysicalContext").value_or("");
    if (auto value = param<std::string>(params, "Implementation"))
    {
        info.implementation = std::make_unique<std::string>(*value);
    }
    if (auto value = param<double>(params, "MaxAllowableOperatingValue"))
    {
        info.maxAllowableValue = *value;
    }
    if (auto value = param<double>(params, "MaxValue"))
    {
        info.maxValue = *value;
    }
    if (auto value = param<double>(params, "MinValue"))
    {
        info.minValue = *value;
    }
    if (auto value = param<std::string>(params, "ReadingBasis"))
    {
        info.readingBasis = std::make_unique<std::string>(*value);
    }
    if (auto value = param<std::string>(params, "Description"))
    {
        info.description = std::make_unique<std::string>(*value);
    }

    // The first entry is the main chassis (chassis_association)
    features::addAssociations(info.associations, params,
                              "ChassisAssociationPaths", "chassis",
                              "all_sensors");
    features::addAssociations(info.associations, params,
                              "ProcessorAssociationPaths", "processor",
                              "all_sensors");
    features::addAssociations(info.associations, params,
                              "MemoryAssociationPaths", "memory",
                              "all_sensors");
    resolveAssociations(info);

    if (info.chassis_association.empty())
    {
        lg2::error("Feature sensor of {NAME} has no chassis association. "
                   "Name={SENSOR}, Type={TYPE}",
                   "NAME", dev.name, "SENSOR", info.name, "TYPE", type);
        return std::nullopt;
    }
    if (param<bool>(params, "PrimaryTemperatureAssociationEnabled")
            .value_or(false) &&
        std::ranges::none_of(info.associations, [](const auto& assoc) {
        return assoc.backward == "primary_temperature_sensor";
    }))
    {
        info.associations.push_back({"chassis", "primary_temperature_sensor",
                                     info.chassis_association});
    }
    return info;
}

void NumericSensorFactory::makeAggregatorAndAddSensor(
    NumericSensorAggregatorBuilder* builder, const NumericSensorInfo& info,
    std::shared_ptr<NsmNumericSensor> sensor, const uuid_t& uuid,
    NsmDevice* nsmDevice)
{
    std::shared_ptr<NsmNumericAggregator> aggregator{};
    // Check if Aggregator object for the NSM Command already exists.
    if (info.aggregated)
    {
        aggregator = nsmDevice->findAggregatorByType(info.type);
        if (aggregator)
        {
            // If existing Aggregator has low priority and this NSM
            // Command has high priority, update the existing
            // Aggregator's priority to high, remove it from round-robin
            // queue, and place it in priority queue.
            if (info.priority && !aggregator->priority)
            {
                aggregator->priority = true;
                std::erase(nsmDevice->deviceSensors, aggregator);
                std::erase(nsmDevice->roundRobinSensors, aggregator);
                nsmDevice->addSensor(aggregator, PollingType::Priority);
            }
        }
        else
        {
            aggregator = builder->makeAggregator(info);
            nsmDevice->sensorAggregators.emplace_back(aggregator);
            lg2::info(
                "Created NSM Sensor Aggregator : UUID={UUID}, Name={NAME}, Type={TYPE}",
                "UUID", uuid, "NAME", info.name, "TYPE", info.type);

            nsmDevice->addSensor(aggregator, info.priority);
        }
    }

    if (info.aggregated)
    {
        nsmDevice->deviceSensors.push_back(sensor);
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
        nsmDevice->addSensor(sensor, info.priority);
    }
}

}; // namespace nsm
