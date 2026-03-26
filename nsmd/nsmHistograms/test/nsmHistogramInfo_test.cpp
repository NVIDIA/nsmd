/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
using namespace ::testing;

#include "base.h"
#include "network-ports.h"

#define private public
#define protected public

#include "nsmHistogramInfo.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();
std::string sensorName("test_histogram");
std::string sensorType("test_type");
std::string inventoryObjPath("/xyz/openbmc_project/inventory/test_device");

TEST(NsmHistogramFormat, GoodGenReq)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent", "histogram",
                                  "/xyz/openbmc_project/inventory/system");

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_TRUE(request.has_value());

    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req));
}

TEST(NsmHistogramFormat, GoodHandleResp)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_resp) + 64, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    struct nsm_histogram_format_metadata metadata = {};
    metadata.num_of_buckets = 10;
    metadata.min_sampling_time = 100;
    metadata.accumulation_cycle = 200; // uint8_t max is 255
    metadata.increment_duration = 50;
    metadata.bucket_data_type = NvU32;
    metadata.bucket_unit_of_measure = NSM_BUCKET_UNIT_WATTS;

    std::vector<uint8_t> bucket_offsets(40, 0); // 10 buckets * 4 bytes (U32)

    uint8_t rc = encode_get_histogram_format_resp(
        0, cc, reason_code, &metadata, bucket_offsets.data(),
        bucket_offsets.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Verify interface values were updated
    EXPECT_EQ(formatIntf->numOfBuckets(), 10);
    EXPECT_EQ(formatIntf->minSamplingTime(), 100);
}

TEST(NsmHistogramFormat, BadHandleResp)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    size_t msg_len = responseMsg.size();

    // Test with NULL pointer
    uint8_t rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_NE(rc, NSM_SUCCESS);

    // Test with zero length
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST(NsmHistogramData, GoodGenReq)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    const uint8_t eid{12};
    const uint8_t instance_id{30};

    auto request = sensor.genRequestMsg(eid, instance_id);
    EXPECT_TRUE(request.has_value());

    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req));
}

TEST(NsmHistogramData, GoodHandleRespWithError)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_ERR_INVALID_DATA;
    uint16_t reason_code = ERR_INVALID_RQD;
    std::vector<uint8_t> histogram_data; // Empty data
    uint8_t bucket_data_type = NvU8;
    uint16_t num_of_buckets = 0;

    uint8_t rc = encode_get_histogram_data_resp(
        0, cc, reason_code, bucket_data_type, num_of_buckets,
        histogram_data.data(), 0, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_ERR_INVALID_DATA);
}

TEST(NsmHistogramData, BadHandleResp)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    size_t msg_len = responseMsg.size();

    // Test with NULL pointer
    uint8_t rc = sensor.handleResponseMsg(NULL, msg_len);
    EXPECT_NE(rc, NSM_SUCCESS);

    // Test with zero length
    rc = sensor.handleResponseMsg(response, 0);
    EXPECT_NE(rc, NSM_SUCCESS);
}

// Additional tests for NsmHistogramFormat
TEST(NsmHistogramFormatTest, Constructor)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    associationsList.emplace_back("parent", "histogram",
                                  "/xyz/openbmc_project/inventory/system");

    uint32_t histogramId = 0x1234;
    uint16_t parameter = 0x5678;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    EXPECT_EQ(sensor.histogramName, sensorName);
    EXPECT_EQ(sensor.deviceType, sensorType);
    EXPECT_EQ(sensor.histogramId, histogramId);
    EXPECT_EQ(sensor.parameter, parameter);
    EXPECT_NE(sensor.formatIntf, nullptr);
    EXPECT_NE(sensor.bucketInfoIntf, nullptr);
    EXPECT_NE(sensor.associationDefIntf, nullptr);
}

TEST(NsmHistogramFormatTest, GenRequestMsg_InvalidInstanceId)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    const uint8_t eid = 12;
    auto request = sensor.genRequestMsg(eid, NSM_INSTANCE_MAX + 1);

    EXPECT_FALSE(request.has_value());
}

