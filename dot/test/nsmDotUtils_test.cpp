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
#include <openssl/core_names.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <array>
#include <cstring>
#include <memory>
#include <string>

#include <gtest/gtest.h>

// Helper: generate a PEM public key for the named algorithm/group.
// Returns empty string on failure.
static std::string generatePEMPublicKey(const char* algorithm,
                                        const char* groupName = nullptr)
{
    std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)> kctx{
        EVP_PKEY_CTX_new_from_name(nullptr, algorithm, nullptr),
        &EVP_PKEY_CTX_free};
    if (!kctx || EVP_PKEY_keygen_init(kctx.get()) <= 0)
    {
        return {};
    }
    if (groupName)
    {
        OSSL_PARAM params[] = {
            OSSL_PARAM_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME,
                                   const_cast<char*>(groupName), 0),
            OSSL_PARAM_END};
        if (EVP_PKEY_CTX_set_params(kctx.get(), params) <= 0)
        {
            return {};
        }
    }
    EVP_PKEY* rawKey = nullptr;
    if (EVP_PKEY_keygen(kctx.get(), &rawKey) <= 0 || !rawKey)
    {
        return {};
    }
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey{rawKey,
                                                             &EVP_PKEY_free};
    std::unique_ptr<BIO, decltype(&BIO_free)> bio{BIO_new(BIO_s_mem()),
                                                  &BIO_free};
    if (!bio || PEM_write_bio_PUBKEY(bio.get(), pkey.get()) <= 0)
    {
        return {};
    }
    char* data = nullptr;
    long len = BIO_get_mem_data(bio.get(), &data);
    return {data, static_cast<size_t>(len)};
}

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

// Test to cover line 146 in nsmDotUtils.cpp (hex parsing error with correct
// length)
TEST(DecodeHexTest, InvalidHexCharactersCorrectLength)
{
    // String has correct length (16 chars for 8 bytes) but contains invalid hex
    std::string hexInput = "0123456789abcdgz"; // 'g' and 'z' are invalid hex
    uint8_t output[8] = {0};
    EXPECT_FALSE(decodeHex(hexInput, output, 8));
}

// decodeHex: empty input → TRUE branch of first condition in
// `if (input.empty() || !output || expectedSize == 0)` (line 128)
TEST(DecodeHexTest, EmptyInputReturnsFalse)
{
    uint8_t output[8] = {0};
    EXPECT_FALSE(decodeHex("", output, 8));
}

// decodeHex: null output → second condition TRUE (line 128)
TEST(DecodeHexTest, NullOutputReturnsFalse)
{
    EXPECT_FALSE(decodeHex("0102", nullptr, 1));
}

// decodeHex: zero expectedSize → third condition TRUE (line 128)
TEST(DecodeHexTest, ZeroExpectedSizeReturnsFalse)
{
    uint8_t output[8] = {0};
    EXPECT_FALSE(decodeHex("01", output, 0));
}

TEST(DecodeBase64Test, EmptyInputReturnsfalse)
{
    uint8_t output[96] = {0};
    EXPECT_FALSE(decodeBase64("", output, 96));
}

TEST(DecodeBase64Test, ValidBase64Decoding)
{
    // Test successful base64 decoding to cover lines 117-120
    // "SGVsbG8gV29ybGQh" is base64 for "Hello World!"
    std::string base64Input = "SGVsbG8gV29ybGQh";
    uint8_t output[12] = {0};

    EXPECT_TRUE(decodeBase64(base64Input, output, 12));

    // Verify decoded content
    const char* expected = "Hello World!";
    EXPECT_EQ(std::memcmp(output, expected, 12), 0);
}

TEST(DecodeBase64Test, InvalidBase64Length)
{
    // Test when decoded length doesn't match expected size
    std::string base64Input = "SGVsbG8="; // "Hello" in base64
    uint8_t output[10] = {0};

    // Expecting 10 bytes but will only decode 5
    EXPECT_FALSE(decodeBase64(base64Input, output, 10));
}

TEST(DecodeBase64Test, NullOutputPointer)
{
    std::string base64Input = "SGVsbG8=";
    EXPECT_FALSE(decodeBase64(base64Input, nullptr, 5));
}

