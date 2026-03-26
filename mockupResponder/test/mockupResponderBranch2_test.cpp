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
// Test fixture
// =============================================================================

class MockupResponderBranch2Test : public Test
{
  protected:
    MockupResponderBranch2Test()
    {
        init(31, NSM_DEV_ID_GPU, 3);
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

// Fixture for Switch device type
class MockupResponderBranch2SwitchTest : public Test
{
  protected:
    MockupResponderBranch2SwitchTest()
    {
        instanceId = 0;
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            false, event, *objServer, 32, NSM_DEV_ID_SWITCH, 1);
    }

    uint8_t instanceId;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;
};

// Fixture for non-verbose mode
class MockupResponderBranch2QuietTest : public Test
{
  protected:
    MockupResponderBranch2QuietTest()
    {
        instanceId = 0;
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            false, event, *objServer, 33, NSM_DEV_ID_GPU, 2);
    }

    uint8_t instanceId;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;
};

// =============================================================================
// Helper to build a request via processRxMsg
// =============================================================================

static std::vector<uint8_t> makeRequest(uint8_t instanceId, uint8_t nsmType,
                                        uint8_t command, size_t payloadSize = 0)
{
    std::vector<uint8_t> request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) + payloadSize, 0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = nsmType;
    pack_nsm_header(&info, &msg->hdr);
    msg->payload[0] = command;
    return request;
}

// =============================================================================
// processRxMsg - dispatch to type0 handlers not tested in existing tests
// =============================================================================

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetEventSubscription)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_event_subscription_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetEventSubscription)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_event_subscription_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(
        instanceId, GLOBAL_EVENT_GENERATION_ENABLE_POLLING, 10, requestMsg);
    ASSERT_EQ(rc, NSM_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetSupportedEventSources)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_supported_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    ASSERT_EQ(rc, NSM_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetCurrentEventSources)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_current_event_source_req(
        instanceId, NSM_TYPE_NETWORK_PORT, requestMsg);
    ASSERT_EQ(rc, NSM_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetCurrentEventSources)
{
    bitfield8_t sources[EVENT_SOURCES_LENGTH] = {};
    sources[0].byte = 0x03;

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_current_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_current_event_sources_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, sources, requestMsg);
    ASSERT_EQ(rc, NSM_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgConfigureEventAcknowledgement)
{
    bitfield8_t mask[EVENT_SOURCES_LENGTH] = {};
    mask[0].byte = 0x01;

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_configure_event_acknowledgement_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_configure_event_acknowledgement_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, mask, requestMsg);
    ASSERT_EQ(rc, NSM_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// processRxMsg - type1 additional dispatch
// =============================================================================

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetPortDisableFuture)
{
    bitfield8_t portMask[PORT_MASK_DATA_SIZE] = {};
    portMask[0].byte = 0xFF;

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_port_disable_future_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_disable_future_req(instanceId, portMask,
                                                 requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetPortDisableFuture)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_disable_future_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetPowerMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_mode_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetPowerMode)
{
    struct nsm_power_mode_data powerData = {};

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_power_mode_req(instanceId, requestMsg, powerData);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetSwitchIsolationMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_switch_isolation_mode_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetSwitchIsolationMode)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_switch_isolation_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_switch_isolation_mode_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// processRxMsg - type3 additional dispatch
// =============================================================================

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetEnergyCount)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_energy_count_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_energy_count_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetEnergyCountAggregate)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_energy_count_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_energy_count_req(instanceId, 255, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetVoltage)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetVoltageAggregate)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, 255, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetAltitudePressure)
{
    auto request = makeRequest(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                               NSM_GET_ALTITUDE_PRESSURE);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetEdppScalingFactor)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_programmable_EDPp_scaling_factor_req(instanceId,
                                                              requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetEdppScalingFactor)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_programmable_EDPp_scaling_factor_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_programmable_EDPp_scaling_factor_req(instanceId, 0, 0,
                                                              50, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetClockLimit)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_limit_req(instanceId, GRAPHICS_CLOCK,
                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetClockLimit)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_clock_limit_req(instanceId, GRAPHICS_CLOCK, 0, 100,
                                         2000, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetCurrentClockFrequency)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_curr_clock_freq_req(instanceId, GRAPHICS_CLOCK,
                                             requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetProcessorThrottleReason)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                             requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetAccumGpuUtilTime)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_accum_GPU_util_time_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetPowerLimit)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_limit_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgSetPowerLimit)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_power_limit_req(instanceId, 0, 0, 0, 10000,
                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetClockOutputEnableState)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, PCIE_CLKBUF_INDEX, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetRowRemapState)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_state_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetRowRemappingCounts)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remapping_counts_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ProcessRxMsgGetRowRemapAvailability)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_availability_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getClockLimitHandler - invalid clock_id branch
