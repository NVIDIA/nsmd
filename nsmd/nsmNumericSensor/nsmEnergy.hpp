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

#include "nsmNumericSensor.hpp"
#include "nsmSensor.hpp"

namespace nsm
{
class NsmEnergy : public NsmNumericSensor
{
  public:
    NsmEnergy(sdbusplus::bus::bus& bus, const std::string& name,
              const std::string& type, uint8_t sensorId,
              const std::vector<utils::Association>& association,
              const std::string& chassis_association,
              const std::string& physicalContext,
              const std::string* implementation, const double maxAllowableValue,
              const std::string* readingBasis, const std::string* description);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    std::string getSensorType() override
    {
        return "energy";
    }
};

} // namespace nsm
