/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/*
 * Branch coverage tests for dot/ directory:
 *   - dotErrorHandler.cpp — additional formatDotDeviceError paths
 *   - nsmDotUtils.cpp — BIOPtr, decodeBase64, decodeHex, decodeKeyData,
 *     buildKeyAuthData, decodePEMKey additional branches
 */

#include "base.h"

#include "dotErrorHandler.hpp"
#include "nsmDotUtils.hpp"

#include <openssl/bio.h>
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <cstring>
#include <memory>

#include <gtest/gtest.h>

using namespace nsm::dot;

// ============================================================================
// BIOPtr — destructor with null bio (no crash)
// ============================================================================

TEST(BIOPtrBranch, ConstructWithNull)
{
    BIOPtr ptr(nullptr);
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get(), nullptr);
    // Destructor should not crash when bio_ is null
}

TEST(BIOPtrBranch, MoveFromValid_LeavesSourceNull)
{
    BIO* bio = BIO_new(BIO_s_mem());
    ASSERT_NE(bio, nullptr);
    BIOPtr src(bio);
    BIOPtr dest(std::move(src));
    EXPECT_FALSE(src);
    EXPECT_TRUE(dest);
    EXPECT_EQ(dest.get(), bio);
}

TEST(BIOPtrBranch, MoveAssignToNull)
{
    BIO* bio = BIO_new(BIO_s_mem());
    ASSERT_NE(bio, nullptr);
    BIOPtr src(bio);
    BIOPtr dest(nullptr); // dest.bio_ is null
    dest = std::move(src);
    EXPECT_TRUE(dest);
    EXPECT_FALSE(src);
}

TEST(BIOPtrBranch, MoveAssignBothValid)
{
    BIO* bio1 = BIO_new(BIO_s_mem());
    BIO* bio2 = BIO_new(BIO_s_mem());
    ASSERT_NE(bio1, nullptr);
    ASSERT_NE(bio2, nullptr);

    BIOPtr ptr1(bio1);
    BIOPtr ptr2(bio2);
    ptr1 = std::move(ptr2);
    EXPECT_TRUE(ptr1);
    EXPECT_EQ(ptr1.get(), bio2);
    EXPECT_FALSE(ptr2);
}

// ============================================================================
// decodeBase64 — edge cases
// ============================================================================

TEST(DecodeBase64Branch, ValidShortDecode)
{
    // "AQID" is base64 for bytes {0x01, 0x02, 0x03}
    uint8_t output[3] = {};
    EXPECT_TRUE(decodeBase64("AQID", output, 3));
    EXPECT_EQ(output[0], 0x01);
    EXPECT_EQ(output[1], 0x02);
    EXPECT_EQ(output[2], 0x03);
}

TEST(DecodeBase64Branch, WrongExpectedSize_ReturnsFalse)
{
    // "AQID" decodes to 3 bytes, but requesting 2
    uint8_t output[2] = {};
    EXPECT_FALSE(decodeBase64("AQID", output, 2));
}

TEST(DecodeBase64Branch, NullOutput_ReturnsFalse)
{
    EXPECT_FALSE(decodeBase64("AQID", nullptr, 3));
}

TEST(DecodeBase64Branch, ZeroExpectedSize_ReturnsFalse)
{
    uint8_t output[1] = {};
    EXPECT_FALSE(decodeBase64("AQID", output, 0));
}

TEST(DecodeBase64Branch, InvalidBase64Data_ReturnsFalse)
{
    uint8_t output[3] = {};
    // Completely invalid base64
    EXPECT_FALSE(decodeBase64("!@#$", output, 3));
}

// ============================================================================
// decodeHex — edge cases
// ============================================================================

TEST(DecodeHexBranch, SingleByte)
{
    uint8_t output[1] = {};
    EXPECT_TRUE(decodeHex("ab", output, 1));
    EXPECT_EQ(output[0], 0xAB);
}

