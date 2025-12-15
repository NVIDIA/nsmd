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
    {0x8A, "Firmware Update or Background Copy InProgress"},
    {0x8B, "Firmware Pending Activation"},
};

const static std::unordered_map<uint16_t, std::string>
    imageCopyInvalidStateErrors = {
        {ERR_NO_BOOT_COMPLETE,
         "The RoT has not received a boot complete indication from the AP"},
        {ERR_UPDATE_IN_PROGRESS, "A firmware update is in progress"},
        {ERR_IMAGE_COPY_IN_PROGRESS, "An image copy is in progress"},
        {ERR_IMAGE_COPY_COMPLETED, "An image copy was completed successfully"},
        {ERR_FLASH_WEAR_MITIGATION,
         "A flash wear out mitigation policy is in effect"},
};

const static std::unordered_map<uint16_t, std::string>
    imageCopyInvalidRequestErrors = {
        {ERR_INCOMPLETE_COMPONENT_SET,
         "The RoT requires additional components to be included in the "
         "request"},
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
            case NSM_FW_SET_ROT_PROPERTY:
                return {cc, "Invalid SetRoTProperty"};
            case NSM_FW_IMAGE_COPY_CONTROL:
                return {cc, "Invalid Image Copy Control"};
            default:
                return {cc, "Unknown Error: cc=" + std::to_string(cc)};
        }
    }

    if (cc == NSM_ERR_UNSUPPORTED_COMMAND_CODE)
    {
        return {cc, "Unsupported Command Code"};
    }

    if (cc == NSM_ERR_INVALID_STATE_FOR_COMMAND)
    {
        switch (commandType)
        {
            case NSM_FW_IMAGE_COPY_CONTROL:
            {
                if (imageCopyInvalidStateErrors.find(reasonCode) !=
                    imageCopyInvalidStateErrors.end())
                {
                    return {cc, imageCopyInvalidStateErrors.at(reasonCode)};
                }
                return {cc, "Invalid State For Command"};
            }
            default:
                return {cc, "Unknown Error: cc=" + std::to_string(cc)};
        }
    }

    if (cc == NSM_ERR_INVALID_REQUEST_TYPE)
    {
        switch (commandType)
        {
            case NSM_FW_IMAGE_COPY_CONTROL:
            {
                if (imageCopyInvalidRequestErrors.find(reasonCode) !=
                    imageCopyInvalidRequestErrors.end())
                {
                    return {cc, imageCopyInvalidRequestErrors.at(reasonCode)};
                }
                return {cc, "Invalid Request Type"};
            }
            default:
                return {cc, "Unknown Error: cc=" + std::to_string(cc)};
        }
    }

    // else: At present there are no specific errors for other cc
    return {cc, "Unknown Error: cc=" + std::to_string(cc) +
                    " reason_code=" + std::to_string(reasonCode)};
}

} // namespace nsm
