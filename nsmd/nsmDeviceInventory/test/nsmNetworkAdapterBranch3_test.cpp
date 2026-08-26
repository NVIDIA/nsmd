/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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
 * Unit tests for NsmNetworkAdapterProtectionOptionsMode (single class,
 * NSM Type 5 Device Mode Index 26) and its factory gate in
 * createNSMNetworkAdapter.
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

#include "nsmNetworkAdapter.hpp"

#undef private
#undef protected

namespace nsm
{
requester::Coroutine createNSMNetworkAdapter(SensorManager& manager,
                                             const std::string& interface,
                                             const std::string& objPath);
} // namespace nsm

using namespace nsm;

// ============================================================================
// Helpers
// ============================================================================

static std::vector<uint8_t> makeGetResp(uint16_t bitmask)
{
    uint16_t bitmaskLE = htole16(bitmask);
    uint8_t data[PROTECTION_OPTIONS_MODE_DATA_SIZE];
    memcpy(data, &bitmaskLE, sizeof(bitmaskLE));

    // mode_data[1] in the struct already holds 1 byte; we need
    // (current + pending) - 1 = 2*2 - 1 = 3 extra bytes.
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) +
                                  sizeof(nsm_get_device_mode_settings_v2_resp) +
                                  PROTECTION_OPTIONS_MODE_DATA_SIZE * 2 - 1,
                              0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_get_device_mode_settings_v2_resp(
        0, NSM_SUCCESS, ERR_NULL, data, PROTECTION_OPTIONS_MODE_DATA_SIZE, data,
        PROTECTION_OPTIONS_MODE_DATA_SIZE, msg);
    return resp;
}

static std::vector<uint8_t> makeSetSuccessResp()
{
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    return resp;
}

static std::vector<uint8_t> makeSetErrorResp()
{
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto* msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_device_mode_settings_v2_resp(0, NSM_ERROR, ERR_INVALID_RQD, msg);
    return resp;
}

// ============================================================================
// Fixture
// ============================================================================

struct ProtectionOptionsModeV2Test :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string intf =
        "xyz.openbmc_project.Configuration.NSM_NetworkAdapter";
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string nicPath =
        "/xyz/openbmc_project/inventory/system/chassis/CX_0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    ProtectionOptionsModeV2Test() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~ProtectionOptionsModeV2Test() override
    {
        cleanupDeviceSensors(devices);
    }

    // Create a sensor with its own D-Bus objects at a unique path.
    std::shared_ptr<NsmNetworkAdapterProtectionOptionsMode>
        makeSensor(const std::string& suffix)
    {
        auto& bus = utils::DBusHandler::getBus();
        const std::string path = "/xyz/openbmc_project/test/prot_v2/" + suffix;

        auto assocIntf =
            std::make_shared<AssociationDefinitionsInft>(bus, path.c_str());
        assocIntf->associations(
            {{"network_adapter", "protection_options_mode", nicPath}});

        auto modeIntf =
            std::make_shared<ProtectionOptionsModeIntf>(bus, path.c_str());
        modeIntf->hostFirmwareUpdateRestrictionEnabled(false);
        modeIntf->hostConfigurationChangeRestrictionEnabled(false);
        modeIntf->hostTransceiverFirmwareUpdateRestrictionEnabled(false);
        modeIntf->hostTransceiverConfigurationChangeRestrictionEnabled(false);

        return std::make_shared<NsmNetworkAdapterProtectionOptionsMode>(
            "prot_v2_" + suffix, "NSM_NetworkAdapter", modeIntf, assocIntf);
    }
};

// ============================================================================
// genRequestMsg
// ============================================================================

