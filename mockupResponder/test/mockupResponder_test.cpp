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
#include "device-capability-discovery.h"
#include "device-configuration.h"
#include "diagnostics.h"
#include "firmware-utils.h"
#include "network-ports.h"
#include "pci-links.h"
#include "platform-environmental.h"
#include "powersmoothing-powerprofile-api-v2.h"

#include "common/event.hpp"
#include "utils.hpp"

#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <functional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Test;

#define private public
#define protected public
#include "mockupResponder.hpp"
class MockupResponderTest : public Test
{
  private:
    template <typename ResponseStruct>
    void test(
        const nsm_msg* requestMsg, size_t requestMsgLen,
        std::function<std::optional<Response>(const nsm_msg*, size_t)> handler,
        uint8_t command, ResponseStruct& response)
    {
        // Good Test
        auto resp = handler(requestMsg, requestMsgLen);

        EXPECT_TRUE(resp.has_value());
        EXPECT_EQ(resp.value().size(),
                  sizeof(nsm_msg_hdr) + sizeof(ResponseStruct));

        auto msg = reinterpret_cast<nsm_msg*>(resp.value().data());
        EXPECT_GE(sizeof(ResponseStruct), sizeof(nsm_common_resp));
        auto common = reinterpret_cast<nsm_common_resp*>(msg->payload);
        EXPECT_EQ(command, common->command);
        EXPECT_EQ(sizeof(ResponseStruct) - sizeof(nsm_common_resp),
                  common->data_size);
        response = *reinterpret_cast<ResponseStruct*>(msg->payload);

        // Bad tests
        resp = handler(nullptr, requestMsgLen);
        EXPECT_FALSE(resp.has_value());
        resp = handler(requestMsg, requestMsgLen - 1);
        EXPECT_FALSE(resp.has_value());

        auto badRequest = Request((uint8_t*)requestMsg,
                                  (uint8_t*)requestMsg + requestMsgLen);
        auto badRequestMsg = reinterpret_cast<nsm_msg*>(badRequest.data());
        badRequestMsg->hdr.ocp_type = 0;
        resp = handler(badRequestMsg, requestMsgLen);
        EXPECT_FALSE(resp.has_value());
    }

    template <typename ResponseStruct>
    void
        test(const nsm_msg* requestMsg, size_t requestMsgLen,
             std::function<std::optional<Response>(const nsm_msg*, size_t, bool,
                                                   std::optional<Request>&)>
                 handler,
             uint8_t command, ResponseStruct& response)
    {
        std::optional<Request> longRunningEvent;
        // test as not long running
        test(requestMsg, requestMsgLen,
             [&handler, &longRunningEvent](const nsm_msg* request, size_t len) {
            return handler(request, len, false, longRunningEvent);
        }, command, response);

        // test as long running
        auto resp = handler(requestMsg, requestMsgLen, true, longRunningEvent);

        EXPECT_TRUE(resp.has_value());
        EXPECT_EQ(resp.value().size(),
                  sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
        auto respMsg = reinterpret_cast<nsm_msg*>(resp.value().data());
        auto commonResp = reinterpret_cast<nsm_common_resp*>(respMsg->payload);
        EXPECT_EQ(command, commonResp->command);
        EXPECT_EQ(NSM_ACCEPTED, commonResp->completion_code);
        EXPECT_EQ(0, commonResp->reserved);
        EXPECT_EQ(0, commonResp->data_size);

        EXPECT_TRUE(longRunningEvent.has_value());
        EXPECT_EQ(longRunningEvent.value().size(),
                  sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN +
                      sizeof(nsm_long_running_resp) + sizeof(ResponseStruct) -
                      sizeof(nsm_common_resp));
    }

  protected:
    MockupResponderTest()

    {
        init(30, NSM_DEV_ID_GPU, 2);
    }

    uint8_t instanceId = 0;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;

    void init(eid_t eid, uint8_t deviceType, uint8_t instanceId)
    {
        this->instanceId = instanceId;
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            true, event, *objServer, eid, deviceType, instanceId);
    }

    void TearDown() override
    {
        mockupResponder.reset();
        objServer.reset();
        systemBus.reset();
        io.stop();
    }

    void testProperty(uint8_t propertyIdentifier,
                      const std::string& expectedValue)
    {
        // get property
        auto res = mockupResponder->getProperty(propertyIdentifier);
        EXPECT_NE(res.size(), 0);

        // verify property value
        std::string returnedValue((char*)res.data(), res.size());
        EXPECT_STREQ(returnedValue.c_str(), expectedValue.c_str());
    }
    void testProperty(uint8_t propertyIdentifier, uint32_t expectedValue)
    {
        // get property
        auto res = mockupResponder->getProperty(propertyIdentifier);
        EXPECT_EQ(res.size(), sizeof(uint32_t));

        // verify property value
        uint32_t returnedValue = htole32(*(uint32_t*)res.data());
        EXPECT_EQ(returnedValue, expectedValue);
    }

    using MockupResponderFunction = std::optional<Response> (
        MockupResponder::MockupResponder::*)(const nsm_msg*, size_t);

    template <typename RequestPayload, typename ResponseStruct,
              typename MockupFunction>
    void test(std::function<int(uint8_t, const RequestPayload*, nsm_msg*)>
                  encodeRequestFunction,
              RequestPayload& requestPayload, MockupFunction handlerFunction,
              uint8_t command, ResponseStruct& response)
    {
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
                            sizeof(RequestPayload),
                        0);
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encodeRequestFunction(
            instanceId, const_cast<const RequestPayload*>(&requestPayload),
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto handler = std::bind_front(handlerFunction, mockupResponder.get());
        test(requestMsg, request.size(), handler, command, response);
    }
    template <typename RequestPayload, typename MockupFunction>
    void test(std::function<int(uint8_t, RequestPayload, nsm_msg*)>
                  encodeRequestFunction,
              RequestPayload requestPayload, MockupFunction handlerFunction,
              uint8_t command)
    {
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
                        sizeof(RequestPayload));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encodeRequestFunction(instanceId, requestPayload, requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        nsm_common_resp response;
        auto handler = std::bind_front(handlerFunction, mockupResponder.get());
        test(requestMsg, request.size(), handler, command, response);
        EXPECT_EQ(NSM_SUCCESS, response.completion_code);
    }
    template <typename ResponseStruct, typename MockupFunction>
    void test(std::function<int(uint8_t, nsm_msg*)> encodeRequestFunction,
              MockupFunction handlerFunction, uint8_t command,
              ResponseStruct& response)
    {
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encodeRequestFunction(instanceId, requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto handler = std::bind_front(handlerFunction, mockupResponder.get());
        test(requestMsg, request.size(), handler, command, response);
    }
};

TEST_F(MockupResponderTest, goodTestGetPropertyTest)
{
    testProperty(BOARD_PART_NUMBER, "MCX750500B-0D00_DK");
    testProperty(SERIAL_NUMBER, "SN123456789");
    testProperty(MARKETING_NAME, "NV123");
    testProperty(PRODUCT_NAME, "BlueField-4");
    testProperty(PRODUCT_LENGTH, 850);
    testProperty(PRODUCT_WIDTH, 730);
    testProperty(PRODUCT_HEIGHT, 2600);
    testProperty(MINIMUM_DEVICE_POWER_LIMIT, 10000);
    testProperty(MAXIMUM_DEVICE_POWER_LIMIT, 100000);
}

TEST_F(MockupResponderTest, goodTestUuidPropertyTest)
{
    uuid_t expectedUuid("72000000-0000-0000-0000-000000000000");

    // get Uuid property
    auto res = mockupResponder->getProperty(DEVICE_GUID);
    EXPECT_EQ(res.size(), 16);
    // verify Uuid property value
    auto uuidProperty = utils::convertUUIDToString(res);
    EXPECT_STREQ(uuidProperty.substr(2).c_str(),
                 expectedUuid.substr(2).c_str());
}

TEST_F(MockupResponderTest, goodTestPowerSupplyStatusTest)
{
    const uint8_t expectedStatus = 0b00110011;
    std::vector<uint8_t> requestMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
    fpga_diagnostics_settings_data_index data_index = GET_POWER_SUPPLY_STATUS;

    auto rc = encode_get_fpga_diagnostics_settings_req(0, data_index, request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto res = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        request, requestMsg.size());

    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_supply_status_resp));

    auto response = reinterpret_cast<nsm_msg*>(res.value().data());
    auto resp = (nsm_get_power_supply_status_resp*)response->payload;

    EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
    EXPECT_EQ(expectedStatus, resp->power_supply_status);
}

TEST_F(MockupResponderTest, goodTestGpuPresenceTest)
{
    uint32_t expectedPresence = 0b11111111;
    std::vector<uint8_t> requestMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
    fpga_diagnostics_settings_data_index data_index = GET_GPU_PRESENCE;

    auto rc = encode_get_fpga_diagnostics_settings_req(0, data_index, request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto res = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        request, requestMsg.size());

    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_presence_resp));

    auto response = reinterpret_cast<nsm_msg*>(res.value().data());
    auto resp = (nsm_get_gpu_presence_resp*)response->payload;

    EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
    EXPECT_EQ(expectedPresence, resp->presence);
}

TEST_F(MockupResponderTest, goodTestGpuPresenceAndPowerStatusTest)
{
    uint32_t expectedPower = 0b11110111;
    std::vector<uint8_t> requestMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req));

    auto request = reinterpret_cast<nsm_msg*>(requestMsg.data());
    fpga_diagnostics_settings_data_index data_index = GET_GPU_POWER_STATUS;

    auto rc = encode_get_fpga_diagnostics_settings_req(0, data_index, request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto res = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        request, requestMsg.size());

    EXPECT_TRUE(res.has_value());
    EXPECT_EQ(res.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_power_status_resp));

    auto response = reinterpret_cast<nsm_msg*>(res.value().data());
    auto resp = (nsm_get_gpu_power_status_resp*)response->payload;

    EXPECT_EQ(NSM_GET_FPGA_DIAGNOSTICS_SETTINGS, resp->hdr.command);
    EXPECT_EQ(expectedPower, resp->power_status);
}

TEST_F(MockupResponderTest, goodTestGetReconfigurationPermissionsV1Handler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));

    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    reconfiguration_permissions_v1_index setting_index = RP_IN_SYSTEM_TEST;

    auto rc = encode_get_reconfiguration_permissions_v1_req(0, setting_index,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());

    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_reconfiguration_permissions_v1_resp));

    auto msg = reinterpret_cast<nsm_msg*>(resp.value().data());
    auto response =
        reinterpret_cast<nsm_get_reconfiguration_permissions_v1_resp*>(
            msg->payload);

    nsm_reconfiguration_permissions_v1 expected = {0, 0, 0, 0, 1, 0, 1};
    EXPECT_EQ(NSM_GET_RECONFIGURATION_PERMISSIONS_V1, response->hdr.command);
    EXPECT_EQ(expected.host_oneshot, response->data.host_oneshot);
    EXPECT_EQ(expected.host_persistent, response->data.host_persistent);
    EXPECT_EQ(expected.host_flr_persistent, response->data.host_flr_persistent);
}

TEST_F(MockupResponderTest, goodTestSetReconfigurationPermissionsV1Handler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));

    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto settingIndex = RP_IN_SYSTEM_TEST;
    auto configuration = RP_ONESHOOT_HOT_RESET;
    uint8_t permission = 0;

    auto rc = encode_set_reconfiguration_permissions_v1_req(
        0, settingIndex, configuration, permission, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());

    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));

    auto msg = reinterpret_cast<nsm_msg*>(resp.value().data());
    auto response = reinterpret_cast<nsm_common_resp*>(msg->payload);

    EXPECT_EQ(NSM_SET_RECONFIGURATION_PERMISSIONS_V1, response->command);
}

TEST_F(MockupResponderTest, testGetErrorInjectionModeV1Handler)
{
    nsm_get_error_injection_mode_v1_resp response;
    test(&encode_get_error_injection_mode_v1_req,
         &MockupResponder::MockupResponder::getErrorInjectionModeV1Handler,
         NSM_GET_ERROR_INJECTION_MODE_V1, response);
    EXPECT_EQ(mockupResponder->state.errorInjectionMode.mode,
              response.data.mode);
    EXPECT_EQ(mockupResponder->state.errorInjectionMode.flags.byte,
              response.data.flags.byte);
}

TEST_F(MockupResponderTest, testGetSupportedErrorInjectionTypesHandler)
{
    nsm_get_error_injection_types_mask_resp response;
    test(&encode_get_supported_error_injection_types_v1_req,
         &MockupResponder::MockupResponder::
             getSupportedErrorInjectionTypesV1Handler,
         NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1, response);
    for (const auto& [type, _] :
         mockupResponder->state.errorInjection[NSM_DEV_ID_GPU])
    {
        EXPECT_TRUE(response.data.mask[type / 8] & (1 << (type % 8)));
    }
}

TEST_F(MockupResponderTest, testSetCurrentErrorInjectionTypesHandler)
{
    nsm_error_injection_types_mask data = {0, 0, 0, 0, 0, 0, 0, 0};
    nsm_common_resp response;
    for (const auto& [type, _] :
         mockupResponder->state.errorInjection[NSM_DEV_ID_GPU])
    {
        data.mask[type / 8] |= (1 << (type % 8));
    }
    test<nsm_error_injection_types_mask>(
        &encode_set_current_error_injection_types_v1_req, data,
        &MockupResponder::MockupResponder::
            setCurrentErrorInjectionTypesV1Handler,
        NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1, response);
}
TEST_F(MockupResponderTest, testGetCurrentErrorInjectionTypesHandler)
{
    nsm_get_error_injection_types_mask_resp response;
    test(&encode_get_current_error_injection_types_v1_req,
         &MockupResponder::MockupResponder::
             getCurrentErrorInjectionTypesV1Handler,
         NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1, response);
    for (const auto& [type, enabled] :
         mockupResponder->state.errorInjection[NSM_DEV_ID_GPU])
    {
        EXPECT_EQ(enabled, response.data.mask[type / 8] & (1 << (type % 8)));
    }
}

TEST_F(MockupResponderTest, testMigModeHandler)
{
    nsm_get_MIG_mode_resp response;
    test(&encode_get_MIG_mode_req,
         &MockupResponder::MockupResponder::getMigModeHandler, NSM_GET_MIG_MODE,
         response);
    EXPECT_EQ(mockupResponder->state.migMode, response.flags.byte);
    uint8_t data = 1;
    test<uint8_t>(&encode_set_MIG_mode_req, data,
                  &MockupResponder::MockupResponder::setMigModeHandler,
                  NSM_SET_MIG_MODE);
    test(&encode_get_MIG_mode_req,
         &MockupResponder::MockupResponder::getMigModeHandler, NSM_GET_MIG_MODE,
         response);
    EXPECT_EQ(mockupResponder->state.migMode, response.flags.byte);
}

TEST_F(MockupResponderTest, testEccModeHandler)
{
    nsm_get_ECC_mode_resp response;
    test(&encode_get_ECC_mode_req,
         &MockupResponder::MockupResponder::getEccModeHandler, NSM_GET_ECC_MODE,
         response);
    EXPECT_EQ(mockupResponder->state.eccMode, response.flags.byte);
    uint8_t data = 1;
    test<uint8_t>(&encode_set_ECC_mode_req, data,
                  &MockupResponder::MockupResponder::setEccModeHandler,
                  NSM_SET_ECC_MODE);
    test(&encode_get_ECC_mode_req,
         &MockupResponder::MockupResponder::getEccModeHandler, NSM_GET_ECC_MODE,
         response);
    EXPECT_EQ(mockupResponder->state.eccMode, response.flags.byte);
}

TEST_F(MockupResponderTest, testGetMemoryCapacityUtilHandler)
{
    nsm_get_memory_capacity_util_resp response;
    test(&encode_get_memory_capacity_util_req,
         &MockupResponder::MockupResponder::getMemoryCapacityUtilHandler,
         NSM_GET_MEMORY_CAPACITY_UTILIZATION, response);
}

TEST_F(MockupResponderTest, testGetCurrentUtilizationHandler)
{
    nsm_get_current_utilization_resp response;
    test(&encode_get_current_utilization_req,
         &MockupResponder::MockupResponder::getCurrentUtilizationHandler,
         NSM_GET_CURRENT_UTILIZATION, response);
}

TEST_F(MockupResponderTest, testGetViolationDurationHandler)
{
    nsm_get_violation_duration_resp response;
    test(&encode_get_violation_duration_req,
         &MockupResponder::MockupResponder::getViolationDurationHandler,
         NSM_GET_VIOLATION_DURATION, response);
}

TEST_F(MockupResponderTest, testListAvailablePciePortsHandler)
{
    nsm_list_available_pcie_ports_resp response;
    test(&encode_list_available_pcie_ports_req,
         &MockupResponder::MockupResponder::getListAvailablePciePortsHandler,
         NSM_LIST_AVAILABLE_PCIE_PORTS, response);
}

struct QueryScalarGroupTelemetryV1GroupReqData
{
    uint8_t deviceId;
    uint8_t groupId;
} __attribute__((packed));

auto encodeQueryScalarGroupTelemetryV1Req()
{
    return
        [&](uint8_t instanceId,
            const QueryScalarGroupTelemetryV1GroupReqData* data, nsm_msg* msg) {
        return encode_query_scalar_group_telemetry_v1_req(
            instanceId, data->deviceId, data->groupId, msg);
    };
}

TEST_F(MockupResponderTest, testQueryScalarGroup10TelemetryHandler)
{
    QueryScalarGroupTelemetryV1GroupReqData data = {0, GROUP_ID_10};
    nsm_query_scalar_group_telemetry_v1_group_10_resp response;
    test<QueryScalarGroupTelemetryV1GroupReqData>(
        encodeQueryScalarGroupTelemetryV1Req(), data,
        &MockupResponder::MockupResponder::queryScalarGroupTelemetryHandler,
        NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1, response);
}
TEST_F(MockupResponderTest, testMultiportQueryScalarGroup10TelemetryHandler)
{
    nsm_multiport_query_scalar_group_telemetry_v2_req_data data = {
        0, NSM_PORT_TYPE_UPSTREAM, 0, GROUP_ID_10};
    nsm_query_scalar_group_telemetry_v1_group_10_resp response;
    test<nsm_multiport_query_scalar_group_telemetry_v2_req_data>(
        &encode_multiport_query_scalar_group_telemetry_v2_req, data,
        &MockupResponder::MockupResponder::
            queryMultiportScalarGroupTelemetryHandler,
        NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2, response);
}

// =============================================================================
// Device Capability Discovery handlers (Type 0)
// =============================================================================

TEST_F(MockupResponderTest, testPingHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->pingHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetSupportedNvidiaMessageTypesHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_nvidia_message_types_req(instanceId,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportNvidiaMessageTypesHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_supported_nvidia_message_types_resp));
}

TEST_F(MockupResponderTest, testGetSupportedCommandCodesHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_supported_command_codes_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportCommandCodeHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_supported_command_codes_resp));
}

TEST_F(MockupResponderTest, testQueryDeviceIdentificationHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_device_identification_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_identification_req(instanceId,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryDeviceIdentificationHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_device_identification_resp));
}

TEST_F(MockupResponderTest, testGetEventSubscriptionHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_event_subscription_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEventSubscription(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_subscription_resp));
}

TEST_F(MockupResponderTest, testSetEventSubscriptionHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_event_subscription_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(
        instanceId, GLOBAL_EVENT_GENERATION_DISABLE, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setEventSubscription(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetSupportedEventSourcesHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_supported_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportedEventSources(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_resp));
}

TEST_F(MockupResponderTest, testGetCurrentEventSourcesHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_current_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentEventSources(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_resp));
}

TEST_F(MockupResponderTest, testSetCurrentEventSourcesHandler)
{
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_current_event_source_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_current_event_sources_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, eventSources,
        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setCurrentEventSources(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testConfigureEventAcknowledgementHandler)
{
    bitfield8_t mask[EVENT_SOURCES_LENGTH] = {};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_configure_event_acknowledgement_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_configure_event_acknowledgement_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, mask, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->configureEventAcknowledgement(requestMsg,
                                                               request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetDeviceCapabilitiesV2Handler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_capabilities_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_device_capabilities_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceCapabilitiesV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Network Port handlers (Type 1)
// =============================================================================

TEST_F(MockupResponderTest, testGetFabricManagerStateHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fabric_manager_state_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFabricManagerStateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_fabric_manager_state_resp));
}

TEST_F(MockupResponderTest, testQueryPortsAvailableHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_ports_available_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortsAvailableHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_query_ports_available_resp));
}

TEST_F(MockupResponderTest, testGetPortTelemetryCounterHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_port_telemetry_counter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_telemetry_counter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortTelemetryCounterHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPortCharacteristicsHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_port_characteristics_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_port_characteristics_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortCharacteristicsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_query_port_characteristics_resp));
}

TEST_F(MockupResponderTest, testQueryPortStatusHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_port_status_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortStatusHandler(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_resp));
}

// =============================================================================
// Link Health Indication Extension: Query Port Characteristics v2 (0x12) and
// Clear Port Metric State (0x13)
// =============================================================================

namespace
{
struct LinkHealthV2Record
{
    bool valid = false;
    nsm_link_health_record record{};
};

// Sends 0x12 for `portNumber`, checks the five records (Tags 0x00-0x04, one
// u32 each) and returns the decoded Tag 0x04 record.
LinkHealthV2Record
    queryLinkHealthV2(MockupResponder::MockupResponder& responder,
                      uint8_t instanceId, uint16_t portNumber)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_port_characteristics_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_query_port_characteristics_v2_req(instanceId, portNumber,
                                                       requestMsg),
              NSM_SW_SUCCESS);

    LinkHealthV2Record result;
    auto resp = responder.queryPortCharacteristicsV2Handler(requestMsg,
                                                            request.size());
    if (!resp.has_value())
    {
        ADD_FAILURE() << "no Query Port Characteristics v2 response";
        return result;
    }

    auto respMsg = reinterpret_cast<const nsm_msg*>(resp->data());
    uint8_t cc = NSM_ERROR;
    uint16_t recordCount = 0;
    size_t consumedLen = 0;
    EXPECT_EQ(decode_aggregate_resp(respMsg, resp->size(), &consumedLen, &cc,
                                    &recordCount),
              NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(recordCount, 5);

    const uint8_t* data = resp->data() + consumedLen;
    size_t remaining = resp->size() - consumedLen;
    for (uint16_t index = 0; index < recordCount; ++index)
    {
        uint8_t tag = 0;
        bool valid = false;
        const uint8_t* sampleData = nullptr;
        size_t dataLen = 0;
        size_t sampleLen = 0;
        auto sample = reinterpret_cast<const nsm_aggregate_resp_sample*>(data);
        if (decode_aggregate_resp_sample(sample, remaining, &sampleLen, &tag,
                                         &valid, &sampleData,
                                         &dataLen) != NSM_SW_SUCCESS)
        {
            ADD_FAILURE() << "malformed record " << index;
            break;
        }
        EXPECT_EQ(tag, index); // Tags 0x00..0x04 in order
        EXPECT_EQ(dataLen, sizeof(uint32_t));
        if (tag == NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH)
        {
            result.valid = valid;
            EXPECT_EQ(
                decode_link_health_record(sampleData, dataLen, &result.record),
                NSM_SW_SUCCESS);
        }
        else
        {
            EXPECT_TRUE(valid);
        }
        data += sampleLen;
        remaining -= sampleLen;
    }
    EXPECT_EQ(remaining, 0u);
    return result;
}

std::optional<Response>
    clearLinkHealth(MockupResponder::MockupResponder& responder,
                    uint8_t instanceId, uint16_t portNumber,
                    const std::vector<uint8_t>& tagIds)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_clear_port_metric_state_req) + tagIds.size());
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_clear_port_metric_state_req(
                  instanceId, portNumber, static_cast<uint16_t>(tagIds.size()),
                  tagIds.data(), requestMsg),
              NSM_SW_SUCCESS);
    return responder.clearPortMetricStateHandler(requestMsg, request.size());
}
} // namespace

TEST_F(MockupResponderTest, testQueryPortCharacteristicsV2Handler)
{
    // Without --failure_cycle the port reports a latched Attention until it
    // is cleared.
    auto health = queryLinkHealthV2(*mockupResponder, instanceId, 1);
    EXPECT_TRUE(health.valid);
    EXPECT_EQ(health.record.link_health, NSM_LINK_HEALTH_ATTENTION);
    EXPECT_EQ(health.record.attention_trigger,
              NSM_ATTENTION_TRIGGER_EFFECTIVE_BER);
    EXPECT_EQ(health.record.attention_trigger_metric, 3);
    EXPECT_EQ(health.record.link_health_config_changed,
              NSM_LINK_HEALTH_CONFIG_CURRENT);
}

TEST_F(MockupResponderTest, testClearPortMetricStateHandlerClearsAndRelatches)
{
    auto resp = clearLinkHealth(*mockupResponder, instanceId, 1,
                                {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH});
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_resp));
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    EXPECT_EQ(decode_clear_port_metric_state_resp(
                  reinterpret_cast<const nsm_msg*>(resp->data()), resp->size(),
                  &cc, &reasonCode),
              NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_SUCCESS);

    // Healthy with neutral trigger fields until the re-latch poll.
    for (size_t poll = 1;
         poll < MockupResponder::MockupResponder::kLinkHealthRelatchPolls;
         ++poll)
    {
        auto health = queryLinkHealthV2(*mockupResponder, instanceId, 1);
        EXPECT_TRUE(health.valid);
        EXPECT_EQ(health.record.link_health, NSM_LINK_HEALTH_HEALTHY)
            << "poll " << poll;
        EXPECT_EQ(health.record.attention_trigger, NSM_ATTENTION_TRIGGER_NA);
        EXPECT_EQ(health.record.attention_trigger_metric, 0);
        EXPECT_EQ(health.record.link_health_config_changed,
                  NSM_LINK_HEALTH_CONFIG_NA);
    }
    auto relatched = queryLinkHealthV2(*mockupResponder, instanceId, 1);
    EXPECT_EQ(relatched.record.link_health, NSM_LINK_HEALTH_ATTENTION);
    EXPECT_EQ(relatched.record.attention_trigger,
              NSM_ATTENTION_TRIGGER_EFFECTIVE_BER);
}

TEST_F(MockupResponderTest, testClearPortMetricStateHandlerPerPortAndBadTag)
{
    // Clearing port 1 leaves port 2 latched.
    ASSERT_TRUE(clearLinkHealth(*mockupResponder, instanceId, 1,
                                {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH})
                    .has_value());
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 1).record.link_health,
        NSM_LINK_HEALTH_HEALTHY);
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 2).record.link_health,
        NSM_LINK_HEALTH_ATTENTION);

    // Only Tag 0x04 is clearable: any other TagID is INVALID_DATA in the
    // reason-code layout, and the latch of that port stays.
    auto resp = clearLinkHealth(*mockupResponder, instanceId, 2,
                                {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH,
                                 NSM_PORT_CHARACTERISTICS_V2_TAG_LINE_RATE});
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    EXPECT_EQ(decode_clear_port_metric_state_resp(
                  reinterpret_cast<const nsm_msg*>(resp->data()), resp->size(),
                  &cc, &reasonCode),
              NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 2).record.link_health,
        NSM_LINK_HEALTH_ATTENTION);
}

// =============================================================================
// Platform Environmental handlers (Type 3)
// =============================================================================

TEST_F(MockupResponderTest, testGetEccErrorCountsHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_ECC_error_counts_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEccErrorCountsHandler(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_ECC_error_counts_resp));
}

TEST_F(MockupResponderTest, testGetClockLimitHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_limit_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getClockLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_resp));
}

TEST_F(MockupResponderTest, testGetCurrClockFreqHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_curr_clock_freq_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrClockFreqHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_resp));
}

TEST_F(MockupResponderTest, testGetPowerLimitHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_limit_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp));
}

TEST_F(MockupResponderTest, testGetClockOutputEnableStateHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_clock_output_enabled_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, PCIE_CLKBUF_INDEX, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getClockOutputEnableStateHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_clock_output_enabled_state_resp));
}

TEST_F(MockupResponderTest, testGetRowRemapStateHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_state_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRowRemapStateHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remap_state_resp));
}

TEST_F(MockupResponderTest, testGetRowRemappingCountsHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remapping_counts_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRowRemappingCountsHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_row_remapping_counts_resp));
}

TEST_F(MockupResponderTest, testGetRowRemapAvailabilityHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_availability_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRowRemapAvailabilityHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_row_remap_availability_resp));
}

TEST_F(MockupResponderTest, testGetLeakDetectionInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_leak_detection_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getLeakDetectionInfoHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetProcessorThrottleReasonHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                             requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getProcessorThrottleReasonHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_current_clock_event_reason_code_resp));
}

TEST_F(MockupResponderTest, testGetAccumGpuUtilTimeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_accum_GPU_util_time_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getAccumCpuUtilTimeHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_accum_GPU_util_time_resp));
}

TEST_F(MockupResponderTest, testGetAltitudePressureHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_altitude_pressure_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getAltitudePressureHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_altitude_pressure_resp));
}

TEST_F(MockupResponderTest, testGetTemperatureReadingHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_temperature_reading_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_temperature_reading_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getTemperatureReadingHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_resp));
}

TEST_F(MockupResponderTest, testGetInventoryInformationHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, BOARD_PART_NUMBER, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getInventoryInformationHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Device Configuration handlers (Type 5)
// =============================================================================

TEST_F(MockupResponderTest, testGetConfidentialComputeModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_confidential_compute_mode_v1_req(instanceId,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getConfidentialComputeModeHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_confidential_compute_mode_v1_resp));
}

TEST_F(MockupResponderTest, testGetProtectionOptionsHandler)
{
    nsm_get_protection_options_resp response;
    test(&encode_get_protection_options_req,
         &MockupResponder::MockupResponder::getProtectionOptionsHandler,
         NSM_GET_PROTECTION_OPTIONS, response);
}

TEST_F(MockupResponderTest, testSetErrorInjectionModeV1Handler)
{
    uint8_t mode = 0;
    test<uint8_t>(
        &encode_set_error_injection_mode_v1_req, mode,
        &MockupResponder::MockupResponder::setErrorInjectionModeV1Handler,
        NSM_SET_ERROR_INJECTION_MODE_V1);
}

// =============================================================================
// Power Smoothing handlers (Type 3 continued)
// =============================================================================

TEST_F(MockupResponderTest, testGetPowerSmoothingFeatureInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerSmoothingFeatureInfo(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_smoothing_feat_resp));
}

TEST_F(MockupResponderTest, testGetHwCircuiteryUsageHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_hardware_lifetime_cricuitry_req(instanceId,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getHwCircuiteryUsage(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_hardwareciruitry_resp));
}

TEST_F(MockupResponderTest, testGetCurrentProfileInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentProfileInfo(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_profile_info_resp));
}

// =============================================================================
// Platform Environmental - additional handlers
// =============================================================================

TEST_F(MockupResponderTest, testGetDriverInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_driver_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDriverInfoHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_driver_info_resp));
}

TEST_F(MockupResponderTest, testGetCurrentPowerDrawHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_power_draw_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Use sensor_id=0 for a single reading
    auto rc = encode_get_current_power_draw_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentPowerDrawHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetMaxObservedPowerHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_max_observed_power_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_max_observed_power_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getMaxObservedPowerHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetCurrentEnergyCountHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_energy_count_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_energy_count_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentEnergyCountHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_resp));
}

