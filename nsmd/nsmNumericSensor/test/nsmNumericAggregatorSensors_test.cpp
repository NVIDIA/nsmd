#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::DoubleNear;

#include "base.h"
#include "platform-environmental.h"

#include "nsmNumericSensorValue_mock.hpp"

#define private public
#define protected public

#include "nsmEnergyAggregator.hpp"
#include "nsmPowerAggregator.hpp"
#include "nsmTempAggregator.hpp"
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
    auto sensor = std::make_shared<MockNsmNumericSensorValue>();

    aggregator.addSensor(1, sensor);

    const double reading{58.49488};
    std::array<uint8_t, 4> sample;
    size_t data_size;

    auto rc = encode_aggregate_temperature_reading_data(reading, sample.data(),
                                                        &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(DoubleNear(reading, 0.01), 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data()}});
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
        {{1, static_cast<uint8_t>(data_size), nullptr}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data()}});
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
    auto sensor = std::make_shared<MockNsmNumericSensorValue>();

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
          timestamp_sample.data()},
         {1, static_cast<uint8_t>(reading_datasize), reading_sample.data()}});
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
        {{1, static_cast<uint8_t>(reading_datasize), nullptr}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(reading_datasize - 1),
          reading_sample.data()}});
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
    auto sensor = std::make_shared<MockNsmNumericSensorValue>();

    aggregator.addSensor(1, sensor);

    const uint64_t reading{3437844348};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc =
        encode_aggregate_energy_count_data(reading, sample.data(), &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(reading, 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data()}});
}

TEST(nsmEnergySensorAggregator, BadHandleSampleData)
{
    NsmEnergyAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                   false};

    const uint64_t reading{3437844348};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc =
        encode_aggregate_energy_count_data(reading, sample.data(), &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), nullptr}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data()}});
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
    auto sensor = std::make_shared<MockNsmNumericSensorValue>();

    aggregator.addSensor(1, sensor);

    const uint32_t reading{903484034};
    std::array<uint8_t, sizeof(reading)> sample;
    size_t data_size;

    auto rc = encode_aggregate_voltage_data(reading, sample.data(), &data_size);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_CALL(*sensor, updateReading(reading / 1'000'000.0, 0)).Times(1);

    aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size), sample.data()}});
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
        {{1, static_cast<uint8_t>(data_size), nullptr}});
    EXPECT_EQ(rc, NSM_SW_ERROR_NULL);

    rc = aggregator.handleSamples(
        {{1, static_cast<uint8_t>(data_size - 1), sample.data()}});
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}
