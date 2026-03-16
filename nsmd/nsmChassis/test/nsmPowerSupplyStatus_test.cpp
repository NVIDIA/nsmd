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
 * Tests for nsmd/nsmChassis/nsmPowerSupplyStatus.cpp
 *
 *   - NsmPowerSupplyStatus::NsmPowerSupplyStatus (constructor)
 *   - NsmPowerSupplyStatus::genRequestMsg (success)
 *   - NsmPowerSupplyStatus::handleResponse (success, bit set → On)
 *   - NsmPowerSupplyStatus::handleResponse (success, bit clear → Off)
 *   - NsmPowerSupplyStatus::handleResponse (error CC → Unknown)
 */

#include "test/mockDBusHandler.hpp"

using namespace ::testing;

#include "base.h"
#include "device-configuration.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmPowerSupplyStatus.hpp"

using namespace nsm;

static auto& testBus = utils::DBusHandler::getBus();

static NsmInterfaceProvider<PowerStateIntf>
    makeProvider(const std::string& path,
                 std::shared_ptr<PowerStateIntf>& outIntf)
{
    outIntf = std::make_shared<PowerStateIntf>(testBus, path.c_str());
    return NsmInterfaceProvider<PowerStateIntf>(
        "psu_status", "NSM_PowerSupplyStatus", std::filesystem::path(path),
        outIntf);
}

// =============================================================================
// Constructor
// =============================================================================

TEST(NsmPowerSupplyStatus, Constructor_CreatesObject)
{
    std::shared_ptr<PowerStateIntf> powerIntf;
    auto provider = makeProvider("/test/psu_status/ctor", powerIntf);
    NsmPowerSupplyStatus sensor(provider, 0);

    EXPECT_EQ(sensor.getName(), "psu_status");
    EXPECT_EQ(sensor.getType(), "NSM_PowerSupplyStatus");
    EXPECT_EQ(sensor.gpuInstanceId, 0);
}

// =============================================================================
// genRequestMsg
// =============================================================================

TEST(NsmPowerSupplyStatus, GenRequestMsg_ReturnsValidRequest)
{
    std::shared_ptr<PowerStateIntf> powerIntf;
    auto provider = makeProvider("/test/psu_status/genreq", powerIntf);
    NsmPowerSupplyStatus sensor(provider, 0);

    auto request = sensor.genRequestMsg(5, 2);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto msg = reinterpret_cast<const nsm_msg*>(request->data());
    auto cmd = reinterpret_cast<const nsm_get_fpga_diagnostics_settings_req*>(
        msg->payload);
    EXPECT_EQ(cmd->data_index, GET_POWER_SUPPLY_STATUS);
}

// =============================================================================
// handleResponse – success, bit set → On
// =============================================================================

TEST(NsmPowerSupplyStatus, HandleResponse_BitSet_SetsPowerStateOn)
{
    std::shared_ptr<PowerStateIntf> powerIntf;
    auto provider = makeProvider("/test/psu_status/bit_set", powerIntf);
    // gpuInstanceId = 0, so bit 0 of status byte
    NsmPowerSupplyStatus sensor(provider, 0);

    uint8_t status = 0x01; // bit 0 set → GPU instance 0 is On
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_supply_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  status, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponse(responseMsg, responseData.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerIntf->currentPowerState(), PowerStateIntf::PowerState::On);
}

// =============================================================================
// handleResponse – success, bit clear → Off
// =============================================================================

TEST(NsmPowerSupplyStatus, HandleResponse_BitClear_SetsPowerStateOff)
{
    std::shared_ptr<PowerStateIntf> powerIntf;
    auto provider = makeProvider("/test/psu_status/bit_clear", powerIntf);
    // gpuInstanceId = 1, bit 1 is NOT set in status 0x01
    NsmPowerSupplyStatus sensor(provider, 1);

    uint8_t status = 0x01; // bit 1 not set
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_supply_status_resp(0, NSM_SUCCESS, ERR_NULL,
                                                  status, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponse(responseMsg, responseData.size());

    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(powerIntf->currentPowerState(), PowerStateIntf::PowerState::Off);
}

// =============================================================================
// handleResponse – error CC → Unknown
// =============================================================================

TEST(NsmPowerSupplyStatus, HandleResponse_ErrorCC_SetsPowerStateUnknown)
{
    std::shared_ptr<PowerStateIntf> powerIntf;
    auto provider = makeProvider("/test/psu_status/error", powerIntf);
    NsmPowerSupplyStatus sensor(provider, 0);

    uint8_t status = 0xFF;
    std::vector<uint8_t> responseData(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp), 0);
    auto responseMsg = reinterpret_cast<nsm_msg*>(responseData.data());
    auto rc = encode_get_power_supply_status_resp(0, NSM_ERROR, ERR_NULL,
                                                  status, responseMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = sensor.handleResponse(responseMsg, responseData.size());

    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_EQ(powerIntf->currentPowerState(),
              PowerStateIntf::PowerState::Unknown);
}