TEST(NsmHistogramFormatTest, HandleResponseMsg_Success_Watts)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    uint16_t numBuckets = 10;
    struct nsm_histogram_format_metadata metadata = {
        .num_of_buckets = numBuckets,
        .min_sampling_time = 1000,
        .accumulation_cycle = 50,
        .reserved0 = 0,
        .increment_duration = 100,
        .bucket_unit_of_measure = NSM_BUCKET_UNIT_WATTS,
        .reserved1 = 0,
        .bucket_data_type = NvU32,
        .reserved2 = 0};

    std::vector<uint32_t> bucketOffsets = {0,   100, 200, 300, 400,
                                           500, 600, 700, 800, 900};
    std::vector<uint8_t> offsetData(numBuckets * sizeof(uint32_t));
    memcpy(offsetData.data(), bucketOffsets.data(), offsetData.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_format_resp) +
                                     offsetData.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_format_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  &metadata, offsetData.data(),
                                                  offsetData.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);

    EXPECT_EQ(formatIntf->numOfBuckets(), numBuckets);
    EXPECT_EQ(formatIntf->minSamplingTime(), 1000);
    EXPECT_EQ(formatIntf->accumulationCycle(), 50);
    EXPECT_EQ(formatIntf->incrementDuration(), 100);
    EXPECT_EQ(formatIntf->bucketDataType(), BucketDataTypes::NvU32);
    EXPECT_EQ(formatIntf->unitOfMeasure(), BucketUnits::Watts);

    auto bucketData = bucketInfoIntf->bucketData();
    EXPECT_EQ(bucketData.size(), numBuckets);
}

TEST(NsmHistogramFormatTest, HandleResponseMsg_Success_Percent)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    uint16_t numBuckets = 5;
    struct nsm_histogram_format_metadata metadata = {
        .num_of_buckets = numBuckets,
        .min_sampling_time = 500,
        .accumulation_cycle = 20,
        .reserved0 = 0,
        .increment_duration = 50,
        .bucket_unit_of_measure = NSM_BUCKET_UNIT_PERCENT,
        .reserved1 = 0,
        .bucket_data_type = NvU16,
        .reserved2 = 0};

    std::vector<uint16_t> bucketOffsets = {0, 20, 40, 60, 80};
    std::vector<uint8_t> offsetData(numBuckets * sizeof(uint16_t));
    memcpy(offsetData.data(), bucketOffsets.data(), offsetData.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_format_resp) +
                                     offsetData.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_format_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  &metadata, offsetData.data(),
                                                  offsetData.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);

    EXPECT_EQ(formatIntf->unitOfMeasure(), BucketUnits::Percent);
    EXPECT_EQ(bucketInfoIntf->unit(), BucketUnits::Percent);
}

TEST(NsmHistogramFormatTest, HandleResponseMsg_Success_Count)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;

    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    uint16_t numBuckets = 3;
    struct nsm_histogram_format_metadata metadata = {
        .num_of_buckets = numBuckets,
        .min_sampling_time = 100,
        .accumulation_cycle = 10,
        .reserved0 = 0,
        .increment_duration = 10,
        .bucket_unit_of_measure = NSM_BUCKET_UNIT_COUNTS,
        .reserved1 = 0,
        .bucket_data_type = NvU8,
        .reserved2 = 0};

    std::vector<uint8_t> bucketOffsets = {0, 50, 100};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_format_resp) +
                                     bucketOffsets.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_format_resp(
        0, NSM_SUCCESS, ERR_NULL, &metadata, bucketOffsets.data(),
        bucketOffsets.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);

    EXPECT_EQ(formatIntf->unitOfMeasure(), BucketUnits::Count);
    EXPECT_EQ(bucketInfoIntf->unit(), BucketUnits::Count);
}

