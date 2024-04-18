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

#include "nsmNumericAggregator.hpp"

#include "nsmNumericSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>

namespace nsm
{
int NsmNumericAggregator::addSensor(
    uint8_t tag, std::shared_ptr<NsmNumericSensorValue> sensor)
{
    if (tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
    {
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag] = sensor;

    return NSM_SW_SUCCESS;
}

int NsmNumericAggregator::updateSensorReading(uint8_t tag, double reading,
                                              uint64_t timestamp)
{
    if (!sensors[tag])
    {
        lg2::warning(
            "updateSensorReading: No NSM Sensor found for Tag {TAG} in "
            "NSM Aggregator {NAME} of type {TYPE}.",
            "TAG", tag, "NAME", getName(), "TYPE", getType());
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag]->updateReading(reading, timestamp);

    return NSM_SW_SUCCESS;
}

int NsmNumericAggregator::updateSensorNotWorking(uint8_t tag)
{
    if (!sensors[tag])
    {
        lg2::warning(
            "updateSensorReading: No NSM Sensor found for Tag {TAG} in "
            "NSM Aggregator {NAME} of type {TYPE}.",
            "TAG", tag, "NAME", getName(), "TYPE", getType());
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag]->updateReading(std::numeric_limits<double>::signaling_NaN());

    return NSM_SW_SUCCESS;
}
} // namespace nsm
