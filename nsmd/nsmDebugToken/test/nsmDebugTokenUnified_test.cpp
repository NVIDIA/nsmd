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

#include <fcntl.h>
using namespace ::testing;

#include "debug-token.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"

#define private public
#define protected public

#include "nsmDebugTokenUnified.hpp"

using namespace nsm;

struct NsmDebugTokenUnifiedTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "Unified_DebugToken";
    const uuid_t uuid = "ABCDEF12-3456-7890-ABCD-EF1234567890";
    const std::string debugTokenDeviceType = "TestDeviceType";

    NsmDeviceTable devices;

    NsmDebugTokenUnifiedTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmDebugTokenUnifiedTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);
    EXPECT_NE(debugToken, nullptr);
    EXPECT_EQ(debugToken->uuid, uuid);
    EXPECT_EQ(debugToken->getName(), name);
    EXPECT_EQ(debugToken->getType(), "NSM_DebugTokenUnified");
}

TEST_F(NsmDebugTokenUnifiedTest, testConstructorWithDifferentNames)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_1", uuid, debugTokenDeviceType);
    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_2", uuid, debugTokenDeviceType);
    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->getName(), "Unified_DebugToken_1");
    EXPECT_EQ(debugToken2->getName(), "Unified_DebugToken_2");
}

TEST_F(NsmDebugTokenUnifiedTest, testConstructorWithDifferentUUIDs)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t uuid1 = "11111111-1111-1111-1111-111111111111";
    uuid_t uuid2 = "22222222-2222-2222-2222-222222222222";
    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_UUID1", uuid1, debugTokenDeviceType);
    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_UUID2", uuid2, debugTokenDeviceType);
    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, uuid1);
    EXPECT_EQ(debugToken2->uuid, uuid2);
    EXPECT_NE(debugToken1->uuid, debugToken2->uuid);
}

TEST_F(NsmDebugTokenUnifiedTest, testNsmObjectInheritance)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);
    NsmObject* nsmObj = debugToken.get();
    EXPECT_NE(nsmObj, nullptr);
    EXPECT_EQ(nsmObj->getName(), name);
    EXPECT_EQ(nsmObj->getType(), "NSM_DebugTokenUnified");
}

TEST_F(NsmDebugTokenUnifiedTest, testInstallationChunkSizeInitial)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);
    EXPECT_EQ(debugToken->installationChunkSize, 0);
}

TEST_F(NsmDebugTokenUnifiedTest, testMultipleInstancesWithSameUUID)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_Same1", uuid, debugTokenDeviceType);
    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_Same2", uuid, debugTokenDeviceType);
    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, debugToken2->uuid);
    EXPECT_NE(debugToken1->getName(), debugToken2->getName());
}

TEST_F(NsmDebugTokenUnifiedTest, testTypeIsConsistent)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken1 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_A", uuid, debugTokenDeviceType);
    auto debugToken2 = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, "Unified_DebugToken_B", uuid, debugTokenDeviceType);
    EXPECT_EQ(debugToken1->getType(), debugToken2->getType());
    EXPECT_EQ(debugToken1->getType(), "NSM_DebugTokenUnified");
}

TEST_F(NsmDebugTokenUnifiedTest, testUUIDStorage)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "FEDCBA09-8765-4321-FEDC-BA0987654321";
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, testUuid, debugTokenDeviceType);
    EXPECT_EQ(debugToken->uuid, testUuid);
}

TEST_F(NsmDebugTokenUnifiedTest, testNameStorage)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "CustomUnifiedDebugToken";
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, testName, uuid, debugTokenDeviceType);
    EXPECT_EQ(debugToken->getName(), testName);
}

TEST_F(NsmDebugTokenUnifiedTest, testDebugTokenActionIntfInheritance)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);
    DebugTokenActionIntf* actionIntf = debugToken.get();
    EXPECT_NE(actionIntf, nullptr);
}

TEST_F(NsmDebugTokenUnifiedTest, testDebugTokenStatusIntfInheritance)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);
    DebugTokenStatusIntf* statusIntf = debugToken.get();
    EXPECT_NE(statusIntf, nullptr);
}