// Additional tests for NsmHistogramData
TEST(NsmHistogramDataTest, HandleResponseMsg_Success_U16)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    uint16_t numBuckets = 3;
    formatIntf->numOfBuckets(numBuckets);
    formatIntf->bucketDataType(BucketDataTypes::NvU16);

    std::vector<std::tuple<uint16_t, std::tuple<double, double, double>>>
        bucketData;
    for (uint16_t i = 0; i < numBuckets; i++)
    {
        bucketData.push_back(
            std::make_tuple(i, std::make_tuple(0.0, 0.0, 0.0)));
    }
    bucketInfoIntf->bucketData(bucketData);

    std::vector<uint16_t> dataValues = {100, 200, 300};
    std::vector<uint8_t> bucketDataBytes(numBuckets * sizeof(uint16_t));
    memcpy(bucketDataBytes.data(), dataValues.data(), bucketDataBytes.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_data_resp) +
                                     bucketDataBytes.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_data_resp(
        0, NSM_SUCCESS, ERR_NULL, NvU16, numBuckets, bucketDataBytes.data(),
        bucketDataBytes.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(NsmHistogramDataTest, HandleResponseMsg_Success_U8)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    uint16_t numBuckets = 4;
    formatIntf->numOfBuckets(numBuckets);
    formatIntf->bucketDataType(BucketDataTypes::NvU8);

    std::vector<std::tuple<uint16_t, std::tuple<double, double, double>>>
        bucketData;
    for (uint16_t i = 0; i < numBuckets; i++)
    {
        bucketData.push_back(
            std::make_tuple(i, std::make_tuple(0.0, 0.0, 0.0)));
    }
    bucketInfoIntf->bucketData(bucketData);

    std::vector<uint8_t> dataValues = {10, 20, 30, 40};

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_data_resp) +
                                     dataValues.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_data_resp(0, NSM_SUCCESS, ERR_NULL, NvU8,
                                                numBuckets, dataValues.data(),
                                                dataValues.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(NsmHistogramDataTest, HandleResponseMsg_MismatchedBucketCount)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    uint16_t numBuckets = 5;
    formatIntf->numOfBuckets(numBuckets);
    formatIntf->bucketDataType(BucketDataTypes::NvU32);

    std::vector<std::tuple<uint16_t, std::tuple<double, double, double>>>
        bucketData;
    for (uint16_t i = 0; i < numBuckets; i++)
    {
        bucketData.push_back(
            std::make_tuple(i, std::make_tuple(0.0, 0.0, 0.0)));
    }
    bucketInfoIntf->bucketData(bucketData);

    // Response has 3 buckets but format expects 5
    uint16_t responseBuckets = 3;
    std::vector<uint32_t> dataValues = {10, 20, 30};
    std::vector<uint8_t> bucketDataBytes(responseBuckets * sizeof(uint32_t));
    memcpy(bucketDataBytes.data(), dataValues.data(), bucketDataBytes.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_data_resp) +
                                     bucketDataBytes.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_data_resp(
        0, NSM_SUCCESS, ERR_NULL, NvU32, responseBuckets,
        bucketDataBytes.data(), bucketDataBytes.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmHistogramDataTest, HandleResponseMsg_MismatchedDataType)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    uint16_t numBuckets = 3;
    formatIntf->numOfBuckets(numBuckets);
    formatIntf->bucketDataType(BucketDataTypes::NvU32); // Expecting U32

    std::vector<std::tuple<uint16_t, std::tuple<double, double, double>>>
        bucketData;
    for (uint16_t i = 0; i < numBuckets; i++)
    {
        bucketData.push_back(
            std::make_tuple(i, std::make_tuple(0.0, 0.0, 0.0)));
    }
    bucketInfoIntf->bucketData(bucketData);

    // Response sends U16 data type
    std::vector<uint16_t> dataValues = {10, 20, 30};
    std::vector<uint8_t> bucketDataBytes(numBuckets * sizeof(uint16_t));
    memcpy(bucketDataBytes.data(), dataValues.data(), bucketDataBytes.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_data_resp) +
                                     bucketDataBytes.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_data_resp(
        0, NSM_SUCCESS, ERR_NULL, NvU16, numBuckets, bucketDataBytes.data(),
        bucketDataBytes.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

TEST(NsmHistogramDataTest, HandleResponseMsg_CompletionError)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    uint16_t numBuckets = 1;
    formatIntf->numOfBuckets(numBuckets);
    formatIntf->bucketDataType(BucketDataTypes::NvU32);

    std::vector<std::tuple<uint16_t, std::tuple<double, double, double>>>
        bucketData;
    bucketData.push_back(std::make_tuple(0, std::make_tuple(0.0, 0.0, 0.0)));
    bucketInfoIntf->bucketData(bucketData);

    std::vector<uint32_t> dataValues = {10};
    std::vector<uint8_t> bucketDataBytes(numBuckets * sizeof(uint32_t));
    memcpy(bucketDataBytes.data(), dataValues.data(), bucketDataBytes.size());

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                     sizeof(nsm_get_histogram_data_resp) +
                                     bucketDataBytes.size());
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_data_resp(
        0, NSM_SUCCESS, ERR_NULL, NvU32, numBuckets, bucketDataBytes.data(),
        bucketDataBytes.size(), response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    // Manually set completion code to error
    struct nsm_get_histogram_data_resp* resp =
        (struct nsm_get_histogram_data_resp*)response->payload;
    resp->hdr.completion_code = NSM_ERROR;
    responseMsg.resize(sizeof(nsm_msg_hdr) +
                       sizeof(nsm_common_non_success_resp));

    size_t msg_len = responseMsg.size();
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_ERROR);
}

