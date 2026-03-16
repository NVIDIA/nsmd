/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "libnsm/base.h"
#include "libnsm/firmware-utils.h"

#include "nsmFirmwareUtils/nsmFirmwareUtilsCommon.hpp"

#include <string>
#include <tuple>

#include <gtest/gtest.h>

using namespace nsm;

// Test fixture for getErrorCode function
class NsmFirmwareUtilsCommonTest : public ::testing::Test
{};

// ============================================================================
// Tests for reason code path (cc != NSM_SUCCESS && reasonCode != 0)
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, ReasonCodeFoundInFirmwareCommandErrors)
{
    // Test each reason code in firmwareCommandErrors map
    auto [code, msg] = getErrorCode(0, NSM_ERROR, 0x86);
    EXPECT_EQ(code, NSM_ERROR);
    EXPECT_EQ(msg, "EFUSE Update Failed");

    std::tie(code, msg) = getErrorCode(0, NSM_ERROR, 0x87);
    EXPECT_EQ(code, NSM_ERROR);
    EXPECT_EQ(msg, "IrreversibleConfig Disabled");

    std::tie(code, msg) = getErrorCode(0, NSM_ERROR, 0x88);
    EXPECT_EQ(code, NSM_ERROR);
    EXPECT_EQ(msg, "Nonce Mismatch");

    std::tie(code, msg) = getErrorCode(0, NSM_ERROR, 0x89);
    EXPECT_EQ(code, NSM_ERROR);
    EXPECT_EQ(msg, "Debug Token Installed");

    std::tie(code, msg) = getErrorCode(0, NSM_ERROR, 0x8A);
    EXPECT_EQ(code, NSM_ERROR);
    EXPECT_EQ(msg, "Firmware Update or Background Copy InProgress");

    std::tie(code, msg) = getErrorCode(0, NSM_ERROR, 0x8B);
    EXPECT_EQ(code, NSM_ERROR);
    EXPECT_EQ(msg, "Firmware Pending Activation");
}

TEST_F(NsmFirmwareUtilsCommonTest, ReasonCodeNotFoundFallsThrough)
{
    // Reason code not in firmwareCommandErrors, should fall through to cc-based
    // logic
    auto [code, msg] = getErrorCode(0, NSM_ERR_INVALID_DATA, 0xFF);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    // Should hit default case in NSM_ERR_INVALID_DATA switch
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

TEST_F(NsmFirmwareUtilsCommonTest, ReasonCodeZeroSkipsMap)
{
    // reasonCode = 0, should skip firmwareCommandErrors lookup
    auto [code, msg] = getErrorCode(0, NSM_ERR_INVALID_DATA, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

TEST_F(NsmFirmwareUtilsCommonTest, CcSuccessSkipsReasonCodePath)
{
    // cc = NSM_SUCCESS, should skip reason code path entirely
    auto [code, msg] = getErrorCode(0, NSM_SUCCESS, 0x86);
    EXPECT_EQ(code, NSM_SUCCESS);
    // Should fall through to final default
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

// ============================================================================
// Tests for NSM_ERR_INVALID_DATA path (switch on commandType)
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, InvalidDataMinSecurityVersion)
{
    auto [code, msg] = getErrorCode(NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER,
                                    NSM_ERR_INVALID_DATA, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(msg, "Invalid MinimumSecurityVersion");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidDataCodeAuthKeyPerm)
{
    auto [code, msg] = getErrorCode(NSM_FW_UPDATE_CODE_AUTH_KEY_PERM,
                                    NSM_ERR_INVALID_DATA, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(msg, "Invalid KeyIndexes");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidDataSetRotProperty)
{
    auto [code, msg] = getErrorCode(NSM_FW_SET_ROT_PROPERTY,
                                    NSM_ERR_INVALID_DATA, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(msg, "Invalid SetRoTProperty");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidDataImageCopyControl)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_DATA, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    EXPECT_EQ(msg, "Invalid Image Copy Control");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidDataUnknownCommandType)
{
    // Unknown command type should hit default case
    auto [code, msg] = getErrorCode(0xFF, NSM_ERR_INVALID_DATA, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
    EXPECT_TRUE(msg.find("cc=") != std::string::npos);
}

// ============================================================================
// Tests for NSM_ERR_UNSUPPORTED_COMMAND_CODE path
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, UnsupportedCommandCode)
{
    auto [code, msg] = getErrorCode(0, NSM_ERR_UNSUPPORTED_COMMAND_CODE, 0);
    EXPECT_EQ(code, NSM_ERR_UNSUPPORTED_COMMAND_CODE);
    EXPECT_EQ(msg, "Unsupported Command Code");
}

// ============================================================================
// Tests for NSM_ERR_INVALID_STATE_FOR_COMMAND path
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateImageCopyNoBootComplete)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_STATE_FOR_COMMAND,
                                    ERR_NO_BOOT_COMPLETE);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_EQ(
        msg, "The RoT has not received a boot complete indication from the AP");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateUpdateInProgress)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_STATE_FOR_COMMAND,
                                    ERR_UPDATE_IN_PROGRESS);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_EQ(msg, "A firmware update is in progress");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateImageCopyInProgress)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_STATE_FOR_COMMAND,
                                    ERR_IMAGE_COPY_IN_PROGRESS);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_EQ(msg, "An image copy is in progress");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateImageCopyCompleted)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_STATE_FOR_COMMAND,
                                    ERR_IMAGE_COPY_COMPLETED);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_EQ(msg, "An image copy was completed successfully");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateFlashWearMitigation)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_STATE_FOR_COMMAND,
                                    ERR_FLASH_WEAR_MITIGATION);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_EQ(msg, "A flash wear out mitigation policy is in effect");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateReasonNotFound)
{
    // Reason code not in imageCopyInvalidStateErrors map
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_STATE_FOR_COMMAND, 0xFFFF);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_EQ(msg, "Invalid State For Command");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidStateUnknownCommandType)
{
    // Command type not NSM_FW_IMAGE_COPY_CONTROL
    auto [code, msg] = getErrorCode(0xFF, NSM_ERR_INVALID_STATE_FOR_COMMAND, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_STATE_FOR_COMMAND);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

// ============================================================================
// Tests for NSM_ERR_INVALID_REQUEST_TYPE path
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, InvalidRequestIncompleteComponentSet)
{
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_REQUEST_TYPE,
                                    ERR_INCOMPLETE_COMPONENT_SET);
    EXPECT_EQ(code, NSM_ERR_INVALID_REQUEST_TYPE);
    EXPECT_TRUE(msg.find("additional components") != std::string::npos);
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidRequestReasonNotFound)
{
    // Reason code not in imageCopyInvalidRequestErrors map
    auto [code, msg] = getErrorCode(NSM_FW_IMAGE_COPY_CONTROL,
                                    NSM_ERR_INVALID_REQUEST_TYPE, 0xFFFF);
    EXPECT_EQ(code, NSM_ERR_INVALID_REQUEST_TYPE);
    EXPECT_EQ(msg, "Invalid Request Type");
}

