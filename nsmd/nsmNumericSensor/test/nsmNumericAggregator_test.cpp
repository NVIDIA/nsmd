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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::_;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::FieldsAre;
using ::testing::NiceMock;
using ::testing::Return;

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
    MOCK_METHOD(int, handleSample, (const TelemetrySample& sample), (override));
};

class MockNsmNumericAggregator : public NsmNumericAggregator
{
  public:
    using NsmNumericAggregator::NsmNumericAggregator;

    MOCK_METHOD(std::optional<std::vector<uint8_t>>, genRequestMsg,
                (eid_t eid, uint8_t instanceId), (override));
    MOCK_METHOD(int, handleSample, (const TelemetrySample& sample), (override));
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
        handleSample(FieldsAre(
            tags[0], data_len,
            ArrayPointee(data_len, ElementsAreArray(reading[0])), true)))
        .Times(1);
    EXPECT_CALL(
        aggregator,
        handleSample(FieldsAre(
            tags[1], data_len,
            ArrayPointee(data_len, ElementsAreArray(reading[1])), true)))
        .Times(1);

    // Invoke expected method
    aggregator.handleResponseMsg(reinterpret_cast<nsm_msg*>(response.data()),
                                 response.size());
}

// handleResponseMsg – decode_aggregate_resp_sample() failure path (line 71).
// When telemetryCount=1 but the buffer has no sample payload, the decode
// of the sample fails → the `continue` branch is taken and the loop ends
// without calling handleSample(). Overall return is NSM_SW_SUCCESS because
// returnValue is never set (we just skip bad samples).
TEST(nsmSensorAggregator, DecodeSampleFails_ContinueBranchTaken_ReturnSuccess)
{
    MockNsmSensorAggregator aggregator{"BadSample",
                                       "GetSensorReadingAggregate"};

    // Aggregate header claims 1 sample, but no sample bytes follow
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    ASSERT_EQ(encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg),
              NSM_SW_SUCCESS);
    // No sample data appended → responseLen after header consumption is 0
    // → decode_aggregate_resp_sample will return NSM_SW_ERROR_LENGTH

    // handleSample must NOT be called (bad sample is skipped via continue)
    EXPECT_CALL(aggregator, handleSample).Times(0);

    auto rc = aggregator.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handleResponseMsg – handleSample() returns error for first sample.
// Line 84 branch: returnValue == NSM_SW_SUCCESS → sets returnValue = rc.
// The overall return should be NSM_SW_ERROR_DATA.
TEST(nsmSensorAggregator, HandleSampleError_FirstSample_PropagatesError)
{
    MockNsmSensorAggregator aggregator{"ErrSensor",
                                       "GetSensorReadingAggregate"};
    const uint8_t tag = 1;
    constexpr size_t data_len{4};
    const uint8_t reading[data_len]{0x01, 0x02, 0x03, 0x04};

    // Build aggregate response with one sample
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    ASSERT_EQ(encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 1, responseMsg),
              NSM_SW_SUCCESS);

    size_t consumed_len;
    std::array<uint8_t, 50> sample{};
    auto nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
    ASSERT_EQ(encode_aggregate_resp_sample(tag, true, reading, data_len,
                                           nsm_sample, &consumed_len),
              NSM_SW_SUCCESS);
    response.insert(response.end(), sample.begin(),
                    std::next(sample.begin(), consumed_len));

    // handleSample returns error → returnValue gets set to NSM_SW_ERROR_DATA
    EXPECT_CALL(aggregator, handleSample).WillOnce(Return(NSM_SW_ERROR_DATA));

    auto rc = aggregator.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// handleResponseMsg – handleSample() returns error for both samples.
// Second sample: returnValue is already != NSM_SW_SUCCESS, so the condition
// on line 84 is false and returnValue is NOT overwritten by the second rc.
TEST(nsmSensorAggregator, HandleSampleError_TwoSamples_ReturnFirstError)
{
    MockNsmSensorAggregator aggregator{"ErrSensor2",
                                       "GetSensorReadingAggregate"};
    const std::array<uint8_t, 2> tags{1, 2};
    constexpr size_t data_len{4};
    const uint8_t reading[data_len]{0xAA, 0xBB, 0xCC, 0xDD};

    // Build aggregate response with two samples
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    ASSERT_EQ(encode_aggregate_resp(0, 0x01, NSM_SUCCESS, 2, responseMsg),
              NSM_SW_SUCCESS);

    size_t consumed_len;
    std::array<uint8_t, 50> sample{};
    auto nsm_sample =
        reinterpret_cast<nsm_aggregate_resp_sample*>(sample.data());
    for (auto t : tags)
    {
        ASSERT_EQ(encode_aggregate_resp_sample(t, true, reading, data_len,
                                               nsm_sample, &consumed_len),
                  NSM_SW_SUCCESS);
        response.insert(response.end(), sample.begin(),
                        std::next(sample.begin(), consumed_len));
    }

    // Both handleSample calls return error; second one hits the false branch
    EXPECT_CALL(aggregator, handleSample)
        .WillOnce(Return(NSM_SW_ERROR_DATA))
        .WillOnce(Return(NSM_SW_ERROR_LENGTH));

    auto rc = aggregator.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(response.data()), response.size());
    // returnValue stays as NSM_SW_ERROR_DATA (set by first sample)
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
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

