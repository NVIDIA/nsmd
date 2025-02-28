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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::DoubleNear;

#include "base.h"
#include "platform-environmental.h"

#include "nsmNumericSensorValue_mock.hpp"

#define private public
#define protected public

#include "nsmEnergyAggregator.hpp"
#include "nsmPeakPowerAggregator.hpp"
#include "nsmPowerAggregator.hpp"
#include "nsmTempAggregator.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmVoltageAggregator.hpp"

using namespace nsm;

TEST(nsmTempSensorAggregator, GoodGenReq)
{
    NsmTempAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true};
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = aggregator.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_temperature_reading_req*>(msg->payload);
    EXPECT_EQ(command->hdr.command, NSM_GET_TEMPERATURE_READING);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->sensor_id, 255);
}

TEST(nsmTempSensorAggregator, GoodHandleSampleData)
{
    NsmTempAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();

    aggregator.addSensor(1, sensor);

    const double reading{58.49488};
    std::array<uint8_t, 4> sample;
    size_t data_size;

    auto rc = encode_aggregate_temperature_reading_data(reading, sample.data(),
                                                        &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(DoubleNear(reading, 0.01), 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data(), true}});
}

TEST(nsmTempSensorAggregator, BadHandleSampleData)
{
    NsmTempAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true};

    const double reading{58.49488};
    std::array<uint8_t, 4> sample;
    size_t data_size;

    auto rc = encode_aggregate_temperature_reading_data(reading, sample.data(),
                                                        &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), nullptr, true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data(), true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmPowerSensorAggregator, GoodGenReq)
{
    NsmPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true,
                                  0};
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = aggregator.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_current_power_draw_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_POWER);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->sensor_id, 255);
    EXPECT_EQ(command->averaging_interval, 0);
}

TEST(nsmPowerSensorAggregator, GoodHandleSampleData)
{
    NsmPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true,
                                  0};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();

    aggregator.addSensor(1, sensor);

    const uint32_t reading{903484034};
    const uint64_t timestamp{10945847};
    std::array<uint8_t, sizeof(timestamp)> timestamp_sample;
    std::array<uint8_t, sizeof(reading)> reading_sample;
    size_t timestamp_datasize;
    size_t reading_datasize;

    EXPECT_CALL(*sensor, updateReading(reading / 1000.0, 10945847)).Times(1);

    auto rc = encode_aggregate_timestamp_data(
        timestamp, timestamp_sample.data(), &timestamp_datasize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = encode_aggregate_get_current_power_draw_reading(
        reading, reading_sample.data(), &reading_datasize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    aggregator.handleSamples(
        {{aggregator.TIMESTAMP, static_cast<uint8_t>(timestamp_datasize),
          timestamp_sample.data(), true},
         {1, static_cast<uint8_t>(reading_datasize), reading_sample.data(),
          true}});
}

TEST(nsmPowerSensorAggregator, BadHandleSampleData)
{
    NsmPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true,
                                  0};

    const uint32_t reading{903484034};
    std::array<uint8_t, sizeof(reading)> reading_sample;
    size_t reading_datasize;

    auto rc = encode_aggregate_get_current_power_draw_reading(
        reading, reading_sample.data(), &reading_datasize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(reading_datasize), nullptr, true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(reading_datasize - 1), reading_sample.data(),
          true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmPeakPowerSensorAggregator, GoodGenReq)
{
    NsmPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate", true,
                                  0};
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = aggregator.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_current_power_draw_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_POWER);
    EXPECT_EQ(command->hdr.data_size, 2);
    EXPECT_EQ(command->sensor_id, 255);
    EXPECT_EQ(command->averaging_interval, 0);
}