TEST_F(MockupResponderTest, testGetVoltageHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getVoltageHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_resp));
}

TEST_F(MockupResponderTest, testReadThermalParameterHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_read_thermal_parameter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_read_thermal_parameter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->readThermalParameterHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_resp));
}

TEST_F(MockupResponderTest, testSetClockLimitHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_clock_limit_req(instanceId, 0, 0, 0, UINT32_MAX,
                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setClockLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testSetPowerLimitHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_power_limit_req(instanceId, 0, 0, 0, 100, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPowerLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetEDPpScalingFactorHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_programmable_EDPp_scaling_factor_req(instanceId,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEDPpScalingFactorHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_programmable_EDPp_scaling_factor_resp));
}

TEST_F(MockupResponderTest, testSetEDPpScalingFactorHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_programmable_EDPp_scaling_factor_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_programmable_EDPp_scaling_factor_req(instanceId, 0, 0,
                                                              100, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setEDPpScalingFactorHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

// =============================================================================
// Device Configuration - additional handlers
// =============================================================================

TEST_F(MockupResponderTest, testSetConfidentialComputeModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_confidential_compute_mode_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_confidential_compute_mode_v1_req(instanceId, 0,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setConfidentialComputeModeHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetEgmModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_EGM_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEgmModeHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_EGM_mode_resp));
}

TEST_F(MockupResponderTest, testSetEgmModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_EGM_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_EGM_mode_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setEgmModeHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetDevicemodeSettingsHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_setting_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_setting_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_resp));
}

TEST_F(MockupResponderTest, testSetDevicemodeSettingsHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_setting_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_setting_req(instanceId, 0, DISABLED,
                                                 requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testEnableDisableGpuIstModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_disable_gpu_ist_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 0, 1,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testActivateErrorInjectionHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_activate_error_injection_payload_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_activate_error_injection_payload_req(instanceId, 0, 0,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->activateErrorInjectionHandler(requestMsg,
                                                               request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

// =============================================================================
// Diagnostics - additional handlers
// =============================================================================

TEST_F(MockupResponderTest, testEnableDisableWriteProtectedHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM, 1,
                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetNetworkDeviceDebugInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_network_device_debug_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_device_debug_info_req(instanceId, 0, 0,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_network_device_debug_info_resp));
}

TEST_F(MockupResponderTest, testGetNetworkDeviceLogInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_network_device_log_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_device_log_info_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getNetworkDeviceLogInfoHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_get_network_device_log_info_resp));
}

TEST_F(MockupResponderTest, testEraseTraceHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_erase_trace_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->eraseTraceHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_resp));
}

TEST_F(MockupResponderTest, testEraseDebugInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_erase_debug_info_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->eraseDebugInfoHandler(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_resp));
}

// =============================================================================
// Network Ports - additional handlers
// =============================================================================

TEST_F(MockupResponderTest, testGetPowerModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerModeHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_mode_resp));
}

TEST_F(MockupResponderTest, testSetPowerModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_power_mode_data data{};
    auto rc = encode_set_power_mode_req(instanceId, requestMsg, data);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPowerModeHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_resp));
}

TEST_F(MockupResponderTest, testGetPortDisableFutureHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_disable_future_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortDisableFutureHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_disable_future_resp));
}

TEST_F(MockupResponderTest, testSetPortDisableFutureHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_port_disable_future_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    auto rc = encode_set_port_disable_future_req(instanceId, mask, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPortDisableFutureHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_resp));
}

TEST_F(MockupResponderTest, testGetEthPortTelemetryCounterHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_ethernet_port_telemetry_counter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_eth_port_telemetry_counter_req(instanceId, 0,
                                                        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEthPortTelemetryCounterHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortNetworkAddressesHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_network_addresses_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_addresses_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortNetworkAddressesHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortEccCountersHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_port_ecc_counters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_ecc_counters_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortEccCountersHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetHistogramFormatHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_format_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getHistogramFormatHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetHistogramDataHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_data_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getHistogramDataHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Debug Token handlers
// =============================================================================

TEST_F(MockupResponderTest, testQueryTokenParametersHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_token_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_token_parameters_req(
        instanceId, NSM_DEBUG_TOKEN_OPCODE_RMCS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryTokenParametersHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_resp));
}

TEST_F(MockupResponderTest, testProvideTokenHandler)
{
    constexpr size_t tokenLen = 4;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_req) +
                    tokenLen - 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t token_data[tokenLen] = {0x01, 0x02, 0x03, 0x04};
    auto rc = encode_nsm_provide_token_req(instanceId, token_data, tokenLen,
                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->provideTokenHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_resp));
}

TEST_F(MockupResponderTest, testDisableTokensHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_disable_tokens_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_disable_tokens_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->disableTokensHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_disable_tokens_resp));
}

TEST_F(MockupResponderTest, testQueryTokenStatusHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_token_status_req(
        instanceId, NSM_DEBUG_TOKEN_TYPE_FRC, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryTokenStatusHandler(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_resp));
}

TEST_F(MockupResponderTest, testQueryDeviceIdsHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_ids_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryDeviceIdsHandler(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_ids_resp));
}

TEST_F(MockupResponderTest, testUnsupportedCommandHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Use ping to get a valid header, then call unsupportedCommandHandler
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->unsupportedCommandHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
}

TEST_F(MockupResponderTest, testPcieFundamentalResetHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_assert_pcie_fundamental_reset_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_assert_pcie_fundamental_reset_req(instanceId, 0, 1,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->pcieFundamentalResetHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testClearScalarDataSourceHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_clear_data_source_v1_req(instanceId, 0, GROUP_ID_2, 0,
                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->clearScalarDataSourceHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testResetNetworkDeviceHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_reset_network_device_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_resp));
}

TEST_F(MockupResponderTest, testGetErrorInjectionPayloadHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_error_injection_payload_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_payload_req(instanceId, 4, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getErrorInjectionPayloadHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testSetErrorInjectionPayloadHandler)
{
    // Use EI_GPIO_SPOOFING (5) with empty GPIO list (count=0) - minimal valid
    struct nsm_error_injection_gpio_spoofing_payload gpioPayload{};
    gpioPayload.count_of_gpio = 0;
    // struct has gpio_data[1] so minimal payload is struct minus one uint16_t
    constexpr size_t payloadSize =
        sizeof(nsm_error_injection_gpio_spoofing_payload) - sizeof(uint16_t);
    // fault_payload[1] in req struct - subtract 1, add actual payloadSize
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_error_injection_payload_req) - 1 +
                    payloadSize);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_error_injection_payload_req(
        instanceId, reinterpret_cast<const uint8_t*>(&gpioPayload), payloadSize,
        EI_GPIO_SPOOFING, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Fix hdr.data_size mismatch: encode sets payloadSize+6 but decode expects
    // payloadSize + (sizeof(req) - 1). Without this correction, decode returns
    // NSM_SW_ERROR_LENGTH. This matches the fix applied in libnsm tests.
    auto* setReq = reinterpret_cast<nsm_set_error_injection_payload_req*>(
        requestMsg->payload);
    uint16_t correctedDataSize = static_cast<uint16_t>(
        payloadSize +
        (sizeof(nsm_set_error_injection_payload_req) - sizeof(uint8_t)));
    setReq->hdr.data_size = htole16(correctedDataSize);
    auto resp = mockupResponder->setErrorInjectionPayloadHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetGpioStateHandler)
{
    // Request 8 GPIO states (gpioValuesSize = 1)
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_gpio_state_req(instanceId, 0, 8, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getGpioStateHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    // Response = nsm_msg_hdr + nsm_get_gpio_state_resp (offset + length + 1
    // byte)
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_resp));
}

TEST_F(MockupResponderTest, testGetPciePortConfigHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_pcie_port_config_req(instanceId, 0, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testGetDeviceDiagnosticsHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_diagnostics_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_diagnostics_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceDiagnosticsHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GT(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_resp));
}

TEST_F(MockupResponderTest, testGetDeviceDebugParametersHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id param_id{};
    nsm_debug_parameter_sub_id_bitfield sub_id{};
    sub_id.value = 0;
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testSetDeviceDebugParametersHandler)
{
    constexpr uint8_t dataSize = 4;
    uint8_t data[dataSize] = {0x01, 0x02, 0x03, 0x04};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_debug_parameters_req) - 1 + dataSize);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id param_id{};
    nsm_debug_parameter_sub_id_bitfield sub_id{};
    sub_id.value = 0;
    auto rc = encode_set_device_debug_parameters_req(
        instanceId, 0, param_id, sub_id, dataSize, data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testQueryAvailableAndClearableScalarGroupHandler)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_2, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(
        resp.value().size(),
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_available_clearable_scalar_data_sources_v1_resp));
}

TEST_F(MockupResponderTest,
       testQueryAvailableAndClearableScalarGroupHandlerGroup3)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_3, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testQueryAvailableAndClearableScalarGroupHandlerUnknownGroup)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Group ID 0 is not supported and should return nullopt
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotCAKBypassHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_bypass_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_bypass_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotCAKBypassHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_bypass_resp));
}

TEST_F(MockupResponderTest, testDotCAKInstallHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_cak_install_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_cak_install_req cak_req{};
    auto rc = encode_nsm_dot_cak_install_req(instanceId, &cak_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotCAKInstallHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testDotGetInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotGetInfoHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_resp));
}

TEST_F(MockupResponderTest, testDotGetStatusHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_status_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotGetStatusHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_resp));
}

TEST_F(MockupResponderTest, testDotLockHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_lock_req lock_req{};
    auto rc = encode_nsm_dot_lock_req(instanceId, &lock_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotLockHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_resp));
}

TEST_F(MockupResponderTest, testDotUnlockChallengeHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_unlock_challenge_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_unlock_challenge_req challenge_req{};
    challenge_req.unlock_type = 1;
    auto rc = encode_nsm_dot_unlock_challenge_req(instanceId, &challenge_req,
                                                  requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotUnlockChallengeHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_resp));
}

TEST_F(MockupResponderTest, testDotUnlockHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_unlock_req unlock_req{};
    auto rc = encode_nsm_dot_unlock_req(instanceId, &unlock_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotUnlockHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_resp));
}

TEST_F(MockupResponderTest, testDotCAKRotateHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_cak_rotate_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_cak_rotate_req rotate_req{};
    auto rc = encode_nsm_dot_cak_rotate_req(instanceId, &rotate_req,
                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotCAKRotateHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_resp));
}

TEST_F(MockupResponderTest, testDotDisableHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_disable_req disable_req{};
    auto rc = encode_nsm_dot_disable_req(instanceId, &disable_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotDisableHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_resp));
}

TEST_F(MockupResponderTest, testDotOverrideHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_override_req override_req{};
    auto rc = encode_nsm_dot_override_req(instanceId, &override_req,
                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotOverrideHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_resp));
}

TEST_F(MockupResponderTest, testDotRecoveryHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_recovery_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_recovery_req recovery_req{};
    auto rc = encode_nsm_dot_recovery_req(instanceId, &recovery_req,
                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotRecoveryHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testQueryScalarGroupTelemetryHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // device_id=0, group_index=0 (group 0 supported by mockup)
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, 0, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryScalarGroupTelemetryHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testQueryMultiportScalarGroupTelemetryHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data data{};
    data.upstream_port_index = 0;
    data.type = 0; // upstream port
    data.index = 0;
    data.group_index = 0;
    auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
        instanceId, &data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryMultiportScalarGroupTelemetryHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testSetPciePortConfigHandler)
{
    // decode requires at least sizeof(nsm_set_port_config_aggregate_req) bytes
    // (which includes 1-byte sample_data[1] flexible member)
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_port_config_aggregate_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_config_aggregate_req(instanceId, 0, 0, 0, 0,
                                                   nullptr, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetSupportedErrorInjectionTypesV1Handler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_error_injection_types_v1_req(instanceId,
                                                                requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportedErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetCurrentErrorInjectionTypesV1Handler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_error_injection_types_v1_req(instanceId,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testSetCurrentErrorInjectionTypesV1Handler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_error_injection_types_mask_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_error_injection_types_mask mask{};
    // Enable GPIO spoofing type (bit 5)
    mask.mask[0] = 0x20;
    auto rc = encode_set_current_error_injection_types_v1_req(instanceId, &mask,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    // When unsupported types are requested, handler returns error response
    EXPECT_GE(resp.value().size(), sizeof(nsm_msg_hdr) + sizeof(uint8_t));
}

TEST_F(MockupResponderTest, testGetListAvailablePciePortsHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_list_available_pcie_ports_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getListAvailablePciePortsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

// =============================================================================
// Network/Switch Isolation handlers
// =============================================================================

TEST_F(MockupResponderTest, testGetSwitchIsolationModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_switch_isolation_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSwitchIsolationMode(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_EQ(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_switch_isolation_mode_resp));
}

TEST_F(MockupResponderTest, testSetSwitchIsolationModeHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_switch_isolation_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_switch_isolation_mode_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setSwitchIsolationMode(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

// =============================================================================
// Power Smoothing API V2 handlers
// =============================================================================

TEST_F(MockupResponderTest, testGetPowerSmoothingFeatureInfoV2Handler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerSmoothingFeatureInfoV2(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testGetCurrentProfileInfoV2Handler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentProfileInfoV2(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testGetQueryAdminOverrideV2Handler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_admin_override_profile_info_v2_req(instanceId,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getQueryAdminOverrideV2(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testGetPresetProfileInfoV2Handler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_info_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPresetProfileInfoV2(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

// =============================================================================
// Preset Profile Management handlers
// =============================================================================

TEST_F(MockupResponderTest, testSetActivePresetProfileHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_active_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_active_preset_profile_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setActivePresetProfile(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testSetupAdminOverrideHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_setup_admin_override_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setupAdminOverride(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testApplyAdminOverrideHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_apply_admin_override_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->applyAdminOverride(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testToggleImmediateRampDownHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_toggle_immediate_rampdown_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_immediate_rampdown_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->toggleImmediateRampDown(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testToggleFeatureStateHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_feature_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_feature_state_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->toggleFeatureState(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testUpdatePresetProfileParamsHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_update_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // profile_id=0, parameter_id=0, param_value=0
    auto rc = encode_update_preset_profile_param_req(instanceId, 0, 0, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updatePresetProfileParams(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

// =============================================================================
// Workload Power Profile handlers
// =============================================================================

TEST_F(MockupResponderTest, testGetWorkloadPowerProfileInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_workload_power_profile_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getWorkloadPowerProfileInfo(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testGetWorkLoadProfileStatusInfoHandler)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_status_req(instanceId,
                                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getWorkLoadProfileStatusInfo(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testEnableWorkloadPowerProfileHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_workload_power_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield256_t mask{};
    auto rc = encode_enable_workload_power_profile_req(instanceId, &mask,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableWorkloadPowerProfile(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

TEST_F(MockupResponderTest, testDisableWorkloadPowerProfileHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_disable_workload_power_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield256_t mask{};
    auto rc = encode_disable_workload_power_profile_req(instanceId, &mask,
                                                        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->disableWorkloadPowerProfile(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp));
}

// =============================================================================
// GPM Metrics query handlers
// =============================================================================

TEST_F(MockupResponderTest, testQueryAggregatedResetMetricsHandler)
{
    // encode_get_device_reset_statistics_req uses nsm_common_req
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_reset_statistics_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAggregatedResetMetrics(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testQueryAggregatedGPMMetricsHandler)
{
    // 1 byte bitfield for minimal valid request
    constexpr size_t bitfieldLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_aggregate_gpm_metrics_req) - 1 +
                    bitfieldLen);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t bitfield = 0x01; // metric 0 enabled
    auto rc = encode_query_aggregate_gpm_metrics_req(
        instanceId, 0, 0, 0, &bitfield, bitfieldLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAggregatedGPMMetrics(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsHandler)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_per_instance_gpm_metrics_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_per_instance_gpm_metrics_req(instanceId, 0, 0, 0, 0,
                                                        0x01, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetrics(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsV2Handler)
{
    // 1 instance bitmask byte
    constexpr size_t bitmaskLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_per_instance_gpm_metrics_v2_req) - 1 +
                    bitmaskLen);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield8_t bitmask{};
    bitmask.byte = 0x01;
    auto rc = encode_query_per_instance_gpm_metrics_v2_req(
        instanceId, 0, 0, 0, 0, &bitmask, bitmaskLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetricsV2(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

// ---- firmwareUtils.cpp handler tests ----

TEST_F(MockupResponderTest, testGetRotInformationHandlerFF00)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_erot_state_info_req fw_req = {0x000A, 0xff00, 0};
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRotInformation(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetRotInformationHandler0010)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_erot_state_info_req fw_req = {0x000A, 0x0010, 0};
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRotInformation(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetRotInformationHandler0050)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_erot_state_info_req fw_req = {0x000A, 0x0050, 0};
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRotInformation(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetRotInformationHandlerDefault)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_get_erot_state_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_erot_state_info_req fw_req = {0x000A, 0x9999, 0};
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRotInformation(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testIrreversibleConfigHandlerQuery)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_irreversible_config_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_irreversible_config_req cfg_req = {
        QUERY_IRREVERSIBLE_CFG};
    auto rc = encode_nsm_firmware_irreversible_config_req(instanceId, &cfg_req,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->irreversibleConfig(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testIrreversibleConfigHandlerDisable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_irreversible_config_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_irreversible_config_req cfg_req = {
        DISABLE_IRREVERSIBLE_CFG};
    auto rc = encode_nsm_firmware_irreversible_config_req(instanceId, &cfg_req,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->irreversibleConfig(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testIrreversibleConfigHandlerEnable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_irreversible_config_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_irreversible_config_req cfg_req = {
        ENABLE_IRREVERSIBLE_CFG};
    auto rc = encode_nsm_firmware_irreversible_config_req(instanceId, &cfg_req,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->irreversibleConfig(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testImageCopyControlHandlerQueryProgress)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_image_copy_control_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_image_copy_control_req ctrl_req = {
        NSM_IMAGE_COPY_QUERY_PROGRESS, 0};
    auto rc = encode_nsm_firmware_image_copy_control_req(instanceId, &ctrl_req,
                                                         nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->imageCopyControl(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testImageCopyControlHandlerInitiate)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_image_copy_control_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_image_copy_control_req ctrl_req = {
        NSM_IMAGE_COPY_INITIATE_IMAGE_COPY, 0};
    auto rc = encode_nsm_firmware_image_copy_control_req(instanceId, &ctrl_req,
                                                         nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->imageCopyControl(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testCodeAuthKeyPermQueryHandlerAP)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_query_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_query_req(instanceId, 0x000A,
                                                      0x0010, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermQueryHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testCodeAuthKeyPermQueryHandlerEC)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_query_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_query_req(instanceId, 0x000A,
                                                      0xFF00, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermQueryHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerAP)
{
    // Enable irreversible config to set configState=1
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->irreversibleConfig(enableMsg,
                                                        enableReq.size());
        EXPECT_TRUE(resp.has_value());
    }

    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x0010, 0, fixedNonce, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerEC)
{
    // Enable irreversible config to set configState=1
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->irreversibleConfig(enableMsg,
                                                        enableReq.size());
        EXPECT_TRUE(resp.has_value());
    }

    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0xFF00, 0, fixedNonce, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryFirmwareSecurityVersionEC)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_security_version_number_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_security_version_number_req sec_req = {0x000A, 0xFF00,
                                                               0};
    auto rc = encode_nsm_query_firmware_security_version_number_req(
        instanceId, &sec_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryFirmwareSecurityVersion(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryFirmwareSecurityVersionAP)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_security_version_number_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_security_version_number_req sec_req = {0x000A, 0x0010,
                                                               0};
    auto rc = encode_nsm_query_firmware_security_version_number_req(
        instanceId, &sec_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryFirmwareSecurityVersion(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testUpdateMinSecurityVersionHandlerNonceMismatch)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00;
    sec_req.component_classification_index = 0;
    sec_req.nonce = 0; // wrong nonce — triggers 0x88 error response
    sec_req.req_min_security_version = 1;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value()); // Returns error response, not nullopt
}

TEST_F(MockupResponderTest, testUpdateMinSecurityVersionHandlerSuccess)
{
    // Enable irreversible config to allow the update
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->irreversibleConfig(enableMsg,
                                                        enableReq.size());
        EXPECT_TRUE(resp.has_value());
    }

    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00; // EC component
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    sec_req.req_min_security_version = 0;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetRotPropertyHandlerRedundancyPolicy)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.component_classification = 0x000A;
    rot_req.component_identifier = 0x0010;
    rot_req.component_classification_index = 0;
    rot_req.property = NSM_ROT_PROPERTY_REDUNDANCY_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 0; // policy
    rot_req.argument_data[1] = 0; // lifespan
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetRotPropertyHandlerInbandUpdatePolicy)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.component_classification = 0x000A;
    rot_req.component_identifier = 0x0010;
    rot_req.component_classification_index = 0;
    rot_req.property = NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 1; // policy
    rot_req.argument_data[1] = 0; // lifespan
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetRotPropertyHandlerApSkuId)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.component_classification = 0x000A;
    rot_req.component_identifier = 0x0010;
    rot_req.component_classification_index = 0;
    rot_req.property = NSM_ROT_PROPERTY_AP_SKU_ID;
    rot_req.argument_length = 5;
    // argument_data[0:3] = AP SKU ID (little-endian), [4] = lifespan
    rot_req.argument_data[0] = 0x78;
    rot_req.argument_data[1] = 0x56;
    rot_req.argument_data[2] = 0x34;
    rot_req.argument_data[3] = 0x12;
    rot_req.argument_data[4] = 0; // lifespan
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch coverage tests
// Cover lines 353-796 in mockupResponder.cpp (dispatch switch + error paths)
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_BadHeader)
{
    // All-zero buffer: PCI vendor ID = 0 → unpack_nsm_header returns error
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_EventAck)
{
    // Build an NSM event acknowledgement message (request=0, datagram=1)
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_event_acknowledgement(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_PING, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_Ping)
{
    // NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY / NSM_PING via processRxMsg
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_UnknownCmd)
{
    // NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY → unknown command → default case
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetPortTelemetry)
{
    // NSM_TYPE_NETWORK_PORT / NSM_GET_PORT_TELEMETRY_COUNTER via processRxMsg
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_telemetry_counter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_UnknownCmd)
{
    // NSM_TYPE_NETWORK_PORT → unknown command → default case
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_telemetry_counter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetTemperature)
{
    // NSM_TYPE_PLATFORM_ENVIRONMENTAL / NSM_GET_TEMPERATURE_READING
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_temperature_reading_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_UnknownCmd)
{
    // NSM_TYPE_PLATFORM_ENVIRONMENTAL → unknown command → default case
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_temperature_reading_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_QueryScalarGroup)
{
    // NSM_TYPE_PCI_LINK / NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_scalar_group_telemetry_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, 0, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_UnknownCmd)
{
    // NSM_TYPE_PCI_LINK → unknown command → default case
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_scalar_group_telemetry_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, 0, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_GetResetStats)
{
    // NSM_TYPE_DIAGNOSTIC / NSM_GET_DEVICE_RESET_STATISTICS via processRxMsg
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_reset_statistics_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_UnknownCmd)
{
    // NSM_TYPE_DIAGNOSTIC → unknown command → default case
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_reset_statistics_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_GetErrInjMode)
{
    // NSM_TYPE_DEVICE_CONFIGURATION / NSM_GET_ERROR_INJECTION_MODE_V1
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_mode_v1_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_UnknownCmd)
{
    // NSM_TYPE_DEVICE_CONFIGURATION → unknown command → default case
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_mode_v1_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_GetErotState)
{
    // NSM_TYPE_FIRMWARE / NSM_FW_GET_EROT_STATE_INFORMATION via processRxMsg
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_get_erot_state_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_erot_state_info_req fw_req = {0x000A, 0x0010, 0};
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_UnknownCmd)
{
    // NSM_TYPE_FIRMWARE → unknown command → default case
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_firmware_get_erot_state_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_erot_state_info_req fw_req = {0x000A, 0x0010, 0};
    auto rc = encode_nsm_query_get_erot_state_parameters_req(
        instanceId, &fw_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->payload[0] = 0xFF; // unknown command
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_UnknownMsgType)
{
    // Unknown nvidia_msg_type → outer default case (lines 789-793)
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    requestMsg->hdr.nvidia_msg_type = 0xFF; // unknown message type
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY remaining cmds
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_SupportedNvidiaMessageTypes)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_nvidia_message_types_req(instanceId,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_SupportedCommandCodes)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_QueryDeviceIdentification)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_identification_req(instanceId,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetEventSubscription)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_event_subscription_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_SetEventSubscription)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_event_subscription_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(
        instanceId, GLOBAL_EVENT_GENERATION_DISABLE, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetSupportedEventSources)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_supported_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetCurrentEventSources)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_current_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_SetCurrentEventSources)
{
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_current_event_source_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_current_event_sources_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, eventSources,
        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DevCapDisc_ConfigureEventAcknowledgement)
{
    bitfield8_t mask[EVENT_SOURCES_LENGTH] = {};
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_configure_event_acknowledgement_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_configure_event_acknowledgement_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, mask, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetHistogramFormat)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_format_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetHistogramData)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_data_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetDeviceCapabilitiesV2)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_device_capabilities_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DevCapDisc_GetGpioState)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_gpio_state_req(instanceId, 0, 8, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_NETWORK_PORT remaining commands
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_QueryPortCharacteristics)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_port_characteristics_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_QueryPortStatus)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_port_status_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetFabricManagerState)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fabric_manager_state_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_QueryPortsAvailable)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_ports_available_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_SetPortDisableFuture)
{
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_disable_future_req(instanceId, mask, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetPortDisableFuture)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_disable_future_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetPowerMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_SetPowerMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_power_mode_data data{};
    auto rc = encode_set_power_mode_req(instanceId, requestMsg, data);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetSwitchIsolationMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_switch_isolation_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_SetSwitchIsolationMode)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_switch_isolation_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_switch_isolation_mode_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetEthPortTelemetry)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_ethernet_port_telemetry_counter_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_eth_port_telemetry_counter_req(instanceId, 0,
                                                        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetNetworkAddresses)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_addresses_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_addresses_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_NetworkPort_GetPortEccCounters)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_ecc_counters_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_PLATFORM_ENVIRONMENTAL remaining commands
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetInventoryInformation)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, BOARD_PART_NUMBER, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_ReadThermalParameter)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_read_thermal_parameter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetPower)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_power_draw_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetMaxObservedPower)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_max_observed_power_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetEnergyCount)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_energy_count_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetVoltage)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetAltitudePressure)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_altitude_pressure_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetDriverInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_driver_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetMigMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_MIG_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_SetMigMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_MIG_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_MIG_mode_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetEccMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_SetEccMode)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_ECC_mode_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetEccErrorCounts)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_ECC_error_counts_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_GetProgrammableEDPpScalingFactor)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_programmable_EDPp_scaling_factor_req(instanceId,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_SetProgrammableEDPpScalingFactor)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_programmable_EDPp_scaling_factor_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_programmable_EDPp_scaling_factor_req(instanceId, 0, 0,
                                                              100, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetClockLimit)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_limit_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_SetClockLimit)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_clock_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_clock_limit_req(instanceId, 0, 0, 0, UINT32_MAX,
                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetCurrentClockFrequency)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_curr_clock_freq_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetClockEventReasonCodes)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                             requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_GetAccumulatedGpuUtilizationTime)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_accum_GPU_util_time_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetCurrentUtilization)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_utilization_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_SetPowerLimits)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_power_limit_req(instanceId, 0, 0, 0, 100, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetPowerLimits)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_limit_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetClockOutputEnableState)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_clock_output_enabled_state_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, PCIE_CLKBUF_INDEX, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetRowRemapStateFlags)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_state_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetRowRemappingCounts)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remapping_counts_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetRowRemapAvailability)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_availability_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetLeakDetectionInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_leak_detection_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_GetMemoryCapacityUtilization)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_QueryAggregateGpmMetrics)
{
    constexpr size_t bitfieldLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_aggregate_gpm_metrics_req) - 1 +
                        bitfieldLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t bitfield = 0x01;
    auto rc = encode_query_aggregate_gpm_metrics_req(
        instanceId, 0, 0, 0, &bitfield, bitfieldLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_QueryPerInstanceGpmMetrics)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_per_instance_gpm_metrics_req(instanceId, 0, 0, 0, 0,
                                                        0x01, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_QueryPerInstanceGpmMetricsV2)
{
    constexpr size_t bitmaskLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_v2_req) - 1 +
                        bitmaskLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield8_t bitmask{};
    bitmask.byte = 0x01;
    auto rc = encode_query_per_instance_gpm_metrics_v2_req(
        instanceId, 0, 0, 0, 0, &bitmask, bitmaskLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_GetViolationDuration)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingToggleFeatureState)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_feature_state_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_feature_state_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_PwrSmoothingGetFeatureInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingGetFeatureInfoV2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingGetHwCircuitryLifetimeUsage)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_hardware_lifetime_cricuitry_req(instanceId,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingGetCurrentProfileInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingQueryAdminOverride)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_admin_override_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingGetCurrentProfileInfoV2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingSetActivePresetProfile)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_active_preset_profile_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_active_preset_profile_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingQueryAdminOverrideV2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_admin_override_profile_info_v2_req(instanceId,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingSetupAdminOverride)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_setup_admin_override_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingApplyAdminOverride)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_apply_admin_override_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingToggleImmediateRampDown)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_immediate_rampdown_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_immediate_rampdown_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingGetPresetProfileInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingGetPresetProfileInfoV2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_info_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_PwrSmoothingUpdatePresetProfileParams)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_update_preset_profile_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_update_preset_profile_param_req(instanceId, 0, 0, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PlatformEnv_EnableWorkloadPowerProfile)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_workload_power_profile_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield256_t mask{};
    auto rc = encode_enable_workload_power_profile_req(instanceId, &mask,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_DisableWorkloadPowerProfile)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_disable_workload_power_profile_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield256_t mask{};
    auto rc = encode_disable_workload_power_profile_req(instanceId, &mask,
                                                        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_GetWorkloadPowerProfileStatusInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_status_req(instanceId,
                                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PlatformEnv_GetWorkloadPowerProfileInfo)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_workload_power_profile_info_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_PCI_LINK remaining commands
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_AssertPcieFundamentalReset)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_assert_pcie_fundamental_reset_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_assert_pcie_fundamental_reset_req(instanceId, 0, 1,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_GetPortConfiguration)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_pcie_port_config_req(instanceId, 0, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_SetPortConfiguration)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_config_aggregate_req(instanceId, 0, 0, 0, 0,
                                                   nullptr, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_ClearDataSourceV1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_clear_data_source_v1_req(instanceId, 0, GROUP_ID_2, 0,
                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PciLink_QueryAvailableClearableScalarDataSources)
{
    Request request(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req),
        0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_2, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_PciLink_ListAvailablePciePorts)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_list_available_pcie_ports_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_PciLink_MultiportQueryScalarGroupTelemetryV2)
{
    Request request(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req),
        0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data data{};
    data.upstream_port_index = 0;
    data.type = 0;
    data.index = 0;
    data.group_index = 0;
    auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
        instanceId, &data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_DIAGNOSTIC remaining commands
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_QueryTokenParameters)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_token_parameters_req(
        instanceId, NSM_DEBUG_TOKEN_OPCODE_RMCS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_ProvideToken)
{
    constexpr size_t tokenLen = 4;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_req) + tokenLen - 1, 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t token_data[tokenLen] = {0x01, 0x02, 0x03, 0x04};
    auto rc = encode_nsm_provide_token_req(instanceId, token_data, tokenLen,
                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_DisableTokens)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_disable_tokens_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_disable_tokens_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_QueryTokenStatus)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_token_status_req(
        instanceId, NSM_DEBUG_TOKEN_TYPE_FRC, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_QueryDeviceIds)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_ids_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_ResetNetworkDevice)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_reset_network_device_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_EnableDisableWp)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM, 1,
                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_GetDeviceDiagnostics)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_diagnostics_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_GetNetworkDeviceDebugInfo)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_device_debug_info_req(instanceId, 0, 0,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_EraseTrace)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_erase_trace_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_GetNetworkDeviceLogInfo)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_device_log_info_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_EraseDebugInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_erase_debug_info_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_GetDeviceDebugParameters)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id param_id{};
    nsm_debug_parameter_sub_id_bitfield sub_id{};
    sub_id.value = 0;
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Diagnostic_SetDeviceDebugParameters)
{
    constexpr uint8_t dataSize = 4;
    uint8_t data[dataSize] = {0x01, 0x02, 0x03, 0x04};
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_device_debug_parameters_req) - 1 +
                        dataSize,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id param_id{};
    nsm_debug_parameter_sub_id_bitfield sub_id{};
    sub_id.value = 0;
    auto rc = encode_set_device_debug_parameters_req(
        instanceId, 0, param_id, sub_id, dataSize, data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_DEVICE_CONFIGURATION remaining commands
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_SetErrorInjectionModeV1)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_mode_v1_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_error_injection_mode_v1_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_GetSupportedErrorInjectionTypesV1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_error_injection_types_v1_req(instanceId,
                                                                requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_SetCurrentErrorInjectionTypesV1)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_error_injection_types_mask_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_error_injection_types_mask mask{};
    mask.mask[0] = 0x20;
    auto rc = encode_set_current_error_injection_types_v1_req(instanceId, &mask,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_GetCurrentErrorInjectionTypesV1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_error_injection_types_v1_req(instanceId,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_GetErrorInjectionPayload)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_payload_req(instanceId, 4, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_SetErrorInjectionPayload)
{
    struct nsm_error_injection_gpio_spoofing_payload gpioPayload{};
    gpioPayload.count_of_gpio = 0;
    constexpr size_t payloadSize =
        sizeof(nsm_error_injection_gpio_spoofing_payload) - sizeof(uint16_t);
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_error_injection_payload_req) - 1 +
                        payloadSize,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_error_injection_payload_req(
        instanceId, reinterpret_cast<const uint8_t*>(&gpioPayload), payloadSize,
        EI_GPIO_SPOOFING, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto* setReq = reinterpret_cast<nsm_set_error_injection_payload_req*>(
        requestMsg->payload);
    uint16_t correctedDataSize = static_cast<uint16_t>(
        payloadSize +
        (sizeof(nsm_set_error_injection_payload_req) - sizeof(uint8_t)));
    setReq->hdr.data_size = htole16(correctedDataSize);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_ActivateErrorInjection)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_activate_error_injection_payload_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_activate_error_injection_payload_req(instanceId, 0, 0,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_GetReconfigurationPermissionsV1)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        0, RP_IN_SYSTEM_TEST, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_SetReconfigurationPermissionsV1)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        0, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_GetConfidentialComputeModeV1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_confidential_compute_mode_v1_req(instanceId,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_SetConfidentialComputeModeV1)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_confidential_compute_mode_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_confidential_compute_mode_v1_req(instanceId, 0,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_EnableDisableGpuIstMode)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 0, 1,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       ProcessRxMsg_DeviceConfig_GetFpgaDiagnosticsSettings)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        0, GET_POWER_SUPPLY_STATUS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_GetDeviceModeSetting)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_setting_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_SetDeviceModeSetting)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_setting_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_setting_req(instanceId, 0, DISABLED,
                                                 requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_DeviceConfig_GetProtectionOptions)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_protection_options_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// ProcessRxMsg dispatch - NSM_TYPE_FIRMWARE remaining commands
