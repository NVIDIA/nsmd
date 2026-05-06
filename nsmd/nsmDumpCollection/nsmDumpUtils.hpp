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

#pragma once

#include "asyncOperationManager.hpp"
#include "nsmEraseTrace.hpp"

#include <cstdint>

namespace nsm
{

// NSM-error-to-AsyncOperationStatus / EraseOperationStatus mapping.
//
// Replaces the historical InternalFailure catch-all with categorized
// statuses (Timeout / Unavailable / UnsupportedRequest / ... ) aligned
// with OpenBMC xyz.openbmc_project.Common.Error and the DMTF Redfish
// Base message vocabulary, so consumers can act on the failure without
// reading the nsmd journal.

// Decoded form of the raw NSM error packed into
// com.nvidia.Async.Value.Value. Lets a consumer recover the original
// device verdict even after the nsmd journal has rotated.
struct UnpackedNsmError
{
    int32_t swRc;        //!< NSM software return code (nsm_sw_codes)
    uint8_t cc;          //!< NSM completion code (nsm_completion_codes)
    uint16_t reasonCode; //!< NSM reason code (nsm_reason_codes)
};

// Map an NSM transport / completion / reason triple to the categorized
// AsyncOperationStatus value published on com.nvidia.Async.Status.Status
// by the asynchronous dump / log handlers.
AsyncOperationStatusType mapNsmErrorToAsyncStatus(int32_t swRc, uint8_t cc,
                                                  uint16_t reasonCode);

// Sister mapper for the Erase status property; same matrix, returns the
// Dump.Erase.OperationStatus value.
EraseOperationStatus mapNsmErrorToEraseStatus(int32_t swRc, uint8_t cc,
                                              uint16_t reasonCode);

// Pack a raw NSM error triple into the 64-bit value carried on
// com.nvidia.Async.Value.Value. Layout MUST stay in sync with the
// nsm-dump-tool consumer:
//   bits  0- 7: reserved (zero)
//   bits  8-15: cc          (uint8_t)
//   bits 16-31: reasonCode  (uint16_t)
//   bits 32-63: swRc        (int32_t)
uint64_t packNsmError(int32_t swRc, uint8_t cc, uint16_t reasonCode);

// Reverse of packNsmError().
UnpackedNsmError unpackNsmError(uint64_t packed);

} // namespace nsm
