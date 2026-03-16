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
 * Tests for nsmd/nsmChassis/nsmWriteProtectedJumper.cpp
 *
 *   - NsmWriteProtectedJumper::NsmWriteProtectedJumper (constructor)
 *   - NsmWriteProtectedJumper::genRequestMsg (success)
 *   - NsmWriteProtectedJumper::handleResponseMsg (success, presence=1)
 *   - NsmWriteProtectedJumper::handleResponseMsg (success, presence=0)
 *   - NsmWriteProtectedJumper::handleResponseMsg (error CC)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmWriteProtectedJumper.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

static NsmInterfaceProvider<SettingsIntf>
    makeProvider(const std::string& path,
                 std::shared_ptr<SettingsIntf>& outIntf)
{
    outIntf = std::make_shared<SettingsIntf>(testBus, path.c_str());
    return NsmInterfaceProvider<SettingsIntf>(
        "wp_jumper", "NSM_WriteProtectedJumper", std::filesystem::path(path),
        outIntf);
}

// =============================================================================
// Constructor
// =============================================================================

TEST(NsmWriteProtectedJumper, Constructor_CreatesObject)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeProvider("/test/wp_jumper/ctor", settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    EXPECT_EQ(sensor.getName(), "wp_jumper");
    EXPECT_EQ(sensor.getType(), "NSM_WriteProtectedJumper");
}

// =============================================================================
// genRequestMsg
// =============================================================================

TEST(NsmWriteProtectedJumper, GenRequestMsg_ReturnsValidRequest)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeProvider("/test/wp_jumper/genreq", settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    auto request = sensor.genRequestMsg(5, 0);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto cmd = reinterpret_cast<const nsm_get_fpga_diagnostics_settings_req*>(
        msg->payload);
    EXPECT_EQ(cmd->data_index, GET_WP_JUMPER_PRESENCE);
}

// =============================================================================
// handleResponseMsg – success paths
// =============================================================================

TEST(NsmWriteProtectedJumper, HandleResponseMsg_PresenceOne_SetsWriteProtected)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeProvider("/test/wp_jumper/presence1", settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    data.presence = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(settingsIntf->writeProtected(), true);
    EXPECT_EQ(settingsIntf->writeProtectedControl(), true);
}

TEST(NsmWriteProtectedJumper,
     HandleResponseMsg_PresenceZero_ClearsWriteProtected)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeProvider("/test/wp_jumper/presence0", settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    data.presence = 0;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_SUCCESS, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(settingsIntf->writeProtected(), false);
    EXPECT_EQ(settingsIntf->writeProtectedControl(), false);
}

// =============================================================================
// handleResponseMsg – error CC
// =============================================================================

TEST(NsmWriteProtectedJumper, HandleResponseMsg_ErrorCC_ReturnsError)
{
    std::shared_ptr<SettingsIntf> settingsIntf;
    auto provider = makeProvider("/test/wp_jumper/error", settingsIntf);
    NsmWriteProtectedJumper sensor(provider);

    nsm_fpga_diagnostics_settings_wp_jumper data = {};
    data.presence = 1;

    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp),
        0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_fpga_diagnostics_settings_wp_jumper_resp(
        0, NSM_ERROR, ERR_NULL, &data, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponseMsg(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SUCCESS);
}
