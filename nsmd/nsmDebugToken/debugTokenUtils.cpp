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

#include "debugTokenUtils.hpp"

#include <array>
#include <unordered_map>
#include <utility>

namespace nsm::token_utils
{

/**
 * @brief Device-specific token type and subtype mappings.
 *
 * Each device type has a static array of supported token types and subtypes.
 * Uses direct enum mappings for efficient conversion.
 */
namespace DeviceTokenMappings
{

// ERoT device token types (numeric to enum)
constexpr std::array<std::pair<uint32_t, TokenTypeEnum>, 2> erotTokenTypes{
    {{0, TokenTypeEnum::None}, {1, TokenTypeEnum::DebugFirmwareUnlock}}};

// ERoT device token subtypes - map indexed by token type value
const std::unordered_map<uint32_t,
                         std::vector<std::pair<uint32_t, TokenSubtypeEnum>>>
    erotTokenSubtypesMap = {{0, {{0, TokenSubtypeEnum::None}}},
                            {1, {{0, TokenSubtypeEnum::None}}}};

// CPU device token types (numeric to enum)
constexpr std::array<std::pair<uint32_t, TokenTypeEnum>, 8> cpuTokenTypes{
    {{0, TokenTypeEnum::None},
     {1, TokenTypeEnum::DebugFirmwareUnlock},
     {2, TokenTypeEnum::CcplexArmJtagDebugCont},
     {3, TokenTypeEnum::NVJtagControl},
     {4, TokenTypeEnum::DiagnosticBoot},
     {5, TokenTypeEnum::FwDebugKnobs},
     {6, TokenTypeEnum::FirewallLifting},
     {7, TokenTypeEnum::Verbosity}}};

// CPU device token subtypes - map indexed by token type value (numeric to enum)
const std::unordered_map<uint32_t,
                         std::vector<std::pair<uint32_t, TokenSubtypeEnum>>>
    cpuTokenSubtypesMap = {
        {0, {{0, TokenSubtypeEnum::None}}},
        {1, // DebugFirmwareUnlock subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::OOBHub},
          {0x00000002, TokenSubtypeEnum::RAS},
          {0x00000004, TokenSubtypeEnum::Pcore},
          {0x00000008, TokenSubtypeEnum::PscFirmware},
          {0x00000010, TokenSubtypeEnum::MB1},
          {0x00000020, TokenSubtypeEnum::MB2},
          {0x00000040, TokenSubtypeEnum::BpmpFirmware},
          {0x00000080, TokenSubtypeEnum::BpmpIst},
          {0x00000100, TokenSubtypeEnum::BpmpDiag},
          {0x00000200, TokenSubtypeEnum::MSEQ},
          {0x00000400, TokenSubtypeEnum::C2C},
          {0x00000800, TokenSubtypeEnum::ATF},
          {0x00001000, TokenSubtypeEnum::RMM}}},
        {2, // CcplexArmJtagDebugCont subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::Dbgen},
          {0x00000002, TokenSubtypeEnum::Niden},
          {0x00000004, TokenSubtypeEnum::Spiden},
          {0x00000008, TokenSubtypeEnum::Spniden},
          {0x00000010, TokenSubtypeEnum::JtagEnable},
          {0x00000020, TokenSubtypeEnum::DeviceenApbap},
          {0x00000040, TokenSubtypeEnum::DeviceenAxiap},
          {0x00000080, TokenSubtypeEnum::Rlpiden},
          {0x00000100, TokenSubtypeEnum::Rtpiden},
          {0x00010000, TokenSubtypeEnum::NvcpuUcfApb2ctlpDbgen},
          {0x00020000, TokenSubtypeEnum::NvcpuUcfElaEn},
          {0x00040000, TokenSubtypeEnum::EcProbesEn},
          {0x80000000, TokenSubtypeEnum::EnableCswp}}},
        {3, // NVJtagControl subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::DftAccessAllowed}}},
        {4, // DiagnosticBoot subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::BpmpDiagnosticBoot},
          {0x00000002, TokenSubtypeEnum::CPUDiagnosticBoot}}},
        {5, // FwDebugKnobs subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::BpmpFwDebugfs},
          {0x00000002, TokenSubtypeEnum::MseqEnableSbeReporting}}},
        {6, // FirewallLifting subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::GroupB},
          {0x00000002, TokenSubtypeEnum::GroupC}}},
        {7, // Verbosity subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::OOBHub},
          {0x00000002, TokenSubtypeEnum::RAS},
          {0x00000004, TokenSubtypeEnum::Pcore},
          {0x00000008, TokenSubtypeEnum::PscFirmware},
          {0x00000010, TokenSubtypeEnum::MB1},
          {0x00000020, TokenSubtypeEnum::MB2},
          {0x00000040, TokenSubtypeEnum::BpmpFirmware},
          {0x00000080, TokenSubtypeEnum::BpmpIst},
          {0x00000100, TokenSubtypeEnum::BpmpDiag},
          {0x00000200, TokenSubtypeEnum::MSEQ},
          {0x00000400, TokenSubtypeEnum::C2C},
          {0x00000800, TokenSubtypeEnum::ATF},
          {0x00001000, TokenSubtypeEnum::RMM},
          {0x00002000, TokenSubtypeEnum::PscFmc},
          {0x80000000, TokenSubtypeEnum::EnableSensitiveLogging}}}};