TEST(DecodeHexBranch, UpperCaseHex)
{
    uint8_t output[2] = {};
    EXPECT_TRUE(decodeHex("ABCD", output, 2));
    EXPECT_EQ(output[0], 0xAB);
    EXPECT_EQ(output[1], 0xCD);
}

TEST(DecodeHexBranch, MixedCaseHex)
{
    uint8_t output[3] = {};
    EXPECT_TRUE(decodeHex("aAbBcC", output, 3));
    EXPECT_EQ(output[0], 0xAA);
    EXPECT_EQ(output[1], 0xBB);
    EXPECT_EQ(output[2], 0xCC);
}

TEST(DecodeHexBranch, InvalidCharInMiddle)
{
    uint8_t output[3] = {};
    // 6 chars for 3 bytes, but 'zz' is invalid
    EXPECT_FALSE(decodeHex("01zz03", output, 3));
}

// ============================================================================
// decodeKeyData — fallthrough paths
// ============================================================================

TEST(DecodeKeyDataBranch, ZeroExpectedSize_ReturnsFalse)
{
    uint8_t output[1] = {};
    EXPECT_FALSE(decodeKeyData("test", output, 0));
}

TEST(DecodeKeyDataBranch, AllDecodersFail_ReturnsFalse)
{
    uint8_t output[10] = {};
    // Not PEM, not valid base64 for 10 bytes, not valid hex for 10 bytes
    EXPECT_FALSE(decodeKeyData("short", output, 10));
}

TEST(DecodeKeyDataBranch, HexFallback_Success)
{
    uint8_t output[4] = {};
    // "01020304" is valid hex for 4 bytes
    // Not PEM (no header), not valid base64 for 4 bytes (decodes to 6 bytes)
    EXPECT_TRUE(decodeKeyData("01020304", output, 4));
    EXPECT_EQ(output[0], 0x01);
    EXPECT_EQ(output[1], 0x02);
    EXPECT_EQ(output[2], 0x03);
    EXPECT_EQ(output[3], 0x04);
}

// ============================================================================
// buildKeyAuthData — edge cases
// ============================================================================

TEST(BuildKeyAuthDataBranch, AuthSchemeZero)
{
    uint8_t ecdsa[ECDSA_KEY_SIZE] = {};
    uint8_t lms[LMS_KEY_SIZE] = {};
    uint8_t output[KEY_AUTH_DATA_SIZE] = {};

    memset(ecdsa, 0x11, ECDSA_KEY_SIZE);
    memset(lms, 0x22, LMS_KEY_SIZE);

    EXPECT_TRUE(buildKeyAuthData(0, ecdsa, lms, output));

    // Verify auth scheme is 0
    uint32_t scheme;
    memcpy(&scheme, output, sizeof(scheme));
    EXPECT_EQ(scheme, 0u);

    // Verify ecdsa data
    EXPECT_EQ(output[AUTH_SCHEME_SIZE], 0x11);
    // Verify lms data
    EXPECT_EQ(output[AUTH_SCHEME_SIZE + ECDSA_KEY_SIZE], 0x22);
}

TEST(BuildKeyAuthDataBranch, AllNullPointers)
{
    EXPECT_FALSE(buildKeyAuthData(0, nullptr, nullptr, nullptr));
}

// ============================================================================
// decodePEMKey — non-PEM format (no markers)
// ============================================================================

TEST(DecodePEMKeyBranch, JustRandomText_ReturnsFalse)
{
    uint8_t output[ECDSA_KEY_SIZE] = {};
    EXPECT_FALSE(decodePEMKey("random text without PEM markers", output,
                              ECDSA_KEY_SIZE));
}

TEST(DecodePEMKeyBranch, EmptyBetweenMarkers_ReturnsFalse)
{
    uint8_t output[ECDSA_KEY_SIZE] = {};
    std::string pem = "-----BEGIN PUBLIC KEY-----\n"
                      "\n"
                      "-----END PUBLIC KEY-----\n";
    EXPECT_FALSE(decodePEMKey(pem, output, ECDSA_KEY_SIZE));
}

// ============================================================================
// formatDotDeviceError — additional branch combinations
// ============================================================================

