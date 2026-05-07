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

struct NsmDebugTokenNICBranchTest :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "NIC_DebugToken_Branch";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenNICObject> debugToken;

    NsmDebugTokenNICBranchTest() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenNICBranchTest()
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
// disableTokensAsyncHandler branch coverage
// ============================================================================

// reasonCode != successReasonCode => InternalFailure (else branch at line 98)
TEST_F(NsmDebugTokenNICBranchTest,
       DISABLED_disableTokensAsyncReasonCodeNotSuccess)
{
    // reasonCode = 42, successReasonCode = 0, so 42 != 0 => else branch
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 42)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// reasonCode == successReasonCode => Success (line 93-96)
TEST_F(NsmDebugTokenNICBranchTest, disableTokensAsyncReasonCodeSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse(NSM_SUCCESS, 0)));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// decode fail via valgrind-safe short response
TEST_F(NsmDebugTokenNICBranchTest, disableTokensAsyncDecodeFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// postPatchIO fail with generic error (not UNSUPPORTED)
TEST_F(NsmDebugTokenNICBranchTest, disableTokensAsyncPostPatchIOGenericFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeDisableTokensRequest();
    auto [rc, statusIntf, valueIntf] = callDisableTokensAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// postPatchIO fail with NSM_ERR_UNSUPPORTED_COMMAND_CODE
TEST_F(NsmDebugTokenNICBranchTest, disableTokensAsyncPostPatchIOUnsupported)
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
// getRequestAsyncHandler branch coverage
// ============================================================================

// postPatchIO fail generic
TEST_F(NsmDebugTokenNICBranchTest, getRequestAsyncPostPatchIOGenericFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// postPatchIO fail unsupported
TEST_F(NsmDebugTokenNICBranchTest, getRequestAsyncPostPatchIOUnsupported)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

// decode fail via short response
TEST_F(NsmDebugTokenNICBranchTest, getRequestAsyncDecodeFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// tokenAlreadyActive reason code => WriteFailure
TEST_F(NsmDebugTokenNICBranchTest, DISABLED_getRequestAsyncTokenAlreadyActive)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse(
            NSM_SUCCESS, tokenAlreadyActiveReasonCode)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// success path with normal reasonCode (not tokenAlreadyActive)
TEST_F(NsmDebugTokenNICBranchTest, getRequestAsyncSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryTokenParametersResponse(NSM_SUCCESS, 0)));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// cc != NSM_SUCCESS in decode (non-success CC)
TEST_F(NsmDebugTokenNICBranchTest, getRequestAsyncNonSuccessCC)
{
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    struct nsm_debug_token_request dummyReq = {};
    encode_nsm_query_token_parameters_resp(0, NSM_ERROR, 0x5678, &dummyReq,
                                           errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(errorResp));
    auto request = makeQueryTokenParametersRequest();
    auto [rc, statusIntf, valueIntf] = callGetRequestAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// installTokenAsyncHandler branch coverage
// ============================================================================

// postPatchIO fail generic
TEST_F(NsmDebugTokenNICBranchTest, installTokenAsyncPostPatchIOGenericFail)
{
    std::vector<uint8_t> tokenData(50, 0xAA);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
    auto val = valueIntf->value();
    auto* errorTuple = std::get_if<std::tuple<uint16_t, std::string>>(&val);
    ASSERT_NE(errorTuple, nullptr);
    EXPECT_NE(std::get<0>(*errorTuple), 0);
}

// postPatchIO fail unsupported
TEST_F(NsmDebugTokenNICBranchTest, installTokenAsyncPostPatchIOUnsupported)
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

// decode fail via short response
TEST_F(NsmDebugTokenNICBranchTest, installTokenAsyncDecodeFail)
{
    std::vector<uint8_t> tokenData(50, 0xCC);
    // Use a malformed response that causes decode to fail
    Response malformedResp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(malformedResp.data());
    encode_nsm_provide_token_resp(0, NSM_ERROR, 0x1234, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(malformedResp));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// cc != NSM_SUCCESS => InvalidArgument
TEST_F(NsmDebugTokenNICBranchTest, installTokenAsyncCCNotSuccess)
{
    std::vector<uint8_t> tokenData(50, 0xDD);
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_provide_token_resp(0, NSM_ERROR, 0x9999, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce([errorResp](eid_t, Request&,
                              std::shared_ptr<const nsm_msg>& responseMsg,
                              size_t& responseLen) -> requester::Coroutine {
        responseLen = errorResp.size();
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

// reasonCode == successReasonCode => Success
TEST_F(NsmDebugTokenNICBranchTest, installTokenAsyncReasonCodeSuccess)
{
    std::vector<uint8_t> tokenData(50, 0xEE);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(NSM_SUCCESS, 0)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// reasonCode == tokenAlreadyActiveReasonCode => WriteFailure
TEST_F(NsmDebugTokenNICBranchTest, DISABLED_installTokenAsyncTokenAlreadyActive)
{
    std::vector<uint8_t> tokenData(50, 0xFF);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(
            NSM_SUCCESS, tokenAlreadyActiveReasonCode)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// reasonCode is neither success nor tokenAlreadyActive => InternalFailure
TEST_F(NsmDebugTokenNICBranchTest, DISABLED_installTokenAsyncReasonCodeOther)
{
    std::vector<uint8_t> tokenData(50, 0x11);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse(NSM_SUCCESS, 99)));
    auto request = makeProvideTokenRequest(tokenData);
    auto [rc, statusIntf, valueIntf] = callInstallTokenAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// getStatusAsyncHandler branch coverage
// ============================================================================

// postPatchIO fail generic
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncPostPatchIOGenericFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::WriteFailure);
}

// postPatchIO fail unsupported
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncPostPatchIOUnsupported)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

// decode fail via short response
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncDecodeFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDecodeFail()));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// invalid token type in response => default branch
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncInvalidTokenType)
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

// invalid token status in response => default branch
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncInvalidTokenStatus)
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

// invalid additional info => default branch
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncInvalidAdditionalInfo)
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

// All valid token type enum values
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncTokenTypeFRC)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_FRC, 0)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_FRC);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncTokenTypeCRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRDT, 0)));
    auto request = makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_CRDT);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncTokenTypeDebugFirmware)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE, 0)));
    auto request =
        makeQueryTokenStatusRequest(NSM_DEBUG_TOKEN_TYPE_DEBUG_FIRMWARE);
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// All valid token status enum values
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncStatusDebugSessionEnded)
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

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncStatusOperationFailure)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_OPERATION_FAILURE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncStatusDebugSessionActive)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 3600)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncStatusChallengeProvided)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_CHALLENGE_PROVIDED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncStatusInstallationTimeout)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_INSTALLATION_TIMEOUT,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncStatusTokenTimeout)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_TOKEN_TIMEOUT,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// All valid additional info enum values
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncAdditionalInfoNoDebugSession)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NO_DEBUG_SESSION,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest,
       getStatusAsyncAdditionalInfoFirmwareNotSecured)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_FIRMWARE_NOT_SECURED,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest,
       getStatusAsyncAdditionalInfoEndRequestNotAccepted)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_END_REQUEST_NOT_ACCEPTED,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 100)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncAdditionalInfoQueryDisallowed)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_QUERY_DISALLOWED,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 0)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

