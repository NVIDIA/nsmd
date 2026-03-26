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
 * Branch coverage for nsmDeviceInventory directory:
 * - NsmDeviceProtectionOptions: genRequestMsg encode fail, handleResponseMsg
 *   all protection modes (None, PreventAll, PreventFwUpdate, PreventConfig,
 *   Unknown/default), decode fail
 * - NsmDeviceProtectionOptions: setProtectionOptions asyncPatchInProgress
 * guard, invalid arg type, all ProtectionOption switch branches, exception path
 * - NsmNetworkAdapterDI: constructor with associations
 */

#include "test/commonMock.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

using namespace ::testing;

#define private public
#define protected public

#include "libnsm/device-configuration.h"

#include "nsmDeviceInventory/nsmNetworkAdapter.hpp"

#undef private
#undef protected

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmDeviceInventoryBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;

    NsmDeviceInventoryBranchTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDeviceInventoryBranchTest()
    {
        cleanupDeviceSensors(devices);
    }

    Response createGetProtectionOptionsResponse(uint8_t protectionMode,
                                                uint8_t cc = NSM_SUCCESS,
                                                uint16_t reasonCode = 0)
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        encode_get_protection_options_resp(0, cc, reasonCode, protectionMode,
                                           msg);
        return response;
    }

    Response createSetProtectionOptionsResponse(uint8_t cc = NSM_SUCCESS,
                                                uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        encode_set_protection_options_resp(0, cc, reasonCode, msg);
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
// NsmDeviceProtectionOptions: genRequestMsg success
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_genRequestMsg_Success)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_gen", "prot_gen",
        "NSM_NetworkAdapter");

    auto result = sensor->genRequestMsg(0, 0);
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// NsmDeviceProtectionOptions: handleResponseMsg all protection modes
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_handleResp_NoProtection)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_none", "prot_none",
        "NSM_NetworkAdapter");

    auto resp = createGetProtectionOptionsResponse(PROTECTION_NONE);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->protectionIntf->protectionLevel(),
              ProtectionOption::NoProtection);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_handleResp_PreventAll)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_all", "prot_all",
        "NSM_NetworkAdapter");

    auto resp = createGetProtectionOptionsResponse(
        PROTECTION_PREVENT_FW_UPDATE_AND_CONFIG);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->protectionIntf->protectionLevel(),
              ProtectionOption::PreventAll);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_handleResp_PreventFwUpdate)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_fw", "prot_fw",
        "NSM_NetworkAdapter");

    auto resp =
        createGetProtectionOptionsResponse(PROTECTION_PREVENT_FW_UPDATE);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->protectionIntf->protectionLevel(),
              ProtectionOption::PreventHostFirmwareUpdates);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_handleResp_PreventConfig)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_cfg", "prot_cfg",
        "NSM_NetworkAdapter");

    auto resp = createGetProtectionOptionsResponse(PROTECTION_PREVENT_CONFIG);
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->protectionIntf->protectionLevel(),
              ProtectionOption::PreventHostConfigurations);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_handleResp_Unknown)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_unk", "prot_unk",
        "NSM_NetworkAdapter");

    auto resp = createGetProtectionOptionsResponse(0xFF); // Unknown mode
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(sensor->protectionIntf->protectionLevel(),
              ProtectionOption::Unknown);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_handleResp_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_decf", "prot_decf",
        "NSM_NetworkAdapter");

    auto resp = createDecodeFail();
    auto msg = reinterpret_cast<const nsm_msg*>(resp.data());
    auto rc = sensor->handleResponseMsg(msg, resp.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions asyncPatchInProgress
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest,
       DevProtection_setProtOpts_AlreadyInProgress)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_busy", "prot_busy",
        "NSM_NetworkAdapter");
    sensor->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");
    sensor->setProtectionOptions(value, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions invalid arg type
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_InvalidArgType)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_invarg", "prot_invarg",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = double(42.0);
    sensor->setProtectionOptions(value, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions bad string throws
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_BadString)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_badstr", "prot_badstr",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string("invalid_enum_value");
    sensor->setProtectionOptions(value, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions all valid modes
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_NoProtection)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_setnone", "prot_setnone",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createSetProtectionOptionsResponse()));
    sensor->setProtectionOptions(value, &status, mockDevice);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_PreventAll)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_setall", "prot_setall",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.DeviceProtection.ProtectionOption.PreventAll");

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createSetProtectionOptionsResponse()));
    sensor->setProtectionOptions(value, &status, mockDevice);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_PreventFw)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_setfw", "prot_setfw",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.PreventHostFirmwareUpdates");

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createSetProtectionOptionsResponse()));
    sensor->setProtectionOptions(value, &status, mockDevice);
}

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_PreventConfig)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_setcfg", "prot_setcfg",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.PreventHostConfigurations");

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createSetProtectionOptionsResponse()));
    sensor->setProtectionOptions(value, &status, mockDevice);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions postPatchIO fail
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_PostPatchFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_setppf", "prot_setppf",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");

    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    sensor->setProtectionOptions(value, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmDeviceProtectionOptions: setProtectionOptions decode fail
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, DevProtection_setProtOpts_DecodeFail)
{
    auto& bus = utils::DBusHandler::getBus();
    auto sensor = std::make_shared<NsmDeviceProtectionOptions>(
        bus, "/xyz/openbmc_project/test/prot_setdecf", "prot_setdecf",
        "NSM_NetworkAdapter");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    sensor->setProtectionOptions(value, &status, mockDevice);
    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// NsmNetworkAdapterDI: constructor with associations
// ============================================================================

TEST_F(NsmDeviceInventoryBranchTest, NetworkAdapterDI_Constructor)
{
    auto& bus = utils::DBusHandler::getBus();
    std::vector<utils::Association> associations;
    associations.push_back(
        {"chassis", "all_adapters", "/xyz/openbmc_project/test/chassis"});

    auto adapter = std::make_shared<NsmNetworkAdapterDI>(
        bus, "NIC_branch", associations, "NSM_NetworkAdapter",
        "/xyz/openbmc_project/inventory/test/");
    EXPECT_NE(adapter, nullptr);
}
