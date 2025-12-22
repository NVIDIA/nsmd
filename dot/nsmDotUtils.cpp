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

#include <endian.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <array>
#include <charconv>
#include <cstring>
#include <string_view>
#include <vector>

namespace nsm
{
namespace dot
{

BIOPtr::BIOPtr(BIO* bio) : bio_(bio) {}

BIOPtr::~BIOPtr()
{
    if (bio_)
    {
        BIO_free_all(bio_);
    }
}

BIOPtr::BIOPtr(BIOPtr&& other) noexcept : bio_(other.bio_)
{
    other.bio_ = nullptr;
}

BIOPtr& BIOPtr::operator=(BIOPtr&& other) noexcept
{
    if (this != &other)
    {
        if (bio_)
        {
            BIO_free_all(bio_);
        }
        bio_ = other.bio_;
        other.bio_ = nullptr;
    }
    return *this;
}

BIO* BIOPtr::get() const
{
    return bio_;
}

BIOPtr::operator bool() const
{
    return bio_ != nullptr;
}

bool decodeBase64(const std::string& input, uint8_t* output,
                  size_t expectedSize)
{
    if (input.empty() || !output || expectedSize == 0)
    {
        return false;
    }

    BIO* bio = BIO_new_mem_buf(input.data(), static_cast<int>(input.length()));
    if (!bio)
    {
        return false;
    }

    BIO* b64 = BIO_new(BIO_f_base64());
    if (!b64)
    {
        BIO_free(bio);
        return false;
    }

    BIO* bioChain = BIO_push(b64, bio);
    if (!bioChain)
    {
        BIO_free(b64);
        BIO_free(bio);
        return false;
    }
    BIO_set_flags(bioChain, BIO_FLAGS_BASE64_NO_NL);

    std::vector<uint8_t> decoded(expectedSize * 2);
    int decodedLength = BIO_read(bioChain, decoded.data(), decoded.size());

    BIO_free_all(bioChain);

    if (decodedLength <= 0)
    {
        return false;
    }

    if (static_cast<size_t>(decodedLength) == expectedSize)
    {
        std::memcpy(output, decoded.data(), expectedSize);
        return true;
    }

    return false;
}

bool decodeHex(const std::string& input, uint8_t* output, size_t expectedSize)
{
    if (input.empty() || !output || expectedSize == 0)
    {
        return false;
    }

    if (input.length() != expectedSize * 2)
    {
        return false;
    }

    for (size_t i = 0; i < expectedSize; ++i)
    {
        const char* start = input.data() + (i * 2);
        const char* end = start + 2;
        uint8_t byte;
        auto result = std::from_chars(start, end, byte, 16);
        if (result.ec != std::errc() || result.ptr != end)
        {
            return false;
        }
        output[i] = byte;
    }
    return true;
}

bool decodeKeyData(const std::string& input, uint8_t* output,
                   size_t expectedSize)
{
    if (input.empty() || !output || expectedSize == 0)
    {
        return false;
    }

    if (decodePEMKey(input, output, expectedSize))
    {
        return true;
    }

    if (decodeBase64(input, output, expectedSize))
    {
        return true;
    }

    if (decodeHex(input, output, expectedSize))
    {
        return true;
    }

    return false;
}

bool buildKeyAuthData(uint32_t authScheme, const uint8_t* ecdsaKey,
                      const uint8_t* lmsKey, uint8_t* output)
{
    if (!ecdsaKey || !lmsKey || !output)
    {
        return false;
    }

    uint32_t authSchemeLE = htole32(authScheme);
    std::memcpy(output, &authSchemeLE, AUTH_SCHEME_SIZE);
    std::memcpy(output + AUTH_SCHEME_SIZE, ecdsaKey, ECDSA_KEY_SIZE);
    std::memcpy(output + AUTH_SCHEME_SIZE + ECDSA_KEY_SIZE, lmsKey,
                LMS_KEY_SIZE);
    return true;
}

bool decodePEMKey(const std::string& input, uint8_t* output,
                  size_t expectedSize)
{
    if (input.empty() || !output || expectedSize != ECDSA_KEY_SIZE)
    {
        return false;
    }

    BIOPtr bio{BIO_new_mem_buf(input.data(), static_cast<int>(input.size()))};
    if (!bio)
    {
        return false;
    }

    std::unique_ptr<EVP_PKEY, decltype(&::EVP_PKEY_free)> pubKey{
        PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr),
        &::EVP_PKEY_free};
    if (!pubKey)
    {
        return false;
    }

    if (EVP_PKEY_get_id(pubKey.get()) != EVP_PKEY_EC)
    {
        return false;
    }

    std::unique_ptr<EVP_PKEY_CTX, decltype(&::EVP_PKEY_CTX_free)> ctx{
        EVP_PKEY_CTX_new(pubKey.get(), nullptr), &::EVP_PKEY_CTX_free};
    if (!ctx)
    {
        return false;
    }

    if (EVP_PKEY_public_check(ctx.get()) != 1)
    {
        return false;
    }

    BIGNUM* x = nullptr;
    BIGNUM* y = nullptr;

    if (EVP_PKEY_get_bn_param(pubKey.get(), OSSL_PKEY_PARAM_EC_PUB_X, &x) != 1)
    {
        return false;
    }

    if (EVP_PKEY_get_bn_param(pubKey.get(), OSSL_PKEY_PARAM_EC_PUB_Y, &y) != 1)
    {
        BN_free(x);
        return false;
    }

    std::unique_ptr<BIGNUM, decltype(&::BN_free)> xPtr{x, &::BN_free};
    std::unique_ptr<BIGNUM, decltype(&::BN_free)> yPtr{y, &::BN_free};

    const size_t coordinateSize = expectedSize / 2;

    if (static_cast<size_t>(BN_num_bytes(xPtr.get())) > coordinateSize ||
        static_cast<size_t>(BN_num_bytes(yPtr.get())) > coordinateSize)
    {
        return false;
    }

    if (BN_bn2binpad(xPtr.get(), output, coordinateSize) !=
        static_cast<int>(coordinateSize))
    {
        return false;
    }

    if (BN_bn2binpad(yPtr.get(), output + coordinateSize, coordinateSize) !=
        static_cast<int>(coordinateSize))
    {
        return false;
    }

    return true;
}

} // namespace dot
} // namespace nsm