TEST_F(NsmDebugTokenUnifiedTest, testInstallationChunkSizeModification)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
        bus, name, uuid, debugTokenDeviceType);
    debugToken->installationChunkSize = 4096;
    EXPECT_EQ(debugToken->installationChunkSize, 4096);
    debugToken->installationChunkSize = 8192;
    EXPECT_EQ(debugToken->installationChunkSize, 8192);
}

// ============================================================================
// Tests using MockNsmDevice for coroutine-based handlers
// ============================================================================

struct NsmDebugTokenUnifiedWithDeviceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "Unified_DebugToken_Dev";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string debugTokenDeviceType = "GPU";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenUnifiedObject> debugToken;

    NsmDebugTokenUnifiedWithDeviceTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenUnifiedWithDeviceTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        debugToken = std::make_shared<NsmDebugTokenUnifiedObject>(
            bus, name, uuid, debugTokenDeviceType);
        EXPECT_NE(debugToken, nullptr);
    }

    Response createEraseTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_erase_token_resp(0, completionCode, reasonCode,
                                              msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createInstallTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                        uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_install_token_resp(0, completionCode, reasonCode,
                                                msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createQueryDeviceIdsResponse(const std::vector<uint8_t>& deviceId,
                                          uint8_t completionCode = NSM_SUCCESS,
                                          uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_query_device_ids_resp) - 1 +
                              deviceId.size(),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_query_device_ids_resp(0, completionCode,
                                                   reasonCode, deviceId.data(),
                                                   deviceId.size(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createDeviceCapabilitiesV2Response(
        uint8_t timestampGen = 1, uint32_t maxInputBufferSize = 4096,
        uint8_t completionCode = NSM_SUCCESS, uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) +
                              sizeof(nsm_get_device_capabilities_v2_resp) - 1 +
                              NSM_GET_DEVICE_CAPABILITIES_V2_DATA_SIZE,
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_get_device_capabilities_v2_resp(
            0, completionCode, reasonCode, timestampGen, maxInputBufferSize,
            msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createQueryTokenResponse(const std::vector<uint8_t>& tlvPayload,
                                      uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_resp) -
                              1 + tlvPayload.size(),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_query_token_resp(0, completionCode, reasonCode,
                                              tlvPayload.data(),
                                              tlvPayload.size(), msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    auto callEraseTokenAsync(uint32_t tokenTypeValue)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = debugToken
                      ->eraseTokenAsyncHandler(tokenTypeValue, statusInterface,
                                               valueInterface)
                      .data();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callInstallTokenAsync(
        std::shared_ptr<NsmDebugTokenUnifiedObject::TokenInstallationInfo> info)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = debugToken
                      ->installTokenAsyncHandler(info, statusInterface,
                                                 valueInterface)
                      .data();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    std::shared_ptr<NsmDebugTokenUnifiedObject::TokenInstallationInfo>
        createTokenInstallationInfo(const std::vector<uint8_t>& tokenData)
    {
        char tempPath[] = "/tmp/unified_test_XXXXXX";
        int fd = mkstemp(tempPath);
        EXPECT_NE(fd, -1);
        if (fd != -1)
        {
            ssize_t written = write(fd, tokenData.data(), tokenData.size());
            EXPECT_EQ(written, static_cast<ssize_t>(tokenData.size()));
            lseek(fd, 0, SEEK_SET);
            unlink(tempPath);
        }
        return std::make_shared<
            NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
            fd, tokenData.size());
    }
};

// ============================================================================
// eraseToken() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, eraseTokenEraseAllDispatch)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypesEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;
    auto objPath = debugToken->eraseToken(EraseTypeEnum::EraseAll,
                                          TokenTypesEnum::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, eraseTokenEraseAllAndRatchetDispatch)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse()))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypesEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;
    auto objPath = debugToken->eraseToken(
        EraseTypeEnum::EraseAllAndRatchetCounterIncreased,
        TokenTypesEnum::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// eraseTokenAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       eraseTokenAsyncHandlerSuccessResponse)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    auto [rc, statusIntf,
          valueIntf] = callEraseTokenAsync(NSM_DEBUG_TOKEN_ERASE_ALL_TOKENS);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       eraseTokenAsyncHandlerPostPatchIOFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto [rc, statusIntf,
          valueIntf] = callEraseTokenAsync(NSM_DEBUG_TOKEN_ERASE_ALL_TOKENS);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
    auto val = valueIntf->value();
    auto* errorTuple = std::get_if<std::tuple<uint16_t, std::string>>(&val);
    ASSERT_NE(errorTuple, nullptr);
    EXPECT_NE(std::get<0>(*errorTuple), 0);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       eraseTokenAsyncHandlerUnsupportedCommand)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto [rc, statusIntf,
          valueIntf] = callEraseTokenAsync(NSM_DEBUG_TOKEN_ERASE_ALL_TOKENS);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, eraseTokenAsyncHandlerDeviceErrorCC)
{
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_erase_token_resp(0, NSM_ERROR, 0x1234, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResp](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), errorResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto [rc, statusIntf,
          valueIntf] = callEraseTokenAsync(NSM_DEBUG_TOKEN_ERASE_ALL_TOKENS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// createInstallTokenRequest() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, createInstallTokenRequestInvalidFd)
{
    auto info =
        std::make_shared<NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
            -1, 100);
    auto result = debugToken->createInstallTokenRequest(info);
    EXPECT_FALSE(result.has_value());
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, createInstallTokenRequestValidData)
{
    debugToken->installationChunkSize = 256;
    std::vector<uint8_t> tokenData(100, 0xAB);
    auto info = createTokenInstallationInfo(tokenData);
    auto result = debugToken->createInstallTokenRequest(info);
    EXPECT_TRUE(result.has_value());
    EXPECT_GT(result->size(), 0u);
    EXPECT_EQ(info->offset, 100u);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, createInstallTokenRequestChunkedRead)
{
    debugToken->installationChunkSize = 50;
    std::vector<uint8_t> tokenData(200, 0xCD);
    auto info = createTokenInstallationInfo(tokenData);
    auto result1 = debugToken->createInstallTokenRequest(info);
    EXPECT_TRUE(result1.has_value());
    EXPECT_EQ(info->offset, 50u);
    auto result2 = debugToken->createInstallTokenRequest(info);
    EXPECT_TRUE(result2.has_value());
    EXPECT_EQ(info->offset, 100u);
}

// ============================================================================
// installTokenAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenAsyncHandlerSuccessSingleChunk)
{
    debugToken->installationChunkSize = 512;
    std::vector<uint8_t> tokenData(100, 0xEF);
    auto info = createTokenInstallationInfo(tokenData);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenAsyncHandlerPostPatchIOFailure)
{
    debugToken->installationChunkSize = 512;
    std::vector<uint8_t> tokenData(100, 0xAA);
    auto info = createTokenInstallationInfo(tokenData);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
    auto val = valueIntf->value();
    auto* errorTuple = std::get_if<std::tuple<uint16_t, std::string>>(&val);
    ASSERT_NE(errorTuple, nullptr);
    EXPECT_NE(std::get<0>(*errorTuple), 0);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenAsyncHandlerUnsupportedCommand)
{
    debugToken->installationChunkSize = 512;
    std::vector<uint8_t> tokenData(100, 0xBB);
    auto info = createTokenInstallationInfo(tokenData);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenAsyncHandlerDeviceRejectsToken)
{
    debugToken->installationChunkSize = 512;
    std::vector<uint8_t> tokenData(100, 0xCC);
    auto info = createTokenInstallationInfo(tokenData);
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_install_token_resp(0, NSM_ERROR, 0x5678, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResp](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), errorResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenAsyncHandlerInvalidCreateRequest)
{
    debugToken->installationChunkSize = 512;
    auto info =
        std::make_shared<NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
            -1, 100);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(rc, NSM_SW_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
    auto val = valueIntf->value();
    auto* errorTuple = std::get_if<std::tuple<uint16_t, std::string>>(&val);
    ASSERT_NE(errorTuple, nullptr);
    EXPECT_NE(std::get<0>(*errorTuple), 0);
}

// ============================================================================
// installToken() (D-Bus method) tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, installTokenValidFd)
{
    debugToken->installationChunkSize = 512;
    std::vector<uint8_t> tokenData(64, 0xDD);
    char tempPath[] = "/tmp/install_test_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    if (write(fd, tokenData.data(), tokenData.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    sdbusplus::message::unix_fd unixFd(fd);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillRepeatedly(
            mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)));
    auto objPath = debugToken->installToken(unixFd);
    EXPECT_FALSE(objPath.str.empty());
    close(fd);
}

