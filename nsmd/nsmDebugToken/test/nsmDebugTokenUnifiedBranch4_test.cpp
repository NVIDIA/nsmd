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
 * Branch coverage batch 4 for nsmd/nsmDebugToken/nsmDebugTokenUnified.cpp.
 *
 * Targets zero-covered branches:
 * - installTokenDirect: multi-chunk with 3+ chunks, fail on second chunk
 * - deviceIdHandler: second decode cc != SUCCESS, empty/single-byte device ID
 * - deviceCapabilitiesHandler: cc != SUCCESS but rc == SUCCESS
 * - update(): skip deviceId/capabilities, all fail, partial fail
 * - eraseToken: EraseType variants (EraseAll, EraseAllAndRatchetCounter,
 *   TokenType with valid type)
 * - installTokenAsyncHandler: 3 chunk recursion
 * - createInstallTokenRequest: valid read, invalid fd
 * - queryTokenHandler: various token type scenarios
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#include "debug-token.h"
#include "debug-token/tlv.h"
#include "debug-token/types.h"

#define private public
#define protected public

#include "nsmDebugTokenUnified.hpp"

using namespace nsm;

// ============================================================================
// Fixture
// ============================================================================

struct NsmDebugTokenUnifiedBranch4Test :
    public Test,
    public utils::DBusTest,
    public SensorManagerTest
{
    const std::string name = "Unified_Branch4";
    const uuid_t uuid = "STATIC:0:0:NSM_DEVICE_INSTANCE_NUMBER:0";
    const std::string debugTokenDeviceType = "GPU";

    NsmDeviceTable devices;
    std::shared_ptr<MockNsmDevice> mockDevice;
    std::shared_ptr<NsmDebugTokenUnifiedObject> debugToken;

    NsmDebugTokenUnifiedBranch4Test() : SensorManagerTest(devices)
    {
        mockDevice = std::dynamic_pointer_cast<MockNsmDevice>(
            mockManager.getNsmDeviceFromStaticUUID(uuid));
        EXPECT_EQ(1, devices.size());
        EXPECT_NE(mockDevice, nullptr);
    }

    ~NsmDebugTokenUnifiedBranch4Test()
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

    // -- Response helpers --

    Response createEraseTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                      uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_erase_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        [[maybe_unused]] auto rc =
            encode_nsm_erase_token_resp(0, completionCode, reasonCode, msg);
        return response;
    }

    Response createInstallTokenResponse(uint8_t completionCode = NSM_SUCCESS,
                                        uint16_t reasonCode = 0)
    {
        Response response(sizeof(nsm_msg_hdr) + sizeof(nsm_install_token_resp),
                          0);
        auto msg = reinterpret_cast<nsm_msg*>(response.data());
        [[maybe_unused]] auto rc =
            encode_nsm_install_token_resp(0, completionCode, reasonCode, msg);
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
        [[maybe_unused]] auto rc = encode_nsm_query_device_ids_resp(
            0, completionCode, reasonCode, deviceId.data(), deviceId.size(),
            msg);
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
        [[maybe_unused]] auto rc = encode_nsm_get_device_capabilities_v2_resp(
            0, completionCode, reasonCode, timestampGen, maxInputBufferSize,
            msg);
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
        [[maybe_unused]] auto rc = encode_nsm_query_token_resp(
            0, completionCode, reasonCode, tlvPayload.data(), tlvPayload.size(),
            msg);
        return response;
    }

    Response createDecodeFail()
    {
        Response response(
            sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
        auto buf = response.data();
        buf[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
        return response;
    }

    auto callEraseTokenAsync(uint32_t tokenTypeValue)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        debugToken->eraseTokenAsyncHandler(tokenTypeValue, statusInterface,
                                           valueInterface);
        return std::make_pair(statusInterface, valueInterface);
    }

    auto callInstallTokenAsync(
        std::shared_ptr<NsmDebugTokenUnifiedObject::TokenInstallationInfo> info)
    {
        const auto [_, statusInterface, valueInterface] =
            AsyncOperationManager::getInstance()->getNewStatusValueInterface();
        debugToken->installTokenAsyncHandler(info, statusInterface,
                                             valueInterface);
        return std::make_pair(statusInterface, valueInterface);
    }

    std::shared_ptr<NsmDebugTokenUnifiedObject::TokenInstallationInfo>
        createTokenInstallationInfo(const std::vector<uint8_t>& tokenData)
    {
        char tempPath[] = "/tmp/unified_b4_XXXXXX";
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
// installTokenDirect: multi-chunk with 3 chunks
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, installTokenDirect_ThreeChunks_Success)
{
    debugToken->installationChunkSize = 50;
    char tempPath[] = "/tmp/branch4_3chunk_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(150, 0x88);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    debugToken->installTokenDirect(fd, data.size(), errorCode, errorMessage);
    EXPECT_EQ(errorCode, 0);
    EXPECT_EQ(errorMessage, "Success");
}

// ============================================================================
// installTokenDirect: fail on second chunk
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, installTokenDirect_FailOnSecondChunk)
{
    debugToken->installationChunkSize = 50;
    char tempPath[] = "/tmp/branch4_2fail_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0x99);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(NSM_ERROR));
    debugToken->installTokenDirect(fd, data.size(), errorCode, errorMessage);
    EXPECT_NE(errorCode, 0);
}