// =============================================================================

TEST_F(MockupResponderBranch2Test, GetClockLimitInvalidClockId)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Use a valid encode but override clock_id to invalid value
    auto rc = encode_get_clock_limit_req(instanceId, GRAPHICS_CLOCK,
                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Override clock_id field to an invalid value
    auto payload =
        reinterpret_cast<nsm_get_clock_limit_req*>(requestMsg->payload);
    payload->clock_id = 0xFF; // invalid

    auto resp = mockupResponder->getClockLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetClockLimitMemoryClock)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_limit_req(instanceId, MEMORY_CLOCK, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getClockLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getCurrClockFreqHandler - invalid clock_id branch
// =============================================================================

TEST_F(MockupResponderBranch2Test, GetCurrClockFreqInvalidClockId)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_curr_clock_freq_req(instanceId, GRAPHICS_CLOCK,
                                             requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    auto payload =
        reinterpret_cast<nsm_get_curr_clock_freq_req*>(requestMsg->payload);
    payload->clock_id = 0xFF;

    auto resp = mockupResponder->getCurrClockFreqHandler(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetCurrClockFreqMemoryClock)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_curr_clock_freq_req(instanceId, MEMORY_CLOCK,
                                             requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getCurrClockFreqHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getClockOutputEnableStateHandler - invalid index branch
// =============================================================================

TEST_F(MockupResponderBranch2Test, GetClockOutputEnableStateInvalidIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, PCIE_CLKBUF_INDEX, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    auto payload = reinterpret_cast<nsm_get_clock_output_enabled_state_req*>(
        requestMsg->payload);
    payload->index = 0xFF; // invalid

    auto resp = mockupResponder->getClockOutputEnableStateHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetClockOutputEnableStateNvhs)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, NVHS_CLKBUF_INDEX, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getClockOutputEnableStateHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetClockOutputEnableStateIblink)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, IBLINK_CLKBUF_INDEX, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getClockOutputEnableStateHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// enableDisableGpuIstModeHandler - device_index branches
// =============================================================================

TEST_F(MockupResponderBranch2Test, GpuIstModeSingleGpu)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_disable_gpu_ist_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 0, 1,
                                                     requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GpuIstModeAllGpus)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_disable_gpu_ist_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(
        instanceId, ALL_GPUS_DEVICE_INDEX, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GpuIstModeInvalidIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_disable_gpu_ist_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 0, 1,
                                                     requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Override device_index to invalid
    auto payload = reinterpret_cast<nsm_enable_disable_gpu_ist_mode_req*>(
        requestMsg->payload);
    payload->device_index = 200; // not < 8 and not ALL_GPUS

    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GpuIstModeDisable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_disable_gpu_ist_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 3, 0,
                                                     requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getFpgaDiagnosticsSettingsHandler - different data_index branches
// =============================================================================

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsGetWpSettings)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_WP_SETTINGS, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsGetWpJumperPresence)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_WP_JUMPER_PRESENCE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsGetPowerSupplyStatus)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_POWER_SUPPLY_STATUS, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsGetGpuPresence)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_GPU_PRESENCE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsGetGpuPowerStatus)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_GPU_POWER_STATUS, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsGetGpuIstModeSettings)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_fpga_diagnostics_settings_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_GPU_IST_MODE_SETTINGS, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, FpgaDiagnosticsSettingsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// setupAdminOverride - parameter_id == 0 vs else
// =============================================================================

