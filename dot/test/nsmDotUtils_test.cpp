/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
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

#include "nsmDotUtils.hpp"

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace nsm::dot;

TEST(DecodeKeyDataTest, EmptyInputReturnsfalse)
{
    uint8_t output[96] = {0};
    EXPECT_FALSE(decodeKeyData("", output, 96));
}

TEST(DecodeKeyDataTest, NullOutputReturnsfalse)
{
    EXPECT_FALSE(decodeKeyData("test", nullptr, 96));
}

TEST(DecodeHexTest, ValidHexDecoding)
{
    std::string hexInput = "0123456789abcdef";
    uint8_t output[8] = {0};
    EXPECT_TRUE(decodeHex(hexInput, output, 8));
    EXPECT_EQ(output[0], 0x01);
    EXPECT_EQ(output[1], 0x23);
    EXPECT_EQ(output[2], 0x45);
    EXPECT_EQ(output[3], 0x67);
    EXPECT_EQ(output[4], 0x89);
    EXPECT_EQ(output[5], 0xab);
    EXPECT_EQ(output[6], 0xcd);
    EXPECT_EQ(output[7], 0xef);
}

TEST(DecodeHexTest, InvalidHexLength)
{
    std::string hexInput = "0123456789abcde"; // Odd length
    uint8_t output[8] = {0};
    EXPECT_FALSE(decodeHex(hexInput, output, 8));
}

TEST(DecodeHexTest, InvalidHexCharacters)
{
    std::string hexInput = "01234567xyz";
    uint8_t output[8] = {0};
    EXPECT_FALSE(decodeHex(hexInput, output, 8));
}

TEST(DecodeBase64Test, EmptyInputReturnsfalse)
{
    uint8_t output[96] = {0};
    EXPECT_FALSE(decodeBase64("", output, 96));
}

TEST(BuildKeyAuthDataTest, ValidInput)
{
    uint8_t ecdsaKey[ECDSA_KEY_SIZE] = {0};
    uint8_t lmsKey[LMS_KEY_SIZE] = {0};
    uint8_t output[KEY_AUTH_DATA_SIZE] = {0};

    std::memset(ecdsaKey, 0xAA, ECDSA_KEY_SIZE);
    std::memset(lmsKey, 0xBB, LMS_KEY_SIZE);

    EXPECT_TRUE(buildKeyAuthData(1, ecdsaKey, lmsKey, output));

    uint32_t authScheme;
    std::memcpy(&authScheme, output, AUTH_SCHEME_SIZE);
    EXPECT_EQ(authScheme, 1u);

    EXPECT_EQ(output[AUTH_SCHEME_SIZE], 0xAA);
    EXPECT_EQ(output[AUTH_SCHEME_SIZE + ECDSA_KEY_SIZE], 0xBB);
}

TEST(BuildKeyAuthDataTest, NullEcdsaKey)
{
    uint8_t lmsKey[LMS_KEY_SIZE] = {0};
    uint8_t output[KEY_AUTH_DATA_SIZE] = {0};

    EXPECT_FALSE(buildKeyAuthData(0, nullptr, lmsKey, output));
}

TEST(BuildKeyAuthDataTest, NullLmsKey)
{
    uint8_t ecdsaKey[ECDSA_KEY_SIZE] = {0};
    uint8_t output[KEY_AUTH_DATA_SIZE] = {0};

    EXPECT_FALSE(buildKeyAuthData(0, ecdsaKey, nullptr, output));
}

TEST(BuildKeyAuthDataTest, NullOutput)
{
    uint8_t ecdsaKey[ECDSA_KEY_SIZE] = {0};
    uint8_t lmsKey[LMS_KEY_SIZE] = {0};

    EXPECT_FALSE(buildKeyAuthData(0, ecdsaKey, lmsKey, nullptr));
}

