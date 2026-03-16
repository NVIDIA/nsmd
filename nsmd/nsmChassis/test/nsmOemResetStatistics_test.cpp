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
using namespace ::testing;

#include "base.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include <sdbusplus/bus.hpp>

#define private public
#define protected public

#include "nsmOemResetStatistics.hpp"

#undef private
#undef protected

using namespace nsm;

static auto busRSA = sdbusplus::bus::new_default();
static std::string sNameRSA("dummy_sensor_rsa");
static std::string sTypeRSA("dummy_type_rsa");
static std::string
    invPathRSA("/xyz/openbmc_project/inventory/dummy_device_rsa");

// =============================================================================
// ResetStatisticsAggregator tests
// =============================================================================

TEST(ResetStatisticsAggregator, GenRequestMsg_Success)
{
    std::string resetPath = invPathRSA + "/ResetStatistics";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef =
        std::make_unique<AssociationDefinitionsIntf>(busRSA, resetPath.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath,
                                     resetCountersIntf,
                                     std::move(associationDef));

    const uint8_t eid{10};
    const uint8_t instanceId{5};

    auto request = sensor.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());

    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto command = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(command->command, NSM_GET_DEVICE_RESET_STATISTICS);
    EXPECT_EQ(command->data_size, 0);
}

TEST(ResetStatisticsAggregator, HandleSample_ResetCountTag0)
{
    std::string resetPath2 = invPathRSA + "/ResetStatistics2";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath2.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath2.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath2,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 0 = PF_FLR_ResetEntryCount (uint16 counter)
    uint16_t countValue = 42;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = sizeof(uint16_t);
    sample.data = reinterpret_cast<const uint8_t*>(&countValue);
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->pfflrResetEntryCount(), 42.0);
}

TEST(ResetStatisticsAggregator, HandleSample_ResetCountTag1)
{
    std::string resetPath3 = invPathRSA + "/ResetStatistics3";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath3.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath3.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath3,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 1 = PF_FLR_ResetExitCount
    uint16_t countValue = 99;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 1;
    sample.data_len = sizeof(uint16_t);
    sample.data = reinterpret_cast<const uint8_t*>(&countValue);
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->pfflrResetExitCount(), 99.0);
}

TEST(ResetStatisticsAggregator, HandleSample_LastResetTypeTag7)
{
    std::string resetPath4 = invPathRSA + "/ResetStatistics4";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath4.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath4.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath4,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 7 = LastResetType (enum8)
    uint8_t resetType2 = 0; // PF_FLR
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 7;
    sample.data_len = sizeof(uint8_t);
    sample.data = &resetType2;
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ResetStatisticsAggregator, HandleSample_BootReasonTag8)
{
    std::string resetPath5 = invPathRSA + "/ResetStatistics5";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath5.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath5.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath5,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 8 = BootReason (256-bit data, i.e. 32 bytes / 4 uint64_t)
    // Set power_on bit (bit 1)
    std::array<uint64_t, 4> bootReasonData = {};
    bootReasonData[0] = 0x02; // power_on bit set
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 8;
    sample.data_len = sizeof(bootReasonData);
    sample.data = reinterpret_cast<const uint8_t*>(bootReasonData.data());
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto reasons = resetCountersIntf->bootReason();
    bool foundPowerOn = false;
    for (const auto& r : reasons)
    {
        if (r == BootReasonTypes::PowerOn)
        {
            foundPowerOn = true;
        }
    }
    EXPECT_TRUE(foundPowerOn);
}

