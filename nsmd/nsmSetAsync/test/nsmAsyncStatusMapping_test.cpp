/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

// Unit tests for mapNsmCompletionToAsyncStatus (nsmAsyncStatusMapping.hpp):
// the full (swRc, cc, reasonCode) -> AsyncOperationStatus matrix and its
// evaluation order (software code, then specific completion code, then the
// reason code refining a generic NSM_ERROR).

#include "nsmAsyncStatusMapping.hpp"

#include <gtest/gtest.h>

using nsm::AsyncOperationStatusType;
using nsm::mapNsmCompletionToAsyncStatus;

// --- software return code branches

TEST(nsmAsyncStatusMapping, SwRcTimeout_MapsToTimeout)
{
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_ERROR_TIMEOUT, NSM_SUCCESS,
                                            ERR_NULL),
              AsyncOperationStatusType::Timeout);
}

TEST(nsmAsyncStatusMapping, SwRcGenericError_MapsToUnavailable)
{
    EXPECT_EQ(
        mapNsmCompletionToAsyncStatus(NSM_SW_ERROR, NSM_SUCCESS, ERR_NULL),
        AsyncOperationStatusType::Unavailable);
}

TEST(nsmAsyncStatusMapping, SwRcDecodeFailures_MapToInternalFailure)
{
    for (auto swRc : {NSM_SW_ERROR_DATA, NSM_SW_ERROR_LENGTH, NSM_SW_ERROR_NULL,
                      NSM_SW_ERROR_COMMAND_FAIL})
    {
        EXPECT_EQ(mapNsmCompletionToAsyncStatus(swRc, NSM_SUCCESS, ERR_NULL),
                  AsyncOperationStatusType::InternalFailure)
            << "swRc=" << swRc;
    }
    // An unknown software code is an internal failure as well.
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(0x7F, NSM_SUCCESS, ERR_NULL),
              AsyncOperationStatusType::InternalFailure);
}

TEST(nsmAsyncStatusMapping, SwRcEvaluatedBeforeCompletionCode)
{
    // A transport failure wins over whatever the (stale) cc / reason say.
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_ERROR_TIMEOUT,
                                            NSM_ERR_INVALID_DATA, ERR_NULL),
              AsyncOperationStatusType::Timeout);
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_ERROR, NSM_ERROR,
                                            ERR_NOT_SUPPORTED),
              AsyncOperationStatusType::Unavailable);
}

// --- success

TEST(nsmAsyncStatusMapping, Success_IgnoresReasonCode)
{
    EXPECT_EQ(
        mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_SUCCESS, ERR_NULL),
        AsyncOperationStatusType::Success);
    // A reason code is meaningless on success and must not refine it.
    EXPECT_EQ(
        mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_SUCCESS, ERR_TIMEOUT),
        AsyncOperationStatusType::Success);
}

// --- specific completion codes

TEST(nsmAsyncStatusMapping, CcAccepted_MapsToInProgress)
{
    EXPECT_EQ(
        mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_ACCEPTED, ERR_NULL),
        AsyncOperationStatusType::InProgress);
}

TEST(nsmAsyncStatusMapping, CcBusyFamily_MapsToUnavailable)
{
    for (auto cc : {NSM_BUSY, NSM_ERR_NOT_READY, NSM_ERR_BUS_ACCESS,
                    NSM_ERR_INVALID_STATE_FOR_COMMAND})
    {
        EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, cc, ERR_NULL),
                  AsyncOperationStatusType::Unavailable)
            << "cc=" << cc;
    }
}

TEST(nsmAsyncStatusMapping, CcUnsupported_MapsToUnsupportedRequest)
{
    for (auto cc :
         {NSM_ERR_UNSUPPORTED_COMMAND_CODE, NSM_ERR_UNSUPPORTED_MSG_TYPE})
    {
        EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, cc, ERR_NULL),
                  AsyncOperationStatusType::UnsupportedRequest)
            << "cc=" << cc;
    }
}

