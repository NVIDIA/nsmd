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

namespace debug_token
{
namespace types
{
/**
 * @brief Common TLV type identifiers used across all device types
 *
 * These type identifiers are used in TLV (Type-Length-Value) structures
 * for common fields that appear in debug tokens regardless of the specific
 * device type (GPU, NvSwitch, etc.).
 */
enum Common
{
    DeviceType = 0x0001,
    ChallengeNonce = 0x0002,
    DeviceSerialNumber = 0x0003,
    DeviceSerialNumberArray = 0x0004,
    FirmwareVersion = 0x0005,
    AgentVersion = 0x0006,
    LifecycleState = 0x0007,
    TokenIdentifier = 0x0008,
    TokenType = 0x0009,
    TokenConfig = 0x000A,
    NvidiaSignature = 0x000B,
    OemSignature = 0x000C,
    InstallationStatus = 0x000D,
    ProcessingStatus = 0x000E,
    SkuInformation = 0x000F,
    NvidiaRatchet = 0x0010,
    OemRatchet = 0x0011,
    ValidityCounter = 0x0012,
    CertificateChain = 0x0013,
    MeasurementTranscript = 0x0014,
    DeviceId = 0x0015,
    TokenTypeSubtypeList = 0x0016,
    Payload = 0x0017,
    LegacyToken = 0x0018
};

/**
 * @brief GPU-specific TLV type identifiers
 *
 * These type identifiers are specific to GPU devices and contain
 * GPU-related configuration and identification information.
 */
enum GPU
{
    FeatureMask = 0x4000,
    ChipId = 0x4001
};

/**
 * @brief NBU-specific TLV type identifiers
 *
 * These type identifiers are specific to NBU devices and contain
 * NBU-related configuration and identification information.
 */
enum NBU
{
    KeypairUUID = 0x4400,
    PSID = 0x4401,
    FileDeviceUnique = 0x4402
};

/**
 * @brief BMC IRoT-specific TLV type identifiers
 *
 * These type identifiers are specific to BMC IRoT devices and contain
 * BMC IRoT-related configuration and identification information.
 */
enum BMCIRoT
{
    TokenVersion = 0x4801,
    NvidiaSignatureAlgorithm = 0x4802
};

} // namespace types

} // namespace debug_token