// ============================================================================
// deviceIdHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, deviceIdHandlerSuccess)
{
    std::vector<uint8_t> deviceIdBytes = {0xAB, 0xCD, 0xEF, 0x01};
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(mockSensorIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    auto rc = debugToken->deviceIdHandler(mockDevice).data();
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xABCDEF01");
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, deviceIdHandlerHexFormatting)
{
    std::vector<uint8_t> deviceIdBytes = {0x00, 0x0A, 0xFF};
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(mockSensorIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    auto rc = debugToken->deviceIdHandler(mockDevice).data();
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0x000AFF");
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, deviceIdHandlerSensorIOFailure)
{
    EXPECT_CALL(*mockDevice, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    auto rc = debugToken->deviceIdHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, deviceIdHandlerDecodeFailure)
{
    Response badResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                     0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, 0x1111, nullptr, 0, badMsg);
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce([badResp](eid_t, Request&,
                            std::shared_ptr<const nsm_msg>& responseMsg,
                            size_t& responseLen, bool) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), badResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto rc = debugToken->deviceIdHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// deviceCapabilitiesHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, deviceCapabilitiesHandlerSuccess)
{
    uint32_t maxInputBufferSize = 8192;
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(mockSensorIO(
            createDeviceCapabilitiesV2Response(1, maxInputBufferSize)));
    auto rc = debugToken->deviceCapabilitiesHandler(mockDevice).data();
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->installationChunkSize,
              NSM_DEBUG_TOKEN_INSTALL_CHUNK_SIZE(maxInputBufferSize));
    EXPECT_GT(debugToken->installationChunkSize, 0u);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       deviceCapabilitiesHandlerSensorIOFailure)
{
    EXPECT_CALL(*mockDevice, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    auto rc = debugToken->deviceCapabilitiesHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->installationChunkSize, 0u);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       deviceCapabilitiesHandlerDecodeFailure)
{
    Response badResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                     0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    encode_nsm_get_device_capabilities_v2_resp(0, NSM_ERROR, 0x2222, 0, 0,
                                               badMsg);
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce([badResp](eid_t, Request&,
                            std::shared_ptr<const nsm_msg>& responseMsg,
                            size_t& responseLen, bool) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), badResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto rc = debugToken->deviceCapabilitiesHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->installationChunkSize, 0u);
}

// ============================================================================
// queryTokenHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, queryTokenHandlerSensorIOFailure)
{
    EXPECT_CALL(*mockDevice, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, queryTokenHandlerDecodeFailure)
{
    Response badResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                     0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    encode_nsm_query_token_resp(0, NSM_ERROR, 0x3333, nullptr, 0, badMsg);
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce([badResp](eid_t, Request&,
                            std::shared_ptr<const nsm_msg>& responseMsg,
                            size_t& responseLen, bool) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), badResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// update() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, updateCallsDeviceIdWhenEmpty)
{
    std::vector<uint8_t> deviceIdBytes = {0xDE, 0xAD};
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(mockSensorIO(createQueryDeviceIdsResponse(deviceIdBytes)))
        .WillOnce(mockSensorIO(createDeviceCapabilitiesV2Response(1, 4096)))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));
    debugToken->update(mockDevice).data();
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xDEAD");
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, updateSkipsDeviceIdWhenAlreadySet)
{
    debugToken->tokenDeviceID("0xABCD");
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(mockSensorIO(createDeviceCapabilitiesV2Response(1, 4096)))
        .WillRepeatedly(mockSensorIO(NSM_ERROR));
    debugToken->update(mockDevice).data();
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xABCD");
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       updateSkipsCapabilitiesWhenAlreadySet)
{
    debugToken->installationChunkSize = 1024;
    debugToken->tokenDeviceID("0x1234");
    EXPECT_CALL(*mockDevice, sensorIO).WillOnce(mockSensorIO(NSM_ERROR));
    debugToken->update(mockDevice).data();
    EXPECT_EQ(debugToken->installationChunkSize, 1024u);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, updateAllCallsFail)
{
    EXPECT_CALL(*mockDevice, sensorIO).WillRepeatedly(mockSensorIO(NSM_ERROR));
    auto rc = debugToken->update(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

// ============================================================================
// installTokenDirect() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenDirectChunkSizeNotInitialized)
{
    char tempPath[] = "/tmp/direct_test_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0x11);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    auto rc = debugToken
                  ->installTokenDirect(fd, data.size(), errorCode, errorMessage)
                  .data();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_NE(errorCode, 0);
    EXPECT_FALSE(errorMessage.empty());
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, installTokenDirectSuccess)
{
    debugToken->installationChunkSize = 512;
    char tempPath[] = "/tmp/direct_succ_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0x22);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    auto rc = debugToken
                  ->installTokenDirect(fd, data.size(), errorCode, errorMessage)
                  .data();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(errorCode, 0);
    EXPECT_EQ(errorMessage, "Success");
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, installTokenDirectPostPatchIOFailure)
{
    debugToken->installationChunkSize = 512;
    char tempPath[] = "/tmp/direct_fail_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0x33);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto rc = debugToken
                  ->installTokenDirect(fd, data.size(), errorCode, errorMessage)
                  .data();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_NE(errorCode, 0);
}

// ============================================================================
// deviceTypeStr storage test
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, deviceTypeStrStored)
{
    EXPECT_EQ(debugToken->deviceTypeStr, debugTokenDeviceType);
}

// ============================================================================
// eraseToken() TokenType tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, eraseTokenTokenTypeInvalid)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypesEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;
    // CRCS is not a valid GPU token type → tokenTypeToUint32 returns 0 → throws
    EXPECT_THROW(
        debugToken->eraseToken(EraseTypeEnum::TokenType, TokenTypesEnum::CRCS),
        Common::Error::InvalidArgument);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, eraseTokenTokenTypeValid)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypesEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;
    // DebugFirmwareUnlock maps to type=1 for GPU device → valid non-zero value
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    auto objPath = debugToken->eraseToken(EraseTypeEnum::TokenType,
                                          TokenTypesEnum::DebugFirmwareUnlock);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// Additional installTokenAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, installTokenAsyncHandlerMultiChunk)
{
    // Two chunks: chunkSize=50, totalSize=100 → two postPatchIO calls needed
    debugToken->installationChunkSize = 50;
    std::vector<uint8_t> tokenData(100, 0xCD);
    auto info = createTokenInstallationInfo(tokenData);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// Additional installTokenDirect() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenDirectCreateRequestFailure)
{
    debugToken->installationChunkSize = 512;
    uint16_t errorCode = 0;
    std::string errorMessage;
    // fd = -1 causes createInstallTokenRequest to return nullopt
    auto rc =
        debugToken->installTokenDirect(-1, 100, errorCode, errorMessage).data();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_NE(errorCode, 0);
    EXPECT_FALSE(errorMessage.empty());
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, installTokenDirectNonSuccessCC)
{
    debugToken->installationChunkSize = 512;
    char tempPath[] = "/tmp/direct_cc_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0x55);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_install_token_resp(0, NSM_ERROR, 0xABCD, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResp](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), errorResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto rc = debugToken
                  ->installTokenDirect(fd, data.size(), errorCode, errorMessage)
                  .data();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_NE(errorCode, 0);
    close(fd);
}

// ============================================================================
// Additional queryTokenHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, queryTokenHandlerTLVDecodeException)
{
    // Valid NSM response (cc=SUCCESS) but garbage TLV payload (< header size)
    std::vector<uint8_t> garbageTlv = {0xFF, 0xFE, 0xFD};
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(garbageTlv, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       queryTokenHandlerInstallationStatusMissing)
{
    // TLV with ProcessingStatus but no InstallationStatus → exception
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       queryTokenHandlerProcessingStatusMissingInstallZero)
{
    // TLV with InstallationStatus=0 only; no ProcessingStatus
    // installStatus=0 → catch sets procStatus=0 and continues → SUCCESS
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(0));
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       queryTokenHandlerProcessingStatusMissingInstallNonZero)
{
    // TLV with InstallationStatus=1 only; no ProcessingStatus
    // installStatus != 0 → co_return NSM_SW_ERROR
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(1));
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, queryTokenHandlerOddTokenTypesSize)
{
    // TLV with TokenTypeSubtypeList having 3 uint32 values (odd count)
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(0));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    enc.add(debug_token::types::TokenTypeSubtypeList,
            std::vector<uint32_t>{1, 2, 3});
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       queryTokenHandlerInstalledButNoTokenTypes)
{
    // TLV with InstallationStatus=1 and ProcessingStatus=0, no
    // TokenTypeSubtypeList TokenTypeSubtypeList get throws → tokenTypesSubtypes
    // stays empty installStatus != 0 && size == 0 → co_return NSM_SW_ERROR //

    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(1));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, queryTokenHandlerValidWithTokenTypes)
{
    // Full valid TLV: installed=1, processing=1, one GPU type=1 pair
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(1));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(1));
    enc.add(debug_token::types::TokenTypeSubtypeList,
            std::vector<uint32_t>{1, 0}); // GPU DebugFirmwareUnlock, subtype=0
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->queryTokenHandler(mockDevice).data();
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
}

