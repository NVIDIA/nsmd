/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for nsmFabricManager.cpp
 *
 * Targets uncovered branches:
 * - genRequestMsg: encode fail (instanceId > NSM_INSTANCE_MAX)
 * - handleResponseMsg: decode fail (truncated buffer, valgrind-safe)
 * - handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR → skips update
 * - handleResponseMsg: success with all report_status variants
 * - handleResponseMsg: success with all fm_state variants under Received
 * - updateAggregateFabricManagerState: no Received FMs (all NotReceived)
 * - updateAggregateFabricManagerState: first FM not received, second received
 * - NsmAggregateFabricManagerState::getInstance: path mismatch branch
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "base.h"

#include "nsmFabricManager.hpp"

// ============================================================================
// Fixture
// ============================================================================

class NsmFabricManagerBranchTest : public Test
{
  protected:
    sdbusplus::bus_t& bus = utils::DBusHandler::getBus();

    void SetUp() override
    {
        nsm::NsmAggregateFabricManagerState::nsmAggregateFabricManagerState
            .reset();
        nsm::NsmAggregateFabricManagerState::associatedFabricManagerIntfs
            .clear();
        nsm::NsmAggregateFabricManagerState::associatedOperationalStatusIntfs
            .clear();
        nsm::NsmAggregateFabricManagerState::aggregateFMObjPath = "";
    }

    nsm::NsmFabricManagerState makeFM(const std::string& suffix)
    {
        std::string name = "FM_" + suffix;
        std::string type = "FabricManager";
        std::string path = "/test/fm_branch/" + suffix + "/";
        std::string pathFM = "/test/fm_branch/aggregate_" + suffix;
        std::string desc = "Branch Test FM " + suffix;
        return nsm::NsmFabricManagerState(name, type, path, pathFM, bus, desc);
    }

    // Helper: encode a valid Received response with given fm_state
    std::vector<uint8_t> makeReceivedResp(uint8_t fmState,
                                          uint16_t timestamp = 100,
                                          uint16_t duration = 10)
    {
        struct nsm_fabric_manager_state_data d = {
            .fm_state = fmState,
            .report_status = NSM_FM_REPORT_STATUS_RECEIVED,
            .last_restart_timestamp = timestamp,
            .duration_since_last_restart_sec = duration};
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                                 sizeof(nsm_fabric_manager_state_data));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_get_fabric_manager_state_resp(1, NSM_SUCCESS, ERR_NULL, &d, msg);
        return buf;
    }

    // Helper: encode a response with given report_status
    std::vector<uint8_t> makeReportStatusResp(uint8_t reportStatus,
                                              uint8_t fmState = 0)
    {
        struct nsm_fabric_manager_state_data d = {
            .fm_state = fmState,
            .report_status = reportStatus,
            .last_restart_timestamp = 0,
            .duration_since_last_restart_sec = 0};
        std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) +
                                 sizeof(nsm_fabric_manager_state_data));
        auto* msg = reinterpret_cast<nsm_msg*>(buf.data());
        encode_get_fabric_manager_state_resp(1, NSM_SUCCESS, ERR_NULL, &d, msg);
        return buf;
    }
};

// ============================================================================
// genRequestMsg: encode fail (instanceId > NSM_INSTANCE_MAX)
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, GenReq_BadInstanceId_ReturnsNullopt)
{
    auto fm = makeFM("gen_bad");
    auto request = fm.genRequestMsg(10, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

// ============================================================================
// genRequestMsg: success
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, GenReq_ValidInstanceId_ReturnsValue)
{
    auto fm = makeFM("gen_good");
    auto request = fm.genRequestMsg(10, 1);
    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_req));
}

// ============================================================================
// handleResponseMsg: decode fail (truncated buffer, valgrind-safe)
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, HandleResp_DecodeFail_ReturnsError)
{
    auto fm = makeFM("decode_fail");

    // Valgrind-safe: 7 bytes with cc=0 but too short for full response
    std::vector<uint8_t> buf(sizeof(nsm_msg_hdr) + 2, 0);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = fm.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// handleResponseMsg: rc==success, cc==NSM_ERROR → skips update
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, HandleResp_NonZeroCC_SkipsUpdate)
{
    auto fm = makeFM("cc_err");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = fm.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);

    // State should NOT have been updated
    EXPECT_EQ(fm.getFabricManagerIntf()->reportStatus(),
              nsm::FMReportStatus::NotReceived);
}