TEST(nsmSensorAggregator, HandleResponseMsg_DecodeFail_ReturnsError)
{
    MockNsmSensorAggregator aggregator{"DecodeFail",
                                       "GetSensorReadingAggregate"};

    // 7-byte buffer: decode_aggregate_resp requires 9 bytes minimum so the
    // short buffer triggers NSM_SW_ERROR_LENGTH before accessing any fields
    std::vector<uint8_t> response(sizeof(nsm_msg_hdr) + 2, 0);
    auto rc = aggregator.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(response.data()), response.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// updateSensorReading: tag with no registered sensor → !sensors[tag] TRUE
// → returns NSM_SW_ERROR_DATA (covers the null-sensor error branch).
TEST(nsmNumericSensorAggregator, UpdateSensorReading_NoSensor_ReturnsError)
{
    MockNsmNumericAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                        true};
    // Tag 5 has no sensor registered
    EXPECT_EQ(aggregator.sensors[5], nullptr);
    auto rc = aggregator.updateSensorReading(5, 1.0, 0);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// updateSensorNotWorking: tag with no registered sensor → !sensors[tag] TRUE
// → returns NSM_SW_ERROR_DATA.
// Also covers the success path (valid=true with sensor) for full branch
// coverage.
TEST(nsmNumericSensorAggregator, UpdateSensorNotWorking_NoSensor_ReturnsError)
{
    MockNsmNumericAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                        true};
    // Tag 7 has no sensor registered
    EXPECT_EQ(aggregator.sensors[7], nullptr);
    auto rc = aggregator.updateSensorNotWorking(7, false);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
}

// updateSensorNotWorking: valid sensor at registered tag → success path.
TEST(nsmNumericSensorAggregator, UpdateSensorNotWorking_ValidSensor_Success)
{
    MockNsmNumericAggregator aggregator{"Sensor", "GetSensorReadingAggregate",
                                        true};
    auto sensor = std::make_shared<MockNsmNumericSensorValueAggregate>();
    aggregator.addSensor(3, sensor);

    EXPECT_CALL(*sensor,
                updateReading(::testing::NanSensitiveDoubleEq(
                                  std::numeric_limits<double>::quiet_NaN()),
                              0))
        .Times(1);
    auto rc = aggregator.updateSensorNotWorking(3, false);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// handleResponseMsg – error-CC response: decode_aggregate_resp succeeds
// (rc = NSM_SW_SUCCESS) but sets cc = NSM_ERROR (non-zero).
// Line 50: cc != NSM_SUCCESS → condition is TRUE → early return taken.
// Line 52: return cc ? cc : rc → cc != 0 → TRUE branch → returns cc.
TEST(nsmSensorAggregator, HandleResponseMsg_ErrorCC_CoversTrueBranch)
{
    MockNsmSensorAggregator aggregator{"ErrorCC", "GetSensorReadingAggregate"};

    // Encode a proper-sized aggregate response with cc = NSM_ERROR
    std::vector<uint8_t> response(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(response.data());
    ASSERT_EQ(encode_aggregate_resp(0, 0x01, NSM_ERROR, 0, responseMsg),
              NSM_SW_SUCCESS);

    // decode_aggregate_resp will succeed (rc = NSM_SW_SUCCESS) but set
    // cc = NSM_ERROR (non-zero).  The early-return condition triggers and
    // "return cc ? cc : rc" takes the TRUE branch (cc != 0) → returns cc.
    EXPECT_CALL(aggregator, handleSample).Times(0);
    auto rc = aggregator.handleResponseMsg(
        reinterpret_cast<nsm_msg*>(response.data()), response.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// ============================================================================
// NsmNumericAggregator::logFalseValid branch coverage
// ============================================================================

// logFalseValid(tag, valid=true): isAnyBitSet() == false → just clears (no log)
TEST(nsmNumericSensorAggregator, LogFalseValid_ValidTrue_NoBitSet_JustClears)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};
    // tag_map is empty; valid=true → isAnyBitSet()=false branch taken
    EXPECT_NO_THROW(agg.logFalseValid(5, true));
}

// logFalseValid(tag, valid=true): isAnyBitSet() == true → logs then clears
TEST(nsmNumericSensorAggregator, LogFalseValid_ValidTrue_BitSet_LogsAndClears)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};
    // First call sets bit 3 (valid=false, setBit returns true)
    agg.logFalseValid(3, false);
    // Now isAnyBitSet()==true; valid=true → logs then clears
    EXPECT_NO_THROW(agg.logFalseValid(3, true));
}