TEST_F(NsmDebugTokenNICBranchTest,
       getStatusAsyncAdditionalInfoDebugSessionActive)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse(
            NSM_DEBUG_TOKEN_STATUS_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_DEBUG_SESSION_ACTIVE,
            NSM_DEBUG_TOKEN_TYPE_CRCS, 500)));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(rc, NSM_SW_SUCCESS);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// cc != NSM_SUCCESS in decode
TEST_F(NsmDebugTokenNICBranchTest, getStatusAsyncNonSuccessCC)
{
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_query_token_status_resp(
        0, NSM_ERROR, 0x1234, NSM_DEBUG_TOKEN_STATUS_NO_TOKEN_APPLIED,
        NSM_DEBUG_TOKEN_STATUS_ADDITIONAL_INFO_NONE, NSM_DEBUG_TOKEN_TYPE_CRCS,
        0, errMsg);
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(errorResp));
    auto request = makeQueryTokenStatusRequest();
    auto [rc, statusIntf, valueIntf] = callGetStatusAsync(request);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::InternalFailure);
}

// ============================================================================
// update() branch coverage
// ============================================================================

// update success
TEST_F(NsmDebugTokenNICBranchTest, updateSuccess)
{
    std::vector<uint8_t> deviceIdBytes = {0xAB, 0xCD, 0xEF, 0x01};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(deviceIdBytes, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0xABCDEF01");
}

// update postPatchIO fail
TEST_F(NsmDebugTokenNICBranchTest, updatePostPatchIOFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    debugToken->update(mockDevice);
}

// update first decode fail (cc != NSM_SUCCESS)
TEST_F(NsmDebugTokenNICBranchTest, updateFirstDecodeFail)
{
    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_query_device_ids_resp(0, NSM_ERROR, 0x4444, nullptr, 0, errMsg);
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

// update with empty device ID
TEST_F(NsmDebugTokenNICBranchTest, DISABLED_updateEmptyDeviceId)
{
    std::vector<uint8_t> emptyId;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(emptyId, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0x");
}

// ============================================================================
// disableTokens() synchronous method branch coverage
// ============================================================================

TEST_F(NsmDebugTokenNICBranchTest, disableTokensSuccess)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createDisableTokensResponse()));
    auto objPath = debugToken->disableTokens();
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// getRequest() synchronous method branch coverage
// ============================================================================

TEST_F(NsmDebugTokenNICBranchTest, getRequestCRCS)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, getRequestCRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenParametersResponse()));
    auto objPath = debugToken->getRequest(DebugToken::TokenOpcodes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, getRequestInvalidOpcode)
{
    EXPECT_THROW(
        debugToken->getRequest(static_cast<DebugToken::TokenOpcodes>(999)),
        std::invalid_argument);
}

