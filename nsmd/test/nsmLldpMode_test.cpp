/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

// Unit tests for NsmLldpMode: cold-start read, DCBX precheck and the
// TX / RX / DCBX setters.

#include "test/mockDBusHandler.hpp"

#include <algorithm>
#include <cstring>

#define private public
#define protected public

#include "libnsm/device-configuration.h"

#include "nsmLLDPLib/nsmLldpMode.hpp"
#include "nsmLLDPLib/nsmLldpPacket.hpp"
#include "nsmLLDPLib/nsmLldpPort.hpp"
#include "test/mockSensorManager.hpp"

using namespace nsm;
using namespace ::testing;

namespace
{

constexpr auto txAll = "com.nvidia.Network.LLDP.Modes.LLDPModeType.All";
constexpr auto txMandatory =
    "com.nvidia.Network.LLDP.Modes.LLDPModeType.Mandatory";
constexpr auto txOff = "com.nvidia.Network.LLDP.Modes.LLDPModeType.Off";
constexpr auto dcbxEnabled =
    "com.nvidia.Network.LLDP.Modes.DCBXModeType.Enabled";
constexpr auto dcbxDisabled =
    "com.nvidia.Network.LLDP.Modes.DCBXModeType.Disabled";

} // namespace

struct NsmLldpModeTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;
    std::unique_ptr<NsmLldpMode> mode;

    NsmLldpModeTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(
                "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0"));
        EXPECT_NE(device, nullptr);
        mode = std::make_unique<NsmLldpMode>(
            utils::DBusHandler::getBus(), "LldpMode", "NSM_LLDP_Mode",
            "/xyz/openbmc_project/inventory/system/chassis/CX_NIC_0/Settings/"
            "Oem/Nvidia/LLDPModes",
            "/xyz/openbmc_project/inventory/system/chassis/CX_NIC_0", device);
    }

    ~NsmLldpModeTest()
    {
        mode.reset();
        cleanupDeviceSensors(devices);
    }

    /** @brief Get Device Mode Settings v2 response carrying one mode byte */
    static Response getResponse(uint8_t tx, uint8_t rx, uint8_t dcbx,
                                uint16_t currentLength = 1)
    {
        nsm_lldp_mode_bitfield bits{};
        bits.tx_mode = tx;
        bits.rx_mode = rx;
        bits.dcbx_mode = dcbx;
        uint8_t current[2] = {};
        std::memcpy(current, &bits, 1);
        uint8_t pending[1] = {0};

        Response response(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_device_mode_settings_v2_resp) + 8,
                          0);
        auto* msg = reinterpret_cast<nsm_msg*>(response.data());
        EXPECT_EQ(
            encode_get_device_mode_settings_v2_resp(
                0, NSM_SUCCESS, 0, current, currentLength, pending, 1, msg),
            NSM_SW_SUCCESS);
        return response;
    }

    static Response setResponse(uint8_t cc = NSM_SUCCESS)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + 16,
                          0);
        auto* msg = reinterpret_cast<nsm_msg*>(response.data());
        EXPECT_EQ(encode_set_device_mode_settings_v2_resp(0, cc, 0x0101, msg),
                  NSM_SW_SUCCESS);
        return response;
    }

    uint8_t handle(const Response& response)
    {
        return mode->handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    }

    /** @brief Run a setter with a string value and return the status */
    bool threw = false;

    template <typename Setter>
    AsyncOperationStatusType set(Setter setter,
                                 const AsyncSetOperationValueType& value,
                                 requester::Coroutine& rc)
    {
        AsyncOperationStatusType status = AsyncOperationStatusType::InProgress;
        threw = false;
        try
        {
            rc = (mode.get()->*setter)(value, &status, device);
        }
        catch (const std::exception&)
        {
            // Coroutines run inline in coverage builds, so the error
            // surfaces here instead of in rc.exception()
            threw = true;
        }
        return status;
    }
};

