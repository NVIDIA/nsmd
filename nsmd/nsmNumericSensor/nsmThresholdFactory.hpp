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

#include "sensorManager.hpp"

#include <sdbusplus/asio/object_server.hpp>

namespace nsm
{

struct NumericSensorInfo;
class NsmNumericSensor;
class NsmDevice;
class NsmThresholdValue;

class NsmThresholdFactory
{
  public:
    NsmThresholdFactory(SensorManager& manager, const std::string& interface,
                        const std::string& objPath,
                        std::shared_ptr<NsmNumericSensor> numericSensor,
                        const NumericSensorInfo& info, const uuid_t& uuid);
    void make();

  private:
    struct ThresholdsPairInfo
    {
        std::string lowerThreshold;
        std::string upperThreshold;
    };

    std::unordered_map<std::string, std::string> getThresholdInterfaces();

    template <typename DBusIntf,
              std::derived_from<NsmThresholdValue> ThresholdValueLow,
              std::derived_from<NsmThresholdValue> ThresholdValueHigh>
    void processThresholdsPair(
        const std::unordered_map<std::string, std::string>& thresholdInterfaces,
        const ThresholdsPairInfo& thresholdsPairInfo);

    void createNsmThreshold(const std::string& intfName,
                            const std::string& thresholdType,
                            std::unique_ptr<NsmThresholdValue> thresholdValue);

  private:
    SensorManager& manager;
    const std::string interface;
    const std::string objPath;
    std::shared_ptr<NsmNumericSensor> numericSensor;
    const NumericSensorInfo& info;
    const uuid_t& uuid;
    std::shared_ptr<NsmDevice> nsmDevice;
};

} // namespace nsm