// GPU device token types (numeric to enum)
constexpr std::array<std::pair<uint32_t, TokenTypeEnum>, 6> gpuTokenTypes{
    {{0, TokenTypeEnum::None},
     {1, TokenTypeEnum::DebugFirmwareUnlock},
     {2, TokenTypeEnum::JtagUnlock},
     {4, TokenTypeEnum::HardwareUnlock},
     {8, TokenTypeEnum::RuntimeDebugUnlock},
     {16, TokenTypeEnum::FeatureUnlock}}};

// GPU device token subtypes - map indexed by token type value (numeric to enum)
const std::unordered_map<uint32_t,
                         std::vector<std::pair<uint32_t, TokenSubtypeEnum>>>
    gpuTokenSubtypesMap = {
        {0, {{0, TokenSubtypeEnum::None}}},
        {1, // DebugFirmwareUnlock subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::VBIOS},
          {0x00000002, TokenSubtypeEnum::FSPRT},
          {0x00000004, TokenSubtypeEnum::OOBHubRT},
          {0x00000008, TokenSubtypeEnum::NetIR},
          {0x00000010, TokenSubtypeEnum::PCIe},
          {0x00000020, TokenSubtypeEnum::FBFALCON},
          {0x00000040, TokenSubtypeEnum::CTCHBI},
          {0x00000080, TokenSubtypeEnum::BuildInfo},
          {0x00000100, TokenSubtypeEnum::DataTables},
          {0x00000200, TokenSubtypeEnum::PreltssmDevinit},
          {0x00000400, TokenSubtypeEnum::PostltssmDevinit},
          {0x00000800, TokenSubtypeEnum::PrimaryImage},
          {0x00001000, TokenSubtypeEnum::UEFIx86},
          {0x00002000, TokenSubtypeEnum::UEFIArm},
          {0x00004000, TokenSubtypeEnum::Duck},
          {0x00008000, TokenSubtypeEnum::InforomRO},
          {0x00010000, TokenSubtypeEnum::DevInit},
          {0x00020000, TokenSubtypeEnum::ExtensionImage},
          {0x00040000, TokenSubtypeEnum::MSE},
          {0x00080000, TokenSubtypeEnum::RomDirectory},
          {0x00100000, TokenSubtypeEnum::GspRm}}},
        {2, {{0, TokenSubtypeEnum::None}}}, // JtagUnlock
        {4,                                 // HardwareUnlock subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::MemQual},
          {0x00000002, TokenSubtypeEnum::GlobalFunctionalDebug},
          {0x00000004, TokenSubtypeEnum::SpeedoKappaIddq},
          {0x00000008, TokenSubtypeEnum::ECCEnable},
          {0x00000010, TokenSubtypeEnum::ReducedGlobalFunctionalDebug},
          {0x00000020, TokenSubtypeEnum::ECCUnlock},
          {0x00000040, TokenSubtypeEnum::Bar0DecouplerUnlock},
          {0x00000080, TokenSubtypeEnum::I2ccAccess},
          {0x00000100, TokenSubtypeEnum::ECCPoisonDisable},
          {0x00000200, TokenSubtypeEnum::NvlinkGlobalHulk},
          {0x00000400, TokenSubtypeEnum::LowerRomProtection},
          {0x00000800, TokenSubtypeEnum::RecreateInforomRW},
          {0x00001000, TokenSubtypeEnum::PrcKnobEn},
          {0x00002000, TokenSubtypeEnum::Nuke},
          {0x00004000, TokenSubtypeEnum::ArchFunctionalTests},
          {0x00008000, TokenSubtypeEnum::SsgDebug},
          {0x00010000, TokenSubtypeEnum::CxlDebug},
          {0x00020000, TokenSubtypeEnum::VideDebug},
          {0x00040000, TokenSubtypeEnum::GlobalFunctionalDebugMinimal},
          {0x00080000, TokenSubtypeEnum::IstDebug},
          {0x00100000, TokenSubtypeEnum::PxucGlobalHulk}}},
        {8, // RuntimeDebugUnlock subtypes
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::NvlinkErrorInjectionEnable},
          {0x00000002, TokenSubtypeEnum::NvlinkDebugApiEnable},
          {0x00000004, TokenSubtypeEnum::NetIRUnitTestEnable},
          {0x00000008, TokenSubtypeEnum::NvlinkMissionDebugGmt},
          {0x00000010, TokenSubtypeEnum::PxucDebugApiEnable},
          {0x00000020, TokenSubtypeEnum::PxucErrorInjectionEnable},
          {0x00000040, TokenSubtypeEnum::PxucMissionDebugGmt},
          {0x00000080, TokenSubtypeEnum::EnableGspTaskDebug}}},
        {16, {{0, TokenSubtypeEnum::None}}} // FeatureUnlock
};

