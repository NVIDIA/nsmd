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

/*
 * Unit tests for the Device Mode Settings v2 sensors of a network adapter
 * (DPU operation mode, PCIe device mode): the cold-start read, the PATCH
 * dispatchers and the feature gate in createNetworkAdapterFeature.
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/device-configuration.h"

#include "features/creators.hpp"
#include "nsmNetworkAdapter.hpp"

#undef private
#undef protected

using namespace nsm;

namespace
{

constexpr uint8_t noChange = 0xFF;

using Bytes = std::vector<uint8_t>;

/** @brief Get Device Mode Settings v2 response with the given mode bytes */
Bytes getResponse(const Bytes& current, const Bytes& pending,
                  uint8_t cc = NSM_SUCCESS)
{
    Bytes response(sizeof(nsm_msg_hdr) +
                       sizeof(nsm_get_device_mode_settings_v2_resp) +
                       current.size() + pending.size() + 8,
                   0);
    auto* msg = reinterpret_cast<nsm_msg*>(response.data());
    EXPECT_EQ(encode_get_device_mode_settings_v2_resp(
                  0, cc, cc == NSM_SUCCESS ? 0 : 0x0101,
                  current.empty() ? nullptr : current.data(),
                  static_cast<uint16_t>(current.size()),
                  pending.empty() ? nullptr : pending.data(),
                  static_cast<uint16_t>(pending.size()), msg),
              NSM_SW_SUCCESS);
    return response;
}

Bytes setResponse(uint8_t cc = NSM_SUCCESS)
{
    Bytes response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp) + 16, 0);
    auto* msg = reinterpret_cast<nsm_msg*>(response.data());
    EXPECT_EQ(encode_set_device_mode_settings_v2_resp(0, cc, 0x0101, msg),
              NSM_SW_SUCCESS);
    return response;
}

} // namespace

struct DeviceModeV2Test :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    static inline int counter = 0;

    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;
    std::string suffix = std::to_string(counter++);

    DeviceModeV2Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~DeviceModeV2Test() override
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<DPUOperationModeIntf> makeDpuIntf()
    {
        auto intf = std::make_shared<DPUOperationModeIntf>(
            utils::DBusHandler::getBus(),
            ("/xyz/openbmc_project/test/dpu/" + suffix).c_str());
        intf->currentMode(OperationMode::DPU);
        intf->pendingMode(OperationMode::DPU);
        return intf;
    }

    std::shared_ptr<PCIeDeviceModeIntf> makePcieIntf()
    {
        auto intf = std::make_shared<PCIeDeviceModeIntf>(
            utils::DBusHandler::getBus(),
            ("/xyz/openbmc_project/test/pcie/" + suffix).c_str());
        intf->PCIeMultiSocketServer::currentMode(0);
        intf->PCIeMultiSocketServer::pendingMode(0);
        intf->PCIeControlledEWTrafficServer::currentMode(
            EWTrafficMode::Disabled);
        intf->PCIeControlledEWTrafficServer::pendingMode(
            EWTrafficMode::Disabled);
        intf->PCIeBifurcationServer::currentMode(0);
        intf->PCIeBifurcationServer::pendingMode(0);
        return intf;
    }

    static uint8_t handle(NsmSensor& sensor, const Bytes& response)
    {
        return sensor.handleResponseMsg(
            reinterpret_cast<const nsm_msg*>(response.data()), response.size());
    }

    bool threw = false;

    template <typename Sensor, typename Method>
    AsyncOperationStatusType set(Sensor& sensor, Method method,
                                 const AsyncSetOperationValueType& value,
                                 requester::Coroutine& rc)
    {
        AsyncOperationStatusType status = AsyncOperationStatusType::InProgress;
        threw = false;
        try
        {
            rc = (sensor.*method)(value, &status, device);
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
// Base class
// ---------------------------------------------------------------------------

TEST(DeviceModeV2BaseTest, IsModeBitSet)
{
    EXPECT_TRUE(NsmDeviceModeSettingsV2Base::isModeBitSet(0x01, 0));
    EXPECT_FALSE(NsmDeviceModeSettingsV2Base::isModeBitSet(0x01, 1));
    EXPECT_TRUE(NsmDeviceModeSettingsV2Base::isModeBitSet(0x84, 7));
    EXPECT_TRUE(NsmDeviceModeSettingsV2Base::isModeBitSet(0x84, 2));
    EXPECT_FALSE(NsmDeviceModeSettingsV2Base::isModeBitSet(0x00, 3));
}

TEST_F(DeviceModeV2Test, SetSensors_AreNotPolled)
{
    NsmDPUOperationModeDeviceModeSettingsV2Set sensor("DpuSet", "NSM_Test", 1,
                                                      makeDpuIntf());

    EXPECT_FALSE(sensor.genRequestMsg(8, 1).has_value());
    EXPECT_EQ(sensor.handleResponseMsg(nullptr, 0), NSM_SW_SUCCESS);
    auto rc = sensor.update(device);
    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
}

TEST_F(DeviceModeV2Test, Patchability_FollowsTheBitmap)
{
    NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test",
                                                    0b101, makePcieIntf());

    EXPECT_TRUE(sensor.isSubDeviceModePatchable(SUB_MODE_PCIE_MULTI_SOCKET));
    EXPECT_FALSE(
        sensor.isSubDeviceModePatchable(SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC));
    EXPECT_TRUE(sensor.isSubDeviceModePatchable(SUB_MODE_PCIE_BIFURCATION));
}

// ---------------------------------------------------------------------------
// DPU operation mode: read
// ---------------------------------------------------------------------------

TEST_F(DeviceModeV2Test, DpuGet_GenRequestMsg_Encodes)
{
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      makeDpuIntf());

    auto request = sensor.genRequestMsg(8, 1);

    ASSERT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) +
                                   sizeof(nsm_get_device_mode_settings_v2_req));
}

