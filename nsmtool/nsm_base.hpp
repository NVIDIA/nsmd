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

#include "base.h"
#include "platform-environmental.h"

#include "cmd_helper.hpp"
#include "utils.hpp"

#include <CLI/CLI.hpp>

#pragma once

namespace nsmtool
{

namespace
{
using namespace nsmtool::helper;
std::vector<std::unique_ptr<CommandInterface>> commands;
} // namespace

class AggregateResponseParser
{
  private:
    virtual int handleSampleData(uint8_t tag, const uint8_t* data,
                                 size_t data_len,
                                 ordered_json& sample_json) = 0;

  public:
    virtual ~AggregateResponseParser() = default;

    void parseAggregateResponse(nsm_msg* responsePtr, size_t payloadLength)
    {
        uint8_t cc{};
        uint16_t telemetry_count{};
        size_t consumed_len{};
        size_t msg_len = payloadLength + sizeof(nsm_msg_hdr);
        auto response_data = reinterpret_cast<const uint8_t*>(responsePtr);

        auto rc = decode_aggregate_resp(responsePtr, msg_len, &consumed_len,
                                        &cc, &telemetry_count);

        if (rc != NSM_SW_SUCCESS)
        {
            std::cerr << "Response message error: "
                      << "rc=" << rc << ", cc=" << (int)cc << "\n";

            return;
        }

        ordered_json result;
        result["Completion Code"] = cc;
        result["Sample Count"] = telemetry_count;

        uint64_t timestamp{};
        constexpr size_t time_size{100};
        char time[time_size];
        bool has_timestamp{false};

        std::vector<ordered_json> samples;
        while (telemetry_count--)
        {
            uint8_t tag{};
            bool valid;
            const uint8_t* data;
            size_t data_len;

            msg_len -= consumed_len;

            response_data += consumed_len;

            auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(
                response_data);

            rc = decode_aggregate_resp_sample(sample, msg_len, &consumed_len,
                                              &tag, &valid, &data, &data_len);

            if (rc != NSM_SW_SUCCESS || !valid)
            {
                std::cerr
                    << "Response message error while parsing sample header: "
                    << "tag=" << static_cast<int>(tag) << ", rc=" << rc << "\n";

                continue;
            }

            if (tag == 0xFF)
            {
                if (data_len != 8)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample : "
                        << "tag=" << static_cast<int>(tag) << ", rc=" << rc
                        << "\n";

                    continue;
                }

                rc = decode_aggregate_timestamp_data(data, data_len,
                                                     &timestamp);
                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing timestamp sample data : "
                        << "tag=" << static_cast<int>(tag) << ", rc=" << rc
                        << "\n";

                    continue;
                }

                has_timestamp = true;
                auto stime = static_cast<std::time_t>(timestamp);
                std::strftime(time, time_size, "%F %T %Z",
                              std::localtime(&stime));
            }
            else if (tag < 0xF0)
            {
                ordered_json sample_json;

                rc = handleSampleData(tag, data, data_len, sample_json);

                if (rc != NSM_SW_SUCCESS)
                {
                    std::cerr
                        << "Response message error while parsing sample data: "
                        << "tag=" << static_cast<int>(tag) << ", rc=" << rc
                        << "\n";

                    continue;
                }

                if (has_timestamp)
                {
                    sample_json["Timestamp"] = time;
                }

                samples.push_back(std::move(sample_json));
            }
        }

        result["Samples"] = std::move(samples);
        nsmtool::helper::DisplayInJson(result);
    }
};
} // namespace nsmtool