TEST_F(MockupResponderBranch2Test, SetupAdminOverrideParamId0)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_setup_admin_override_req(instanceId, 0, 12345, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setupAdminOverride(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetupAdminOverrideParamIdNon0)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_setup_admin_override_req(instanceId, 1, 12345, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setupAdminOverride(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetupAdminOverrideBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setupAdminOverride(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// updatePresetProfileParams - parameter_id == 0 vs else
// =============================================================================

TEST_F(MockupResponderBranch2Test, UpdatePresetProfileParamsParamId0)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_update_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_update_preset_profile_param_req(instanceId, 0, 0, 12345,
                                                     requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->updatePresetProfileParams(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, UpdatePresetProfileParamsParamIdNon0)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_update_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_update_preset_profile_param_req(instanceId, 0, 1, 12345,
                                                     requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->updatePresetProfileParams(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, UpdatePresetProfileParamsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->updatePresetProfileParams(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// Handler bad-decode tests (error branches not in existing tests)
// =============================================================================

TEST_F(MockupResponderBranch2Test, ApplyAdminOverrideBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->applyAdminOverride(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ToggleImmediateRampDownBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->toggleImmediateRampDown(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ToggleFeatureStateBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->toggleFeatureState(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPresetProfileInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPresetProfileInfo(requestMsg,
                                                      request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetActivePresetProfileBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setActivePresetProfile(requestMsg,
                                                        request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetCurrentProfileInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getCurrentProfileInfo(requestMsg,
                                                       request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetQueryAdminOverrideBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getQueryAdminOverride(requestMsg,
                                                       request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPowerSmoothingFeatureInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPowerSmoothingFeatureInfo(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetHwCircuiteryUsageBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getHwCircuiteryUsage(requestMsg,
                                                      request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPowerSmoothingFeatureInfoV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPowerSmoothingFeatureInfoV2(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetCurrentProfileInfoV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getCurrentProfileInfoV2(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetQueryAdminOverrideV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getQueryAdminOverrideV2(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPresetProfileInfoV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPresetProfileInfoV2(requestMsg,
                                                        request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetSupportedGPMMetricsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getSupportedGPMMetrics(requestMsg,
                                                        request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, EnableWorkloadPowerProfileBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->enableWorkloadPowerProfile(requestMsg,
                                                            request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, DisableWorkloadPowerProfileBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->disableWorkloadPowerProfile(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetWorkLoadProfileStatusInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getWorkLoadProfileStatusInfo(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetWorkloadPowerProfileInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getWorkloadPowerProfileInfo(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetHistogramFormatBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getHistogramFormatHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetHistogramDataBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getHistogramDataHandler(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetDeviceCapabilitiesV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDeviceCapabilitiesV2Handler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetGpioStateBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getGpioStateHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetEthPortTelemetryCounterBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getEthPortTelemetryCounterHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPortNetworkAddressesBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPortNetworkAddressesHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPortEccCountersBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPortEccCountersHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetDeviceDiagnosticsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDeviceDiagnosticsHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetNetworkDeviceDebugInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, EraseTraceBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->eraseTraceHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetNetworkDeviceLogInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getNetworkDeviceLogInfoHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, EraseDebugInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->eraseDebugInfoHandler(requestMsg,
                                                       request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetPciePortConfigBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetPciePortConfigBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ResetNetworkDeviceBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetLeakDetectionInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getLeakDetectionInfoHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, QueryPerInstanceGPMMetricsV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryPerInstanceGPMMetricsV2(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetErrorInjectionModeV1BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setErrorInjectionModeV1Handler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetErrorInjectionModeV1BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getErrorInjectionModeV1Handler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetSupportedErrorInjectionTypesV1BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getSupportedErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetCurrentErrorInjectionTypesV1BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetCurrentErrorInjectionTypesV1BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetErrorInjectionPayloadBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getErrorInjectionPayloadHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetErrorInjectionPayloadBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setErrorInjectionPayloadHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ActivateErrorInjectionBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->activateErrorInjectionHandler(requestMsg,
                                                               request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetListAvailablePciePortsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getListAvailablePciePortsHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// Long-running handlers - non-long-running path (isLongRunning=false)
// =============================================================================

TEST_F(MockupResponderBranch2Test, GetMigModeNotLongRunning)
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
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetMigModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->getMigModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetMigModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->setMigModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetMigModeNotLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_MIG_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_MIG_mode_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->setMigModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetEccModeNotLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_ECC_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getEccModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetEccModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->getEccModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetEccModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->setEccModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetEccModeNotLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_ECC_mode_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->setEccModeHandler(requestMsg, request.size(),
                                                   false, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetCurrentUtilizationNotLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_CURRENT_UTILIZATION, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getCurrentUtilizationHandler(
        requestMsg, request.size(), false, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetCurrentUtilizationBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->getCurrentUtilizationHandler(
        requestMsg, request.size(), false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetMemoryCapacityUtilNotLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getMemoryCapacityUtilHandler(
        requestMsg, request.size(), false, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetMemoryCapacityUtilBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->getMemoryCapacityUtilHandler(
        requestMsg, request.size(), false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetViolationDurationNotLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, request.size(), false, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_FALSE(longRunning.has_value());
}

TEST_F(MockupResponderBranch2Test, GetViolationDurationBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, request.size(), false, longRunning);
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// getReconfigurationPermissionsV1 - invalid settings index
// =============================================================================

TEST_F(MockupResponderBranch2Test, GetReconfigurationPermissionsInvalidIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Override settings index to invalid
    auto payload =
        reinterpret_cast<nsm_get_reconfiguration_permissions_v1_req*>(
            requestMsg->payload);
    payload->setting_index =
        static_cast<reconfiguration_permissions_v1_index>(0xFF);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// setReconfigurationPermissionsV1 - different configuration branches
// =============================================================================

TEST_F(MockupResponderBranch2Test, SetReconfigurationPermissionsOneShotHotReset)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0x03, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetReconfigurationPermissionsPersistent)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_PERSISTENT, 0x03, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetReconfigurationPermissionsOneShotFLR)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOT_FLR, 0x03, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetReconfigurationPermissionsInvalidConfig)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0x01, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Override configuration to invalid
    auto payload =
        reinterpret_cast<nsm_set_reconfiguration_permissions_v1_req*>(
            requestMsg->payload);
    payload->configuration =
        static_cast<reconfiguration_permissions_v1_setting>(0xFF);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, SetReconfigurationPermissionsInvalidIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0x01, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    auto payload =
        reinterpret_cast<nsm_set_reconfiguration_permissions_v1_req*>(
            requestMsg->payload);
    payload->setting_index =
        static_cast<reconfiguration_permissions_v1_index>(0xFF);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// Scalar group telemetry - different group IDs
// =============================================================================

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup0)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_0);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup1)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_1);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup2)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_2);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup3)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_3);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup4)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_4);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup5)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_5);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup6)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_6);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup8)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_8);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryGroup9)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_9);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, ScalarGroupTelemetryInvalidGroup)
{
    auto resp =
        mockupResponder->getQueryScalarGroupTelemetryResponse(instanceId, 0xFF);
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// queryScalarGroupTelemetryHandler / multiport - bad decode
// =============================================================================

TEST_F(MockupResponderBranch2Test, QueryScalarGroupTelemetryBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryScalarGroupTelemetryHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, QueryMultiportScalarGroupTelemetryBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryMultiportScalarGroupTelemetryHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// queryAvailableAndClearableScalarGroupHandler - different groups
// =============================================================================

TEST_F(MockupResponderBranch2Test, AvailableClearableGroup3)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_3, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, AvailableClearableGroup4)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_4, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, AvailableClearableGroup8)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_8, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, AvailableClearableGroup9)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_9, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, AvailableClearableGroupDefault)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// getWorkloadPowerProfileInfo - last page vs not
// =============================================================================

TEST_F(MockupResponderBranch2Test, GetWorkloadPowerProfileInfoLastPage)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_workload_power_profile_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, 3,
                                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getWorkloadPowerProfileInfo(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, GetWorkloadPowerProfileInfoNotLastPage)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_workload_power_profile_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, 0,
                                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getWorkloadPowerProfileInfo(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Non-verbose mode tests to cover the other side of verbose checks
// =============================================================================

TEST_F(MockupResponderBranch2QuietTest, ProcessRxMsgPingQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2QuietTest, ProcessRxMsgEventAckQuiet)
{
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 1, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_EVENT_ACKNOWLEDGMENT;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    pack_nsm_header(&info, hdr);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// getSupportCommandCodeHandler - various device types
// =============================================================================

TEST_F(MockupResponderBranch2SwitchTest, GetSupportedCommandCodesSwitch)
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
// enableDisableWriteProtectedHandler - various data_index branches
// =============================================================================

TEST_F(MockupResponderBranch2Test, WriteProtectedBaseboardFruEeprom)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, BASEBOARD_FRU_EEPROM, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedHmcSpiFlash)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, HMC_SPI_FLASH, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedPexSwEeprom)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, PEX_SW_EEPROM, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedRetimerEeprom)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedNvswEepromBoth)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, NVSW_EEPROM_BOTH, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedNvswEeprom1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, NVSW_EEPROM_1, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedNvswEeprom2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, NVSW_EEPROM_2, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedGpu14SpiFlash)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_1_4_SPI_FLASH, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedGpu58SpiFlash)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_5_8_SPI_FLASH, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedGpuSpiFlash)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedGpuSpiFlash1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_1, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedRetimerEeprom1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_1, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch2Test, WriteProtectedCx8SpiFlash)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, CX8_SPI_FLASH, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}
