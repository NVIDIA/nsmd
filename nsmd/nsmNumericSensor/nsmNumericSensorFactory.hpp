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

#include "nsmDevice.hpp"
#include "nsmNumericAggregator.hpp"
#include "nsmNumericSensor.hpp"
#include "nsmObjectFactory.hpp"

#include <string>
#include <vector>

namespace nsm
{

struct NumericSensorInfo
{
    std::string name;
    std::string type;
    uint8_t sensorId;
    std::vector<utils::Association> associations;
    std::string chassis_association;
    std::string physicalContext;
    std::unique_ptr<std::string> implementation{};
    bool priority;
    bool aggregated;
    double maxAllowableValue{std::numeric_limits<double>::infinity()};
    std::unique_ptr<std::string> readingBasis{};
    std::unique_ptr<std::string> description{};
};

class NumericSensorAggregatorBuilder
{
  public:
    virtual ~NumericSensorAggregatorBuilder() = default;

    virtual std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) = 0;
};

class NumericSensorBuilder : public NumericSensorAggregatorBuilder
{
  public:
    virtual ~NumericSensorBuilder() = default;

    virtual std::shared_ptr<NsmNumericSensor>
        makeSensor(const std::string& interface, const std::string& objPath,
                   sdbusplus::bus::bus& bus, const NumericSensorInfo& info) = 0;

    virtual std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) = 0;
};

class NumericSensorFactory
{
  public:
    NumericSensorFactory(std::unique_ptr<NumericSensorBuilder> builder) :
        builder(std::move(builder)){};

    CreationFunction getCreationFunction();

    requester::Coroutine make(SensorManager& manager,
                              const std::string& interface,
                              const std::string& objPath);

    static void
        makeAggregatorAndAddSensor(NumericSensorAggregatorBuilder* builder,
                                   const NumericSensorInfo& info,
                                   std::shared_ptr<NsmNumericSensor> sensor,
                                   const uuid_t& uuid, NsmDevice* nsmDevice);

    static void makePeakValueAndAdd(const std::string& interface,
                                    const std::string& objPath,
                                    const NumericSensorInfo& info,
                                    const uuid_t& uuid, NsmDevice* nsmDevice);

  private:
    std::unique_ptr<NumericSensorBuilder> builder;
};

}; // namespace nsm