// Tests for static utility functions
// These functions are static in nsmHistogramInfo.cpp, so we declare them extern
namespace nsm
{
extern double getMaxValueForType(uint8_t data_type);
extern bool checkSizeOfBucketArrayIsValid(uint32_t total_size,
                                          uint16_t num_of_buckets,
                                          uint8_t bucket_data_type,
                                          uint32_t& calculated_size);
extern double getValueFromBucketArray(uint8_t* data, BucketDataTypes data_type,
                                      size_t index);
extern BucketDataTypes getDataTypeEnum(uint8_t data_type);
} // namespace nsm

// Tests for getMaxValueForType
TEST(getMaxValueForTypeTest, NvU8ReturnsUInt8Max)
{
    auto result = getMaxValueForType(NvU8);
    EXPECT_EQ(result, std::numeric_limits<uint8_t>::max());
}

TEST(getMaxValueForTypeTest, NvS8ReturnsInt8Max)
{
    auto result = getMaxValueForType(NvS8);
    EXPECT_EQ(result, std::numeric_limits<int8_t>::max());
}

TEST(getMaxValueForTypeTest, NvU16ReturnsUInt16Max)
{
    auto result = getMaxValueForType(NvU16);
    EXPECT_EQ(result, std::numeric_limits<uint16_t>::max());
}

TEST(getMaxValueForTypeTest, NvS16ReturnsInt16Max)
{
    auto result = getMaxValueForType(NvS16);
    EXPECT_EQ(result, std::numeric_limits<int16_t>::max());
}

TEST(getMaxValueForTypeTest, NvU32ReturnsUInt32Max)
{
    auto result = getMaxValueForType(NvU32);
    EXPECT_EQ(result, std::numeric_limits<uint32_t>::max());
}

TEST(getMaxValueForTypeTest, NvS32ReturnsInt32Max)
{
    auto result = getMaxValueForType(NvS32);
    EXPECT_EQ(result, std::numeric_limits<int32_t>::max());
}

TEST(getMaxValueForTypeTest, NvS24_8ReturnsFloatMax)
{
    auto result = getMaxValueForType(NvS24_8);
    EXPECT_EQ(result, std::numeric_limits<float>::max());
}

TEST(getMaxValueForTypeTest, NvU64ReturnsUInt64Max)
{
    auto result = getMaxValueForType(NvU64);
    EXPECT_EQ(result, std::numeric_limits<uint64_t>::max());
}

TEST(getMaxValueForTypeTest, NvS64ReturnsInt64Max)
{
    auto result = getMaxValueForType(NvS64);
    EXPECT_EQ(result, std::numeric_limits<int64_t>::max());
}

TEST(getMaxValueForTypeTest, InvalidTypeReturnsNaN)
{
    auto result = getMaxValueForType(0xFF);
    EXPECT_TRUE(std::isnan(result));
}

