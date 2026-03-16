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

/*
 * nsmBatch16_test.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmSensor.cpp
 *      - NsmSensor::equals() (line 66-73): comparison returns true when
 *        both sensors produce identical genRequestMsg output; returns false
 *        when outputs differ or either returns nullopt.
 *      - NsmSensor::operator==() (line 75-78): delegates to equals().
 */

#include "base.h"
#include "platform-environmental.h"

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensor/nsmTemp.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Test helpers
// ============================================================================

static auto& bus = utils::DBusHandler::getBus();
static const std::vector<utils::Association>
    associations({{"chassis", "all_sensors",
                   "/xyz/openbmc_project/inventory/dummy_device"}});
static const std::string physicalContext("GPU");
static const double maxAllowableValue{std::numeric_limits<double>::infinity()};
static const double maxValue{std::numeric_limits<double>::quiet_NaN()};
static const double minValue{std::numeric_limits<double>::quiet_NaN()};
static const std::string readingBasis("Headroom");
static const std::string description("test_sensor");

// Minimal NsmSensor subclass that always returns nullopt from genRequestMsg
class NullRequestSensor : public NsmSensor
{
  public:
    NullRequestSensor(const std::string& name, const std::string& type) :
        NsmSensor(name, type)
    {}

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t /*eid*/, uint8_t /*instanceId*/) override
    {
        return std::nullopt;
    }

    uint8_t handleResponseMsg(const struct nsm_msg* /*responseMsg*/,
                              size_t /*responseLen*/) override
    {
        return NSM_SW_SUCCESS;
    }
};

// ============================================================================
// NsmSensor::equals() tests
// ============================================================================

// Two NsmTemp sensors with the same sensorId produce identical request
// messages (same NSM command + same sensor_id field). equals() must return
// true.
TEST(NsmSensorEquals, SameSensorType_SameSensorId_ReturnsTrue)
{
    NsmTemp sensorA{bus,
                    "temp_sensor_A",
                    "NSM_Temp",
                    5,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_sensor_B",
                    "NSM_Temp",
                    5,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_TRUE(sensorA.equals(sensorB));
    EXPECT_TRUE(sensorB.equals(sensorA));
}

// Two NsmTemp sensors with different sensorIds produce different request
// messages. equals() must return false.
TEST(NsmSensorEquals, SameSensorType_DifferentSensorId_ReturnsFalse)
{
    NsmTemp sensorA{bus,
                    "temp_sensor_C",
                    "NSM_Temp",
                    1,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_sensor_D",
                    "NSM_Temp",
                    2,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_FALSE(sensorA.equals(sensorB));
    EXPECT_FALSE(sensorB.equals(sensorA));
}

// Comparing a sensor to itself must return true.
TEST(NsmSensorEquals, SelfComparison_ReturnsTrue)
{
    NsmTemp sensor{bus,
                   "temp_sensor_self",
                   "NSM_Temp",
                   3,
                   associations,
                   associations[0].absolutePath,
                   physicalContext,
                   nullptr,
                   maxAllowableValue,
                   maxValue,
                   minValue,
                   &readingBasis,
                   &description};

    EXPECT_TRUE(sensor.equals(sensor));
}

// When either sensor returns nullopt from genRequestMsg, equals() returns
// false because the short-circuit "requestMsg && sensorRequestMsg" fails.
TEST(NsmSensorEquals, NullRequestSensor_ReturnsFalse)
{
    NsmTemp normalSensor{bus,
                         "temp_sensor_normal",
                         "NSM_Temp",
                         7,
                         associations,
                         associations[0].absolutePath,
                         physicalContext,
                         nullptr,
                         maxAllowableValue,
                         maxValue,
                         minValue,
                         &readingBasis,
                         &description};
    NullRequestSensor nullSensor("null_sensor", "NSM_Null");

    EXPECT_FALSE(normalSensor.equals(nullSensor));
    EXPECT_FALSE(nullSensor.equals(normalSensor));
}

// Two NullRequestSensors: both return nullopt; result is false because
// "requestMsg && sensorRequestMsg" evaluates to false.
TEST(NsmSensorEquals, BothNullRequest_ReturnsFalse)
{
    NullRequestSensor sensorA("null_A", "NSM_Null");
    NullRequestSensor sensorB("null_B", "NSM_Null");

    EXPECT_FALSE(sensorA.equals(sensorB));
}

// ============================================================================
// NsmSensor::operator==() tests — delegates to equals()
// ============================================================================

TEST(NsmSensorOperatorEqual, SameSensorId_ReturnsTrue)
{
    NsmTemp sensorA{bus,
                    "temp_op_A",
                    "NSM_Temp",
                    10,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_op_B",
                    "NSM_Temp",
                    10,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_TRUE(sensorA == sensorB);
}

TEST(NsmSensorOperatorEqual, DifferentSensorId_ReturnsFalse)
{
    NsmTemp sensorA{bus,
                    "temp_op_C",
                    "NSM_Temp",
                    11,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};
    NsmTemp sensorB{bus,
                    "temp_op_D",
                    "NSM_Temp",
                    12,
                    associations,
                    associations[0].absolutePath,
                    physicalContext,
                    nullptr,
                    maxAllowableValue,
                    maxValue,
                    minValue,
                    &readingBasis,
                    &description};

    EXPECT_FALSE(sensorA == sensorB);
}

TEST(NsmSensorOperatorEqual, SelfComparison_ReturnsTrue)
{
    NsmTemp sensor{bus,
                   "temp_op_self",
                   "NSM_Temp",
                   4,
                   associations,
                   associations[0].absolutePath,
                   physicalContext,
                   nullptr,
                   maxAllowableValue,
                   maxValue,
                   minValue,
                   &readingBasis,
                   &description};

    EXPECT_TRUE(sensor == sensor);
}
