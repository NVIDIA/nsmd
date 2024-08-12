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

#include "nsmPeakPowerAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmPeakPowerAggregator::NsmPeakPowerAggregator(const std::string& name,
                                               const std::string& type,
                                               bool priority,
                                               uint8_t averagingInterval) :
    NsmNumericAggregator(name, type, priority),
    averagingInterval(averagingInterval)
{}

std::optional<std::vector<uint8_t>>
    NsmPeakPowerAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_get_max_observed_power_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_max_observed_power_req(instanceId, sensorId,
                                                averagingInterval, requestPtr);

    if (rc)
    {
        lg2::error("encode_get_max_observed_power_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmPeakPowerAggregator::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    uint32_t reading{};
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        if (!sample.valid)
        {
            updateSensorNotWorking(sample.tag, sample.valid);
            continue;
        }

        if (sample.tag == TIMESTAMP)
        {
            auto rc = decode_aggregate_timestamp_data(
                sample.data, sample.data_len, &timestamp);

            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error("decode_aggregate_timestamp_data failed. rc={RC}.",
                           "RC", rc);
                returnValue = rc;
            }
        }
        else if (sample.tag <= NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            auto rc = decode_aggregate_get_current_power_draw_reading(
                sample.data, sample.data_len, &reading);

            if (rc == NSM_SW_SUCCESS)
            {
                // unit of power is milliwatt in NSM Command Response and
                // selected unit in SensorValue PDI is Watts. Hence it is
                // converted to Watts.
                updateSensorReading(sample.tag, reading / 1000.0, timestamp);
            }
            else
            {
                lg2::error(
                    "decode_aggregate_get_current_power_draw_reading failed. rc={RC}.",
                    "RC", rc);
                returnValue = rc;
                updateSensorNotWorking(sample.tag, sample.valid);
            }
        }
    }

    return returnValue;
}
} // namespace nsm
