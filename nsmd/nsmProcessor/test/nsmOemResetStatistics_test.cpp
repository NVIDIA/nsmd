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

static auto& bus = utils::DBusHandler::getBus();

// Helper: create a fully constructed ResetStatisticsAggregator
static std::shared_ptr<ResetStatisticsAggregator>
    makeAggregator(const std::string& path)
{
    auto resetIntf = std::make_shared<ResetCountersIntf>(bus, path.c_str());
    auto assocIntf = std::make_unique<AssociationDefinitionsIntf>(bus,
                                                                  path.c_str());
    return std::make_shared<ResetStatisticsAggregator>(
        "test_reset", "NSM_Reset", path, resetIntf, std::move(assocIntf));
}

// =============================================================================
// Constructor
// =============================================================================

TEST(ResetStatisticsAggregator, Constructor_CreatesObject)
{
    auto agg = makeAggregator("/test/reset_stats/ctor");
    EXPECT_NE(agg, nullptr);
}

// =============================================================================
// genRequestMsg
// =============================================================================

TEST(ResetStatisticsAggregator, GenRequestMsg_ReturnsValidRequest)
{
    auto agg = makeAggregator("/test/reset_stats/genreq");
    const uint8_t eid{5};
    const uint8_t instanceId{0};
    auto req = agg->genRequestMsg(eid, instanceId);
    ASSERT_TRUE(req.has_value());
    EXPECT_GE(req->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto msg = reinterpret_cast<const nsm_msg*>(req->data());
    auto cmd = reinterpret_cast<const nsm_common_req*>(msg->payload);
    EXPECT_EQ(cmd->command, NSM_GET_DEVICE_RESET_STATISTICS);
}

TEST(ResetStatisticsAggregator, BadGenReq_InvalidInstanceId_ReturnsNullopt)
{
    auto agg = makeAggregator("/test/reset_stats/badreq");
    auto req = agg->genRequestMsg(5, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(req.has_value());
}

// =============================================================================
// handleSample – counter tags (0-6)
// =============================================================================

TEST(ResetStatisticsAggregator, HandleSample_CounterTag0_UpdatesPfFlrEntry)
{
    auto agg = makeAggregator("/test/reset_stats/counter_tag0");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(42, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        0, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->pfflrResetEntryCount(), 42.0);
}

TEST(ResetStatisticsAggregator, HandleSample_CounterTag1_UpdatesPfFlrExit)
{
    auto agg = makeAggregator("/test/reset_stats/counter_tag1");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    ASSERT_EQ(encode_reset_count_data(7, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        1, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_DOUBLE_EQ(agg->resetCountersIntf->pfflrResetExitCount(), 7.0);
}

TEST(ResetStatisticsAggregator, HandleSample_CounterTags2to6_Update)
{
    // Verify tags 2-6 also work without error
    const std::vector<uint8_t> counterTags = {2, 3, 4, 5, 6};
    for (auto tag : counterTags)
    {
        auto path = "/test/reset_stats/countertag_" + std::to_string((int)tag);
        auto agg = makeAggregator(path);
        std::array<uint8_t, 4> data{};
        size_t data_len = 0;
        ASSERT_EQ(encode_reset_count_data(10, data.data(), &data_len),
                  NSM_SW_SUCCESS);
        NsmSensorAggregator::TelemetrySample sample{
            tag, static_cast<uint8_t>(data_len), data.data(), true};
        auto rc = agg->handleSample(sample);
        EXPECT_EQ(rc, NSM_SW_SUCCESS) << "tag=" << (int)tag;
    }
}

// =============================================================================
// handleSample – LastResetType (tag 7)
// =============================================================================

TEST(ResetStatisticsAggregator, HandleSample_LastResetType_UpdatesEnum)
{
    auto agg = makeAggregator("/test/reset_stats/last_reset_type");
    std::array<uint8_t, 4> data{};
    size_t data_len = 0;
    // Conventional = 1 in diagnostics enum
    uint8_t resetType = 1;
    ASSERT_EQ(encode_reset_enum_data(resetType, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        7, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ResetStatisticsAggregator, HandleSample_LastResetType_InvalidData_Fails)
{
    auto agg = makeAggregator("/test/reset_stats/last_reset_invalid");
    // zero data length → decode_reset_enum_data fails
    NsmSensorAggregator::TelemetrySample sample{7, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// handleSample – BootReason (tag 8)
// =============================================================================

TEST(ResetStatisticsAggregator, HandleSample_BootReason_WakeUpBit)
{
    auto agg = makeAggregator("/test/reset_stats/boot_reason_wakeup");
    std::array<uint8_t, 32> data{};
    size_t data_len = 0;
    uint64_t bootReasons[4] = {0x01, 0, 0, 0}; // wake_up bit set
    ASSERT_EQ(encode_reset_count_256data(bootReasons, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        8, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto bootReasonList = agg->resetCountersIntf->bootReason();
    EXPECT_THAT(bootReasonList,
                Contains(ResetCountersIntf::BootReasonTypes::WakeUp));
}

TEST(ResetStatisticsAggregator, HandleSample_BootReason_AllZeroBits)
{
    auto agg = makeAggregator("/test/reset_stats/boot_reason_zero");
    std::array<uint8_t, 32> data{};
    size_t data_len = 0;
    uint64_t bootReasons[4] = {0, 0, 0, 0}; // no bits set
    ASSERT_EQ(encode_reset_count_256data(bootReasons, data.data(), &data_len),
              NSM_SW_SUCCESS);

    NsmSensorAggregator::TelemetrySample sample{
        8, static_cast<uint8_t>(data_len), data.data(), true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(agg->resetCountersIntf->bootReason().empty());
}

TEST(ResetStatisticsAggregator, HandleSample_BootReason_InvalidData_Fails)
{
    auto agg = makeAggregator("/test/reset_stats/boot_reason_invalid");
    // zero data length → decode_reset_count_256data fails
    NsmSensorAggregator::TelemetrySample sample{8, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// handleSample – special / unknown tags
// =============================================================================

TEST(ResetStatisticsAggregator, HandleSample_SpecialTag_EarlyReturn)
{
    auto agg = makeAggregator("/test/reset_stats/special_tag");
    // 0xFF > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE (0xEF)
    NsmSensorAggregator::TelemetrySample sample{0xFF, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ResetStatisticsAggregator, HandleSample_SpecialTagUUID_EarlyReturn)
{
    auto agg = makeAggregator("/test/reset_stats/special_tag_uuid");
    // 0xFE > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
    NsmSensorAggregator::TelemetrySample sample{0xFE, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST(ResetStatisticsAggregator, HandleSample_UnknownTag_ReturnsSuccess)
{
    auto agg = makeAggregator("/test/reset_stats/unknown_tag");
    // Tag 9 is not in tagToPropertyMap – returns early with success + debug log
    NsmSensorAggregator::TelemetrySample sample{9, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handleSample – invalid count data (tags 0-6)
// =============================================================================

TEST(ResetStatisticsAggregator, HandleSample_CounterInvalidData_Fails)
{
    auto agg = makeAggregator("/test/reset_stats/invalid_count");
    // Tag 0 with zero data_len → decode_reset_count_data fails
    NsmSensorAggregator::TelemetrySample sample{0, 0, nullptr, true};
    auto rc = agg->handleSample(sample);
    EXPECT_EQ(rc, NSM_SW_ERROR_LENGTH);
}

// =============================================================================
// handleSample – BootReason all bits set (covers all TRUE branches)
// =============================================================================

TEST(ResetStatisticsAggregator, HandleSample_BootReason_AllBitsSet)
{
    auto agg = makeAggregator("/test/reset_stats/boot_reason_all");

    // Set all named bits in nsm_boot_reason_type_breakdown to 1 so that
    // every TRUE branch in updateBootReasonProperty is executed.
    nsm_boot_reason_type_breakdown all_bits = {};
    all_bits.wake_up = 1;
    all_bits.power_on = 1;
    all_bits.voltage_detect = 1;
    all_bits.warm_reset = 1;
    all_bits.fatal_error = 1;
    all_bits.pin = 1;
    all_bits.debug_access_port = 1;
    all_bits.reset_timeout = 1;
    all_bits.low_power_acknowledge_timeout = 1;
    all_bits.system_clock_generator = 1;
    all_bits.windowed_watchdog_0 = 1;
    all_bits.software = 1;
    all_bits.lockup_reset = 1;
    all_bits.cpu1 = 1;
    all_bits.vbat = 1;
    all_bits.windowed_watchdog_1 = 1;
    all_bits.code_watchdog_0 = 1;
    all_bits.code_watchdog_1 = 1;
    all_bits.jtag = 1;
    all_bits.security_violation = 1;
    all_bits.tamper = 1;
    all_bits.iaccviol = 1;
    all_bits.daccviol = 1;
    all_bits.munstkerr = 1;
    all_bits.mstkerr = 1;
    all_bits.mmfarvalid = 1;
    all_bits.bfarvalid = 1;
    all_bits.stkerr = 1;
    all_bits.unstkerr = 1;
    all_bits.imprecise_error = 1;
    all_bits.precise_error = 1;
    all_bits.ibuserr = 1;
    all_bits.undefinstr = 1;
    all_bits.invstate = 1;
    all_bits.invpc = 1;
    all_bits.nocp = 1;
    all_bits.unaligned = 1;
    all_bits.devbyzero = 1;
    all_bits.vecttbl = 1;
    all_bits.forced = 1;
    all_bits.debugevt = 1;
    all_bits.mctp = 1;
    all_bits.i2c = 1;
    all_bits.i3c = 1;
    all_bits.pldm = 1;
    all_bits.usb = 1;
    all_bits.flash = 1;
    all_bits.logger = 1;
    all_bits.spdm = 1;

    uint64_t bootReasons[4] = {};
    std::memcpy(bootReasons, &all_bits, sizeof(all_bits));

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
    EXPECT_THAT(bootReasonList, Contains(BRT::WakeUp));
    EXPECT_THAT(bootReasonList, Contains(BRT::PowerOn));
    EXPECT_THAT(bootReasonList, Contains(BRT::VoltageDetect));
    EXPECT_THAT(bootReasonList, Contains(BRT::WarmReset));
    EXPECT_THAT(bootReasonList, Contains(BRT::FatalError));
    EXPECT_THAT(bootReasonList, Contains(BRT::Pin));
    EXPECT_THAT(bootReasonList, Contains(BRT::DebugAccessPort));
    EXPECT_THAT(bootReasonList, Contains(BRT::ResetTimeout));
    EXPECT_THAT(bootReasonList, Contains(BRT::LowPowerAcknowledgeTimeout));
    EXPECT_THAT(bootReasonList, Contains(BRT::SystemClockGenerator));
    EXPECT_THAT(bootReasonList, Contains(BRT::WindowedWatchdog0));
    EXPECT_THAT(bootReasonList, Contains(BRT::Software));
    EXPECT_THAT(bootReasonList, Contains(BRT::LockupReset));
    EXPECT_THAT(bootReasonList, Contains(BRT::CPU1));
    EXPECT_THAT(bootReasonList, Contains(BRT::VBAT));
    EXPECT_THAT(bootReasonList, Contains(BRT::WindowedWatchdog1));
    EXPECT_THAT(bootReasonList, Contains(BRT::CodeWatchdog0));
    EXPECT_THAT(bootReasonList, Contains(BRT::CodeWatchdog1));
    EXPECT_THAT(bootReasonList, Contains(BRT::JTAG));
    EXPECT_THAT(bootReasonList, Contains(BRT::SecurityViolation));
    EXPECT_THAT(bootReasonList, Contains(BRT::Tamper));
    EXPECT_THAT(bootReasonList, Contains(BRT::IAccViol));
    EXPECT_THAT(bootReasonList, Contains(BRT::DAccViol));
    EXPECT_THAT(bootReasonList, Contains(BRT::Munstkerr));
    EXPECT_THAT(bootReasonList, Contains(BRT::Mstkerr));
    EXPECT_THAT(bootReasonList, Contains(BRT::MMFarValid));
    EXPECT_THAT(bootReasonList, Contains(BRT::BFarValid));
    EXPECT_THAT(bootReasonList, Contains(BRT::Stkerr));
    EXPECT_THAT(bootReasonList, Contains(BRT::Unstkerr));
    EXPECT_THAT(bootReasonList, Contains(BRT::ImpreciseError));
    EXPECT_THAT(bootReasonList, Contains(BRT::PreciseError));
    EXPECT_THAT(bootReasonList, Contains(BRT::IBusErr));
    EXPECT_THAT(bootReasonList, Contains(BRT::UndefInstr));
    EXPECT_THAT(bootReasonList, Contains(BRT::InvState));
    EXPECT_THAT(bootReasonList, Contains(BRT::InvPC));
    EXPECT_THAT(bootReasonList, Contains(BRT::NoCP));
    EXPECT_THAT(bootReasonList, Contains(BRT::Unaligned));
    EXPECT_THAT(bootReasonList, Contains(BRT::DevByZero));
    EXPECT_THAT(bootReasonList, Contains(BRT::VectTbl));
    EXPECT_THAT(bootReasonList, Contains(BRT::Forced));
    EXPECT_THAT(bootReasonList, Contains(BRT::DebugEvt));
    EXPECT_THAT(bootReasonList, Contains(BRT::MCTP));
    EXPECT_THAT(bootReasonList, Contains(BRT::I2C));
    EXPECT_THAT(bootReasonList, Contains(BRT::I3C));
    EXPECT_THAT(bootReasonList, Contains(BRT::PLDM));
    EXPECT_THAT(bootReasonList, Contains(BRT::USB));
    EXPECT_THAT(bootReasonList, Contains(BRT::Flash));
    EXPECT_THAT(bootReasonList, Contains(BRT::Logger));
    EXPECT_THAT(bootReasonList, Contains(BRT::SPDM));
}