TEST(BuildKeyAuthDataTest, VerifyStructureLayout)
{
    uint8_t ecdsaKey[ECDSA_KEY_SIZE] = {0};
    uint8_t lmsKey[LMS_KEY_SIZE] = {0};
    uint8_t output[KEY_AUTH_DATA_SIZE] = {0};

    for (size_t i = 0; i < ECDSA_KEY_SIZE; i++)
    {
        ecdsaKey[i] = static_cast<uint8_t>(i);
    }
    for (size_t i = 0; i < LMS_KEY_SIZE; i++)
    {
        lmsKey[i] = static_cast<uint8_t>(i + 100);
    }

    uint32_t authScheme = 0x12345678;
    EXPECT_TRUE(buildKeyAuthData(authScheme, ecdsaKey, lmsKey, output));

    uint32_t readAuthScheme;
    std::memcpy(&readAuthScheme, output, AUTH_SCHEME_SIZE);
    EXPECT_EQ(readAuthScheme, authScheme);

    for (size_t i = 0; i < ECDSA_KEY_SIZE; i++)
    {
        EXPECT_EQ(output[AUTH_SCHEME_SIZE + i], static_cast<uint8_t>(i));
    }

    for (size_t i = 0; i < LMS_KEY_SIZE; i++)
    {
        EXPECT_EQ(output[AUTH_SCHEME_SIZE + ECDSA_KEY_SIZE + i],
                  static_cast<uint8_t>(i + 100));
    }
}

TEST(BIOPtrTest, Construction)
{
    BIO* bio = BIO_new(BIO_s_mem());
    ASSERT_NE(bio, nullptr);

    BIOPtr bioPtr(bio);
    EXPECT_TRUE(bioPtr);
    EXPECT_EQ(bioPtr.get(), bio);
}

TEST(BIOPtrTest, MoveConstructor)
{
    BIO* bio = BIO_new(BIO_s_mem());
    ASSERT_NE(bio, nullptr);

    BIOPtr bioPtr1(bio);
    BIOPtr bioPtr2(std::move(bioPtr1));

    EXPECT_FALSE(bioPtr1);
    EXPECT_TRUE(bioPtr2);
    EXPECT_EQ(bioPtr2.get(), bio);
}

TEST(BIOPtrTest, MoveAssignment)
{
    BIO* bio1 = BIO_new(BIO_s_mem());
    BIO* bio2 = BIO_new(BIO_s_mem());
    ASSERT_NE(bio1, nullptr);
    ASSERT_NE(bio2, nullptr);

    BIOPtr bioPtr1(bio1);
    BIOPtr bioPtr2(bio2);

    bioPtr2 = std::move(bioPtr1);

    EXPECT_FALSE(bioPtr1);
    EXPECT_TRUE(bioPtr2);
    EXPECT_EQ(bioPtr2.get(), bio1);
}

TEST(DecodePEMKeyTest, EmptyInputReturnsFalse)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    EXPECT_FALSE(decodePEMKey("", output, ECDSA_KEY_SIZE));
}

TEST(DecodePEMKeyTest, NullOutputReturnsFalse)
{
    std::string pemKey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "DBoqGuZt4QpdbxnQqdTf0axWndLd0Y5DpKNx2ZwANYGXXTBcoW77wBehN9ILJLWi\n"
        "wUrhAKJUqRDqISYCcYKHtGP6V3kM+CyD\n"
        "-----END PUBLIC KEY-----\n";
    EXPECT_FALSE(decodePEMKey(pemKey, nullptr, ECDSA_KEY_SIZE));
}

TEST(DecodePEMKeyTest, ZeroExpectedSizeReturnsFalse)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    std::string pemKey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "DBoqGuZt4QpdbxnQqdTf0axWndLd0Y5DpKNx2ZwANYGXXTBcoW77wBehN9ILJLWi\n"
        "wUrhAKJUqRDqISYCcYKHtGP6V3kM+CyD\n"
        "-----END PUBLIC KEY-----\n";
    EXPECT_FALSE(decodePEMKey(pemKey, output, 0));
}