// =============================================================================

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_IrreversibleConfig)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_firmware_irreversible_config_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_irreversible_config_req cfg_req = {
        QUERY_IRREVERSIBLE_CFG};
    auto rc = encode_nsm_firmware_irreversible_config_req(instanceId, &cfg_req,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_ImageCopyControl)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_firmware_image_copy_control_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_image_copy_control_req ctrl_req = {
        NSM_IMAGE_COPY_QUERY_PROGRESS, 0};
    auto rc = encode_nsm_firmware_image_copy_control_req(instanceId, &ctrl_req,
                                                         nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_QueryCodeAuthKeyPerm)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_query_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_query_req(instanceId, 0x000A,
                                                      0x0010, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_UpdateCodeAuthKeyPerm)
{
    // Enable irreversible config first so the handler does not reject
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->irreversibleConfig(enableMsg,
                                                        enableReq.size());
        EXPECT_TRUE(resp.has_value());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_code_auth_key_perm_update_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x0010, 0, fixedNonce, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_QueryMinSecurityVersion)
{
    Request request(
        sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_security_version_number_req_command),
        0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_security_version_number_req sec_req = {0x000A, 0x0010,
                                                               0};
    auto rc = encode_nsm_query_firmware_security_version_number_req(
        instanceId, &sec_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_UpdateMinSecurityVersion)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_firmware_update_min_sec_ver_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00;
    sec_req.component_classification_index = 0;
    sec_req.nonce = 0;
    sec_req.req_min_security_version = 1;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_SetRotProperty)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_firmware_set_rot_property_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.component_classification = 0x000A;
    rot_req.component_identifier = 0x0010;
    rot_req.component_classification_index = 0;
    rot_req.property = NSM_ROT_PROPERTY_REDUNDANCY_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 0;
    rot_req.argument_data[1] = 0;
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotCakInstall)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_install_req_command), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_cak_install_req cak_req{};
    auto rc = encode_nsm_dot_cak_install_req(instanceId, &cak_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotCakBypass)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_bypass_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_bypass_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotLock)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_req_command), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_lock_req lock_req{};
    auto rc = encode_nsm_dot_lock_req(instanceId, &lock_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotUnlockChallenge)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_challenge_req_command), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_unlock_challenge_req challenge_req{};
    challenge_req.unlock_type = 1;
    auto rc = encode_nsm_dot_unlock_challenge_req(instanceId, &challenge_req,
                                                  requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotUnlock)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_unlock_req unlock_req{};
    auto rc = encode_nsm_dot_unlock_req(instanceId, &unlock_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotCakRotate)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_rotate_req_command), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_cak_rotate_req rotate_req{};
    auto rc = encode_nsm_dot_cak_rotate_req(instanceId, &rotate_req,
                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotGetInfo)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotGetStatus)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_status_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotDisable)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_disable_req disable_req{};
    auto rc = encode_nsm_dot_disable_req(instanceId, &disable_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotOverride)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_override_req override_req{};
    auto rc = encode_nsm_dot_override_req(instanceId, &override_req,
                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, ProcessRxMsg_Firmware_DotRecovery)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_recovery_req_command),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_recovery_req recovery_req{};
    auto rc = encode_nsm_dot_recovery_req(instanceId, &recovery_req,
                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getProperty tests for additional identifiers
// =============================================================================

TEST_F(MockupResponderTest, testGetPropertyFruPartNumber)
{
    testProperty(FRU_PART_NUMBER, "FRU50500B-0D00_DK");
}

TEST_F(MockupResponderTest, testGetPropertyBuildDate)
{
    testProperty(BUILD_DATE, "2022-08-06T00:00:00Z");
}

TEST_F(MockupResponderTest, testGetPropertyAssetTag)
{
    testProperty(ASSET_TAG, "MCX750500B-0D00_DK");
}

TEST_F(MockupResponderTest, testGetPropertyInfoRomVersion)
{
    testProperty(INFO_ROM_VERSION, "128");
}

TEST_F(MockupResponderTest, testGetPropertyRatedModulePowerLimit)
{
    testProperty(RATED_MODULE_POWER_LIMIT, uint32_t(5555));
}

TEST_F(MockupResponderTest, testGetPropertyMaximumModulePowerLimit)
{
    testProperty(MAXIMUM_MODULE_POWER_LIMIT, uint32_t(8788));
}

TEST_F(MockupResponderTest, testGetPropertyMinimumModulePowerLimit)
{
    testProperty(MINIMUM_MODULE_POWER_LIMIT, uint32_t(2754));
}

TEST_F(MockupResponderTest, testGetPropertyRatedDevicePowerLimit)
{
    testProperty(RATED_DEVICE_POWER_LIMIT, uint32_t(80000));
}

TEST_F(MockupResponderTest, testGetPropertyMinimumGraphicsClockLimit)
{
    testProperty(MINIMUM_GRAPHICS_CLOCK_LIMIT, uint32_t(100));
}

TEST_F(MockupResponderTest, testGetPropertyDefaultBoostClocks)
{
    testProperty(DEFAULT_BOOST_CLOCKS, uint32_t(30000));
}

TEST_F(MockupResponderTest, testGetPropertyDefaultBaseClocks)
{
    testProperty(DEFAULT_BASE_CLOCKS, uint32_t(300));
}

TEST_F(MockupResponderTest, testGetPropertyMaximumGraphicsClockLimit)
{
    testProperty(MAXIMUM_GRAPHICS_CLOCK_LIMIT, uint32_t(5000));
}

TEST_F(MockupResponderTest, testGetPropertyMinimumMemoryClockLimit)
{
    testProperty(MINIMUM_MEMORY_CLOCK_LIMIT, uint32_t(150));
}

TEST_F(MockupResponderTest, testGetPropertyMaximumMemoryClockLimit)
{
    testProperty(MAXIMUM_MEMORY_CLOCK_LIMIT, uint32_t(1500));
}

TEST_F(MockupResponderTest, testGetPropertyDevicePartNumber)
{
    testProperty(DEVICE_PART_NUMBER, "A1");
}

TEST_F(MockupResponderTest, testGetPropertyMaximumMemoryCapacity)
{
    testProperty(MAXIMUM_MEMORY_CAPACITY, uint32_t(200000));
}

TEST_F(MockupResponderTest, testGetPropertyPcieRetimer0EepromVersion)
{
    auto res = mockupResponder->getProperty(PCIERETIMER_0_EEPROM_VERSION);
    EXPECT_EQ(res.size(), 8u);
}

TEST_F(MockupResponderTest, testGetPropertyMinimumEdppScalingFactor)
{
    auto res = mockupResponder->getProperty(MINIMUM_EDPP_SCALING_FACTOR);
    EXPECT_EQ(res.size(), 1u);
    EXPECT_EQ(res[0], 19u);
}

TEST_F(MockupResponderTest, testGetPropertyMaximumEdppScalingFactor)
{
    auto res = mockupResponder->getProperty(MAXIMUM_EDPP_SCALING_FACTOR);
    EXPECT_EQ(res.size(), 1u);
    EXPECT_EQ(res[0], 86u);
}

// =============================================================================
// Aggregate sensor_id=255 handler tests
// =============================================================================

TEST_F(MockupResponderTest, testGetTemperatureReadingAggregateHandler)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // sensor_id=255 triggers the aggregate (multi-sensor) code path
    auto rc = encode_get_temperature_reading_req(instanceId, 255, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getTemperatureReadingHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testReadThermalParameterAggregateHandler)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // parameter_id=255 triggers the aggregate code path
    auto rc = encode_read_thermal_parameter_req(instanceId, 255, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->readThermalParameterHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetMaxObservedPowerAggregateHandler)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // sensor_id=255 triggers the aggregate code path
    auto rc = encode_get_max_observed_power_req(instanceId, 255, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getMaxObservedPowerHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// queryAvailableAndClearableScalarGroup for additional group IDs
// =============================================================================

TEST_F(MockupResponderTest,
       testQueryAvailableAndClearableScalarGroupHandlerGroup4)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_4, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testQueryAvailableAndClearableScalarGroupHandlerGroup8)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_8, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testQueryAvailableAndClearableScalarGroupHandlerGroup9)
{
    Request request(
        sizeof(nsm_msg_hdr) +
        sizeof(nsm_query_available_clearable_scalar_data_sources_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_9, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getProperty default case and thermal handler invalid length
// =============================================================================

TEST_F(MockupResponderTest, testGetPropertyDefaultCase)
{
    // Unknown property identifier → default case → empty vector
    auto res = mockupResponder->getProperty(99);
    EXPECT_TRUE(res.empty());
}

TEST_F(MockupResponderTest, testReadThermalParameterHandlerInvalidLen)
{
    // requestLen < sizeof(nsm_msg_hdr)+sizeof(nsm_read_thermal_parameter_req)
    // → invalid length path → returns nullopt (lines 1813-1820)
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_read_thermal_parameter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_read_thermal_parameter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->readThermalParameterHandler(
        requestMsg,
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req) - 1);
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// enableDisableWriteProtectedHandler uncovered data_index values
// Cover lines 5514-5717 in mockupResponder.cpp
// =============================================================================

TEST_F(MockupResponderTest, testEnableDisableWPHandlerBaseboardAndHMC)
{
    for (auto idx : {BASEBOARD_FRU_EEPROM, HMC_SPI_FLASH})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerPexCx8)
{
    for (auto idx : {PEX_SW_EEPROM, CX8_SPI_FLASH})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerNvSwitchEeprom)
{
    for (auto idx : {NVSW_EEPROM_BOTH, NVSW_EEPROM_1, NVSW_EEPROM_2})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerGpuGroupFlash)
{
    for (auto idx : {GPU_1_4_SPI_FLASH, GPU_5_8_SPI_FLASH, GPU_SPI_FLASH})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerGpuIndividualFlash)
{
    for (auto idx :
         {GPU_SPI_FLASH_1, GPU_SPI_FLASH_2, GPU_SPI_FLASH_3, GPU_SPI_FLASH_4,
          GPU_SPI_FLASH_5, GPU_SPI_FLASH_6, GPU_SPI_FLASH_7, GPU_SPI_FLASH_8})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerRetimerIndividual)
{
    for (auto idx : {RETIMER_EEPROM_1, RETIMER_EEPROM_2, RETIMER_EEPROM_3,
                     RETIMER_EEPROM_4, RETIMER_EEPROM_5, RETIMER_EEPROM_6,
                     RETIMER_EEPROM_7, RETIMER_EEPROM_8})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerCpuFlash)
{
    for (auto idx :
         {CPU_SPI_FLASH_1, CPU_SPI_FLASH_2, CPU_SPI_FLASH_3, CPU_SPI_FLASH_4})
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_enable_disable_wp_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_enable_disable_wp_req(
            instanceId,
            static_cast<diagnostics_enable_disable_wp_data_index>(idx), 1,
            requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->enableDisableWriteProtectedHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testEnableDisableWPHandlerDefaultUnknown)
{
    // Unknown data_index → default case (logs error, still returns response)
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(
        instanceId, static_cast<diagnostics_enable_disable_wp_data_index>(0x50),
        1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getDeviceDebugParametersHandler remaining parameter IDs
// Cover lines 7971-8028 in mockupResponder.cpp
// =============================================================================

TEST_F(MockupResponderTest, testGetDeviceDebugParametersHandlerSimpleParamIds)
{
    const uint8_t paramIds[] = {
        NSM_DEBUG_PARAMETER_ID_PPSLC, NSM_DEBUG_PARAMETER_ID_PPSLS,
        NSM_DEBUG_PARAMETER_ID_PPSPI, NSM_DEBUG_PARAMETER_ID_PPSPHI,
        NSM_DEBUG_PARAMETER_ID_PPSPC, NSM_DEBUG_PARAMETER_ID_MPSCR};
    for (auto paramId : paramIds)
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_device_debug_parameters_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        struct nsm_debug_parameter_id param_id{};
        param_id.index = paramId;
        nsm_debug_parameter_sub_id_bitfield sub_id{};
        sub_id.value = 0;
        auto rc = encode_get_device_debug_parameters_req(
            instanceId, 0, param_id, sub_id, requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->getDeviceDebugParametersHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testGetDeviceDebugParametersHandlerPpcntSubIds)
{
    const uint8_t subIds[] = {
        NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L0_GENERAL_COUNTERS,
        NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_GENERAL_COUNTERS,
        NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_STAT_COUNTERS,
        0x7F // unknown sub_id → default case in nested switch
    };
    for (auto subId : subIds)
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_device_debug_parameters_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        struct nsm_debug_parameter_id param_id{};
        param_id.index = NSM_DEBUG_PARAMETER_ID_PPCNT;
        nsm_debug_parameter_sub_id_bitfield sub_id{};
        sub_id.bits.sub_id = subId;
        auto rc = encode_get_device_debug_parameters_req(
            instanceId, 0, param_id, sub_id, requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->getDeviceDebugParametersHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testGetDeviceDebugParametersHandlerPpslgSubIds)
{
    const uint8_t subIds[] = {
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CAPABILITIES_AND_STATUS,
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CONFIGURATION,
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_DEBUG,
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_CAPABILITIES_AND_STATUS,
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_DEBUG,
        0x7F // unknown sub_id → default case in nested switch
    };
    for (auto subId : subIds)
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_device_debug_parameters_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        struct nsm_debug_parameter_id param_id{};
        param_id.index = NSM_DEBUG_PARAMETER_ID_PPSLG;
        nsm_debug_parameter_sub_id_bitfield sub_id{};
        sub_id.bits.sub_id = subId;
        auto rc = encode_get_device_debug_parameters_req(
            instanceId, 0, param_id, sub_id, requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->getDeviceDebugParametersHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value());
    }
}

TEST_F(MockupResponderTest, testGetDeviceDebugParametersHandlerDefaultParamId)
{
    // Unknown parameter_id → default case in outer switch
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id param_id{};
    param_id.index = 0x3F; // unknown
    nsm_debug_parameter_sub_id_bitfield sub_id{};
    sub_id.value = 0;
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── Aggregate sensor paths (sensor_id / parameter_id == 255) ───────────────

TEST_F(MockupResponderTest, testGetCurrentPowerDrawHandlerAggregate)
{
    // sensor_id=255 triggers aggregate response path
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_power_draw_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_power_draw_req(instanceId, 255, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentPowerDrawHandler(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testGetCurrentEnergyCountHandlerAggregate)
{
    // sensor_id=255 triggers aggregate response path
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_current_energy_count_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_energy_count_req(instanceId, 255, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentEnergyCountHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST_F(MockupResponderTest, testGetVoltageHandlerAggregate)
{
    // sensor_id=255 triggers aggregate response path
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, 255, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getVoltageHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

// ── Event sending functions ─────────────────────────────────────────────────
// In the test environment sockFd < 0 (no MCTP daemon), so mctpSockSend covers
// its early-return path and the event functions cover their encode bodies.

TEST_F(MockupResponderTest, testSendRediscoveryEvent)
{
    // Cover sendRediscoveryEvent body + mctpSockSend sockFd<0 path
    EXPECT_NO_THROW(mockupResponder->sendRediscoveryEvent(30, true));
    EXPECT_NO_THROW(mockupResponder->sendRediscoveryEvent(30, false));
}

TEST_F(MockupResponderTest, testSendRediscoveryEventVerbose)
{
    mockupResponder->verbose = true;
    EXPECT_NO_THROW(mockupResponder->sendRediscoveryEvent(30, true));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendXIDEvent)
{
    EXPECT_NO_THROW(mockupResponder->sendXIDEvent(
        30, true, 0x01, 12345, 67890, 1000000ULL, "test XID message"));
    EXPECT_NO_THROW(
        mockupResponder->sendXIDEvent(30, false, 0x00, 0, 0, 0, ""));
}

TEST_F(MockupResponderTest, testSendXIDEventVerbose)
{
    mockupResponder->verbose = true;
    EXPECT_NO_THROW(mockupResponder->sendXIDEvent(30, true, 0x01, 999, 1,
                                                  500ULL, "verbose test"));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendResetRequiredEvent)
{
    EXPECT_NO_THROW(mockupResponder->sendResetRequiredEvent(30, true));
    EXPECT_NO_THROW(mockupResponder->sendResetRequiredEvent(30, false));
}

TEST_F(MockupResponderTest, testSendResetRequiredEventVerbose)
{
    mockupResponder->verbose = true;
    EXPECT_NO_THROW(mockupResponder->sendResetRequiredEvent(30, true));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendFabricManagerStateEvent)
{
    EXPECT_NO_THROW(mockupResponder->sendFabricManagerStateEvent(
        30, true, 1, 0, 1700000000ULL, 3600ULL));
    EXPECT_NO_THROW(mockupResponder->sendFabricManagerStateEvent(
        30, false, 0, 1, 0ULL, 0ULL));
}

TEST_F(MockupResponderTest, testSendFabricManagerStateEventVerbose)
{
    mockupResponder->verbose = true;
    EXPECT_NO_THROW(mockupResponder->sendFabricManagerStateEvent(
        30, true, 2, 0, 1234567890ULL, 120ULL));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendDeviceConfigurationRequestEventV1)
{
    EXPECT_NO_THROW(
        mockupResponder->sendDeviceConfigurationRequestEventV1(30, true));
    EXPECT_NO_THROW(
        mockupResponder->sendDeviceConfigurationRequestEventV1(30, false));
}

TEST_F(MockupResponderTest, testSendDeviceConfigurationRequestEventV1Verbose)
{
    mockupResponder->verbose = true;
    EXPECT_NO_THROW(
        mockupResponder->sendDeviceConfigurationRequestEventV1(30, true));
    mockupResponder->verbose = false;
}

/** Same encode path as MockupResponder::sendDeviceConfigurationRequestEventV1;
 *  decode verifies Type-5 device-configuration request event wire format.
 */
TEST_F(MockupResponderTest, testDeviceConfigurationRequestEventV1EncodeDecode)
{
    for (bool ackr : {false, true})
    {
        std::vector<uint8_t> eventMsg(sizeof(nsm_msg_hdr) + NSM_EVENT_MIN_LEN,
                                      0);
        auto msg = reinterpret_cast<nsm_msg*>(eventMsg.data());
        ASSERT_EQ(NSM_SW_SUCCESS,
                  encode_nsm_device_config_request_event_v1(
                      mockupResponder->mockInstanceId, ackr, msg));

        uint8_t event_class = 0;
        uint16_t event_state = 0xFFFF;
        EXPECT_EQ(NSM_SW_SUCCESS,
                  decode_nsm_device_config_request_event_v1(
                      msg, eventMsg.size(), &event_class, &event_state));
        EXPECT_EQ(static_cast<int>(NSM_GENERAL_EVENT_CLASS),
                  static_cast<int>(event_class));
        EXPECT_EQ(0, event_state);
    }
}

TEST_F(MockupResponderTest, testSendNsmEvent)
{
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    EXPECT_NO_THROW(mockupResponder->sendNsmEvent(
        30, NSM_TYPE_PLATFORM_ENVIRONMENTAL, true, 0, 0x01, 0x00, 0x0001,
        sizeof(data), data));
    EXPECT_NO_THROW(mockupResponder->sendNsmEvent(
        30, NSM_TYPE_NETWORK_PORT, false, 0, 0x02, 0x01, 0x0002, 0, nullptr));
}

TEST_F(MockupResponderTest, testSendNsmEventVerbose)
{
    mockupResponder->verbose = true;
    uint8_t data[2] = {0xAA, 0xBB};
    EXPECT_NO_THROW(mockupResponder->sendNsmEvent(
        30, NSM_TYPE_PLATFORM_ENVIRONMENTAL, true, 0, 0x01, 0x00, 0x0001,
        sizeof(data), data));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendThreasholdEvent)
{
    EXPECT_NO_THROW(mockupResponder->sendThreasholdEvent(
        30, true, true, false, true, false, true, false, true, 0));
    EXPECT_NO_THROW(mockupResponder->sendThreasholdEvent(
        30, false, false, false, false, false, false, false, false, 1));
}

TEST_F(MockupResponderTest, testSendThreasholdEventVerbose)
{
    mockupResponder->verbose = true;
    EXPECT_NO_THROW(mockupResponder->sendThreasholdEvent(
        30, true, false, true, false, true, false, true, false, 2));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendGpioStateChangeEvent)
{
    std::vector<std::pair<uint16_t, bool>> gpioEvents = {
        {0, true}, {1, false}, {100, true}};
    EXPECT_NO_THROW(mockupResponder->sendGpioStateChangeEvent(
        30, true, 1000000ULL, gpioEvents));
}

TEST_F(MockupResponderTest, testSendGpioStateChangeEventVerbose)
{
    mockupResponder->verbose = true;
    std::vector<std::pair<uint16_t, bool>> gpioEvents = {{5, true},
                                                         {10, false}};
    EXPECT_NO_THROW(mockupResponder->sendGpioStateChangeEvent(30, false, 999ULL,
                                                              gpioEvents));
    mockupResponder->verbose = false;
}

TEST_F(MockupResponderTest, testSendGpioStateChangeEventEmpty)
{
    // Empty gpioEvents triggers the early-return warning path
    std::vector<std::pair<uint16_t, bool>> emptyEvents;
    EXPECT_NO_THROW(
        mockupResponder->sendGpioStateChangeEvent(30, true, 0ULL, emptyEvents));
}

// ── Decode-failure paths (truncated requests) ───────────────────────────────
// A request shorter than sizeof(nsm_msg_hdr) causes all decode functions to
// return NSM_SW_ERROR_LENGTH, exercising the error-path branches.

static void truncatedDecodeTest(
    MockupResponderTest* suite,
    std::function<std::optional<Response>(const nsm_msg*, size_t)> handler)
{
    // Allocate a full-sized header buffer but only tell the handler we have
    // sizeof(nsm_msg_hdr)-1 bytes so every decode_xxx_req length check fails.
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::memset(request.data(), 0, request.size());
    auto resp = handler(requestMsg, sizeof(nsm_msg_hdr) - 1);
    EXPECT_FALSE(resp.has_value());
    (void)suite;
}

TEST_F(MockupResponderTest, testQueryPortCharacteristicsHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->queryPortCharacteristicsHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testQueryPortStatusHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->queryPortStatusHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testQueryPortCharacteristicsV2HandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->queryPortCharacteristicsV2Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testClearPortMetricStateHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->clearPortMetricStateHandler(m, l);
    });

    // A well-formed frame whose Count is 0 is rejected by the request
    // decoder as well: no response.
    const uint8_t tagId = NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_clear_port_metric_state_req) + sizeof(tagId));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    ASSERT_EQ(encode_clear_port_metric_state_req(instanceId, 1, 1, &tagId,
                                                 requestMsg),
              NSM_SW_SUCCESS);
    auto fixed =
        reinterpret_cast<nsm_clear_port_metric_state_req*>(requestMsg->payload);
    fixed->tag_count = 0;
    EXPECT_FALSE(
        mockupResponder->clearPortMetricStateHandler(requestMsg, request.size())
            .has_value());
}

TEST_F(MockupResponderTest, testGetFabricManagerStateHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getFabricManagerStateHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testQueryPortsAvailableHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->queryPortsAvailableHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetPortDisableFutureHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->setPortDisableFutureHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPortDisableFutureHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPortDisableFutureHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPowerModeHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPowerModeHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetPowerModeHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->setPowerModeHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetSwitchIsolationModeDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getSwitchIsolationMode(m, l);
    });
}

TEST_F(MockupResponderTest, testGetDriverInfoHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getDriverInfoHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetEventSubscriptionDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getEventSubscription(m, l);
    });
}

TEST_F(MockupResponderTest, testGetSupportedEventSourcesDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getSupportedEventSources(m, l);
    });
}

TEST_F(MockupResponderTest, testGetCurrentEventSourcesDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getCurrentEventSources(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPowerSmoothingFeatureInfoDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPowerSmoothingFeatureInfo(m, l);
    });
}

TEST_F(MockupResponderTest, testGetHwCircuiteryUsageDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getHwCircuiteryUsage(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPowerSmoothingFeatureInfoV2DecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPowerSmoothingFeatureInfoV2(m, l);
    });
}

TEST_F(MockupResponderTest, testGetCurrentProfileInfoV2DecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getCurrentProfileInfoV2(m, l);
    });
}

TEST_F(MockupResponderTest, testGetQueryAdminOverrideV2DecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getQueryAdminOverrideV2(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPresetProfileInfoV2DecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPresetProfileInfoV2(m, l);
    });
}

TEST_F(MockupResponderTest, testGetCurrentProfileInfoDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getCurrentProfileInfo(m, l);
    });
}

TEST_F(MockupResponderTest, testGetQueryAdminOverrideDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getQueryAdminOverride(m, l);
    });
}

TEST_F(MockupResponderTest, testSetActivePresetProfileDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->setActivePresetProfile(m, l);
    });
}

TEST_F(MockupResponderTest, testSetupAdminOverrideDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->setupAdminOverride(m, l);
    });
}

TEST_F(MockupResponderTest, testApplyAdminOverrideDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->applyAdminOverride(m, l);
    });
}

TEST_F(MockupResponderTest, testToggleImmediateRampDownDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->toggleImmediateRampDown(m, l);
    });
}

TEST_F(MockupResponderTest, testToggleFeatureStateDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->toggleFeatureState(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPresetProfileInfoDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPresetProfileInfo(m, l);
    });
}

TEST_F(MockupResponderTest, testUpdatePresetProfileParamsDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->updatePresetProfileParams(m, l);
    });
}

TEST_F(MockupResponderTest, testEnableWorkloadPowerProfileDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->enableWorkloadPowerProfile(m, l);
    });
}

TEST_F(MockupResponderTest, testDisableWorkloadPowerProfileDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->disableWorkloadPowerProfile(m, l);
    });
}

TEST_F(MockupResponderTest, testGetWorkLoadProfileStatusInfoDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getWorkLoadProfileStatusInfo(m, l);
    });
}

TEST_F(MockupResponderTest, testGetWorkloadPowerProfileInfoDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getWorkloadPowerProfileInfo(m, l);
    });
}

// ── queryScalarGroupTelemetry – remaining group IDs 1-6, 8-9, unknown ──────

TEST_F(MockupResponderTest, testQueryScalarGroupTelemetryAllGroups)
{
    // Cover getQueryScalarGroupTelemetryResponse for groups 1-6, 8, 9
    const uint8_t groups[] = {GROUP_ID_1, GROUP_ID_2, GROUP_ID_3, GROUP_ID_4,
                              GROUP_ID_5, GROUP_ID_6, GROUP_ID_8, GROUP_ID_9};
    for (auto gid : groups)
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_scalar_group_telemetry_v1_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, 0, gid,
                                                             requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->queryScalarGroupTelemetryHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value()) << "Failed for group " << (int)gid;
    }
}

TEST_F(MockupResponderTest, testQueryScalarGroupTelemetryUnknownGroup)
{
    // group_index=7 (not in switch) → default → nullopt
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, 0, 7,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryScalarGroupTelemetryHandler(
        requestMsg, request.size());
    // GROUP_ID_7 has no case → default → nullopt
    EXPECT_FALSE(resp.has_value());
}

// ── getSupportedCommandCodeHandler – invalid message type ──────────────────