TEST(FormatDotDeviceErrorBranch, SuccessWithKnownOperation)
{
    auto [code, msg] = formatDotDeviceError(NSM_SUCCESS, 0,
                                            NSM_FW_DOT_CAK_INSTALL);
    EXPECT_EQ(code, 0);
    EXPECT_NE(msg.find("DOT CAK Install"), std::string::npos);
    EXPECT_NE(msg.find("completed successfully"), std::string::npos);
}

TEST(FormatDotDeviceErrorBranch, ErrorWithZeroReasonCode)
{
    auto [code, msg] = formatDotDeviceError(NSM_ERROR, 0,
                                            NSM_FW_DOT_CAK_BYPASS);
    EXPECT_EQ(code, 0);
    EXPECT_NE(msg.find("unexpected internal failure"), std::string::npos);
    // Should NOT contain "reason code" since it is 0
    EXPECT_EQ(msg.find("reason code"), std::string::npos);
}

TEST(FormatDotDeviceErrorBranch, ErrorWithNonZeroReasonCode_NotInMap)
{
    // reasonCode 9999 is not in reasonCodeMap
    auto [code, msg] = formatDotDeviceError(NSM_ERROR, 9999, NSM_FW_DOT_LOCK);
    EXPECT_EQ(code, 9999);
    EXPECT_NE(msg.find("reason code"), std::string::npos);
}

TEST(FormatDotDeviceErrorBranch, CompletionCodeNotInMap_NoReasonCode)
{
    // cc=0xFE is not in completionCodeMap, reasonCode=0
    auto [code, msg] = formatDotDeviceError(0xFE, 0,
                                            NSM_FW_DOT_UNLOCK_CHALLENGE);
    EXPECT_EQ(code, 0);
    EXPECT_NE(msg.find("failed with completion code"), std::string::npos);
    EXPECT_EQ(msg.find("reason code"), std::string::npos);
}

TEST(FormatDotDeviceErrorBranch, CompletionCodeNotInMap_WithReasonCode)
{
    // cc=0xFE is not in completionCodeMap, reasonCode=42
    auto [code, msg] = formatDotDeviceError(0xFE, 42, NSM_FW_DOT_CAK_ROTATE);
    EXPECT_EQ(code, 42);
    EXPECT_NE(msg.find("failed with completion code"), std::string::npos);
    EXPECT_NE(msg.find("reason code"), std::string::npos);
}

TEST(FormatDotDeviceErrorBranch, KnownReasonCode_OverridesCompletionCode)
{
    // Even with NSM_SUCCESS as cc, if reasonCode is in the map it takes
    // priority
    auto [code, msg] = formatDotDeviceError(NSM_SUCCESS, DOT_RC_STORAGE_ERROR,
                                            NSM_FW_DOT_LOCK);
    EXPECT_EQ(code, DOT_RC_STORAGE_ERROR);
    EXPECT_EQ(msg, "DOTStorageError");
}

