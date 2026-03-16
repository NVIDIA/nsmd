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

#include "debug-token.h"

#define private public
#define protected public

#include "nsmDebugTokenNIC.hpp"

using namespace nsm;
using DebugToken = sdbusplus::server::com::nvidia::DebugToken;

struct NsmDebugTokenNICTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NIC_DebugToken";
    const uuid_t uuid = "12345678-1234-1234-1234-123456789012";

    NsmDeviceTable devices;

    NsmDebugTokenNICTest() : SensorManagerTest(devices) {}
};

TEST_F(NsmDebugTokenNICTest, testConstructor)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name, uuid);
    EXPECT_NE(debugToken, nullptr);
    EXPECT_EQ(debugToken->uuid, uuid);
    EXPECT_EQ(debugToken->getName(), name);
    EXPECT_EQ(debugToken->getType(), "NSM_DebugTokenNIC");
}

TEST_F(NsmDebugTokenNICTest, testConstructorWithDifferentNames)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken1 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_1", uuid);
    auto debugToken2 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_2", uuid);
    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->getName(), "NIC_DebugToken_1");
    EXPECT_EQ(debugToken2->getName(), "NIC_DebugToken_2");
}

TEST_F(NsmDebugTokenNICTest, testConstructorWithDifferentUUIDs)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t uuid1 = "11111111-1111-1111-1111-111111111111";
    uuid_t uuid2 = "22222222-2222-2222-2222-222222222222";
    auto debugToken1 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_UUID1", uuid1);
    auto debugToken2 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_UUID2", uuid2);
    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, uuid1);
    EXPECT_EQ(debugToken2->uuid, uuid2);
    EXPECT_NE(debugToken1->uuid, debugToken2->uuid);
}

TEST_F(NsmDebugTokenNICTest, testNsmObjectInheritance)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name, uuid);
    NsmObject* nsmObj = debugToken.get();
    EXPECT_NE(nsmObj, nullptr);
    EXPECT_EQ(nsmObj->getName(), name);
    EXPECT_EQ(nsmObj->getType(), "NSM_DebugTokenNIC");
}

TEST_F(NsmDebugTokenNICTest, testSuccessReasonCode)
{
    EXPECT_EQ(successReasonCode, 0);
}

TEST_F(NsmDebugTokenNICTest, testTokenAlreadyActiveReasonCode)
{
    EXPECT_EQ(tokenAlreadyActiveReasonCode, 1);
}

TEST_F(NsmDebugTokenNICTest, testMultipleInstancesWithSameUUID)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken1 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_Same1", uuid);
    auto debugToken2 = std::make_shared<NsmDebugTokenNICObject>(
        bus, "NIC_DebugToken_Same2", uuid);
    EXPECT_NE(debugToken1, nullptr);
    EXPECT_NE(debugToken2, nullptr);
    EXPECT_EQ(debugToken1->uuid, debugToken2->uuid);
    EXPECT_NE(debugToken1->getName(), debugToken2->getName());
}

TEST_F(NsmDebugTokenNICTest, testTypeIsConsistent)
{
    auto& bus = utils::DBusHandler::getBus();
    auto debugToken1 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_A", uuid);
    auto debugToken2 =
        std::make_shared<NsmDebugTokenNICObject>(bus, "NIC_DebugToken_B", uuid);
    EXPECT_EQ(debugToken1->getType(), debugToken2->getType());
    EXPECT_EQ(debugToken1->getType(), "NSM_DebugTokenNIC");
}

TEST_F(NsmDebugTokenNICTest, testUUIDStorage)
{
    auto& bus = utils::DBusHandler::getBus();
    uuid_t testUuid = "ABCDEF12-3456-7890-ABCD-EF1234567890";
    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name,
                                                               testUuid);
    EXPECT_EQ(debugToken->uuid, testUuid);
}

TEST_F(NsmDebugTokenNICTest, testNameStorage)
{
    auto& bus = utils::DBusHandler::getBus();
    std::string testName = "CustomNICDebugToken";
    auto debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, testName,
                                                               uuid);
    EXPECT_EQ(debugToken->getName(), testName);
}

// ============================================================================
// Tests using MockNsmDevice for coroutine-based handlers
// ============================================================================

struct NsmDebugTokenNICWithDeviceTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NIC_DebugToken_Dev";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenNICObject> debugToken;

    NsmDebugTokenNICWithDeviceTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenNICWithDeviceTest()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name, uuid);
        EXPECT_NE(debugToken, nullptr);
    }

    Response createDisableTokensResponse(uint8_t completionCode = NSM_SUCCESS,
                                         uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_disable_tokens_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_disable_tokens_resp(0, completionCode, reasonCode,
                                                 msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createProvideTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                        uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_provide_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_provide_token_resp(0, completionCode, reasonCode,
                                                msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response
        createQueryTokenParametersResponse(uint8_t completionCode = NSM_SUCCESS,
                                           uint16_t reasonCode = 0)
    {
        struct nsm_debug_token_request tokenReq = {};
        tokenReq.token_request_version = 1;
        tokenReq.token_request_size = sizeof(nsm_debug_token_request);
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_query_token_parameters_resp(
            0, completionCode, reasonCode, &tokenReq, msg);
        EXPECT_EQ(rc, NSM_SW_SUCCESS);
        return response;
    }

    Response createQueryTokenStatusResponse(
        nsm_debug_token_status status = NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
        nsm_debug_token_status_additional_info addInfo =
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
        nsm_debug_token_type tokenType = NSM_DEBUG_TOKEN_TYPE_CRCS,
        uint32_t timeLeft = 0, uint8_t completionCode = NSM_SUCCESS,
        uint16_t reasonCode = 0)
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_resp), 0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        auto rc = encode_nsm_query_token_status_resp(
            0, completionCode, reasonCode, status, addInfo, tokenType, timeLeft,
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

    auto callDisableTokensAsync(std::shared_ptr<Request> request)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = debugToken
                      ->disableTokensAsyncHandler(request, statusInterface,
                                                  valueInterface)
                      .data();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callGetRequestAsync(std::shared_ptr<Request> request)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = debugToken
                      ->getRequestAsyncHandler(request, statusInterface,
                                               valueInterface)
                      .data();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callGetStatusAsync(std::shared_ptr<Request> request)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = debugToken
                      ->getStatusAsyncHandler(request, statusInterface,
                                              valueInterface)
                      .data();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    auto callInstallTokenAsync(std::shared_ptr<Request> request)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        auto rc = debugToken
                      ->installTokenAsyncHandler(request, statusInterface,
                                                 valueInterface)
                      .data();
        return std::make_tuple(rc, statusInterface, valueInterface);
    }

    std::shared_ptr<Request> makeDisableTokensRequest()
    {
        auto request = std::make_shared<Request>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_disable_tokens_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
        encode_nsm_disable_tokens_req(0, requestMsg);
        return request;
    }

    std::shared_ptr<Request> makeQueryTokenParametersRequest(
        nsm_debug_token_opcode opcode = NSM_DEBUG_TOKEN_OPCODE_CRCS)
    {
        auto request = std::make_shared<Request>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_parameters_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
        encode_nsm_query_token_parameters_req(0, opcode, requestMsg);
        return request;
    }

    std::shared_ptr<Request> makeQueryTokenStatusRequest(
        nsm_debug_token_type tokenType = NSM_DEBUG_TOKEN_TYPE_CRCS)
    {
        auto request = std::make_shared<Request>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_query_token_status_req));
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
        encode_nsm_query_token_status_req(0, tokenType, requestMsg);
        return request;
    }

    std::shared_ptr<Request>
        makeProvideTokenRequest(const std::vector<uint8_t>& tokenData)
    {
        auto request = std::make_shared<Request>(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_req_v2) + tokenData.size());
        auto requestMsg = reinterpret_cast<struct nsm_msg*>(request->data());
        encode_nsm_provide_token_req(0, tokenData.data(), tokenData.size(),
                                     requestMsg);
        return request;
    }
};

// ============================================================================
// disableTokens() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, disableTokensSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse()));
    auto objPath = debugToken->disableTokens();
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// disableTokensAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, disableTokensAsyncHandlerSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 0)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       disableTokensAsyncHandlerPostPatchIOFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       disableTokensAsyncHandlerUnsupportedCommand)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       disableTokensAsyncHandlerNonZeroReasonCode)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 42)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// getRequest() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestCRCSOpcode)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestCRDTOpcode)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestInvalidOpcode)
{
    EXPECT_THROW(
        debugToken->getRequest(static_cast<DebugToken::TokenOpcodes>(999)),
        std::invalid_argument);
}

// ============================================================================
// getRequestAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestAsyncHandlerSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestAsyncHandlerPostPatchIOFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestAsyncHandlerUnsupportedCommand)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getRequestAsyncHandlerTokenAlreadyActive)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse(
            NSM_SUCCESS, tokenAlreadyActiveReasonCode)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// getStatus() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusFRC)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::FRC);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusCRCS)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusCRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusDebugFirmware)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::DebugFirmware);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusInvalidTokenType)
{
    EXPECT_THROW(
        debugToken->getStatus(static_cast<DebugToken::TokenTypes>(999)),
        std::invalid_argument);
}

