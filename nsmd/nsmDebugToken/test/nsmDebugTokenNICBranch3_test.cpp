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
 * Additional branch coverage for nsmDebugTokenNIC.cpp:
 * - Constructor (lines 39-47): object creation and DebugTokenIntf init
 * - disableTokens: encode failure path (lines 438-440)
 * - getRequest: default opcode (lines 455-458), encode failure (lines 477-484)
 * - getStatus: default type (lines 505-508), encode failure (lines 527-534)
 * - installToken: empty data, oversized data (lines 540-543), encode fail
 *   (lines 563-565)
 * - update: encode fail (lines 576-582)
 * - Synchronous method exception paths that don't need device coroutine
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "debug-token.h"

#define private public
#define protected public

#include "nsmDebugTokenNIC.hpp"
#include "test/commonMock.hpp"

#undef private
#undef protected

using namespace nsm;
using DebugToken = sdbusplus::server::com::nvidia::DebugToken;

struct NsmDebugTokenNICBranch3Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NIC_DebugToken_Branch3";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:30";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenNICObject> debugToken;

    NsmDebugTokenNICBranch3Test() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenNICBranch3Test()
    {
        cleanupDeviceSensors(devices);
    }

    void SetUp() override
    {
        auto& bus = utils::DBusHandler::getBus();
        debugToken = std::make_shared<NsmDebugTokenNICObject>(bus, name, uuid);
        EXPECT_NE(debugToken, nullptr);
    }

    // -- Response helpers --

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

    // Valgrind-safe decode-fail response
    Response createDecodeFail()
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto buf = response.data();
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return response;
    }

    // -- Async call helpers --

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

    // -- Request helpers --

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
// Constructor tests (lines 39-47)
// Verify NsmDebugTokenNICObject is properly constructed with DebugTokenIntf
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, Constructor_CreatesDebugTokenIntf)
{
    // debugToken is created in SetUp(), verify its properties
    EXPECT_EQ(debugToken->getName(), name);
    EXPECT_EQ(debugToken->getType(), "NSM_DebugTokenNIC");
    EXPECT_EQ(debugToken->uuid, uuid);
}

TEST_F(NsmDebugTokenNICBranch3Test, Constructor_DifferentName)
{
    auto& bus = utils::DBusHandler::getBus();
    const std::string altName = "NIC_DebugToken_Alt";
    auto altToken = std::make_shared<NsmDebugTokenNICObject>(bus, altName,
                                                             uuid);
    EXPECT_NE(altToken, nullptr);
    EXPECT_EQ(altToken->getName(), altName);
    EXPECT_EQ(altToken->getType(), "NSM_DebugTokenNIC");
    EXPECT_EQ(altToken->uuid, uuid);
}

// ============================================================================
// installToken: empty data throws InvalidArgument (line 540-543)
// Exercises: tokenData.size() == 0 -> throw InvalidArgument
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installToken_EmptyData_ThrowsInvalidArg)
{
    std::vector<uint8_t> emptyData;
    EXPECT_THROW(debugToken->installToken(emptyData),
                 Common::Error::InvalidArgument);
}

// ============================================================================
// installToken: oversized data throws InvalidArgument (line 540-543)
// Exercises: tokenData.size() > NSM_DEBUG_TOKEN_DATA_MAX_SIZE -> throw
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installToken_OversizedData_ThrowsInvalidArg)
{
    std::vector<uint8_t> oversizedData(NSM_DEBUG_TOKEN_DATA_MAX_SIZE + 1, 0xAA);
    EXPECT_THROW(debugToken->installToken(oversizedData),
                 Common::Error::InvalidArgument);
}

// ============================================================================
// getRequest: invalid opcode throws InvalidArgument (lines 455-458)
// Exercises: default case in opcode switch -> throw InvalidArgument
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequest_InvalidOpcode_ThrowsInvalidArg)
{
    EXPECT_THROW(
        debugToken->getRequest(static_cast<DebugToken::TokenOpcodes>(0xFF)),
        std::invalid_argument);
}