TEST(FormatDotDeviceErrorBranch, AllReasonCodesAreStrings)
{
    // Test all remaining reason codes not yet tested individually
    auto [code1, msg1] = formatDotDeviceError(NSM_ERROR, DOT_RC_LOCK_DISABLED,
                                              NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg1, "DOTLockDisabled");

    auto [code2, msg2] = formatDotDeviceError(NSM_ERROR, DOT_RC_KEY_MISMATCH,
                                              NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg2, "DOTKeyMismatch");

    auto [code3, msg3] = formatDotDeviceError(
        NSM_ERROR, DOT_RC_INVALID_UNLOCK_TYPE, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg3, "DOTInvalidUnlockType");

    auto [code4, msg4] = formatDotDeviceError(
        NSM_ERROR, DOT_RC_INVALID_UNLOCK_METHOD, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg4, "DOTInvalidUnlockMethod");

    auto [code5, msg5] = formatDotDeviceError(NSM_ERROR, DOT_RC_CRYPTO_ERROR,
                                              NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg5, "DOTCryptoError");

    auto [code6, msg6] = formatDotDeviceError(
        NSM_ERROR, DOT_RC_BLOB_CREATION_FAILED, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg6, "DOTBlobCreationFailed");

    auto [code7, msg7] = formatDotDeviceError(NSM_ERROR, DOT_RC_NULL_POINTER,
                                              NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg7, "DOTNullPointer");

    auto [code8, msg8] = formatDotDeviceError(
        NSM_ERROR, DOT_RC_FUSE_INCREMENT_FAILED, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg8, "DOTFuseIncrementFailed");

    auto [code9, msg9] = formatDotDeviceError(NSM_ERROR, DOT_RC_RECOVERY_FAILED,
                                              NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg9, "DOTRecoveryFailed");

    auto [code10, msg10] = formatDotDeviceError(
        NSM_ERROR, DOT_RC_INVALID_COMMAND, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg10, "DOTInvalidCommand");

    auto [code11, msg11] = formatDotDeviceError(
        NSM_ERROR, DOT_RC_UNSUPPORTED_COMMAND, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg11, "DOTUnsupportedCommand");

    auto [code12, msg12] =
        formatDotDeviceError(NSM_ERROR, DOT_RC_INVALID_LENGTH, NSM_FW_DOT_LOCK);
    EXPECT_EQ(msg12, "DOTInvalidDataLength");
}

// ============================================================================
// dotOperationToString — all operations via formatDotDeviceError
// ============================================================================

TEST(DotOperationToStringBranch, AllOperations)
{
    // Each operation should produce a different string in the success message
    auto check = [](nsm_firmware_commands op, const char* expectedSubstr) {
        auto [code, msg] = formatDotDeviceError(NSM_SUCCESS, 0, op);
        EXPECT_NE(msg.find(expectedSubstr), std::string::npos)
            << "Expected '" << expectedSubstr << "' in: " << msg;
    };

    check(NSM_FW_DOT_CAK_INSTALL, "DOT CAK Install");
    check(NSM_FW_DOT_CAK_BYPASS, "DOT CAK Bypass");
    check(NSM_FW_DOT_CAK_ROTATE, "DOT CAK Rotate");
    check(NSM_FW_DOT_LOCK, "DOT Lock");
    check(NSM_FW_DOT_UNLOCK_CHALLENGE, "DOT Unlock Challenge");
    check(NSM_FW_DOT_UNLOCK, "DOT Unlock");
    check(NSM_FW_DOT_GET_INFO, "DOT Get Info");
    check(NSM_FW_DOT_GET_STATUS, "DOT Get Status");
    check(NSM_FW_DOT_DISABLE, "DOT Disable");
    check(NSM_FW_DOT_OVERRIDE, "DOT Override");
    check(NSM_FW_DOT_RECOVERY, "DOT Recovery");
    check(static_cast<nsm_firmware_commands>(0xFF), "DOT Unknown Operation");
}

// ============================================================================
// completionCodeMap entries via formatDotDeviceError
// ============================================================================

TEST(CompletionCodeMapBranch, AllEntries)
{
    auto [c1, m1] = formatDotDeviceError(NSM_ERR_INVALID_DATA, 0,
                                         NSM_FW_DOT_LOCK);
    EXPECT_EQ(m1, "Invalid data provided for DOT operation");

    auto [c2, m2] = formatDotDeviceError(NSM_ERR_INVALID_DATA_LENGTH, 0,
                                         NSM_FW_DOT_LOCK);
    EXPECT_EQ(m2, "Invalid data length for DOT operation");

    auto [c3, m3] = formatDotDeviceError(NSM_ERR_NOT_READY, 0, NSM_FW_DOT_LOCK);
    EXPECT_EQ(m3, "Device not ready for DOT operation");

    auto [c4, m4] = formatDotDeviceError(NSM_ERR_UNSUPPORTED_COMMAND_CODE, 0,
                                         NSM_FW_DOT_LOCK);
    EXPECT_EQ(m4, "Unsupported DOT command");
}
