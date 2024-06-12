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

#include "config.h"

#include "nsmNumericSensor.hpp"

#include "sensorManager.hpp"
#include "utils.hpp"

#include <endian.h>

#include <tal.hpp>

namespace nsm
{
#ifdef NVIDIA_SHMEM
const std::string NsmNumericSensorShmem::valueInterface{
    "xyz.openbmc_project.Sensor.Value"};
const std::string NsmNumericSensorShmem::valueProperty{"Value"};
#endif

using namespace std::string_literals;

NsmNumericSensorDbusValue::NsmNumericSensorDbusValue(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::string& sensor_type, const SensorUnit unit,
    const std::vector<utils::Association>& associations,
    const std::string& physicalContext, const std::string* implementation,
    const double maxAllowableValue) :
    valueIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str()),
    associationDefinitionsIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str()),
    decoratorAreaIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str())
{
    valueIntf.unit(unit);
    valueIntf.maxAllowableValue(maxAllowableValue);
    decoratorAreaIntf.physicalContext(
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Area::
            convertPhysicalContextTypeFromString(
                "xyz.openbmc_project.Inventory.Decorator.Area.PhysicalContextType." +
                physicalContext));

    if (implementation)
    {
        typeIntf = std::make_unique<TypeIntf>(
            bus, ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name)
                     .c_str());

        typeIntf->implementation(
            sdbusplus::common::xyz::openbmc_project::sensor::Type::
                convertImplementationTypeFromString(
                    "xyz.openbmc_project.Sensor.Type.ImplementationType." +
                    *implementation));
    }

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefinitionsIntf.associations(associations_list);

    updateReading(std::numeric_limits<double>::quiet_NaN());
}

void NsmNumericSensorDbusValue::updateReading(double value,
                                              uint64_t /*timestamp*/)
{
    valueIntf.value(value);
}

NsmNumericSensorDbusValueTimestamp::NsmNumericSensorDbusValueTimestamp(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::string& sensor_type, const SensorUnit unit,
    const std::vector<utils::Association>& association,
    const std::string& physicalContext, const std::string* implementation,
    const double maxAllowableValue) :
    NsmNumericSensorDbusValue(bus, name, sensor_type, unit, association,
                              physicalContext, implementation,
                              maxAllowableValue),
    timestampIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str())
{}

void NsmNumericSensorDbusValueTimestamp::updateReading(double value,
                                                       uint64_t timestamp)
{
    timestampIntf.elapsed(timestamp);
    NsmNumericSensorDbusValue::updateReading(value);
}

void NsmNumericSensorValueAggregate::append(
    std::unique_ptr<NsmNumericSensorValue> elem)
{
    objects.push_back(std::move(elem));
}

void NsmNumericSensorValueAggregate::updateReading(double value,
                                                   uint64_t timestamp)
{
    for (const auto& elem : objects)
    {
        elem->updateReading(value, timestamp);
    }
}

NsmNumericSensorDbusStatus::NsmNumericSensorDbusStatus(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::string& sensor_type) :
    availabilityIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str()),
    operationalStatusIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str())
{
    availabilityIntf.available(true);
    operationalStatusIntf.functional(true);
}

void NsmNumericSensorDbusStatus::updateStatus(bool available, bool functional)
{
    availabilityIntf.available(available);
    operationalStatusIntf.functional(functional);
}

std::vector<uint8_t> SMBPBIPowerSMBusSensorBytesConverter::convert(double val)
{
    std::vector<uint8_t> data(4);
    // unit of power is milliwatt in SMBus Sensors and selected unit
    // in SensorValue PDI is Watts. Hence it is converted to milliwatts.
    auto smbusVal = static_cast<uint32_t>(val * 1000.0);
    smbusVal = htole32(smbusVal);
    std::memcpy(data.data(), &smbusVal, 4);

    return data;
}

std::vector<uint8_t> SFxP24F8SMBusSensorBytesConverter::convert(double val)
{
    std::vector<uint8_t> data(4);
    auto smbusVal = static_cast<int32_t>(val * (1 << 8));
    smbusVal = htole32(smbusVal);
    std::memcpy(data.data(), &smbusVal, 4);

    return data;
}

std::vector<uint8_t> Uint64SMBusSensorBytesConverter::convert(double val)
{
    std::vector<uint8_t> data(8);
    auto smbusVal = static_cast<uint64_t>(val);
    smbusVal = htole64(smbusVal);
    std::memcpy(data.data(), &smbusVal, 8);

    return data;
}

#ifdef NVIDIA_SHMEM
NsmNumericSensorShmem::NsmNumericSensorShmem(
    const std::string& name, const std::string& sensor_type,
    const std::string& association,
    std::unique_ptr<SMBusSensorBytesConverter> smbusSensorBytesConverter) :
    objPath("/xyz/openbmc_project/sensors/" + sensor_type + '/' + name),
    association(association),
    smbusSensorBytesConverter(std::move(smbusSensorBytesConverter))
{
    updateReading(std::numeric_limits<double>::quiet_NaN());
}

void NsmNumericSensorShmem::updateReading(double value, uint64_t /*timestamp*/)
{
    auto timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    nv::sensor_aggregation::DbusVariantType valueVariant{value};

    std::vector<uint8_t> smbusData = smbusSensorBytesConverter->convert(value);

    tal::TelemetryAggregator::updateTelemetry(
        objPath, valueInterface, valueProperty, smbusData, timestamp, 0,
        valueVariant, association);
}
#endif

NsmNumericSensorCompositeChildValue::NsmNumericSensorCompositeChildValue(
    const std::string& name, const std::string& sensorType,
    const std::vector<std::string>& parents) :
    name(name),
    sensorType(sensorType), parents(parents)
{}

void NsmNumericSensorCompositeChildValue::updateReading(double value,
                                                        uint64_t /*timestamp*/)
{
    SensorManager& manager = SensorManager::getInstance();

    // Iterate through the parents vector to check if it corresponding sensor is
    // created. when all sensors are cached parents container wil be empty
    for (auto it = parents.begin(); it != parents.end();)
    {
        auto sensorIt = manager.objectPathToSensorMap.find(*it);
        if (sensorIt != manager.objectPathToSensorMap.end())
        {
            auto sensor = sensorIt->second;
            if (sensor)
            {
                sensorCache.emplace_back(
                    std::dynamic_pointer_cast<NsmNumericSensorComposite>(
                        sensor));
                it = parents.erase(it);
                continue;
            }
        }
        ++it;
    }

    // update each cached sensor
    for (const auto& sensor : sensorCache)
    {
        sensor->updateCompositeReading(name, value);
    }
}

} // namespace nsm
