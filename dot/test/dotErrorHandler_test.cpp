/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#include "base.h"

#include "dotErrorHandler.hpp"

#include <gtest/gtest.h>

using namespace nsm::dot;

struct DotErrorHandlerTest : public ::testing::Test
{};

TEST_F(DotErrorHandlerTest, testSuccessfulOperation)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_GET_STATUS;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_NE(message.find("completed successfully"), std::string::npos);
}

TEST_F(DotErrorHandlerTest, testInternalError)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_LOCK;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_NE(message.find("unexpected internal failure"), std::string::npos);
}

TEST_F(DotErrorHandlerTest, testInternalErrorWithReasonCode)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = 123;
    nsm_firmware_commands operation = NSM_FW_DOT_UNLOCK;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 123);
    EXPECT_NE(message.find("reason code"), std::string::npos);
}

TEST_F(DotErrorHandlerTest, testReasonCodeInternalError)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = DOT_RC_INTERNAL_ERROR;
    nsm_firmware_commands operation = NSM_FW_DOT_CAK_INSTALL;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, DOT_RC_INTERNAL_ERROR);
    EXPECT_EQ(message, "DOTInternalError");
}

TEST_F(DotErrorHandlerTest, testReasonCodeStateInvalid)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = DOT_RC_STATE_INVALID;
    nsm_firmware_commands operation = NSM_FW_DOT_LOCK;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, DOT_RC_STATE_INVALID);
    EXPECT_EQ(message, "DOTStateInvalid");
}

TEST_F(DotErrorHandlerTest, testReasonCodeSignatureVerificationFailed)
{
    uint8_t cc = NSM_ERROR;
    uint16_t reasonCode = DOT_RC_SIGNATURE_VERIFICATION_FAILED;
    nsm_firmware_commands operation = NSM_FW_DOT_CAK_ROTATE;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, DOT_RC_SIGNATURE_VERIFICATION_FAILED);
    EXPECT_EQ(message, "DOTSignatureVerificationFailed");
}

TEST_F(DotErrorHandlerTest, testCompletionCodeInvalidData)
{
    uint8_t cc = NSM_ERR_INVALID_DATA;
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_UNLOCK_CHALLENGE;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_EQ(message, "Invalid data provided for DOT operation");
}

TEST_F(DotErrorHandlerTest, testCompletionCodeInvalidDataLength)
{
    uint8_t cc = NSM_ERR_INVALID_DATA_LENGTH;
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_DISABLE;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_EQ(message, "Invalid data length for DOT operation");
}

TEST_F(DotErrorHandlerTest, testCompletionCodeNotReady)
{
    uint8_t cc = NSM_ERR_NOT_READY;
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_GET_INFO;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_EQ(message, "Device not ready for DOT operation");
}

TEST_F(DotErrorHandlerTest, testCompletionCodeUnsupportedCommand)
{
    uint8_t cc = NSM_ERR_UNSUPPORTED_COMMAND_CODE;
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_OVERRIDE;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_EQ(message, "Unsupported DOT command");
}

TEST_F(DotErrorHandlerTest, testUnknownCompletionCode)
{
    uint8_t cc = 0xFF; // Unknown completion code
    uint16_t reasonCode = 0;
    nsm_firmware_commands operation = NSM_FW_DOT_RECOVERY;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    EXPECT_NE(message.find("failed with completion code"), std::string::npos);
}

TEST_F(DotErrorHandlerTest, testUnknownCompletionCodeWithReasonCode)
{
    uint8_t cc = 0xFF;
    uint16_t reasonCode = 456;
    nsm_firmware_commands operation = NSM_FW_DOT_CAK_BYPASS;

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 456);
    EXPECT_NE(message.find("reason code"), std::string::npos);
}

TEST_F(DotErrorHandlerTest, testAllDotOperations)
{
    nsm_firmware_commands operations[] = {
        NSM_FW_DOT_CAK_INSTALL,      NSM_FW_DOT_CAK_BYPASS,
        NSM_FW_DOT_CAK_ROTATE,       NSM_FW_DOT_LOCK,
        NSM_FW_DOT_UNLOCK_CHALLENGE, NSM_FW_DOT_UNLOCK,
        NSM_FW_DOT_GET_INFO,         NSM_FW_DOT_GET_STATUS,
        NSM_FW_DOT_DISABLE,          NSM_FW_DOT_OVERRIDE,
        NSM_FW_DOT_RECOVERY};

    for (const auto& op : operations)
    {
        auto [code, message] = formatDotDeviceError(NSM_SUCCESS, 0, op);
        EXPECT_EQ(code, 0);
        EXPECT_FALSE(message.empty());
    }
}

TEST_F(DotErrorHandlerTest, testAllReasonCodes)
{
    std::vector<uint16_t> reasonCodes = {DOT_RC_INTERNAL_ERROR,
                                         DOT_RC_STATE_INVALID,
                                         DOT_RC_SIGNATURE_VERIFICATION_FAILED,
                                         DOT_RC_STORAGE_ERROR,
                                         DOT_RC_LOCK_DISABLED,
                                         DOT_RC_KEY_MISMATCH,
                                         DOT_RC_INVALID_UNLOCK_TYPE,
                                         DOT_RC_INVALID_UNLOCK_METHOD,
                                         DOT_RC_CRYPTO_ERROR,
                                         DOT_RC_BLOB_CREATION_FAILED,
                                         DOT_RC_INVALID_STATE_FOR_LOCK,
                                         DOT_RC_INVALID_STATE_FOR_UNLOCK,
                                         DOT_RC_INVALID_STATE_FOR_DISABLE,
                                         DOT_RC_INVALID_STATE_FOR_CAK_ROTATE,
                                         DOT_RC_INVALID_STATE_FOR_CAK_INSTALL,
                                         DOT_RC_NULL_POINTER,
                                         DOT_RC_FUSE_INCREMENT_FAILED,
                                         DOT_RC_RECOVERY_FAILED,
                                         DOT_RC_INVALID_COMMAND,
                                         DOT_RC_UNSUPPORTED_COMMAND,
                                         DOT_RC_INVALID_LENGTH};

    for (const auto& rc : reasonCodes)
    {
        auto [code, message] = formatDotDeviceError(NSM_ERROR, rc,
                                                    NSM_FW_DOT_LOCK);
        EXPECT_EQ(code, rc);
        EXPECT_FALSE(message.empty());
    }
}

/**
 * Test formatDotDeviceError with an unknown/invalid operation
 * This covers the default case in dotOperationToString (lines 76-77 in
 * dotErrorHandler.cpp)
 */
TEST_F(DotErrorHandlerTest, testUnknownOperation)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = 0;
    // Cast an invalid value to nsm_firmware_commands to trigger default case
    nsm_firmware_commands operation = static_cast<nsm_firmware_commands>(0xFF);

    auto [code, message] = formatDotDeviceError(cc, reasonCode, operation);

    EXPECT_EQ(code, 0);
    // The message should contain "DOT Unknown Operation" from the default case
    EXPECT_NE(message.find("DOT Unknown Operation"), std::string::npos);
    EXPECT_NE(message.find("completed successfully"), std::string::npos);
}