TEST_F(DeviceModeV2Test, DpuGet_PublishesCurrentAndPending)
{
    auto intf = makeDpuIntf();
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      intf);

    // 0 = DPU, 1 = NIC
    EXPECT_EQ(handle(sensor, getResponse({1}, {1})), NSM_SW_SUCCESS);
    EXPECT_EQ(intf->currentMode(), OperationMode::NIC);
    EXPECT_EQ(intf->pendingMode(), OperationMode::NIC);

    EXPECT_EQ(handle(sensor, getResponse({0}, {0})), NSM_SW_SUCCESS);
    EXPECT_EQ(intf->currentMode(), OperationMode::DPU);
    EXPECT_EQ(intf->pendingMode(), OperationMode::DPU);
}

TEST_F(DeviceModeV2Test, DpuGet_NoChangeAndEmptyPayloadsKeepValues)
{
    auto intf = makeDpuIntf();
    intf->currentMode(OperationMode::NIC);
    intf->pendingMode(OperationMode::NIC);
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      intf);

    EXPECT_EQ(handle(sensor, getResponse({noChange}, {noChange})),
              NSM_SW_SUCCESS);
    EXPECT_EQ(handle(sensor, getResponse({}, {})), NSM_SW_SUCCESS);

    EXPECT_EQ(intf->currentMode(), OperationMode::NIC);
    EXPECT_EQ(intf->pendingMode(), OperationMode::NIC);
}

TEST_F(DeviceModeV2Test, DpuGet_WithoutInterface_StillSucceeds)
{
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      nullptr);

    EXPECT_EQ(handle(sensor, getResponse({1}, {1})), NSM_SW_SUCCESS);
}

TEST_F(DeviceModeV2Test, Get_OversizedPayload_IsRejected)
{
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      makeDpuIntf());

    EXPECT_EQ(handle(sensor, getResponse(Bytes(20, 1), {1})),
              NSM_SW_ERROR_LENGTH);
    EXPECT_EQ(handle(sensor, getResponse({1}, Bytes(20, 1))),
              NSM_SW_ERROR_LENGTH);
}

TEST_F(DeviceModeV2Test, Get_ErrorCompletionCode_IsReturned)
{
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      makeDpuIntf());

    EXPECT_EQ(handle(sensor, getResponse({}, {}, NSM_ERROR)), NSM_ERROR);
}