// ---------------------------------------------------------------------------
// Construction, request and cold-start read
// ---------------------------------------------------------------------------

TEST_F(NsmLldpModeTest, Constructor_StartsOff)
{
    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::Off);
    EXPECT_EQ(mode->rxMode(), NsmLldpMode::LLDPModeType::Off);
    EXPECT_EQ(mode->dcbxMode(), NsmLldpMode::DCBXModeType::Disabled);
    EXPECT_NE(mode->dbusObjectPath().find("LLDPModes"), std::string::npos);
}

TEST_F(NsmLldpModeTest, GenRequestMsg_Encodes)
{
    auto request = mode->genRequestMsg(8, 2);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) +
                                   sizeof(nsm_get_device_mode_settings_v2_req));
}

TEST_F(NsmLldpModeTest, HandleResponse_PublishesDeviceModes)
{
    EXPECT_EQ(
        handle(getResponse(NSM_LLDP_DIR_MODE_ALL, NSM_LLDP_DIR_MODE_MANDATORY,
                           NSM_LLDP_DCBX_ENABLED)),
        NSM_SUCCESS);

    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::All);
    EXPECT_EQ(mode->rxMode(), NsmLldpMode::LLDPModeType::Mandatory);
    EXPECT_EQ(mode->dcbxMode(), NsmLldpMode::DCBXModeType::Enabled);
}

TEST_F(NsmLldpModeTest, HandleResponse_OffAndDisabled)
{
    ASSERT_EQ(handle(getResponse(NSM_LLDP_DIR_MODE_ALL, NSM_LLDP_DIR_MODE_ALL,
                                 NSM_LLDP_DCBX_ENABLED)),
              NSM_SUCCESS);

    EXPECT_EQ(handle(getResponse(NSM_LLDP_DIR_MODE_OFF, NSM_LLDP_DIR_MODE_OFF,
                                 NSM_LLDP_DCBX_DISABLED)),
              NSM_SUCCESS);

    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::Off);
    EXPECT_EQ(mode->dcbxMode(), NsmLldpMode::DCBXModeType::Disabled);
}

TEST_F(NsmLldpModeTest, HandleResponse_ReservedValueMapsToOff)
{
    ASSERT_EQ(handle(getResponse(NSM_LLDP_DIR_MODE_ALL, NSM_LLDP_DIR_MODE_ALL,
                                 NSM_LLDP_DCBX_DISABLED)),
              NSM_SUCCESS);

    // 3 is not a defined TX / RX mode
    EXPECT_EQ(handle(getResponse(3, 3, NSM_LLDP_DCBX_DISABLED)), NSM_SUCCESS);

    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::Off);
    EXPECT_EQ(mode->rxMode(), NsmLldpMode::LLDPModeType::Off);
}

TEST_F(NsmLldpModeTest, HandleResponse_WrongPayloadLength_Fails)
{
    EXPECT_EQ(handle(getResponse(NSM_LLDP_DIR_MODE_ALL, NSM_LLDP_DIR_MODE_ALL,
                                 NSM_LLDP_DCBX_DISABLED, 2)),
              NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::Off);
}

TEST_F(NsmLldpModeTest, HandleResponse_ErrorCompletionCode_IsReturned)
{
    Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + 16, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(response.data());
    ASSERT_EQ(encode_get_device_mode_settings_v2_resp(
                  0, NSM_ERROR, 0x0101, nullptr, 0, nullptr, 0, msg),
              NSM_SW_SUCCESS);

    EXPECT_EQ(handle(response), NSM_ERROR);
}

TEST_F(NsmLldpModeTest, HandleResponse_TooShortMessage_Fails)
{
    auto response = getResponse(0, 0, 0);

    EXPECT_NE(mode->handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(response.data()), 2),
              NSM_SUCCESS);
}

// ---------------------------------------------------------------------------
// DCBX precheck
// ---------------------------------------------------------------------------

