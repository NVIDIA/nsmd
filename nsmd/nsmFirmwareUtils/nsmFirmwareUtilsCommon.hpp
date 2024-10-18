/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION &
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
#include "firmware-utils.h"

#include <format>
#include <string>
#include <unordered_map>

namespace nsm
{
const static std::unordered_map<uint16_t, std::string> firmwareCommandErrors = {
    {0x86, "EFUSE Update Failed"},
    {0x87, "IrreversibleConfig Disabled"},
    {0x88, "Nonce Mismatch"},
    {0x89, "Debug Token Installed"},
    {0x8A, "Firmware Update InProgress"},
    {0x8B, "Firmware Pending Activation"},
};

/**
 * @brief Method to get redfish error message based on completion code and
 * reason code.
 *
 * @param commandType
 * @param cc
 * @param reasonCode
 * @return std::tuple<uint16_t, std::string>
 */
inline std::tuple<uint16_t, std::string>
    getErrorCode(uint8_t commandType, uint16_t cc, uint16_t reasonCode = 0)
{
    // Use reason code for the error mapping when it's set since it's command
    // specific
    if (cc != NSM_SUCCESS && reasonCode != 0)
    {
        if (firmwareCommandErrors.find(reasonCode) !=
            firmwareCommandErrors.end())
        {
            return {cc, firmwareCommandErrors.at(reasonCode)};
        }
    }
    // When reason code is None use cc for error mapping
    if (cc == NSM_ERR_INVALID_DATA)
    {
        switch (commandType)
        {
            case NSM_FW_UPDATE_MIN_SECURITY_VERSION_NUMBER:
                return {cc, "Invalid MinimumSecurityVersion"};
            case NSM_FW_UPDATE_CODE_AUTH_KEY_PERM:
                return {cc, "Invalid KeyIndexes"};
            default:
                return {cc, std::format("Unknown Error: cc={}", cc)};
        }
    }
    // else: At present there are no specific errors for other cc
    return {cc,
            std::format("Unknown Error: cc={} reason_code={}", cc, reasonCode)};
}

} // namespace nsm