TEST_F(ProtectionOptionsModeV2Test, GenRequestMsg_EncodeFail_ReturnsNullopt)
{
    auto sensor = makeSensor("genreq_fail");
    // instanceId > NSM_INSTANCE_MAX triggers encode failure
    auto result = sensor->genRequestMsg(12, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(result.has_value());
}

TEST_F(ProtectionOptionsModeV2Test, GenRequestMsg_Success_ReturnsBuffer)
{
    auto sensor = makeSensor("genreq_ok");
    auto result = sensor->genRequestMsg(12, 0);
    ASSERT_TRUE(result.has_value());

    // Decode to verify mode index
    uint32_t decoded_index = 0;
    auto rc = decode_get_device_mode_settings_v2_req(
        reinterpret_cast<const nsm_msg*>(result->data()), result->size(),
        &decoded_index);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(decoded_index,
              static_cast<uint32_t>(DEVICE_MODE_PROTECTION_OPTIONS_MODE));
}

// ============================================================================
// handleResponseMsg — bitmask decoding
// ============================================================================

TEST_F(ProtectionOptionsModeV2Test, HandleResponse_AllZero_AllPropertiesFalse)
{
    auto sensor = makeSensor("resp_zero");
    auto resp = makeGetResp(0x0000);
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp.data()), resp.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostConfigurationChangeRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test,
       HandleResponse_AllFourBits_AllPropertiesTrue)
{
    auto sensor = makeSensor("resp_all");
    auto resp = makeGetResp(0x000F);
    auto rc = sensor->handleResponseMsg(
        reinterpret_cast<const nsm_msg*>(resp.data()), resp.size());

    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostFirmwareUpdateRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostConfigurationChangeRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, HandleResponse_Bit0Only_OnlyFwTrue)
{
    auto sensor = makeSensor("resp_bit0");
    auto resp = makeGetResp(0x0001);
    sensor->handleResponseMsg(reinterpret_cast<const nsm_msg*>(resp.data()),
                              resp.size());

    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostConfigurationChangeRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, HandleResponse_ReservedBits_Ignored)
{
    auto sensor = makeSensor("resp_rsvd");
    // bits 4-15 set; bits 0-3 = 0b0101 (fw=1, cfg=0, txFw=1, txCfg=0)
    auto resp = makeGetResp(0xFF05);
    sensor->handleResponseMsg(reinterpret_cast<const nsm_msg*>(resp.data()),
                              resp.size());

    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostConfigurationChangeRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

// ============================================================================
// setFlag — per-property async handler
// ============================================================================

TEST_F(ProtectionOptionsModeV2Test, SetFlag_BadValueType_ReturnsError)
{
    auto sensor = makeSensor("set_bad_type");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Pass a string instead of bool — wrong variant type
    AsyncSetOperationValueType badValue = std::string("true");
    auto coro = sensor->setFlag(badValue, &status, device, 0);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_PatchInProgress_ThrowsUnavailable)
{
    auto sensor = makeSensor("set_in_prog");
    sensor->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);

    EXPECT_THROW_COROUTINE(
        sensor->setFlag(value, &status, device, 0),
        sdbusplus::error::xyz::openbmc_project::common::Unavailable);
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_BadBitIndex_ReturnsError)
{
    auto sensor = makeSensor("set_bad_bit");
    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);

    auto coro = sensor->setFlag(value, &status, device, 99);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_PostPatchIOFails_FlagCleared)
{
    auto sensor = makeSensor("set_pio_fail");

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 0);

    EXPECT_FALSE(sensor->asyncPatchInProgress);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    // Property must NOT be updated on failure
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostFirmwareUpdateRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_NSMErrorCC_PropertiesUnchanged)
{
    auto sensor = makeSensor("set_err_cc");

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetErrorResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 0);

    EXPECT_FALSE(sensor->asyncPatchInProgress);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostFirmwareUpdateRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_Bit0_Success_FwPropertyUpdated)
{
    auto sensor = makeSensor("set_bit0_ok");

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 0);

    EXPECT_FALSE(sensor->asyncPatchInProgress);
    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostFirmwareUpdateRestrictionEnabled());
    // Other properties unchanged
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostConfigurationChangeRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_Bit1_Success_CfgPropertyUpdated)
{
    auto sensor = makeSensor("set_bit1_ok");

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 1);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostFirmwareUpdateRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_Bit2_Success_TxFwPropertyUpdated)
{
    auto sensor = makeSensor("set_bit2_ok");

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 2);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_Bit3_Success_TxCfgPropertyUpdated)
{
    auto sensor = makeSensor("set_bit3_ok");

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 3);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