// ============================================================================
// installTokenDirect: cc != SUCCESS on second chunk
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, installTokenDirect_CCFailOnSecondChunk)
{
    debugToken->installationChunkSize = 50;
    char tempPath[] = "/tmp/branch4_cc2_XXXXXX";
    int fd = mkstemp(tempPath);
    ASSERT_NE(fd, -1);
    std::vector<uint8_t> data(100, 0xAA);
    if (write(fd, data.data(), data.size()) < 0)
    {}
    lseek(fd, 0, SEEK_SET);
    unlink(tempPath);
    uint16_t errorCode = 0;
    std::string errorMessage;

    Response errorResp(
        sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    auto errMsg = reinterpret_cast<nsm_msg*>(errorResp.data());
    encode_nsm_install_token_resp(0, NSM_ERROR, 0x1234, errMsg);

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
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
    debugToken->installTokenDirect(fd, data.size(), errorCode, errorMessage);
    EXPECT_NE(errorCode, 0);
    close(fd);
}

// ============================================================================
// deviceIdHandler: second decode cc != NSM_SUCCESS
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test,
       deviceIdHandler_SecondDecodeCC_NonSuccess)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp.data()[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));
    debugToken->deviceIdHandler(mockDevice);
}

// ============================================================================
// deviceCapabilitiesHandler: cc != SUCCESS but rc == SUCCESS
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test,
       deviceCapabilitiesHandler_CCNonSuccess_RcSuccess)
{
    Response resp(sizeof(nsm_msg_hdr) + sizeof(nsm_common_non_success_resp), 0);
    resp.data()[sizeof(nsm_msg_hdr) + 1] = NSM_ERROR;
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(resp));
    debugToken->deviceCapabilitiesHandler(mockDevice);
    EXPECT_EQ(debugToken->installationChunkSize, 0u);
}

// ============================================================================
// update(): skip deviceId and capabilities (both already set)
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, update_SkipDeviceIdAndCapabilities)
{
    debugToken->tokenDeviceID("0x1234ABCD");
    debugToken->installationChunkSize = 2048;

    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(1));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(1));
    enc.add(debug_token::types::TokenTypeSubtypeList,
            std::vector<uint32_t>{1, 0});
    auto payload = enc.encode();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
    EXPECT_TRUE(debugToken->installationStatus());
}

// ============================================================================
// update(): all three handlers fail
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, update_AllHandlersFail_ReturnsError)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));
    debugToken->update(mockDevice);
}

// ============================================================================
// eraseToken: EraseType::EraseAllAndRatchetCounterIncreased
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, eraseToken_EraseAllRatchetCounter)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    auto objPath = debugToken->eraseToken(
        EraseTypeEnum::EraseAllAndRatchetCounterIncreased, TokenTypeEnum::None);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// eraseToken: EraseType::EraseAll
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, eraseToken_EraseAll)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    auto objPath = debugToken->eraseToken(EraseTypeEnum::EraseAll,
                                          TokenTypeEnum::None);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// eraseToken: EraseType::TokenType with valid tokenType (DebugFirmwareUnlock)
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, eraseToken_TokenType_Valid)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createEraseTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    auto objPath = debugToken->eraseToken(EraseTypeEnum::TokenType,
                                          TokenTypeEnum::DebugFirmwareUnlock);
    EXPECT_FALSE(objPath.str.empty());
}

// ============================================================================
// eraseToken: EraseType::TokenType with None tokenType -> tokenTypeValue==0
// -> throws InvalidArgument
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, eraseToken_TokenType_Invalid_Throws)
{
    using EraseTypeEnum =
        sdbusplus::server::com::nvidia::debug_token::Action::EraseType;
    using TokenTypeEnum =
        sdbusplus::common::com::nvidia::debug_token::Common::Types;

    EXPECT_THROW(
        debugToken->eraseToken(EraseTypeEnum::TokenType, TokenTypeEnum::None),
        Common::Error::InvalidArgument);
}

// ============================================================================
// queryTokenHandler: multiple token type/subtype pairs
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test,
       queryTokenHandler_MultipleTypes_DifferentTokens)
{
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(1));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(1));
    enc.add(debug_token::types::TokenTypeSubtypeList,
            std::vector<uint32_t>{1, 0x01, 2, 0x03});
    auto payload = enc.encode();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    debugToken->queryTokenHandler(mockDevice);
    EXPECT_TRUE(debugToken->installationStatus());
    EXPECT_TRUE(debugToken->processingStatus());
}

// ============================================================================
// queryTokenHandler: installStatus=0, procStatus=0, no token types
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test,
       queryTokenHandler_NotInstalled_NoTokenTypes_Success)
{
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(0));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    auto payload = enc.encode();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    debugToken->queryTokenHandler(mockDevice);
    EXPECT_FALSE(debugToken->installationStatus());
    EXPECT_FALSE(debugToken->processingStatus());
}