// ============================================================================
// update() - success path (both already set, queryToken succeeds)
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, updateBothAlreadySetSuccess)
{
    // Both tokenDeviceID and installationChunkSize already set
    // Only queryTokenHandler called → succeeds → co_return NSM_SUCCESS //

    debugToken->tokenDeviceID("0xABCD");
    debugToken->installationChunkSize = 1024;
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(0));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    enc.add(debug_token::types::TokenTypeSubtypeList, std::vector<uint32_t>{});
    auto payload = enc.encode();
    EXPECT_CALL(*mockDevice, sensorIO)
        .WillOnce(
            mockSensorIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    auto rc = debugToken->update(mockDevice).data();
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xABCD");
    EXPECT_EQ(debugToken->installationChunkSize, 1024u);
}

// ============================================================================
// eraseTokenAsyncHandler() decode failure (lines 109-114)
// ============================================================================

// postPatchIO returns NSM_SW_SUCCESS but with a response that triggers
// NSM_SW_ERROR_LENGTH: cc (payload[1]) != NSM_SUCCESS but length is wrong
// → decode_reason_code_and_cc returns NSM_SW_ERROR_LENGTH
// → decodeRc != NSM_SW_SUCCESS → lines 109-114 covered → WriteFailure
TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       eraseTokenAsyncHandlerDecodeFails_WriteFailure)
{
    // payload[1] = NSM_ERROR (cc != SUCCESS) and length !=
    // sizeof(non_success_resp) → decode returns NSM_SW_ERROR_LENGTH
    Response badResp(sizeof(nsm_msg_hdr) + 2, 0);
    badResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc at payload[1]
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(badResp));
    auto [rc, statusIntf,
          valueIntf] = callEraseTokenAsync(NSM_DEBUG_TOKEN_ERASE_ALL_TOKENS);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// createInstallTokenRequest() read() failure (lines 149-152)
