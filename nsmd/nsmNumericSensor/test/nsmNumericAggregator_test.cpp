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

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::FieldsAre;

#include "base.h"
#include "platform-environmental.h"

#include "nsmNumericSensorValue_mock.hpp"

#define private public
#define protected public

#include "nsmNumericAggregator.hpp"

using namespace nsm;

MATCHER_P2(ArrayPointee, size, subMatcher, "")
{
    return ExplainMatchResult(subMatcher, std::make_tuple(arg, size),
                              result_listener);
}

class MockNsmSensorAggregator : public NsmSensorAggregator
{
  public:
    using NsmSensorAggregator::NsmSensorAggregator;

  public:
    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t eid, uint8_t instanceId), (override));
    MOCK_METHOD(int, handleSamples,
                (const std::vector<TelemetrySample>& samples), (override));
};

class MockNsmNumericAggregator : public NsmNumericAggregator
{
  public:
    using NsmNumericAggregator::NsmNumericAggregator;

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t eid, uint8_t instanceId), (override));
    MOCK_METHOD(int, handleSamples,
                (const std::vector<TelemetrySample>& samples), (override));
};

TEST(nsmSensorAggregator, GoodTest)
{
    const auto name = "Numeric Sensor";
    MockNsmSensorAggregator aggregator{name, "GetSensorReadingAggregate"};
    const uint8_t instance_id{30};
    const std::array<uint8_t, 2> tags{0, 39};
    constexpr size_t data_len{4};

    // Generate response of aggregate type.
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());

    auto rc = encode_aggregate_resp(instance_id, 0x01, NSM_SUCCESS, tags.size(),
                                    responseMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    const uint8_t reading[2][data_len]{{0x23, 0x44, 0x45, 0x00},
                                       {0x98, 0x78, 0x90, 0x46}};
    size_t consumed_len;
    std::array<uint8_t, 50> sample;
    auto nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());

    // add sample 1
    rc = encode_aggregate_resp_sample(tags[0], true, reading[0], 4, nsm_sample,
                                      &consumed_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    // add sample 2
    rc = encode_aggregate_resp_sample(tags[1], true, reading[1], 4, nsm_sample,
                                      &consumed_len);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    EXPECT_CALL(
        aggregator,
        handleSamples(ElementsAre(
            FieldsAre(tags[0], data_len,
                      ArrayPointee(data_len, ElementsAreArray(reading[0])),
                      true),
            FieldsAre(tags[1], data_len,
                      ArrayPointee(data_len, ElementsAreArray(reading[1])),
                      true))))
        .Times(1);

    // Invoke expected method
    aggregator.handleResponseMsg(reinterpret_cast<nsm_msg*>(response.data()),
                                 response.size());
}

TEST(nsmNumericSensorAggregator, GoodTest)
{
    MockNsmNumericAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                        true};

    EXPECT_EQ(aggregator.sensors[12], nullptr);

    auto sensor1 = std::make_shared<MockNsmNumericSensorValueAggregate>();
    auto sensor2 = std::make_shared<MockNsmNumericSensorValueAggregate>();

    uint8_t rc;

    rc = aggregator.addSensor(aggregator.TIMESTAMP, sensor1);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    rc = aggregator.addSensor(1, sensor1);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    rc = aggregator.addSensor(24, sensor2);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    EXPECT_EQ(aggregator.sensors[1].get(), sensor1.get());
    EXPECT_EQ(aggregator.sensors[24].get(), sensor2.get());

    const double reading1{343780.348};
    const double reading2{9843.384730};
    const uint64_t timestamp1{43889};
    const uint64_t timestamp2{3458277};

    EXPECT_CALL(*sensor1, updateReading(reading1, timestamp1)).Times(1);
    aggregator.updateSensorReading(1, reading1, timestamp1);

    EXPECT_CALL(*sensor2, updateReading(reading2, timestamp2)).Times(1);
    aggregator.updateSensorReading(24, reading2, timestamp2);
}