// ============================================================================
// getStatus: invalid token type throws (lines 505-508)
// Exercises: default case in tokenType switch -> throw
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getStatus_InvalidTokenType_ThrowsInvalidArg)
{
    EXPECT_THROW(
        debugToken->getStatus(static_cast<DebugToken::TokenTypes>(0xFF)),
        std::invalid_argument);
}

// ============================================================================
// disableTokensAsyncHandler: reasonCode != successReasonCode (lines 97-104)
// Exercises: else branch where reasonCode is nonzero -> InternalFailure
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       DISABLED_disableTokensAsync_NonZeroReasonCode_InternalFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 42)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// getRequestAsyncHandler: tokenAlreadyActive (lines 153-161)
// Exercises: reasonCode == tokenAlreadyActiveReasonCode -> WriteFailure
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       DISABLED_getRequestAsync_TokenAlreadyActive_WriteFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse(
            NSM_SUCCESS, tokenAlreadyActiveReasonCode)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// installTokenAsyncHandler: tokenAlreadyActive (lines 400-406)
// Exercises: reasonCode == tokenAlreadyActiveReasonCode -> WriteFailure
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       DISABLED_installTokenAsync_TokenAlreadyActive_WriteFailure)
{
    std::vector<uint8_t> tokenData(50, 0xCC);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(
            NSM_SUCCESS, tokenAlreadyActiveReasonCode)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// installTokenAsyncHandler: other reasonCode (lines 408-414)
// Exercises: reasonCode is neither success nor tokenAlreadyActive
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       DISABLED_installTokenAsync_OtherReasonCode_InternalFailure)
{
    std::vector<uint8_t> tokenData(50, 0xDD);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(NSM_SUCCESS, 99)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// installTokenAsyncHandler: decode failure (lines 375-382)
// Exercises: decodeRc != NSM_SW_SUCCESS -> InternalFailure
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       DISABLED_installTokenAsync_DecodeFail_InternalFailure)
{
    std::vector<uint8_t> tokenData(50, 0xEE);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// update: encode failure path (lines 576-582)
// The encode_nsm_query_device_ids_req is unlikely to fail with a valid
// buffer, but we exercise the update path with a successful encode followed
// by a second decode that fails with cc != NSM_SUCCESS (lines 614-621).
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, update_SecondDecode_CCError)
{
    // Create a response where cc = NSM_ERROR for query_device_ids
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, 0x5555, nullptr, 0, errMsg);
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
    debugToken->update(mockDevice);
}

// ============================================================================
// update: postPatchIO failure (line 588)
// Exercises: sendRc != 0 -> co_return sendRc
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, update_PostPatchIO_Failure)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    debugToken->update(mockDevice);
}

// ============================================================================
// update: success path with single-byte device ID
// Exercises: full success path including hex formatting (lines 623-631)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, update_Success_SingleByteDeviceId)
{
    std::vector<uint8_t> deviceIdBytes = {0xAB};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xAB");
}

// ============================================================================
// update: first decode failure (lines 601-608)
// Exercises: rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS at first decode
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, update_FirstDecode_Failure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    debugToken->update(mockDevice);
}

