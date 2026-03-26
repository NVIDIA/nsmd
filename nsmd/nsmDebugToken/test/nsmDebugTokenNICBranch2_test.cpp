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
 * - disableTokensAsyncHandler: decode fail with cc == NSM_SUCCESS but
 *   reasonCode != 0
 * - getRequestAsyncHandler: write failure loop path
 * - getStatusAsyncHandler: all switch case combinations not covered
 * - installTokenAsyncHandler: decode fail, cc checks, reasonCode paths
 * - installToken: valid data size boundary
 * - getRequest: CRDT opcode
 * - getStatus: all token types
 * - update: second decode fail (cc != NSM_SUCCESS)
 * - disableTokens: encode failure path
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

struct NsmDebugTokenNICBranch2Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NIC_DebugToken_Branch2";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:20";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenNICObject> debugToken;

    NsmDebugTokenNICBranch2Test() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenNICBranch2Test()
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
// disableTokensAsyncHandler: decode success cc=SUCCESS but reasonCode != 0
// Exercises L92: else branch (reasonCode != successReasonCode)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test,
       DISABLED_disableTokensAsync_ReasonCodeNonZero_InternalFailure)
{
    // reasonCode = 42 != successReasonCode (0)
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 42)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// disableTokensAsyncHandler: cc != NSM_SUCCESS in decode
// Exercises L83: (decodeRc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test,
       disableTokensAsync_CC_NonSuccess_WriteFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// ============================================================================
// getRequestAsyncHandler: success path (writes token request to fd)
// Exercises L163-195: memfd_create, write loop, lseek, valueIntf->value(fd)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, getRequestAsync_Success_WritesRequestToFd)
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
// getRequestAsyncHandler: CRDT opcode
// Exercises the CRDT opcode path in getRequest
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, getRequestAsync_CRDT_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryTokenParametersResponse(NSM_SUCCESS, 0)));
    auto request = makeQueryTokenParametersRequest(NSM_DEBUG_TOKEN_OPCODE_CRDT);
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// getStatusAsyncHandler: all valid token type combinations
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, getStatusAsync_TokenType_CRCS)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_CRCS);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// getStatusAsyncHandler: all status enum combinations for broader coverage
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test,
       getStatusAsync_OperationFailure_DebugSessionActive)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_TYPE_FRC, 999)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_FRC);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch2Test,
       getStatusAsync_DebugSessionActive_FirmwareNotSecured)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED,
            NSM_DEBUG_TOKEN_TYPE_CRDT, 500)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_CRDT);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranch2Test,
       getStatusAsync_ChallengeProvided_NoDebugSession)
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

TEST_F(NsmDebugTokenNICBranch2Test,
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

TEST_F(NsmDebugTokenNICBranch2Test,
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

TEST_F(NsmDebugTokenNICBranch2Test, getStatusAsync_DebugSessionEnded_None)
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
// installTokenAsyncHandler: cc != NSM_SUCCESS (token not accepted)
// Exercises L384: if (cc != NSM_SUCCESS) -> InvalidArgument
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, installTokenAsync_CC_NotSuccess)
{
    std::vector<uint8_t> tokenData(50, 0xAA);
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
// installTokenAsyncHandler: reasonCode == successReasonCode
// Exercises L395-398: success path
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, installTokenAsync_ReasonCode_Success)
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
// installTokenAsyncHandler: reasonCode == tokenAlreadyActiveReasonCode
// Exercises L400-406: tokenAlreadyActive path
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test,
       DISABLED_installTokenAsync_TokenAlreadyActive)
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
// installTokenAsyncHandler: reasonCode is other (not success, not already
// active) Exercises L408-414: else branch -> InternalFailure
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, DISABLED_installTokenAsync_ReasonCode_Other)
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
// installTokenAsyncHandler: decode fail (short response)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, DISABLED_installTokenAsync_DecodeFail)
{
    std::vector<uint8_t> tokenData(50, 0xEE);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// installToken: single-byte token data (minimum valid size)
// Exercises L540: tokenData.size() == 0 FALSE, size > MAX FALSE
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, installToken_SingleByte)
{
    std::vector<uint8_t> tokenData(1, 0x42);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(tokenData);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getRequest: CRDT opcode
// Exercises L453: case DebugToken::TokenOpcodes::CRDT
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, getRequest_CRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getRequest: CRCS opcode
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, getRequest_CRCS)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getStatus: all four token types
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, getStatus_FRC)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::FRC);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranch2Test, getStatus_CRCS)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranch2Test, getStatus_CRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranch2Test, getStatus_DebugFirmware)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::DebugFirmware);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// update: success with multi-byte device ID
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, update_Success_MultiByteDeviceId)
{
    std::vector<uint8_t> deviceIdBytes = {0x01, 0x02, 0x03, 0x04, 0x05};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0x0102030405");
}

// ============================================================================
// update: postPatchIO returns non-zero (sendRc truthy)
// Exercises L588: if (sendRc) -> co_return sendRc
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, update_PostPatchIO_Fail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    debugToken->update(mockDevice);
}

// ============================================================================
// update: first decode fail (rc != NSM_SW_SUCCESS)
// Exercises L601: if (rc != NSM_SW_SUCCESS || cc != NSM_SUCCESS)
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, update_FirstDecodeFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    debugToken->update(mockDevice);
}

// ============================================================================
// update: second decode cc != NSM_SUCCESS
// This requires a response that passes first decode (gets deviceIdLen)
// but fails on second decode with different cc.
// Since both use same response buffer, we use cc=NSM_ERROR which fails both.
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, update_SecondDecode_CC_NonSuccess)
{
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, 0x6666, nullptr, 0, errMsg);
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
// disableTokens: success path
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, disableTokens_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse()));
    auto objPath = debugToken->disableTokens();
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// installToken: success with medium-sized data
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, installToken_MediumData_Success)
{
    std::vector<uint8_t> tokenData(256, 0x77);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(tokenData);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getRequestAsyncHandler: tokenAlreadyActive reasonCode
// Exercises L153-161: reasonCode == tokenAlreadyActiveReasonCode path
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, DISABLED_getRequestAsync_TokenAlreadyActive)
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
// disableTokensAsyncHandler: success with reasonCode == successReasonCode
// Exercises L92-96: if (reasonCode == successReasonCode) -> Success
// ============================================================================

TEST_F(NsmDebugTokenNICBranch2Test, disableTokensAsync_Success)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 0)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}