// logFalseValid(tag, valid=false): setBit returns false (bit already set)
TEST(nsmNumericSensorAggregator,
     LogFalseValid_ValidFalse_DuplicateBit_ElseBranchFalse)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};
    agg.logFalseValid(7, false); // first: setBit(7) returns true → logs
    // second: setBit(7) returns false (already set) → else-if is false
    EXPECT_NO_THROW(agg.logFalseValid(7, false));
}

// ============================================================================
// NsmNumericAggregator::update() coroutine branch coverage
// Uses SensorManagerTest fixture to obtain MockNsmDevice for sensorIO mocking
// ============================================================================

struct NsmNumericAggregatorUpdateTest :
    public ::testing::Test,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmNumericAggregatorUpdateTest() : SensorManagerTest(devices)
    {
        gpu = std::static_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(
                "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0"));
    }
    ~NsmNumericAggregatorUpdateTest()
    {
        cleanupDeviceSensors(devices);
    }
};

// update(): genRequestMsg returns nullopt → branch true → co_return //
// NSM_SW_ERROR sensorIO must NOT be called
TEST_F(NsmNumericAggregatorUpdateTest,
       Update_GenRequestMsgFails_SensorIONotCalled)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};
    EXPECT_CALL(agg, genRequestMsg(_, _))
        .WillOnce(Return(std::optional<std::vector<uint8_t>>(std::nullopt)));
    EXPECT_CALL(*gpu, sensorIO).Times(0);
    agg.update(gpu).detach();
}

// update(): sensorIO fails, sampleTags empty → if(rc) true, loop body skipped
TEST_F(NsmNumericAggregatorUpdateTest,
       Update_SensorIOFails_EmptySampleTags_LoopSkipped)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};
    Request reqMsg(10, 0);
    EXPECT_CALL(agg, genRequestMsg(_, _))
        .WillOnce(Return(std::optional<std::vector<uint8_t>>(reqMsg)));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    // sampleTags is empty → loop body never runs
    agg.update(gpu).detach();
}

// update(): sensorIO fails, sampleTags non-empty → loop body runs
TEST_F(NsmNumericAggregatorUpdateTest,
       Update_SensorIOFails_NonEmptySampleTags_UpdatesNotWorking)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};
    auto sensor =
        std::make_shared<NiceMock<MockNsmNumericSensorValueAggregate>>();
    agg.addSensor(2, sensor);
    agg.sampleTags.push_back(2);

    Request reqMsg(10, 0);
    EXPECT_CALL(agg, genRequestMsg(_, _))
        .WillOnce(Return(std::optional<std::vector<uint8_t>>(reqMsg)));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    EXPECT_CALL(*sensor, updateReading(_, _)).Times(AtLeast(1));
    agg.update(gpu).detach();
}

// update(): sensorIO succeeds → if(rc) false branch taken, handleResponseMsg
// called
TEST_F(NsmNumericAggregatorUpdateTest,
       Update_SensorIOSuccess_HandleResponseMsgCalled)
{
    NiceMock<MockNsmNumericAggregator> agg{"A", "T", true};

    // Build minimal valid aggregate response (cc=NSM_ERROR, 0 samples)
    Response respBuf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp), 0);
    encode_aggregate_resp(0, 0x01, NSM_ERROR, 0,
                          reinterpret_cast<nsm_msg*>(respBuf.data()));

    Request reqMsg(10, 0);
    EXPECT_CALL(agg, genRequestMsg(_, _))
        .WillOnce(Return(std::optional<std::vector<uint8_t>>(reqMsg)));
    EXPECT_CALL(*gpu, sensorIO).WillOnce(mockSensorIO(respBuf));
    // handleSample not called (error CC causes early return in
    // handleResponseMsg)
    EXPECT_CALL(agg, handleSample).Times(0);
    agg.update(gpu).detach();
}
