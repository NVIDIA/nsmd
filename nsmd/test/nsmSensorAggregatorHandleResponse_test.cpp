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
 * nsmBatch17_test.cpp
 *
 * Coverage targets:
 *   1. nsmd/nsmSensorAggregator.cpp
 *      - handleResponseMsg: decode_aggregate_resp failure branch
 *        (lines 50-53): shouldLog + early return when decode_aggregate_resp
 *        itself fails (buffer too small or bad data).
 *      - handleResponseMsg: cc != NSM_SUCCESS branch (lines 50-53): early
 *        return when completion code is non-zero but decode succeeded.
 *      - handleResponseMsg: zero-telemetry-count path (line 54): sampleTags
 *        cleared but while loop body never executes.
 */

#include "base.h"
#include "platform-environmental.h"

#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmNumericSensor/nsmThresholdAggregator.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Helper: build a properly encoded aggregate response
// ============================================================================

static std::vector<uint8_t> buildAggregateResp(uint8_t cc,
                                               uint16_t telemetryCount)
{
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp),
                             0);
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_aggregate_resp(0, 0x01, cc, telemetryCount, msg);
    return buf;
}

// ============================================================================
// NsmSensorAggregator::handleResponseMsg - uncovered branches
// ============================================================================

// Passing a buffer that is too small for decode_aggregate_resp to succeed
// (only 1 byte) triggers the error-log branch and the early return when
// rc != NSM_SW_SUCCESS.
TEST(NsmSensorAggregatorBranchTest,
     HandleResponseMsg_DecodeAggregateFails_EarlyReturn)
{
    NsmThresholdAggregator agg("AggDecFail", "NSM_ThermalParam", false);

    // A 1-byte buffer is far too small for decode_aggregate_resp to succeed.
    std::vector<uint8_t> tiny(1, 0);
    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(tiny.data()), tiny.size());

    // decode_aggregate_resp fails → rc != NSM_SW_SUCCESS → early return
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// A response whose completion code is non-zero (NSM_ERROR = 1): decode
// succeeds (rc == NSM_SW_SUCCESS) but cc != NSM_SUCCESS triggers the early
// return branch returning the completion code.
TEST(NsmSensorAggregatorBranchTest, HandleResponseMsg_NonSuccessCC_EarlyReturn)
{
    NsmThresholdAggregator agg("AggBadCC", "NSM_ThermalParam", false);
    // NSM_ERROR = 1 (non-zero completion code)
    auto buf = buildAggregateResp(NSM_ERROR, 0);

    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // cc != NSM_SUCCESS → early return cc (NSM_ERROR = 1, non-zero)
    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// A valid response with cc == NSM_SUCCESS and telemetry_count == 0 exercises
// the sampleTags.clear() call and the while loop with zero iterations.
// handleResponseMsg should return NSM_SW_SUCCESS without processing any
// samples.
TEST(NsmSensorAggregatorBranchTest, HandleResponseMsg_ZeroTelemetry_Success)
{
    NsmThresholdAggregator agg("AggZeroTelemetry", "NSM_ThermalParam", false);
    auto buf = buildAggregateResp(NSM_SUCCESS, 0);

    auto rc = agg.handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(buf.data()), buf.size());

    // sampleTags.clear() called, loop runs 0 times, returnValue unchanged
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(agg.sampleTags.empty());
}
