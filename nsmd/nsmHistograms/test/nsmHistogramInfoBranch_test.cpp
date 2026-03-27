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

/*
 * Branch coverage tests for nsmHistogramInfo.cpp
 *
 * Targets:
 * - NsmHistogramFormat::handleResponseMsg line 298:
 *   checkSizeOfBucketArrayIsValid returns false → NSM_SW_ERROR_COMMAND_FAIL
 *   (metadata says 10 NvU32 buckets = 40B but response has 39B of bucket data)
 * - NsmHistogramData::genRequestMsg line 383:
 *   encode_get_histogram_data_req fails (invalid instanceId > NSM_INSTANCE_MAX)
 *   → function returns std::nullopt
 * - NsmHistogramFormat::handleResponseMsg: unit of measure default branch
 *   (bucket_unit_of_measure not in {WATTS, PERCENT, COUNTS})
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <cstring>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "base.h"

#include "nsmHistogramInfo.hpp"

using namespace nsm;
using namespace ::testing;

static auto busH = sdbusplus::bus::new_default();
// Non-const because NsmHistogramFormat constructor takes std::string&
// (non-const ref)
static std::string sBranchSensorName("branch_histogram");
static std::string sBranchSensorType("branch_type");
static std::string
    sBranchInvObjPath("/xyz/openbmc_project/inventory/histogram_branch");

// ============================================================================
// NsmHistogramFormat::handleResponseMsg: checkSizeOfBucketArrayIsValid fails
// Line 298 TRUE branch: total_bucket_offset_size ≠ num_of_buckets*sizeof(type)
//
// Strategy: encode with correct 40-byte bucket data (10 NvU32, no OOB), then
// patch data_size field to claim only 36 bytes → total_bucket_offset_size=36,
// calculated_size=40 → checkSizeOfBucketArrayIsValid returns false.
// Note: encode_get_histogram_format_resp calls htoleArrayData which reads
// num_of_buckets*4 bytes; passing fewer bytes would trigger a valgrind
// out-of-bounds read (production code bug). We avoid this by encoding the
// full 40-byte buffer, then manually patching the data_size.
// ============================================================================

TEST(NsmHistogramInfoBranch, HandleFormat_InvalidBucketSize_ReturnsCommandFail)
{
    auto formatIntf = std::make_shared<FormatIntf>(busH,
                                                   sBranchInvObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(busH, sBranchInvObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>> assoc;

    NsmHistogramFormat sensor(busH, sBranchSensorName, sBranchSensorType,
                              formatIntf, bucketInfoIntf, sBranchInvObjPath,
                              assoc, /*histogramId=*/1, /*parameter=*/0);

    const uint16_t numBuckets = 10;
    struct nsm_histogram_format_metadata metadata = {};
    metadata.num_of_buckets = numBuckets;
    metadata.min_sampling_time = 1000;
    metadata.accumulation_cycle = 50;
    metadata.increment_duration = 100;
    metadata.bucket_unit_of_measure = NSM_BUCKET_UNIT_WATTS;
    metadata.bucket_data_type = NvU32; // expects 4*10 = 40 bytes

    // Encode with the correct 40-byte buffer so htoleArrayData reads in-bounds
    const size_t correctDataSize = 40; // 10 NvU32 buckets
    std::vector<uint8_t> offsetData(correctDataSize, 0xAB);
    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) +
                                         sizeof(nsm_get_histogram_format_resp) +
                                         correctDataSize,
                                     0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t encRc = encode_get_histogram_format_resp(
        0, NSM_SUCCESS, ERR_NULL, &metadata, offsetData.data(),
        offsetData.size(), response);
    EXPECT_EQ(encRc, NSM_SW_SUCCESS);

    // Patch data_size in the common response header to claim 36 bytes of
    // bucket data (sizeof(metadata)+36) instead of the correct 40.
    // decode_get_histogram_format_resp derives total_bucket_offset_size =
    // data_size - sizeof(metadata); 36 ≠ 10*4=40 → checkSize returns false.
    auto* respPayload =
        reinterpret_cast<nsm_get_histogram_format_resp*>(response->payload);
    const uint16_t patchedDataSize =
        static_cast<uint16_t>(sizeof(nsm_histogram_format_metadata) + 36);
    respPayload->hdr.data_size = htole16(patchedDataSize);

    uint8_t rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, static_cast<uint8_t>(NSM_SW_ERROR_COMMAND_FAIL));
}

// ============================================================================
// NsmHistogramData::genRequestMsg line 383:
// encode_get_histogram_data_req fails with instanceId > NSM_INSTANCE_MAX
// → returns std::nullopt
// ============================================================================

TEST(NsmHistogramInfoBranch, DataGenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    auto formatIntf = std::make_shared<FormatIntf>(busH,
                                                   sBranchInvObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(busH, sBranchInvObjPath.c_str());

    NsmHistogramData sensor(sBranchSensorName, sBranchSensorType, formatIntf,
                            bucketInfoIntf, /*histogramId=*/3,
                            /*parameter=*/0);

    const uint8_t eid = 5;
    // NSM_INSTANCE_MAX + 1 triggers encode failure
    auto request = sensor.genRequestMsg(eid, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// NsmHistogramFormat::handleResponseMsg: default unit-of-measure branch
// (bucket_unit_of_measure not WATTS/PERCENT/COUNTS → BucketUnits::Others)
// Line 292: default case in the switch statement.
// ============================================================================

TEST(NsmHistogramInfoBranch, HandleFormat_DefaultUnitOfMeasure_SetsOthers)
{
    auto formatIntf = std::make_shared<FormatIntf>(busH,
                                                   sBranchInvObjPath.c_str());
    auto bucketInfoIntf =
        std::make_shared<BucketInfoIntf>(busH, sBranchInvObjPath.c_str());
    std::vector<std::tuple<std::string, std::string, std::string>> assoc;

    NsmHistogramFormat sensor(busH, sBranchSensorName, sBranchSensorType,
                              formatIntf, bucketInfoIntf, sBranchInvObjPath,
                              assoc, /*histogramId=*/1, /*parameter=*/0);

    const uint16_t numBuckets = 5;
    struct nsm_histogram_format_metadata metadata = {};
    metadata.num_of_buckets = numBuckets;
    metadata.min_sampling_time = 100;
    metadata.accumulation_cycle = 10;
    metadata.increment_duration = 5;
    // Use a unit-of-measure value not in the switch (e.g., 0xFF)
    metadata.bucket_unit_of_measure = 0xFF;
    metadata.bucket_data_type = NvU32; // 4 bytes each

    // Provide correct-sized bucket data (5 * 4 = 20 bytes) so the size
    // check passes and we reach the switch statement
    const size_t dataSize = numBuckets * sizeof(uint32_t);
    std::vector<uint32_t> bucketOffsets(numBuckets, 0);
    std::vector<uint8_t> offsetData(dataSize);
    memcpy(offsetData.data(), bucketOffsets.data(), dataSize);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_resp) + dataSize,
        0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t encRc = encode_get_histogram_format_resp(
        0, NSM_SUCCESS, ERR_NULL, &metadata, offsetData.data(),
        offsetData.size(), response);
    EXPECT_EQ(encRc, NSM_SW_SUCCESS);

    uint8_t rc = sensor.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // Verify the default branch set BucketUnits::Others
    EXPECT_EQ(formatIntf->unitOfMeasure(), BucketUnits::Others);
}