// ============================================================================
// handleResponseMsg: all fm_state values under Received report_status
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedOffline)
{
    auto fm = makeFM("r_offline");
    auto resp = makeReceivedResp(NSM_FM_STATE_OFFLINE);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Offline);
    EXPECT_EQ(fm.getOperaStatusIntf()->state(), nsm::OpState::Starting);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedStandby)
{
    auto fm = makeFM("r_standby");
    auto resp = makeReceivedResp(NSM_FM_STATE_STANDBY);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Standby);
    EXPECT_EQ(fm.getOperaStatusIntf()->state(), nsm::OpState::StandbyOffline);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedConfigured)
{
    auto fm = makeFM("r_configured");
    auto resp = makeReceivedResp(NSM_FM_STATE_CONFIGURED);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Configured);
    EXPECT_EQ(fm.getOperaStatusIntf()->state(), nsm::OpState::Enabled);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedTimeout)
{
    auto fm = makeFM("r_timeout");
    auto resp = makeReceivedResp(NSM_FM_STATE_RESERVED_TIMEOUT);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Timeout);
    EXPECT_EQ(fm.getOperaStatusIntf()->state(),
              nsm::OpState::UnavailableOffline);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedError)
{
    auto fm = makeFM("r_error");
    auto resp = makeReceivedResp(NSM_FM_STATE_ERROR);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Error);
    EXPECT_EQ(fm.getOperaStatusIntf()->state(),
              nsm::OpState::UnavailableOffline);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedDefaultFmState)
{
    auto fm = makeFM("r_default");
    auto resp = makeReceivedResp(NSM_FM_STATE_RESERVED);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Unknown);
    EXPECT_EQ(fm.getOperaStatusIntf()->state(), nsm::OpState::StandbyOffline);
}

// ============================================================================
// handleResponseMsg: report_status variants
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReportStatusNotReceived)
{
    auto fm = makeFM("rs_not");
    auto resp = makeReportStatusResp(NSM_FM_REPORT_STATUS_NOT_RECEIVED);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->reportStatus(),
              nsm::FMReportStatus::NotReceived);
    EXPECT_EQ(fm.getFabricManagerIntf()->fmState(), nsm::FMState::Unknown);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReportStatusTimeout)
{
    auto fm = makeFM("rs_timeout");
    auto resp = makeReportStatusResp(NSM_FM_REPORT_STATUS_TIMEOUT);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->reportStatus(),
              nsm::FMReportStatus::Timeout);
}

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReportStatusDefault)
{
    auto fm = makeFM("rs_default");
    auto resp = makeReportStatusResp(NSM_FM_REPORT_STATUS_RESERVED);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->reportStatus(),
              nsm::FMReportStatus::Unknown);
}

// ============================================================================
// Aggregate: no Received FMs → all skipped by first continue
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, Aggregate_AllNotReceived_DefaultState)
{
    std::string type = "FabricManager";
    std::string n0 = "FM_agg0", n1 = "FM_agg1";
    std::string p0 = "/test/fm_branch/agg0/";
    std::string p1 = "/test/fm_branch/agg1/";
    std::string pFM = "/test/fm_branch/agg_path";
    std::string desc = "Agg Test";

    nsm::NsmFabricManagerState fm0(n0, type, p0, pFM, bus, desc);
    nsm::NsmFabricManagerState fm1(n1, type, p1, pFM, bus, desc);

    // Neither FM has received a response → both are NotReceived
    // Directly call updateAggregateFabricManagerState
    auto agg = fm0.getAggregateFabricManagerState();
    ASSERT_NE(agg, nullptr);
    agg->updateAggregateFabricManagerState();

    // Both switches pending → aggregate is Pending (NotReceived), FMState
    // Unknown
    EXPECT_EQ(agg->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::NotReceived);
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

// ============================================================================
// Aggregate: first FM not received, second received → else branch (first
// received) sets state from second FM
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, Aggregate_FirstNotReceivedSecondReceived)
{
    std::string type = "FabricManager";
    std::string n0 = "FM_fnr0", n1 = "FM_fnr1";
    std::string p0 = "/test/fm_branch/fnr0/";
    std::string p1 = "/test/fm_branch/fnr1/";
    std::string pFM = "/test/fm_branch/fnr_path";
    std::string desc = "FNR Test";

    nsm::NsmFabricManagerState fm0(n0, type, p0, pFM, bus, desc);
    nsm::NsmFabricManagerState fm1(n1, type, p1, pFM, bus, desc);

    // Only set FM1 to Received/Configured
    auto resp = makeReceivedResp(NSM_FM_STATE_CONFIGURED);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    fm1.handleResponseMsg(msg, resp.size());

    // Aggregate should reflect FM1's state
    auto agg = fm0.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->reportStatus(),
              nsm::FMReportStatus::Received);
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), nsm::FMState::Configured);
}