TEST(ResetStatisticsAggregator, HandleSample_SpecialTag_Ignored)
{
    std::string resetPath6 = invPathRSA + "/ResetStatistics6";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath6.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath6.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath6,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tags > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE (0xEF) should
    // be silently ignored
    uint8_t dummyData = 0;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 0xFE; // UUID special tag
    sample.data_len = 1;
    sample.data = &dummyData;
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ResetStatisticsAggregator, HandleSample_UnknownTag)
{
    std::string resetPath7 = invPathRSA + "/ResetStatistics7";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath7.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath7.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath7,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 50 is not in the tagToPropertyMap (only 0-8 are defined)
    uint8_t dummyData = 0;
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 50;
    sample.data_len = 1;
    sample.data = &dummyData;
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ResetStatisticsAggregator, HandleSample_InvalidResetCountData)
{
    std::string resetPath8 = invPathRSA + "/ResetStatistics8";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath8.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath8.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath8,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 0 with 0-length data should fail decode
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 0;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(ResetStatisticsAggregator, HandleSample_InvalidLastResetTypeData)
{
    std::string resetPath9 = invPathRSA + "/ResetStatistics9";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath9.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath9.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath9,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 7 (LastResetType) with 0-length data should fail decode
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 7;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(ResetStatisticsAggregator, HandleSample_InvalidBootReasonData)
{
    std::string resetPath10 = invPathRSA + "/ResetStatistics10";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath10.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath10.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath10,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Tag 8 (BootReason) with 0-length data should fail decode
    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 8;
    sample.data_len = 0;
    sample.data = nullptr;
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

TEST(ResetStatisticsAggregator, HandleSample_ConventionalResetTags)
{
    std::string resetPath11 = invPathRSA + "/ResetStatistics11";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath11.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath11.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath11,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Test tags 2-6 (ConventionalResetEntry/Exit, FundamentalResetEntry/Exit,
    // IRoTResetExitCount)
    uint16_t countVal = 0;

    // Tag 2: ConventionalResetEntryCount
    countVal = 10;
    NsmSensorAggregator::TelemetrySample sample2 = {};
    sample2.tag = 2;
    sample2.data_len = sizeof(uint16_t);
    sample2.data = reinterpret_cast<const uint8_t*>(&countVal);
    sample2.valid = true;
    EXPECT_EQ(sensor.handleSample(sample2), NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->conventionalResetEntryCount(), 10.0);

    // Tag 3: ConventionalResetExitCount
    countVal = 20;
    NsmSensorAggregator::TelemetrySample sample3 = {};
    sample3.tag = 3;
    sample3.data_len = sizeof(uint16_t);
    sample3.data = reinterpret_cast<const uint8_t*>(&countVal);
    sample3.valid = true;
    EXPECT_EQ(sensor.handleSample(sample3), NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->conventionalResetExitCount(), 20.0);

    // Tag 4: FundamentalResetEntryCount
    countVal = 30;
    NsmSensorAggregator::TelemetrySample sample4 = {};
    sample4.tag = 4;
    sample4.data_len = sizeof(uint16_t);
    sample4.data = reinterpret_cast<const uint8_t*>(&countVal);
    sample4.valid = true;
    EXPECT_EQ(sensor.handleSample(sample4), NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->fundamentalResetEntryCount(), 30.0);

    // Tag 5: FundamentalResetExitCount
    countVal = 40;
    NsmSensorAggregator::TelemetrySample sample5 = {};
    sample5.tag = 5;
    sample5.data_len = sizeof(uint16_t);
    sample5.data = reinterpret_cast<const uint8_t*>(&countVal);
    sample5.valid = true;
    EXPECT_EQ(sensor.handleSample(sample5), NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->fundamentalResetExitCount(), 40.0);

    // Tag 6: IRoTResetExitCount
    countVal = 50;
    NsmSensorAggregator::TelemetrySample sample6 = {};
    sample6.tag = 6;
    sample6.data_len = sizeof(uint16_t);
    sample6.data = reinterpret_cast<const uint8_t*>(&countVal);
    sample6.valid = true;
    EXPECT_EQ(sensor.handleSample(sample6), NSM_SW_SUCCESS);
    EXPECT_EQ(resetCountersIntf->iRoTResetExitCount(), 50.0);
}

TEST(ResetStatisticsAggregator, HandleSample_BootReasonMultipleBits)
{
    std::string resetPath12 = invPathRSA + "/ResetStatistics12";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath12.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath12.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath12,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // Set wake_up (bit 0), warm_reset (bit 4), pin (bit 8)
    std::array<uint64_t, 4> bootReasonData = {};
    bootReasonData[0] = (1ULL << 0) | // wake_up
                        (1ULL << 4) | // warm_reset
                        (1ULL << 8);  // pin

    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 8;
    sample.data_len = sizeof(bootReasonData);
    sample.data = reinterpret_cast<const uint8_t*>(bootReasonData.data());
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto reasons = resetCountersIntf->bootReason();
    bool foundWakeUp = false;
    bool foundWarmReset = false;
    bool foundPin = false;
    for (const auto& r : reasons)
    {
        if (r == BootReasonTypes::WakeUp)
            foundWakeUp = true;
        if (r == BootReasonTypes::WarmReset)
            foundWarmReset = true;
        if (r == BootReasonTypes::Pin)
            foundPin = true;
    }
    EXPECT_TRUE(foundWakeUp);
    EXPECT_TRUE(foundWarmReset);
    EXPECT_TRUE(foundPin);
}

TEST(ResetStatisticsAggregator, HandleSample_BootReasonEmpty)
{
    std::string resetPath13 = invPathRSA + "/ResetStatistics13";
    auto resetCountersIntf =
        std::make_shared<ResetCountersIntf>(busRSA, resetPath13.c_str());
    resetCountersIntf->lastResetType(
        ResetCountersIntf::ResetTypes::Conventional);
    resetCountersIntf->pfflrResetEntryCount(0);
    resetCountersIntf->pfflrResetExitCount(0);
    resetCountersIntf->conventionalResetEntryCount(0);
    resetCountersIntf->conventionalResetExitCount(0);
    resetCountersIntf->fundamentalResetEntryCount(0);
    resetCountersIntf->fundamentalResetExitCount(0);
    resetCountersIntf->iRoTResetExitCount(0);

    auto associationDef = std::make_unique<AssociationDefinitionsIntf>(
        busRSA, resetPath13.c_str());

    std::string resetName = "ResetMetrics";
    std::string resetType = "NSM_ResetStatistics";
    ResetStatisticsAggregator sensor(resetName, resetType, resetPath13,
                                     resetCountersIntf,
                                     std::move(associationDef));

    // All bits zero - should produce empty boot reason list
    std::array<uint64_t, 4> bootReasonData = {};

    NsmSensorAggregator::TelemetrySample sample = {};
    sample.tag = 8;
    sample.data_len = sizeof(bootReasonData);
    sample.data = reinterpret_cast<const uint8_t*>(bootReasonData.data());
    sample.valid = true;

    int rc = sensor.handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto reasons = resetCountersIntf->bootReason();
    EXPECT_TRUE(reasons.empty());
}
