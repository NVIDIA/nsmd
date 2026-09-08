// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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

#include "base.h"

#include "asyncOperationManager.hpp"

#include <cstdint>

namespace nsm
{

namespace detail
{

// Reason-code predicates used by mapNsmCompletionToAsyncStatus.
inline bool reasonIsTimeout(uint16_t reasonCode)
{
    return reasonCode == ERR_TIMEOUT || reasonCode == ERR_DOWNSTREAM_TIMEOUT;
}

inline bool reasonIsUnsupported(uint16_t reasonCode)
{
    return reasonCode == ERR_NOT_SUPPORTED;
}

inline bool reasonIsUnavailable(uint16_t reasonCode)
{
    return reasonCode == ERR_NO_BOOT_COMPLETE ||
           reasonCode == ERR_UPDATE_IN_PROGRESS ||
           reasonCode == ERR_IMAGE_COPY_IN_PROGRESS ||
           reasonCode == ERR_FLASH_WEAR_MITIGATION;
}

inline bool reasonIsI2cNack(uint16_t reasonCode)
{
    return reasonCode == ERR_I2C_NACK_FROM_DEV_ADDR ||
           reasonCode == ERR_I2C_NACK_FROM_DEV_CMD_DATA ||
           reasonCode == ERR_I2C_NACK_FROM_DEV_ADDR_RS;
}

inline bool reasonIsInvalid(uint16_t reasonCode)
{
    return reasonCode == ERR_INVALID_PCI || reasonCode == ERR_INVALID_RQD ||
           reasonCode == ERR_INCOMPLETE_COMPONENT_SET;
}

} // namespace detail

/**
 * @brief Shared device-side completion mapping for asynchronous write paths
 *        (0x13 Clear Port Metric State today; reusable by future async
 *        commands).
 *
 * Maps an NSM transport / completion / reason triple to the categorized
 * AsyncOperationStatus published on com.nvidia.Async.Status.Status. Maps a
 * device-reported triple only; WriteFailure / ConflictingOperation /
 * ResourceNotFound are BMC-side and set directly by the handlers.
 *
 * Evaluation order: a software return code is mapped first; then a specific
 * completion code wins (accepted, busy / not ready / bus access / invalid
 * state, unsupported, invalid data); only the generic NSM_ERROR (or an
 * unknown completion code) is refined by the reason code, and anything left
 * is InternalFailure. NSM_ACCEPTED maps to InProgress; callers issuing a
 * command that is not long-running must treat InProgress as a failure.
 *
 * Duplicates the result matrix of nsmDumpCollection/nsmDumpUtils.cpp
 * (mapNsmErrorToAsyncStatus) on purpose so this MR does not modify the
 * optional debug-info module. Note that the dump module's copy still
 * evaluates reason codes before the specific completion codes (its own doc
 * comment says otherwise); it should be aligned to this header in the
 * follow-up consolidation MR that points nsmDumpCollection here.
 *
 * @param[in] swRc - NSM software return code (nsm_sw_codes)
 * @param[in] cc - NSM completion code (nsm_completion_codes)
 * @param[in] reasonCode - NSM reason code (nsm_reason_codes)
 * @return AsyncOperationStatus for the com.nvidia.Async.Status result object
 */
inline AsyncOperationStatusType
    mapNsmCompletionToAsyncStatus(int32_t swRc, uint8_t cc, uint16_t reasonCode)
{
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
            // Generic error: the reason code is the only detail the device
            // gives, so let it refine the status. Unknown reason codes fall
            // through to InternalFailure.
            if (detail::reasonIsUnsupported(reasonCode))
            {
                return AsyncOperationStatusType::UnsupportedRequest;
            }
            if (detail::reasonIsTimeout(reasonCode))
            {
                return AsyncOperationStatusType::Timeout;
            }
            if (detail::reasonIsUnavailable(reasonCode) ||
                detail::reasonIsI2cNack(reasonCode))
            {
                return AsyncOperationStatusType::Unavailable;
            }
            if (detail::reasonIsInvalid(reasonCode))
            {
                return AsyncOperationStatusType::InvalidArgument;
            }
            return AsyncOperationStatusType::InternalFailure;
    }
}

} // namespace nsm
