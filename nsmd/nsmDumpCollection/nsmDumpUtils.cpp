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

#include "nsmDumpUtils.hpp"

#include "base.h"

#include <cstring>

namespace nsm
{

namespace
{

// Reason-code predicates shared by the AsyncStatus / EraseStatus mappers.
bool reasonIsTimeout(uint16_t reasonCode)
{
    return reasonCode == ERR_TIMEOUT || reasonCode == ERR_DOWNSTREAM_TIMEOUT;
}

bool reasonIsUnsupported(uint16_t reasonCode)
{
    return reasonCode == ERR_NOT_SUPPORTED;
}

bool reasonIsUnavailable(uint16_t reasonCode)
{
    return reasonCode == ERR_NO_BOOT_COMPLETE ||
           reasonCode == ERR_UPDATE_IN_PROGRESS ||
           reasonCode == ERR_IMAGE_COPY_IN_PROGRESS ||
           reasonCode == ERR_FLASH_WEAR_MITIGATION;
}

bool reasonIsI2cNack(uint16_t reasonCode)
{
    return reasonCode == ERR_I2C_NACK_FROM_DEV_ADDR ||
           reasonCode == ERR_I2C_NACK_FROM_DEV_CMD_DATA ||
           reasonCode == ERR_I2C_NACK_FROM_DEV_ADDR_RS;
}

bool reasonIsInvalid(uint16_t reasonCode)
{
    return reasonCode == ERR_INVALID_PCI || reasonCode == ERR_INVALID_RQD ||
           reasonCode == ERR_INCOMPLETE_COMPONENT_SET;
}

} // namespace

AsyncOperationStatusType mapNsmErrorToAsyncStatus(int32_t swRc, uint8_t cc,
                                                  uint16_t reasonCode)
{
    // Maps a device-reported triple only. WriteFailure / ConflictingOperation
    // / ResourceNotFound are BMC-side and set directly by the handlers.
    // Evaluation order: swRc -> cc -> reasonCode -> InternalFailure.
    if (swRc != NSM_SW_SUCCESS)
    {
        switch (swRc)
        {
            case NSM_SW_ERROR_TIMEOUT:
                return AsyncOperationStatusType::Timeout;
            case NSM_SW_ERROR:
                return AsyncOperationStatusType::Unavailable;
            case NSM_SW_ERROR_DATA:
            case NSM_SW_ERROR_LENGTH:
            case NSM_SW_ERROR_NULL:
            case NSM_SW_ERROR_COMMAND_FAIL:
            default:
                return AsyncOperationStatusType::InternalFailure;
        }
    }

    if (cc == NSM_SUCCESS)
    {
        return AsyncOperationStatusType::Success;
    }

    // Reason codes are the most specific signal the device gives us and
    // take precedence over cc when set. Unknown reason codes fall
    // through to the cc-based mapping below.
    if (reasonCode != ERR_NULL)
    {
        if (reasonIsUnsupported(reasonCode))
        {
            return AsyncOperationStatusType::UnsupportedRequest;
        }
        if (reasonIsTimeout(reasonCode))
        {
            return AsyncOperationStatusType::Timeout;
        }
        if (reasonIsUnavailable(reasonCode))
        {
            return AsyncOperationStatusType::Unavailable;
        }
        if (reasonIsI2cNack(reasonCode))
        {
            return AsyncOperationStatusType::Unavailable;
        }
        if (reasonIsInvalid(reasonCode))
        {
            return AsyncOperationStatusType::InvalidArgument;
        }
    }

    switch (cc)
    {
        case NSM_ACCEPTED:
            return AsyncOperationStatusType::InProgress;
        case NSM_BUSY:
        case NSM_ERR_NOT_READY:
        case NSM_ERR_BUS_ACCESS:
        case NSM_ERR_INVALID_STATE_FOR_COMMAND:
            return AsyncOperationStatusType::Unavailable;
        case NSM_ERR_UNSUPPORTED_COMMAND_CODE:
        case NSM_ERR_UNSUPPORTED_MSG_TYPE:
            return AsyncOperationStatusType::UnsupportedRequest;
        case NSM_ERR_INVALID_DATA:
        case NSM_ERR_INVALID_DATA_LENGTH:
        case NSM_ERR_INVALID_REQUEST_TYPE:
            return AsyncOperationStatusType::InvalidArgument;
        case NSM_ERROR:
        default:
            return AsyncOperationStatusType::InternalFailure;
    }
}

EraseOperationStatus mapNsmErrorToEraseStatus(int32_t swRc, uint8_t cc,
                                              uint16_t reasonCode)
{
    // Erase enum is 1:1 with AsyncOperationStatus; delegate and translate.
    const auto async = mapNsmErrorToAsyncStatus(swRc, cc, reasonCode);
    switch (async)
    {
        case AsyncOperationStatusType::InProgress:
            return EraseOperationStatus::InProgress;
        case AsyncOperationStatusType::Success:
            return EraseOperationStatus::Success;
        case AsyncOperationStatusType::Timeout:
            return EraseOperationStatus::Timeout;
        case AsyncOperationStatusType::Unavailable:
            return EraseOperationStatus::Unavailable;
        case AsyncOperationStatusType::UnsupportedRequest:
            return EraseOperationStatus::UnsupportedRequest;
        case AsyncOperationStatusType::InvalidArgument:
            return EraseOperationStatus::InvalidArgument;
        case AsyncOperationStatusType::WriteFailure:
            return EraseOperationStatus::WriteFailure;
        case AsyncOperationStatusType::ConflictingOperation:
            return EraseOperationStatus::ConflictingOperation;
        case AsyncOperationStatusType::ResourceNotFound:
            return EraseOperationStatus::ResourceNotFound;
        case AsyncOperationStatusType::InternalFailure:
        default:
            return EraseOperationStatus::InternalFailure;
    }
}

uint64_t packNsmError(int32_t swRc, uint8_t cc, uint16_t reasonCode)
{
    // Bit-pattern of swRc through uint32_t avoids implementation-defined
    // sign-extension on the shift. Layout documented in the header.
    const uint64_t swRcBits = static_cast<uint64_t>(static_cast<uint32_t>(swRc))
                              << 32;
    const uint64_t reasonBits = static_cast<uint64_t>(reasonCode) << 16;
    const uint64_t ccBits = static_cast<uint64_t>(cc) << 8;
    return swRcBits | reasonBits | ccBits;
}

UnpackedNsmError unpackNsmError(uint64_t packed)
{
    UnpackedNsmError out{};
    // memcpy (not static_cast) to portably round-trip a negative swRc.
    const uint32_t swRcBits = static_cast<uint32_t>(packed >> 32);
    std::memcpy(&out.swRc, &swRcBits, sizeof(out.swRc));
    out.reasonCode = static_cast<uint16_t>((packed >> 16) & 0xFFFFU);
    out.cc = static_cast<uint8_t>((packed >> 8) & 0xFFU);
    return out;
}

} // namespace nsm
