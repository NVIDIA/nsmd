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

#include "nsmSensor.hpp"

#include <ranges>

namespace nsm
{

/**
 * @brief Subsensor class to handle a single NSM response.
 */
class NsmSubSensor
{
  public:
    virtual uint8_t handleResponse(const nsm_msg* responseMsg,
                                   size_t responseLen) = 0;
};

/**
 * @brief Group sensor class to handle multiple subsensors with a single NSM
 * request and response.
 */
class NsmGroupSensor : public NsmSensor, public NsmSubSensor
{
  private:
    uint8_t handleResponseMsg(const nsm_msg* responseMsg,
                              size_t responseLen) override final
    {
        uint8_t rc = handleResponse(responseMsg, responseLen);
        if (rc != NSM_SW_SUCCESS)
        {
            return rc;
        }
        for (auto sensor : sensors)
        {
            rc = sensor->handleResponse(responseMsg, responseLen);
            if (rc != NSM_SW_SUCCESS)
            {
                break;
            }
        }
        return rc;
    }

  public:
    using NsmSensor::NsmSensor;
    std::vector<std::shared_ptr<NsmSubSensor>> sensors;
};

} // namespace nsm
