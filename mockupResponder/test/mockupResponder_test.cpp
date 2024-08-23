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

#include "mockupResponder.hpp"
#include "utils.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;

class MockupResponderTest : public testing::Test
{
  protected:
    MockupResponderTest() : event(sdeventplus::Event::get_default())

    {
        systemBus = std::make_shared<sdbusplus::asio::connection>(io);
        objServer = std::make_shared<sdbusplus::asio::object_server>(systemBus);
        mockupResponder = std::make_shared<MockupResponder::MockupResponder>(
            false, event, *objServer, 30, NSM_DEV_ID_GPU, 0);
    }

    sdeventplus::Event event;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
    std::shared_ptr<MockupResponder::MockupResponder> mockupResponder;

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
