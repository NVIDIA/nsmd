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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "nsmSecurityRBP.hpp"

using namespace nsm;

struct NsmSecurityRBPTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string objPath = "/xyz/openbmc_project/software/security";
    const std::string name = "SecurityRBP";
    const std::string type = "NSM_SecurityConfig";
    const uuid_t uuid = "12345678-1234-1234-1234-123456789012";

    NsmDeviceTable devices;

    NsmSecurityRBPTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmSecurityRBPTest, goodTestBasic)
{
    // Basic test to verify test infrastructure works
    EXPECT_EQ(name, "SecurityRBP");
    EXPECT_EQ(type, "NSM_SecurityConfig");
}

TEST_F(NsmSecurityRBPTest, testSecurityConfigurationConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto progressIntf = std::make_shared<ProgressIntf>(bus, objPath.c_str());

    MockSensor mockSensor("TestSensor", "TestType");

    auto securityConfig = std::make_shared<SecurityConfiguration>(
        bus, objPath, uuid, progressIntf, mockSensor);

    EXPECT_NE(securityConfig, nullptr);
    EXPECT_EQ(securityConfig->uuid, uuid);
}

TEST_F(NsmSecurityRBPTest, testSecurityConfigurationUpdateState)
{
    auto& bus = utils::DBusHandler::getBus();
    auto progressIntf = std::make_shared<ProgressIntf>(bus, objPath.c_str());

    MockSensor mockSensor("TestSensor", "TestType");

    auto securityConfig = std::make_shared<SecurityConfiguration>(
        bus, objPath, uuid, progressIntf, mockSensor);

    struct ::nsm_firmware_irreversible_config_request_0_resp cfg_state;
    cfg_state.irreversible_config_state = 1;

    EXPECT_NO_THROW(securityConfig->updateState(cfg_state));
    EXPECT_EQ(securityConfig->irreversibleConfigState(), 1);
}

TEST_F(NsmSecurityRBPTest, testSecurityConfigurationUpdateNonce)
{
    auto& bus = utils::DBusHandler::getBus();
    auto progressIntf = std::make_shared<ProgressIntf>(bus, objPath.c_str());

    MockSensor mockSensor("TestSensor", "TestType");

    auto securityConfig = std::make_shared<SecurityConfiguration>(
        bus, objPath, uuid, progressIntf, mockSensor);

    struct ::nsm_firmware_irreversible_config_request_2_resp cfg_resp;
    cfg_resp.nonce = 0x12345678;

    EXPECT_NO_THROW(securityConfig->updateNonce(cfg_resp));
    EXPECT_EQ(securityConfig->nonce(), 0x12345678);
}

TEST_F(NsmSecurityRBPTest, testMultipleSecurityConfigurations)
{
    auto& bus = utils::DBusHandler::getBus();
    auto progressIntf1 = std::make_shared<ProgressIntf>(bus, objPath.c_str());
    auto progressIntf2 =
        std::make_shared<ProgressIntf>(bus, (objPath + "2").c_str());

    MockSensor mockSensor("TestSensor", "TestType");

    auto securityConfig1 = std::make_shared<SecurityConfiguration>(
        bus, objPath, uuid, progressIntf1, mockSensor);

    uuid_t uuid2 = "87654321-4321-4321-4321-210987654321";
    auto securityConfig2 = std::make_shared<SecurityConfiguration>(
        bus, objPath + "2", uuid2, progressIntf2, mockSensor);

    EXPECT_NE(securityConfig1, nullptr);
    EXPECT_NE(securityConfig2, nullptr);
    EXPECT_NE(securityConfig1, securityConfig2);
    EXPECT_EQ(securityConfig1->uuid, uuid);
    EXPECT_EQ(securityConfig2->uuid, uuid2);
}

TEST_F(NsmSecurityRBPTest, testSecurityConfigurationWithDifferentStates)
{
    auto& bus = utils::DBusHandler::getBus();
    auto progressIntf = std::make_shared<ProgressIntf>(bus, objPath.c_str());

    MockSensor mockSensor("TestSensor", "TestType");

    auto securityConfig = std::make_shared<SecurityConfiguration>(
        bus, objPath, uuid, progressIntf, mockSensor);

    // Test with state 0 (false)
    struct ::nsm_firmware_irreversible_config_request_0_resp cfg_state0;
    cfg_state0.irreversible_config_state = 0;
    securityConfig->updateState(cfg_state0);
    EXPECT_FALSE(securityConfig->irreversibleConfigState());

    // Test with state 1 (true)
    struct ::nsm_firmware_irreversible_config_request_0_resp cfg_state1;
    cfg_state1.irreversible_config_state = 1;
    securityConfig->updateState(cfg_state1);
    EXPECT_TRUE(securityConfig->irreversibleConfigState());
}

TEST_F(NsmSecurityRBPTest, testSecurityConfigurationWithDifferentNonces)
{
    auto& bus = utils::DBusHandler::getBus();
    auto progressIntf = std::make_shared<ProgressIntf>(bus, objPath.c_str());

    MockSensor mockSensor("TestSensor", "TestType");

    auto securityConfig = std::make_shared<SecurityConfiguration>(
        bus, objPath, uuid, progressIntf, mockSensor);

    // Test with nonce 0
    struct ::nsm_firmware_irreversible_config_request_2_resp cfg_resp0;
    cfg_resp0.nonce = 0;
    securityConfig->updateNonce(cfg_resp0);
    EXPECT_EQ(securityConfig->nonce(), 0);

    // Test with max nonce
    struct ::nsm_firmware_irreversible_config_request_2_resp cfg_respMax;
    cfg_respMax.nonce = 0xFFFFFFFF;
    securityConfig->updateNonce(cfg_respMax);
    EXPECT_EQ(securityConfig->nonce(), 0xFFFFFFFF);
}