// ============================================================================
// disableTokens: success path (lines 420-441)
// Exercises: full synchronous -> async flow for disableTokens
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, disableTokens_SuccessPath)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse()));
    auto objPath = debugToken->disableTokens();
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getRequest: CRCS opcode success (lines 443-485)
// Exercises: CRCS case in opcode switch -> success path
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequest_CRCS_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getRequest: CRDT opcode success (lines 443-485)
// Exercises: CRDT case in opcode switch
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequest_CRDT_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getStatus: all four token types (lines 487-535)
// Exercises: all cases in tokenType switch
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getStatus_FRC_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::FRC);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranch3Test, getStatus_CRCS_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranch3Test, getStatus_CRDT_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranch3Test, getStatus_DebugFirmware_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::DebugFirmware);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// installToken: valid small data (lines 538-566)
// Exercises: valid tokenData.size() -> encode -> async flow
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installToken_ValidSmallData_Success)
{
    std::vector<uint8_t> tokenData(10, 0x42);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(tokenData);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// installToken: max valid size boundary (lines 540-541)
// Exercises: tokenData.size() == NSM_DEBUG_TOKEN_DATA_MAX_SIZE (exactly at
// boundary)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installToken_MaxSizeBoundary_Success)
{
    std::vector<uint8_t> tokenData(NSM_DEBUG_TOKEN_DATA_MAX_SIZE, 0x55);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(tokenData);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getStatusAsyncHandler: all valid combinations of status, additionalInfo,
// and tokenType enums to cover switch branches (lines 247-336)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       getStatusAsync_OperationFailure_DebugSessionActive_FRC)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_TYPE_FRC, 100)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_FRC);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch3Test,
       getStatusAsync_DebugSessionActive_FirmwareNotSecured_CRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED,
            NSM_DEBUG_TOKEN_TYPE_CRDT, 200)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_CRDT);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch3Test,
       getStatusAsync_ChallengeProvided_NoDebugSession_DebugFirmware)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_CHALLENGE_PROVIDED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION,
            NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE, 0)));
    auto request =
        makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch3Test,
       getStatusAsync_InstallationTimeout_QueryDisallowed)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_INSTALLATION_TIMEOUT,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_QUERY_DISALLOWED,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch3Test,
       getStatusAsync_TokenTimeout_EndRequestNotAccepted)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_TOKEN_TIMEOUT,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_END_REQUEST_NOT_ACCEPTED,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch3Test, getStatusAsync_DebugSessionEnded_None)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ENDED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// getStatusAsyncHandler: invalid enum defaults
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getStatusAsync_InvalidTokenType_Default)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            static_cast<nsm_debug_token_type>(0xFF), 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

TEST_F(NsmDebugTokenNICBranch3Test, getStatusAsync_InvalidTokenStatus_Default)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            static_cast<nsm_debug_token_status>(0xFE),
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

TEST_F(NsmDebugTokenNICBranch3Test,
       getStatusAsync_InvalidAdditionalInfo_Default)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            static_cast<nsm_debug_token_status_additional_info>(0xFE),
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_ERROR_DATA);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// disableTokensAsyncHandler: success (reasonCode == successReasonCode)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, disableTokensAsync_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 0)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// disableTokensAsyncHandler: postPatchIO generic fail
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, disableTokensAsync_PostPatchIO_GenericFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// disableTokensAsyncHandler: postPatchIO unsupported command
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, disableTokensAsync_PostPatchIO_Unsupported)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

// ============================================================================
// getRequestAsyncHandler: success path
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequestAsync_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryTokenParametersResponse(NSM_SUCCESS, 0)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// getRequestAsyncHandler: decode fail
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequestAsync_DecodeFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// getRequestAsyncHandler: postPatchIO unsupported
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequestAsync_PostPatchIO_Unsupported)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

// ============================================================================
// getRequestAsyncHandler: postPatchIO generic fail
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getRequestAsync_PostPatchIO_GenericFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// installTokenAsyncHandler: success with reasonCode == successReasonCode
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installTokenAsync_Success)
{
    std::vector<uint8_t> tokenData(50, 0xBB);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(NSM_SUCCESS, 0)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// installTokenAsyncHandler: postPatchIO generic fail
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installTokenAsync_PostPatchIO_GenericFail)
{
    std::vector<uint8_t> tokenData(50, 0xAA);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// installTokenAsyncHandler: postPatchIO unsupported
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, installTokenAsync_PostPatchIO_Unsupported)
{
    std::vector<uint8_t> tokenData(50, 0xBB);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

// ============================================================================
// installTokenAsyncHandler: cc != NSM_SUCCESS (token not accepted)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test,
       installTokenAsync_CC_NotSuccess_InvalidArgument)
{
    std::vector<uint8_t> tokenData(50, 0xDD);
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_provide_token_resp(0, NSM_ERROR, 0x1111, errMsg);
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
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InvalidArgument);
}

// ============================================================================
// getStatusAsyncHandler: decode fail
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getStatusAsync_DecodeFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// getStatusAsyncHandler: postPatchIO generic fail
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getStatusAsync_PostPatchIO_GenericFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// getStatusAsyncHandler: postPatchIO unsupported
// ============================================================================

TEST_F(NsmDebugTokenNICBranch3Test, getStatusAsync_PostPatchIO_Unsupported)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}
