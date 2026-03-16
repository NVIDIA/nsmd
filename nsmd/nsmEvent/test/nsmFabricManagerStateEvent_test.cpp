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
 * Tests for nsmd/nsmEvent/nsmFabricManagerStateEvent.cpp
 *
 *   - NsmFabricManagerStateEvent::NsmFabricManagerStateEvent (constructor)
 *   - NsmFabricManagerStateEvent::handle (decode failure → returns error)
 *   - NsmFabricManagerStateEvent::handle (NOT_RECEIVED →
 * FMReportStatus::NotReceived)
 *   - NsmFabricManagerStateEvent::handle (TIMEOUT →
 * Timeout/Unknown/StandbyOffline)
 *   - NsmFabricManagerStateEvent::handle (unknown report_status →
 * Unknown/Unknown)
 *   - NsmFabricManagerStateEvent::handle (RECEIVED + OFFLINE → Starting)
 *   - NsmFabricManagerStateEvent::handle (RECEIVED + STANDBY → StandbyOffline)
 *   - NsmFabricManagerStateEvent::handle (RECEIVED + CONFIGURED → Enabled)
 *   - NsmFabricManagerStateEvent::handle (RECEIVED + RESERVED_TIMEOUT →
 * UnavailableOffline)
 *   - NsmFabricManagerStateEvent::handle (RECEIVED + ERROR →
 * UnavailableOffline)
 *   - NsmFabricManagerStateEvent::handle (RECEIVED + default fm_state →
 * StandbyOffline)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "base.h"
#include "network-ports.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmEvent/nsmFabricManagerStateEvent.hpp"
#include "nsmManagers/nsmFabricManager.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// Helper: build a valid encoded fabric manager state event message
static std::vector<uint8_t> buildEventMsg(uint8_t fm_state,
                                          uint8_t report_status,
                                          uint64_t last_restart_ts = 0,
                                          uint64_t duration_since_restart = 0)
{
    nsm_get_fabric_manager_state_event_payload payload = {};
    payload.fm_state = fm_state;
    payload.report_status = report_status;
    payload.last_restart_timestamp = last_restart_ts;
    payload.duration_since_last_restart_sec = duration_since_restart;

    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                             sizeof(payload));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    auto rc = encode_nsm_get_fabric_manager_state_event(0, false, payload, msg);
    (void)rc; // Always NSM_SW_SUCCESS for valid payload
    return buf;
}

// Helper: construct all objects needed for NsmFabricManagerStateEvent
struct FMEventFixtureObjs
{
    std::shared_ptr<FabricManagerIntf> fmIntf;
    std::shared_ptr<OperaStatusIntf> opIntf;
    std::shared_ptr<NsmAggregateFabricManagerState> aggregateState;
    std::shared_ptr<NsmFabricManagerStateEvent> event;
};

static FMEventFixtureObjs makeFMEvent(const std::string& basePath,
                                      const std::string& aggPath)
{
    FMEventFixtureObjs o;
    o.fmIntf = std::make_shared<FabricManagerIntf>(testBus, basePath.c_str());
    o.opIntf = std::make_shared<OperaStatusIntf>(testBus, basePath.c_str());
    o.aggregateState = NsmAggregateFabricManagerState::getInstance(
        aggPath, o.fmIntf, o.opIntf, "test");
    o.event = std::make_shared<NsmFabricManagerStateEvent>(
        "fm_event", "NSM_FabricManagerStateEvent", o.fmIntf, o.opIntf,
        o.aggregateState);
    return o;
}

class NsmFabricManagerStateEventTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Reset the singleton before each test
        NsmAggregateFabricManagerState::nsmAggregateFabricManagerState.reset();
        NsmAggregateFabricManagerState::associatedFabricManagerIntfs.clear();
        NsmAggregateFabricManagerState::associatedOperationalStatusIntfs
            .clear();
        NsmAggregateFabricManagerState::aggregateFMObjPath = "";
    }
};

// =============================================================================
// Constructor
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest, Constructor_CreatesObject)
{
    auto o = makeFMEvent("/test/fmse/ctor", "/test/fmse/ctor_agg");

    EXPECT_EQ(o.event->getName(), "fm_event");
    EXPECT_EQ(o.event->getType(), "NSM_FabricManagerStateEvent");
}

// =============================================================================
// handle – decode failure (buffer too small)
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest, Handle_ShortBuffer_ReturnsError)
{
    auto o = makeFMEvent("/test/fmse/short_buf", "/test/fmse/short_buf_agg");

    // Buffer is too small to decode
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 1, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_NE(rc, NSM_SW_SUCCESS);
}