// ============================================================================
// getStatusAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusAsyncHandlerSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusAsyncHandlerPostPatchIOFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusAsyncHandlerUnsupportedCommand)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusAsyncHandlerAllTokenStatuses)
{
    // Test DEBUG_SESSION_ACTIVE status
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_TYPE_FRC, 3600)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_FRC);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusAsyncHandlerDebugSessionEnded)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ENDED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION,
            NSM_DEBUG_TOKEN_TYPE_CRDT, 0)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_CRDT);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, getStatusAsyncHandlerOperationFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED,
            NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE, 0)));
    auto request =
        makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// installToken() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenValidData)
{
    std::vector<uint8_t> tokenData(100, 0xAB);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(tokenData);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenEmptyData)
{
    std::vector<uint8_t> emptyData;
    EXPECT_THROW(
        debugToken->installToken(emptyData),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenOversizedData)
{
    std::vector<uint8_t> oversizedData(NSM_DEBUG_TOKEN_DATA_MAX_SIZE + 1, 0xFF);
    EXPECT_THROW(
        debugToken->installToken(oversizedData),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenMaxSizeData)
{
    std::vector<uint8_t> maxData(NSM_DEBUG_TOKEN_DATA_MAX_SIZE, 0xCC);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(maxData);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenSingleByteData)
{
    std::vector<uint8_t> singleByte = {0x42};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(singleByte);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// installTokenAsyncHandler() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenAsyncHandlerSuccess)
{
    std::vector<uint8_t> tokenData(50, 0xDD);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(NSM_SUCCESS, 0)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       installTokenAsyncHandlerPostPatchIOFailure)
{
    std::vector<uint8_t> tokenData(50, 0xEE);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_ERROR);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       installTokenAsyncHandlerUnsupportedCommand)
{
    std::vector<uint8_t> tokenData(50, 0xFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, installTokenAsyncHandlerTokenNotAccepted)
{
    std::vector<uint8_t> tokenData(50, 0x11);
    // cc \!= NSM_SUCCESS but decode succeeds
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_provide_token_resp(0, NSM_ERROR, 0x9999, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResp](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t& responseLen) -> requester::Coroutine {
        // Provide enough bytes for decode to extract cc
        responseLen = errorResp.size();
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), errorResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    // When cc \!= NSM_SUCCESS, should report InvalidArgument
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InvalidArgument);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       installTokenAsyncHandlerTokenAlreadyActive)
{
    std::vector<uint8_t> tokenData(50, 0x22);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(
            NSM_SUCCESS, tokenAlreadyActiveReasonCode)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICWithDeviceTest,
       installTokenAsyncHandlerNonZeroReasonCode)
{
    std::vector<uint8_t> tokenData(50, 0x33);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(NSM_SUCCESS, 42)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// update() tests
// ============================================================================

TEST_F(NsmDebugTokenNICWithDeviceTest, updateSuccess)
{
    std::vector<uint8_t> deviceIdBytes = {0xDE, 0xAD, 0xBE, 0xEF};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    auto rc = debugToken->update(mockDevice).data();
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xDEADBEEF");
}

TEST_F(NsmDebugTokenNICWithDeviceTest, updatePostPatchIOFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto rc = debugToken->update(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, updateDecodeFailure)
{
    Response badResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp),
                     0);
    auto badMsg = reinterpret_cast<nsm_msg*>(badResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, 0x4444, nullptr, 0, badMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([badResp](eid_t, Request&,
                            std::shared_ptr<const nsm_msg>& responseMsg,
                            size_t& responseLen) -> requester::Coroutine {
        responseLen = sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp);
        responseMsg = std::shared_ptr<const nsm_msg>(
            reinterpret_cast<const nsm_msg*>(malloc(responseLen)),
            [](const nsm_msg* ptr) { free((void*)ptr); });
        memcpy((uint8_t*)responseMsg.get(), badResp.data(), responseLen);
        co_return NSM_SW_SUCCESS;
    });
    auto rc = debugToken->update(mockDevice).data();
    EXPECT_NE(rc, NSM_SUCCESS);
}

TEST_F(NsmDebugTokenNICWithDeviceTest, updateDeviceIdHexFormatting)
{
    std::vector<uint8_t> deviceIdBytes = {0x00, 0x0F, 0xAA, 0x55};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    auto rc = debugToken->update(mockDevice).data();
    EXPECT_EQ(rc, NSM_SUCCESS);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0x000FAA55");
}