// Tests for checkSizeOfBucketArrayIsValid
TEST(checkSizeOfBucketArrayIsValidTest, NvU8ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(uint8_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvU8,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvS8ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(int8_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvS8,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvU16ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(uint16_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvU16,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvS16ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(int16_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvS16,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvU32ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(uint32_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvU32,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvS32ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(int32_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvS32,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvU64ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(uint64_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvU64,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvS64ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(int64_t) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvS64,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, NvS24_8ValidSize)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = sizeof(float) * num_buckets;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets,
                                                NvS24_8, calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, InvalidSizeMismatch)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = 999; // Wrong size

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvU32,
                                                calculated_size);

    EXPECT_FALSE(result);
    EXPECT_NE(calculated_size, total_size);
}

TEST(checkSizeOfBucketArrayIsValidTest, InvalidDataTypeReturnsZero)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 10;
    uint32_t total_size = 100;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, 0xFF,
                                                calculated_size);

    EXPECT_FALSE(result);
    EXPECT_EQ(calculated_size, 0);
}

TEST(checkSizeOfBucketArrayIsValidTest, ZeroBuckets)
{
    uint32_t calculated_size = 0;
    uint16_t num_buckets = 0;
    uint32_t total_size = 0;

    auto result = checkSizeOfBucketArrayIsValid(total_size, num_buckets, NvU32,
                                                calculated_size);

    EXPECT_TRUE(result);
    EXPECT_EQ(calculated_size, 0);
}

// Tests for getValueFromBucketArray
TEST(getValueFromBucketArrayTest, NvU8Extraction)
{
    uint8_t data[] = {10, 20, 30, 40};
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvU8, 2);
    EXPECT_EQ(result, 30.0);
}

TEST(getValueFromBucketArrayTest, NvU8FirstElement)
{
    uint8_t data[] = {10, 20, 30, 40};
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvU8, 0);
    EXPECT_EQ(result, 10.0);
}

TEST(getValueFromBucketArrayTest, NvS8ExtractionPositive)
{
    uint8_t data[] = {0x7F, 0x00, 0x01, 0x02}; // 127, 0, 1, 2
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS8, 0);
    EXPECT_EQ(result, 127.0);
}

TEST(getValueFromBucketArrayTest, NvS8ExtractionNegative)
{
    int8_t values[] = {-10, -20, 30, 40};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS8, 1);
    EXPECT_EQ(result, -20.0);
}

TEST(getValueFromBucketArrayTest, NvU16Extraction)
{
    uint16_t values[] = {100, 200, 300, 400};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvU16, 1);
    EXPECT_EQ(result, 200.0);
}

TEST(getValueFromBucketArrayTest, NvS16ExtractionPositive)
{
    int16_t values[] = {1000, 2000, 3000, 4000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS16, 3);
    EXPECT_EQ(result, 4000.0);
}

TEST(getValueFromBucketArrayTest, NvS16ExtractionNegative)
{
    int16_t values[] = {-100, -200, 300, 400};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS16, 0);
    EXPECT_EQ(result, -100.0);
}

TEST(getValueFromBucketArrayTest, NvU32Extraction)
{
    uint32_t values[] = {1000, 2000, 3000, 4000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvU32, 2);
    EXPECT_EQ(result, 3000.0);
}

TEST(getValueFromBucketArrayTest, NvS32ExtractionPositive)
{
    int32_t values[] = {10000, 20000, 30000, 40000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS32, 1);
    EXPECT_EQ(result, 20000.0);
}

TEST(getValueFromBucketArrayTest, NvS32ExtractionNegative)
{
    int32_t values[] = {-1000, -2000, 3000, 4000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS32, 0);
    EXPECT_EQ(result, -1000.0);
}

TEST(getValueFromBucketArrayTest, NvS24_8Extraction)
{
    float values[] = {1.5f, 2.5f, 3.5f, 4.5f};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS24_8, 1);
    EXPECT_FLOAT_EQ(result, 2.5);
}

TEST(getValueFromBucketArrayTest, NvU64Extraction)
{
    uint64_t values[] = {10000, 20000, 30000, 40000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvU64, 3);
    EXPECT_EQ(result, 40000.0);
}

TEST(getValueFromBucketArrayTest, NvS64ExtractionPositive)
{
    int64_t values[] = {100000, 200000, 300000, 400000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS64, 2);
    EXPECT_EQ(result, 300000.0);
}

TEST(getValueFromBucketArrayTest, NvS64ExtractionNegative)
{
    int64_t values[] = {-10000, -20000, 30000, 40000};
    uint8_t* data = reinterpret_cast<uint8_t*>(values);
    auto result = getValueFromBucketArray(data, BucketDataTypes::NvS64, 1);
    EXPECT_EQ(result, -20000.0);
}