// =============================================================================
// handle – NOT_RECEIVED
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest, Handle_NotReceived_SetsNotReceived)
{
    auto o = makeFMEvent("/test/fmse/not_recv", "/test/fmse/not_recv_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_OFFLINE,
                             NSM_FM_REPORT_STATUS_NOT_RECEIVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->reportStatus(), FMReportStatus::NotReceived);
}

// =============================================================================
// handle – TIMEOUT
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Timeout_SetsTimeoutAndUnknownState)
{
    auto o = makeFMEvent("/test/fmse/timeout", "/test/fmse/timeout_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_OFFLINE,
                             NSM_FM_REPORT_STATUS_TIMEOUT);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->reportStatus(), FMReportStatus::Timeout);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Unknown);
    EXPECT_EQ(o.opIntf->state(), OpState::StandbyOffline);
}

// =============================================================================
// handle – unknown report_status (default branch)
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest, Handle_UnknownReportStatus_SetsUnknown)
{
    auto o = makeFMEvent("/test/fmse/unknown_rs", "/test/fmse/unknown_rs_agg");

    // Use report_status = 0 (NSM_FM_REPORT_STATUS_RESERVED) — hits default
    auto buf = buildEventMsg(NSM_FM_STATE_OFFLINE,
                             NSM_FM_REPORT_STATUS_RESERVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->reportStatus(), FMReportStatus::Unknown);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Unknown);
    EXPECT_EQ(o.opIntf->state(), OpState::StandbyOffline);
}

// =============================================================================
// handle – RECEIVED + NSM_FM_STATE_OFFLINE
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Received_StateOffline_SetsStarting)
{
    auto o = makeFMEvent("/test/fmse/offline", "/test/fmse/offline_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_OFFLINE,
                             NSM_FM_REPORT_STATUS_RECEIVED, 1000, 100);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->reportStatus(), FMReportStatus::Received);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Offline);
    EXPECT_EQ(o.opIntf->state(), OpState::Starting);
    EXPECT_EQ(o.fmIntf->lastRestartTime(), 1000u);
    EXPECT_EQ(o.fmIntf->lastRestartDuration(), 100u);
}

// =============================================================================
// handle – RECEIVED + NSM_FM_STATE_STANDBY
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Received_StateStandby_SetsStandbyOffline)
{
    auto o = makeFMEvent("/test/fmse/standby", "/test/fmse/standby_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_STANDBY,
                             NSM_FM_REPORT_STATUS_RECEIVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Standby);
    EXPECT_EQ(o.opIntf->state(), OpState::StandbyOffline);
}

// =============================================================================
// handle – RECEIVED + NSM_FM_STATE_CONFIGURED
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Received_StateConfigured_SetsEnabled)
{
    auto o = makeFMEvent("/test/fmse/configured", "/test/fmse/configured_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_CONFIGURED,
                             NSM_FM_REPORT_STATUS_RECEIVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Configured);
    EXPECT_EQ(o.opIntf->state(), OpState::Enabled);
}

// =============================================================================
// handle – RECEIVED + NSM_FM_STATE_RESERVED_TIMEOUT
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Received_StateReservedTimeout_SetsUnavailableOffline)
{
    auto o = makeFMEvent("/test/fmse/res_timeout",
                         "/test/fmse/res_timeout_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_RESERVED_TIMEOUT,
                             NSM_FM_REPORT_STATUS_RECEIVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Timeout);
    EXPECT_EQ(o.opIntf->state(), OpState::UnavailableOffline);
}

// =============================================================================
// handle – RECEIVED + NSM_FM_STATE_ERROR
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Received_StateError_SetsUnavailableOffline)
{
    auto o = makeFMEvent("/test/fmse/error", "/test/fmse/error_agg");

    auto buf = buildEventMsg(NSM_FM_STATE_ERROR, NSM_FM_REPORT_STATUS_RECEIVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Error);
    EXPECT_EQ(o.opIntf->state(), OpState::UnavailableOffline);
}

// =============================================================================
// handle – RECEIVED + unknown fm_state (default inner branch)
// =============================================================================

TEST_F(NsmFabricManagerStateEventTest,
       Handle_Received_UnknownFmState_SetsUnknownStandbyOffline)
{
    auto o = makeFMEvent("/test/fmse/unknown_fm", "/test/fmse/unknown_fm_agg");

    // NSM_FM_STATE_RESERVED = 0x00 hits the default inner branch
    auto buf = buildEventMsg(NSM_FM_STATE_RESERVED,
                             NSM_FM_REPORT_STATUS_RECEIVED);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = o.event->handle(5, {}, {}, msg, buf.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(o.fmIntf->fmState(), FMState::Unknown);
    EXPECT_EQ(o.opIntf->state(), OpState::StandbyOffline);
}
