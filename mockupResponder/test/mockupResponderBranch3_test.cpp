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

class MockupResponderBranch3Test : public Test
{
  protected:
    MockupResponderBranch3Test()
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

// Fixture for non-verbose GPU
class MockupResponderBranch3QuietTest : public Test
{
  protected:
    MockupResponderBranch3QuietTest()
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
// enableDisableWriteProtectedHandler - remaining data_index values
// =============================================================================

TEST_F(MockupResponderBranch3Test, WriteProtectedCx7FruEeprom)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, CX7_FRU_EEPROM, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedHmcFruEeprom)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, HMC_FRU_EEPROM, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_2, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash3)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_3, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash4)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_4, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash5)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_5, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash6)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_6, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash7)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_7, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedGpuSpiFlash8)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, GPU_SPI_FLASH_8, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_2, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom3)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_3, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom4)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_4, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom5)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_5, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom6)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_6, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom7)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_7, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedRetimerEeprom8)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, RETIMER_EEPROM_8, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedCpuSpiFlash1)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, CPU_SPI_FLASH_1, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedCpuSpiFlash2)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, CPU_SPI_FLASH_2, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedCpuSpiFlash3)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, CPU_SPI_FLASH_3, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedCpuSpiFlash4)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, CPU_SPI_FLASH_4, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, WriteProtectedDefaultDataIndex)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_enable_disable_wp_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_disable_wp_req(instanceId, BASEBOARD_FRU_EEPROM, 1,
                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);
    // Override data_index to an unknown value to hit the default case
    auto payload =
        reinterpret_cast<nsm_enable_disable_wp_req*>(requestMsg->payload);
    payload->data_index =
        static_cast<diagnostics_enable_disable_wp_data_index>(0xFF);

    auto resp = mockupResponder->enableDisableWriteProtectedHandler(
        requestMsg, request.size());
    // Still returns a response (default logs error but continues)
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// setReconfigurationPermissionsV1Handler - different setting indices
// =============================================================================

TEST_F(MockupResponderBranch3Test, SetReconfigPermsFusingMode)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_FUSING_MODE, RP_ONESHOOT_HOT_RESET, 0x03, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetReconfigPermsConfidentialCompute)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_CONFIDENTIAL_COMPUTE, RP_PERSISTENT, 0x05, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetReconfigPermsBar0Firewall)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_BAR0_FIREWALL, RP_ONESHOT_FLR, 0x01, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getReconfigurationPermissionsV1Handler - different indices
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetReconfigPermsFusingMode)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_FUSING_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetReconfigPermsConfidentialCompute)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_CONFIDENTIAL_COMPUTE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetReconfigPermsBar0Firewall)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_BAR0_FIREWALL, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetReconfigPermsClockLimit)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_CLOCK_LIMIT, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetReconfigPermsNvlinkDisable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_NVLINK_DISABLE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetReconfigPermsEccEnable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_ECC_ENABLE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetReconfigPermsEdppScalingFactor)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_reconfiguration_permissions_v1_req(
        instanceId, RP_EDPP_SCALING_FACTOR, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// getDeviceModeSettingsV2Handler - different device mode index branches
// =============================================================================

TEST_F(MockupResponderBranch3Test,
       DISABLED_GetDeviceModeSettingsV2OneShotGpuPower)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test,
       DISABLED_GetDeviceModeSettingsV2PersistentGpuPower)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_PERSISTENT_GPU_BASE_POWER_LIMIT, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test,
       DISABLED_GetDeviceModeSettingsV2OneShotCpuPower)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_ONE_SHOT_CPU_POWER_LIMIT_GPU_COPY, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test,
       DISABLED_GetDeviceModeSettingsV2PersistentCpuPower)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_PERSISTENT_CPU_POWER_LIMIT_GPU_COPY,
        requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DISABLED_GetDeviceModeSettingsV2DefaultIndex)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    // Use index 0 to hit the default case in getDeviceModeSettingsV2Data
    auto rc = encode_get_device_mode_settings_v2_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceModeSettingsV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetDeviceModeSettingsV2BadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// setDeviceModeSettingsV2Handler - valid call
// =============================================================================

