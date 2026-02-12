/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION &
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

#include <cstdint>
#include <format>
#include <map>
#include <stdexcept>
#include <string>

namespace debug_token
{

/**
 * @brief Error codes used throughout the debug token system
 *
 * This enum defines all possible error codes that can be returned by debug
 * token operations. Each error code has a unique hexadecimal value and
 * corresponding human-readable message.
 */
enum ErrorCode
{
    InternalError = 0x1000,
    InvalidFormat = 0x1001,
    SignatureVerificationFailed = 0x1002,
    InvalidNonce = 0x1003,
    InvalidLifecycleState = 0x1004,
    UnsupportedType = 0x1005,
    StorageError = 0x1006,
    RatchetCheckFailed = 0x1007,
    DeviceInternalError = 0x1008,
    FeatureDisabled = 0x1009,
    FeatureDisabledByPolicy = 0x100A,
    FwVersionMismatch = 0x100B,
    InvalidSerialNumber = 0x100C,
    InvalidPsid = 0x100D,
    AlreadyInstalled = 0x100E,
    NotInstalled = 0x100F,
    TokenHashVerificationFailed = 0x1010
};

/**
 * @brief Error class for handling debug token error codes
 *
 * This class provides a convenient way to work with error codes, including
 * conversion to human-readable strings and automatic string conversion.
 */
class Error
{
  public:
    /**
     * @brief Convert the error code to a human-readable string
     *
     * @return String representation of the error code
     */
    std::string to_string() const noexcept
    {
        static const std::map<ErrorCode, std::string> errorMessages{
            {InternalError, "Internal error"},
            {InvalidFormat, "Invalid format"},
            {SignatureVerificationFailed, "Signature verification failed"},
            {InvalidNonce, "Invalid nonce"},
            {InvalidLifecycleState, "Invalid lifecycle state"},
            {UnsupportedType, "Unsupported type"},
            {StorageError, "Storage error"},
            {RatchetCheckFailed, "Ratchet check failed"},
            {DeviceInternalError, "Device internal error"},
            {FeatureDisabled, "Feature disabled"},
            {FeatureDisabledByPolicy, "Feature disabled by policy"},
            {FwVersionMismatch, "Firmware version mismatch"},
            {InvalidSerialNumber, "Invalid serial number"},
            {InvalidPsid, "Invalid PSID"},
            {AlreadyInstalled, "Token already installed"},
            {NotInstalled, "Token not installed"},
            {TokenHashVerificationFailed, "Token hash verification failed"}};
        auto it = errorMessages.find(static_cast<ErrorCode>(code));
        if (it != errorMessages.end())
        {
            return it->second;
        }
        else
        {
            return std::format("Unknown error code: {:04x}", code);
        }
    }

    /**
     * @brief Construct an Error object with the given error code
     *
     * @param code The error code value
     */
    explicit Error(uint16_t code) : code(code) {}

  private:
    uint16_t code;
};

} // namespace debug_token
