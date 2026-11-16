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

#include <optional>
#include <unordered_map>

namespace nsm
{

struct NumericSensorInfo;
class NsmNumericSensor;
class NsmDevice;
class NsmThresholdValue;

/** @brief One threshold tier given by value; static when
 *  parameterId is empty, else read from the device by ParameterId
 */
struct FeatureThreshold
{
    double value{};
    std::optional<uint64_t> parameterId;
    bool periodicUpdate{false};
    bool priority{false};
    bool aggregated{false};
};

class NsmThresholdFactory
{
  public:
    /** @brief Thresholds given by value (tier name, e.g. "UpperFatal", to
     *  threshold)
     */
    NsmThresholdFactory(
        SensorManager& manager, std::shared_ptr<NsmNumericSensor> numericSensor,
        const NumericSensorInfo& info, const uuid_t& uuid,
        std::shared_ptr<NsmDevice> nsmDevice,
        std::unordered_map<std::string, FeatureThreshold> featureThresholds);
    requester::Coroutine make();

  private:
    struct ThresholdsPairInfo
    {
        std::string lowerThreshold;
        std::string upperThreshold;
    };

    // outIntf, when non-null, receives the created tier D-Bus interface
    // object (e.g. ThresholdWarningIntf) on success, for use by the caller
    // in constructing the sensor's NsmThresholdEvaluator. Left untouched if
    // neither threshold of the pair is present or creation fails.
    template <typename DBusIntf,
              std::derived_from<NsmThresholdValue> ThresholdValueLow,
              std::derived_from<NsmThresholdValue> ThresholdValueHigh>
    requester::Coroutine processThresholdsPair(
        const std::unordered_map<std::string, std::string>& thresholdInterfaces,
        const ThresholdsPairInfo& thresholdsPairInfo,
        std::shared_ptr<DBusIntf>* outIntf = nullptr);

    requester::Coroutine
        createNsmThreshold(const std::string& intfName,
                           const std::string& thresholdType,
                           std::unique_ptr<NsmThresholdValue> thresholdValue);

  private:
    SensorManager& manager;
    std::shared_ptr<NsmNumericSensor> numericSensor;
    const NumericSensorInfo& info;
    const uuid_t& uuid;
    std::shared_ptr<NsmDevice> nsmDevice;
    std::unordered_map<std::string, FeatureThreshold> featureThresholds;
};

} // namespace nsm
