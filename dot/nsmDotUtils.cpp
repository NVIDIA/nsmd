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
#include <openssl/buffer.h>
#include <openssl/evp.h>

#include <algorithm>
#include <charconv>
#include <cstring>
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

} // namespace dot
} // namespace nsm
