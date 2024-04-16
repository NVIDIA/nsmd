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



#include "nsmTempAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmTempAggregator::NsmTempAggregator(const std::string& name,
                                     const std::string& type, bool priority) :
    NsmNumericAggregator(name, type, priority)
{}

std::optional<std::vector<uint8_t>>
    NsmTempAggregator::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_get_temperature_reading_req));
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc =
        encode_get_temperature_reading_req(instanceId, sensorId, requestPtr);

    if (rc)
    {
        lg2::error("encode_get_temperature_reading_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmTempAggregator::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    double reading{};
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        if (sample.tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
        {
            continue;
        }

        auto rc = decode_aggregate_temperature_reading_data(
            sample.data, sample.data_len, &reading);

        if (rc == NSM_SW_SUCCESS)
        {
            updateSensorReading(sample.tag, reading);
        }
        else
        {
            lg2::error(
                "decode_aggregate_temperature_reading_data failed. rc={RC}.",
                "RC", rc);
            returnValue = rc;
            updateSensorNotWorking(sample.tag);
        }
    }

    return returnValue;
}
} // namespace nsm
