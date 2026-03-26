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

#include "base.h"
#include "debug-token.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "firmware-utils.h"
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "common/event.hpp"
#include "utils.hpp"

#include <cstdio>
#include <fstream>
#include <functional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Test;

#define private public
#define protected public
#include "mockupResponder.hpp"

// =============================================================================
// Test fixture - verbose GPU
// =============================================================================

class MockupResponderBranch4Test : public Test
{
  protected:
    MockupResponderBranch4Test()
    {
        init(41, NSM_DEV_ID_GPU, 4);
    }

    uint8_t instanceId = 0;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;

    void init(eid_t eid, uint8_t deviceType, uint8_t instId)
    {
        this->instanceId = instId;
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            true, event, *objServer, eid, deviceType, instId);
    }
};

// Fixture for non-verbose GPU
class MockupResponderBranch4QuietTest : public Test
{
  protected:
    MockupResponderBranch4QuietTest()
    {
        instanceId = 0;
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            false, event, *objServer, 43, NSM_DEV_ID_GPU, 5);
    }

    uint8_t instanceId;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;
};

// Fixture for Switch device type
class MockupResponderBranch4SwitchTest : public Test
{
  protected:
    MockupResponderBranch4SwitchTest()
    {
        instanceId = 0;
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            true, event, *objServer, 44, NSM_DEV_ID_SWITCH, 0);
    }

    uint8_t instanceId;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;
};

// =============================================================================
// getHistogramFormatHandler - verbose mode
// =============================================================================
TEST_F(MockupResponderBranch4Test, GetHistogramFormatVerbose)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_format_req(instanceId, 0, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getHistogramFormatHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

// getHistogramDataHandler - verbose mode
TEST_F(MockupResponderBranch4Test, GetHistogramDataVerbose)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_data_req(instanceId, 0, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getHistogramDataHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
}

// getGpioStateHandler - verbose mode
TEST_F(MockupResponderBranch4Test, GetGpioStateVerbose)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_gpio_state_req(instanceId, 0, 16, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getGpioStateHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

// getDeviceCapabilitiesV2Handler - verbose mode
TEST_F(MockupResponderBranch4Test, GetDeviceCapabilitiesV2Verbose)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_capabilities_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_device_capabilities_v2_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceCapabilitiesV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// getListAvailablePciePortsHandler - verbose mode
TEST_F(MockupResponderBranch4Test, GetListAvailablePciePortsVerbose)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_list_available_pcie_ports_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getListAvailablePciePortsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// resetNetworkDeviceHandler - verbose mode
TEST_F(MockupResponderBranch4Test, ResetNetworkDeviceVerbose)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_reset_network_device_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

// getEgmModeHandler - verbose mode
TEST_F(MockupResponderBranch4Test, GetEgmModeVerbose)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_EGM_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getEgmModeHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Non-verbose mode tests (hits the verbose=false branches)
// =============================================================================

TEST_F(MockupResponderBranch4QuietTest, GetHistogramFormatQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_format_req(instanceId, 0, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getHistogramFormatHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetHistogramDataQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_data_req(instanceId, 0, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getHistogramDataHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetGpioStateQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_gpio_state_req(instanceId, 0, 8, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getGpioStateHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetDeviceCapabilitiesV2Quiet)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_capabilities_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_device_capabilities_v2_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceCapabilitiesV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetListAvailablePciePortsQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_list_available_pcie_ports_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getListAvailablePciePortsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, ResetNetworkDeviceQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_reset_network_device_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetEgmModeQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_EGM_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getEgmModeHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetMigModeVerboseFalse)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_MIG_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getMigModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, GetDeviceDebugParametersQuiet)
{
    struct nsm_debug_parameter_id paramId = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MLPC};
    nsm_debug_parameter_sub_id_bitfield subId = {.value = 0};

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, paramId,
                                                     subId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4QuietTest, SetDeviceDebugParametersQuiet)
{
    struct nsm_debug_parameter_id paramId = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MLPC};
    nsm_debug_parameter_sub_id_bitfield subId = {.value = 0};
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    uint8_t dataSize = data.size();

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_debug_parameters_req) - 1 + dataSize);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_debug_parameters_req(
        instanceId, 0, paramId, subId, dataSize, data.data(), requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Switch device type - triggers different behavior in some handlers
// =============================================================================

TEST_F(MockupResponderBranch4SwitchTest, PingSwitchDevice)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->pingHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4SwitchTest, GetSupportedMessageTypesSwitch)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_nvidia_message_types_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_nvidia_message_types_req(instanceId,
                                                            requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getSupportNvidiaMessageTypesHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch4SwitchTest, GetSupportedCommandCodesSwitch)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getSupportCommandCodeHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Bad decode tests for handlers not covered elsewhere
// =============================================================================

TEST_F(MockupResponderBranch4Test, GetHistogramFormatBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getHistogramFormatHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetHistogramDataBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getHistogramDataHandler(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetGpioStateBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getGpioStateHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetDeviceCapabilitiesV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDeviceCapabilitiesV2Handler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetListAvailablePciePortsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getListAvailablePciePortsHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, ResetNetworkDeviceBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetLeakDetectionInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getLeakDetectionInfoHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetEthPortTelemetryCounterBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getEthPortTelemetryCounterHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetPortNetworkAddressesBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPortNetworkAddressesHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetPortEccCountersBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPortEccCountersHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetPciePortConfigBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, SetPciePortConfigBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, GetFpgaDiagnosticsSettingsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test,
       DISABLED_EnableDisableWriteProtectedBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_EnableDisableGpuIstModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_GetConfidentialComputeModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getConfidentialComputeModeHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_SetConfidentialComputeModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setConfidentialComputeModeHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_GetReconfigPermsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_SetReconfigPermsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_GetDevicemodeSettingsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_SetDevicemodeSettingsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_GetDeviceDiagnosticsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDeviceDiagnosticsHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_GetNetworkDeviceDebugInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_EraseTraceBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->eraseTraceHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_GetNetworkDeviceLogInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getNetworkDeviceLogInfoHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch4Test, DISABLED_EraseDebugInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->eraseDebugInfoHandler(requestMsg,
                                                       request.size());
    EXPECT_FALSE(resp.has_value());
}