TEST(DecodePEMKeyTest, WrongExpectedSizeReturnsFalse)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    std::string pemKey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "DBoqGuZt4QpdbxnQqdTf0axWndLd0Y5DpKNx2ZwANYGXXTBcoW77wBehN9ILJLWi\n"
        "wUrhAKJUqRDqISYCcYKHtGP6V3kM+CyD\n"
        "-----END PUBLIC KEY-----\n";
    // Wrong size - should be ECDSA_KEY_SIZE (96), not LMS_KEY_SIZE (48)
    EXPECT_FALSE(decodePEMKey(pemKey, output, LMS_KEY_SIZE));
}

TEST(DecodePEMKeyTest, InvalidPEMFormatMissingBeginMarker)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    std::string invalidPem =
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "-----END PUBLIC KEY-----\n";
    EXPECT_FALSE(decodePEMKey(invalidPem, output, ECDSA_KEY_SIZE));
}

TEST(DecodePEMKeyTest, InvalidPEMFormatMissingEndMarker)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    std::string invalidPem =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n";
    EXPECT_FALSE(decodePEMKey(invalidPem, output, ECDSA_KEY_SIZE));
}

TEST(DecodePEMKeyTest, InvalidPEMFormatMalformed)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    std::string invalidPem = "-----BEGIN PUBLIC KEY-----\n"
                             "INVALID_BASE64_DATA!!!\n"
                             "-----END PUBLIC KEY-----\n";
    EXPECT_FALSE(decodePEMKey(invalidPem, output, ECDSA_KEY_SIZE));
}

TEST(DecodePEMKeyTest, ValidPEMKeyDecodesSuccessfully)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    // Valid P-384 ECDSA public key in PEM format
    std::string pemKey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "DBoqGuZt4QpdbxnQqdTf0axWndLd0Y5DpKNx2ZwANYGXXTBcoW77wBehN9ILJLWi\n"
        "wUrhAKJUqRDqISYCcYKHtGP6V3kM+CyD\n"
        "-----END PUBLIC KEY-----\n";
    EXPECT_TRUE(decodePEMKey(pemKey, output, ECDSA_KEY_SIZE));
    // Verify output is not all zeros (key was decoded)
    bool hasNonZero = false;
    for (size_t i = 0; i < ECDSA_KEY_SIZE; ++i)
    {
        if (output[i] != 0)
        {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST(DecodeKeyDataTest, PEMFormatDelegatesToDecodePEMKey)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    // Valid P-384 ECDSA public key in PEM format
    std::string pemKey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "DBoqGuZt4QpdbxnQqdTf0axWndLd0Y5DpKNx2ZwANYGXXTBcoW77wBehN9ILJLWi\n"
        "wUrhAKJUqRDqISYCcYKHtGP6V3kM+CyD\n"
        "-----END PUBLIC KEY-----\n";
    EXPECT_TRUE(decodeKeyData(pemKey, output, ECDSA_KEY_SIZE));
    // Verify output is not all zeros (key was decoded)
    bool hasNonZero = false;
    for (size_t i = 0; i < ECDSA_KEY_SIZE; ++i)
    {
        if (output[i] != 0)
        {
            hasNonZero = true;
            break;
        }
    }
    EXPECT_TRUE(hasNonZero);
}

TEST(DecodeKeyDataTest, PEMFormatWithWrongSizeReturnsFalse)
{
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    std::string pemKey =
        "-----BEGIN PUBLIC KEY-----\n"
        "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE+kgsKd+pG4sxoxgz74FzMvMGEus5jfJA\n"
        "DBoqGuZt4QpdbxnQqdTf0axWndLd0Y5DpKNx2ZwANYGXXTBcoW77wBehN9ILJLWi\n"
        "wUrhAKJUqRDqISYCcYKHtGP6V3kM+CyD\n"
        "-----END PUBLIC KEY-----\n";
    // Wrong size - should fail
    EXPECT_FALSE(decodeKeyData(pemKey, output, LMS_KEY_SIZE));
}