TEST_F(MockupResponderBranch3Test, SetDeviceModeSettingsV2Valid)
{
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) + sizeof(data));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT, data,
        sizeof(data), requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Power smoothing handlers - valid calls
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetPowerSmoothingFeatureInfoValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getPowerSmoothingFeatureInfo(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetHwCircuiteryUsageValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_hardware_lifetime_cricuitry_req(instanceId,
                                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getHwCircuiteryUsage(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetCurrentProfileInfoValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getCurrentProfileInfo(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetQueryAdminOverrideValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_admin_override_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getQueryAdminOverride(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetActivePresetProfileValid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_active_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_active_preset_profile_req(instanceId, 2, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setActivePresetProfile(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, ApplyAdminOverrideValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_apply_admin_override_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->applyAdminOverride(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, ToggleFeatureStateValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_toggle_feature_state_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_feature_state_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->toggleFeatureState(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, ToggleImmediateRampDownValid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_toggle_immediate_rampdown_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_toggle_immediate_rampdown_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->toggleImmediateRampDown(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetPresetProfileInfoValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getPresetProfileInfo(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, EnableWorkloadPowerProfileValid)
{
    bitfield256_t mask{};
    mask.fields[0].byte = 0x01;

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_enable_workload_power_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_enable_workload_power_profile_req(instanceId, &mask,
                                                       requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->enableWorkloadPowerProfile(requestMsg,
                                                            request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DisableWorkloadPowerProfileValid)
{
    bitfield256_t mask{};
    mask.fields[0].byte = 0x01;

    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_disable_workload_power_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_disable_workload_power_profile_req(instanceId, &mask,
                                                        requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->disableWorkloadPowerProfile(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetWorkLoadProfileStatusInfoValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_workload_power_profile_status_req(instanceId,
                                                           requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getWorkLoadProfileStatusInfo(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// V2 power smoothing handlers - valid calls
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetPowerSmoothingFeatureInfoV2Valid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_v2_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getPowerSmoothingFeatureInfoV2(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetCurrentProfileInfoV2Valid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_v2_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getCurrentProfileInfoV2(requestMsg,
                                                         request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetPresetProfileInfoV2Valid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_preset_profile_info_v2_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getPresetProfileInfoV2(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Device configuration handlers - valid calls
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetConfidentialComputeModeValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_confidential_compute_mode_v1_req(instanceId,
                                                          requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getConfidentialComputeModeHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetConfidentialComputeModeValid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_confidential_compute_mode_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_confidential_compute_mode_v1_req(instanceId, 1,
                                                          requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setConfidentialComputeModeHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetEgmModeValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_EGM_mode_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getEgmModeHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetEgmModeValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_EGM_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_EGM_mode_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setEgmModeHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetProtectionOptionsValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_protection_options_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getProtectionOptionsHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetProtectionOptionsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getProtectionOptionsHandler(requestMsg,
                                                             request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDevicemodeSettingsValid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_setting_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_setting_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDevicemodeSettingsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->getDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetDevicemodeSettingsBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->setDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// PCI Link handlers - valid calls
// =============================================================================

TEST_F(MockupResponderBranch3Test, PcieFundamentalResetValid)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_assert_pcie_fundamental_reset_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_assert_pcie_fundamental_reset_req(instanceId, 0, 1,
                                                       requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->pcieFundamentalResetHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, ClearScalarDataSourceValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_clear_data_source_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_clear_data_source_v1_req(instanceId, 0, 3, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->clearScalarDataSourceHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// DOT handlers - valid calls
// =============================================================================

TEST_F(MockupResponderBranch3Test, DotCAKInstallValid)
{
    struct nsm_dot_cak_install_req cak_req = {};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_cak_install_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_install_req(instanceId, &cak_req, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotCAKInstallHandler(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotCAKBypassValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_bypass_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_bypass_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotCAKBypassHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotLockValid)
{
    struct nsm_dot_lock_req lock_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_lock_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_lock_req(instanceId, &lock_req, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotLockHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotUnlockChallengeValid)
{
    struct nsm_dot_unlock_challenge_req challenge_req = {};
    challenge_req.unlock_type = 0;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_unlock_challenge_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_unlock_challenge_req(instanceId, &challenge_req,
                                                  requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotUnlockChallengeHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotUnlockValid)
{
    struct nsm_dot_unlock_req unlock_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_unlock_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_unlock_req(instanceId, &unlock_req, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotUnlockHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotCAKRotateValid)
{
    struct nsm_dot_cak_rotate_req rotate_req = {};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_cak_rotate_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_rotate_req(instanceId, &rotate_req,
                                            requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotCAKRotateHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotGetInfoValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_info_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotGetInfoHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotGetStatusValid)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_status_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotGetStatusHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotDisableValid)
{
    struct nsm_dot_disable_req disable_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_disable_req(instanceId, &disable_req, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotDisableHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotOverrideValid)
{
    struct nsm_dot_override_req override_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_override_req(instanceId, &override_req,
                                          requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotOverrideHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotRecoveryValid)
{
    struct nsm_dot_recovery_req recovery_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_recovery_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_recovery_req(instanceId, &recovery_req,
                                          requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotRecoveryHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// DOT bad decode tests
TEST_F(MockupResponderBranch3Test, DotCAKInstallBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotCAKInstallHandler(requestMsg,
                                                      request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotCAKBypassBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotCAKBypassHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotLockBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotLockHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotUnlockChallengeBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotUnlockChallengeHandler(requestMsg,
                                                           request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotUnlockBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotUnlockHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotCAKRotateBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotCAKRotateHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotGetInfoBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotGetInfoHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotGetStatusBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotGetStatusHandler(requestMsg,
                                                     request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotDisableBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotDisableHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotOverrideBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotOverrideHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, DotRecoveryBadDecode)
{
    Request request(sizeof(nsm_msg_hdr), 0);
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto resp = mockupResponder->dotRecoveryHandler(requestMsg, request.size());
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// getDeviceDebugParametersHandler - remaining parameter_id branches
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsMLPC)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MLPC};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLC)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLC};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLS)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLS};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSPI)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSPI};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSPHI)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0,
        .port_number = 0,
        .index = NSM_DEBUG_PARAMETER_ID_PPSPHI};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSPC)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSPC};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsMPSCR)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MPSCR};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPCNTL0General)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPCNT};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id =
        NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L0_GENERAL_COUNTERS;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPCNTL1General)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPCNT};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id =
        NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_GENERAL_COUNTERS;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPCNTL1Stat)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPCNT};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id =
        NSM_DEBUG_PARAMETER_SUB_ID_PPCNT_GROUP_L1_STAT_COUNTERS;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPCNTDefaultSubId)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPCNT};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id = 0x3F; // invalid sub_id -> default case
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLGL1CapStatus)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLG};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id =
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CAPABILITIES_AND_STATUS;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLGL1Config)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLG};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id = NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_CONFIGURATION;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLGL1Debug)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLG};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id = NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L1_DEBUG;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLGL0CapStatus)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLG};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id =
        NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_CAPABILITIES_AND_STATUS;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLGL0Debug)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLG};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id = NSM_DEBUG_PARAMETER_SUB_ID_PPSLG_PAGE_L0_DEBUG;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsPPSLGDefaultSubId)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_PPSLG};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    sub_id.bits.sub_id = 0x3F; // invalid -> default case
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, GetDeviceDebugParamsDefaultIndex)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = 0xFF}; // default case
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// setDeviceDebugParametersHandler - valid call
// =============================================================================

TEST_F(MockupResponderBranch3Test, SetDeviceDebugParamsValid)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MLPC};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    uint8_t data[4] = {0x11, 0x22, 0x33, 0x44};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_debug_parameters_req) + sizeof(data));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_debug_parameters_req(
        instanceId, 0, param_id, sub_id, sizeof(data), data, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Scalar group telemetry - group 7 and 10
// =============================================================================

TEST_F(MockupResponderBranch3Test, ScalarGroupTelemetryGroup7Unsupported)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_7);
    // GROUP_ID_7 is not in the switch, returns nullopt
    EXPECT_FALSE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, ScalarGroupTelemetryGroup10)
{
    auto resp = mockupResponder->getQueryScalarGroupTelemetryResponse(
        instanceId, GROUP_ID_10);
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Long-running handlers - isLongRunning=true path
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetMigModeLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_MIG_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getMigModeHandler(requestMsg, request.size(),
                                                   true, longRunning);
    // When isLongRunning=true, returns WIP response and sets longRunning
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

TEST_F(MockupResponderBranch3Test, SetMigModeLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_MIG_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_MIG_mode_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->setMigModeHandler(requestMsg, request.size(),
                                                   true, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

TEST_F(MockupResponderBranch3Test, GetEccModeLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_ECC_MODE, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getEccModeHandler(requestMsg, request.size(),
                                                   true, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

TEST_F(MockupResponderBranch3Test, SetEccModeLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_set_ECC_mode_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_ECC_mode_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->setEccModeHandler(requestMsg, request.size(),
                                                   true, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

TEST_F(MockupResponderBranch3Test, GetCurrentUtilizationLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_common_req(instanceId, NSM_TYPE_PLATFORM_ENVIRONMENTAL,
                                NSM_GET_CURRENT_UTILIZATION, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getCurrentUtilizationHandler(
        requestMsg, request.size(), true, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

TEST_F(MockupResponderBranch3Test, GetMemoryCapacityUtilLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_memory_capacity_util_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getMemoryCapacityUtilHandler(
        requestMsg, request.size(), true, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

TEST_F(MockupResponderBranch3Test, GetViolationDurationLongRunning)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_violation_duration_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    std::optional<Request> longRunning;
    auto resp = mockupResponder->getViolationDurationHandler(
        requestMsg, request.size(), true, longRunning);
    EXPECT_TRUE(resp.has_value());
    EXPECT_TRUE(longRunning.has_value());
}

// =============================================================================
// Non-verbose mode tests for coverage of verbose-check else branches
// =============================================================================

TEST_F(MockupResponderBranch3QuietTest, GetPowerSmoothingFeatureInfoQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getPowerSmoothingFeatureInfo(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, GetHwCircuiteryUsageQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_hardware_lifetime_cricuitry_req(instanceId,
                                                         requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getHwCircuiteryUsage(requestMsg,
                                                      request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, GetCurrentProfileInfoQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_current_profile_info_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getCurrentProfileInfo(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, GetQueryAdminOverrideQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_admin_override_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getQueryAdminOverride(requestMsg,
                                                       request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, SetActivePresetProfileQuiet)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_active_preset_profile_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_active_preset_profile_req(instanceId, 1, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setActivePresetProfile(requestMsg,
                                                        request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotCAKBypassQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_cak_bypass_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_cak_bypass_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotCAKBypassHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotUnlockChallengeQuiet)
{
    struct nsm_dot_unlock_challenge_req challenge_req = {};
    challenge_req.unlock_type = 1;
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_dot_unlock_challenge_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_unlock_challenge_req(instanceId, &challenge_req,
                                                  requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotUnlockChallengeHandler(requestMsg,
                                                           request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotGetInfoQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_info_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_info_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotGetInfoHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotGetStatusQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_get_status_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_get_status_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotGetStatusHandler(requestMsg,
                                                     request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotDisableQuiet)
{
    struct nsm_dot_disable_req disable_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_disable_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_disable_req(instanceId, &disable_req, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotDisableHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotOverrideQuiet)
{
    struct nsm_dot_override_req override_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_override_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_override_req(instanceId, &override_req,
                                          requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotOverrideHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DotRecoveryQuiet)
{
    struct nsm_dot_recovery_req recovery_req = {};
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_dot_recovery_req_command));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_nsm_dot_recovery_req(instanceId, &recovery_req,
                                          requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->dotRecoveryHandler(requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, GetDevicemodeSettingsQuiet)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_setting_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_setting_req(instanceId, 0, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDevicemodeSettingsHandler(requestMsg,
                                                              request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, GetProtectionOptionsQuiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_protection_options_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getProtectionOptionsHandler(requestMsg,
                                                             request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, DISABLED_GetDeviceModeSettingsV2Quiet)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_mode_settings_v2_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, SetDeviceModeSettingsV2Quiet)
{
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_mode_settings_v2_req) + sizeof(data));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_mode_settings_v2_req(
        instanceId, DEVICE_MODE_ONE_SHOT_GPU_BASE_POWER_LIMIT, data,
        sizeof(data), requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setDeviceModeSettingsV2Handler(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, GetDeviceDebugParamsQuiet)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MLPC};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_get_device_debug_parameters_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_device_debug_parameters_req(instanceId, 0, param_id,
                                                     sub_id, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, SetDeviceDebugParamsQuiet)
{
    struct nsm_debug_parameter_id param_id = {
        .reserved = 0, .port_number = 0, .index = NSM_DEBUG_PARAMETER_ID_MLPC};
    nsm_debug_parameter_sub_id_bitfield sub_id = {.value = 0};
    uint8_t data[4] = {0x11, 0x22, 0x33, 0x44};
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_device_debug_parameters_req) + sizeof(data));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_device_debug_parameters_req(
        instanceId, 0, param_id, sub_id, sizeof(data), data, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setDeviceDebugParametersHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3QuietTest, WriteProtectedHandlerQuiet)
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

TEST_F(MockupResponderBranch3QuietTest, GetPowerSmoothingFeatureInfoV2Quiet)
{
    Request request(sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_get_powersmoothing_featinfo_v2_req(instanceId, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->getPowerSmoothingFeatureInfoV2(requestMsg,
                                                                request.size());
    EXPECT_TRUE(resp.has_value());
}

// =============================================================================
// Available and clearable scalar groups - additional groups
// =============================================================================

TEST_F(MockupResponderBranch3Test, AvailableClearableGroup2)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_2, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, AvailableClearableGroup5Unsupported)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_query_scalar_group_telemetry_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_query_available_clearable_scalar_data_sources_v1_req(
        instanceId, 0, GROUP_ID_5, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->queryAvailableAndClearableScalarGroupHandler(
        requestMsg, request.size());
    // GROUP_ID_5 is not in the switch, falls to default
    EXPECT_FALSE(resp.has_value());
}

// =============================================================================
// getProperty - additional retimer eeprom versions
// =============================================================================

TEST_F(MockupResponderBranch3Test, GetPropertyPcieRetimer2to6)
{
    auto res2 = mockupResponder->getProperty(PCIERETIMER_2_EEPROM_VERSION);
    EXPECT_EQ(res2.size(), 8u);
    auto res3 = mockupResponder->getProperty(PCIERETIMER_3_EEPROM_VERSION);
    EXPECT_EQ(res3.size(), 8u);
    auto res4 = mockupResponder->getProperty(PCIERETIMER_4_EEPROM_VERSION);
    EXPECT_EQ(res4.size(), 8u);
    auto res5 = mockupResponder->getProperty(PCIERETIMER_5_EEPROM_VERSION);
    EXPECT_EQ(res5.size(), 8u);
    auto res6 = mockupResponder->getProperty(PCIERETIMER_6_EEPROM_VERSION);
    EXPECT_EQ(res6.size(), 8u);
}

// =============================================================================
// Additional setReconfigurationPermissions - more setting indices
// =============================================================================

TEST_F(MockupResponderBranch3Test, SetReconfigPermsConfidentialComputeDevMode)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_CONFIDENTIAL_COMPUTE_DEV_MODE, RP_PERSISTENT, 0x01,
        requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetReconfigPermsClockLimit)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_CLOCK_LIMIT, RP_ONESHOOT_HOT_RESET, 0x02, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetReconfigPermsNvlinkDisable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_NVLINK_DISABLE, RP_ONESHOT_FLR, 0x03, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetReconfigPermsEccEnable)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_ECC_ENABLE, RP_PERSISTENT, 0x04, requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}

TEST_F(MockupResponderBranch3Test, SetReconfigPermsEdppScalingFactor)
{
    Request request(sizeof(nsm_msg_hdr) +
                    sizeof(nsm_set_reconfiguration_permissions_v1_req));
    auto requestMsg = reinterpret_cast<nsm_msg*>(request.data());
    auto rc = encode_set_reconfiguration_permissions_v1_req(
        instanceId, RP_EDPP_SCALING_FACTOR, RP_ONESHOOT_HOT_RESET, 0x05,
        requestMsg);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    auto resp = mockupResponder->setReconfigurationPermissionsV1Handler(
        requestMsg, request.size());
    EXPECT_TRUE(resp.has_value());
}