TEST(nsmAsyncStatusMapping, CcInvalidData_MapsToInvalidArgument)
{
    for (auto cc : {NSM_ERR_INVALID_DATA, NSM_ERR_INVALID_DATA_LENGTH,
                    NSM_ERR_INVALID_REQUEST_TYPE})
    {
        EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, cc, ERR_NULL),
                  AsyncOperationStatusType::InvalidArgument)
            << "cc=" << cc;
    }
}

TEST(nsmAsyncStatusMapping, SpecificCcWinsOverReasonCode)
{
    // The reason code only refines the generic error; a specific cc is
    // final whatever the reason says.
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_BUSY,
                                            ERR_NOT_SUPPORTED),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS,
                                            NSM_ERR_INVALID_DATA, ERR_TIMEOUT),
              AsyncOperationStatusType::InvalidArgument);
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS,
                                            NSM_ERR_UNSUPPORTED_COMMAND_CODE,
                                            ERR_TIMEOUT),
              AsyncOperationStatusType::UnsupportedRequest);
}

// --- generic NSM_ERROR refined by the reason code

TEST(nsmAsyncStatusMapping, NsmErrorReasonCodes_Refined)
{
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR,
                                            ERR_NOT_SUPPORTED),
              AsyncOperationStatusType::UnsupportedRequest);

    for (auto reason : {ERR_TIMEOUT, ERR_DOWNSTREAM_TIMEOUT})
    {
        EXPECT_EQ(
            mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, reason),
            AsyncOperationStatusType::Timeout)
            << "reason=" << reason;
    }

    for (auto reason :
         {ERR_NO_BOOT_COMPLETE, ERR_UPDATE_IN_PROGRESS,
          ERR_IMAGE_COPY_IN_PROGRESS, ERR_FLASH_WEAR_MITIGATION,
          ERR_I2C_NACK_FROM_DEV_ADDR, ERR_I2C_NACK_FROM_DEV_CMD_DATA,
          ERR_I2C_NACK_FROM_DEV_ADDR_RS})
    {
        EXPECT_EQ(
            mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, reason),
            AsyncOperationStatusType::Unavailable)
            << "reason=" << reason;
    }

    for (auto reason :
         {ERR_INVALID_PCI, ERR_INVALID_RQD, ERR_INCOMPLETE_COMPONENT_SET})
    {
        EXPECT_EQ(
            mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, reason),
            AsyncOperationStatusType::InvalidArgument)
            << "reason=" << reason;
    }
}

TEST(nsmAsyncStatusMapping, NsmErrorUnknownReason_MapsToInternalFailure)
{
    for (auto reason :
         {ERR_NULL, ERR_NVLINK_PORT_INVALID, ERR_NVLINK_PORT_DISABLED,
          ERR_PROPERTY_NOT_SUPPORTED, ERR_IMAGE_COPY_COMPLETED})
    {
        EXPECT_EQ(
            mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, NSM_ERROR, reason),
            AsyncOperationStatusType::InternalFailure)
            << "reason=" << reason;
    }
}

TEST(nsmAsyncStatusMapping, UnknownCc_TreatedLikeNsmError)
{
    // A completion code outside the enum falls into the generic branch: the
    // reason code may refine it, otherwise it is an internal failure.
    constexpr uint8_t unknownCc = 0x55;
    EXPECT_EQ(
        mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, unknownCc, ERR_TIMEOUT),
        AsyncOperationStatusType::Timeout);
    EXPECT_EQ(mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, unknownCc,
                                            ERR_NO_BOOT_COMPLETE),
              AsyncOperationStatusType::Unavailable);
    EXPECT_EQ(
        mapNsmCompletionToAsyncStatus(NSM_SW_SUCCESS, unknownCc, ERR_NULL),
        AsyncOperationStatusType::InternalFailure);
}
