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
#include <telemetry_mrd_producer.hpp>

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

    auto uuid = co_await utils::coGetDbusProperty<uuid_t>(
        objPath.c_str(), "UUID", interface.c_str());

    NumericSensorInfo info{};

    info.name = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "Name", interface.c_str());
    info.name = utils::makeDBusNameValid(info.name);

    info.type = interface.substr(interface.find_last_of('.') + 1);

    info.sensorId = co_await utils::coGetDbusProperty<uint64_t>(
        objPath.c_str(), "SensorId", interface.c_str());

    info.priority = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Priority", interface.c_str());

    info.aggregated = co_await utils::coGetDbusProperty<bool>(
        objPath.c_str(), "Aggregated", interface.c_str());

    info.physicalContext = co_await utils::coGetDbusProperty<std::string>(
        objPath.c_str(), "PhysicalContext", interface.c_str());

    try
    {
        info.implementation = std::make_unique<std::string>(
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "Implementation", interface.c_str()));
    }
    catch (const std::exception& e)
    {}

    try
    {
        info.maxAllowableValue = utils::DBusHandler().getDbusProperty<double>(
            objPath.c_str(), "MaxAllowableOperatingValue", interface.c_str());
    }
    catch (const std::exception& e)
    {}

    try
    {
        info.readingBasis = std::make_unique<std::string>(
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "ReadingBasis", interface.c_str()));
    }
    catch (const std::exception& e)
    {}

    try
    {
        info.description = std::make_unique<std::string>(
            utils::DBusHandler().getDbusProperty<std::string>(
                objPath.c_str(), "Description", interface.c_str()));
    }
    catch (const std::exception& e)
    {}

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

    if (info.chassis_association.empty())
    {
        lg2::error(
            "Association Property of Numeric Sensor PDI has no chassis association. "
            "Name={NAME}, Type={TYPE}, Object_Path={OBJPATH}",
            "NAME", info.name, "TYPE", info.type, "OBJPATH", objPath);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of Numeric Sensor PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", info.name, "TYPE", info.type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto sensor = builder->makeSensor(interface, objPath, bus, info);
    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", info.name, "TYPE", info.type);

    makeAggregatorAndAddSensor(builder.get(), info, sensor, uuid,
                               nsmDevice.get());

    try
    {
        makePeakValueAndAdd(interface, objPath, info, uuid, nsmDevice.get());
    }
    catch (const std::exception& e)
    {}

    co_await NsmThresholdFactory{manager, interface, objPath,
                                 sensor,  info,      uuid}
        .make();
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

void NumericSensorFactory::makePeakValueAndAdd(const std::string& interface,
                                               const std::string& objPath,
                                               const NumericSensorInfo& info,
                                               const uuid_t& uuid,
                                               NsmDevice* nsmDevice)
{
    auto& bus = utils::DBusHandler::getBus();

    const auto peakValueInterface = interface + ".PeakValue";

    NumericSensorInfo peakValueInfo{};

    peakValueInfo.name = info.name;

    peakValueInfo.type = info.type + "_PeakValue";

    peakValueInfo.sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "SensorId", peakValueInterface.c_str());

    peakValueInfo.priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", peakValueInterface.c_str());

    peakValueInfo.aggregated = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Aggregated", peakValueInterface.c_str());

    if (info.type == "NSM_Power")
    {
        PeakPowerSensorBuilder builder;

        auto sensor = builder.makeSensor(peakValueInterface, objPath, bus,
                                         peakValueInfo);
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