TEST(DecodeBase64Test, ZeroExpectedSize)
{
    std::string base64Input = "SGVsbG8=";
    uint8_t output[10] = {0};
    EXPECT_FALSE(decodeBase64(base64Input, output, 0));
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

// BIOPtr self-move-assignment: this == &other → FALSE branch of
// `if (this != &other)` (line 55). bio_ must remain unchanged.
TEST(BIOPtrTest, SelfMoveAssignment)
{
    BIO* bio = BIO_new(BIO_s_mem());
    ASSERT_NE(bio, nullptr);
    BIOPtr bioPtr(bio);

    // Suppress -Wself-move: intentionally testing the self-assignment guard
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    bioPtr = std::move(bioPtr);
#pragma GCC diagnostic pop
    EXPECT_EQ(bioPtr.get(), bio);
}

// BIOPtr move-assign from a moved-from (null) source: this != &other AND
// this->bio_ == nullptr → FALSE branch of `if (bio_)` (line 57).
TEST(BIOPtrTest, MoveAssignFromNullSource)
{
    BIO* bio1 = BIO_new(BIO_s_mem());
    ASSERT_NE(bio1, nullptr);
    BIOPtr src(bio1);

    // Move src into temp to make src null
    BIOPtr temp(std::move(src)); // src.bio_ = nullptr now

    // Create dest also with null bio_
    BIO* bio2 = BIO_new(BIO_s_mem());
    ASSERT_NE(bio2, nullptr);
    BIOPtr dest(bio2);
    BIOPtr dest2(std::move(dest)); // dest.bio_ = nullptr

    // dest.bio_ == nullptr → `if (bio_)` FALSE: skip BIO_free_all
    // Then dest.bio_ = src.bio_ = nullptr
    dest = std::move(src); // both bio_ are nullptr
    EXPECT_FALSE(dest);
    EXPECT_FALSE(src);
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

// decodeKeyData: decodePEMKey fails (no PEM header), decodeBase64 SUCCEEDS →
// TRUE branch of `if (decodeBase64(...))` (line 166).
// 96 zero bytes base64-encoded = 128 'A' characters (no padding needed
// since 96 % 3 == 0); BIO_FLAGS_BASE64_NO_NL mode is used.
TEST(DecodeKeyDataTest, Base64InputSucceeds_CoversLine166)
{
    // 96 zero bytes encoded as base64: each triplet 0x00,0x00,0x00 → "AAAA"
    // 32 groups × "AAAA" = 128 'A' characters
    std::string b64Input(128, 'A');
    uint8_t output[ECDSA_KEY_SIZE] = {};
    EXPECT_TRUE(decodeKeyData(b64Input, output, ECDSA_KEY_SIZE));
    // All bytes should be zero
    for (size_t i = 0; i < ECDSA_KEY_SIZE; i++)
    {
        EXPECT_EQ(output[i], 0x00) << "byte " << i;
    }
}

// decodeKeyData: decodePEMKey fails (no PEM header), decodeBase64 fails
// (wrong decoded size), decodeHex SUCCEEDS →
// TRUE branch of `if (decodeHex(...))` (line 171).
// 96 zero bytes hex-encoded = 192 '0' characters.
// decodeBase64 of "000...000" (192 chars) → 144 bytes decoded ≠ 96 → fails.
TEST(DecodeKeyDataTest, HexInputSucceeds_CoversLine171)
{
    // 96 zero bytes as hex: "000000...000000" (192 '0' chars)
    std::string hexInput(ECDSA_KEY_SIZE * 2, '0');
    uint8_t output[ECDSA_KEY_SIZE] = {};
    EXPECT_TRUE(decodeKeyData(hexInput, output, ECDSA_KEY_SIZE));
    for (size_t i = 0; i < ECDSA_KEY_SIZE; i++)
    {
        EXPECT_EQ(output[i], 0x00) << "byte " << i;
    }
}

// decodePEMKey: RSA public key → EVP_PKEY_get_id() != EVP_PKEY_EC (TRUE
// branch at the non-EC-type check). Covers the early-return path for
// non-elliptic-curve key types.
TEST(DecodePEMKeyTest, RSAKey_NotECType_ReturnsFalse)
{
    std::string rsaPem = generatePEMPublicKey("RSA");
    ASSERT_FALSE(rsaPem.empty()) << "RSA key generation failed";
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    EXPECT_FALSE(decodePEMKey(rsaPem, output, ECDSA_KEY_SIZE));
}

// decodePEMKey: P-521 EC key → coordinates are 66 bytes each, but
// coordinateSize = ECDSA_KEY_SIZE/2 = 48. BN_num_bytes(x) > coordinateSize
// → TRUE branch of the coordinate-size guard (covers the L253 condition).
TEST(DecodePEMKeyTest, P521Key_OversizedCoordinate_ReturnsFalse)
{
    std::string p521Pem = generatePEMPublicKey("EC", "P-521");
    ASSERT_FALSE(p521Pem.empty()) << "P-521 key generation failed";
    uint8_t output[ECDSA_KEY_SIZE] = {0};
    // ECDSA_KEY_SIZE=96 → coordinateSize=48; P-521 has 66-byte coordinates
    EXPECT_FALSE(decodePEMKey(p521Pem, output, ECDSA_KEY_SIZE));
}