TEST_F(NsmLldpModeTest, DcbxPrecheck_OnlyEnabledNeedsBothAll)
{
    using L = NsmLldpMode::LLDPModeType;
    using D = NsmLldpMode::DCBXModeType;

    EXPECT_TRUE(NsmLldpMode::dcbxPrecheck(L::Off, L::Off, D::Disabled));
    EXPECT_TRUE(NsmLldpMode::dcbxPrecheck(L::All, L::All, D::Enabled));
    EXPECT_FALSE(NsmLldpMode::dcbxPrecheck(L::Mandatory, L::All, D::Enabled));
    EXPECT_FALSE(NsmLldpMode::dcbxPrecheck(L::All, L::Off, D::Enabled));
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

TEST_F(NsmLldpModeTest, SetTxMode_Success_UpdatesProperty)
{
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setTxMode, std::string(txAll), rc);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_FALSE(rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::All);
    EXPECT_FALSE(mode->asyncPatchInProgress_);
}

TEST_F(NsmLldpModeTest, SetRxMode_Success_UpdatesProperty)
{
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setRxMode, std::string(txMandatory), rc);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(mode->rxMode(), NsmLldpMode::LLDPModeType::Mandatory);
}

TEST_F(NsmLldpModeTest, SetDcbxMode_EnableWithAllAll_Succeeds)
{
    mode->intf_->txMode(NsmLldpMode::LLDPModeType::All);
    mode->intf_->rxMode(NsmLldpMode::LLDPModeType::All);
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setDcbxMode, std::string(dcbxEnabled), rc);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(mode->dcbxMode(), NsmLldpMode::DCBXModeType::Enabled);
}

TEST_F(NsmLldpModeTest, SetDcbxMode_DisableAlwaysAllowed)
{
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setDcbxMode, std::string(dcbxDisabled), rc);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
}

TEST_F(NsmLldpModeTest, SetDcbxMode_EnableWithoutAllAll_IsRejected)
{
    EXPECT_CALL(*device, postPatchIO).Times(0);
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setDcbxMode, std::string(dcbxEnabled), rc);

    EXPECT_TRUE(threw || rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(mode->dcbxMode(), NsmLldpMode::DCBXModeType::Disabled);
    EXPECT_FALSE(mode->asyncPatchInProgress_);
}

TEST_F(NsmLldpModeTest, SetTxMode_BreakingDcbxPrerequisite_IsRejected)
{
    mode->intf_->txMode(NsmLldpMode::LLDPModeType::All);
    mode->intf_->rxMode(NsmLldpMode::LLDPModeType::All);
    mode->intf_->dcbxMode(NsmLldpMode::DCBXModeType::Enabled);
    EXPECT_CALL(*device, postPatchIO).Times(0);
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setTxMode, std::string(txOff), rc);

    EXPECT_TRUE(threw || rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::All);
}

TEST_F(NsmLldpModeTest, SetRxMode_BreakingDcbxPrerequisite_IsRejected)
{
    mode->intf_->txMode(NsmLldpMode::LLDPModeType::All);
    mode->intf_->rxMode(NsmLldpMode::LLDPModeType::All);
    mode->intf_->dcbxMode(NsmLldpMode::DCBXModeType::Enabled);
    EXPECT_CALL(*device, postPatchIO).Times(0);
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setRxMode, std::string(txMandatory), rc);

    EXPECT_TRUE(threw || rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmLldpModeTest, Setters_NonStringValue_AreRejected)
{
    EXPECT_CALL(*device, postPatchIO).Times(0);

    for (auto setter : {&NsmLldpMode::setTxMode, &NsmLldpMode::setRxMode,
                        &NsmLldpMode::setDcbxMode})
    {
        requester::Coroutine rc;
        auto status = set(setter, uint64_t{1}, rc);

        EXPECT_TRUE(threw || rc.exception());
        EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
        EXPECT_FALSE(mode->asyncPatchInProgress_);
    }
}

