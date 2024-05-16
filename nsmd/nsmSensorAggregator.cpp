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

#include "nsmSensorAggregator.hpp"

#include "platform-environmental.h"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
NsmSensorAggregator::NsmSensorAggregator(const std::string& name,
                                         const std::string& type) :
    NsmSensor(name, type)
{
    samples.reserve(256);
}

uint8_t NsmSensorAggregator::handleResponseMsg(const nsm_msg* responseMsg,
                                               size_t responseLen)
{
    uint8_t cc{};
    uint16_t telemetry_count{};
    size_t consumed_len{};
    auto response_data = reinterpret_cast<const uint8_t*>(responseMsg);

    auto rc = decode_aggregate_resp(responseMsg, responseLen, &consumed_len,
                                    &cc, &telemetry_count);

    if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
    {
        lg2::error("responseHandler: decode_aggregate_resp failed. "
                   "Type={TYPE} sensor={NAME} rc={RC} cc={CC}.",
                   "TYPE", getType(), "NAME", getName(), "RC", rc, "CC", cc);
        return rc;
    }

    samples.clear();

    while (telemetry_count--)
    {
        uint8_t tag;
        bool valid;
        const uint8_t* data;
        size_t data_len;

        responseLen -= consumed_len;
        response_data += consumed_len;

        auto sample =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(response_data);

        rc = decode_aggregate_resp_sample(sample, responseLen, &consumed_len,
                                          &tag, &valid, &data, &data_len);

        if (rc != NSM_SW_SUCCESS || !valid)
        {
            lg2::error(
                "responseHandler: decode_aggregate_resp_sample failed. "
                "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}, valid_bit={VALID}",
                "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC", rc,
                "VALID", valid);

            continue;
        }

        samples.emplace_back(tag, data_len, data);
    }

    rc = handleSamples(samples);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::warning(
            "responseHandler: decoding failed for one or more samples. "
            "Type={TYPE}, sensor={NAME}, rc={RC}",
            "TYPE", getType(), "NAME", getName(), "RC", rc);
    }

    return rc;
}
} // namespace nsm
