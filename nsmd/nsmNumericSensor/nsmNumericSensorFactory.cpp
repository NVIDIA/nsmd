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

#include "nsmDevice.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPeakPower.hpp"
#include "nsmThresholdFactory.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include <telemetry_mrd_producer.hpp>
#endif

namespace nsm
{

CreationFunction NumericSensorFactory::getCreationFunction()
{
    return std::bind_front(&NumericSensorFactory::make, this);
}

requester::Coroutine NumericSensorFactory::make(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath)

{
    auto& bus = utils::DBusHandler::getBus();

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), interface.c_str());

    uuid_t uuid{};
    if (allCurrentIfaceProperties.count("UUID"))
    {
        uuid = std::get<uuid_t>(allCurrentIfaceProperties.at("UUID"));
    }

    NumericSensorInfo info{};

    if (allCurrentIfaceProperties.count("Name"))
    {
        info.name = std::get<std::string>(allCurrentIfaceProperties.at("Name"));
    }
    info.name = utils::makeDBusNameValid(info.name);

    info.type = interface.substr(interface.find_last_of('.') + 1);

    if (allCurrentIfaceProperties.count("SensorId"))
    {
        info.sensorId =
            std::get<uint64_t>(allCurrentIfaceProperties.at("SensorId"));
    }

    if (allCurrentIfaceProperties.count("Priority"))
    {
        info.priority =
            std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }

    if (allCurrentIfaceProperties.count("Aggregated"))
    {
        info.aggregated =
            std::get<bool>(allCurrentIfaceProperties.at("Aggregated"));
    }

    if (allCurrentIfaceProperties.count("PhysicalContext"))
    {
        info.physicalContext = std::get<std::string>(
            allCurrentIfaceProperties.at("PhysicalContext"));
    }

    if (allCurrentIfaceProperties.count("Implementation"))
        info.implementation =
            std::make_unique<std::string>(std::get<std::string>(
                allCurrentIfaceProperties.at("Implementation")));

    if (allCurrentIfaceProperties.count("MaxAllowableOperatingValue"))
        info.maxAllowableValue = std::get<double>(
            allCurrentIfaceProperties.at("MaxAllowableOperatingValue"));

    if (allCurrentIfaceProperties.count("MaxValue"))
        info.maxValue =
            std::get<double>(allCurrentIfaceProperties.at("MaxValue"));

    if (allCurrentIfaceProperties.count("MinValue"))
        info.minValue =
            std::get<double>(allCurrentIfaceProperties.at("MinValue"));

    if (allCurrentIfaceProperties.count("ReadingBasis"))
        info.readingBasis = std::make_unique<std::string>(std::get<std::string>(
            allCurrentIfaceProperties.at("ReadingBasis")));

    if (allCurrentIfaceProperties.count("Description"))
        info.description = std::make_unique<std::string>(
            std::get<std::string>(allCurrentIfaceProperties.at("Description")));

    co_await utils::coGetAssociations(objPath, interface + ".Associations",
                                      info.associations);

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

    if (info.chassis_association.empty())
    {
        lg2::error(
            "Association Property of Numeric Sensor PDI has no chassis association. "
            "Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
            "NAME", info.name, "TYPE", info.type, "OBJPATH", objPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of Numeric Sensor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", info.name, "TYPE", info.type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto sensor = builder->makeSensor(interface, objPath, bus, info,
                                      allCurrentIfaceProperties);
    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", info.name, "TYPE", info.type);

    makeAggregatorAndAddSensor(builder.get(), info, sensor, uuid,
                               nsmDevice.get());

    try
    {
        auto peakValueProperties = co_await utils::coGetAllDbusProperty(
            utils::entityManagerServiceStr, objPath, interface + ".PeakValue");
        makePeakValueAndAdd(interface, objPath, info, uuid, nsmDevice.get(),
                            peakValueProperties);
    }
    catch (const std::exception& e)
    {}

    co_await NsmThresholdFactory{manager, interface, objPath,
                                 sensor,  info,      uuid}
        .make();
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

void NumericSensorFactory::makePeakValueAndAdd(
    const std::string& interface, const std::string& objPath,
    const NumericSensorInfo& info, const uuid_t& uuid, NsmDevice* nsmDevice,
    const dbus::PropertyMap& peakValueProperties)
{
    auto& bus = utils::DBusHandler::getBus();

    const auto peakValueInterface = interface + ".PeakValue";

    NumericSensorInfo peakValueInfo{};

    peakValueInfo.name = info.name;
    peakValueInfo.type = info.type + "_PeakValue";
    peakValueInfo.sensorId =
        std::get<uint64_t>(peakValueProperties.at("SensorId"));
    peakValueInfo.priority = std::get<bool>(peakValueProperties.at("Priority"));
    peakValueInfo.aggregated =
        std::get<bool>(peakValueProperties.at("Aggregated"));

    if (info.type == "NSM_Power")
    {
        PeakPowerSensorBuilder builder;

        auto sensor = builder.makeSensor(peakValueInterface, objPath, bus,
                                         peakValueInfo, peakValueProperties);
        lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
                  "UUID", uuid, "NAME", peakValueInfo.name, "TYPE",
                  peakValueInfo.type);

        makeAggregatorAndAddSensor(&builder, peakValueInfo, sensor, uuid,
                                   nsmDevice);
    }
    else
    {
        lg2::error(
            "The Numeric Sensor Type {TYPE} does not support Reading Peak Value : UUID={UUID}, Name={NAME}",
            "UUID", uuid, "NAME", info.name, "TYPE", info.type);
    }
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