// ============================================================================

// Use a valid fd integer that has been closed → read() returns EBADF < 0
// → createInstallTokenRequest returns nullopt
TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       createInstallTokenRequest_ReadFails_Nullopt)
{
    debugToken->installationChunkSize = 256;
    // Open a file, capture its fd, then close it so read() fails
    int fd = open("/dev/null", O_RDONLY);
    ASSERT_GE(fd, 0);
    close(fd); // fd is now closed; the integer is still >=0
    auto info =
        std::make_shared<NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
            fd, 100);
    auto result = debugToken->createInstallTokenRequest(info);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// installTokenAsyncHandler() decode failure (lines 215-220)
// ============================================================================

// postPatchIO returns NSM_SW_SUCCESS with a response where cc != SUCCESS but
// length is wrong → decode_nsm_install_token_resp returns NSM_SW_ERROR_LENGTH
// → lines 215-220 covered → InternalFailure
TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installTokenAsyncHandlerDecodeFails_InternalFailure)
{
    debugToken->installationChunkSize = 512;
    std::vector<uint8_t> tokenData(64, 0xEE);
    auto info = createTokenInstallationInfo(tokenData);
    // payload[1] = NSM_ERROR, length != sizeof(non_success_resp) → length error
    Response badResp(sizeof(nsm_msg_hdr) + 2, 0);
    badResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // cc at payload[1]
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(badResp));
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// installToken() D-Bus method: dup(fd) failure (lines 302-304)
// ============================================================================

