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
// Test fixture matching the existing pattern
// =============================================================================

class MockupResponderBranchTest : public Test
{
  protected:
    MockupResponderBranchTest()
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

// =============================================================================
// processRxMsg branch coverage
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgInvalidHeader)
{
    // Zero-length or invalid header should return nullopt
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr), 0);
    // Corrupt the header so unpack_nsm_header fails
    memset(rxMsg.data(), 0, rxMsg.size());
    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    // unpack_nsm_header with zeroed header may fail
    // Result depends on implementation, but should not crash
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgEventAcknowledgement)
{
    // Build a valid NSM message with type = EVENT_ACKNOWLEDGMENT
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 1, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_EVENT_ACKNOWLEDGMENT;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    // Event ack returns nullopt
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgUnsupportedMsgType)
{
    // Build a message with unsupported nvidia_msg_type
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = 0x7F; // unsupported type
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0x00; // command code

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    // Should return unsupported command response
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType0UnsupportedCommand)
{
    // Type 0 with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE; // unsupported command

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType1UnsupportedCommand)
{
    // Type 1 (Network Port) with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_NETWORK_PORT;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE;

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType2UnsupportedCommand)
{
    // Type 2 (PCI Link) with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_PCI_LINK;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE;

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType3UnsupportedCommand)
{
    // Type 3 (Platform Environmental) with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE;

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType4UnsupportedCommand)
{
    // Type 4 (Diagnostic) with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_DIAGNOSTIC;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE;

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType5UnsupportedCommand)
{
    // Type 5 (Device Configuration) with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_DEVICE_CONFIGURATION;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE;

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgType6UnsupportedCommand)
{
    // Type 6 (Firmware) with unsupported command code
    std::vector<uint8_t> rxMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto hdr = reinterpret_cast<nsm_msg_hdr*>(rxMsg.data());
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_FIRMWARE;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, hdr);
    auto msg = reinterpret_cast<nsm_msg*>(rxMsg.data());
    msg->payload[0] = 0xFE;

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(rxMsg, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// processRxMsg dispatching to known handlers
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgPingHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_ping_req(instanceId, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetSupportedMessageTypes)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_get_supported_nvidia_message_types_req(instanceId, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetSupportedCommandCodes)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_supported_command_codes_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgQueryDeviceIdentification)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_device_identification_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_nsm_query_device_identification_req(instanceId, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Type 1 (Network Port) handler dispatch via processRxMsg
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetPortTelemetryCounter)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_port_telemetry_counter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_get_port_telemetry_counter_req(instanceId, 0, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgQueryPortCharacteristics)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_port_characteristics_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_query_port_characteristics_req(instanceId, 0, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgQueryPortStatus)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_query_port_status_req(instanceId, 0,
                                                            requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetFabricManagerState)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_fabric_manager_state_req(instanceId,
                                                                   requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgQueryPortsAvailable)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_query_ports_available_req(instanceId,
                                                                requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Type 3 (Platform Environmental) handler dispatch via processRxMsg
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetInventoryInformation)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_inventory_information_req(
        instanceId, BOARD_PART_NUMBER, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetTemperatureReading)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_temperature_reading_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_temperature_reading_req(instanceId, 0,
                                                                  requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetTemperatureReadingAggregate)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_temperature_reading_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_get_temperature_reading_req(instanceId, 255, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetCurrentPowerDraw)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_power_draw_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_current_power_draw_req(instanceId, 0,
                                                                 0, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetCurrentPowerDrawAggregate)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_power_draw_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_get_current_power_draw_req(instanceId, 255, 0, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetMaxObservedPower)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_max_observed_power_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_max_observed_power_req(instanceId, 0,
                                                                 0, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetMaxObservedPowerAggregate)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_max_observed_power_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_get_max_observed_power_req(instanceId, 255, 0, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetEccErrorCounts)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_ECC_error_counts_req(instanceId,
                                                               requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetDriverInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_driver_info_req(instanceId,
                                                          requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Handler decode error paths (nullptr and short length)
// =============================================================================

TEST_F(MockupResponderBranchTest, PingHandlerDirectCall)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_ping_req(instanceId, requestMsg);

    auto resp = mockupResponder->pingHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderBranchTest, UnsupportedCommandHandlerDirectCall)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_ping_req(instanceId, requestMsg);

    auto resp = mockupResponder->unsupportedCommandHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, GetPortTelemetryCounterBadDecode)
{
    // Short buffer for decode to fail
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPortTelemetryCounterHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, QueryPortCharacteristicsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryPortCharacteristicsHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, QueryPortStatusBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryPortStatusHandler(requestMsg,
                                                        request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, GetFabricManagerStateBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getFabricManagerStateHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, QueryPortsAvailableBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryPortsAvailableHandler(requestMsg,
                                                            request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, SetPortDisableFutureBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setPortDisableFutureHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, GetPortDisableFutureBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPortDisableFutureHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, GetPowerModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getPowerModeHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, SetPowerModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setPowerModeHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, GetSwitchIsolationModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getSwitchIsolationMode(requestMsg,
                                                        request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, SetSwitchIsolationModeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setSwitchIsolationMode(requestMsg,
                                                        request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, GetInventoryInformationBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getInventoryInformationHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ReadThermalParameterBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->readThermalParameterHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// getProperty branch coverage (different property identifiers)
// =============================================================================

TEST_F(MockupResponderBranchTest, GetPropertyBoardPartNumber)
{
    auto res = mockupResponder->getProperty(BOARD_PART_NUMBER);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyFruPartNumber)
{
    auto res = mockupResponder->getProperty(FRU_PART_NUMBER);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertySerialNumber)
{
    auto res = mockupResponder->getProperty(SERIAL_NUMBER);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyMarketingName)
{
    auto res = mockupResponder->getProperty(MARKETING_NAME);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyBuildDate)
{
    auto res = mockupResponder->getProperty(BUILD_DATE);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyDeviceGuid)
{
    auto res = mockupResponder->getProperty(DEVICE_GUID);
    EXPECT_EQ(res.size(), 16u);
}

TEST_F(MockupResponderBranchTest, GetPropertyAssetTag)
{
    auto res = mockupResponder->getProperty(ASSET_TAG);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyInfoRomVersion)
{
    auto res = mockupResponder->getProperty(INFO_ROM_VERSION);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyProductLength)
{
    auto res = mockupResponder->getProperty(PRODUCT_LENGTH);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyProductWidth)
{
    auto res = mockupResponder->getProperty(PRODUCT_WIDTH);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyProductHeight)
{
    auto res = mockupResponder->getProperty(PRODUCT_HEIGHT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyRatedModulePowerLimit)
{
    auto res = mockupResponder->getProperty(RATED_MODULE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMaximumModulePowerLimit)
{
    auto res = mockupResponder->getProperty(MAXIMUM_MODULE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMinimumModulePowerLimit)
{
    auto res = mockupResponder->getProperty(MINIMUM_MODULE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMinimumDevicePowerLimit)
{
    auto res = mockupResponder->getProperty(MINIMUM_DEVICE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMaximumDevicePowerLimit)
{
    auto res = mockupResponder->getProperty(MAXIMUM_DEVICE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyRatedDevicePowerLimit)
{
    auto res = mockupResponder->getProperty(RATED_DEVICE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMinEdppScalingFactor)
{
    auto res = mockupResponder->getProperty(MINIMUM_EDPP_SCALING_FACTOR);
    EXPECT_EQ(res.size(), 1u);
}

TEST_F(MockupResponderBranchTest, GetPropertyMaxEdppScalingFactor)
{
    auto res = mockupResponder->getProperty(MAXIMUM_EDPP_SCALING_FACTOR);
    EXPECT_EQ(res.size(), 1u);
}

TEST_F(MockupResponderBranchTest, GetPropertyMinGraphicsClockLimit)
{
    auto res = mockupResponder->getProperty(MINIMUM_GRAPHICS_CLOCK_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMaxGraphicsClockLimit)
{
    auto res = mockupResponder->getProperty(MAXIMUM_GRAPHICS_CLOCK_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyDefaultBoostClocks)
{
    auto res = mockupResponder->getProperty(DEFAULT_BOOST_CLOCKS);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyDefaultBaseClocks)
{
    auto res = mockupResponder->getProperty(DEFAULT_BASE_CLOCKS);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMinMemoryClockLimit)
{
    auto res = mockupResponder->getProperty(MINIMUM_MEMORY_CLOCK_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMaxMemoryClockLimit)
{
    auto res = mockupResponder->getProperty(MAXIMUM_MEMORY_CLOCK_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyRatedGpuBasePowerLimit)
{
    auto res = mockupResponder->getProperty(RATED_GPU_BASE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMinGpuBasePowerLimit)
{
    auto res = mockupResponder->getProperty(MINIMUM_GPU_BASE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyMaxGpuBasePowerLimit)
{
    auto res = mockupResponder->getProperty(MAXIMUM_GPU_BASE_POWER_LIMIT);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyPcieRetimerEepromVersion)
{
    auto res = mockupResponder->getProperty(PCIERETIMER_0_EEPROM_VERSION);
    EXPECT_EQ(res.size(), 8u);
    auto res1 = mockupResponder->getProperty(PCIERETIMER_1_EEPROM_VERSION);
    EXPECT_EQ(res1.size(), 8u);
    auto res7 = mockupResponder->getProperty(PCIERETIMER_7_EEPROM_VERSION);
    EXPECT_EQ(res7.size(), 8u);
}

TEST_F(MockupResponderBranchTest, GetPropertyDevicePartNumber)
{
    auto res = mockupResponder->getProperty(DEVICE_PART_NUMBER);
    EXPECT_GT(res.size(), 0u);
}

TEST_F(MockupResponderBranchTest, GetPropertyMaxMemoryCapacity)
{
    auto res = mockupResponder->getProperty(MAXIMUM_MEMORY_CAPACITY);
    EXPECT_EQ(res.size(), sizeof(uint32_t));
}

TEST_F(MockupResponderBranchTest, GetPropertyUnknownReturnsEmpty)
{
    auto res = mockupResponder->getProperty(0xFF);
    EXPECT_TRUE(res.empty());
}

// =============================================================================
// getSupportCommandCodeHandler branch coverage
// =============================================================================

TEST_F(MockupResponderBranchTest, GetSupportedCommandCodesInvalidType)
{
    // Use nvidia_msg_type >= NUM_NSM_TYPES to trigger the error branch
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_supported_command_codes_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    // Override the nvidia_message_type field in payload to invalid
    auto reqPayload = reinterpret_cast<nsm_get_supported_command_codes_req*>(
        requestMsg->payload);
    reqPayload->nvidia_message_type = 0xFF; // Invalid

    auto resp = mockupResponder->getSupportCommandCodeHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// EventSource constructor branch coverage
// =============================================================================

TEST(EventSourceBranch, EmptyEvents)
{
    MockupResponder::EventSource es(std::vector<uint64_t>{});
    // All bits should be zero
    for (auto& e : es.events)
    {
        EXPECT_EQ(e.byte, 0u);
    }
}

TEST(EventSourceBranch, ValidEvents)
{
    MockupResponder::EventSource es(std::vector<uint64_t>{0, 1, 8, 15, 63});
    // bit 0 in byte 0
    EXPECT_NE(es.events[0].byte & 0x01, 0);
    // bit 1 in byte 0
    EXPECT_NE(es.events[0].byte & 0x02, 0);
    // bit 0 in byte 1
    EXPECT_NE(es.events[1].byte & 0x01, 0);
    // bit 7 in byte 1
    EXPECT_NE(es.events[1].byte & 0x80, 0);
    // bit 7 in byte 7
    EXPECT_NE(es.events[7].byte & 0x80, 0);
}

TEST(EventSourceBranch, EventBeyondRange)
{
    // event >= EVENT_SOURCES_LENGTH * 8 should be ignored
    MockupResponder::EventSource es(
        std::vector<uint64_t>{EVENT_SOURCES_LENGTH * 8});
    for (auto& e : es.events)
    {
        EXPECT_EQ(e.byte, 0u);
    }
}

TEST(EventSourceBranch, EventAtBoundary)
{
    // event == EVENT_SOURCES_LENGTH * 8 - 1 should be accepted
    uint64_t maxEvent = EVENT_SOURCES_LENGTH * 8 - 1;
    MockupResponder::EventSource es(std::vector<uint64_t>{maxEvent});
    auto idx = maxEvent >> 3;
    auto bit = maxEvent & 0x7;
    EXPECT_NE(es.events[idx].byte & (1u << bit), 0u);
}

// =============================================================================
// ReadThermalParameter handler branches
// =============================================================================

TEST_F(MockupResponderBranchTest, ReadThermalParameterSingleSensor)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_read_thermal_parameter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Set up a valid request for parameter_id != 255
    auto reqPayload =
        reinterpret_cast<nsm_read_thermal_parameter_req*>(requestMsg->payload);
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, &requestMsg->hdr);
    reqPayload->hdr.command = NSM_READ_THERMAL_PARAMETER;
    reqPayload->hdr.data_size = 1;
    reqPayload->parameter_id = 0; // Not aggregate

    auto resp = mockupResponder->readThermalParameterHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ReadThermalParameterAggregate)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_read_thermal_parameter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto reqPayload =
        reinterpret_cast<nsm_read_thermal_parameter_req*>(requestMsg->payload);
    nsm_header_info info{};
    info.nsm_msg_type = NSM_REQUEST;
    info.instance_id = instanceId;
    info.nvidia_msg_type = NSM_TYPE_PLATFORM_ENVIRONMENTAL;
    [[maybe_unused]] auto rc = pack_nsm_header(&info, &requestMsg->hdr);
    reqPayload->hdr.command = NSM_READ_THERMAL_PARAMETER;
    reqPayload->hdr.data_size = 1;
    reqPayload->parameter_id = 255; // Aggregate

    auto resp = mockupResponder->readThermalParameterHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Type 5 (Device Configuration) handler dispatch
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetErrorInjectionModeV1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc =
        encode_get_error_injection_mode_v1_req(instanceId, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranchTest, ProcessRxMsgGetReconfigurationPermissionsV1)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Type 2 (PCI Link) handler dispatch
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgQueryScalarGroupTelemetry)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_query_scalar_group_telemetry_v1_req(
        instanceId, 0, GROUP_ID_10, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Type 6 (Firmware) handler dispatch
// =============================================================================

TEST_F(MockupResponderBranchTest, ProcessRxMsgFirmwareGetRotInfo)
{
    // Use the proper encode function with required struct
    nsm_firmware_erot_state_info_req fw_req{};
    fw_req.component_classification = 0;
    fw_req.component_classification_index = 0;
    fw_req.component_identifier = 0;

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    [[maybe_unused]] auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->processRxMsg(request, longRunning);
    EXPECT_TRUE(resp.has_value());
}