TEST(nsmPeakPowerSensorAggregator, GoodHandleSampleData)
{
    NsmPeakPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                      true, 0};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();

    aggregator.addSensor(1, sensor);

    const uint32_t reading{903484034};
    const uint64_t timestamp{10945847};
    std::array<uint8_t, sizeof(timestamp)> timestamp_sample;
    std::array<uint8_t, sizeof(reading)> reading_sample;
    size_t timestamp_datasize;
    size_t reading_datasize;

    EXPECT_CALL(*sensor, updateReading(reading / 1000.0, 10945847)).Times(1);

    auto rc = encode_aggregate_timestamp_data(
        timestamp, timestamp_sample.data(), &timestamp_datasize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = encode_aggregate_get_current_power_draw_reading(
        reading, reading_sample.data(), &reading_datasize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    aggregator.handleSamples(
        {{aggregator.TIMESTAMP, static_cast<uint8_t>(timestamp_datasize),
          timestamp_sample.data(), true},
         {1, static_cast<uint8_t>(reading_datasize), reading_sample.data(),
          true}});
}

TEST(nsmPeakPowerSensorAggregator, BadHandleSampleData)
{
    NsmPeakPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                      true, 0};

    const uint32_t reading{903484034};
    std::array<uint8_t, sizeof(reading)> reading_sample;
    size_t reading_datasize;

    auto rc = encode_aggregate_get_current_power_draw_reading(
        reading, reading_sample.data(), &reading_datasize);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(reading_datasize), nullptr, true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(reading_datasize - 1), reading_sample.data(),
          true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmEnergySensorAggregator, GoodGenReq)
{
    NsmEnergyAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                   false};
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = aggregator.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_get_current_energy_count_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_ENERGY_COUNT);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->sensor_id, 255);
}

TEST(nsmEnergySensorAggregator, GoodHandleSampleData)
{
    NsmEnergyAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                   false};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();

    aggregator.addSensor(1, sensor);

    const uint64_t reading{3437844348};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_energy_count_data(reading, sample.data(),
                                                 &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(reading, 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data(), true}});
}

TEST(nsmEnergySensorAggregator, BadHandleSampleData)
{
    NsmEnergyAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                   false};

    const uint64_t reading{3437844348};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_energy_count_data(reading, sample.data(),
                                                 &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), nullptr, true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data(), true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmVoltageSensorAggregator, GoodGenReq)
{
    NsmVoltageAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                    false};
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = aggregator.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_get_voltage_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_GET_VOLTAGE);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->sensor_id, 255);
}

TEST(nsmVoltageSensorAggregator, GoodHandleSampleData)
{
    NsmVoltageAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                    false};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();

    aggregator.addSensor(1, sensor);

    const uint32_t reading{903484034};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_voltage_data(reading, sample.data(), &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(reading / 1'000'000.0, 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data(), true}});
}

TEST(nsmVoltageSensorAggregator, BadHandleSampleData)
{
    NsmVoltageAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                    false};

    const uint32_t reading{903484034};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_voltage_data(reading, sample.data(), &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), nullptr, true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data(), true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(nsmThresholdAggregator, GoodGenReq)
{
    NsmThresholdAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                      false};
    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = aggregator.genRequestMsg(eid, instance_id);
    EXPECT_EQ(request.has_value(), true);

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_read_thermal_parameter_req*>(msg->payload);

    EXPECT_EQ(command->hdr.command, NSM_READ_THERMAL_PARAMETER);
    EXPECT_EQ(command->hdr.data_size, 1);
    EXPECT_EQ(command->parameter_id, 255);
}

TEST(nsmThresholdAggregator, GoodHandleSampleData)
{
    NsmThresholdAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                      false};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();

    aggregator.addSensor(1, sensor);

    const int32_t reading{110};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_thermal_parameter_data(reading, sample.data(),
                                                      &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(reading, 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data(), true}});
}

TEST(nsmThresholdAggregator, BadHandleSampleData)
{
    NsmThresholdAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                      false};

    const int32_t reading{110};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_thermal_parameter_data(reading, sample.data(),
                                                      &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), nullptr, true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data(), true}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
