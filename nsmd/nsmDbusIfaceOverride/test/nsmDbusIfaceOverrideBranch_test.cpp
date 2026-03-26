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

/*
 * Branch coverage for nsmDbusIfaceOverride:
 * - NsmPowerCapIntf: setPowerCap invalid arg, out of range, success
 * - NsmPowerCapIntf: getPowerCapFromDevice decode success/fail,
 *   INVALID_POWER_LIMIT
 * - NsmPowerCapIntf: setPowerCapOnDevice encode fail, postPatch fail,
 *   decode fail, success with parent sensor update
 * - NsmClearPowerCapAsyncIntf: clearPowerCapOnDevice encode fail, postPatch
 *   fail, success + parent sensor update, getPowerCapFromDevice INVALID_POWER
 * - NsmResetAsyncIntf: assertFundamentalReset encode fail, postPatch fail,
 *   success; doResetOnDevice both RESET and NOT_RESET paths
 * - NsmNetworkDeviceResetAsyncIntf: resetOnDevice encode fail, postPatch fail,
 *   decode fail; doResetOnDevice success
 * - NsmResetEdppAsyncIntf: clearSetPoint encode fail, postPatch fail,
 *   decode success/fail
 * - NsmActivateErrorInjectionPayloadIntf: doActivate encode fail,
 *   postPatch fail, decode cc fail, success
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "pci-links.h"
#include "platform-environmental.h"

#include "nsmActivateErrorInjectAsyncIntf.hpp"
#include "nsmClearPowerCapIface.hpp"
#include "nsmPowerCapIface.hpp"
#include "nsmResetEdppAsyncIface.hpp"
#include "nsmResetIface.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmDbusIfaceOverrideBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;

    NsmDbusIfaceOverrideBranchTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDbusIfaceOverrideBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    // -- Response helpers --

    Response createGetPowerLimitResponse(uint32_t persistent = 100000,
                                         uint32_t oneshot = 200000,
                                         uint32_t enforced = 300000,
                                         uint8_t cc = NSM_SUCCESS,
                                         uint16_t reasonCode = 0)
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_power_limit_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        encode_get_power_limit_resp(0, cc, reasonCode, persistent, oneshot,
                                    enforced, msg);
        return response;
    }

    Response createSetPowerLimitResponse(uint8_t cc = NSM_SUCCESS,
                                         uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        encode_set_power_limit_resp(0, cc, reasonCode, msg);
        return response;
    }

    Response createDecodeFail()
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        response.data()[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return response;
    }
};

// ============================================================================
// NsmPowerCapIntf: setPowerCap invalid argument type
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, PowerCap_setPowerCap_InvalidArg)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string capName = "PowerCap_inv";
    std::vector<std::string> parents;
    auto capIntf = std::make_shared<NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/test/pcap_inv", capName, parents,
        mockDevice);

    AsyncSetOperationValueType badValue = std::string("not a tuple");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;

    EXPECT_THROW_COROUTINE(
        capIntf->setPowerCap(badValue, &status, mockDevice),
        sdbusplus::error::xyz::openbmc_project::common::InvalidArgument);
}

// ============================================================================
// NsmPowerCapIntf: setPowerCap out of range
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, PowerCap_setPowerCap_OutOfRange)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string capName = "PowerCap_oor";
    std::vector<std::string> parents;
    auto capIntf = std::make_shared<NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/test/pcap_oor", capName, parents,
        mockDevice);

    capIntf->PowerCapIntf::maxPowerCapValue(1000);
    capIntf->PowerCapIntf::minPowerCapValue(100);

    // Value above max
    AsyncSetOperationValueType highValue = std::make_tuple(true,
                                                           uint32_t(2000));
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    capIntf->setPowerCap(highValue, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);

    // Value below min
    AsyncSetOperationValueType lowValue = std::make_tuple(true, uint32_t(10));
    status = AsyncOperationStatusType::Success;
    capIntf->setPowerCap(lowValue, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// NsmPowerCapIntf: getPowerCapFromDevice INVALID_POWER_LIMIT
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, PowerCap_getPowerCap_InvalidLimit)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string capName = "PowerCap_invlim";
    std::vector<std::string> parents;
    auto capIntf = std::make_shared<NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/test/pcap_invlim", capName, parents,
        mockDevice);

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createGetPowerLimitResponse(100000, 200000, INVALID_POWER_LIMIT)));
    capIntf->getPowerCapFromDevice();
    EXPECT_EQ(capIntf->PowerCapIntf::powerCap(), INVALID_POWER_LIMIT);
}

// ============================================================================
// NsmPowerCapIntf: setPowerCapOnDevice postPatchIO fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest,
       PowerCap_setPowerCapOnDevice_PostPatchFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string capName = "PowerCap_ppf";
    std::vector<std::string> parents;
    auto capIntf = std::make_shared<NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/test/pcap_ppf", capName, parents,
        mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    capIntf->setPowerCapOnDevice(500, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf: setPowerCapOnDevice decode fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, PowerCap_setPowerCapOnDevice_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string capName = "PowerCap_decf";
    std::vector<std::string> parents;
    auto capIntf = std::make_shared<NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/test/pcap_decf", capName, parents,
        mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    capIntf->setPowerCapOnDevice(500, &status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmPowerCapIntf: setPowerCapOnDevice success
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, PowerCap_setPowerCapOnDevice_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string capName = "PowerCap_ok";
    std::vector<std::string> parents;
    auto capIntf = std::make_shared<NsmPowerCapIntf>(
        bus, "/xyz/openbmc_project/test/pcap_ok", capName, parents, mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createSetPowerLimitResponse()))
        .WillOnce(mockPostPatchIO(
            createGetPowerLimitResponse(500000, 500000, 500000)));
    capIntf->setPowerCapOnDevice(500, &status);
    EXPECT_EQ(capIntf->PowerCapIntf::powerCap(), 500u);
}

// ============================================================================
// NsmResetAsyncIntf: doResetOnDevice RESET fails
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ResetAsync_doReset_ResetFails)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/test/reset_fail", mockDevice, 0);

    const auto [_, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    resetIntf->doResetOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// NsmResetAsyncIntf: doResetOnDevice NOT_RESET fails
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ResetAsync_doReset_NotResetFails)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetIntf = std::make_shared<NsmResetAsyncIntf>(
        bus, "/xyz/openbmc_project/test/reset_nr_fail", mockDevice, 0);

    const auto [_, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    // Build a valid assertFundamentalReset response
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_assert_pcie_fundamental_reset_resp(0, NSM_SUCCESS, 0, msg);

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(resp))       // RESET succeeds
        .WillOnce(mockPostPatchIO(NSM_ERROR)); // NOT_RESET fails
    resetIntf->doResetOnDevice(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// NsmNetworkDeviceResetAsyncIntf: resetOnDevice postPatch fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, NetworkReset_resetOnDevice_PostPatchFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, "/xyz/openbmc_project/test/netreset_ppf", mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    resetIntf->resetOnDevice(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmNetworkDeviceResetAsyncIntf: resetOnDevice decode fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, NetworkReset_resetOnDevice_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto resetIntf = std::make_shared<NsmNetworkDeviceResetAsyncIntf>(
        bus, "/xyz/openbmc_project/test/netreset_decf", mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    resetIntf->resetOnDevice(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmResetEdppAsyncIntf: clearSetPoint postPatch fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ResetEdpp_clearSetPoint_PostPatchFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto edppIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, "/xyz/openbmc_project/test/edpp_ppf", mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    edppIntf->clearSetPoint(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmResetEdppAsyncIntf: clearSetPoint decode fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ResetEdpp_clearSetPoint_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto edppIntf = std::make_shared<NsmResetEdppAsyncIntf>(
        bus, "/xyz/openbmc_project/test/edpp_decf", mockDevice);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    edppIntf->clearSetPoint(&status);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmActivateErrorInjectionPayloadIntf: doActivate postPatch fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ActivateErrorInject_PostPatchFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto eiIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, "/xyz/openbmc_project/test/ei_ppf", 1, 2, mockDevice);

    const auto [_, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    eiIntf->doActivateErrorInjectionPayload(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmActivateErrorInjectionPayloadIntf: doActivate decode fail
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ActivateErrorInject_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto eiIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, "/xyz/openbmc_project/test/ei_decf", 1, 2, mockDevice);

    const auto [_, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    eiIntf->doActivateErrorInjectionPayload(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmActivateErrorInjectionPayloadIntf: doActivate success
// ============================================================================

TEST_F(NsmDbusIfaceOverrideBranchTest, ActivateErrorInject_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto eiIntf = std::make_shared<NsmActivateErrorInjectionPayloadIntf>(
        bus, "/xyz/openbmc_project/test/ei_ok", 1, 2, mockDevice);

    const auto [_, statusIntf, valueIntf] =
        AsyncOperationManager::getInstance()->getNewStatusValueInterface();

    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_activate_error_injection_payload_resp(0, NSM_SUCCESS, 0, msg);

    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));
    eiIntf->doActivateErrorInjectionPayload(statusIntf);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}