// Passing an invalid file descriptor (-1) causes dup(-1) to fail →
// throws Common::Error::InternalFailure
TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installToken_InvalidFd_ThrowsInternalFailure)
{
    sdbusplus::message::unix_fd badFd(-1);
    EXPECT_THROW(debugToken->installToken(badFd),
                 Common::Error::InternalFailure);
}

// ============================================================================
// installTokenDirect() decode failure (lines 397-407)
// ============================================================================

TEST_F(NsmDebugTokenUnifiedWithDeviceTest, installTokenDirectDecodeFailure)
{
    debugToken->installationChunkSize = 512;
    char tempPath[] = "/tmp/direct_dec_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0x44);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    // payload[1] = cc = NSM_ERROR, length 8 != 10 →
    // decode_reason_code_and_cc returns NSM_SW_ERROR_LENGTH → lines 397-407
    Response badResp(sizeof(nsm_msg_hdr) + 2, 0);
    badResp[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(badResp));
    auto rc = debugToken
                  ->installTokenDirect(fd, data.size(), errorCode, errorMessage)
                  .data();
    EXPECT_NE(rc, NSM_SW_SUCCESS);
    EXPECT_NE(errorCode, 0);
    close(fd);
}

// ============================================================================
// installToken() D-Bus method: lseek failure (lines 313-318)
// ============================================================================