TEST_F(NsmLldpModeTest, Setters_UnknownEnumString_AreRejected)
{
    EXPECT_CALL(*device, postPatchIO).Times(0);

    for (auto setter : {&NsmLldpMode::setTxMode, &NsmLldpMode::setRxMode,
                        &NsmLldpMode::setDcbxMode})
    {
        requester::Coroutine rc;
        auto status = set(setter, std::string("NotAMode"), rc);

        EXPECT_TRUE(threw || rc.exception());
        EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
        EXPECT_FALSE(mode->asyncPatchInProgress_);
    }
}

TEST_F(NsmLldpModeTest, Setters_WhilePatchInProgress_AreUnavailable)
{
    mode->asyncPatchInProgress_ = true;
    EXPECT_CALL(*device, postPatchIO).Times(0);

    for (auto setter : {&NsmLldpMode::setTxMode, &NsmLldpMode::setRxMode,
                        &NsmLldpMode::setDcbxMode})
    {
        requester::Coroutine rc;
        auto status = set(setter, std::string(txAll), rc);

        EXPECT_EQ(rc.data(), NSM_SW_ERROR);
        EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
    }
}

TEST_F(NsmLldpModeTest, Set_TransportFailure_IsWriteFailure)
{
    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(setResponse(), NSM_ERROR));
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setTxMode, std::string(txAll), rc);

    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::Off);
}

TEST_F(NsmLldpModeTest, Set_DeviceReportsError_IsWriteFailure)
{
    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(setResponse(NSM_ERROR)));
    requester::Coroutine rc;

    auto status = set(&NsmLldpMode::setTxMode, std::string(txAll), rc);

    EXPECT_NE(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(mode->txMode(), NsmLldpMode::LLDPModeType::Off);
}

// ---------------------------------------------------------------------------
// nsmLldpPort.cpp helpers
// ---------------------------------------------------------------------------

TEST(NsmLldpPortTest, IsLldpModeIndexSupported_FollowsDeviceModesSupported)
{
    EXPECT_FALSE(isLldpModeIndexSupported({}));
    EXPECT_FALSE(
        isLldpModeIndexSupported(std::vector<int64_t>(DEVICE_MODE_LLDP, 0)));

    std::vector<int64_t> modes(DEVICE_MODE_LLDP + 1, DEVICE_MODE_NOT_SUPPORTED);
    EXPECT_FALSE(isLldpModeIndexSupported(modes));

    modes[DEVICE_MODE_LLDP] = 0;
    EXPECT_TRUE(isLldpModeIndexSupported(modes));

    modes[DEVICE_MODE_LLDP] = 5;
    EXPECT_TRUE(isLldpModeIndexSupported(modes));
}

TEST_F(NsmLldpModeTest, CreateLldpPacketSensorsForPort_AddsRxAndTx)
{
    const auto isPacket = [](const auto& sensor) {
        return std::dynamic_pointer_cast<NsmLldpPacket>(sensor) != nullptr;
    };
    const auto packetsBefore = std::count_if(device->roundRobinSensors.begin(),
                                             device->roundRobinSensors.end(),
                                             isPacket);

    createLldpPacketSensorsForPort(
        utils::DBusHandler::getBus(), device, "CX_NIC_0_Port_1", "NSM_LLDP",
        "/xyz/openbmc_project/inventory/system/chassis/CX_NIC_0/Ports/Port_1",
        1, false);

    const auto packetsAfter = std::count_if(device->roundRobinSensors.begin(),
                                            device->roundRobinSensors.end(),
                                            isPacket);
    EXPECT_EQ(packetsAfter, packetsBefore + 2);
}

TEST_F(NsmLldpModeTest, CreateLldpModeSensor_AddsStaticSensorAndHandlers)
{
    const size_t staticBefore = device->staticSensors.size();

    createLldpModeSensor(
        utils::DBusHandler::getBus(), device, "CX_NIC_1", "NSM_LLDP",
        "/xyz/openbmc_project/inventory/system/chassis/CX_NIC_1");

    EXPECT_EQ(device->staticSensors.size(), staticBefore + 1);
}