TEST_F(DeviceModeV2Test, Get_TooShortMessage_Fails)
{
    NsmDPUOperationModeDeviceModeSettingsV2Get sensor("DpuGet", "NSM_Test", 1,
                                                      makeDpuIntf());
    auto response = getResponse({1}, {1});

    EXPECT_NE(sensor.handleResponseMsg(
                  reinterpret_cast<const nsm_msg*>(response.data()), 2),
              NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// DPU operation mode: write
// ---------------------------------------------------------------------------

constexpr auto dpuMode =
    "com.nvidia.DeviceMode.DPUOperationMode.OperationMode.";

TEST_F(DeviceModeV2Test, DpuSet_Success_UpdatesPendingMode)
{
    auto intf = makeDpuIntf();
    NsmDPUOperationModeDeviceModeSettingsV2Set sensor("DpuSet", "NSM_Test", 1,
                                                      intf);
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    auto status =
        set(sensor, &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
            std::string(std::string(dpuMode) + "NIC"), rc);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(intf->pendingMode(), OperationMode::NIC);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(DeviceModeV2Test, DpuSet_BuildsTheModeByte)
{
    EXPECT_EQ(
        NsmDPUOperationModeDeviceModeSettingsV2Set::buildDpuOperationModeData(
            OperationMode::DPU),
        Bytes(DPU_DEVICE_MODE_DATA_SIZE, 0));
    EXPECT_EQ(
        NsmDPUOperationModeDeviceModeSettingsV2Set::buildDpuOperationModeData(
            OperationMode::NIC),
        Bytes{1});
}

TEST_F(DeviceModeV2Test, DpuSet_RejectedInputs)
{
    NsmDPUOperationModeDeviceModeSettingsV2Set sensor("DpuSet", "NSM_Test", 1,
                                                      makeDpuIntf());
    EXPECT_CALL(*device, postPatchIO).Times(0);

    requester::Coroutine rc;
    auto status =
        set(sensor, &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
            uint64_t{1}, rc);
    EXPECT_TRUE(threw || rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_FALSE(sensor.asyncPatchInProgress);

    status = set(sensor,
                 &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
                 std::string("NotAMode"), rc);
    EXPECT_TRUE(threw || rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(DeviceModeV2Test, DpuSet_NotPatchable_IsRejected)
{
    // Bitmap 0: the sub mode cannot be changed
    NsmDPUOperationModeDeviceModeSettingsV2Set sensor("DpuSet", "NSM_Test", 0,
                                                      makeDpuIntf());
    EXPECT_CALL(*device, postPatchIO).Times(0);
    requester::Coroutine rc;

    auto status =
        set(sensor, &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
            std::string(std::string(dpuMode) + "NIC"), rc);

    EXPECT_TRUE(threw || rc.exception());
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(DeviceModeV2Test, DpuSet_WhilePatchInProgress_IsUnavailable)
{
    NsmDPUOperationModeDeviceModeSettingsV2Set sensor("DpuSet", "NSM_Test", 1,
                                                      makeDpuIntf());
    sensor.asyncPatchInProgress = true;
    EXPECT_CALL(*device, postPatchIO).Times(0);
    requester::Coroutine rc;

    auto status =
        set(sensor, &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
            std::string(std::string(dpuMode) + "NIC"), rc);

    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

TEST_F(DeviceModeV2Test, DpuSet_TransportFailureAndDeviceError_AreWriteFailures)
{
    auto intf = makeDpuIntf();
    NsmDPUOperationModeDeviceModeSettingsV2Set sensor("DpuSet", "NSM_Test", 1,
                                                      intf);
    requester::Coroutine rc;

    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(setResponse(), NSM_ERROR));
    auto status =
        set(sensor, &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
            std::string(std::string(dpuMode) + "NIC"), rc);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor.asyncPatchInProgress);

    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(setResponse(NSM_ERROR)));
    status = set(sensor,
                 &NsmDPUOperationModeDeviceModeSettingsV2Set::setPendingMode,
                 std::string(std::string(dpuMode) + "NIC"), rc);
    EXPECT_NE(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_EQ(intf->pendingMode(), OperationMode::DPU);
}

// ---------------------------------------------------------------------------
// PCIe device mode: read
// ---------------------------------------------------------------------------

TEST_F(DeviceModeV2Test, PcieGet_PublishesEverySubMode)
{
    auto intf = makePcieIntf();
    NsmPCIeDeviceModeDeviceModeSettingsV2Get sensor("PcieGet", "NSM_Test", 7,
                                                    intf);
    // multi socket, controlled EW traffic, bifurcation
    Bytes current{2, 1, 3, 0, 0, 0, 0, 0};
    Bytes pending{4, 0, 5, 0, 0, 0, 0, 0};

    EXPECT_EQ(handle(sensor, getResponse(current, pending)), NSM_SW_SUCCESS);

    EXPECT_EQ(intf->PCIeMultiSocketServer::currentMode(), 2);
    EXPECT_EQ(intf->PCIeMultiSocketServer::pendingMode(), 4);
    EXPECT_EQ(intf->PCIeControlledEWTrafficServer::currentMode(),
              EWTrafficMode::Enabled);
    EXPECT_EQ(intf->PCIeControlledEWTrafficServer::pendingMode(),
              EWTrafficMode::Disabled);
    EXPECT_EQ(intf->PCIeBifurcationServer::currentMode(), 3);
    EXPECT_EQ(intf->PCIeBifurcationServer::pendingMode(), 5);
}

TEST_F(DeviceModeV2Test, PcieGet_NoChangeAndShortPayloadsKeepValues)
{
    auto intf = makePcieIntf();
    intf->PCIeMultiSocketServer::currentMode(9);
    NsmPCIeDeviceModeDeviceModeSettingsV2Get sensor("PcieGet", "NSM_Test", 7,
                                                    intf);

    // Every sub mode "no change"
    EXPECT_EQ(handle(sensor, getResponse({noChange, noChange, noChange},
                                         {noChange, noChange, noChange})),
              NSM_SW_SUCCESS);
    // Payload shorter than the sub mode offset
    EXPECT_EQ(handle(sensor, getResponse({1}, {1})), NSM_SW_SUCCESS);

    EXPECT_EQ(intf->PCIeBifurcationServer::currentMode(), 0);
    EXPECT_EQ(intf->PCIeControlledEWTrafficServer::currentMode(),
              EWTrafficMode::Disabled);
}

TEST_F(DeviceModeV2Test, PcieGet_WithoutInterface_StillSucceeds)
{
    NsmPCIeDeviceModeDeviceModeSettingsV2Get sensor("PcieGet", "NSM_Test", 7,
                                                    nullptr);

    EXPECT_EQ(handle(sensor, getResponse({1, 1, 1}, {1, 1, 1})),
              NSM_SW_SUCCESS);
}

// ---------------------------------------------------------------------------
// PCIe device mode: write
// ---------------------------------------------------------------------------

using ModeEntries = std::vector<std::tuple<std::string, uint32_t>>;

TEST_F(DeviceModeV2Test, PcieSet_BuildsTheModeBytes)
{
    auto data =
        NsmPCIeDeviceModeDeviceModeSettingsV2Set::buildPcieDeviceModeData(1, 2,
                                                                          3);

    ASSERT_EQ(data.size(), static_cast<size_t>(PCIE_DEVICE_MODE_DATA_SIZE));
    EXPECT_EQ(data[SUB_MODE_PCIE_MULTI_SOCKET], 1);
    EXPECT_EQ(data[SUB_MODE_PCIE_CONTROLLED_EW_TRAFFIC], 2);
    EXPECT_EQ(data[SUB_MODE_PCIE_BIFURCATION], 3);
}

TEST_F(DeviceModeV2Test, PcieSet_Success_UpdatesPendingModes)
{
    auto intf = makePcieIntf();
    NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test", 7,
                                                    intf);
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    auto status =
        set(sensor, &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
            ModeEntries{{"PCIeMultiSockets", 2},
                        {"PCIeControlledEWTraffic", 1},
                        {"PCIeBifurcation", 4}},
            rc);

    EXPECT_EQ(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(intf->PCIeMultiSocketServer::pendingMode(), 2);
    EXPECT_EQ(intf->PCIeControlledEWTrafficServer::pendingMode(),
              EWTrafficMode::Enabled);
    EXPECT_EQ(intf->PCIeBifurcationServer::pendingMode(), 4);
    EXPECT_FALSE(sensor.asyncPatchInProgress);
}

TEST_F(DeviceModeV2Test, PcieSet_OnlyTheGivenSubModesChange)
{
    auto intf = makePcieIntf();
    intf->PCIeBifurcationServer::pendingMode(7);
    NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test", 7,
                                                    intf);
    EXPECT_CALL(*device, postPatchIO).WillOnce(mockPostPatchIO(setResponse()));
    requester::Coroutine rc;

    // An unknown key is ignored
    auto status =
        set(sensor, &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
            ModeEntries{{"PCIeMultiSockets", 3}, {"Other", 9}}, rc);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_EQ(intf->PCIeMultiSocketServer::pendingMode(), 3);
    EXPECT_EQ(intf->PCIeBifurcationServer::pendingMode(), 7);
}

TEST_F(DeviceModeV2Test, PcieSet_RejectedInputs)
{
    NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test", 7,
                                                    makePcieIntf());
    EXPECT_CALL(*device, postPatchIO).Times(0);

    for (const AsyncSetOperationValueType& value :
         {AsyncSetOperationValueType{std::string("x")},
          AsyncSetOperationValueType{ModeEntries{}}})
    {
        requester::Coroutine rc;
        auto status = set(
            sensor, &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
            value, rc);
        EXPECT_TRUE(threw || rc.exception());
        EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
        EXPECT_FALSE(sensor.asyncPatchInProgress);
    }
}

TEST_F(DeviceModeV2Test, PcieSet_NotPatchableSubMode_IsRejected)
{
    EXPECT_CALL(*device, postPatchIO).Times(0);

    for (const auto& [key, bitmap] :
         std::vector<std::pair<std::string, uint8_t>>{
             {"PCIeMultiSockets", 0b110},
             {"PCIeControlledEWTraffic", 0b101},
             {"PCIeBifurcation", 0b011}})
    {
        NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test",
                                                        bitmap, makePcieIntf());
        requester::Coroutine rc;
        auto status = set(
            sensor, &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
            ModeEntries{{key, 1}}, rc);

        EXPECT_TRUE(threw || rc.exception()) << key;
        EXPECT_EQ(status, AsyncOperationStatusType::Unavailable) << key;
        EXPECT_FALSE(sensor.asyncPatchInProgress) << key;
    }
}

TEST_F(DeviceModeV2Test, PcieSet_WhilePatchInProgress_IsUnavailable)
{
    NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test", 7,
                                                    makePcieIntf());
    sensor.asyncPatchInProgress = true;
    EXPECT_CALL(*device, postPatchIO).Times(0);
    requester::Coroutine rc;

    auto status =
        set(sensor, &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
            ModeEntries{{"PCIeMultiSockets", 1}}, rc);

    EXPECT_EQ(rc.data(), NSM_SW_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

TEST_F(DeviceModeV2Test,
       PcieSet_TransportFailureAndDeviceError_AreWriteFailures)
{
    NsmPCIeDeviceModeDeviceModeSettingsV2Set sensor("PcieSet", "NSM_Test", 7,
                                                    makePcieIntf());
    requester::Coroutine rc;

    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(setResponse(), NSM_ERROR));
    auto status =
        set(sensor, &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
            ModeEntries{{"PCIeBifurcation", 1}}, rc);
    EXPECT_EQ(rc.data(), NSM_ERROR);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor.asyncPatchInProgress);

    EXPECT_CALL(*device, postPatchIO)
        .WillOnce(mockPostPatchIO(setResponse(NSM_ERROR)));
    status = set(sensor,
                 &NsmPCIeDeviceModeDeviceModeSettingsV2Set::setPendingModes,
                 ModeEntries{{"PCIeBifurcation", 1}}, rc);
    EXPECT_NE(rc.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ---------------------------------------------------------------------------
// Feature gate: DeviceModesSupported
// ---------------------------------------------------------------------------

namespace
{

template <typename T>
size_t countOf(const std::shared_ptr<MockNsmDevice>& device)
{
    size_t count = 0;
    for (const auto& sensor : device->roundRobinSensors)
    {
        count += std::dynamic_pointer_cast<T>(sensor) != nullptr;
    }
    for (const auto& sensor : device->staticSensors)
    {
        count += std::dynamic_pointer_cast<T>(sensor) != nullptr;
    }
    return count;
}

features::DeviceContext nicDevice(const std::shared_ptr<MockNsmDevice>& device,
                                  const uuid_t& uuid, const std::string& name)
{
    features::DeviceContext dev;
    dev.name = name;
    dev.kind = "cx_nic";
    dev.staticUuid = uuid;
    dev.chassisPath = "/xyz/openbmc_project/inventory/system/chassis/" + name;
    dev.device = device;
    return dev;
}

} // namespace

TEST_F(DeviceModeV2Test, Feature_DpuAndPcieModesSupported_CreateBothPairs)
{
    const std::string name = "CX_Modes" + suffix;
    std::vector<int64_t> modes(27, DEVICE_MODE_NOT_SUPPORTED);
    modes[DEVICE_MODE_DPU_OPERATION_MODE] = 1;
    modes[DEVICE_MODE_PCIE_DEVICE_MODE] = 7;
    dbus::PropertyMap params{{"Name", name}, {"DeviceModesSupported", modes}};

    auto rc = features::createNetworkAdapterFeature(
        mockManager, nicDevice(device, deviceUuid, name), params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmDPUOperationModeDeviceModeSettingsV2Get>(device), 1u);
    EXPECT_EQ(countOf<NsmDPUOperationModeDeviceModeSettingsV2Set>(device), 1u);
    EXPECT_EQ(countOf<NsmPCIeDeviceModeDeviceModeSettingsV2Get>(device), 1u);
    EXPECT_EQ(countOf<NsmPCIeDeviceModeDeviceModeSettingsV2Set>(device), 1u);
}

TEST_F(DeviceModeV2Test, Feature_OnlyDpuModeSupported_CreatesOnlyDpuPair)
{
    const std::string name = "CX_Dpu" + suffix;
    std::vector<int64_t> modes(27, DEVICE_MODE_NOT_SUPPORTED);
    modes[DEVICE_MODE_DPU_OPERATION_MODE] = 0;
    dbus::PropertyMap params{{"Name", name}, {"DeviceModesSupported", modes}};

    auto rc = features::createNetworkAdapterFeature(
        mockManager, nicDevice(device, deviceUuid, name), params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmDPUOperationModeDeviceModeSettingsV2Get>(device), 1u);
    EXPECT_EQ(countOf<NsmPCIeDeviceModeDeviceModeSettingsV2Get>(device), 0u);
}

TEST_F(DeviceModeV2Test, Feature_OnlyPcieModeSupported_CreatesOnlyPciePair)
{
    const std::string name = "CX_Pcie" + suffix;
    std::vector<int64_t> modes(27, DEVICE_MODE_NOT_SUPPORTED);
    modes[DEVICE_MODE_PCIE_DEVICE_MODE] = 3;
    dbus::PropertyMap params{{"Name", name}, {"DeviceModesSupported", modes}};

    auto rc = features::createNetworkAdapterFeature(
        mockManager, nicDevice(device, deviceUuid, name), params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmDPUOperationModeDeviceModeSettingsV2Get>(device), 0u);
    EXPECT_EQ(countOf<NsmPCIeDeviceModeDeviceModeSettingsV2Get>(device), 1u);
}

TEST_F(DeviceModeV2Test, Feature_ShortModesVector_TreatsMissingAsUnsupported)
{
    const std::string name = "CX_Short" + suffix;
    dbus::PropertyMap params{{"Name", name},
                             {"DeviceModesSupported", std::vector<int64_t>{}}};

    auto rc = features::createNetworkAdapterFeature(
        mockManager, nicDevice(device, deviceUuid, name), params);

    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    EXPECT_EQ(countOf<NsmDPUOperationModeDeviceModeSettingsV2Get>(device), 0u);
    EXPECT_EQ(countOf<NsmPCIeDeviceModeDeviceModeSettingsV2Get>(device), 0u);
}