// Passing a pipe fd causes lseek(SEEK_SET) to fail with ESPIPE →
// throws Common::Error::InternalFailure (after successful dup)
TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       installToken_PipeFd_LseekFailsThrowsInternalFailure)
{
    int pipefd[2];
    ASSERT_EQ(pipe(pipefd), 0);
    // pipefd[0] is the read end; lseek on a pipe returns -1 (ESPIPE)
    sdbusplus::message::unix_fd readEnd(pipefd[0]);
    EXPECT_THROW(debugToken->installToken(readEnd),
                 Common::Error::InternalFailure);
    close(pipefd[0]);
    close(pipefd[1]);
}

// ============================================================================
// deviceIdHandler – second operand of || at L702:
//   rc==NSM_SW_SUCCESS but cc==NSM_ERROR
// 9-byte buffer: decode_reason_code_and_cc returns NSM_SW_SUCCESS with
// cc=NSM_ERROR → decode_nsm_query_device_ids_resp returns NSM_SW_SUCCESS,
// cc=NSM_ERROR → second operand of (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
// is TRUE.
// ============================================================================
TEST_F(NsmDebugTokenUnifiedWithDeviceTest,
       deviceIdHandlerDecodeSuccessNonZeroCC_ReturnsError)
{
    std::vector<uint8_t> buf(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR; // completion_code = NSM_ERROR
    EXPECT_CALL(*mockDevice, sensorIO).WillOnce(mockSensorIO(buf));
    auto rc = debugToken->deviceIdHandler(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}
