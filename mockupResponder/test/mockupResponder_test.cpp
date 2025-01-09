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
#include "device-configuration.h"
#include "diagnostics.h"
#include "platform-environmental.h"

#include "utils.hpp"

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
        test(
            requestMsg, requestMsgLen,
            [&handler, &longRunningEvent](const nsm_msg* request, size_t len) {
            return handler(request, len, false, longRunningEvent);
        },
            command, response);

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
    MockupResponderTest() : event(sdeventplus::Event::get_default())

    {
        init(30, NSM_DEV_ID_GPU, 2);
    }

    uint8_t instanceId = 0;
    sdeventplus::Event event;
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
    void test(std::function<int(uint8_t, RequestPayload, nsm_msg*)>
                  encodeRequestFunction,
              RequestPayload requestPayload, MockupFunction handlerFunction,
              uint8_t command, ResponseStruct& response)
    {
        Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req) +
                        sizeof(RequestPayload));
        auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
        auto rc = encodeRequestFunction(instanceId, requestPayload, requestMsg);
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

    nsm_reconfiguration_permissions_v1 expected = {0, 0, 0, 0};
    EXPECT_EQ(NSM_GET_RECONFIGURATION_PERMISSIONS_V1, response->hdr.command);
    EXPECT_EQ(expected.oneshot, response->data.oneshot);
    EXPECT_EQ(expected.persistent, response->data.persistent);
    EXPECT_EQ(expected.flr_persistent, response->data.flr_persistent);
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
    for (const auto& [type, _] :
         mockupResponder->state.errorInjection[NSM_DEV_ID_GPU])
    {
        data.mask[type / 8] |= (1 << (type % 8));
    }
    test<const nsm_error_injection_types_mask*>(
        &encode_set_current_error_injection_types_v1_req,
        const_cast<const nsm_error_injection_types_mask*>(&data),
        &MockupResponder::MockupResponder::
            setCurrentErrorInjectionTypesV1Handler,
        NSM_SET_CURRENT_ERROR_INJECTION_TYPES_V1);
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
