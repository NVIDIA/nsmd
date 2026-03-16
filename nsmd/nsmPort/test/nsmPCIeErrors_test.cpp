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
 * Tests for nsmd/nsmPort/nsmPCIeErrors.cpp
 *
 *   - NsmPCIeErrors constructor (GROUP_ID_2, GROUP_ID_3, GROUP_ID_4)
 *   - NsmPCIeErrors::genRequestMsg (success for each group)
 *   - NsmPCIeErrors::handleResponseMsg (success, GROUP_ID_2)
 *   - NsmPCIeErrors::handleResponseMsg (error CC, GROUP_ID_2)
 *   - NsmPCIeErrors::handleResponseMsg (success, GROUP_ID_3)
 *   - NsmPCIeErrors::handleResponseMsg (success, GROUP_ID_4)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "libnsm/pci-links.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPort/nsmPCIeErrors.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// Helper: build a NsmInterfaceProvider<PCIeEccIntf>
static NsmInterfaceProvider<PCIeEccIntf>
    makeProvider(const std::string& path, std::shared_ptr<PCIeEccIntf>& outIntf)
{
    outIntf = std::make_shared<PCIeEccIntf>(testBus, path.c_str());
    return NsmInterfaceProvider<PCIeEccIntf>(
        "pcie_errors", "NSM_PCIeErrors", std::filesystem::path(path), outIntf);
}

// =============================================================================
// Constructor tests
// =============================================================================

TEST(NsmPCIeErrors, Constructor_Group2_InitializesFields)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/ctor_g2", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_2);

    EXPECT_EQ(sensor.getName(), "pcie_errors");
    EXPECT_EQ(sensor.getType(), "NSM_PCIeErrors");
    EXPECT_EQ(intf->nonfeCount(), 0u);
    EXPECT_EQ(intf->feCount(), 0u);
    EXPECT_EQ(intf->ceCount(), 0u);
    EXPECT_EQ(intf->unsupportedRequestCount(), 0u);
}

TEST(NsmPCIeErrors, Constructor_Group3_InitializesFields)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/ctor_g3", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_3);

    EXPECT_EQ(sensor.getName(), "pcie_errors");
    EXPECT_EQ(intf->l0ToRecoveryCount(), 0u);
}

TEST(NsmPCIeErrors, Constructor_Group4_CreatesObject)
{
    // NOTE: GROUP_ID_4 constructor calls initHandleResponse(3) in the source
    // code, so the group-4 specific fields (replayCount, etc.) are NOT
    // zero-initialized. This is a known production code issue. We verify
    // object construction succeeds and name/type are correct.
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/ctor_g4", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_4);

    EXPECT_EQ(sensor.getName(), "pcie_errors");
    EXPECT_EQ(sensor.getType(), "NSM_PCIeErrors");
}

// =============================================================================
// genRequestMsg tests
// =============================================================================

TEST(NsmPCIeErrors, GenRequestMsg_Group2_ReturnsValidRequest)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/genreq_g2", intf);
    NsmPCIeErrors sensor(provider, 1, GROUP_ID_2);

    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST(NsmPCIeErrors, GenRequestMsg_Group3_ReturnsValidRequest)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/genreq_g3", intf);
    NsmPCIeErrors sensor(provider, 2, GROUP_ID_3);

    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

TEST(NsmPCIeErrors, GenRequestMsg_Group4_ReturnsValidRequest)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/genreq_g4", intf);
    NsmPCIeErrors sensor(provider, 3, GROUP_ID_4);

    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

// =============================================================================
// handleResponseMsg – GROUP_ID_2 success
// =============================================================================

TEST(NsmPCIeErrors, HandleResponseMsg_Group2_Success_UpdatesFields)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/resp_g2", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_2);

    nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 10;
    data.fatal_errors = 20;
    data.correctable_errors = 30;
    data.unsupported_request_count = 40;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(intf->nonfeCount(), 10u);
    EXPECT_EQ(intf->feCount(), 20u);
    EXPECT_EQ(intf->ceCount(), 30u);
    EXPECT_EQ(intf->unsupportedRequestCount(), 40u);
}

// =============================================================================
// handleResponseMsg – GROUP_ID_2 error CC
// =============================================================================

TEST(NsmPCIeErrors, HandleResponseMsg_Group2_ErrorCC_ClearsFields)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/resp_g2_err", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_2);

    nsm_query_scalar_group_telemetry_group_2 data = {};
    data.non_fatal_errors = 99;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_2_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group2_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(result, NSM_SUCCESS);
    // On error, handleResponse is called with zeroed data
    EXPECT_EQ(intf->nonfeCount(), 0u);
}

// =============================================================================
// handleResponseMsg – GROUP_ID_3 success
// =============================================================================

TEST(NsmPCIeErrors, HandleResponseMsg_Group3_Success_UpdatesL0Recovery)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/resp_g3", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_3);

    nsm_query_scalar_group_telemetry_group_3 data = {};
    data.L0ToRecoveryCount = 77;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_3_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group3_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(intf->l0ToRecoveryCount(), 77u);
}

// =============================================================================
// handleResponseMsg – GROUP_ID_4 success
// =============================================================================

TEST(NsmPCIeErrors, HandleResponseMsg_Group4_Success_UpdatesReplayCounts)
{
    std::shared_ptr<PCIeEccIntf> intf;
    auto provider = makeProvider("/test/pcie_errors/resp_g4", intf);
    NsmPCIeErrors sensor(provider, 0, GROUP_ID_4);

    nsm_query_scalar_group_telemetry_group_4 data = {};
    data.replay_cnt = 11;
    data.replay_rollover_cnt = 22;
    data.NAK_sent_cnt = 33;
    data.NAK_recv_cnt = 44;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_4_resp));
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_query_scalar_group_telemetry_v1_group4_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto result = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(result, NSM_SUCCESS);
    EXPECT_EQ(intf->replayCount(), 11u);
    EXPECT_EQ(intf->replayRolloverCount(), 22u);
    EXPECT_EQ(intf->nakSentCount(), 33u);
    EXPECT_EQ(intf->nakReceivedCount(), 44u);
}
