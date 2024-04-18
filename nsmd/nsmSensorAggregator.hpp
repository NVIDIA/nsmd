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

#pragma once

#include "base.h"

#include "nsmSensor.hpp"

#include <cstdint>

namespace nsm
{
/** @class NsmSensorAggregator
 *
 * Abstract class to provide common functionalities of NSM Aggregate command.
 * To add a support for new NSM Aggregate command, derive publicly from it
 * and put command specific details (typically calls to libnsm) in its two pure
 * virtual methods.
 */
class NsmSensorAggregator : public NsmSensor
{
  public:
    NsmSensorAggregator(const std::string& name, const std::string& type);
    virtual ~NsmSensorAggregator() = default;

    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) final;

    struct TelemetrySample
    {
        uint8_t tag;
        uint8_t data_len;
        const uint8_t* data;
    };

  private:
    /** @brief this function will be called for each telemetry sample found in
     * response message. You might want to call updateSensorReading after
     * decoding value from the data. Special Tag values (i.e. timestamp, uuid
     * etc) are expected to be handled by this function.
     *
     *  @param[in] tag - tag of telemetry sample
     *  @param[in] data - data of telemetry sample
     *  @param[in] data_len - number of bytes in data of telemetry sample
     *  @return nsm_completion_codes
     */
    virtual int handleSamples(const std::vector<TelemetrySample>& samples) = 0;

  protected:
    enum SpecialTag : uint8_t
    {
        UUID = 0xFE,
        TIMESTAMP = 0xFF
    };

    std::vector<TelemetrySample> samples;
};
} // namespace nsm