// SMA device token types (numeric to enum)
constexpr std::array<std::pair<uint32_t, TokenTypeEnum>, 4> smaTokenTypes{
    {{0, TokenTypeEnum::None},
     {1, TokenTypeEnum::DebugFirmwareUnlock},
     {2, TokenTypeEnum::OTPDumpEnable},
     {4, TokenTypeEnum::RAS}}};

// SMA device token subtypes - map indexed by token type value (numeric to enum)
const std::unordered_map<uint32_t,
                         std::vector<std::pair<uint32_t, TokenSubtypeEnum>>>
    smaTokenSubtypesMap = {
        {0, {{0, TokenSubtypeEnum::None}}},
        {1, // DebugFirmwareUnlock
         {{0x00000000, TokenSubtypeEnum::None},
          {0x00000001, TokenSubtypeEnum::MCU},
          {0x00000002, TokenSubtypeEnum::CPLD}}},
        {2, {{0, TokenSubtypeEnum::None}}}, // OTPDumpEnable
        {4, {{0, TokenSubtypeEnum::None}}}  // RAS
};

// Default/generic token types for other devices (numeric to enum)
constexpr std::array<std::pair<uint32_t, TokenTypeEnum>, 4> defaultTokenTypes{
    {{0, TokenTypeEnum::None},
     {1, TokenTypeEnum::DebugFirmwareUnlock},
     {2, TokenTypeEnum::OTPDumpEnable},
     {4, TokenTypeEnum::RAS}}};

std::pair<const std::pair<uint32_t, TokenTypeEnum>*, size_t>
    getTokenTypeMappingForDevice(std::string_view deviceType)
{
    if (deviceType == "ERoT")
    {
        return {erotTokenTypes.data(), erotTokenTypes.size()};
    }
    if (deviceType == "CPU")
    {
        return {cpuTokenTypes.data(), cpuTokenTypes.size()};
    }
    if (deviceType == "GPU")
    {
        return {gpuTokenTypes.data(), gpuTokenTypes.size()};
    }
    if (deviceType == "SMA")
    {
        return {smaTokenTypes.data(), smaTokenTypes.size()};
    }
    return {defaultTokenTypes.data(), defaultTokenTypes.size()};
}

const std::unordered_map<uint32_t,
                         std::vector<std::pair<uint32_t, TokenSubtypeEnum>>>*
    getTokenSubtypeMappingForDevice(std::string_view deviceType)
{
    if (deviceType == "ERoT")
    {
        return &erotTokenSubtypesMap;
    }
    if (deviceType == "CPU")
    {
        return &cpuTokenSubtypesMap;
    }
    if (deviceType == "GPU")
    {
        return &gpuTokenSubtypesMap;
    }
    if (deviceType == "SMA")
    {
        return &smaTokenSubtypesMap;
    }
    return nullptr;
}

} // namespace DeviceTokenMappings

uint32_t tokenTypeToUint32(TokenTypeEnum tokenTypeEnum,
                           std::string_view deviceType)
{
    const auto [mapping, size] =
        DeviceTokenMappings::getTokenTypeMappingForDevice(deviceType);
    for (size_t i = 0; i < size; ++i)
    {
        if (mapping[i].second == tokenTypeEnum)
        {
            return mapping[i].first;
        }
    }
    return 0;
}

TokenTypeEnum tokenTypeToEnum(uint32_t tokenType, std::string_view deviceType)
{
    const auto [mapping, size] =
        DeviceTokenMappings::getTokenTypeMappingForDevice(deviceType);

    for (size_t i = 0; i < size; ++i)
    {
        if (mapping[i].first == tokenType)
        {
            return mapping[i].second;
        }
    }
    return TokenTypeEnum::None;
}

TokenSubtypeEnum tokenSubtypeToEnum(uint32_t tokenType, uint32_t tokenSubtype,
                                    std::string_view deviceType)
{
    const auto* subtypesMap =
        DeviceTokenMappings::getTokenSubtypeMappingForDevice(deviceType);
    if (subtypesMap == nullptr)
    {
        return TokenSubtypeEnum::None;
    }

    auto it = subtypesMap->find(tokenType);
    if (it != subtypesMap->end())
    {
        for (const auto& [key, value] : it->second)
        {
            if (key == tokenSubtype)
            {
                return value;
            }
        }
    }
    return TokenSubtypeEnum::None;
}

std::vector<TokenSubtypeEnum>
    tokenSubtypeBitmapToEnumArray(uint32_t tokenType, uint32_t tokenSubtype,
                                  std::string_view deviceType)
{
    std::vector<TokenSubtypeEnum> subtypeEnums;

    if (tokenSubtype == 0)
    {
        return subtypeEnums;
    }
    for (uint32_t bitPos = 0; bitPos < 32; ++bitPos)
    {
        uint32_t bitMask = 1U << bitPos;
        if ((tokenSubtype & bitMask) != 0)
        {
            TokenSubtypeEnum enumValue = tokenSubtypeToEnum(tokenType, bitMask,
                                                            deviceType);
            if (enumValue != TokenSubtypeEnum::None || bitMask == 0)
            {
                subtypeEnums.push_back(enumValue);
            }
        }
    }

    return subtypeEnums;
}

} // namespace nsm::token_utils
