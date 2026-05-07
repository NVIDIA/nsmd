/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "debug-token/error.h"

#include <gtest/gtest.h>

using namespace debug_token;

// Test fixture for Error class
class DebugTokenErrorTest : public ::testing::Test
{};

/**
 * Test Error constructor and to_string() for all defined error codes
 */
TEST_F(DebugTokenErrorTest, testErrorCodeToString)
{
    // Test InternalError
    Error internalError(ErrorCode::InternalError);
    EXPECT_EQ(internalError.to_string(), "Internal error");

    // Test InvalidFormat
    Error invalidFormat(ErrorCode::InvalidFormat);
    EXPECT_EQ(invalidFormat.to_string(), "Invalid format");

    // Test SignatureVerificationFailed
    Error sigVerifyFailed(ErrorCode::SignatureVerificationFailed);
    EXPECT_EQ(sigVerifyFailed.to_string(), "Signature verification failed");

    // Test InvalidNonce
    Error invalidNonce(ErrorCode::InvalidNonce);
    EXPECT_EQ(invalidNonce.to_string(), "Invalid nonce");

    // Test InvalidLifecycleState
    Error invalidLifecycle(ErrorCode::InvalidLifecycleState);
    EXPECT_EQ(invalidLifecycle.to_string(), "Invalid lifecycle state");

    // Test UnsupportedType
    Error unsupportedType(ErrorCode::UnsupportedType);
    EXPECT_EQ(unsupportedType.to_string(), "Unsupported type");

    // Test StorageError
    Error storageError(ErrorCode::StorageError);
    EXPECT_EQ(storageError.to_string(), "Storage error");

    // Test RatchetCheckFailed
    Error ratchetCheckFailed(ErrorCode::RatchetCheckFailed);
    EXPECT_EQ(ratchetCheckFailed.to_string(), "Ratchet check failed");

    // Test DeviceInternalError
    Error deviceInternalError(ErrorCode::DeviceInternalError);
    EXPECT_EQ(deviceInternalError.to_string(), "Device internal error");

    // Test FeatureDisabled
    Error featureDisabled(ErrorCode::FeatureDisabled);
    EXPECT_EQ(featureDisabled.to_string(), "Feature disabled");

    // Test FeatureDisabledByPolicy
    Error featureDisabledByPolicy(ErrorCode::FeatureDisabledByPolicy);
    EXPECT_EQ(featureDisabledByPolicy.to_string(),
              "Feature disabled by policy");

    // Test FwVersionMismatch
    Error fwVersionMismatch(ErrorCode::FwVersionMismatch);
    EXPECT_EQ(fwVersionMismatch.to_string(), "Firmware version mismatch");

    // Test InvalidSerialNumber
    Error invalidSerialNumber(ErrorCode::InvalidSerialNumber);
    EXPECT_EQ(invalidSerialNumber.to_string(), "Invalid serial number");

    // Test InvalidPsid
    Error invalidPsid(ErrorCode::InvalidPsid);
    EXPECT_EQ(invalidPsid.to_string(), "Invalid PSID");

    // Test AlreadyInstalled
    Error alreadyInstalled(ErrorCode::AlreadyInstalled);
    EXPECT_EQ(alreadyInstalled.to_string(), "Token already installed");

    // Test NotInstalled
    Error notInstalled(ErrorCode::NotInstalled);
    EXPECT_EQ(notInstalled.to_string(), "Token not installed");

    // Test TokenHashVerificationFailed
    Error tokenHashVerifyFailed(ErrorCode::TokenHashVerificationFailed);
    EXPECT_EQ(tokenHashVerifyFailed.to_string(),
              "Token hash verification failed");

    // Test DebugFirmwareActive
    Error debugFwActive(ErrorCode::DebugFirmwareActive);
    EXPECT_EQ(debugFwActive.to_string(),
              "Token erase not permitted while debug firmware is active");
}

/**
 * Test Error with unknown error code
 */
TEST_F(DebugTokenErrorTest, testUnknownErrorCode)
{
    // Test with an unknown error code (not in the enum)
    uint16_t unknownCode = 0x9999;
    Error unknownError(unknownCode);
    std::string result = unknownError.to_string();

    // Should return formatted string with the unknown code
    EXPECT_NE(result.find("Unknown error code"), std::string::npos);
    EXPECT_NE(result.find("9999"), std::string::npos);
}

/**
 * Test Error constructor with raw hex values
 */
TEST_F(DebugTokenErrorTest, testErrorConstructorWithRawHexValues)
{
    // Test creating Error with raw hex values matching the enum
    Error error1(0x1000); // InternalError
    EXPECT_EQ(error1.to_string(), "Internal error");

    Error error2(0x1002); // SignatureVerificationFailed
    EXPECT_EQ(error2.to_string(), "Signature verification failed");

    Error error3(0x100E); // AlreadyInstalled
    EXPECT_EQ(error3.to_string(), "Token already installed");

    Error error4(0x1010); // TokenHashVerificationFailed
    EXPECT_EQ(error4.to_string(), "Token hash verification failed");

    Error error5(0x1011); // DebugFirmwareActive
    EXPECT_EQ(error5.to_string(),
              "Token erase not permitted while debug firmware is active");
}

/**
 * Test Error with boundary values
 */
TEST_F(DebugTokenErrorTest, testErrorBoundaryValues)
{
    // Test with zero
    Error error0(0x0000);
    std::string result0 = error0.to_string();
    EXPECT_NE(result0.find("Unknown error code"), std::string::npos);

    // Test with max uint16_t
    Error errorMax(0xFFFF);
    std::string resultMax = errorMax.to_string();
    EXPECT_NE(resultMax.find("Unknown error code"), std::string::npos);
    EXPECT_NE(resultMax.find("ffff"), std::string::npos);
}
