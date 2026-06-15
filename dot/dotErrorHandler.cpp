/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#include "dotErrorHandler.hpp"

#include "base.h"

#include <map>
#include <sstream>

namespace nsm
{
namespace dot
{

// Static map for DOT reason code to error message
static const std::map<uint16_t, std::string> reasonCodeMap = {
    {DOT_RC_INTERNAL_ERROR, "DOTInternalError"},
    {DOT_RC_STATE_INVALID, "DOTStateInvalid"},
    {DOT_RC_SIGNATURE_VERIFICATION_FAILED, "DOTSignatureVerificationFailed"},
    {DOT_RC_STORAGE_ERROR, "DOTStorageError"},
    {DOT_RC_LOCK_DISABLED, "DOTLockDisabled"},
    {DOT_RC_KEY_MISMATCH, "DOTKeyMismatch"},
    {DOT_RC_INVALID_UNLOCK_TYPE, "DOTInvalidUnlockType"},
    {DOT_RC_INVALID_UNLOCK_METHOD, "DOTInvalidUnlockMethod"},
    {DOT_RC_CRYPTO_ERROR, "DOTCryptoError"},
    {DOT_RC_BLOB_CREATION_FAILED, "DOTBlobCreationFailed"},
    {DOT_RC_INVALID_STATE_FOR_LOCK, "DOTInvalidStateForLock"},
    {DOT_RC_INVALID_STATE_FOR_UNLOCK, "DOTInvalidStateForUnlock"},
    {DOT_RC_INVALID_STATE_FOR_DISABLE, "DOTInvalidStateForDisable"},
    {DOT_RC_INVALID_STATE_FOR_CAK_ROTATE, "DOTInvalidStateForCakRotate"},
    {DOT_RC_INVALID_STATE_FOR_CAK_INSTALL, "DOTInvalidStateForCakInstall"},
    {DOT_RC_NULL_POINTER, "DOTNullPointer"},
    {DOT_RC_FUSE_INCREMENT_FAILED, "DOTFuseIncrementFailed"},
    {DOT_RC_RECOVERY_FAILED, "DOTRecoveryFailed"},
    {DOT_RC_INVALID_COMMAND, "DOTInvalidCommand"},
    {DOT_CAK_INSTALL_UNSUPPORTED_IN_RUNTIME,
     "DOTCakInstallUnsupportedInRuntime"},
    {DOT_CAK_INSTALL_VERIFY_FAILED, "DOTCakInstallVerifyFailed"},
    {DOT_RC_UNSUPPORTED_COMMAND, "DOTUnsupportedCommand"},
    {DOT_RC_INVALID_LENGTH, "DOTInvalidDataLength"}};

// Static map for NSM completion code to error message
static const std::map<uint8_t, std::string> completionCodeMap = {
    {NSM_ERR_INVALID_DATA, "Invalid data provided for DOT operation"},
    {NSM_ERR_INVALID_DATA_LENGTH, "Invalid data length for DOT operation"},
    {NSM_ERR_NOT_READY, "Device not ready for DOT operation"},
    {NSM_ERR_UNSUPPORTED_COMMAND_CODE, "Unsupported DOT command"}};

// Convert DOT operation enum to human-readable string
static const char* dotOperationToString(nsm_firmware_commands op)
{
    switch (op)
    {
        case NSM_FW_DOT_CAK_INSTALL:
            return "DOT CAK Install";
        case NSM_FW_DOT_CAK_BYPASS:
            return "DOT CAK Bypass";
        case NSM_FW_DOT_CAK_ROTATE:
            return "DOT CAK Rotate";
        case NSM_FW_DOT_LOCK:
            return "DOT Lock";
        case NSM_FW_DOT_UNLOCK_CHALLENGE:
            return "DOT Unlock Challenge";
        case NSM_FW_DOT_UNLOCK:
            return "DOT Unlock";
        case NSM_FW_DOT_GET_INFO:
            return "DOT Get Info";
        case NSM_FW_DOT_GET_STATUS:
            return "DOT Get Status";
        case NSM_FW_DOT_DISABLE:
            return "DOT Disable";
        case NSM_FW_DOT_OVERRIDE:
            return "DOT Override";
        case NSM_FW_DOT_RECOVERY:
            return "DOT Recovery";
        default:
            return "DOT Unknown Operation";
    }
}

std::tuple<uint16_t, std::string>
    formatDotDeviceError(uint8_t cc, uint16_t reasonCode,
                         nsm_firmware_commands operation)
{
    std::string errorMsg;
    const char* opName = dotOperationToString(operation);

    // Look up reason code first (more specific)
    auto reasonIt = reasonCodeMap.find(reasonCode);
    if (reasonIt != reasonCodeMap.end())
    {
        errorMsg = reasonIt->second;
    }
    else
    {
        // Fallback to completion code mapping
        if (cc == NSM_SUCCESS)
        {
            errorMsg = std::string(opName) + " completed successfully.";
        }
        else if (cc == NSM_ERROR)
        {
            errorMsg = "An unexpected internal failure occurred while "
                       "processing the DOT request";
            if (reasonCode != 0)
            {
                errorMsg += ", reason code: " + std::to_string(reasonCode);
            }
        }
        else
        {
            auto ccIt = completionCodeMap.find(cc);
            if (ccIt != completionCodeMap.end())
            {
                errorMsg = ccIt->second;
            }
            else
            {
                errorMsg =
                    std::string(opName) +
                    " failed with completion code: " + std::to_string(cc);
                if (reasonCode != 0)
                {
                    errorMsg += ", reason code: " + std::to_string(reasonCode);
                }
            }
        }
    }

    return std::make_tuple(reasonCode, errorMsg);
}

} // namespace dot
} // namespace nsm
