#include <gmock/gmock.h>
#include <gtest/gtest.h>
using ::testing::_;
using ::testing::Args;
using ::testing::ElementsAreArray;

#include "base.h"
#include "platform-environmental.h"

#define private public
#define protected public

#include "nsmNumericAggregator.hpp"

using namespace nsm;

class MockNumericNsmSensorAggregator : public NsmNumericAggregator
{
  public:
    using NsmNumericAggregator::NsmNumericAggregator;

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t eid, uint8_t instanceId), (override));
    MOCK_METHOD(int, handleSampleData,
                (uint8_t tag, const uint8_t* data, size_t data_len),
                (override));
};

TEST(nsmNumericSensorAggregator, GoodTest)
{
    const auto name = "Numeric Sensor";
    MockNumericNsmSensorAggregator aggregator{name, "GetSensorReadingAggregate",
                                              true};
    const uint8_t instance_id{30};
    const std::array<uint8_t, 2> tags{0, 39};
    constexpr size_t data_len{4};
    const float value{45.89};
    uint64_t timestamp{84730};

    // Generate response of aggregate type.
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_aggregate_resp(instance_id, 0x01, NSM_SUCCESS, tags.size(),
                                    responseMsg);

    const uint8_t reading[2][data_len]{{0x23, 0x44, 0x45, 0x00},
                                       {0x98, 0x78, 0x90, 0x46}};
    size_t consumed_len;
    std::array<uint8_t, 50> sample;
    auto nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

    // add sample 1
    rc = encode_aggregate_resp_sample(tags[0], true, reading[0], 4, nsm_sample,
                                      &consumed_len);
    assert(rc == NSM_SW_SUCCESS);

    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    // add sample 2
    rc = encode_aggregate_resp_sample(tags[1], true, reading[1], 4, nsm_sample,
                                      &consumed_len);
    assert(rc == NSM_SW_SUCCESS);

    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    // Expected method calls
    EXPECT_CALL(aggregator, handleSampleData(tags[0], _, _))
        .With(Args<1, 2>(ElementsAreArray(reading[0])))
        .Times(1);

    EXPECT_CALL(aggregator, handleSampleData(tags[1], _, _))
        .With(Args<1, 2>(ElementsAreArray(reading[1])))
        .Times(1);

    // Invoke expected method
    aggregator.handleResponseMsg(reinterpret_cast<nsm_msg*>(response.data()),
                                 response.size());
    aggregator.updateSensorReading(tags[0], value, timestamp);
}
/*
TEST(nsmTempSensorAggregator, GoodTest)
{
    NsmTempAggregator aggregator{"Sensor", "GetSensorReadingAggregate"};
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
*/
/*
TEST(nsmPowerSensorAggregator, GoodTest)
{
    NsmPowerAggregator aggregator{"Sensor", "GetSensorReadingAggregate", 1};
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
    EXPECT_EQ(command->averaging_interval, 1);
} */
