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

#include "platform-environmental.h"

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cmath>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmGpmOem.hpp"

using namespace nsm;
using namespace ::testing;

class MockMetricPerInstanceUpdator : public MetricPerInstanceUpdator
{
  public:
    MOCK_METHOD(void, updateMetric, (const std::vector<double>& metrics),
                (override));
};

struct NsmGPMPerInstanceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:70";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmGPMPerInstanceTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmGPMPerInstanceTest()
    {
        cleanupDeviceSensors(devices);
    }
};

TEST_F(NsmGPMPerInstanceTest, testConstructor)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0xFF}, {.byte = 0xFF}, {.byte = 0xFF}, {.byte = 0xFF}};

    NsmGPMPerInstance perInstance("GPM_PerInst_Test", "GPMPerInstance", 2, 0, 0,
                                  10, instanceBitfield,
                                  GPMMetricsUnit::PERCENTAGE, updator);

    EXPECT_EQ(perInstance.getName(), "GPM_PerInst_Test");
    EXPECT_EQ(perInstance.getType(), "GPMPerInstance");
    EXPECT_EQ(perInstance.retrievalSource, 2);
    EXPECT_EQ(perInstance.gpuInstance, 0);
    EXPECT_EQ(perInstance.computeInstance, 0);
    EXPECT_EQ(perInstance.metricId, 10);
    EXPECT_EQ(perInstance.instanceBitfield.size(), instanceBitfield.size());
}

TEST_F(NsmGPMPerInstanceTest, testGenRequest)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0xFF}, {.byte = 0xFF}, {.byte = 0x00}, {.byte = 0x00}};

    NsmGPMPerInstance perInstance("GPM_PerInst_Req", "GPMPerInstance", 2, 1, 2,
                                  11, instanceBitfield,
                                  GPMMetricsUnit::BANDWIDTH, updator);

    eid_t eid = 10;
    uint8_t instanceId = 1;

    auto request = perInstance.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_per_instance_gpm_metrics_req));

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command =
        reinterpret_cast<const nsm_query_per_instance_gpm_metrics_req*>(
            msg->payload);

    EXPECT_EQ(command->retrieval_source, 2);
    EXPECT_EQ(command->gpu_instance, 1);
    EXPECT_EQ(command->compute_instance, 2);
    EXPECT_EQ(command->metric_id, 11);
}

TEST_F(NsmGPMPerInstanceTest, testHandleResponseSuccess)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0x0F}, {.byte = 0x00}, {.byte = 0x00}, {.byte = 0x00}};

    NsmGPMPerInstance perInstance("GPM_PerInst_Update", "GPMPerInstance", 2, 0,
                                  0, 10, instanceBitfield, // 4 instances
                                  GPMMetricsUnit::PERCENTAGE, updator);

    // Expect updateMetric to be called with 4 values
    EXPECT_CALL(*updator, updateMetric(SizeIs(4))).Times(1);

    // Create mock aggregate response
    const size_t data_len = 4;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        4 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 4;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    for (uint8_t i = 0; i < 4; i++)
    {
        auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
        sample->tag = i;
        sample->valid = 1;
        sample->length = std::log2(data_len);
        uint32_t value = (i + 1) * 1000;
        memcpy(sample->data, &value, data_len);
        sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;
    }

    auto rc = perInstance.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmGPMPerInstanceTest, badTestCompletionCodeError)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0x0F}, {.byte = 0x00}, {.byte = 0x00}, {.byte = 0x00}};

    NsmGPMPerInstance perInstance("GPM_PerInst_Err", "GPMPerInstance", 2, 0, 0,
                                  10, instanceBitfield,
                                  GPMMetricsUnit::PERCENTAGE, updator);

    // Create mock error response
    std::vector<uint8_t> responseBuffer(sizeof(nsm_msg_hdr) +
                                        sizeof(nsm_aggregate_resp));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_ERROR;
    payload->telemetry_count = 0;

    auto rc = perInstance.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

TEST_F(NsmGPMPerInstanceTest, testHandleResponseInvalidSample)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0xFF}, {.byte = 0x00}, {.byte = 0x00}, {.byte = 0x00}};

    NsmGPMPerInstance perInstance("GPM_PerInst_Invalid", "GPMPerInstance", 2, 0,
                                  0, 10, instanceBitfield,
                                  GPMMetricsUnit::PERCENTAGE, updator);

    // Expect updateMetric to be called with empty vector since all samples are
    // invalid
    EXPECT_CALL(*updator, updateMetric(SizeIs(0))).Times(1);

    // Create mock response with invalid samples
    // PERCENTAGE uses uint32_t (4 bytes)
    const size_t data_len = 4;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        2 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 2;

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    // Sample with valid=false
    auto sample0 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample0->tag = 0;
    sample0->valid = 0; // Invalid
    sample0->length = std::log2(data_len);
    uint32_t value0 = 100;
    memcpy(sample0->data, &value0, data_len);
    sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;

    // Sample with reserved tag
    auto sample1 = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
    sample1->tag = NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1;
    sample1->valid = 1;
    sample1->length = std::log2(data_len);
    uint32_t value1 = 200;
    memcpy(sample1->data, &value1, data_len);

    auto rc = perInstance.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmGPMPerInstanceTest, testHandleResponsePartialInstances)
{
    auto updator = std::make_shared<MockMetricPerInstanceUpdator>();

    const std::vector<bitfield8_t> instanceBitfield{
        {.byte = 0x0F}, {.byte = 0x00}, {.byte = 0x00}, {.byte = 0x00}};

    // Request 8 instances but only 4 are in bitfield
    NsmGPMPerInstance perInstance("GPM_PerInst_Partial", "GPMPerInstance", 2, 0,
                                  0, 10, instanceBitfield, // Only 4 instances
                                  GPMMetricsUnit::BANDWIDTH, updator);

    // Expect updateMetric with only 4 valid samples (first 4 with valid=1)
    EXPECT_CALL(*updator, updateMetric(SizeIs(4))).Times(1);

    // Create response with all 8 tags but only 4 valid
    // BANDWIDTH uses uint64_t (8 bytes)
    const size_t data_len = 8;
    std::vector<uint8_t> responseBuffer(
        sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp) +
        4 * (sizeof(nsm_aggregate_resp_sample) - 1 + data_len));

    auto responseMsg = reinterpret_cast<nsm_msg*>(responseBuffer.data());
    auto payload = reinterpret_cast<nsm_aggregate_resp*>(responseMsg->payload);
    payload->completion_code = NSM_SUCCESS;
    payload->telemetry_count = 4; // Only 4 samples in response

    auto sample_ptr = reinterpret_cast<uint8_t*>(payload + 1);

    for (uint8_t i = 0; i < 4; i++)
    {
        auto sample = reinterpret_cast<nsm_aggregate_resp_sample*>(sample_ptr);
        sample->tag = i;
        sample->valid = 1; // All valid
        sample->length = std::log2(data_len);
        uint64_t value = (i + 1) * 500;
        memcpy(sample->data, &value, data_len);
        sample_ptr += sizeof(nsm_aggregate_resp_sample) - 1 + data_len;
    }

    auto rc = perInstance.handleResponseMsg(responseMsg, responseBuffer.size());
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}