TEST(getValueFromBucketArrayTest, DefaultCaseReturnsZero)
{
    uint8_t data[] = {10, 20, 30, 40};
    auto result = getValueFromBucketArray(data, BucketDataTypes::Unknown, 0);
    EXPECT_EQ(result, 0.0);
}

// Tests for getDataTypeEnum
TEST(getDataTypeEnumTest, NvU8Conversion)
{
    auto result = getDataTypeEnum(NvU8);
    EXPECT_EQ(result, BucketDataTypes::NvU8);
}

TEST(getDataTypeEnumTest, NvS8Conversion)
{
    auto result = getDataTypeEnum(NvS8);
    EXPECT_EQ(result, BucketDataTypes::NvS8);
}

TEST(getDataTypeEnumTest, NvU16Conversion)
{
    auto result = getDataTypeEnum(NvU16);
    EXPECT_EQ(result, BucketDataTypes::NvU16);
}

TEST(getDataTypeEnumTest, NvS16Conversion)
{
    auto result = getDataTypeEnum(NvS16);
    EXPECT_EQ(result, BucketDataTypes::NvS16);
}

TEST(getDataTypeEnumTest, NvU32Conversion)
{
    auto result = getDataTypeEnum(NvU32);
    EXPECT_EQ(result, BucketDataTypes::NvU32);
}

TEST(getDataTypeEnumTest, NvS32Conversion)
{
    auto result = getDataTypeEnum(NvS32);
    EXPECT_EQ(result, BucketDataTypes::NvS32);
}

TEST(getDataTypeEnumTest, NvS24_8Conversion)
{
    auto result = getDataTypeEnum(NvS24_8);
    EXPECT_EQ(result, BucketDataTypes::NvS24_8);
}

TEST(getDataTypeEnumTest, NvU64Conversion)
{
    auto result = getDataTypeEnum(NvU64);
    EXPECT_EQ(result, BucketDataTypes::NvU64);
}

TEST(getDataTypeEnumTest, NvS64Conversion)
{
    auto result = getDataTypeEnum(NvS64);
    EXPECT_EQ(result, BucketDataTypes::NvS64);
}

TEST(getDataTypeEnumTest, InvalidTypeReturnsUnknown)
{
    auto result = getDataTypeEnum(0xFF);
    EXPECT_EQ(result, BucketDataTypes::Unknown);
}

TEST(getDataTypeEnumTest, DefaultBoundary)
{
    // Test a value just beyond the defined enums
    auto result = getDataTypeEnum(100);
    EXPECT_EQ(result, BucketDataTypes::Unknown);
}

// Error-CC path: decode succeeds but cc = NSM_ERROR (non-zero)
// → if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS) is false
// → return cc ? cc : rc; takes the true branch (return cc).
TEST(NsmHistogramFormat, HandleResponseMsg_ErrorCC_CoversBranch)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    struct nsm_histogram_format_metadata metaData = {};
    uint8_t bucketOffsets[4] = {};
    uint8_t rc = encode_get_histogram_format_resp(
        0, NSM_ERROR, ERR_NULL, &metaData, bucketOffsets, sizeof(bucketOffsets),
        response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// NsmHistogramData::genRequestMsg failure path (invalid instanceId)
TEST(NsmHistogramDataTest, GenRequestMsg_InvalidInstanceId)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());
    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    const uint8_t eid = 12;
    auto request = sensor.genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// Default (unknown) unit-of-measure → both switch default branches in
// NsmHistogramFormat::handleResponseMsg (first sets formatIntf, second sets
// bucketInfoIntf to BucketUnits::Others).
TEST(NsmHistogramFormatTest, HandleResponseMsg_DefaultUnit_Others)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    uint16_t numBuckets = 2;
    struct nsm_histogram_format_metadata metadata = {
        .num_of_buckets = numBuckets,
        .min_sampling_time = 100,
        .accumulation_cycle = 10,
        .reserved0 = 0,
        .increment_duration = 10,
        .bucket_unit_of_measure = 0xFF, // non-standard → default branch
        .reserved1 = 0,
        .bucket_data_type = NvU8,
        .reserved2 = 0};

    // 2 NvU8 buckets = 2 bytes (valid size)
    std::vector<uint8_t> bucketOffsets = {0, 50};
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_format_resp(
        0, NSM_SUCCESS, ERR_NULL, &metadata, bucketOffsets.data(),
        bucketOffsets.size(), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);

    // Both switches hit the default → BucketUnits::Others
    EXPECT_EQ(formatIntf->unitOfMeasure(), BucketUnits::Others);
    EXPECT_EQ(bucketInfoIntf->unit(), BucketUnits::Others);
}