TEST_F(NsmFirmwareUtilsCommonTest, InvalidRequestUnknownCommandType)
{
    // Command type not NSM_FW_IMAGE_COPY_CONTROL
    auto [code, msg] = getErrorCode(0xFF, NSM_ERR_INVALID_REQUEST_TYPE, 0);
    EXPECT_EQ(code, NSM_ERR_INVALID_REQUEST_TYPE);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

// ============================================================================
// Tests for default/fallthrough path
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, UnknownErrorCodeDefaultPath)
{
    // CC not matching any specific case, should hit final default
    auto [code, msg] = getErrorCode(0, 0xDEAD, 0);
    EXPECT_EQ(code, 0xDEAD);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
    EXPECT_TRUE(msg.find("cc=") != std::string::npos);
    EXPECT_TRUE(msg.find("reason_code=") != std::string::npos);
}

TEST_F(NsmFirmwareUtilsCommonTest, UnknownErrorWithReasonCode)
{
    // Unknown CC with reason code
    auto [code, msg] = getErrorCode(0xFF, 0xBEEF, 0x1234);
    EXPECT_EQ(code, 0xBEEF);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
    EXPECT_TRUE(msg.find("cc=") != std::string::npos);
    EXPECT_TRUE(msg.find("reason_code=4660") !=
                std::string::npos); // 0x1234 = 4660
}

// ============================================================================
// Edge case tests
// ============================================================================

TEST_F(NsmFirmwareUtilsCommonTest, AllZeroInputs)
{
    auto [code, msg] = getErrorCode(0, 0, 0);
    EXPECT_EQ(code, 0);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

TEST_F(NsmFirmwareUtilsCommonTest, MaxValueInputs)
{
    auto [code, msg] = getErrorCode(0xFF, 0xFFFF, 0xFFFF);
    EXPECT_EQ(code, 0xFFFF);
    EXPECT_TRUE(msg.find("Unknown Error") != std::string::npos);
}

TEST_F(NsmFirmwareUtilsCommonTest, ReasonCodeTakesPrecedenceOverCc)
{
    // When both reason code and cc-specific logic could apply,
    // reason code should take precedence
    auto [code, msg] = getErrorCode(NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER,
                                    NSM_ERR_INVALID_DATA, 0x86);
    EXPECT_EQ(code, NSM_ERR_INVALID_DATA);
    // Should return firmwareCommandErrors message (reason code path)
    EXPECT_EQ(msg, "EFUSE Update Failed");
}