TEST_F(ProtectionOptionsModeV2Test, SetFlag_PreservesOtherBits_ReadModifyWrite)
{
    auto sensor = makeSensor("set_rmw");
    // Pre-set bits 1 and 3 to true
    sensor->protectionOptionsModeIntf
        ->hostConfigurationChangeRestrictionEnabled(true);
    sensor->protectionOptionsModeIntf
        ->hostTransceiverConfigurationChangeRestrictionEnabled(true);

    EXPECT_CALL(*device, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    // Set bit 0 (fw) to true; bits 1 and 3 should be preserved in NSM call
    AsyncSetOperationValueType value = bool(true);
    sensor->setFlag(value, &status, device, 0);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostFirmwareUpdateRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostConfigurationChangeRestrictionEnabled());
    EXPECT_FALSE(sensor->protectionOptionsModeIntf
                     ->hostTransceiverFirmwareUpdateRestrictionEnabled());
    EXPECT_TRUE(sensor->protectionOptionsModeIntf
                    ->hostTransceiverConfigurationChangeRestrictionEnabled());
}

// ============================================================================
// Factory gate: DeviceModesSupported[26]
// ============================================================================

static size_t
    countProtectionModeSensors(const std::shared_ptr<MockNsmDevice>& dev)
{
    size_t count = 0;
    for (const auto& s : dev->roundRobinSensors)
    {
        if (std::dynamic_pointer_cast<NsmNetworkAdapterProtectionOptionsMode>(
                s))
        {
            ++count;
        }
    }
    return count;
}

TEST_F(ProtectionOptionsModeV2Test, Factory_ModeSupported_SensorCreated)
{
    const std::string testPath = "/xyz/test/prot_v2_factory/mode_supported";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("CX_0");
    pm["UUID"] = deviceUuid;
    pm["Type"] = std::string("NSM_NetworkAdapter");
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/");

    // DeviceModesSupported: index 26 = 0 (supported, patchability 0)
    std::vector<int64_t> modes(27, DEVICE_MODE_NOT_SUPPORTED);
    modes[DEVICE_MODE_PROTECTION_OPTIONS_MODE] = 0;
    pm["DeviceModesSupported"] = modes;

    ASSERT_EQ(countProtectionModeSensors(device), 0u);
    createNSMNetworkAdapter(mockManager, intf, testPath);
    EXPECT_EQ(countProtectionModeSensors(device), 1u);
}

TEST_F(ProtectionOptionsModeV2Test, Factory_ModeNotSupported_NoSensorCreated)
{
    const std::string testPath = "/xyz/test/prot_v2_factory/mode_not_supported";
    auto& pm = utils::MockDbusAsync::propertyMap(testPath, intf);
    pm["Name"] = std::string("CX_1");
    pm["UUID"] = deviceUuid;
    pm["Type"] = std::string("NSM_NetworkAdapter");
    pm["InventoryObjPath"] =
        std::string("/xyz/openbmc_project/inventory/system/chassis/");

    // DeviceModesSupported: index 26 = -1 (not supported)
    std::vector<int64_t> modes(27, DEVICE_MODE_NOT_SUPPORTED);
    pm["DeviceModesSupported"] = modes;

    const size_t before = countProtectionModeSensors(device);
    createNSMNetworkAdapter(mockManager, intf, testPath);
    EXPECT_EQ(countProtectionModeSensors(device), before);
}