// ============================================================================
// installTokenAsyncHandler: multi-chunk with 3 chunks recursion
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, installTokenAsync_ThreeChunk_Recursion)
{
    debugToken->installationChunkSize = 50;
    std::vector<uint8_t> tokenData(150, 0xBB);
    auto info = createTokenInstallationInfo(tokenData);

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillOnce(mockPostPatchIO(createInstallTokenResponse(NSM_SUCCESS, 0)))
        .WillRepeatedly(mockPostPatchIO(NSM_ERROR));

    auto [statusIntf, valueIntf] = callInstallTokenAsync(info);
    EXPECT_EQ(statusIntf->status(), AsyncOperationStatusType::Success);
}

// ============================================================================
// createInstallTokenRequest: valid read scenario
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, createInstallTokenRequest_ValidRead)
{
    debugToken->installationChunkSize = 256;
    std::vector<uint8_t> tokenData(64, 0xDD);
    auto info = createTokenInstallationInfo(tokenData);

    auto result = debugToken->createInstallTokenRequest(info);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(info->offset, 64u);
}

// ============================================================================
// createInstallTokenRequest: fd = -1 returns nullopt
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, createInstallTokenRequest_InvalidFd)
{
    debugToken->installationChunkSize = 256;
    auto info =
        std::make_shared<NsmDebugTokenUnifiedObject::TokenInstallationInfo>(
            -1, 100);

    auto result = debugToken->createInstallTokenRequest(info);
    EXPECT_FALSE(result.has_value());
}

// ============================================================================
// eraseTokenAsyncHandler: sendRc == NSM_ERR_UNSUPPORTED_COMMAND_CODE
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, eraseTokenAsyncHandler_SendUnsupported)
{
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(static_cast<nsm_completion_codes>(
            NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    auto [statusIntf, valueIntf] = callEraseTokenAsync(0xFFFFFFFF);
    EXPECT_EQ(statusIntf->status(),
              AsyncOperationStatusType::UnsupportedRequest);
}

// ============================================================================
// deviceIdHandler: success with empty device ID
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, deviceIdHandler_EmptyDeviceId)
{
    std::vector<uint8_t> emptyId;
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(emptyId, NSM_SUCCESS, 0)));
    debugToken->deviceIdHandler(mockDevice);
    EXPECT_EQ(debugToken->tokenDeviceID(), "");
}

// ============================================================================
// deviceIdHandler: success with single byte device ID
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, deviceIdHandler_SingleByteId)
{
    std::vector<uint8_t> singleByte = {0x42};
    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(singleByte, NSM_SUCCESS, 0)));
    debugToken->deviceIdHandler(mockDevice);
    EXPECT_EQ(debugToken->tokenDeviceID(), "0x42");
}

// ============================================================================
// update: deviceId fails, capabilities succeeds, queryToken succeeds
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, update_DeviceIdFails_RestSucceeds)
{
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(0));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    auto payload = enc.encode();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(NSM_ERROR)) // deviceId fails
        .WillOnce(mockPostPatchIO(createDeviceCapabilitiesV2Response(1, 4096)))
        .WillOnce(
            mockPostPatchIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
}

// ============================================================================
// update: capabilities fails, rest succeeds
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, update_CapabilitiesFails_RestSucceeds)
{
    std::vector<uint8_t> deviceIdBytes = {0xAA, 0xBB};
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(0));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    auto payload = enc.encode();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(mockPostPatchIO(
            createQueryDeviceIdsResponse(deviceIdBytes))) // id ok
        .WillOnce(mockPostPatchIO(NSM_ERROR))             // capabilities fail
        .WillOnce(
            mockPostPatchIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    debugToken->update(mockDevice);
    EXPECT_EQ(debugToken->installationChunkSize, 0u);
}

// ============================================================================
// queryTokenHandler: installStatus=1, procStatus=0, with types
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test,
       queryTokenHandler_InstalledProc0_WithTypes)
{
    debug_token::tlv_encoder::Structure enc;
    enc.add(debug_token::types::InstallationStatus, uint8_t(1));
    enc.add(debug_token::types::ProcessingStatus, uint8_t(0));
    enc.add(debug_token::types::TokenTypeSubtypeList,
            std::vector<uint32_t>{1, 0});
    auto payload = enc.encode();

    EXPECT_CALL(*mockDevice, postPatchIO)
        .WillOnce(
            mockPostPatchIO(createQueryTokenResponse(payload, NSM_SUCCESS, 0)));
    debugToken->queryTokenHandler(mockDevice);
    EXPECT_TRUE(debugToken->installationStatus());
    EXPECT_FALSE(debugToken->processingStatus());
}

// ============================================================================
// queryTokenHandler: postPatchIO fail
// ============================================================================

TEST_F(NsmDebugTokenUnifiedBranch4Test, queryTokenHandler_PostPatchIOFail)
{
    EXPECT_CALL(*mockDevice, postPatchIO).WillOnce(mockPostPatchIO(NSM_ERROR));
    debugToken->queryTokenHandler(mockDevice);
}