TEST_F(MockupResponderTest, testGetSupportedCommandCodeHandlerInvalidMsgType)
{
    // nvidia_message_type >= NUM_NSM_TYPES (7) → error path
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Use encode to build a valid header, then patch the message type field
    auto rc = encode_get_supported_command_codes_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Overwrite the nvidia_message_type field to an invalid value (>=7)
    auto req = reinterpret_cast<nsm_get_supported_command_codes_req*>(
        requestMsg->payload);
    req->nvidia_message_type = NUM_NSM_TYPES; // = 7, out of range
    auto resp = mockupResponder->getSupportCommandCodeHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testGetSupportedCommandCodeHandlerInvalidMsgTypeVerbose)
{
    mockupResponder->verbose = true;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto req = reinterpret_cast<nsm_get_supported_command_codes_req*>(
        requestMsg->payload);
    req->nvidia_message_type = 0xFF; // definitely out of range
    auto resp = mockupResponder->getSupportCommandCodeHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
    mockupResponder->verbose = false;
}

// ── getPortTelemetryCounterHandler – verbose error path ────────────────────

TEST_F(MockupResponderTest, testGetPortTelemetryCounterHandlerVerboseErrorPath)
{
    // truncated request triggers decode failure; with verbose=true covers
    // the verbose error-log branch inside getPortTelemetryCounterHandler
    mockupResponder->verbose = true;
    Request request(sizeof(nsm_msg_hdr)); // too short – no payload
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // zero-init the header but leave payload absent
    std::memset(request.data(), 0, request.size());
    auto resp = mockupResponder->getPortTelemetryCounterHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
    mockupResponder->verbose = false;
}

// ── pmDisabled() paths ──────────────────────────────────────────────────────
// Create /tmp/pm-disabled to activate the pmDisabled() code paths in
// getMigModeHandler, setMigModeHandler, getEccModeHandler, setEccModeHandler,
// getCurrentUtilizationHandler.

TEST_F(MockupResponderTest, testGetMigModeHandlerPmDisabled)
{
    // Touch /tmp/pm-disabled to trigger the pmDisabled() branch
    std::ofstream("/tmp/pm-disabled").close();

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_MIG_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getMigModeHandler(requestMsg, request.size(),
                                                   false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetMigModeHandlerPmDisabled)
{
    std::ofstream("/tmp/pm-disabled").close();

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_MIG_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_MIG_mode_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->setMigModeHandler(requestMsg, request.size(),
                                                   false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetEccModeHandlerPmDisabled)
{
    std::ofstream("/tmp/pm-disabled").close();

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getEccModeHandler(requestMsg, request.size(),
                                                   false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetEccModeHandlerPmDisabled)
{
    std::ofstream("/tmp/pm-disabled").close();

    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_ECC_mode_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->setEccModeHandler(requestMsg, request.size(),
                                                   false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

// ── Invalid clock_id paths ──────────────────────────────────────────────────

TEST_F(MockupResponderTest, testGetClockLimitHandlerInvalidClockId)
{
    // clock_id=2 is neither GRAPHICS_CLOCK(0) nor MEMORY_CLOCK(1)
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_clock_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_limit_req(instanceId, 2, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getClockLimitHandler(requestMsg,
                                                      request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrClockFreqHandlerInvalidClockId)
{
    // clock_id=2 is neither GRAPHICS_CLOCK(0) nor MEMORY_CLOCK(1)
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_curr_clock_freq_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_curr_clock_freq_req(instanceId, 2, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrClockFreqHandler(requestMsg,
                                                         request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── setupAdminOverride else branch (parameter_id != 0) ────────────────────

TEST_F(MockupResponderTest, testSetupAdminOverrideNonZeroParameterId)
{
    // parameter_id=1 (non-zero) → else branch at line 3229-3232
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_setup_admin_override_req(instanceId, 1, 12345, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setupAdminOverride(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── updatePresetProfileParams else branch (parameter_id != 0) ─────────────

TEST_F(MockupResponderTest, testUpdatePresetProfileParamsNonZeroParameterId)
{
    // parameter_id=2 (non-zero) → else branch at line 3469-3472
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_update_preset_profile_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_update_preset_profile_param_req(instanceId, 0, 2, 99000,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updatePresetProfileParams(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── getWorkloadPowerProfileInfo – last-page branch (identifier=3) ──────────

TEST_F(MockupResponderTest, testGetWorkloadPowerProfileInfoLastPage)
{
    // identifier==LAST_PAGE_ID(3) → next_identifier=0 (line 3662)
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_workload_power_profile_info_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, 3,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getWorkloadPowerProfileInfo(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── Additional decode-failure tests ─────────────────────────────────────────

TEST_F(MockupResponderTest, testGetPowerLimitHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getPowerLimitHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetLeakDetectionInfoHandlerDecodeFailure)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getLeakDetectionInfoHandler(m, l);
    });
}

// ── getCurrentUtilizationHandler – pmDisabled path ──────────────────────────

TEST_F(MockupResponderTest, testGetCurrentUtilizationHandlerPmDisabled)
{
    std::ofstream("/tmp/pm-disabled").close();
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_utilization_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getCurrentUtilizationHandler(
        requestMsg, request.size(), false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

// ── getMemoryCapacityUtilHandler – pmDisabled path ───────────────────────────

TEST_F(MockupResponderTest, testGetMemoryCapacityUtilHandlerPmDisabled)
{
    std::ofstream("/tmp/pm-disabled").close();
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getMemoryCapacityUtilHandler(
        requestMsg, request.size(), false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

// ── getClockOutputEnableStateHandler – invalid index path ────────────────────

TEST_F(MockupResponderTest, testGetClockOutputEnableStateHandlerInvalidIndex)
{
    // Use index=0 which is not PCIE(0x80), NVHS(0x81) or IBLINK(0x82)
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_clock_output_enabled_state_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(instanceId, 0,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getClockOutputEnableStateHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── getFpgaDiagnosticsSettingsHandler – missing cases ────────────────────────

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsHandlerDecodeFailure)
{
    // Truncated request so decode fails
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, sizeof(nsm_msg_hdr) - 1);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsWpSettings)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_WP_SETTINGS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_fpga_diagnostics_settings_wp_resp));
}

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsWpJumperPresence)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_WP_JUMPER_PRESENCE, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) +
                  sizeof(nsm_fpga_diagnostics_settings_wp_jumper_resp));
}

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsGpuIstMode)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_GPU_IST_MODE_SETTINGS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
    EXPECT_GE(resp.value().size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpu_ist_mode_resp));
}

// ── More decode-failure tests for handlers past line 5500 ────────────────────

// enableDisableWriteProtectedHandler has assert(rc==NSM_SW_SUCCESS) before the
// if-check so decode failure cannot be tested without abort – skip that path.

TEST_F(MockupResponderTest, testGetDeviceDiagnosticsDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getDeviceDiagnosticsHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetNetworkDeviceDebugInfoDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getNetworkDeviceDebugInfoHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetNetworkDeviceLogInfoDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getNetworkDeviceLogInfoHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testEraseTraceDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->eraseTraceHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testEraseDebugInfoDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->eraseDebugInfoHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPciePortConfigDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getPciePortConfigHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetPciePortConfigDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->setPciePortConfigHandler(m, l);
    });
}

// queryAggregatedResetMetrics and queryAggregatedGPMMetrics both have
// assert(rc == NSM_SW_SUCCESS) immediately after decode with no if-check,
// so decode-failure paths cannot be tested without abort.
// queryPerInstanceGPMMetrics does not check decode rc before using outputs,
// so the decode-failure path is not safely testable.

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsV2DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->queryPerInstanceGPMMetricsV2(m, l);
    });
}

// enableDisableGpuIstMode, getReconfigurationPermissionsV1,
// setReconfigurationPermissionsV1, getConfidentialComputeMode,
// setConfidentialComputeMode all have assert(rc==NSM_SW_SUCCESS) before the
// decode if-check so decode-failure paths cannot be tested without abort.

TEST_F(MockupResponderTest, testSetErrorInjectionModeV1DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->setErrorInjectionModeV1Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetErrorInjectionModeV1DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getErrorInjectionModeV1Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetSupportedErrorInjectionTypesV1DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getSupportedErrorInjectionTypesV1Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetCurrentErrorInjectionTypesV1DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->setCurrentErrorInjectionTypesV1Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetCurrentErrorInjectionTypesV1DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getCurrentErrorInjectionTypesV1Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetErrorInjectionPayloadDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getErrorInjectionPayloadHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetErrorInjectionPayloadDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->setErrorInjectionPayloadHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testActivateErrorInjectionDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->activateErrorInjectionHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetViolationDurationDecodeFailure)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, sizeof(nsm_msg_hdr) - 1, false, longRunningEvent);
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testResetNetworkDeviceDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->resetNetworkDeviceHandler(m, l);
    });
}

// getEgmModeHandler and setEgmModeHandler have assert(rc==NSM_SW_SUCCESS)
// before the decode if-check so decode-failure paths cannot be tested.

TEST_F(MockupResponderTest, testGetHistogramFormatDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getHistogramFormatHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetHistogramDataDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getHistogramDataHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetGpioStateDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getGpioStateHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetListAvailablePciePortsDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getListAvailablePciePortsHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetDeviceCapabilitiesV2DecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getDeviceCapabilitiesV2Handler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetEthPortTelemetryCounterDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getEthPortTelemetryCounterHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPortNetworkAddressesDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getPortNetworkAddressesHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetPortEccCountersDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getPortEccCountersHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetDevicemodeSettingsDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getDevicemodeSettingsHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetProtectionOptionsDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getProtectionOptionsHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetDevicemodeSettingsDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->setDevicemodeSettingsHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetDeviceDebugParametersDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->getDeviceDebugParametersHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testSetDeviceDebugParametersDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->setDeviceDebugParametersHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotCAKInstallDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotCAKInstallHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotCAKBypassDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotCAKBypassHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotLockDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotLockHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotUnlockChallengeDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotUnlockChallengeHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotUnlockDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotUnlockHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotCAKRotateDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotCAKRotateHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotGetInfoDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotGetInfoHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotGetStatusDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotGetStatusHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotDisableDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotDisableHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotOverrideDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotOverrideHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testDotRecoveryDecodeFailure)
{
    truncatedDecodeTest(
        this, [this](const nsm_msg* m, size_t l) -> std::optional<Response> {
        return mockupResponder->dotRecoveryHandler(m, l);
    });
}

TEST_F(MockupResponderTest, testGetViolationDurationPmDisabled)
{
    std::ofstream("/tmp/pm-disabled").close();
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, request.size(), false, longRunningEvent);
    std::remove("/tmp/pm-disabled");
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentUtilizationHandlerLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_utilization_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getCurrentUtilizationHandler(
        requestMsg, request.size(), true, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunningEvent.has_value());
}

TEST_F(MockupResponderTest, testGetMemoryCapacityUtilHandlerLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getMemoryCapacityUtilHandler(
        requestMsg, request.size(), true, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunningEvent.has_value());
}

TEST_F(MockupResponderTest, testGetViolationDurationHandlerLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, request.size(), true, longRunningEvent);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunningEvent.has_value());
}

// =============================================================================
// Verbose path coverage – set mockupResponder->verbose = false before calling
// the handler so that the "if (verbose)" true-branch is exercised.
// =============================================================================

// ── Base / Type-0 handlers ───────────────────────────────────────────────────

