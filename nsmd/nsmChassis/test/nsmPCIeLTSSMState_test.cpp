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
 * Tests for nsmd/nsmChassis/nsmPCIeLTSSMState.cpp
 *
 *   - NsmPCIeLTSSMState constructor
 *   - NsmPCIeLTSSMState::genRequestMsg (success)
 *   - NsmPCIeLTSSMState::handleResponseMsg (success – various LTSSM states)
 *   - NsmPCIeLTSSMState::handleResponseMsg (state 0xFF → IllegalState)
 *   - NsmPCIeLTSSMState::handleResponseMsg (error CC → NA state)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "libnsm/pci-links.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmChassis/nsmPCIeLTSSMState.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

// Helper: build a NsmInterfaceProvider<LTSSMStateIntf>
static NsmInterfaceProvider<LTSSMStateIntf>
    makeProvider(const std::string& path,
                 std::shared_ptr<LTSSMStateIntf>& outIntf)
{
    outIntf = std::make_shared<LTSSMStateIntf>(testBus, path.c_str());
    return NsmInterfaceProvider<LTSSMStateIntf>(
        "ltssm_state", "NSM_LTSSMState", std::filesystem::path(path), outIntf);
}

// Helper: build a valid group-6 response message
static std::vector<uint8_t> buildGroup6Response(uint8_t cc,
                                                uint8_t ltssm_state_val)
{
    nsm_query_scalar_group_telemetry_group_6 data = {};
    data.ltssm_state = ltssm_state_val;

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_scalar_group_telemetry_v1_group_6_resp));
    auto msg = reinterpret_cast<nsm_msg*>(buf.data());
    encode_query_scalar_group_telemetry_v1_group6_resp(0, cc, ERR_NULL, &data,
                                                       msg);
    return buf;
}

// =============================================================================
// Constructor test
// =============================================================================

TEST(NsmPCIeLTSSMState, Constructor_InitializesNameAndType)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/ctor", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    EXPECT_EQ(sensor.getName(), "ltssm_state");
    EXPECT_EQ(sensor.getType(), "NSM_LTSSMState");
}

// =============================================================================
// genRequestMsg tests
// =============================================================================

TEST(NsmPCIeLTSSMState, GenRequestMsg_ReturnsValidRequest)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/genreq", intf);
    NsmPCIeLTSSMState sensor(provider, 2);

    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_scalar_group_telemetry_v1_req));
}

// =============================================================================
// handleResponseMsg – success with various LTSSM states
// =============================================================================

TEST(NsmPCIeLTSSMState, HandleResponseMsg_StateDetect_SetsDetect)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/detect", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildGroup6Response(NSM_SUCCESS, 0x0); // Detect
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::Detect);
}

TEST(NsmPCIeLTSSMState, HandleResponseMsg_StateL0_SetsL0)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/l0", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildGroup6Response(NSM_SUCCESS, 0x5); // L0
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::L0);
}

TEST(NsmPCIeLTSSMState, HandleResponseMsg_IllegalState_SetsIllegalState)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/illegal", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildGroup6Response(NSM_SUCCESS, 0xFF); // IllegalState
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::IllegalState);
}

TEST(NsmPCIeLTSSMState, HandleResponseMsg_ErrorCC_SetsNA)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/err", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildGroup6Response(NSM_ERROR, 0x5);
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::NA);
}

TEST(NsmPCIeLTSSMState, HandleResponseMsg_StateRecovery_SetsRecovery)
{
    std::shared_ptr<LTSSMStateIntf> intf;
    auto provider = makeProvider("/test/ltssm/recovery", intf);
    NsmPCIeLTSSMState sensor(provider, 0);

    auto buf = buildGroup6Response(NSM_SUCCESS, 0x3); // Recovery
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = sensor.handleResponseMsg(msg, buf.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(intf->ltssmState(), LTSSMStateIntf::State::Recovery);
}
