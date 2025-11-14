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

#pragma once

#include <openssl/bio.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace nsm
{
namespace dot
{

constexpr size_t KEY_AUTH_DATA_SIZE = 148;
constexpr size_t ECDSA_KEY_SIZE = 96;
constexpr size_t LMS_KEY_SIZE = 48;
constexpr size_t AUTH_SCHEME_SIZE = 4;
constexpr size_t ECDSA_COORDINATE_SIZE = 48;

/**
 * @brief RAII wrapper for OpenSSL BIO objects
 */
class BIOPtr
{
  public:
    explicit BIOPtr(BIO* bio);
    ~BIOPtr();
    BIOPtr(const BIOPtr&) = delete;
    BIOPtr& operator=(const BIOPtr&) = delete;
    BIOPtr(BIOPtr&& other) noexcept;
    BIOPtr& operator=(BIOPtr&& other) noexcept;

    BIO* get() const;
    explicit operator bool() const;

  private:
    BIO* bio_;
};

/**
 * @brief Decode base64-encoded string to binary data
 *
 * @param input Base64-encoded input string
 * @param output Output buffer for decoded data
 * @param expectedSize Expected size of decoded data
 * @return true if decoding succeeded, false otherwise
 */
bool decodeBase64(const std::string& input, uint8_t* output,
                  size_t expectedSize);

/**
 * @brief Decode hex-encoded string to binary data
 *
 * @param input Hex-encoded input string
 * @param output Output buffer for decoded data
 * @param expectedSize Expected size of decoded data
 * @return true if decoding succeeded, false otherwise
 */
bool decodeHex(const std::string& input, uint8_t* output, size_t expectedSize);

/**
 * @brief Decode key data from string (tries base64 first, then hex)
 *
 * @param input Input string (base64 or hex encoded)
 * @param output Output buffer
 * @param expectedSize Expected output size in bytes
 * @return true if decoding succeeded, false otherwise
 */
bool decodeKeyData(const std::string& input, uint8_t* output,
                   size_t expectedSize);

/**
 * @brief Build 148-byte key authentication data structure
 *
 * Structure layout:
 * - auth_scheme: 4 bytes (uint32, little-endian)
 * - ecdsa384_pub_params: 96 bytes (48 bytes X + 48 bytes Y)
 * - lms: 48 bytes
 *
 * @param authScheme Authentication scheme (0: ECDSA only, 1: Hybrid ECDSA+LMS)
 * @param ecdsaKey ECDSA key data (96 bytes: 48 bytes X + 48 bytes Y)
 * @param lmsKey LMS key data (48 bytes, zeros for ECDSA-only)
 * @param output Output buffer (must be at least 148 bytes)
 * @return true if build succeeded, false otherwise
 */
bool buildKeyAuthData(uint32_t authScheme, const uint8_t* ecdsaKey,
                      const uint8_t* lmsKey, uint8_t* output);

} // namespace dot
} // namespace nsm
