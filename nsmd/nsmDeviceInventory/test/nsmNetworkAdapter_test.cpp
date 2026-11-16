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

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "device-configuration.h"

#define private public
#define protected public

#include "features/creators.hpp"
#include "nsmNetworkAdapter.hpp"

using namespace nsm;

auto bus = sdbusplus::bus::new_default();

TEST(NsmNetworkAdapterDI, Constructor)
{
    std::string name = "NetworkAdapter0";
    std::string type = "NSM_NetworkAdapter";
    std::string inventoryObjPath = "/xyz/openbmc_project/inventory/system/";

    std::vector<utils::Association> associations;
    associations.push_back({"parent_chassis", "network_adapter",
                            "/xyz/openbmc_project/inventory/system"});

    NsmNetworkAdapterDI netAdapter(bus, name, associations, type,
                                   inventoryObjPath);

    EXPECT_EQ(netAdapter.getName(), name);
    EXPECT_EQ(netAdapter.getType(), type);
    EXPECT_NE(netAdapter.associationDefIntf, nullptr);
    EXPECT_NE(netAdapter.pcieDeviceIntf, nullptr);
    EXPECT_NE(netAdapter.networkInterfaceIntf, nullptr);
}

TEST(NsmDeviceProtectionOptions, Constructor)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    EXPECT_EQ(protOptions.getName(), name);
    EXPECT_EQ(protOptions.getType(), type);
    EXPECT_EQ(protOptions.objPath, path);
    EXPECT_NE(protOptions.protectionIntf, nullptr);
    EXPECT_FALSE(protOptions.asyncPatchInProgress);
}

TEST(NsmDeviceProtectionOptions, GenRequestMsg)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    eid_t eid = 10;
    uint8_t instanceId = 5;

    auto request = protOptions.genRequestMsg(eid, instanceId);
    EXPECT_TRUE(request.has_value());
    EXPECT_EQ(request->size(), sizeof(nsm_msg_hdr) + sizeof(nsm_common_req));
}

// genRequestMsg failure: instanceId > NSM_INSTANCE_MAX → encode fails →
// if (rc != NSM_SW_SUCCESS) TRUE → return nullopt.
// Covers lines 89,91-92 in nsmNetworkAdapter.cpp.
TEST(NsmDeviceProtectionOptions, GenRequestMsg_InvalidInstanceId_ReturnsNullopt)
{
    std::string name = "DeviceProtectionBadId";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device_bad";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    auto request = protOptions.genRequestMsg(1, NSM_INSTANCE_MAX + 1);
    EXPECT_FALSE(request.has_value());
}

TEST(NsmDeviceProtectionOptions, HandleResponseMsg)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    // Create mock response
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint8_t protection_mode = 1; // Some protection mode

    uint8_t rc = encode_get_protection_options_resp(0, cc, reason_code,
                                                    protection_mode, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
}

TEST(NsmDeviceProtectionOptions, HandleResponseMsgError)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    // Create mock response with error
    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    uint8_t cc = NSM_ERROR;
    uint16_t reason_code = ERR_INVALID_RQD;
    uint8_t protection_mode = 0;

    uint8_t rc = encode_get_protection_options_resp(0, cc, reason_code,
                                                    protection_mode, response);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);

    rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_ERROR);
}

// Test convertNsmdToDbusProtectionMode() default case:
// protection_mode 0xFF is not in {0,1,2,3} → returns ProtectionOption::Unknown
TEST(NsmDeviceProtectionOptions, HandleResponseMsg_UnknownProtectionMode)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device_unknown";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());

    // Use protection_mode = 0xFF (not in the known set) → default case
    uint8_t protection_mode = 0xFF;
    uint8_t rc = encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                                    protection_mode, response);
    ASSERT_EQ(rc, NSM_SW_SUCCESS);

    rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    // Interface should have been updated with ProtectionOption::Unknown
    EXPECT_EQ(protOptions.protectionIntf->protectionLevel(),
              ProtectionOption::Unknown);
}

// Decode fail test: 7-byte buffer is too short for
// decode_get_protection_options_resp (needs 11 bytes minimum).
TEST(NsmDeviceProtectionOptions, HandleResponseMsg_DecodeFail_ReturnsError)
{
    std::string name = "DeviceProtection";
    std::string type = "NSM_DeviceProtection";
    const char* path = "/xyz/openbmc_project/inventory/test/device_df";

    NsmDeviceProtectionOptions protOptions(bus, path, name, type);

    std::vector<uint8_t> responseMsg(sizeof(nsm_msg_hdr) + 2, 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    uint8_t rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}

// =============================================================================
// setProtectionOptions – coroutine tests
// =============================================================================

static std::vector<uint8_t> makeSetProtectionSuccessResp()
{
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_protection_options_resp(0, NSM_SUCCESS, ERR_NULL, msg);
    return resp;
}

static std::vector<uint8_t> makeSetProtectionErrorResp()
{
    std::vector<uint8_t> resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto msg = reinterpret_cast<nsm_msg*>(resp.data());
    encode_set_protection_options_resp(0, NSM_ERROR, ERR_INVALID_RQD, msg);
    return resp;
}

struct NsmProtectionOptionsSetTest :
    public testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t gpuUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> gpu;

    NsmProtectionOptionsSetTest() : SensorManagerTest(devices)
    {
        gpu = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(gpuUuid));
        EXPECT_NE(gpu, nullptr);
    }

    ~NsmProtectionOptionsSetTest()
    {
        cleanupDeviceSensors(devices);
    }

    std::shared_ptr<NsmDeviceProtectionOptions>
        makeProtectionSensor(const char* path)
    {
        return std::make_shared<NsmDeviceProtectionOptions>(
            utils::DBusHandler::getBus(), path, "DeviceProtection",
            "NSM_DeviceProtection");
    }
};