// ============================================================================
// getStatus() synchronous method branch coverage
// ============================================================================

TEST_F(NsmDebugTokenNICBranchTest, getStatusFRC)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::FRC);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusCRCS)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRCS);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusCRDT)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::CRDT);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusDebugFirmware)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createQueryTokenStatusResponse()));
    auto objPath = debugToken->getStatus(DebugToken::TokenTypes::DebugFirmware);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, getStatusInvalidType)
{
    EXPECT_THROW(
        debugToken->getStatus(static_cast<DebugToken::TokenTypes>(999)),
        std::invalid_argument);
}

// ============================================================================
// installToken() synchronous method branch coverage
// ============================================================================

TEST_F(NsmDebugTokenNICBranchTest, installTokenSuccess)
{
    std::vector<uint8_t> tokenData(100, 0xAB);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(tokenData);
    EXPECT_FALSE(objPath.str.empty());
}

TEST_F(NsmDebugTokenNICBranchTest, installTokenEmptyData)
{
    std::vector<uint8_t> emptyData;
    EXPECT_THROW(
        debugToken->installToken(emptyData),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDebugTokenNICBranchTest, installTokenOversizedData)
{
    std::vector<uint8_t> oversizedData(NSM_DEBUG_TOKEN_DATA_MAX_SIZE + 1, 0xFF);
    EXPECT_THROW(
        debugToken->installToken(oversizedData),
        sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument);
}

TEST_F(NsmDebugTokenNICBranchTest, installTokenMaxSize)
{
    std::vector<uint8_t> maxData(NSM_DEBUG_TOKEN_DATA_MAX_SIZE, 0xCC);
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createProvideTokenResponse()));
    auto objPath = debugToken->installToken(maxData);
    EXPECT_FALSE(objPath.str.empty());
}
