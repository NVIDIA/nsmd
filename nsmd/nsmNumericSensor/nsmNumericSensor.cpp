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

#include "utils.hpp"

#include <telemetry_mrd_producer.hpp>

namespace nsm
{
const std::string NsmNumericSensorShmem::valueInterface{
    "xyz.openbmc_project.Sensor.Value"};
const std::string NsmNumericSensorShmem::valueProperty{"Value"};

using namespace std::string_literals;

NsmNumericSensorDbusValue::NsmNumericSensorDbusValue(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::string& sensor_type, const SensorUnit unit,
    const std::vector<utils::Association>& associations) :
    valueIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str()),
    associationDefinitionsIntf(
        bus,
        ("/xyz/openbmc_project/sensors/"s + sensor_type + '/' + name).c_str())
{
    valueIntf.unit(unit);

    std::vector<std::tuple<std::string, std::string, std::string>>
        associations_list;
    for (const auto& association : associations)
    {
        associations_list.emplace_back(association.forward,
                                       association.backward,
                                       association.absolutePath);
    }
    associationDefinitionsIntf.associations(associations_list);
}

void NsmNumericSensorDbusValue::updateReading(double value,
                                              uint64_t /*timestamp*/)
{
    valueIntf.value(value);
}

NsmNumericSensorDbusValueTimestamp::NsmNumericSensorDbusValueTimestamp(
    sdbusplus::bus::bus& bus, const std::string& name,
    const std::string& sensor_type, const SensorUnit unit,
    const std::vector<utils::Association>& association) :
    NsmNumericSensorDbusValue(bus, name, sensor_type, unit, association),
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

NsmNumericSensorShmem::NsmNumericSensorShmem(const std::string& name,
                                             const std::string& sensor_type,
                                             const std::string& association) :
    objPath("/xyz/openbmc_project/sensors/" + sensor_type + '/' + name),
    association(association)
{
    DbusVariantType valueVariant{double{0}};

    nv::shmem::AggregationService::updateTelemetry(objPath, valueInterface,
                                                   valueProperty, valueVariant,
                                                   0, -1, association);
}

void NsmNumericSensorShmem::updateReading(double value, uint64_t /*timestamp*/)
{
    auto timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());

    DbusVariantType valueVariant{value};

    nv::shmem::AggregationService::updateTelemetry(objPath, valueInterface,
                                                   valueProperty, valueVariant,
                                                   timestamp, 0, association);
}

} // namespace nsm