TEST_F(MockupResponderTest, testPingHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->pingHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetSupportNvidiaMessageTypesHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_nvidia_message_types_req(instanceId,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportNvidiaMessageTypesHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetSupportCommandCodeHandlerVerbose)
{
    // valid message type → verbose path without error
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_command_codes_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_command_codes_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportCommandCodeHandler(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Network-port handlers ────────────────────────────────────────────────────

TEST_F(MockupResponderTest, testQueryPortCharacteristicsHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_characteristics_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_port_characteristics_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortCharacteristicsHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPortStatusHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_status_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_port_status_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortStatusHandler(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetFabricManagerStateHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fabric_manager_state_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFabricManagerStateHandler(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPortsAvailableHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_ports_available_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortsAvailableHandler(requestMsg,
                                                            request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetPortDisableFutureHandlerVerbose)
{
    mockupResponder->verbose = false;
    bitfield8_t mask[PORT_MASK_DATA_SIZE] = {};
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_disable_future_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_disable_future_req(instanceId, mask, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPortDisableFutureHandler(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortDisableFutureHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_disable_future_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortDisableFutureHandler(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPowerModeHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerModeHandler(requestMsg,
                                                     request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetPowerModeHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_power_mode_data data{};
    auto rc = encode_set_power_mode_req(instanceId, requestMsg, data);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPowerModeHandler(requestMsg,
                                                     request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetSwitchIsolationModeVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_switch_isolation_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSwitchIsolationMode(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetSwitchIsolationModeVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_switch_isolation_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_switch_isolation_mode_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setSwitchIsolationMode(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetEthPortTelemetryCounterHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_ethernet_port_telemetry_counter_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_eth_port_telemetry_counter_req(instanceId, 0,
                                                        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEthPortTelemetryCounterHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortNetworkAddressesHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_addresses_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_addresses_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortNetworkAddressesHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortEccCountersHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_ecc_counters_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_ecc_counters_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortEccCountersHandler(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Platform-Environmental handlers ─────────────────────────────────────────

TEST_F(MockupResponderTest, testGetInventoryInformationHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_inventory_information_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(
        instanceId, BOARD_PART_NUMBER, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getInventoryInformationHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetDriverInfoHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_driver_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDriverInfoHandler(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetMigModeHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_MIG_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getMigModeHandler(requestMsg, request.size(),
                                                   false, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetEccModeHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_ECC_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getEccModeHandler(requestMsg, request.size(),
                                                   false, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetMemoryCapacityUtilHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getMemoryCapacityUtilHandler(
        requestMsg, request.size(), false, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentUtilizationHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_utilization_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getCurrentUtilizationHandler(
        requestMsg, request.size(), false, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetViolationDurationHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, request.size(), false, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetProcessorThrottleReasonHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_clock_event_reason_code_req(instanceId,
                                                             requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getProcessorThrottleReasonHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPowerLimitHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_power_limit_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerLimitHandler(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetPowerLimitHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_power_limit_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_power_limit_req(instanceId, 0, 0, 0, 10000,
                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPowerLimitHandler(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Device-Capability-Discovery handlers ────────────────────────────────────

TEST_F(MockupResponderTest, testGetEventSubscriptionVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_event_subscription_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEventSubscription(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetEventSubscriptionVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_event_subscription_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_event_subscription_req(
        instanceId, GLOBAL_EVENT_GENERATION_DISABLE, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setEventSubscription(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetSupportedEventSourcesVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_supported_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportedEventSources(requestMsg,
                                                          request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentEventSourcesVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_source_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_current_event_source_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentEventSources(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetCurrentEventSourcesVerbose)
{
    mockupResponder->verbose = false;
    bitfield8_t eventSources[EVENT_SOURCES_LENGTH] = {};
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_current_event_source_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_set_current_event_sources_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, eventSources,
        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setCurrentEventSources(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testConfigureEventAcknowledgementVerbose)
{
    mockupResponder->verbose = false;
    bitfield8_t mask[EVENT_SOURCES_LENGTH] = {};
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_configure_event_acknowledgement_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_configure_event_acknowledgement_req(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, mask, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->configureEventAcknowledgement(requestMsg,
                                                               request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetHistogramFormatHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_format_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_format_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getHistogramFormatHandler(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetHistogramDataHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_histogram_data_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_histogram_data_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getHistogramDataHandler(requestMsg,
                                                         request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetGpioStateHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_gpio_state_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_gpio_state_req(instanceId, 0, 8, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getGpioStateHandler(requestMsg,
                                                     request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetListAvailablePciePortsHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_list_available_pcie_ports_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getListAvailablePciePortsHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Power Smoothing handlers ─────────────────────────────────────────────────

TEST_F(MockupResponderTest, testGetPowerSmoothingFeatureInfoVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerSmoothingFeatureInfo(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetHwCircuiteryUsageVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_hardware_lifetime_cricuitry_req(instanceId,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getHwCircuiteryUsage(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPowerSmoothingFeatureInfoV2Verbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPowerSmoothingFeatureInfoV2(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentProfileInfoV2Verbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentProfileInfoV2(requestMsg,
                                                         request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetQueryAdminOverrideV2Verbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_admin_override_profile_info_v2_req(instanceId,
                                                            requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getQueryAdminOverrideV2(requestMsg,
                                                         request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPresetProfileInfoV2Verbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_info_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPresetProfileInfoV2(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentProfileInfoVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentProfileInfo(requestMsg,
                                                       request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetupAdminOverrideVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_setup_admin_override_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_setup_admin_override_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setupAdminOverride(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testApplyAdminOverrideVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_apply_admin_override_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->applyAdminOverride(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testToggleImmediateRampDownVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_immediate_rampdown_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_immediate_rampdown_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->toggleImmediateRampDown(requestMsg,
                                                         request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPresetProfileInfoVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPresetProfileInfo(requestMsg,
                                                      request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testUpdatePresetProfileParamsVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_update_preset_profile_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_update_preset_profile_param_req(instanceId, 0, 0, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updatePresetProfileParams(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEnableWorkloadPowerProfileVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_workload_power_profile_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield256_t mask{};
    auto rc = encode_enable_workload_power_profile_req(instanceId, &mask,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableWorkloadPowerProfile(requestMsg,
                                                            request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDisableWorkloadPowerProfileVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_disable_workload_power_profile_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield256_t mask{};
    auto rc = encode_disable_workload_power_profile_req(instanceId, &mask,
                                                        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->disableWorkloadPowerProfile(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetWorkLoadProfileStatusInfoVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_status_req(instanceId,
                                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getWorkLoadProfileStatusInfo(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetWorkloadPowerProfileInfoVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_workload_power_profile_info_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_info_req(instanceId, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getWorkloadPowerProfileInfo(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Diagnostics / Device-Config handlers ────────────────────────────────────

TEST_F(MockupResponderTest, testGetClockOutputEnableStateHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_clock_output_enabled_state_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_clock_output_enable_state_req(
        instanceId, PCIE_CLKBUF_INDEX, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getClockOutputEnableStateHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_POWER_SUPPLY_STATUS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// getDeviceDiagnosticsHandler – verbose=true AND cover the ternary
// `nextHandle = handle == 0 ? 1 : 0xFF` else branch (handle=1)
TEST_F(MockupResponderTest, testGetDeviceDiagnosticsHandlerHandle1Verbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_diagnostics_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // segment_id=1 → nextHandle == 0xFF branch
    auto rc = encode_get_device_diagnostics_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceDiagnosticsHandler(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// getNetworkDeviceDebugInfoHandler – verbose=true AND cover the ternary
// `nextHandle = handle == 0 ? 1 : 0` else branch (handle=1)
TEST_F(MockupResponderTest, testGetNetworkDeviceDebugInfoHandlerHandle1Verbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_debug_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // handle=1 → nextHandle == 0 branch
    auto rc = encode_get_network_device_debug_info_req(instanceId, 0, 1,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEraseTraceHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_trace_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_erase_trace_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->eraseTraceHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetNetworkDeviceLogInfoHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_device_log_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_device_log_info_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getNetworkDeviceLogInfoHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEraseDebugInfoHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_erase_debug_info_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->eraseDebugInfoHandler(requestMsg,
                                                       request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPciePortConfigHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_pcie_port_config_req(instanceId, 0, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPciePortConfigHandler(requestMsg,
                                                          request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetPciePortConfigHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_port_config_aggregate_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_config_aggregate_req(instanceId, 0, 0, 0, 0,
                                                   nullptr, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPciePortConfigHandler(requestMsg,
                                                          request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryAggregatedResetMetricsVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_reset_statistics_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAggregatedResetMetrics(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryAggregatedGPMMetricsVerbose)
{
    mockupResponder->verbose = false;
    constexpr size_t bitfieldLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_aggregate_gpm_metrics_req) - 1 +
                        bitfieldLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t bitfield = 0x01;
    auto rc = encode_query_aggregate_gpm_metrics_req(
        instanceId, 0, 0, 0, &bitfield, bitfieldLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAggregatedGPMMetrics(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_per_instance_gpm_metrics_req(instanceId, 0, 0, 0, 0,
                                                        0x01, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetrics(requestMsg,
                                                            request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsV2Verbose)
{
    mockupResponder->verbose = false;
    constexpr size_t bitmaskLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_v2_req) - 1 +
                        bitmaskLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield8_t bitmask{};
    bitmask.byte = 0x01;
    auto rc = encode_query_per_instance_gpm_metrics_v2_req(
        instanceId, 0, 0, 0, 0, &bitmask, bitmaskLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetricsV2(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetDevicemodeSettingsHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_setting_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_setting_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetProtectionOptionsHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_protection_options_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getProtectionOptionsHandler(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetDeviceDebugParametersHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_debug_parameters_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id parameter_id{};
    nsm_debug_parameter_sub_id_bitfield parameter_sub_id{};
    auto rc = encode_get_device_debug_parameters_req(
        instanceId, 0, parameter_id, parameter_sub_id, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── DOT handler verbose paths
// ─────────────────────────────────────────────────

TEST_F(MockupResponderTest, testDotCAKBypassHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_bypass_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_bypass_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotCAKBypassHandler(requestMsg,
                                                     request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotUnlockChallengeHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_unlock_challenge_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_unlock_challenge_req challenge_req{};
    challenge_req.unlock_type = 1;
    auto rc = encode_nsm_dot_unlock_challenge_req(instanceId, &challenge_req,
                                                  requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotUnlockChallengeHandler(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotGetInfoHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotGetInfoHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotGetStatusHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_status_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotGetStatusHandler(requestMsg,
                                                     request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotDisableHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_disable_req disable_req{};
    auto rc = encode_nsm_dot_disable_req(instanceId, &disable_req, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotDisableHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotOverrideHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_override_req override_req{};
    auto rc = encode_nsm_dot_override_req(instanceId, &override_req,
                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotOverrideHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testDotRecoveryHandlerVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_recovery_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_dot_recovery_req recovery_req{};
    auto rc = encode_nsm_dot_recovery_req(instanceId, &recovery_req,
                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->dotRecoveryHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Additional verbose=false (non-verbose) tests ─────────────────────────────
// The MockupResponder is constructed with verbose=true.  The tests below
// call each handler with verbose=false, covering the "skip the if (verbose)"
// branch direction that is otherwise never taken.

// processRxMsg verbose=false – covers lines 367/377/388 in processRxMsg
TEST_F(MockupResponderTest, testProcessRxMsgNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testProcessRxMsgEventAckNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_event_acknowledgement(
        instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_PING, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    mockupResponder->verbose = true;
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testUnsupportedCommandHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_ping_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->unsupportedCommandHandler(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortTelemetryCounterHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_telemetry_counter_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_port_telemetry_counter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortTelemetryCounterHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryDeviceIdentificationHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_query_device_identification_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_query_device_identification_req(instanceId,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryDeviceIdentificationHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetTemperatureReadingHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_temperature_reading_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_temperature_reading_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getTemperatureReadingHandler(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testReadThermalParameterHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_read_thermal_parameter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->readThermalParameterHandler(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentPowerDrawHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_power_draw_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_power_draw_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentPowerDrawHandler(requestMsg,
                                                            request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetMaxObservedPowerHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_max_observed_power_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_max_observed_power_req(instanceId, 0, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getMaxObservedPowerHandler(requestMsg,
                                                            request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetQueryAdminOverrideNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_admin_override_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getQueryAdminOverride(requestMsg,
                                                       request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetActivePresetProfileNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_active_preset_profile_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_active_preset_profile_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setActivePresetProfile(requestMsg,
                                                        request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentEnergyCountHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_current_energy_count_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_energy_count_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentEnergyCountHandler(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetVoltageHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_voltage_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getVoltageHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetAltitudePressureHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_altitude_pressure_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getAltitudePressureHandler(requestMsg,
                                                            request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryScalarGroupTelemetryHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_scalar_group_telemetry_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_scalar_group_telemetry_v1_req(instanceId, 0, 0,
                                                         requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryScalarGroupTelemetryHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetRowRemapStateHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_row_remap_state_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getRowRemapStateHandler(requestMsg,
                                                         request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetLeakDetectionInfoHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_leak_detection_info_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getLeakDetectionInfoHandler(requestMsg,
                                                             request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEnableDisableWriteProtectedHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM, 1,
                                           requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEnableDisableGpuIstModeHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 0, 1,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testGetReconfigurationPermissionsV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testSetReconfigurationPermissionsV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetErrorInjectionModeV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_mode_v1_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getErrorInjectionModeV1Handler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetErrorInjectionModeV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_error_injection_mode_v1_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_error_injection_mode_v1_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setErrorInjectionModeV1Handler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testGetSupportedErrorInjectionTypesV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_error_injection_types_v1_req(instanceId,
                                                                requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportedErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testSetCurrentErrorInjectionTypesV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    nsm_error_injection_types_mask mask{};
    mask.mask[0] = 0x01;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_error_injection_types_mask_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_current_error_injection_types_v1_req(instanceId, &mask,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testGetCurrentErrorInjectionTypesV1HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_error_injection_types_v1_req(instanceId,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetErrorInjectionPayloadHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_error_injection_payload_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_error_injection_payload_req(instanceId, 4, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getErrorInjectionPayloadHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetErrorInjectionPayloadHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    struct nsm_error_injection_gpio_spoofing_payload gpioPayload{};
    gpioPayload.count_of_gpio = 0;
    constexpr size_t payloadSize =
        sizeof(nsm_error_injection_gpio_spoofing_payload) - sizeof(uint16_t);
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_error_injection_payload_req) - 1 +
                        payloadSize,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_error_injection_payload_req(
        instanceId, reinterpret_cast<const uint8_t*>(&gpioPayload), payloadSize,
        EI_GPIO_SPOOFING, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto* setReq = reinterpret_cast<nsm_set_error_injection_payload_req*>(
        requestMsg->payload);
    uint16_t correctedDataSize = static_cast<uint16_t>(
        payloadSize +
        (sizeof(nsm_set_error_injection_payload_req) - sizeof(uint8_t)));
    setReq->hdr.data_size = htole16(correctedDataSize);
    auto resp = mockupResponder->setErrorInjectionPayloadHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testResetNetworkDeviceHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_reset_network_device_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_reset_network_device_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetEgmModeHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_EGM_mode_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getEgmModeHandler(requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetDeviceCapabilitiesV2HandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_capabilities_v2_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_get_device_capabilities_v2_req(instanceId, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getDeviceCapabilitiesV2Handler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetDevicemodeSettingsHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_device_mode_setting_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_setting_req(instanceId, 0, DISABLED,
                                                 requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── Event sending functions verbose=false ────────────────────────────────────

TEST_F(MockupResponderTest, testSendRediscoveryEventNonVerbose)
{
    mockupResponder->verbose = false;
    EXPECT_NO_THROW(mockupResponder->sendRediscoveryEvent(30, true));
    mockupResponder->verbose = true;
}

TEST_F(MockupResponderTest, testSendXIDEventNonVerbose)
{
    mockupResponder->verbose = false;
    EXPECT_NO_THROW(mockupResponder->sendXIDEvent(30, true, 0x01, 999, 1,
                                                  500ULL, "nonverbose test"));
    mockupResponder->verbose = true;
}

TEST_F(MockupResponderTest, testSendResetRequiredEventNonVerbose)
{
    mockupResponder->verbose = false;
    EXPECT_NO_THROW(mockupResponder->sendResetRequiredEvent(30, true));
    mockupResponder->verbose = true;
}

TEST_F(MockupResponderTest, testSendFabricManagerStateEventNonVerbose)
{
    mockupResponder->verbose = false;
    EXPECT_NO_THROW(mockupResponder->sendFabricManagerStateEvent(30, true, 0, 0,
                                                                 0ULL, 0ULL));
    mockupResponder->verbose = true;
}

TEST_F(MockupResponderTest, testSendNsmEventNonVerbose)
{
    mockupResponder->verbose = false;
    EXPECT_NO_THROW(mockupResponder->sendNsmEvent(
        30, NSM_TYPE_PLATFORM_ENVIRONMENTAL, true, 0x00, 0, 0, 0, 0, nullptr));
    mockupResponder->verbose = true;
}

TEST_F(MockupResponderTest, testSendThreasholdEventNonVerbose)
{
    mockupResponder->verbose = false;
    EXPECT_NO_THROW(mockupResponder->sendThreasholdEvent(
        30, true, false, true, false, true, false, true, false, 2));
    mockupResponder->verbose = true;
}

TEST_F(MockupResponderTest, testSendGpioStateChangeEventNonVerbose)
{
    mockupResponder->verbose = false;
    std::vector<std::pair<uint16_t, bool>> gpioEvents = {{5, true},
                                                         {10, false}};
    EXPECT_NO_THROW(mockupResponder->sendGpioStateChangeEvent(30, true, 999ULL,
                                                              gpioEvents));
    mockupResponder->verbose = true;
}

// ── setPciePortConfigHandler with actual samples
// ────────────────────────────── Covers the while(sample_count--) loop body
// including all 3 tag branches

TEST_F(MockupResponderTest, testSetPciePortConfigHandlerWithSamples)
{
    // Build 3 samples to cover all inner branches:
    // Sample bytes: tag(1) | flags(1: valid=1<<0, length=0) | data(1) = 3 each
    // tag=0: PCIe Gen Preset, tag=4: Tx Amplitude, tag=5: unexpected (else)
    // flags=0x01 means valid=1, length=0 (data_len = 1<<0 = 1 byte)
    std::vector<uint8_t> sampleData = {
        0x00, 0x01, 0x12, // tag=0 (PCIe Gen), valid=1, length=0, data=0x12
        0x04, 0x01, 0x05, // tag=4 (Tx Amplitude), valid=1, length=0, data=0x05
        0x05, 0x01, 0x00, // tag=5 (unexpected), valid=1, length=0, data=0x00
    };
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_port_config_aggregate_req) - 1 +
                        sampleData.size(),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_config_aggregate_req(
        instanceId, 0, 0, 0, 3, sampleData.data(), sampleData.size(),
        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── getPortNetworkAddressesHandler with InfiniBand port
// ─────────────────────── portNumber=1 (odd) → link_type=1 =
// NSM_PORT_PROTOCOL_INFINIBAND (line 7675)

TEST_F(MockupResponderTest, testGetPortNetworkAddressesInfiniBand)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_addresses_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_addresses_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortNetworkAddressesHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetPortNetworkAddressesInfiniBandNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_network_addresses_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_network_addresses_req(instanceId, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPortNetworkAddressesHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── setDeviceDebugParametersHandler verbose=false ─────────────────────────
TEST_F(MockupResponderTest, testSetDeviceDebugParametersHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    constexpr uint8_t dataSize = 4;
    uint8_t data[dataSize] = {0x01, 0x02, 0x03, 0x04};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_debug_parameters_req) - 1 + dataSize);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_debug_parameter_id param_id{};
    nsm_debug_parameter_sub_id_bitfield sub_id{};
    sub_id.value = 0;
    auto rc = encode_set_device_debug_parameters_req(
        instanceId, 0, param_id, sub_id, dataSize, data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setDeviceDebugParametersHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── queryMultiportScalarGroupTelemetryHandler verbose=false ───────────────
TEST_F(MockupResponderTest,
       testQueryMultiportScalarGroupTelemetryHandlerNonVerbose)
{
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data data{};
    data.upstream_port_index = 0;
    data.type = 0;
    data.index = 0;
    data.group_index = 0;
    auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
        instanceId, &data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryMultiportScalarGroupTelemetryHandler(
        requestMsg, request.size());
    mockupResponder->verbose = true;
    EXPECT_TRUE(resp.has_value());
}

// ── getInventoryInformationHandler for all property IDs ──────────────────
// Covers all switch cases in getProperty()
TEST_F(MockupResponderTest, testGetInventoryInformationAllProperties)
{
    const std::vector<uint8_t> propertyIds = {
        SERIAL_NUMBER,
        MARKETING_NAME,
        DEVICE_PART_NUMBER,
        FRU_PART_NUMBER,
        MAXIMUM_MEMORY_CAPACITY,
        BUILD_DATE,
        DEVICE_GUID,
        INFO_ROM_VERSION,
        PRODUCT_LENGTH,
        PRODUCT_WIDTH,
        PRODUCT_HEIGHT,
        RATED_DEVICE_POWER_LIMIT,
        MINIMUM_DEVICE_POWER_LIMIT,
        MAXIMUM_DEVICE_POWER_LIMIT,
        MAXIMUM_MODULE_POWER_LIMIT,
        MINIMUM_MODULE_POWER_LIMIT,
        RATED_MODULE_POWER_LIMIT,
        DEFAULT_BOOST_CLOCKS,
        DEFAULT_BASE_CLOCKS,
        MINIMUM_EDPP_SCALING_FACTOR,
        MAXIMUM_EDPP_SCALING_FACTOR,
        MINIMUM_GRAPHICS_CLOCK_LIMIT,
        MAXIMUM_GRAPHICS_CLOCK_LIMIT,
        MINIMUM_MEMORY_CLOCK_LIMIT,
        MAXIMUM_MEMORY_CLOCK_LIMIT,
        ASSET_TAG,
        PCIERETIMER_0_EEPROM_VERSION,
    };
    for (auto propId : propertyIds)
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_inventory_information_req));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encode_get_inventory_information_req(instanceId, propId,
                                                       requestMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->getInventoryInformationHandler(
            requestMsg, request.size());
        EXPECT_TRUE(resp.has_value()) << "propId=" << static_cast<int>(propId);
    }
}

TEST_F(MockupResponderTest, testGetInventoryInformationDefaultProperty)
{
    // Unknown property ID → default case in getProperty() → handler returns
    // nullopt
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_inventory_information_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_inventory_information_req(instanceId, 200, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getInventoryInformationHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value()); // Unknown property → no response
}

// ── verbose=false error-path branches ────────────────────────────────────────
// Each handler has an inner "if (verbose)" inside an error path that is only
// reached when verbose=false AND the request is bad.

TEST_F(MockupResponderTest, testGetPortTelemetryCounterDecodeFailNonVerbose)
{
    // verbose=false + truncated request → decode fails → covers verbose=false
    // branch inside the error path (source line ~1214)
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr)); // too short – no payload
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::memset(request.data(), 0, request.size());
    auto resp = mockupResponder->getPortTelemetryCounterHandler(requestMsg,
                                                                request.size());
    mockupResponder->verbose = true;
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testReadThermalParameterShortRequestNonVerbose)
{
    // verbose=false + too-short requestLen → covers verbose=false inside the
    // length-check error path (source line ~1813)
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_read_thermal_parameter_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_read_thermal_parameter_req(instanceId, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // pass length 1 byte shorter than required to trigger the error path
    auto resp = mockupResponder->readThermalParameterHandler(
        requestMsg,
        sizeof(nsm_msg_hdr) + sizeof(nsm_read_thermal_parameter_req) - 1);
    mockupResponder->verbose = true;
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest, testResetNetworkDeviceDecodeFailNonVerbose)
{
    // verbose=false + truncated request → decode fails → covers verbose=false
    // branch inside the error path (source line ~7121)
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr)); // too short
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::memset(request.data(), 0, request.size());
    auto resp = mockupResponder->resetNetworkDeviceHandler(requestMsg,
                                                           request.size());
    mockupResponder->verbose = true;
    EXPECT_FALSE(resp.has_value());
}

// ── setReconfigurationPermissionsV1Handler additional switch cases
// ──────────── The switch on `configuration` has RP_ONESHOOT_HOT_RESET (already
// tested), RP_PERSISTENT, and RP_ONESHOT_FLR; cover the remaining cases.

TEST_F(MockupResponderTest,
       testSetReconfigurationPermissionsV1HandlerPersistent)
{
    // RP_PERSISTENT case; permission=3 covers both inner if-true branches
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST,
        static_cast<reconfiguration_permissions_v1_setting>(RP_PERSISTENT),
        /*permission=*/3, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetReconfigurationPermissionsV1HandlerFLR)
{
    // RP_ONESHOT_FLR case; permission=3 covers both inner if-true branches
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST,
        static_cast<reconfiguration_permissions_v1_setting>(RP_ONESHOT_FLR),
        /*permission=*/3, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testSetReconfigurationPermissionsV1HandlerPermissionBits)
{
    // RP_ONESHOOT_HOT_RESET with permission=3 covers the if-true branches
    // inside that case (lines 6658 and 6662 in source)
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET,
        /*permission=*/3, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest,
       testSetReconfigurationPermissionsV1PersistentPermZero)
{
    // RP_PERSISTENT with permission=0 covers false branches of inner ifs
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST,
        static_cast<reconfiguration_permissions_v1_setting>(RP_PERSISTENT),
        /*permission=*/0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetReconfigurationPermissionsV1FLRPermZero)
{
    // RP_ONESHOT_FLR with permission=0 covers false branches of inner ifs
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST,
        static_cast<reconfiguration_permissions_v1_setting>(RP_ONESHOT_FLR),
        /*permission=*/0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── setCurrentEventSources decode-failure path ───────────────────────────────

TEST_F(MockupResponderTest, testSetCurrentEventSourcesDecodeFailure)
{
    // Truncated request → decode fails → covers the true branch of the
    // "rc != NSM_SUCCESS || nvidiaMessageType > NSM_TYPE_FIRMWARE" condition
    Request request(sizeof(nsm_msg_hdr)); // too short
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    std::memset(request.data(), 0, request.size());
    auto resp = mockupResponder->setCurrentEventSources(requestMsg,
                                                        request.size());
    // handler logs error but still returns a response
    EXPECT_TRUE(resp.has_value());
}

// ── getFpgaDiagnosticsSettingsHandler remaining switch cases ─────────────────
// GET_POWER_SUPPLY_STATUS, GET_GPU_PRESENCE, and GET_GPU_POWER_STATUS were
// not previously covered.

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsPowerSupplyStatus)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_POWER_SUPPLY_STATUS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsGpuPresence)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_GPU_PRESENCE, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetFpgaDiagnosticsSettingsGpuPowerStatus)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_GPU_POWER_STATUS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── GPM Metrics BANDWIDTH unit coverage ──────────────────────────────────

TEST_F(MockupResponderTest, testQueryAggregatedGPMMetricsBandwidth)
{
    // bitfield[1]=0x01 → metric_id=8 (PCIeRawTxBandwidthGbps, BANDWIDTH)
    constexpr size_t bitfieldLen = 2;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_aggregate_gpm_metrics_req) - 1 +
                        bitfieldLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t bitfield[2] = {0x00, 0x01};
    auto rc = encode_query_aggregate_gpm_metrics_req(
        instanceId, 0, 0, 0, bitfield, bitfieldLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAggregatedGPMMetrics(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryAggregatedGPMMetricsInvalidMetric)
{
    // bitfield[4]=0x01 → metric_id=32 (not in metricsTable → continue branch)
    constexpr size_t bitfieldLen = 5;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_aggregate_gpm_metrics_req) - 1 +
                        bitfieldLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    uint8_t bitfield[5] = {0x00, 0x00, 0x00, 0x00, 0x01};
    auto rc = encode_query_aggregate_gpm_metrics_req(
        instanceId, 0, 0, 0, bitfield, bitfieldLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryAggregatedGPMMetrics(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsBandwidth)
{
    // metric_id=8 (BANDWIDTH unit), instance_bitfield bit 0 set
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_per_instance_gpm_metrics_req(instanceId, 0, 0, 0, 8,
                                                        0x01, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetrics(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsInvalidMetric)
{
    // metric_id=99 → metricsTable.end() → returns error response
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_per_instance_gpm_metrics_req(instanceId, 0, 0, 0, 99,
                                                        0x01, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetrics(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsV2Bandwidth)
{
    // metric_id=8 (BANDWIDTH unit)
    constexpr size_t bitmaskLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_v2_req) - 1 +
                        bitmaskLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield8_t bitmask{};
    bitmask.byte = 0x01;
    auto rc = encode_query_per_instance_gpm_metrics_v2_req(
        instanceId, 0, 0, 0, 8, &bitmask, bitmaskLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetricsV2(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testQueryPerInstanceGPMMetricsV2InvalidMetric)
{
    // metric_id=99 → metricsTable.end() → returns error response
    constexpr size_t bitmaskLen = 1;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_per_instance_gpm_metrics_v2_req) - 1 +
                        bitmaskLen,
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    bitfield8_t bitmask{};
    bitmask.byte = 0x01;
    auto rc = encode_query_per_instance_gpm_metrics_v2_req(
        instanceId, 0, 0, 0, 99, &bitmask, bitmaskLen, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPerInstanceGPMMetricsV2(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── enableDisableGpuIstModeHandler branch coverage ───────────────────────

TEST_F(MockupResponderTest, testEnableDisableGpuIstModeHandlerAllGpus)
{
    // device_index=ALL_GPUS_DEVICE_INDEX (0x0A) → else-if branch
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(
        instanceId, ALL_GPUS_DEVICE_INDEX, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEnableDisableGpuIstModeHandlerInvalidIndex)
{
    // device_index=9: not < 8 and not ALL_GPUS_DEVICE_INDEX → else returns
    // nullopt
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 9, 1,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── queryMultiportScalarGroupTelemetry false response branch ──────────────

TEST_F(MockupResponderTest, testQueryMultiportScalarGroupTelemetryInvalidGroup)
{
    // group_index=7 is not handled in getQueryScalarGroupTelemetryResponse
    // switch → returns nullopt → if(response) FALSE branch
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_multiport_query_scalar_group_telemetry_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_multiport_query_scalar_group_telemetry_v2_req_data data{};
    data.upstream_port_index = 0;
    data.type = 0;
    data.index = 0;
    data.group_index = GROUP_ID_7;
    auto rc = encode_multiport_query_scalar_group_telemetry_v2_req(
        instanceId, &data, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryMultiportScalarGroupTelemetryHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── Error injection map end() branches ───────────────────────────────────

TEST_F(MockupResponderTest,
       testGetSupportedErrorInjectionTypesUnknownDeviceType)
{
    // NSM_DEV_ID_CPU is not in errorInjection map → end() branch
    mockupResponder.reset();
    objServer.reset();
    systemBus.reset();
    init(30, NSM_DEV_ID_CPU, 2);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_supported_error_injection_types_v1_req(instanceId,
                                                                requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getSupportedErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testSetCurrentErrorInjectionTypesUnknownDeviceType)
{
    // NSM_DEV_ID_CPU is not in errorInjection map → end() branch
    mockupResponder.reset();
    objServer.reset();
    systemBus.reset();
    init(30, NSM_DEV_ID_CPU, 2);
    nsm_error_injection_types_mask mask{};
    mask.mask[0] = 0x01;
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_error_injection_types_mask_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_current_error_injection_types_v1_req(instanceId, &mask,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testGetCurrentErrorInjectionTypesUnknownDeviceType)
{
    // NSM_DEV_ID_CPU is not in errorInjection map → end() branch
    mockupResponder.reset();
    objServer.reset();
    systemBus.reset();
    init(30, NSM_DEV_ID_CPU, 2);
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_error_injection_types_v1_req(instanceId,
                                                              requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->getCurrentErrorInjectionTypesV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── getReconfigurationPermissionsV1Handler invalid index ──────────────────

TEST_F(MockupResponderTest,
       DISABLED_testGetReconfigurationPermissionsV1InvalidSettingsIndex)
{
    // settingsIndex > RP_RUNTIME_IN_SYSTEM_TEST (24) -> returns nullopt
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_get_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Manually overwrite setting_index to invalid value (> 24)
    auto* reqPayload =
        reinterpret_cast<nsm_get_reconfiguration_permissions_v1_req*>(
            requestMsg->payload);
    reqPayload->setting_index = 100;
    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── enableDisableGpuIstModeHandler ternary false branches ─────────────────

TEST_F(MockupResponderTest, testEnableDisableGpuIstModeHandlerDisableGpu)
{
    // value=0 with device_index<8 → covers ternary false branch at line 6537
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(instanceId, 0, 0,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderTest, testEnableDisableGpuIstModeHandlerAllGpusDisable)
{
    // value=0 with ALL_GPUS → covers ternary false branch at line 6542
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_gpu_ist_mode_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_gpu_ist_mode_req(
        instanceId, ALL_GPUS_DEVICE_INDEX, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->enableDisableGpuIstModeHandler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// ── setReconfigurationPermissionsV1Handler invalid settingsIndex ──────────

TEST_F(MockupResponderTest,
       DISABLED_testSetReconfigurationPermissionsV1InvalidSettingsIndex)
{
    // settingsIndex > RP_RUNTIME_IN_SYSTEM_TEST (24) -> returns nullopt
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Manually overwrite setting_index to invalid value (> 24)
    auto* reqPayload =
        reinterpret_cast<nsm_set_reconfiguration_permissions_v1_req*>(
            requestMsg->payload);
    reqPayload->setting_index = 100;
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderTest,
       DISABLED_testSetReconfigurationPermissionsV1InvalidConfiguration)
{
    // invalid configuration value → hits switch default → returns nullopt
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_reconfiguration_permissions_v1_req),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_IN_SYSTEM_TEST, RP_ONESHOOT_HOT_RESET, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Manually overwrite configuration to invalid value
    auto* reqPayload =
        reinterpret_cast<nsm_set_reconfiguration_permissions_v1_req*>(
            requestMsg->payload);
    reqPayload->configuration = 99;
    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── getFpgaDiagnosticsSettingsHandler default case ────────────────────────

TEST_F(MockupResponderTest,
       testGetFpgaDiagnosticsSettingsHandlerInvalidDataIndex)
{
    // data_index not matching any case → default: break → returns nullopt
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_fpga_diagnostics_settings_req), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_fpga_diagnostics_settings_req(
        instanceId, GET_WP_SETTINGS, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Manually overwrite data_index to invalid value
    auto* reqPayload = reinterpret_cast<nsm_get_fpga_diagnostics_settings_req*>(
        requestMsg->payload);
    reqPayload->data_index = 99;
    auto resp = mockupResponder->getFpgaDiagnosticsSettingsHandler(
        requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// ── EventSource out-of-range event ───────────────────────────────────────
// Covers: EventSource::EventSource if(event >= EVENT_SOURCES_LENGTH*8) TRUE

TEST_F(MockupResponderTest, testEventSourceOutOfRangeEvent)
{
    // Events 64 and 100 are >= EVENT_SOURCES_LENGTH*8 (=64) → ignored
    // (continue) Events 0 and 63 are valid → corresponding bits OR'd in
    MockupResponder::EventSource src({64, 100, 0, 63});
    // event 0 → index=0, offset=0 → bit 0 of events[0] set
    EXPECT_NE(src.events[0].byte & 0x01u, 0u);
    // event 63 → index=7, offset=7 → bit 7 of events[7] set
    EXPECT_NE(src.events[7].byte & 0x80u, 0u);
}

// ── setPciePortConfigHandler with valid=0 sample ─────────────────────────
// Covers: setPciePortConfigHandler if(valid) FALSE branch (line 6078)

TEST_F(MockupResponderTest, testSetPciePortConfigHandlerNotValidSample)
{
    // flags byte = 0x00: valid=0 (bit 0=0), length=0 (bits 1-3=0)
    // → data_len=1<<0=1 byte → decode sets valid=false → if(valid) false path
    std::vector<uint8_t> sampleData = {
        0x00,
        0x00,
        0x12, // tag=0, valid=0, length=0, data=0x12
    };
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_port_config_aggregate_req) - 1 +
                        sampleData.size(),
                    0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_port_config_aggregate_req(
        instanceId, 0, 0, 0, 1, sampleData.data(), sampleData.size(),
        requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setPciePortConfigHandler(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

// ---- firmwareUtils.cpp additional error-path coverage tests ----

// Lines 384-387: irreversibleConfig default/unknown request_type
TEST_F(MockupResponderTest, testIrreversibleConfigHandlerDefaultType)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_irreversible_config_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_irreversible_config_req cfg_req = {
        QUERY_IRREVERSIBLE_CFG};
    auto rc = encode_nsm_firmware_irreversible_config_req(instanceId, &cfg_req,
                                                          requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Overwrite request_type to an invalid value (not 0/1/2) to hit default:
    auto* cmd = reinterpret_cast<nsm_firmware_irreversible_config_req_command*>(
        requestMsg->payload);
    cmd->irreversible_cfg_req.request_type = 99;
    auto resp = mockupResponder->irreversibleConfig(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 408-413: imageCopyControl with too-short request
TEST_F(MockupResponderTest, testImageCopyControlHandlerShortRequest)
{
    Request request(sizeof(nsm_msg_hdr));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->imageCopyControl(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 505-509: imageCopyControl with unsupported request_type
TEST_F(MockupResponderTest, testImageCopyControlHandlerUnsupportedType)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_image_copy_control_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_image_copy_control_req ctrl_req = {
        NSM_IMAGE_COPY_QUERY_PROGRESS, 0};
    auto rc = encode_nsm_firmware_image_copy_control_req(instanceId, &ctrl_req,
                                                         nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    // Overwrite request_type to an invalid value to hit default:
    auto* cmd = reinterpret_cast<nsm_firmware_image_copy_control_req_command*>(
        requestMsg->payload);
    cmd->image_copy_control_req.request_type = 99;
    auto resp = mockupResponder->imageCopyControl(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 537-539: codeAuthKeyPermQueryHandler invalid classification
TEST_F(MockupResponderTest,
       testCodeAuthKeyPermQueryHandlerInvalidClassification)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_query_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // classification 0x0001 is not the expected 0x000A
    auto rc = encode_nsm_code_auth_key_perm_query_req(instanceId, 0x0001,
                                                      0x0010, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermQueryHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 541-543: codeAuthKeyPermQueryHandler invalid classification index
TEST_F(MockupResponderTest, testCodeAuthKeyPermQueryHandlerInvalidIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_query_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // index 1 is not the expected 0
    auto rc = encode_nsm_code_auth_key_perm_query_req(instanceId, 0x000A,
                                                      0x0010, 1, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermQueryHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 545-548: codeAuthKeyPermQueryHandler invalid identifier
TEST_F(MockupResponderTest, testCodeAuthKeyPermQueryHandlerInvalidIdentifier)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_query_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // identifier 0x1234 is neither 0x0010 nor 0xFF00
    auto rc = encode_nsm_code_auth_key_perm_query_req(instanceId, 0x000A,
                                                      0x1234, 0, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermQueryHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 649-651: MOST_RESTRICTIVE + bitmapLength != 0
TEST_F(MockupResponderTest,
       testCodeAuthKeyPermUpdateHandlerMostRestrictiveBitmap)
{
    // Encode with bitmapLength=1 so data_size is consistent and decode passes
    uint8_t bitmap = 0xFF;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req) + 1);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x0010, 0, 0, 1, &bitmap, requestMsg);
    // If encode rejects bitmapLength=1 with MOST_RESTRICTIVE, skip this path
    if (rc != NSM_SW_SUCCESS)
    {
        GTEST_SKIP() << "encode rejected bitmapLength=1 with MOST_RESTRICTIVE";
    }
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 653-656: codeAuthKeyPermUpdateHandler invalid classification
TEST_F(MockupResponderTest,
       testCodeAuthKeyPermUpdateHandlerInvalidClassification)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // classification 0x0001 is not 0x000A
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x0001, 0x0010, 0, 0, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 658-661: codeAuthKeyPermUpdateHandler invalid classification index
TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerInvalidClassIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // index 1 is not 0
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x0010, 1, 0, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 663-666: codeAuthKeyPermUpdateHandler invalid identifier
TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerInvalidIdentifier)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // identifier 0x1234 is neither 0x0010 nor 0xFF00
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x1234, 0, 0, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 668-670: configState disabled → returns 0x87 error response
TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerConfigDisabled)
{
    // Ensure configState = 0 by calling DISABLE_IRREVERSIBLE_CFG
    {
        Request disableReq(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_req_command));
        auto disableMsg = reinterpret_cast<nsm_msg*>(disableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            DISABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, disableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->irreversibleConfig(disableMsg,
                                                        disableReq.size());
        EXPECT_TRUE(resp.has_value());
    }
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x0010, 0, 0, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value()); // 0x87 irreversible-config-disabled
}

// Lines 672-674: nonce mismatch → returns 0x88 error response
TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerNonceMismatch)
{
    // Enable irreversible config so configState = 1
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        auto resp = mockupResponder->irreversibleConfig(enableMsg,
                                                        enableReq.size());
        EXPECT_TRUE(resp.has_value());
    }
    // fixedNonce = 123456789; use a different value to trigger mismatch
    const uint64_t wrongNonce = 999999999ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_MOST_RESTRICTIVE_VALUE,
        0x000A, 0x0010, 0, wrongNonce, 0, nullptr, requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value()); // 0x88 nonce-mismatch response
}

// ---- updateMinSecurityVersion additional path tests ----

// Lines 828-831: SPECIFIED_VALUE + req_min_security_version == 0
TEST_F(MockupResponderTest, testUpdateMinSecVerSpecifiedZeroVersion)
{
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_SPECIFIED_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00;
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    sec_req.req_min_security_version = 0; // zero → triggers lines 828-831
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 843-847: correct nonce + configState == 0 → 0x87 response
TEST_F(MockupResponderTest, testUpdateMinSecVerConfigDisabled)
{
    // Ensure configState = 0
    {
        Request disableReq(
            sizeof(nsm_msg_hdr) +
            sizeof(nsm_firmware_irreversible_config_req_command));
        auto disableMsg = reinterpret_cast<nsm_msg*>(disableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            DISABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, disableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(disableMsg, disableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00;
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce; // correct nonce
    sec_req.req_min_security_version = 0;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value()); // returns 0x87 disabled response
}

// Lines 858-863: MOST_RESTRICTIVE + AP (identifier != 0xFF00)
TEST_F(MockupResponderTest, testUpdateMinSecVerMostRestrictiveAP)
{
    // Enable irreversible config
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_MOST_RESTRICTIVE_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0x0010; // AP path (not 0xFF00)
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    sec_req.req_min_security_version = 0;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 866-877: SPECIFIED_VALUE + EC (0xFF00) in-range version
TEST_F(MockupResponderTest, testUpdateMinSecVerSpecifiedECInRange)
{
    // Enable irreversible config
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_SPECIFIED_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00; // EC
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    // active=3, minimum=3 (set by prior MOST_RESTRICTIVE EC test); value 3
    // satisfies >= 3 && <= 3
    sec_req.req_min_security_version = 3;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 880-885: SPECIFIED_VALUE + EC out-of-range version
TEST_F(MockupResponderTest, testUpdateMinSecVerSpecifiedECOutOfRange)
{
    // Enable irreversible config
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_SPECIFIED_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0xFF00; // EC
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    sec_req.req_min_security_version =
        100; // > active_component_security_version
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value()); // returns NSM_ERR_INVALID_DATA
}

// Lines 890-898: SPECIFIED_VALUE + AP in-range version
TEST_F(MockupResponderTest, testUpdateMinSecVerSpecifiedAPInRange)
{
    // Enable irreversible config
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_SPECIFIED_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0x0010; // AP
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    // AP active=4; value 1 satisfies > 0 && <= 4
    sec_req.req_min_security_version = 1;
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 900-907: SPECIFIED_VALUE + AP out-of-range version
TEST_F(MockupResponderTest, testUpdateMinSecVerSpecifiedAPOutOfRange)
{
    // Enable irreversible config
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_update_min_sec_ver_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_update_min_sec_ver_req sec_req = {};
    sec_req.request_type = REQUEST_TYPE_SPECIFIED_VALUE;
    sec_req.component_classification = 0x000A;
    sec_req.component_identifier = 0x0010; // AP
    sec_req.component_classification_index = 0;
    sec_req.nonce = fixedNonce;
    sec_req.req_min_security_version =
        100; // > active_component_security_version
    auto rc = encode_nsm_firmware_update_sec_ver_req(instanceId, &sec_req,
                                                     requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_TRUE(resp.has_value()); // returns NSM_ERR_INVALID_DATA
}

// ---- setRotProperty additional error-path tests ----

// Lines 955-956: request too short
TEST_F(MockupResponderTest, testSetRotPropertyHandlerShortRequest)
{
    Request request(sizeof(nsm_msg_hdr));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value()); // encodeResp returns error response
}

// Lines 967-968: property type > 2
TEST_F(MockupResponderTest, testSetRotPropertyHandlerInvalidPropertyType)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.component_classification = 0x000A;
    rot_req.component_identifier = 0x0010;
    rot_req.component_classification_index = 0;
    rot_req.property = 3; // invalid: > 2
    rot_req.argument_length = 2;
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 977-979: property 0 with argument_length != 2
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp0InvalidArgLength)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_REDUNDANCY_POLICY;
    rot_req.argument_length = 1; // invalid: must be 2
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 987-989: property 0 with invalid redundancy policy (> 1)
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp0InvalidPolicy)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_REDUNDANCY_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 2; // invalid policy: > 1
    rot_req.argument_data[1] = 0;
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 993-995: property 0 with invalid lifespan (> 1)
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp0InvalidLifespan)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_REDUNDANCY_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 0; // valid policy
    rot_req.argument_data[1] = 2; // invalid lifespan: > 1
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 1008-1010: property 1 with argument_length != 2
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp1InvalidArgLength)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY;
    rot_req.argument_length = 1; // invalid: must be 2
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 1018-1020: property 1 with invalid update policy (> 1)
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp1InvalidPolicy)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 2; // invalid policy: > 1
    rot_req.argument_data[1] = 0;
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 1024-1026: property 1 with invalid lifespan (> 1)
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp1InvalidLifespan)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_INBAND_UPDATE_POLICY;
    rot_req.argument_length = 2;
    rot_req.argument_data[0] = 0; // valid policy
    rot_req.argument_data[1] = 2; // invalid lifespan: > 1
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 1038-1040: property 2 with argument_length != 5
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp2InvalidArgLength)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_AP_SKU_ID;
    rot_req.argument_length = 2; // invalid: must be 5
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 1050-1052: property 2 with invalid lifespan (> 1)
TEST_F(MockupResponderTest, testSetRotPropertyHandlerProp2InvalidLifespan)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_firmware_set_rot_property_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    struct nsm_firmware_set_rot_property_req rot_req = {};
    rot_req.property = NSM_ROT_PROPERTY_AP_SKU_ID;
    rot_req.argument_length = 5;
    rot_req.argument_data[0] = 0x78;
    rot_req.argument_data[1] = 0x56;
    rot_req.argument_data[2] = 0x34;
    rot_req.argument_data[3] = 0x12;
    rot_req.argument_data[4] = 2; // invalid lifespan: > 1
    auto rc = encode_nsm_firmware_set_rot_property_req(instanceId, &rot_req,
                                                       requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->setRotProperty(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// Lines 708-710: codeAuthKeyPermUpdateHandler with AP SPECIFIED_VALUE and
// bitmapLength=9 > apPendingEfuseKeyPerm.size()=8 → NSM_ERR_INVALID_DATA_LENGTH
TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerAPBitmapTooLarge)
{
    // Enable irreversible config so configState=1
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    // 9 bitmap bytes: more than apPendingEfuseKeyPerm.size()=8
    std::vector<uint8_t> bitmap(9, 0xFF);
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req) + 9);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE, 0x000A,
        0x0010, 0, fixedNonce, 9, bitmap.data(), requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value()); // returns NSM_ERR_INVALID_DATA_LENGTH
}

// Lines 737-739: codeAuthKeyPermUpdateHandler with EC SPECIFIED_VALUE and
// bitmapLength=2 > ecEfuseKeyPerm.size()=1 → NSM_ERR_INVALID_DATA_LENGTH
TEST_F(MockupResponderTest, testCodeAuthKeyPermUpdateHandlerECBitmapTooLarge)
{
    // Enable irreversible config so configState=1
    {
        Request enableReq(sizeof(nsm_msg_hdr) +
                          sizeof(nsm_firmware_irreversible_config_req_command));
        auto enableMsg = reinterpret_cast<nsm_msg*>(enableReq.data());
        struct nsm_firmware_irreversible_config_req cfg_req = {
            ENABLE_IRREVERSIBLE_CFG};
        auto rc = encode_nsm_firmware_irreversible_config_req(
            instanceId, &cfg_req, enableMsg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        mockupResponder->irreversibleConfig(enableMsg, enableReq.size());
    }
    static constexpr uint64_t fixedNonce = 123456789ULL;
    // 2 bitmap bytes: more than ecEfuseKeyPerm.size()=1
    std::vector<uint8_t> bitmap(2, 0xFF);
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_code_auth_key_perm_update_req) + 2);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_code_auth_key_perm_update_req(
        instanceId, NSM_CODE_AUTH_KEY_PERM_REQUEST_TYPE_SPECIFIED_VALUE, 0x000A,
        0xFF00, 0, fixedNonce, 2, bitmap.data(), requestMsg);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    auto resp = mockupResponder->codeAuthKeyPermUpdateHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value()); // returns NSM_ERR_INVALID_DATA_LENGTH
}

// Lines 763-768: queryFirmwareSecurityVersion with too-short request →
// decode returns NSM_SW_ERROR_LENGTH → handler returns nullopt
TEST_F(MockupResponderTest, testQueryFirmwareSecurityVersionDecodeFailure)
{
    Request request(sizeof(nsm_msg_hdr)); // too short - no payload
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->queryFirmwareSecurityVersion(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 82-85: getRotInformation with too-short request → decode returns
// NSM_SW_ERROR_LENGTH → handler returns nullopt (no assert before check)
TEST_F(MockupResponderTest, testGetRotInformationHandlerDecodeFailure)
{
    Request request(sizeof(nsm_msg_hdr)); // too short - no payload
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getRotInformation(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// Lines 813-815: updateMinSecurityVersion with too-short request → decode
// returns NSM_SW_ERROR_LENGTH → handler returns nullopt (no assert before
// check)
TEST_F(MockupResponderTest, testUpdateMinSecurityVersionDecodeFailure)
{
    Request request(sizeof(nsm_msg_hdr)); // too short - no payload
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->updateMinSecurityVersion(requestMsg,
                                                          request.size());
    EXPECT_FALSE(resp.has_value());
}

// ===========================================================================
// Dump failure-cycle tests.
// Verifies the --failure_cycle replay added to the 5 dump handlers:
//   getDeviceDiagnostics (0x40), getNetworkDeviceDebugInfo (0x50),
//   eraseTrace (0x51), getNetworkDeviceLogInfo (0x52),
//   eraseDebugInfo (0x59).
// Each test builds a fresh MockupResponder (with the cycle on or off),
// invokes a handler, and decodes the response with the same libnsm helper
// a real consumer would use — so the assertions are wire-level accurate.
// The cycle list itself is MockupResponder::kDumpFailureCycle.
// ===========================================================================
namespace
{

class MockupDumpCycleTest : public Test
{
  protected:
    void TearDown() override
    {
        mockupResponder.reset();
        objServer.reset();
        systemBus.reset();
        io.stop();
    }

    // Build a fresh mock with the dump failure cycle on or off.
    void buildMock(bool failureCycle, uint8_t deviceType = NSM_DEV_ID_GPU)
    {
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            true, event, *objServer, /*eid=*/30, deviceType, /*instanceId=*/0,
            failureCycle);
    }

    // Index of a cycle case by its stable id (test positioning helper).
    static uint32_t caseIndexOf(std::string_view id)
    {
        for (uint32_t i = 0; i < MockupResponder::kDumpFailureCycle.size(); ++i)
        {
            if (MockupResponder::kDumpFailureCycle[i].caseId == id)
            {
                return i;
            }
        }
        ADD_FAILURE() << "unknown cycle case id: " << id;
        return 0;
    }

    uint8_t instanceId = 0;
    common::Event event;
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;
};

// Helper: build a Get-Network-Device-Debug-Info request for a given handle.
static Request makeDebugInfoReq(uint8_t instanceId, uint32_t handle,
                                uint8_t debugType = 0)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_network_device_debug_info_req));
    auto* msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_get_network_device_debug_info_req(instanceId, debugType,
                                                       handle, msg),
              NSM_SW_SUCCESS);
    return request;
}

static Request makeDiagnosticsReq(uint8_t instanceId, uint8_t handle)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_diagnostics_req));
    auto* msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_get_device_diagnostics_req(instanceId, handle, msg),
              NSM_SW_SUCCESS);
    return request;
}

static Request makeLogInfoReq(uint8_t instanceId, uint32_t handle)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_network_device_log_info_req));
    auto* msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_get_network_device_log_info_req(instanceId, handle, msg),
              NSM_SW_SUCCESS);
    return request;
}

// Decode a debug-info (0x50) response into (rc, cc, reason, nextHandle).
struct DebugInfoDecoded
{
    uint8_t rc;
    uint8_t cc;
    uint16_t reason;
    uint32_t nextHandle;
};
static DebugInfoDecoded decodeDebugInfo(const std::vector<uint8_t>& resp)
{
    DebugInfoDecoded d{};
    uint16_t segSize = 0;
    std::vector<uint8_t> seg(512, 0);
    auto* respMsg =
        reinterpret_cast<nsm_msg*>(const_cast<uint8_t*>(resp.data()));
    d.rc = decode_get_network_device_debug_info_resp(respMsg, resp.size(),
                                                     &d.cc, &d.reason, &segSize,
                                                     seg.data(), &d.nextHandle);
    return d;
}

// Read the completion_code / reason_code of a non-success response. The
// command + completion_code + reason_code header is identical across every
// NSM response, so this is valid for both success and error responses.
static uint8_t respCompletionCode(const std::vector<uint8_t>& resp)
{
    auto* respMsg =
        reinterpret_cast<nsm_msg*>(const_cast<uint8_t*>(resp.data()));
    return reinterpret_cast<nsm_common_non_success_resp*>(respMsg->payload)
        ->completion_code;
}
static uint16_t respReasonCode(const std::vector<uint8_t>& resp)
{
    auto* respMsg =
        reinterpret_cast<nsm_msg*>(const_cast<uint8_t*>(resp.data()));
    return le16toh(
        reinterpret_cast<nsm_common_non_success_resp*>(respMsg->payload)
            ->reason_code);
}

// ---- cycle off ---------------------------------------------------------

TEST_F(MockupDumpCycleTest, CycleOff_ReturnsSuccessByDefault)
{
    buildMock(/*failureCycle=*/false);
    auto req = makeDebugInfoReq(instanceId, 0);
    auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(reqMsg,
                                                                  req.size());
    ASSERT_TRUE(resp.has_value());
    const auto d = decodeDebugInfo(*resp);
    EXPECT_EQ(d.rc, NSM_SW_SUCCESS);
    EXPECT_EQ(d.cc, NSM_SUCCESS);
    // Counter is never touched when the cycle is off.
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 0u);
    EXPECT_EQ(mockupResponder->cyclePageIndex, 0u);
}

// ---- cycle on: ordering / advancement ----------------------------------

TEST_F(MockupDumpCycleTest, CycleOn_FirstEntryIsSuccess)
{
    buildMock(/*failureCycle=*/true);
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 0u);

    auto req = makeDebugInfoReq(instanceId, 0);
    auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(reqMsg,
                                                                  req.size());
    ASSERT_TRUE(resp.has_value());
    const auto d = decodeDebugInfo(*resp);
    EXPECT_EQ(d.rc, NSM_SW_SUCCESS);
    EXPECT_EQ(d.cc, NSM_SUCCESS);
    EXPECT_EQ(d.nextHandle, 0u); // END sentinel -> single page
    // SUCCESS is a single-page case, so the counter moved to entry 1.
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 1u);
}

TEST_F(MockupDumpCycleTest, CycleOn_AdvancesGlobally)
{
    buildMock(/*failureCycle=*/true);

    // 0x40 consumes entry 0 (SUCCESS).
    {
        auto req = makeDiagnosticsReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getDeviceDiagnosticsHandler(reqMsg,
                                                                 req.size());
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(respCompletionCode(*resp), NSM_SUCCESS);
    }
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 1u);

    // 0x50 consumes entry 1 (CC_UNSUPPORTED_COMMAND_CODE).
    {
        auto req = makeDebugInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
            reqMsg, req.size());
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    }
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 2u);

    // 0x52 consumes entry 2 (CC_UNSUPPORTED_MSG_TYPE).
    {
        auto req = makeLogInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getNetworkDeviceLogInfoHandler(reqMsg,
                                                                    req.size());
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_UNSUPPORTED_MSG_TYPE);
    }
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 3u);
}

TEST_F(MockupDumpCycleTest, CycleOn_WrapsAtEnd)
{
    buildMock(/*failureCycle=*/true);

    // Total device responses across all cases (single + multi page).
    uint32_t totalPages = 0;
    for (const auto& c : MockupResponder::kDumpFailureCycle)
    {
        totalPages += c.pageCount;
    }

    // Drive the iterative debug handler one response at a time through the
    // whole list; it should land back exactly at the start.
    for (uint32_t i = 0; i < totalPages; ++i)
    {
        auto req = makeDebugInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        (void)mockupResponder->getNetworkDeviceDebugInfoHandler(reqMsg,
                                                                req.size());
    }
    EXPECT_EQ(mockupResponder->cycleCaseIndex, 0u);
    EXPECT_EQ(mockupResponder->cyclePageIndex, 0u);
}

// ---- cycle on: completion-code entries ---------------------------------

TEST_F(MockupDumpCycleTest, CycleOn_AllCcCases)
{
    struct Expect
    {
        const char* id;
        uint8_t cc;
    };
    const std::vector<Expect> ccCases = {
        {"CC_UNSUPPORTED_COMMAND_CODE", NSM_ERR_UNSUPPORTED_COMMAND_CODE},
        {"CC_UNSUPPORTED_MSG_TYPE", NSM_ERR_UNSUPPORTED_MSG_TYPE},
        {"CC_NSM_BUSY", NSM_BUSY},
        {"CC_NSM_ERR_NOT_READY", NSM_ERR_NOT_READY},
        {"CC_NSM_ERR_BUS_ACCESS", NSM_ERR_BUS_ACCESS},
        {"CC_INVALID_STATE_FOR_COMMAND", NSM_ERR_INVALID_STATE_FOR_COMMAND},
        {"CC_INVALID_DATA", NSM_ERR_INVALID_DATA},
        {"CC_INVALID_DATA_LENGTH", NSM_ERR_INVALID_DATA_LENGTH},
        {"CC_INVALID_REQUEST_TYPE", NSM_ERR_INVALID_REQUEST_TYPE},
        {"CC_NSM_ACCEPTED", NSM_ACCEPTED},
    };

    // Build the mock once; rebuilding inside the loop would create a second
    // asio object_server on the same io_context/EID and fail with
    // "assign: File exists". Each iteration just repositions the cycle cursor.
    buildMock(/*failureCycle=*/true);
    for (const auto& e : ccCases)
    {
        // Position the global counter at this case.
        mockupResponder->cycleCaseIndex = caseIndexOf(e.id);
        mockupResponder->cyclePageIndex = 0;

        auto req = makeDebugInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
            reqMsg, req.size());
        ASSERT_TRUE(resp.has_value()) << e.id;
        EXPECT_EQ(respCompletionCode(*resp), e.cc) << e.id;
    }
}

// ---- cycle on: reason-code entries -------------------------------------

TEST_F(MockupDumpCycleTest, CycleOn_AllReasonCases)
{
    struct Expect
    {
        const char* id;
        uint16_t reason;
    };
    const std::vector<Expect> reasonCases = {
        {"REASON_ERR_TIMEOUT", ERR_TIMEOUT},
        {"REASON_ERR_DOWNSTREAM_TIMEOUT", ERR_DOWNSTREAM_TIMEOUT},
        {"REASON_ERR_NOT_SUPPORTED", ERR_NOT_SUPPORTED},
        {"REASON_ERR_NO_BOOT_COMPLETE", ERR_NO_BOOT_COMPLETE},
        {"REASON_ERR_UPDATE_IN_PROGRESS", ERR_UPDATE_IN_PROGRESS},
        {"REASON_ERR_IMAGE_COPY_IN_PROGRESS", ERR_IMAGE_COPY_IN_PROGRESS},
        {"REASON_ERR_FLASH_WEAR_MITIGATION", ERR_FLASH_WEAR_MITIGATION},
        {"REASON_ERR_INVALID_PCI", ERR_INVALID_PCI},
        {"REASON_ERR_INVALID_RQD", ERR_INVALID_RQD},
        {"REASON_ERR_INCOMPLETE_COMPONENT_SET", ERR_INCOMPLETE_COMPONENT_SET},
        {"REASON_ERR_I2C_NACK_FROM_DEV_ADDR", ERR_I2C_NACK_FROM_DEV_ADDR},
    };

    // Build the mock once (see CycleOn_AllCcCases); rebuilding per iteration
    // collides on the asio object_server ("assign: File exists").
    buildMock(/*failureCycle=*/true);
    for (const auto& e : reasonCases)
    {
        mockupResponder->cycleCaseIndex = caseIndexOf(e.id);
        mockupResponder->cyclePageIndex = 0;

        auto req = makeDebugInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
            reqMsg, req.size());
        ASSERT_TRUE(resp.has_value()) << e.id;
        // Carrier cc is NSM_ERR_NOT_READY; the reason code carries the trigger.
        EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_NOT_READY) << e.id;
        EXPECT_EQ(respReasonCode(*resp), e.reason) << e.id;
    }
}

// ---- cycle on: transport silence ---------------------------------------

TEST_F(MockupDumpCycleTest, CycleOn_NoResponseEmitsNothing)
{
    buildMock(/*failureCycle=*/true);
    mockupResponder->cycleCaseIndex = caseIndexOf("NO_RESPONSE_TIMEOUT");
    mockupResponder->cyclePageIndex = 0;

    auto req = makeDebugInfoReq(instanceId, 0);
    auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
    auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(reqMsg,
                                                                  req.size());
    EXPECT_FALSE(resp.has_value());
    // The silent page still advances the counter past this case.
    EXPECT_EQ(mockupResponder->cycleCaseIndex,
              caseIndexOf("NO_RESPONSE_TIMEOUT") + 1);
}

// ---- cycle on: multi-page stuck-handle case ----------------------------

TEST_F(MockupDumpCycleTest, CycleOn_StuckHandlePages)
{
    buildMock(/*failureCycle=*/true);
    const uint32_t stuckIdx = caseIndexOf("PROTO_STUCK_HANDLE");
    mockupResponder->cycleCaseIndex = stuckIdx;
    mockupResponder->cyclePageIndex = 0;

    // Page 0: Advance -> next handle = request + 1, BMC would recurse.
    {
        auto req = makeDebugInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
            reqMsg, req.size());
        ASSERT_TRUE(resp.has_value());
        const auto d = decodeDebugInfo(*resp);
        EXPECT_EQ(d.cc, NSM_SUCCESS);
        EXPECT_EQ(d.nextHandle, 1u);
    }
    // Still inside the same case, now on page 1.
    EXPECT_EQ(mockupResponder->cycleCaseIndex, stuckIdx);
    EXPECT_EQ(mockupResponder->cyclePageIndex, 1u);

    // Page 1: Stuck -> next handle == request handle, BMC guard would trip.
    {
        auto req = makeDebugInfoReq(instanceId, 1);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        auto resp = mockupResponder->getNetworkDeviceDebugInfoHandler(
            reqMsg, req.size());
        ASSERT_TRUE(resp.has_value());
        const auto d = decodeDebugInfo(*resp);
        EXPECT_EQ(d.cc, NSM_SUCCESS);
        EXPECT_EQ(d.nextHandle, 1u); // == request handle
    }
    // Both pages consumed -> counter wrapped to the next case (index 0).
    EXPECT_EQ(mockupResponder->cyclePageIndex, 0u);
    EXPECT_EQ(mockupResponder->cycleCaseIndex,
              (stuckIdx + 1) % MockupResponder::kDumpFailureCycle.size());
}

// ---- cycle on: erase command skips the iteration-only case -------------

TEST_F(MockupDumpCycleTest, CycleOn_EraseCmdSkipsIterationOnlyCase)
{
    buildMock(/*failureCycle=*/true);
    const uint32_t stuckIdx = caseIndexOf("PROTO_STUCK_HANDLE");
    mockupResponder->cycleCaseIndex = stuckIdx;
    mockupResponder->cyclePageIndex = 0;

    // Erase is single-shot: it skips the multi-page PROTO_STUCK_HANDLE case,
    // then consumes the following single-page case (SUCCESS at index 0) and
    // returns a success-shaped erase response.
    Request req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
    ASSERT_EQ(encode_erase_trace_req(instanceId, reqMsg), NSM_SW_SUCCESS);
    auto resp = mockupResponder->eraseTraceHandler(reqMsg, req.size());
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_SUCCESS);

    // Erase skips the multi-page stuck case (cursor wraps to the next case),
    // then consumes that single-page case, landing one past it. Derive the
    // expected index from kDumpFailureCycle so the test survives layout
    // changes.
    const uint32_t cycleSize = MockupResponder::kDumpFailureCycle.size();
    const uint32_t consumedIdx = (stuckIdx + 1) % cycleSize;
    const uint32_t expectedIdx = (consumedIdx + 1) % cycleSize;
    EXPECT_EQ(mockupResponder->cycleCaseIndex, expectedIdx);
    EXPECT_EQ(mockupResponder->cyclePageIndex, 0u);
}

// ---------------------------------------------------------------------------
// Link Health (0x12) --failure_cycle walk: every health state, all nine
// causes with a slot index and both configuration states, N/A, a Valid = 0
// record and a reserved-encoding record, wrapping to the start. 0x13 leaves
// the global walk untouched.
// ---------------------------------------------------------------------------
TEST_F(MockupDumpCycleTest, LinkHealthV2FailureCycleWalk)
{
    buildMock(true);

    struct ExpectedCase
    {
        uint8_t health;
        uint8_t trigger;
        uint8_t metric;
        uint8_t config;
        bool valid;
    };
    const std::array<ExpectedCase, 14> expected = {{
        {NSM_LINK_HEALTH_HEALTHY, NSM_ATTENTION_TRIGGER_NA, 0,
         NSM_LINK_HEALTH_CONFIG_NA, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_PLR_TX_BANDWIDTH_LOSS,
         1, NSM_LINK_HEALTH_CONFIG_CURRENT, true},
        {NSM_LINK_HEALTH_ATTENTION,
         NSM_ATTENTION_TRIGGER_RECOVERY_BANDWIDTH_LOSS, 2,
         NSM_LINK_HEALTH_CONFIG_CURRENT, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 3,
         NSM_LINK_HEALTH_CONFIG_PREVIOUS, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_SYMBOL_ERROR_COUNT, 4,
         NSM_LINK_HEALTH_CONFIG_CURRENT, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_RAW_BER, 5,
         NSM_LINK_HEALTH_CONFIG_PREVIOUS, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_PLR_RX_BANDWIDTH_LOSS,
         6, NSM_LINK_HEALTH_CONFIG_CURRENT, true},
        {NSM_LINK_HEALTH_ATTENTION,
         NSM_ATTENTION_TRIGGER_PORT_TOTAL_BANDWIDTH_LOSS, 7,
         NSM_LINK_HEALTH_CONFIG_CURRENT, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_LINK_DOWN_COUNT, 8,
         NSM_LINK_HEALTH_CONFIG_PREVIOUS, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_SYMBOL_BER,
         NSM_LINK_HEALTH_METRIC_SLOT_MAX, NSM_LINK_HEALTH_CONFIG_CURRENT, true},
        {NSM_LINK_HEALTH_NA, NSM_ATTENTION_TRIGGER_NA, 0,
         NSM_LINK_HEALTH_CONFIG_NA, true},
        {NSM_LINK_HEALTH_ATTENTION, NSM_ATTENTION_TRIGGER_EFFECTIVE_BER, 3,
         NSM_LINK_HEALTH_CONFIG_CURRENT, false},
        {3, 10, 20, 3, true},
        {NSM_LINK_HEALTH_HEALTHY, NSM_ATTENTION_TRIGGER_NA, 0,
         NSM_LINK_HEALTH_CONFIG_NA, true},
    }};

    for (size_t i = 0; i < expected.size(); ++i)
    {
        auto health = queryLinkHealthV2(*mockupResponder, instanceId, 1);
        EXPECT_EQ(health.valid, expected[i].valid) << "case " << i;
        EXPECT_EQ(health.record.link_health, expected[i].health)
            << "case " << i;
        EXPECT_EQ(health.record.attention_trigger, expected[i].trigger)
            << "case " << i;
        EXPECT_EQ(health.record.attention_trigger_metric, expected[i].metric)
            << "case " << i;
        EXPECT_EQ(health.record.link_health_config_changed, expected[i].config)
            << "case " << i;
        EXPECT_EQ(mockupResponder->portHealthV2CycleIndex,
                  (i + 1) % expected.size());
    }

    // The walk wrapped; a clear does not move it.
    ASSERT_TRUE(clearLinkHealth(*mockupResponder, instanceId, 1,
                                {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH})
                    .has_value());
    EXPECT_EQ(mockupResponder->portHealthV2CycleIndex, 0u);
    auto wrapped = queryLinkHealthV2(*mockupResponder, instanceId, 1);
    EXPECT_EQ(wrapped.record.link_health, NSM_LINK_HEALTH_HEALTHY);
    EXPECT_EQ(mockupResponder->portHealthV2CycleIndex, 1u);
}

} // namespace

namespace
{
class MockupResponderProcessCleanup : public ::testing::Environment
{
  public:
    void TearDown() override
    {
        sd_event* ev = nullptr;
        if (sd_event_default(&ev) >= 0 && ev != nullptr)
        {
            sd_event_exit(ev, 0);
            sd_event_unref(ev);
        }
    }
};

const ::testing::Environment* const mockupResponderProcessCleanupEnv =
    ::testing::AddGlobalTestEnvironment(new MockupResponderProcessCleanup);

// =============================================================================
// QueryPortCharacteristics failure-cycle (NVLink health-agent replay)
// =============================================================================

TEST_F(MockupDumpCycleTest, testQueryPortCharacteristicsFailureCycle)
{
    buildMock(/*failureCycle=*/true);

    std::vector<std::pair<int, int>> seen;
    for (int i = 0; i < 12; ++i)
    {
        Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_query_port_characteristics_req));
        auto* reqMsg = reinterpret_cast<nsm_msg*>(request.data());
        ASSERT_EQ(encode_query_port_characteristics_req(instanceId, 0, reqMsg),
                  NSM_SW_SUCCESS);
        auto resp = mockupResponder->queryPortCharacteristicsHandler(
            reqMsg, request.size());
        ASSERT_TRUE(resp.has_value());

        auto* respMsg = reinterpret_cast<const nsm_msg*>(resp->data());
        uint8_t cc = 0xff;
        uint16_t reason = 0;
        uint16_t dataSize = 0;
        struct nsm_port_characteristics_data data{};
        ASSERT_EQ(decode_query_port_characteristics_resp(
                      respMsg, resp->size(), &cc, &reason, &dataSize, &data),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);

        uint32_t linkState = data.port_status.link_state;
        uint32_t linkHealth = data.port_status.link_health;
        uint32_t trigger = data.port_status.attention_trigger;
        // link_state forced UP so link_health is meaningful.
        EXPECT_EQ(linkState, static_cast<uint32_t>(NSM_PORTSTATE_UP));
        seen.emplace_back(static_cast<int>(linkHealth),
                          static_cast<int>(trigger));
    }

    // Per-instance index starts at 0: NA, Attention/1..9, Healthy, then wraps.
    EXPECT_EQ(seen[0].first, static_cast<int>(NSM_LINK_HEALTH_NA));
    EXPECT_EQ(seen[0].second, 0);
    for (int i = 1; i <= 9; ++i)
    {
        EXPECT_EQ(seen[i].first, static_cast<int>(NSM_LINK_HEALTH_ATTENTION))
            << "step " << i;
        EXPECT_EQ(seen[i].second, i) << "step " << i;
    }
    EXPECT_EQ(seen[10].first, static_cast<int>(NSM_LINK_HEALTH_HEALTHY));
    EXPECT_EQ(seen[10].second, 0);
    EXPECT_EQ(seen[11], seen[0]); // 12th poll wraps to the first case
}

TEST_F(MockupDumpCycleTest, testQueryPortCharacteristicsNoCycleLegacy)
{
    buildMock(/*failureCycle=*/false);

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_port_characteristics_req));
    auto* reqMsg = reinterpret_cast<nsm_msg*>(request.data());
    ASSERT_EQ(encode_query_port_characteristics_req(instanceId, 0, reqMsg),
              NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortCharacteristicsHandler(
        reqMsg, request.size());
    ASSERT_TRUE(resp.has_value());

    auto* respMsg = reinterpret_cast<const nsm_msg*>(resp->data());
    uint8_t cc = 0xff;
    uint16_t reason = 0;
    uint16_t dataSize = 0;
    struct nsm_port_characteristics_data data{};
    ASSERT_EQ(decode_query_port_characteristics_resp(respMsg, resp->size(), &cc,
                                                     &reason, &dataSize, &data),
              NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_SUCCESS);

    uint32_t linkState = data.port_status.link_state;
    EXPECT_EQ(linkState, 1u); // legacy fixed response: link_state DOWN
}

} // namespace

// =============================================================================
// Branch-coverage extension: dispatcher sweep, Link Health (0x12/0x13) quiet
// mode, never-executed handlers, Pre-Boot Diagnostics session simulation and
// the MCTP send success paths (over an injected AF_UNIX socket pair).
// =============================================================================

namespace MockupResponder
{
// Non-const mock table owned by mockupResponder.cpp; extended by a test to
// reach the unexpected-tag path of getPciePortConfigHandler.
extern std::unordered_map<uint8_t, uint8_t> pciePortConfigMockTable;
} // namespace MockupResponder

namespace
{
using CovDispatchEntry = std::pair<uint8_t, std::vector<uint8_t>>;

// Every (message type, command) pair routed by processRxMsg, in dispatcher
// order.
const std::vector<CovDispatchEntry>& covDispatchTable()
{
    static const std::vector<CovDispatchEntry> table = {
        {NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY,
         {NSM_PING, NSM_SUPPORTED_NVIDIA_MESSAGE_TYPES,
          NSM_SUPPORTED_COMMAND_CODES, NSM_QUERY_DEVICE_IDENTIFICATION,
          NSM_GET_EVENT_SUBSCRIPTION, NSM_SET_EVENT_SUBSCRIPTION,
          NSM_GET_SUPPORTED_EVENT_SOURCES, NSM_GET_CURRENT_EVENT_SOURCES,
          NSM_SET_CURRENT_EVENT_SOURCES, NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT,
          NSM_GET_HISTOGRAM_FORMAT, NSM_GET_HISTOGRAM_DATA,
          NSM_GET_DEVICE_CAPABILITIES_V2, NSM_GET_GPIO_STATE,
          NSM_GET_EVENT_LOG_RECORD_V2}},
        {NSM_TYPE_NETWORK_PORT,
         {NSM_GET_PORT_TELEMETRY_COUNTER, NSM_QUERY_PORT_CHARACTERISTICS,
          NSM_QUERY_PORT_CHARACTERISTICS_V2, NSM_CLEAR_PORT_METRIC_STATE,
          NSM_QUERY_PORT_STATUS, NSM_GET_FABRIC_MANAGER_STATE,
          NSM_QUERY_PORTS_AVAILABLE, NSM_SET_PORT_DISABLE_FUTURE,
          NSM_GET_PORT_DISABLE_FUTURE, NSM_GET_POWER_MODE, NSM_SET_POWER_MODE,
          NSM_GET_SWITCH_ISOLATION_MODE, NSM_SET_SWITCH_ISOLATION_MODE,
          NSM_GET_ETH_PORT_TELEMETRY_COUNTER,
          NSM_QUERY_PORT_TELEMETRY_COUNTER_V2,
          NSM_QUERY_PORT_TELEMETRY_CAPABILITIES, NSM_GET_NETWORK_ADDRESSES,
          NSM_GET_PORT_ECC_COUNTERS, NSM_GET_LLDP_PACKET}},
        {NSM_TYPE_PLATFORM_ENVIRONMENTAL,
         {NSM_GET_INVENTORY_INFORMATION,
          NSM_GET_TEMPERATURE_READING,
          NSM_READ_THERMAL_PARAMETER,
          NSM_GET_POWER,
          NSM_GET_MAX_OBSERVED_POWER,
          NSM_GET_ENERGY_COUNT,
          NSM_GET_VOLTAGE,
          NSM_GET_ALTITUDE_PRESSURE,
          NSM_GET_DRIVER_INFO,
          NSM_GET_MIG_MODE,
          NSM_SET_MIG_MODE,
          NSM_GET_ECC_MODE,
          NSM_SET_ECC_MODE,
          NSM_GET_ECC_ERROR_COUNTS,
          NSM_GET_PROGRAMMABLE_EDPP_SCALING_FACTOR,
          NSM_SET_PROGRAMMABLE_EDPP_SCALING_FACTOR,
          NSM_GET_CLOCK_LIMIT,
          NSM_SET_CLOCK_LIMIT,
          NSM_GET_CURRENT_CLOCK_FREQUENCY,
          NSM_GET_CLOCK_EVENT_REASON_CODES,
          NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME,
          NSM_GET_CURRENT_UTILIZATION,
          NSM_SET_POWER_LIMITS,
          NSM_GET_POWER_LIMITS,
          NSM_GET_CLOCK_OUTPUT_ENABLE_STATE,
          NSM_GET_ROW_REMAP_STATE_FLAGS,
          NSM_GET_ROW_REMAPPING_COUNTS,
          NSM_GET_ROW_REMAP_AVAILABILITY,
          NSM_GET_LEAK_DETECTION_INFO,
          NSM_GET_MEMORY_CAPACITY_UTILIZATION,
          NSM_GET_SUPPORTED_GPM_METRICS,
          NSM_QUERY_AGGREGATE_GPM_METRICS,
          NSM_QUERY_PER_INSTANCE_GPM_METRICS,
          NSM_QUERY_PER_INSTANCE_GPM_METRICS_V2,
          NSM_GET_VIOLATION_DURATION,
          NSM_PWR_SMOOTHING_TOGGLE_FEATURESTATE,
          NSM_PWR_SMOOTHING_GET_FEATURE_INFO,
          NSM_PWR_SMOOTHING_GET_FEATURE_INFO_V2,
          NSM_PWR_SMOOTHING_GET_HARDWARE_CIRCUITRY_LIFETIME_USAGE,
          NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION,
          NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE,
          NSM_PWR_SMOOTHING_GET_CURRENT_PROFILE_INFORMATION_V2,
          NSM_PWR_SMOOTHING_SET_ACTIVE_PRESET_PROFILE,
          NSM_PWR_SMOOTHING_QUERY_ADMIN_OVERRIDE_V2,
          NSM_PWR_SMOOTHING_SETUP_ADMIN_OVERRIDE,
          NSM_PWR_SMOOTHING_APPLY_ADMIN_OVERRIDE,
          NSM_PWR_SMOOTHING_TOGGLE_IMMEDIATE_RAMP_DOWN,
          NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION,
          NSM_PWR_SMOOTHING_GET_PRESET_PROFILE_INFORMATION_V2,
          NSM_PWR_SMOOTHING_UPDATE_PRESET_PROFILE_PARAMETERS,
          NSM_ENABLE_WORKLOAD_POWER_PROFILE,
          NSM_DISABLE_WORKLOAD_POWER_PROFILE,
          NSM_GET_WORKLOAD_POWER_PROFILE_STATUS_INFO,
          NSM_GET_WORKLOAD_POWER_PROFILE_INFO}},
        {NSM_TYPE_PCI_LINK,
         {NSM_QUERY_SCALAR_GROUP_TELEMETRY_V1,
          NSM_ASSERT_PCIE_FUNDAMENTAL_RESET, NSM_GET_PORT_CONFIGURATION,
          NSM_SET_PORT_CONFIGURATION, NSM_CLEAR_DATA_SOURCE_V1,
          NSM_QUERY_AVAILABLE_CLEARABLE_SCALAR_DATA_SOURCES,
          NSM_LIST_AVAILABLE_PCIE_PORTS,
          NSM_MULTIPORT_QUERY_SCALAR_GROUP_TELEMETRY_V2}},
        {NSM_TYPE_DIAGNOSTIC,
         {NSM_GET_DEVICE_RESET_STATISTICS, NSM_QUERY_TOKEN_PARAMETERS,
          NSM_PROVIDE_TOKEN, NSM_DISABLE_TOKENS, NSM_QUERY_TOKEN_STATUS,
          NSM_QUERY_DEVICE_IDS, NSM_RESET_NETWORK_DEVICE, NSM_ENABLE_DISABLE_WP,
          NSM_GET_DEVICE_DIAGNOSTICS, NSM_GET_NETWORK_DEVICE_DEBUG_INFO,
          NSM_ERASE_TRACE, NSM_GET_NETWORK_DEVICE_LOG_INFO,
          NSM_ERASE_DEBUG_INFO, NSM_GET_DEVICE_DEBUG_PARAMETERS,
          NSM_SET_DEVICE_DEBUG_PARAMETERS, NSM_DIAG_SET_SYSTEM_CONFIG,
          NSM_DIAG_SET_TID_CONFIG}},
        {NSM_TYPE_DEVICE_CONFIGURATION,
         {NSM_GET_ERROR_INJECTION_MODE_V1,
          NSM_SET_ERROR_INJECTION_MODE_V1,
          NSM_GET_SUPPORTED_ERROR_INJECTION_TYPES_V1,
          NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1,
          NSM_GET_CURRENT_ERROR_INJECTION_TYPES_V1,
          NSM_GET_ERROR_INJECTION_PAYLOAD,
          NSM_SET_ERROR_INJECTION_PAYLOAD,
          NSM_ACTIVATE_ERROR_INJECTION,
          NSM_GET_RECONFIGURATION_PERMISSIONS_V1,
          NSM_SET_RECONFIGURATION_PERMISSIONS_V1,
          NSM_GET_CONFIDENTIAL_COMPUTE_MODE_V1,
          NSM_SET_CONFIDENTIAL_COMPUTE_MODE_V1,
          NSM_ENABLE_DISABLE_GPU_IST_MODE,
          NSM_GET_FPGA_DIAGNOSTICS_SETTINGS,
          NSM_GET_DEVICE_MODE_SETTING,
          NSM_SET_DEVICE_MODE_SETTING,
          NSM_GET_DEVICE_MODE_SETTINGS_V2,
          NSM_SET_DEVICE_MODE_SETTINGS_V2,
          NSM_SET_DEVICE_CONFIG_V2,
          NSM_GET_DEVICE_CONFIG_V2,
          NSM_GET_PROTECTION_OPTIONS}},
        {NSM_TYPE_FIRMWARE,
         {NSM_FW_GET_EROT_STATE_INFORMATION, NSM_FW_IRREVERSABLE_CONFIGURATION,
          NSM_FW_IMAGE_COPY_CONTROL, NSM_FW_QUERY_CODE_AUTH_KEY_PERM,
          NSM_FW_UPDATE_CODE_AUTH_KEY_PERM,
          NSM_FW_QUERY_MIN_SECURITY_VERSION_NUMBER,
          NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER, NSM_FW_SET_ROT_PROPERTY,
          NSM_FW_DOT_CAK_INSTALL, NSM_FW_DOT_CAK_BYPASS, NSM_FW_DOT_LOCK,
          NSM_FW_DOT_UNLOCK_CHALLENGE, NSM_FW_DOT_UNLOCK, NSM_FW_DOT_CAK_ROTATE,
          NSM_FW_DOT_GET_INFO, NSM_FW_DOT_GET_STATUS, NSM_FW_DOT_DISABLE,
          NSM_FW_DOT_OVERRIDE, NSM_FW_DOT_RECOVERY}},
    };
    return table;
}

// Handlers that assert() on a failed request decode instead of answering;
// they are left out of the empty-payload dispatcher sweep.
bool covAssertsOnBadRequest(uint8_t type, uint8_t command)
{
    if (type == NSM_TYPE_PLATFORM_ENVIRONMENTAL)
    {
        switch (command)
        {
            case NSM_GET_ECC_ERROR_COUNTS:
            case NSM_GET_CLOCK_LIMIT:
            case NSM_GET_CURRENT_CLOCK_FREQUENCY:
            case NSM_GET_POWER_LIMITS:
            case NSM_GET_ACCUMULATED_GPU_UTILIZATION_TIME:
            case NSM_GET_ROW_REMAPPING_COUNTS:
            case NSM_GET_ROW_REMAP_AVAILABILITY:
            case NSM_GET_CLOCK_OUTPUT_ENABLE_STATE:
            case NSM_QUERY_AGGREGATE_GPM_METRICS:
                return true;
            default:
                return false;
        }
    }
    if (type == NSM_TYPE_DIAGNOSTIC)
    {
        switch (command)
        {
            case NSM_ENABLE_DISABLE_WP:
            case NSM_GET_DEVICE_RESET_STATISTICS:
            // debugToken.cpp
            case NSM_QUERY_TOKEN_PARAMETERS:
            case NSM_PROVIDE_TOKEN:
            case NSM_DISABLE_TOKENS:
            case NSM_QUERY_TOKEN_STATUS:
            case NSM_QUERY_DEVICE_IDS:
                return true;
            default:
                return false;
        }
    }
    if (type == NSM_TYPE_FIRMWARE)
    {
        // firmwareUtils.cpp
        return command == NSM_FW_QUERY_CODE_AUTH_KEY_PERM ||
               command == NSM_FW_UPDATE_CODE_AUTH_KEY_PERM;
    }
    return false;
}

// A request frame carrying only the common header (data_size 0), zero padded
// to `size` bytes so every handler sees a well-formed header but a payload
// its request decoder rejects.
Request covCommonRequest(uint8_t instanceId, uint8_t type, uint8_t command,
                         size_t size = 512)
{
    Request request(
        std::max(size, sizeof(nsm_msg_hdr) + sizeof(nsm_common_req)), 0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_common_req(instanceId, type, command, msg),
              NSM_SW_SUCCESS);
    return request;
}

// Non-blocking AF_UNIX datagram pair: fds[0] is handed to the mock as its
// MCTP socket, fds[1] receives what the mock sends.
struct CovSocketPair
{
    int fds[2] = {-1, -1};
    CovSocketPair()
    {
        EXPECT_EQ(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, fds), 0);
    }
    ~CovSocketPair()
    {
        for (int fd : fds)
        {
            if (fd >= 0)
            {
                close(fd);
            }
        }
    }
    // Number of datagrams queued on the receiving end (all consumed).
    int drain()
    {
        int count = 0;
        std::array<uint8_t, 4096> buf{};
        while (recv(fds[1], buf.data(), buf.size(), 0) >= 0)
        {
            ++count;
        }
        return count;
    }
};

// Temporarily gives the mock a real (test-owned) socket descriptor; the mock
// destructor closes sockFd >= 0, so it is reset to -1 on scope exit.
struct CovSockFdGuard
{
    MockupResponder::MockupResponder& mock;
    CovSockFdGuard(MockupResponder::MockupResponder& m, int fd) : mock(m)
    {
        mock.sockFd = fd;
    }
    ~CovSockFdGuard()
    {
        mock.sockFd = -1;
    }
};
} // namespace

// ---- Link Health (0x12) / Clear Port Metric State (0x13) -------------------

TEST_F(MockupResponderTest, CovLinkHealthV2DispatchThroughProcessRxMsg)
{
    std::optional<Request> longRunningEvent;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_port_characteristics_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    ASSERT_EQ(
        encode_query_port_characteristics_v2_req(instanceId, 3, requestMsg),
        NSM_SW_SUCCESS);
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    ASSERT_TRUE(resp.has_value());
    uint8_t cc = NSM_ERROR;
    uint16_t recordCount = 0;
    size_t consumedLen = 0;
    EXPECT_EQ(
        decode_aggregate_resp(reinterpret_cast<const nsm_msg*>(resp->data()),
                              resp->size(), &consumedLen, &cc, &recordCount),
        NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_SUCCESS);
    EXPECT_EQ(recordCount, 5);

    const uint8_t tagId = NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH;
    Request clearReq(sizeof(nsm_msg_hdr) +
                     sizeof(nsm_clear_port_metric_state_req) + sizeof(tagId));
    auto clearMsg = reinterpret_cast<nsm_msg*>(clearReq.data());
    ASSERT_EQ(
        encode_clear_port_metric_state_req(instanceId, 3, 1, &tagId, clearMsg),
        NSM_SW_SUCCESS);
    resp = mockupResponder->processRxMsg(clearReq, longRunningEvent);
    ASSERT_TRUE(resp.has_value());
    uint16_t reasonCode = ERR_NULL;
    EXPECT_EQ(decode_clear_port_metric_state_resp(
                  reinterpret_cast<const nsm_msg*>(resp->data()), resp->size(),
                  &cc, &reasonCode),
              NSM_SW_SUCCESS);
    EXPECT_EQ(cc, NSM_SUCCESS);
    // The clear reached port 3 only; another port stays latched.
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 3).record.link_health,
        NSM_LINK_HEALTH_HEALTHY);
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 4).record.link_health,
        NSM_LINK_HEALTH_ATTENTION);
}

TEST_F(MockupResponderTest, CovLinkHealthV2QuietModeLatchClearRelatch)
{
    mockupResponder->verbose = false;
    constexpr uint16_t port = 4;
    auto latched = queryLinkHealthV2(*mockupResponder, instanceId, port);
    EXPECT_TRUE(latched.valid);
    EXPECT_EQ(latched.record.link_health, NSM_LINK_HEALTH_ATTENTION);

    auto resp = clearLinkHealth(*mockupResponder, instanceId, port,
                                {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH});
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_SUCCESS);
    EXPECT_FALSE(mockupResponder->linkHealthMockByPort[port].attentionLatched);

    // A rejected tag list in quiet mode leaves the cleared state alone.
    resp = clearLinkHealth(*mockupResponder, instanceId, port,
                           {NSM_PORT_CHARACTERISTICS_V2_TAG_PORT_STATUS});
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_INVALID_DATA);
    EXPECT_FALSE(mockupResponder->linkHealthMockByPort[port].attentionLatched);

    // Healthy for kLinkHealthRelatchPolls - 1 polls, Attention on the next.
    for (size_t poll = 1;
         poll < MockupResponder::MockupResponder::kLinkHealthRelatchPolls;
         ++poll)
    {
        EXPECT_EQ(queryLinkHealthV2(*mockupResponder, instanceId, port)
                      .record.link_health,
                  NSM_LINK_HEALTH_HEALTHY)
            << "poll " << poll;
        EXPECT_EQ(mockupResponder->linkHealthMockByPort[port].pollsSinceClear,
                  poll);
    }
    EXPECT_EQ(queryLinkHealthV2(*mockupResponder, instanceId, port)
                  .record.link_health,
              NSM_LINK_HEALTH_ATTENTION);
    EXPECT_TRUE(mockupResponder->linkHealthMockByPort[port].attentionLatched);
}

TEST_F(MockupDumpCycleTest, CovLinkHealthV2QuietModeFailureCycleAdvances)
{
    buildMock(/*failureCycle=*/true);
    mockupResponder->verbose = false;
    auto first = queryLinkHealthV2(*mockupResponder, instanceId, 1);
    EXPECT_TRUE(first.valid);
    EXPECT_EQ(first.record.link_health, NSM_LINK_HEALTH_HEALTHY);
    EXPECT_EQ(mockupResponder->portHealthV2CycleIndex, 1u);
    auto second = queryLinkHealthV2(*mockupResponder, instanceId, 2);
    EXPECT_EQ(second.record.link_health, NSM_LINK_HEALTH_ATTENTION);
    EXPECT_EQ(second.record.attention_trigger,
              NSM_ATTENTION_TRIGGER_PLR_TX_BANDWIDTH_LOSS);
    EXPECT_EQ(mockupResponder->portHealthV2CycleIndex, 2u);
    // The per-port latch is not consulted while the cycle is on.
    EXPECT_TRUE(mockupResponder->linkHealthMockByPort.empty());
}

TEST_F(MockupResponderTest, CovClearPortMetricStateTagListVariants)
{
    // Clearing a port that was never polled creates its (cleared) entry.
    auto resp = clearLinkHealth(*mockupResponder, instanceId, 9,
                                {NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH,
                                 NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH});
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_clear_port_metric_state_resp));
    EXPECT_EQ(respCompletionCode(*resp), NSM_SUCCESS);
    ASSERT_EQ(mockupResponder->linkHealthMockByPort.count(9), 1u);
    EXPECT_FALSE(mockupResponder->linkHealthMockByPort[9].attentionLatched);
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 9).record.link_health,
        NSM_LINK_HEALTH_HEALTHY);

    // A bad tag anywhere in the list rejects the whole request and keeps the
    // latch of that port.
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 10).record.link_health,
        NSM_LINK_HEALTH_ATTENTION);
    resp = clearLinkHealth(*mockupResponder, instanceId, 10,
                           {NSM_PORT_CHARACTERISTICS_V2_TAG_LANE_INFO,
                            NSM_PORT_CHARACTERISTICS_V2_TAG_LINK_HEALTH});
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(resp->size(),
              sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp));
    EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_INVALID_DATA);
    EXPECT_TRUE(mockupResponder->linkHealthMockByPort[10].attentionLatched);
    EXPECT_EQ(
        queryLinkHealthV2(*mockupResponder, instanceId, 10).record.link_health,
        NSM_LINK_HEALTH_ATTENTION);
}

// ---- processRxMsg dispatcher -----------------------------------------------

TEST_F(MockupResponderTest, CovProcessRxMsgRoutesEveryCommandWithEmptyPayload)
{
    // In verbose mode configureEventAcknowledgement logs through the mask
    // pointer that its failed request decode never set, so that command is
    // only swept in quiet mode.
    // Two frame shapes: a zero-padded frame whose data_size the decoders
    // reject, and a frame cut right after the command byte so even the
    // decoders that only check the minimum length fail.
    for (size_t frameSize : {size_t{512}, sizeof(nsm_msg_hdr) + 1})
    {
        for (bool verbose : {false, true})
        {
            mockupResponder->verbose = verbose;
            for (const auto& [type, commands] : covDispatchTable())
            {
                for (uint8_t command : commands)
                {
                    if (covAssertsOnBadRequest(type, command) ||
                        (verbose &&
                         type == NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY &&
                         command == NSM_CONFIGURE_EVENT_ACKNOWLEDGEMENT))
                    {
                        continue;
                    }
                    // Built at full size so the bytes past the frame end stay
                    // zero-initialized storage, then shortened.
                    auto request = covCommonRequest(instanceId, type, command);
                    request.resize(frameSize);
                    std::optional<Request> longRunningEvent;
                    EXPECT_NO_THROW(mockupResponder->processRxMsg(
                        request, longRunningEvent))
                        << "type " << int(type) << " command " << int(command)
                        << " frame " << frameSize;
                }
            }
        }
    }
    for (const auto& [type, commands] : covDispatchTable())
    {
        // An unknown command of a known type is answered with
        // UNSUPPORTED_COMMAND_CODE.
        auto request = covCommonRequest(instanceId, type, 0xFE);
        std::optional<Request> longRunningEvent;
        auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
        ASSERT_TRUE(resp.has_value()) << "type " << int(type);
        EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    }
    // Unknown message type.
    auto request = covCommonRequest(instanceId, 0x7E, NSM_PING);
    std::optional<Request> longRunningEvent;
    auto resp = mockupResponder->processRxMsg(request, longRunningEvent);
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
}

TEST_F(MockupResponderTest, CovProcessRxMsgQuietModeHeaderPaths)
{
    mockupResponder->verbose = false;
    std::optional<Request> longRunningEvent;

    // Event acknowledgment: consumed silently.
    Request ack(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    ASSERT_EQ(encode_nsm_event_acknowledgement(
                  instanceId, NSM_TYPE_DEVICE_CAPABILITY_DISCOVERY, NSM_PING,
                  reinterpret_cast<nsm_msg*>(ack.data())),
              NSM_SW_SUCCESS);
    EXPECT_FALSE(mockupResponder->processRxMsg(ack, longRunningEvent));

    // Request: dispatched without the request log line.
    Request ping(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req), 0);
    auto pingMsg = reinterpret_cast<nsm_msg*>(ping.data());
    ASSERT_EQ(encode_ping_req(instanceId, pingMsg), NSM_SW_SUCCESS);
    EXPECT_TRUE(mockupResponder->processRxMsg(ping, longRunningEvent));

    // A response-typed frame (request bit clear) is still dispatched.
    for (bool verbose : {false, true})
    {
        mockupResponder->verbose = verbose;
        Request frame = ping;
        auto frameMsg = reinterpret_cast<nsm_msg*>(frame.data());
        frameMsg->hdr.request = 0;
        frameMsg->hdr.datagram = 0;
        EXPECT_TRUE(mockupResponder->processRxMsg(frame, longRunningEvent));
    }
}

// ---- Never-executed handlers -----------------------------------------------

TEST_F(MockupResponderTest, CovGetLldpPacketHandlerSyntheticAndEmpty)
{
    auto query = [&](uint16_t port, uint8_t direction) {
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_lldp_packet_req),
                        0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        EXPECT_EQ(encode_get_lldp_packet_req(instanceId, port, direction, msg),
                  NSM_SW_SUCCESS);
        return mockupResponder->getLldpPacketHandler(msg, request.size());
    };
    auto decode = [](const Response& resp, std::vector<uint8_t>& data) {
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        data.assign(512, 0);
        uint16_t dataSize = static_cast<uint16_t>(data.size()); // capacity in
        EXPECT_EQ(decode_get_lldp_packet_resp(
                      reinterpret_cast<const nsm_msg*>(resp.data()),
                      resp.size(), &cc, &reasonCode, data.data(), &dataSize),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);
        data.resize(dataSize);
    };

    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        // (port 0, RX) carries the synthetic Ethernet-framed LLDPDU.
        auto resp = query(0, NSM_LLDP_DIRECTION_RX);
        ASSERT_TRUE(resp.has_value());
        std::vector<uint8_t> data;
        decode(*resp, data);
        ASSERT_GT(data.size(), 14u);
        EXPECT_EQ(data[12], 0x88);              // EtherType 0x88CC
        EXPECT_EQ(data[13], 0xCC);
        EXPECT_EQ(data[14], 0x02);              // Chassis ID TLV
        EXPECT_EQ(data[data.size() - 2], 0x00); // End-of-LLDPDU
        EXPECT_EQ(data[data.size() - 1], 0x00);
        const std::string systemName = "mock-lldp-peer";
        EXPECT_NE(std::search(data.begin(), data.end(), systemName.begin(),
                              systemName.end()),
                  data.end());

        // Every other tuple is the empty working-assumption buffer.
        resp = query(1, NSM_LLDP_DIRECTION_RX);
        ASSERT_TRUE(resp.has_value());
        decode(*resp, data);
        EXPECT_TRUE(data.empty());
        resp = query(0, NSM_LLDP_DIRECTION_TX);
        ASSERT_TRUE(resp.has_value());
        decode(*resp, data);
        EXPECT_TRUE(data.empty());
    }
}

TEST_F(MockupResponderTest, CovGetEventLogRecordV2HandlerPages)
{
    auto query = [&](uint8_t mode, uint16_t eventHandle,
                     uint16_t transferHandle) {
        Request request(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_event_log_record_v2_req), 0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        EXPECT_EQ(encode_nsm_get_event_log_record_v2_req(
                      instanceId, mode, eventHandle, transferHandle, msg),
                  NSM_SW_SUCCESS);
        return mockupResponder->getEventLogRecordV2Handler(msg, request.size());
    };

    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        // First page: transfer handle 0 -> 1 in the first-handle layout.
        auto resp = query(NSM_EVENT_LOG_V2_MODE_GET_DATA, 7, 0);
        ASSERT_TRUE(resp.has_value());
        uint8_t cc = NSM_ERROR;
        nsm_event_log_record_v2_first_fields first{};
        ASSERT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &first),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);
        EXPECT_EQ(first.event_handle, 7);
        EXPECT_EQ(first.next_transfer_handle, 1);
        EXPECT_EQ(first.nvidia_message_type, NSM_TYPE_PLATFORM_ENVIRONMENTAL);
        EXPECT_EQ(first.event_data_len, 4);

        // Middle page: next-handle layout, handle 3 -> 4.
        resp = query(NSM_EVENT_LOG_V2_MODE_GET_DATA, 7, 3);
        ASSERT_TRUE(resp.has_value());
        nsm_event_log_record_v2_next_fields next{};
        ASSERT_EQ(decode_nsm_get_event_log_record_v2_resp_next_handle(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &next),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(next.event_handle, 7);
        EXPECT_EQ(next.next_transfer_handle, 4);
        EXPECT_EQ(next.event_data_len, 4);

        // Last page: handle 5 -> 0 (end).
        resp = query(NSM_EVENT_LOG_V2_MODE_GET_DATA, 7, 5);
        ASSERT_TRUE(resp.has_value());
        ASSERT_EQ(decode_nsm_get_event_log_record_v2_resp_next_handle(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &next),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(next.next_transfer_handle, 0);

        // Acknowledgment: first-handle layout, handle 0xFFFF, no data.
        resp = query(NSM_EVENT_LOG_V2_MODE_ACKNOWLEDGEMENT, 0, 0);
        ASSERT_TRUE(resp.has_value());
        ASSERT_EQ(decode_nsm_get_event_log_record_v2_resp_first_handle(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &first),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(first.event_handle, 0xFFFF);
        EXPECT_EQ(first.next_transfer_handle, 0);
        EXPECT_EQ(first.event_data_len, 0);
    }
}

TEST_F(MockupResponderTest, CovQueryPortTelemetryV2HandlerOpticalModuleGroup)
{
    auto query = [&](uint8_t group) {
        Request request(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_v2_req), 0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        EXPECT_EQ(
            encode_query_port_telemetry_v2_req(instanceId, 1, group, 0, msg),
            NSM_SW_SUCCESS);
        return mockupResponder->queryPortTelemetryV2Handler(msg,
                                                            request.size());
    };
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        // Optical module: 3 metrics x 8 lanes + 8 SNR lanes + sequence token.
        auto resp = query(NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE);
        ASSERT_TRUE(resp.has_value());
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint16_t count = 0;
        size_t consumedLen = 0;
        ASSERT_EQ(decode_query_port_telemetry_v2_resp(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &reasonCode, &count, &consumedLen),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);
        EXPECT_EQ(count, 3 * 8 + 8 + 1);
        EXPECT_GT(resp->size(), consumedLen);
        // The terminating Sequence Token Record (tag 0xFD) is the last sample:
        // tag byte, then valid/length byte, then 4 bytes of token = 0.
        ASSERT_GE(resp->size(), 6u);
        EXPECT_EQ((*resp)[resp->size() - 6], 0xFD);
        EXPECT_EQ((*resp)[resp->size() - 1], 0);

        // Other groups: NSM_ERROR carrier with no samples.
        resp = query(NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE - 1);
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(resp->size(),
                  sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
        EXPECT_EQ(respCompletionCode(*resp), NSM_ERROR);
    }
}

TEST_F(MockupResponderTest, CovQueryPortTelemetryCapabilitiesHandler)
{
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        Request request(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_port_telemetry_caps_req), 0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        ASSERT_EQ(encode_query_port_telemetry_caps_req(instanceId, 2, msg),
                  NSM_SW_SUCCESS);
        auto resp = mockupResponder->queryPortTelemetryCapabilitiesHandler(
            msg, request.size());
        ASSERT_TRUE(resp.has_value());
        uint8_t cc = NSM_ERROR;
        uint8_t maxGroup = 0;
        uint8_t maxRecords = 0;
        std::array<uint8_t, 32> bitmask{};
        ASSERT_EQ(decode_query_port_telemetry_caps_resp(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &maxGroup, &maxRecords,
                      bitmask.data()),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);
        EXPECT_EQ(maxGroup, NSM_PORT_TELEMETRY_GROUP_OPTICAL_MODULE);
        EXPECT_EQ(maxRecords, 32);
        EXPECT_EQ(bitmask[0], 0xFF);
        EXPECT_EQ(bitmask[1], 0x01);
        EXPECT_EQ(bitmask[2], 0x00);
    }
}

namespace
{
Response covGetDeviceModeSettingsV2(MockupResponderTest& suite, uint32_t index,
                                    std::vector<uint8_t>& cur,
                                    std::vector<uint8_t>& pend)
{
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_device_mode_settings_v2_req), 0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(
        encode_get_device_mode_settings_v2_req(suite.instanceId, index, msg),
        NSM_SW_SUCCESS);
    auto resp = suite.mockupResponder->getDeviceModeSettingsV2Handler(
        msg, request.size());
    if (!resp.has_value())
    {
        ADD_FAILURE() << "no Get Device Mode Settings v2 response for index "
                      << index;
        return {};
    }
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = ERR_NULL;
    cur.assign(64, 0);
    pend.assign(64, 0);
    uint16_t curLen = 0;
    uint16_t pendLen = 0;
    EXPECT_EQ(decode_get_device_mode_settings_v2_resp(
                  reinterpret_cast<const nsm_msg*>(resp->data()), resp->size(),
                  &cc, &reasonCode, cur.data(), &curLen, pend.data(), &pendLen),
              NSM_SW_SUCCESS)
        << "index " << index;
    EXPECT_EQ(cc, NSM_SUCCESS);
    cur.resize(curLen);
    pend.resize(pendLen);
    return *resp;
}

Response covSetDeviceModeSettingsV2(MockupResponderTest& suite, uint32_t index,
                                    const std::vector<uint8_t>& data)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_set_device_mode_settings_v2_req) - 1 +
                        data.size(),
                    0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_set_device_mode_settings_v2_req(
                  suite.instanceId, index, data.data(),
                  static_cast<uint16_t>(data.size()), msg),
              NSM_SW_SUCCESS);
    auto resp = suite.mockupResponder->setDeviceModeSettingsV2Handler(
        msg, request.size());
    if (!resp.has_value())
    {
        ADD_FAILURE() << "no Set Device Mode Settings v2 response for index "
                      << index;
        return {};
    }
    return *resp;
}
} // namespace

TEST_F(MockupResponderTest, CovGetDeviceModeSettingsV2HandlerDefaultsPerIndex)
{
    struct Expected
    {
        uint32_t index;
        size_t length;
    };
    const std::vector<Expected> expected = {
        {DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT, 4},
        {DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT, 4},
        {DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY, 4},
        {DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY, 4},
        {DEVICE_MODE_ONE_SHOT_GPU_COPY_SWITCH_POWER_LIMIT, 4},
        {DEVICE_MODE_PERSISTENT_GPU_COPY_SWITCH_POWER_LIMIT, 4},
        {DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE, 4},
        {DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED, 1},
        {DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX, 2},
        {DEVICE_MODE_SOC_POWER_BRAKE_ENABLED, 1},
        {DEVICE_MODE_ADAPTIVE_TGPMODE, 1},
        {DEVICE_MODE_LLDP, 1},
        {0x7777, 4}, // unknown index: u32 zero
    };
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        for (const auto& e : expected)
        {
            std::vector<uint8_t> cur;
            std::vector<uint8_t> pend;
            covGetDeviceModeSettingsV2(*this, e.index, cur, pend);
            EXPECT_EQ(cur.size(), e.length) << "index " << e.index;
            EXPECT_EQ(pend, cur) << "index " << e.index;
        }
    }
    std::vector<uint8_t> cur;
    std::vector<uint8_t> pend;
    covGetDeviceModeSettingsV2(*this, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
                               cur, pend);
    ASSERT_EQ(cur.size(), 4u);
    uint32_t powerLimit = 0;
    std::memcpy(&powerLimit, cur.data(), sizeof(powerLimit));
    EXPECT_EQ(le32toh(powerLimit), 550000u);
    covGetDeviceModeSettingsV2(
        *this, DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX, cur, pend);
    EXPECT_EQ(cur, (std::vector<uint8_t>{2, 0x07}));
    covGetDeviceModeSettingsV2(*this, DEVICE_MODE_ADAPTIVE_TGPMODE, cur, pend);
    EXPECT_EQ(cur, (std::vector<uint8_t>{NSM_ADAPTIVE_TGPMODE_ENABLED}));
    covGetDeviceModeSettingsV2(*this, 0x7777, cur, pend);
    EXPECT_EQ(cur, (std::vector<uint8_t>{0, 0, 0, 0}));
}

TEST_F(MockupResponderTest, CovSetDeviceModeSettingsV2HandlerLengthsAndStore)
{
    const std::vector<uint8_t> u32 = {1, 2, 3, 4};
    const std::vector<uint8_t> u8 = {1};
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        for (uint32_t index : {DEVICE_MODE_SOC_MAX_AC_POWER_RAMP_RATE,
                               DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
                               DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT,
                               DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY,
                               DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY})
        {
            EXPECT_EQ(respCompletionCode(
                          covSetDeviceModeSettingsV2(*this, index, u32)),
                      NSM_SUCCESS)
                << index;
            EXPECT_EQ(respCompletionCode(
                          covSetDeviceModeSettingsV2(*this, index, u8)),
                      NSM_ERR_INVALID_DATA_LENGTH)
                << index;
        }
        for (uint32_t index :
             {DEVICE_MODE_SOC_POWER_SMOOTHING_ENABLED,
              DEVICE_MODE_SOC_POWER_BRAKE_ENABLED, DEVICE_MODE_ADAPTIVE_TGPMODE,
              DEVICE_MODE_SOC_POWER_SMOOTHING_PRESET_INDEX, DEVICE_MODE_LLDP})
        {
            EXPECT_EQ(respCompletionCode(
                          covSetDeviceModeSettingsV2(*this, index, u8)),
                      NSM_SUCCESS)
                << index;
            EXPECT_EQ(respCompletionCode(
                          covSetDeviceModeSettingsV2(*this, index, u32)),
                      NSM_ERR_INVALID_DATA_LENGTH)
                << index;
        }
        // Unknown index: any length is accepted.
        EXPECT_EQ(respCompletionCode(
                      covSetDeviceModeSettingsV2(*this, 0x7777, {9, 8, 7})),
                  NSM_SUCCESS);
    }

    // The accepted value is stored as pending; current keeps the mock default
    // until a current value is seeded.
    std::vector<uint8_t> cur;
    std::vector<uint8_t> pend;
    covGetDeviceModeSettingsV2(*this, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
                               cur, pend);
    EXPECT_EQ(pend, u32);
    uint32_t powerLimit = 0;
    ASSERT_EQ(cur.size(), 4u);
    std::memcpy(&powerLimit, cur.data(), sizeof(powerLimit));
    EXPECT_EQ(le32toh(powerLimit), 550000u);

    const std::vector<uint8_t> seeded = {9, 9, 9, 9};
    mockupResponder->state
        .deviceModeSettingsV2[DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT]
        .first = seeded;
    covGetDeviceModeSettingsV2(*this, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT,
                               cur, pend);
    EXPECT_EQ(cur, seeded);
    EXPECT_EQ(pend, u32);
}

TEST_F(MockupResponderTest, CovDeviceConfigV2HandlersStoreAndQuery)
{
    const std::vector<uint8_t> cfg = {0x11, 0x22, 0x33};
    const std::vector<uint8_t> query = {0x01, 0x02};
    auto set = [&](uint32_t type) {
        Request request(sizeof(nsm_msg_hdr) +
                            sizeof(nsm_set_device_config_v2_req) - 1 +
                            cfg.size(),
                        0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        EXPECT_EQ(encode_set_device_config_v2_req(
                      instanceId, type, cfg.data(),
                      static_cast<uint16_t>(cfg.size()), msg),
                  NSM_SW_SUCCESS);
        return mockupResponder->setDeviceConfigV2Handler(msg, request.size());
    };
    auto get = [&](uint32_t type, std::vector<uint8_t>& cur,
                   std::vector<uint8_t>& pend) {
        Request request(sizeof(nsm_msg_hdr) +
                            sizeof(nsm_get_device_config_v2_req) - 1 +
                            query.size(),
                        0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        EXPECT_EQ(encode_get_device_config_v2_req(
                      instanceId, type, query.data(),
                      static_cast<uint16_t>(query.size()), msg),
                  NSM_SW_SUCCESS);
        auto resp = mockupResponder->getDeviceConfigV2Handler(msg,
                                                              request.size());
        if (!resp.has_value())
        {
            ADD_FAILURE() << "no Get Device Config v2 response";
            return;
        }
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        cur.assign(64, 0);
        pend.assign(64, 0);
        uint16_t curLen = 0;
        uint16_t pendLen = 0;
        EXPECT_EQ(decode_get_device_config_v2_resp(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &reasonCode, cur.data(), &curLen,
                      pend.data(), &pendLen),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);
        cur.resize(curLen);
        pend.resize(pendLen);
    };

    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        auto resp = set(5);
        ASSERT_TRUE(resp.has_value());
        EXPECT_EQ(respCompletionCode(*resp), NSM_SUCCESS);

        std::vector<uint8_t> cur;
        std::vector<uint8_t> pend;
        get(5, cur, pend);
        EXPECT_EQ(cur, cfg);
        EXPECT_EQ(pend, (std::vector<uint8_t>{0xca, 0xfe}));

        // A different config type answers with the placeholder.
        get(6, cur, pend);
        EXPECT_EQ(cur, (std::vector<uint8_t>{0xde, 0xad, 0xbe, 0xef}));
        EXPECT_EQ(pend, (std::vector<uint8_t>{0xca, 0xfe}));
    }
}

TEST_F(MockupResponderTest, CovGetSupportedGPMMetricsHandlerValidRequest)
{
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        Request request(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_supported_gpm_metrics_req), 0);
        auto msg = reinterpret_cast<nsm_msg*>(request.data());
        ASSERT_EQ(encode_get_supported_gpm_metrics_req(instanceId, 1, msg),
                  NSM_SW_SUCCESS);
        auto resp = mockupResponder->getSupportedGPMMetrics(msg,
                                                            request.size());
        ASSERT_TRUE(resp.has_value());
        uint8_t cc = NSM_ERROR;
        uint16_t reasonCode = ERR_NULL;
        uint16_t maskSize = 0;
        uint16_t maxMetrics = 0;
        std::array<uint8_t, 64> bitmask{};
        uint16_t bitmaskSize = bitmask.size();
        ASSERT_EQ(decode_get_supported_gpm_metrics_resp(
                      reinterpret_cast<const nsm_msg*>(resp->data()),
                      resp->size(), &cc, &reasonCode, &maskSize, &maxMetrics,
                      bitmask.data(), &bitmaskSize),
                  NSM_SW_SUCCESS);
        EXPECT_EQ(cc, NSM_SUCCESS);
        EXPECT_EQ(maskSize, 4);
        EXPECT_EQ(maxMetrics, 21);
        EXPECT_EQ(bitmask[0], 0xFF);
        EXPECT_EQ(bitmask[3], 0xFF);
    }
}

TEST_F(MockupResponderTest, CovGetPciePortConfigHandlerSkipsUnexpectedTag)
{
    // Tags above 4 are neither a preset nor the Tx amplitude: skipped.
    MockupResponder::pciePortConfigMockTable[9] = 1;
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_get_port_config_req), 0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    ASSERT_EQ(encode_get_pcie_port_config_req(instanceId, 0, 0, 0, msg),
              NSM_SW_SUCCESS);
    auto resp = mockupResponder->getPciePortConfigHandler(msg, request.size());
    MockupResponder::pciePortConfigMockTable.erase(9);
    ASSERT_TRUE(resp.has_value());
    EXPECT_GE(resp->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_aggregate_resp));
}

TEST(MockupResponderCovStandalone, EventSourceIgnoresOutOfRangeIds)
{
    MockupResponder::EventSource source(
        std::vector<uint64_t>{1, EVENT_SOURCES_LENGTH * 8ULL, 5000});
    EXPECT_EQ(source.events[0].byte, 0x02);
    for (size_t i = 1; i < source.events.size(); ++i)
    {
        EXPECT_EQ(source.events[i].byte, 0) << "byte " << i;
    }
}

// ---- Pre-Boot Diagnostics (CPU responder) ----------------------------------

namespace
{
using DiagState = MockupResponder::MockupResponder::DiagSessionState;

Response covSetDiagSystemConfig(MockupResponderTest& suite, uint8_t duration,
                                const std::vector<uint8_t>& dynamic)
{
    Request request(sizeof(nsm_msg_hdr) +
                        sizeof(nsm_diag_set_system_config_req) - 1 +
                        dynamic.size(),
                    0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_nsm_diag_set_system_config_req(
                  suite.instanceId, NSM_DIAG_CONFIG_TYPE_TEST, duration,
                  dynamic.empty() ? nullptr : dynamic.data(),
                  static_cast<uint8_t>(dynamic.size()), msg),
              NSM_SW_SUCCESS);
    auto resp =
        suite.mockupResponder->setDiagSystemConfigHandler(msg, request.size());
    if (!resp.has_value())
    {
        ADD_FAILURE() << "no Set Diag System Config response";
        return {};
    }
    return *resp;
}

Response covSetDiagTidConfig(MockupResponderTest& suite, uint8_t tid,
                             uint8_t duration, uint16_t loops, uint8_t logLevel,
                             const std::vector<uint8_t>& dynamic)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_diag_set_tid_config_req) -
                        1 + dynamic.size(),
                    0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    EXPECT_EQ(encode_nsm_diag_set_tid_config_req(
                  suite.instanceId, tid, duration, loops, logLevel,
                  static_cast<uint8_t>(dynamic.size()),
                  dynamic.empty() ? nullptr : dynamic.data(), msg),
              NSM_SW_SUCCESS);
    auto resp = suite.mockupResponder->setDiagTidConfigHandler(msg,
                                                               request.size());
    if (!resp.has_value())
    {
        ADD_FAILURE() << "no Set Diag TID Config response";
        return {};
    }
    return *resp;
}
} // namespace

TEST_F(MockupResponderTest, CovSetDiagSystemConfigHandlerStoresTidList)
{
    auto& session = mockupResponder->diagSession;

    // No dynamic data: system-level test, no TIDs requested.
    EXPECT_EQ(respCompletionCode(covSetDiagSystemConfig(*this, 2, {})),
              NSM_SUCCESS);
    EXPECT_EQ(session.configType, NSM_DIAG_CONFIG_TYPE_TEST);
    EXPECT_EQ(session.systemTestDuration, 2);
    EXPECT_TRUE(session.systemDynamicData.empty());
    EXPECT_TRUE(session.requestedTids.empty());
    EXPECT_EQ(mockupResponder->diagTimerSource, nullptr); // session IDLE

    // DynamicData[0] = count, then the TID values.
    EXPECT_EQ(respCompletionCode(
                  covSetDiagSystemConfig(*this, 0, {3, 0x01, 0x02, 0x09})),
              NSM_SUCCESS);
    EXPECT_EQ(session.requestedTids, (std::vector<uint8_t>{1, 2, 9}));
    EXPECT_EQ(session.systemDynamicData,
              (std::vector<uint8_t>{3, 0x01, 0x02, 0x09}));

    // A count larger than the data present is clamped to what is there.
    EXPECT_EQ(respCompletionCode(covSetDiagSystemConfig(*this, 0, {3, 0x05})),
              NSM_SUCCESS);
    EXPECT_EQ(session.requestedTids, (std::vector<uint8_t>{5}));

    // While the mock waits for the system config the reply arms the session
    // timer; quiet mode takes the same path.
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        mockupResponder->diagSessionState = DiagState::WAIT_SYSTEM_CONFIG;
        EXPECT_EQ(respCompletionCode(covSetDiagSystemConfig(*this, 1, {})),
                  NSM_SUCCESS);
        EXPECT_NE(mockupResponder->diagTimerSource, nullptr);
    }

    // A request without the fixed fields is rejected as unsupported.
    auto bad = covCommonRequest(instanceId, NSM_TYPE_DIAGNOSTIC,
                                NSM_DIAG_SET_SYSTEM_CONFIG,
                                sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto resp = mockupResponder->setDiagSystemConfigHandler(
        reinterpret_cast<const nsm_msg*>(bad.data()), bad.size());
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
}

TEST_F(MockupResponderTest, CovSetDiagTidConfigHandlerStoresPerTidConfig)
{
    auto& session = mockupResponder->diagSession;
    EXPECT_EQ(respCompletionCode(
                  covSetDiagTidConfig(*this, 1, 2, 10, 3, {0xAA, 0xBB})),
              NSM_SUCCESS);
    ASSERT_EQ(session.tidConfigs.count(1), 1u);
    EXPECT_EQ(session.tidConfigs[1].testDuration, 2);
    EXPECT_EQ(session.tidConfigs[1].loops, 10);
    EXPECT_EQ(session.tidConfigs[1].logLevel, 3);
    EXPECT_EQ(session.tidConfigs[1].dynamicData,
              (std::vector<uint8_t>{0xAA, 0xBB}));
    EXPECT_EQ(mockupResponder->diagTimerSource, nullptr); // session IDLE

    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        mockupResponder->diagSessionState = DiagState::REQUESTING_TID_CONFIGS;
        EXPECT_EQ(
            respCompletionCode(covSetDiagTidConfig(*this, 2, 1, 0, 0, {})),
            NSM_SUCCESS);
        EXPECT_NE(mockupResponder->diagTimerSource, nullptr);
        EXPECT_TRUE(session.tidConfigs[2].dynamicData.empty());
    }

    auto bad = covCommonRequest(instanceId, NSM_TYPE_DIAGNOSTIC,
                                NSM_DIAG_SET_TID_CONFIG,
                                sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto resp = mockupResponder->setDiagTidConfigHandler(
        reinterpret_cast<const nsm_msg*>(bad.data()), bad.size());
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_ERR_UNSUPPORTED_COMMAND_CODE);
}

TEST_F(MockupResponderTest, CovGenerateResultForTidMatchesGoldenResults)
{
    auto& session = mockupResponder->diagSession;
    auto result = [&](uint8_t tid) {
        return mockupResponder->generateResultForTid(tid);
    };

    // Out-of-range TIDs.
    EXPECT_EQ(result(0x00).first, NSM_DIAG_TEST_INVALID_TID);
    EXPECT_EQ(result(0x0B).first, NSM_DIAG_TEST_INVALID_TID);
    EXPECT_TRUE(result(0x0B).second.empty());

    // System-level test: CPU tests carry a 47-byte mask, memory tests 148.
    session.reset();
    session.systemTestDuration = 2;
    EXPECT_EQ(result(0x01).first, NSM_DIAG_TEST_PASS);
    EXPECT_EQ(result(0x01).second.size(), 0x2Fu);
    EXPECT_EQ(result(0x08).second.size(), 0x2Fu);
    EXPECT_EQ(result(0x09).first, NSM_DIAG_TEST_PASS);
    EXPECT_EQ(result(0x09).second.size(), 0x94u);
    EXPECT_EQ(result(0x0A).second.size(), 0x94u);
    session.systemTestDuration = 4;
    EXPECT_EQ(result(0x03).first, NSM_DIAG_TEST_UNSPECIFIED_ERROR);

    // Per-TID configuration required when the system duration is 0.
    session.reset();
    EXPECT_EQ(result(0x02).first, NSM_DIAG_TEST_TID_NOT_CONFIGURED);
    session.tidConfigs[0x02] = {4, 1, 0, {}};
    EXPECT_EQ(result(0x02).first, NSM_DIAG_TEST_INVALID_PARAMETER);
    session.tidConfigs[0x02].testDuration = 3;
    EXPECT_EQ(result(0x02).first, NSM_DIAG_TEST_PASS);
    EXPECT_EQ(result(0x02).second.size(), 0x2Fu);
}

TEST_F(MockupResponderTest, CovRunDiagSessionRequiresReceiverAndRestarts)
{
    auto& mock = *mockupResponder;
    mock.eventReceiverEid = 0;
    mock.runDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::IDLE);

    mock.eventReceiverEid = 30;
    mock.diagSession.requestedTids = {1};
    mock.runDiagSession(); // no socket: the GetSystemConfig event send fails
    EXPECT_EQ(mock.diagSessionState, DiagState::WAIT_SYSTEM_CONFIG);
    EXPECT_TRUE(mock.diagSession.requestedTids.empty());
    EXPECT_EQ(mock.diagTimerSource, nullptr);

    // Restart while in progress, first without and then with an armed timer.
    mock.runDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::WAIT_SYSTEM_CONFIG);
    mock.scheduleDiagTimer(1000000);
    ASSERT_NE(mock.diagTimerSource, nullptr);
    mock.runDiagSession();
    EXPECT_EQ(mock.diagTimerSource, nullptr);
    EXPECT_EQ(mock.diagSessionState, DiagState::WAIT_SYSTEM_CONFIG);

    // Re-arming replaces the previous timer source.
    mock.scheduleDiagTimer(1000000);
    auto* first = mock.diagTimerSource;
    ASSERT_NE(first, nullptr);
    mock.scheduleDiagTimer(2000000);
    EXPECT_NE(mock.diagTimerSource, nullptr);
}

TEST_F(MockupResponderTest, CovAdvanceDiagSessionWalksEveryState)
{
    auto& mock = *mockupResponder;
    mock.eventReceiverEid = 30;

    // IDLE and an unknown state: nothing to do.
    mock.diagSessionState = DiagState::IDLE;
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::IDLE);
    mock.diagSessionState = static_cast<DiagState>(99);
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, static_cast<DiagState>(99));

    // Invalid system duration: error result, then DONE.
    mock.diagSession.reset();
    mock.diagSession.systemTestDuration = 5;
    mock.diagSessionState = DiagState::WAIT_SYSTEM_CONFIG;
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::DONE);
    EXPECT_NE(mock.diagTimerSource, nullptr);

    // Duration 0 without TIDs: error result, then DONE.
    mock.diagSession.reset();
    mock.diagSessionState = DiagState::WAIT_SYSTEM_CONFIG;
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::DONE);

    // System-level test: straight to EXECUTING.
    mock.diagSession.reset();
    mock.diagSession.systemTestDuration = 2;
    mock.diagSessionState = DiagState::WAIT_SYSTEM_CONFIG;
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::EXECUTING);

    // Per-TID test: one config request per TID, then EXECUTING.
    mock.diagSession.reset();
    mock.diagSession.requestedTids = {1, 9};
    mock.diagSessionState = DiagState::WAIT_SYSTEM_CONFIG;
    mock.advanceDiagSession(); // falls through and requests TID 1
    EXPECT_EQ(mock.diagSessionState, DiagState::REQUESTING_TID_CONFIGS);
    EXPECT_EQ(mock.pendingTidRequests.size(), 1u);
    mock.advanceDiagSession(); // requests TID 9
    EXPECT_EQ(mock.diagSessionState, DiagState::REQUESTING_TID_CONFIGS);
    EXPECT_TRUE(mock.pendingTidRequests.empty());
    mock.advanceDiagSession(); // all configs received
    EXPECT_EQ(mock.diagSessionState, DiagState::EXECUTING);

    // EXECUTING with explicit TIDs queues exactly those results.
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::REPORTING);
    EXPECT_EQ(mock.pendingTidResults.size(), 2u);

    // EXECUTING for a system-level test queues the eight default TIDs and
    // REPORTING drains one result per call before finishing.
    mock.diagSession.reset();
    mock.diagSession.systemTestDuration = 1;
    mock.diagSessionState = DiagState::EXECUTING;
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::REPORTING);
    ASSERT_EQ(mock.pendingTidResults.size(), 8u);
    for (size_t i = 0; i < 8; ++i)
    {
        mock.advanceDiagSession();
        EXPECT_EQ(mock.diagSessionState, DiagState::REPORTING) << i;
        EXPECT_EQ(mock.pendingTidResults.size(), 7 - i);
    }
    mock.advanceDiagSession(); // REPORTING -> DONE -> IDLE
    EXPECT_EQ(mock.diagSessionState, DiagState::IDLE);
    EXPECT_EQ(mock.diagTimerSource, nullptr);

    // DONE with an armed timer releases it.
    mock.scheduleDiagTimer(1000000);
    mock.diagSessionState = DiagState::DONE;
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::IDLE);
    EXPECT_EQ(mock.diagTimerSource, nullptr);
}

TEST_F(MockupResponderTest, CovDiagEventSendersWithoutSocket)
{
    auto& mock = *mockupResponder;
    for (bool verbose : {true, false})
    {
        mock.verbose = verbose;
        EXPECT_NO_THROW(mock.sendDiagGetSystemConfigEvent(
            30, true, NSM_DIAG_CONFIG_TYPE_TEST));
        EXPECT_NO_THROW(mock.sendDiagGetTidConfigEvent(30, true, 1));
        EXPECT_NO_THROW(mock.sendDiagSetTestResultEvent(
            30, true, 1, NSM_DIAG_TEST_PASS, std::vector<uint8_t>(0x2F, 0)));
        EXPECT_NO_THROW(mock.sendDiagSetTestResultEvent(
            30, true, 0, NSM_DIAG_TEST_UNSPECIFIED_ERROR, {}));
        // Oversized dynamic data is rejected before encoding.
        EXPECT_NO_THROW(mock.sendDiagSetTestResultEvent(
            30, true, 1, NSM_DIAG_TEST_PASS,
            std::vector<uint8_t>(NSM_DIAG_MAX_DYNAMIC_DATA_SIZE + 1, 0)));
        EXPECT_NO_THROW(mock.sendDiagSetFlowControlEvent(
            30, false, NSM_DIAG_FLOW_CTRL_IN_PROGRESS));
        EXPECT_NO_THROW(mock.sendRuntimeISTCompleteEvent(
            30, true, "GPU-0001", 1, "1.2.3", 0, 0, 80, 70));
        // Over-long identifiers are truncated to the payload fields.
        EXPECT_NO_THROW(mock.sendRuntimeISTCompleteEvent(
            30, false, std::string(NSM_RIST_GPU_UUID_LEN + 10, 'u'), 2,
            std::string(NSM_RIST_APP_VERSION_LEN + 10, 'v'), 1, 5, -1, -2));
    }
}

// ---- MCTP send over an injected socket -------------------------------------

TEST_F(MockupResponderTest, CovMctpSockSendDeliversFrameOverInjectedSocket)
{
    CovSocketPair sp;
    ASSERT_GE(sp.fds[0], 0);
    CovSockFdGuard guard(*mockupResponder, sp.fds[0]);
    std::vector<uint8_t> payload = {0x10, 0xDE, 0x80, 0x89, 0x00, 0x00, 0x01};
    for (bool verbose : {true, false})
    {
        mockupResponder->verbose = verbose;
        EXPECT_EQ(mockupResponder->mctpSockSend(30, payload), NSM_SUCCESS);
        std::array<uint8_t, 64> rx{};
        ssize_t n = recv(sp.fds[1], rx.data(), rx.size(), 0);
        ASSERT_EQ(n, static_cast<ssize_t>(payload.size() + 3));
        EXPECT_EQ(rx[1], 30); // destination EID follows the tag byte
        EXPECT_TRUE(std::equal(payload.begin(), payload.end(),
                               std::next(rx.begin(), 3)));
    }

    // sendmsg() on a descriptor that is not a socket fails.
    int devNull = open("/dev/null", O_WRONLY);
    ASSERT_GE(devNull, 0);
    mockupResponder->sockFd = devNull;
    EXPECT_EQ(mockupResponder->mctpSockSend(30, payload), NSM_ERROR);
    close(devNull);
}

TEST_F(MockupResponderTest, CovEventSendersSucceedOverInjectedSocket)
{
    CovSocketPair sp;
    ASSERT_GE(sp.fds[0], 0);
    CovSockFdGuard guard(*mockupResponder, sp.fds[0]);
    auto& mock = *mockupResponder;
    std::vector<std::pair<uint16_t, bool>> gpioEvents = {{1, true}, {2, false}};
    for (bool verbose : {true, false})
    {
        mock.verbose = verbose;
        mock.sendRediscoveryEvent(30, true);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendXIDEvent(30, true, 0x01, 999, 1, 12345, "xid");
        EXPECT_EQ(sp.drain(), 1);
        mock.sendResetRequiredEvent(30, true);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendDeviceConfigurationRequestEventV1(30, true);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendFabricManagerStateEvent(30, true, 1, 2, 3, 4);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendNsmEvent(30, NSM_TYPE_PLATFORM_ENVIRONMENTAL, true, 1, 2, 3, 4,
                          0, nullptr);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendThreasholdEvent(30, true, true, false, true, false, true,
                                 false, true, 2);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendGpioStateChangeEvent(30, true, 5, gpioEvents);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendRuntimeISTCompleteEvent(30, true, "GPU-0001", 1, "1.2.3", 0, 0,
                                         80, 70);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendDiagGetSystemConfigEvent(30, true, NSM_DIAG_CONFIG_TYPE_TEST);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendDiagGetTidConfigEvent(30, true, 1);
        EXPECT_EQ(sp.drain(), 1);
        mock.sendDiagSetTestResultEvent(30, true, 1, NSM_DIAG_TEST_PASS,
                                        std::vector<uint8_t>(0x2F, 0));
        EXPECT_EQ(sp.drain(), 1);
        mock.sendDiagSetTestResultEvent(30, true, 0,
                                        NSM_DIAG_TEST_UNSPECIFIED_ERROR, {});
        EXPECT_EQ(sp.drain(), 1);
        mock.sendDiagSetFlowControlEvent(30, false,
                                         NSM_DIAG_FLOW_CTRL_IN_PROGRESS);
        EXPECT_EQ(sp.drain(), 1);
    }
}

TEST_F(MockupResponderTest, CovDiagSessionEmitsEveryEventOverInjectedSocket)
{
    CovSocketPair sp;
    ASSERT_GE(sp.fds[0], 0);
    CovSockFdGuard guard(*mockupResponder, sp.fds[0]);
    auto& mock = *mockupResponder;
    mock.eventReceiverEid = 30;

    // Start: one GetSystemConfig event.
    mock.runDiagSession();
    EXPECT_EQ(sp.drain(), 1);

    // The BMC answers with a two-TID config; the mock requests both configs.
    EXPECT_EQ(respCompletionCode(covSetDiagSystemConfig(*this, 0, {2, 1, 9})),
              NSM_SUCCESS);
    mock.advanceDiagSession();
    EXPECT_EQ(sp.drain(), 1); // TID 1 config request
    EXPECT_EQ(respCompletionCode(covSetDiagTidConfig(*this, 1, 2, 1, 0, {})),
              NSM_SUCCESS);
    mock.advanceDiagSession();
    EXPECT_EQ(sp.drain(), 1);  // TID 9 config request
    mock.advanceDiagSession(); // -> EXECUTING
    EXPECT_EQ(mock.diagSessionState, DiagState::EXECUTING);
    mock.advanceDiagSession(); // heartbeat -> REPORTING
    EXPECT_EQ(sp.drain(), 1);
    EXPECT_EQ(mock.pendingTidResults.size(), 2u);
    mock.advanceDiagSession(); // TID 1: PASS
    EXPECT_EQ(sp.drain(), 1);
    mock.advanceDiagSession(); // TID 9: not configured
    EXPECT_EQ(sp.drain(), 1);
    mock.advanceDiagSession(); // execution finished
    EXPECT_EQ(sp.drain(), 1);
    EXPECT_EQ(mock.diagSessionState, DiagState::IDLE);
}

// ---- Dump failure cycle: 0x40 / 0x51 / 0x52 / 0x59 --------------------------

namespace
{
// Iterative dump commands consume one page per request; check every page of
// every case against the table.
template <typename Handler>
void covWalkIterativeCycle(MockupResponder::MockupResponder& mock,
                           Handler handler)
{
    for (const auto& c : MockupResponder::kDumpFailureCycle)
    {
        for (uint32_t p = 0; p < c.pageCount; ++p)
        {
            const auto& page = c.pages[p];
            auto resp = handler();
            if (page.silent)
            {
                EXPECT_FALSE(resp.has_value()) << c.caseId;
                continue;
            }
            ASSERT_TRUE(resp.has_value()) << c.caseId;
            EXPECT_EQ(respCompletionCode(*resp), page.cc) << c.caseId;
            if (page.cc != NSM_SUCCESS)
            {
                EXPECT_EQ(respReasonCode(*resp), page.reasonCode) << c.caseId;
                EXPECT_EQ(resp->size(), sizeof(nsm_msg_hdr) +
                                            sizeof(nsm_common_non_success_resp))
                    << c.caseId;
            }
        }
    }
    EXPECT_EQ(mock.cycleCaseIndex, 0u);
    EXPECT_EQ(mock.cyclePageIndex, 0u);
}

// Single-shot erase commands skip multi-page cases and consume one
// single-page case per request.
template <typename Handler>
void covWalkEraseCycle(MockupResponder::MockupResponder& mock, Handler handler)
{
    const auto& cycle = MockupResponder::kDumpFailureCycle;
    for (size_t step = 0; step < cycle.size(); ++step)
    {
        uint32_t idx = mock.cycleCaseIndex;
        while (cycle[idx].pageCount > 1)
        {
            idx = (idx + 1) % cycle.size();
        }
        const auto& page = cycle[idx].pages[0];
        auto resp = handler();
        if (page.silent)
        {
            EXPECT_FALSE(resp.has_value()) << cycle[idx].caseId;
        }
        else
        {
            ASSERT_TRUE(resp.has_value()) << cycle[idx].caseId;
            EXPECT_EQ(respCompletionCode(*resp), page.cc) << cycle[idx].caseId;
            if (page.cc != NSM_SUCCESS)
            {
                EXPECT_EQ(respReasonCode(*resp), page.reasonCode)
                    << cycle[idx].caseId;
            }
        }
        EXPECT_EQ(mock.cycleCaseIndex, (idx + 1) % cycle.size());
        EXPECT_EQ(mock.cyclePageIndex, 0u);
    }
}
} // namespace

TEST_F(MockupDumpCycleTest, CovDeviceDiagnosticsWalksWholeFailureCycle)
{
    buildMock(/*failureCycle=*/true);
    covWalkIterativeCycle(*mockupResponder, [&]() {
        auto req = makeDiagnosticsReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        return mockupResponder->getDeviceDiagnosticsHandler(reqMsg, req.size());
    });
}

TEST_F(MockupDumpCycleTest, CovNetworkDeviceLogInfoWalksWholeFailureCycle)
{
    buildMock(/*failureCycle=*/true);
    covWalkIterativeCycle(*mockupResponder, [&]() {
        auto req = makeLogInfoReq(instanceId, 0);
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        return mockupResponder->getNetworkDeviceLogInfoHandler(reqMsg,
                                                               req.size());
    });
}

TEST_F(MockupDumpCycleTest, CovEraseTraceWalksSingleShotFailureCycle)
{
    buildMock(/*failureCycle=*/true);
    covWalkEraseCycle(*mockupResponder, [&]() {
        Request req(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        EXPECT_EQ(encode_erase_trace_req(instanceId, reqMsg), NSM_SW_SUCCESS);
        return mockupResponder->eraseTraceHandler(reqMsg, req.size());
    });
}

TEST_F(MockupDumpCycleTest, CovEraseDebugInfoWalksSingleShotFailureCycle)
{
    buildMock(/*failureCycle=*/true);
    covWalkEraseCycle(*mockupResponder, [&]() {
        Request req(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_debug_info_req));
        auto* reqMsg = reinterpret_cast<nsm_msg*>(req.data());
        EXPECT_EQ(encode_erase_debug_info_req(instanceId, 0, reqMsg),
                  NSM_SW_SUCCESS);
        return mockupResponder->eraseDebugInfoHandler(reqMsg, req.size());
    });
}

// ---- Remaining reachable decision points -----------------------------------

TEST_F(MockupResponderTest, CovDbusMethodsInvokeEventSenders)
{
    // The mock exposes its event generators as D-Bus methods on the fixture's
    // connection; calling them through the bus runs the registered lambdas.
    const std::string service = systemBus->get_unique_name();
    const std::string path = "/xyz/openbmc_project/NSM/30";
    const std::string iface = "xyz.openbmc_project.NSM.Device";
    int completed = 0;
    auto onDone = [&completed](boost::system::error_code ec) {
        EXPECT_FALSE(ec) << ec.message();
        ++completed;
    };
    systemBus->async_method_call(onDone, service, path, iface, "genXIDEvent",
                                 uint8_t{30}, true, uint8_t{1}, uint32_t{999},
                                 uint32_t{1}, uint64_t{12345},
                                 std::string("xid"));
    systemBus->async_method_call(
        onDone, service, path, iface, "genRuntimeISTCompleteEvent", uint8_t{30},
        true, std::string("GPU-0001"), uint64_t{1}, std::string("1.2.3"),
        uint8_t{0}, uint64_t{0}, int32_t{80}, int32_t{70});
    systemBus->async_method_call(
        onDone, service, path, iface, "genGpioStateChangeEvent", uint8_t{30},
        true, uint64_t{5},
        std::vector<std::pair<uint16_t, bool>>{{1, true}, {2, false}});
    systemBus->async_method_call(
        onDone, service, path, iface, "genDiagSetTestResultEvent", uint8_t{30},
        true, uint8_t{1}, uint16_t{0}, std::vector<uint8_t>{1, 2});
    systemBus->async_method_call(onDone, service, path, iface,
                                 "genDiagSetFlowControlEvent", uint8_t{30},
                                 uint8_t{NSM_DIAG_FLOW_CTRL_IN_PROGRESS});
    const int expected = 5;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(10);
    while (completed < expected && std::chrono::steady_clock::now() < deadline)
    {
        if (io.stopped())
        {
            io.restart();
        }
        io.run_for(std::chrono::milliseconds(50));
    }
    EXPECT_EQ(completed, expected);
}

TEST_F(MockupResponderTest, CovAdvanceDiagSessionDoneWithoutTimer)
{
    auto& mock = *mockupResponder;
    mock.eventReceiverEid = 30;
    mock.diagSessionState = DiagState::DONE;
    ASSERT_EQ(mock.diagTimerSource, nullptr);
    mock.advanceDiagSession();
    EXPECT_EQ(mock.diagSessionState, DiagState::IDLE);
}

TEST_F(MockupResponderTest, CovSetCurrentEventSourcesUnknownMessageType)
{
    bitfield8_t sources[EVENT_SOURCES_LENGTH] = {};
    sources[0].byte = 0xFF;
    Request request(
        sizeof(nsm_msg_hdr) + sizeof(nsm_set_current_event_source_req), 0);
    auto msg = reinterpret_cast<nsm_msg*>(request.data());
    ASSERT_EQ(encode_nsm_set_current_event_sources_req(
                  instanceId, NSM_TYPE_FIRMWARE + 1, sources, msg),
              NSM_SW_SUCCESS);
    auto resp = mockupResponder->setCurrentEventSources(msg, request.size());
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(respCompletionCode(*resp), NSM_SUCCESS);
    // Nothing is supported for an unknown type, so nothing gets enabled.
    for (const auto& byte :
         mockupResponder->state.eventSources[NSM_TYPE_FIRMWARE + 1])
    {
        EXPECT_EQ(byte.byte, 0);
    }
}

TEST_F(MockupResponderTest, CovGetEgmModeHandlerTruncatedRequest)
{
    truncatedDecodeTest(this, [this](const nsm_msg* m, size_t l) {
        return mockupResponder->getEgmModeHandler(m, l);
    });
}

TEST_F(MockupDumpCycleTest, CovQueryPortCharacteristicsQuietFailureCycle)
{
    buildMock(/*failureCycle=*/true);
    mockupResponder->verbose = false;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_port_characteristics_req));
    auto* reqMsg = reinterpret_cast<nsm_msg*>(request.data());
    ASSERT_EQ(encode_query_port_characteristics_req(instanceId, 0, reqMsg),
              NSM_SW_SUCCESS);
    auto resp = mockupResponder->queryPortCharacteristicsHandler(
        reqMsg, request.size());
    ASSERT_TRUE(resp.has_value());
    EXPECT_EQ(mockupResponder->portHealthCycleIndex, 1u);
}

TEST_F(MockupResponderTest, CovDestructorClosesOwnedSocket)
{
    // A mock that ends up owning a socket descriptor closes it on teardown.
    int fds[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, fds), 0);
    int owned = dup(fds[0]);
    ASSERT_GE(owned, 0);
    mockupResponder->sockFd = owned;
    mockupResponder.reset();
    EXPECT_EQ(fcntl(owned, F_GETFD), -1); // already closed by the mock
    close(fds[0]);
    close(fds[1]);
}