// checkSizeOfBucketArrayIsValid failure within
// NsmHistogramFormat::handleResponseMsg. We encode 3 NvU8 buckets but report
// bucket_offsets_size=5 (wrong; should be 3). Decode restores
// bucket_offsets_size=5, so the size check fails. Calling twice exercises both
// branches of the inner if (shouldLog(...)).
TEST(NsmHistogramFormatTest, HandleResponseMsg_CheckSizeMismatch_ShouldLog)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    std::vector<std::tuple<std::string, std::string, std::string>>
        associationsList;
    uint32_t histogramId = 1;
    uint16_t parameter = 0;

    NsmHistogramFormat sensor(bus, sensorName, sensorType, formatIntf,
                              bucketInfoIntf, inventoryObjPath,
                              associationsList, histogramId, parameter);

    uint16_t numBuckets = 3;
    struct nsm_histogram_format_metadata metadata = {
        .num_of_buckets = numBuckets,
        .min_sampling_time = 100,
        .accumulation_cycle = 10,
        .reserved0 = 0,
        .increment_duration = 10,
        .bucket_unit_of_measure = NSM_BUCKET_UNIT_WATTS,
        .reserved1 = 0,
        .bucket_data_type = NvU8, // valid size = 3 bytes
        .reserved2 = 0};

    // Intentionally 5 bytes instead of 3 → data_size field encodes 5 →
    // decode sets bucket_offsets_size=5 →
    // checkSizeOfBucketArrayIsValid(5,3,NvU8) fails
    std::vector<uint8_t> offsetData(5, 0);
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_format_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  &metadata, offsetData.data(),
                                                  offsetData.size(), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();

    // First call: shouldLog returns true (state: false→true)
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    // Second call: shouldLog returns false (already in error state)
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}

// checkSizeOfBucketArrayIsValid failure within
// NsmHistogramData::handleResponseMsg. Format expects 3 NvU8 buckets (3 bytes).
// Response encodes 3 NvU8 buckets but bucket_data_size=5 (wrong).  Calling
// twice exercises both shouldLog branches.
TEST(NsmHistogramDataTest, HandleResponseMsg_CheckSizeMismatch_ShouldLog)
{
    auto formatIntf = std::make_shared<FormatIntf>(bus,
                                                   inventoryObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(bus, inventoryObjPath.c_str());

    uint32_t histogramId = 2;
    uint16_t parameter = 1;

    NsmHistogramData sensor(sensorName, sensorType, formatIntf, bucketInfoIntf,
                            histogramId, parameter);

    uint16_t numBuckets = 3;
    formatIntf->numOfBuckets(numBuckets);
    formatIntf->bucketDataType(BucketDataTypes::NvU8);

    std::vector<std::tuple<uint16_t, std::tuple<double, double, double>>>
        bucketData;
    for (uint16_t i = 0; i < numBuckets; i++)
    {
        bucketData.push_back(
            std::make_tuple(i, std::make_tuple(0.0, 0.0, 0.0)));
    }
    bucketInfoIntf->bucketData(bucketData);

    // Encode 3 NvU8 buckets but claim 5 bytes of data → decode returns
    // total_bucket_data_size=5, which != 3*1=3 → checkSize fails.
    std::vector<uint8_t> wrongData(5, 0);
    std::vector<uint8_t> responseMsg(256, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t rc = encode_get_histogram_data_resp(0, NSM_SUCCESS, ERR_NULL, NvU8,
                                                numBuckets, wrongData.data(),
                                                wrongData.size(), response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    size_t msg_len = responseMsg.size();

    // First call: shouldLog returns true
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);

    // Second call: shouldLog returns false (no state change)
    rc = sensor.handleResponseMsg(response, msg_len);
    EXPECT_EQ(rc, NSM_SW_ERROR_COMMAND_FAIL);
}
