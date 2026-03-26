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
 * Branch coverage tests for nsmOemResetStatistics.cpp
 *
 * Targets remaining uncovered branches:
 * - genRequestMsg: encode failure (instanceId > NSM_INSTANCE_MAX)
 * - handleSample: out-of-range tag (> NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG)
 * - handleSample: missing tag in map (unknown tag)
 * - handleSample: LastResetType decode error
 * - handleSample: BootReason decode error
 * - handleSample: standard counter decode error
 * - updateProperty: LastResetType path
 * - updateProperty: numeric property path
 * - updateBootReasonProperty: multiple boot reason bits
 * - updateProperty: exception catch block
 */

#include "test/mockDBusHandler.hpp"
using namespace ::testing;

#include "base.h"
#include "diagnostics.h"

#include "utils.hpp"

#include <sdbusplus/bus.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmOemResetStatistics.hpp"

using namespace nsm;

static auto& busBr = utils::DBusHandler::getBus();

// Helper: create a fully constructed ResetStatisticsAggregator
static std::shared_ptr<ResetStatisticsAggregator>
    makeAggregatorBr(const std::string& path)
{
    auto resetIntf = std::make_shared<ResetCountersIntf>(busBr, path.c_str());
    auto assocIntf = std::make_unique<AssociationDefinitionsIntf>(busBr,
                                                                  path.c_str());
    return std::make_shared<ResetStatisticsAggregator>(
        "test_reset_br", "NSM_Reset", path, resetIntf, std::move(assocIntf));
}

// =============================================================================
// genRequestMsg: encode failure -> nullopt
// =============================================================================
TEST(ResetStatsBranch, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto agg = makeAggregatorBr("/test/reset_br/gen_fail");
    auto req = agg->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// =============================================================================
// handleSample: tag above max unreserved -> early return with success
// =============================================================================
TEST(ResetStatsBranch, HandleSample_AboveMaxUnreserved_EarlyReturn)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag_above_max");
    NsmSensorAggregator::TelemetrySample sample{
        static_cast<uint8_t>(NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE + 1),
        0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleSample: tag within range but not in map -> returns success (unknown)
// =============================================================================
TEST(ResetStatsBranch, HandleSample_UnmappedTag_ReturnsSuccess)
{
    auto agg = makeAggregatorBr("/test/reset_br/unmapped_tag");
    // Tag 10 is not in the tagToPropertyMap (which has 0-8)
    NsmSensorAggregator::TelemetrySample sample{10, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleSample: LastResetType (tag 7) with decode failure (0-length data)
// =============================================================================
TEST(ResetStatsBranch, HandleSample_LastResetType_DecodeError)
{
    auto agg = makeAggregatorBr("/test/reset_br/lrt_decode_err");
    NsmSensorAggregator::TelemetrySample sample{7, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// handleSample: BootReason (tag 8) with decode failure (0-length data)
// =============================================================================
TEST(ResetStatsBranch, HandleSample_BootReason_DecodeError)
{
    auto agg = makeAggregatorBr("/test/reset_br/br_decode_err");
    NsmSensorAggregator::TelemetrySample sample{8, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// handleSample: standard counter (tag 0) with decode failure (0-length data)
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Counter_DecodeError)
{
    auto agg = makeAggregatorBr("/test/reset_br/cnt_decode_err");
    NsmSensorAggregator::TelemetrySample sample{0, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// handleSample: tag 1 with valid data -> updates PF_FLR_ResetExitCount
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Tag1_UpdatesPfFlrExit)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag1_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(55, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        1, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->pfflrResetExitCount(), 55.0);
}

// =============================================================================
// handleSample: tag 2 with valid data -> ConventionalResetEntryCount
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Tag2_UpdatesConventionalEntry)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag2_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(77, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        2, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->conventionalResetEntryCount(),
                     77.0);
}

// =============================================================================
// handleSample: tag 3 with valid data -> ConventionalResetExitCount
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Tag3_UpdatesConventionalExit)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag3_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(88, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        3, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->conventionalResetExitCount(),
                     88.0);
}

// =============================================================================
// handleSample: tag 4 with valid data -> FundamentalResetEntryCount
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Tag4_UpdatesFundamentalEntry)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag4_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(99, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        4, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->fundamentalResetEntryCount(),
                     99.0);
}

// =============================================================================
// handleSample: tag 5 with valid data -> FundamentalResetExitCount
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Tag5_UpdatesFundamentalExit)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag5_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(33, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        5, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->fundamentalResetExitCount(), 33.0);
}

// =============================================================================
// handleSample: tag 6 with valid data -> IRoTResetExitCount
// =============================================================================
TEST(ResetStatsBranch, HandleSample_Tag6_UpdatesIRotExit)
{
    auto agg = makeAggregatorBr("/test/reset_br/tag6_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(11, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        6, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->iRoTResetExitCount(), 11.0);
}

// =============================================================================
// handleSample: LastResetType (tag 7) with valid data -> updates enum
// =============================================================================
TEST(ResetStatsBranch, HandleSample_LastResetType_ValidData)
{
    auto agg = makeAggregatorBr("/test/reset_br/lrt_ok");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    uint8_t resetType = 0; // PF_FLR = 0
    ASSERT_EQ(encode_reset_enum_data(resetType, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        7, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleSample: BootReason (tag 8) with multiple bits set
// =============================================================================
TEST(ResetStatsBranch, HandleSample_BootReason_MultipleBits)
{
    auto agg = makeAggregatorBr("/test/reset_br/br_multi");
    nsm_boot_reason_type_breakdown bits = {};
    bits.power_on = 1;
    bits.warm_reset = 1;
    bits.software = 1;

    uint64_t bootReasons[4] = {};
    std::memcpy(bootReasons, &bits, sizeof(bits));

    std::array<uint8_t, 32> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_256data(bootReasons, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        8, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto bootReasonList = agg->resetCountersIntf->bootReason();
    using BRT = ResetCountersIntf::BootReasonTypes;
    EXPECT_THAT(bootReasonList, Contains(BRT::PowerOn));
    EXPECT_THAT(bootReasonList, Contains(BRT::WarmReset));
    EXPECT_THAT(bootReasonList, Contains(BRT::Software));
}