// asyncPatchInProgress guard: co_return early with Unavailable //

TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_AlreadyInProgress)
{
    auto sensor = makeProtectionSensor("/test/prot/in_progress");
    sensor->asyncPatchInProgress = true;

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Unavailable);
}

// Value is bool (not string): !protectionOptionStr path → InvalidArgument
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_NonStringValue)
{
    auto sensor = makeProtectionSensor("/test/prot/nonstr");

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = true;
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// Invalid string causes convertProtectionOptionFromString to throw → catch //
// block
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_InvalidString)
{
    auto sensor = makeProtectionSensor("/test/prot/invalid_str");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("not_a_valid_protection_option");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// "Unknown" string maps to ProtectionOption::Unknown → default switch case
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_UnknownValue)
{
    auto sensor = makeProtectionSensor("/test/prot/unknown");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).Times(0);

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.DeviceProtection.ProtectionOption.Unknown");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::InvalidArgument);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// NoProtection case: successful postPatchIO + decode → Success
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_NoProtection_Success)
{
    auto sensor = makeProtectionSensor("/test/prot/noprot");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetProtectionSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// PreventAll case: successful postPatchIO + decode → Success
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_PreventAll_Success)
{
    auto sensor = makeProtectionSensor("/test/prot/prevent_all");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetProtectionSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value =
        std::string("com.nvidia.DeviceProtection.ProtectionOption.PreventAll");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// PreventHostFirmwareUpdates case: successful → Success
TEST_F(NsmProtectionOptionsSetTest,
       SetProtectionOptions_PreventFwUpdates_Success)
{
    auto sensor = makeProtectionSensor("/test/prot/prevent_fw");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetProtectionSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.PreventHostFirmwareUpdates");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// PreventHostConfigurations case: successful → Success
TEST_F(NsmProtectionOptionsSetTest,
       SetProtectionOptions_PreventHostConfig_Success)
{
    auto sensor = makeProtectionSensor("/test/prot/prevent_cfg");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetProtectionSuccessResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.PreventHostConfigurations");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::Success);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// postPatchIO returns error code → WriteFailure
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_PostPatchIOFails)
{
    auto sensor = makeProtectionSensor("/test/prot/pio_fail");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetProtectionSuccessResp(), NSM_ERROR));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// Decode shows error CC → WriteFailure
TEST_F(NsmProtectionOptionsSetTest, SetProtectionOptions_DecodeFailure)
{
    auto sensor = makeProtectionSensor("/test/prot/decode_fail");

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _))
        .WillOnce(mockPostPatchIO(makeSetProtectionErrorResp()));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// setProtectionOptions: postPatchIO succeeds,
// decode_set_protection_options_resp returns NSM_SW_SUCCESS with cc=NSM_ERROR
// (buffer exactly nsm_msg_hdr + nsm_common_non_success_resp so
// decode_reason_code_and_cc succeeds) → nsmNetworkAdapter.cpp L245 `if
// (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` FALSE via cc!=NSM_SUCCESS branch.
TEST_F(NsmProtectionOptionsSetTest,
       SetProtectionOptions_DecodeSuccessNonZeroCC_ElseBranch)
{
    auto sensor = makeProtectionSensor("/test/prot/decode_cc_error");

    // Buffer exactly sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp)
    // with completion_code = NSM_ERROR (byte index nsm_msg_hdr+1)
    std::vector<uint8_t> resp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;

    EXPECT_CALL(*gpu, postPatchIO(_, _, _, _)).WillOnce(mockPostPatchIO(resp));

    AsyncOperationStatusType status = AsyncOperationStatusType::Success;
    AsyncSetOperationValueType value = std::string(
        "com.nvidia.DeviceProtection.ProtectionOption.NoProtection");
    auto coro = sensor->setProtectionOptions(value, &status, gpu);

    EXPECT_EQ(status, AsyncOperationStatusType::WriteFailure);
    EXPECT_FALSE(sensor->asyncPatchInProgress);
}

// =============================================================================
// createNetworkAdapterFeature coverage
// =============================================================================

struct NsmNetworkAdapterFactoryTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const uuid_t deviceUuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> device;

    NsmNetworkAdapterFactoryTest() : SensorManagerTest(devices)
    {
        device = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(deviceUuid));
        EXPECT_NE(device, nullptr);
    }

    ~NsmNetworkAdapterFactoryTest()
    {
        cleanupDeviceSensors(devices);
    }

    nsm::features::DeviceContext makeDev()
    {
        nsm::features::DeviceContext dev;
        dev.name = "HGX_CX_0";
        dev.kind = "cx_nic";
        dev.staticUuid = deviceUuid;
        dev.platformPrefix = "HGX_";
        dev.chassisPath =
            "/xyz/openbmc_project/inventory/system/chassis/HGX_CX_0";
        dev.device = device;
        return dev;
    }

    dbus::PropertyMap basicParams = {
        {"Name", std::string("NetworkAdapter_0")},
    };
};

// Name present → NsmNetworkAdapterDI created and added
TEST_F(NsmNetworkAdapterFactoryTest, Feature_AllParams_SensorCreated)
{
    const size_t before = device->deviceSensors.size();
    auto rc = nsm::features::createNetworkAdapterFeature(mockManager, makeDev(),
                                                         basicParams);
    EXPECT_EQ(rc.data(), NSM_SUCCESS);
    ASSERT_GT(device->deviceSensors.size(), before);
    EXPECT_NE(std::dynamic_pointer_cast<NsmNetworkAdapterDI>(
                  device->deviceSensors[before]),
              nullptr);
}

// Missing Name → name="" → D-Bus path ends with "/" → sdbusplus throws →
// no sensor added
TEST_F(NsmNetworkAdapterFactoryTest, Feature_MissingName_NoSensor)
{
    const size_t before = device->deviceSensors.size();
    EXPECT_THROW_COROUTINE(nsm::features::createNetworkAdapterFeature(
                               mockManager, makeDev(), dbus::PropertyMap{}),
                           std::exception);
    EXPECT_EQ(before, device->deviceSensors.size());
}

// ============================================================================
// convertNsmdToDbusProtectionMode: cover remaining switch cases (0, 2, 3)
// ============================================================================

// PROTECTION_NONE (0) → ProtectionOption::NoProtection
TEST(NsmDeviceProtectionOptions,
     HandleResponseMsg_ProtectionNone_SetsNoProtection)
{
    auto protOptions = NsmDeviceProtectionOptions(
        bus, "/xyz/openbmc_project/inventory/test/dev_none", "DevProt_none",
        "NSM_DeviceProtection");

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 PROTECTION_NONE, response),
              NSM_SW_SUCCESS);

    auto rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(protOptions.protectionIntf->protectionLevel(),
              ProtectionOption::NoProtection);
}

// PROTECTION_PREVENT_FW_UPDATE (2) →
// ProtectionOption::PreventHostFirmwareUpdates
TEST(NsmDeviceProtectionOptions,
     HandleResponseMsg_PreventFwUpdate_SetsPreventFwUpdate)
{
    auto protOptions = NsmDeviceProtectionOptions(
        bus, "/xyz/openbmc_project/inventory/test/dev_prevfw", "DevProt_prevfw",
        "NSM_DeviceProtection");

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 PROTECTION_PREVENT_FW_UPDATE,
                                                 response),
              NSM_SW_SUCCESS);

    auto rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(protOptions.protectionIntf->protectionLevel(),
              ProtectionOption::PreventHostFirmwareUpdates);
}

// PROTECTION_PREVENT_CONFIG (3) → ProtectionOption::PreventHostConfigurations
TEST(NsmDeviceProtectionOptions,
     HandleResponseMsg_PreventConfig_SetsPreventConfig)
{
    auto protOptions = NsmDeviceProtectionOptions(
        bus, "/xyz/openbmc_project/inventory/test/dev_prevcfg",
        "DevProt_prevcfg", "NSM_DeviceProtection");

    std::vector<uint8_t> responseMsg(
        sizeof(nsm_msg_hdr) + sizeof(nsm_get_protection_options_resp), 0);
    auto response = reinterpret_cast<nsm_msg*>(responseMsg.data());
    ASSERT_EQ(encode_get_protection_options_resp(0, NSM_SUCCESS, ERR_NULL,
                                                 PROTECTION_PREVENT_CONFIG,
                                                 response),
              NSM_SW_SUCCESS);

    auto rc = protOptions.handleResponseMsg(response, responseMsg.size());
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(protOptions.protectionIntf->protectionLevel(),
              ProtectionOption::PreventHostConfigurations);
}

// handleResponseMsg: rc==NSM_SW_SUCCESS, cc==NSM_ERROR →
// `if (rc==NSM_SW_SUCCESS && cc==NSM_SUCCESS)` false branch →
// protectionIntf NOT updated.
TEST(NsmDeviceProtectionOptions,
     HandleResponseMsg_DecodeSuccessNonZeroCC_DoesNotUpdate)
{
    NsmDeviceProtectionOptions protOptions(
        bus, "/xyz/openbmc_project/inventory/test/cc_branch", "DevProt_cc",
        "NSM_DeviceProtection");

    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    auto msg = reinterpret_cast<const nsm_msg*>(buf.data());

    auto rc = protOptions.handleResponseMsg(msg, buf.size());
    EXPECT_NE(rc, NSM_SUCCESS);
}