// ============================================================================
// Aggregate: two received, same state → continue branch in inner if
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, Aggregate_TwoReceivedSameState)
{
    std::string type = "FabricManager";
    std::string n0 = "FM_same0", n1 = "FM_same1";
    std::string p0 = "/test/fm_branch/same0/";
    std::string p1 = "/test/fm_branch/same1/";
    std::string pFM = "/test/fm_branch/same_path";
    std::string desc = "Same Test";

    nsm::NsmFabricManagerState fm0(n0, type, p0, pFM, bus, desc);
    nsm::NsmFabricManagerState fm1(n1, type, p1, pFM, bus, desc);

    auto resp = makeReceivedResp(NSM_FM_STATE_CONFIGURED);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    fm0.handleResponseMsg(msg, resp.size());
    fm1.handleResponseMsg(msg, resp.size());

    auto agg = fm0.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), nsm::FMState::Configured);
}

// ============================================================================
// Aggregate: two received, different states → Unknown + break
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, Aggregate_TwoReceivedDifferentStates)
{
    std::string type = "FabricManager";
    std::string n0 = "FM_diff0", n1 = "FM_diff1";
    std::string p0 = "/test/fm_branch/diff0/";
    std::string p1 = "/test/fm_branch/diff1/";
    std::string pFM = "/test/fm_branch/diff_path";
    std::string desc = "Diff Test";

    nsm::NsmFabricManagerState fm0(n0, type, p0, pFM, bus, desc);
    nsm::NsmFabricManagerState fm1(n1, type, p1, pFM, bus, desc);

    auto resp0 = makeReceivedResp(NSM_FM_STATE_OFFLINE);
    auto* msg0 = reinterpret_cast<nsm_msg*>(resp0.data());
    fm0.handleResponseMsg(msg0, resp0.size());

    auto resp1 = makeReceivedResp(NSM_FM_STATE_CONFIGURED);
    auto* msg1 = reinterpret_cast<nsm_msg*>(resp1.data());
    fm1.handleResponseMsg(msg1, resp1.size());

    auto agg = fm0.getAggregateFabricManagerState();
    EXPECT_EQ(agg->fabricManagerIntf->fmState(), nsm::FMState::Unknown);
}

// ============================================================================
// getInstance: path mismatch → error logged, same singleton returned
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, GetInstance_PathMismatch)
{
    auto fmIntf0 =
        std::make_shared<nsm::FabricManagerIntf>(bus, "/test/fm_branch/mm_fm0");
    auto opIntf0 =
        std::make_shared<nsm::OperaStatusIntf>(bus, "/test/fm_branch/mm_fm0");
    auto inst1 = nsm::NsmAggregateFabricManagerState::getInstance(
        "/test/fm_branch/mm_pathA", fmIntf0, opIntf0, "desc");
    ASSERT_NE(inst1, nullptr);

    auto fmIntf1 =
        std::make_shared<nsm::FabricManagerIntf>(bus, "/test/fm_branch/mm_fm1");
    auto opIntf1 =
        std::make_shared<nsm::OperaStatusIntf>(bus, "/test/fm_branch/mm_fm1");
    auto inst2 = nsm::NsmAggregateFabricManagerState::getInstance(
        "/test/fm_branch/mm_pathB", fmIntf1, opIntf1, "desc");

    EXPECT_EQ(inst1, inst2);
    EXPECT_EQ(nsm::NsmAggregateFabricManagerState::aggregateFMObjPath,
              "/test/fm_branch/mm_pathA");
}

// ============================================================================
// NsmFabricManagerState: getter methods
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, Getters_ReturnNonNull)
{
    auto fm = makeFM("getters");

    EXPECT_NE(fm.getFabricManagerIntf(), nullptr);
    EXPECT_NE(fm.getOperaStatusIntf(), nullptr);
    EXPECT_NE(fm.getAggregateFabricManagerState(), nullptr);
}

// ============================================================================
// handleResponseMsg: Received with lastRestartTime and duration
// ============================================================================

TEST_F(NsmFabricManagerBranchTest, HandleResp_ReceivedWithTimestamps)
{
    auto fm = makeFM("timestamps");
    auto resp = makeReceivedResp(NSM_FM_STATE_CONFIGURED, 12345, 678);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());

    EXPECT_EQ(fm.handleResponseMsg(msg, resp.size()), NSM_SUCCESS);
    EXPECT_EQ(fm.getFabricManagerIntf()->lastRestartTime(), 12345);
    EXPECT_EQ(fm.getFabricManagerIntf()->lastRestartDuration(), 678);
}
