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
