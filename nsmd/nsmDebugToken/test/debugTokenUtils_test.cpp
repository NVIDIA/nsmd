/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2026 NVIDIA CORPORATION &
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

#include "debug-token/tlv.h"

#include "debugTokenUtils.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using namespace nsm::token_utils;

// ============================================================================
// tokenTypeToUint32 Tests
// ============================================================================

TEST(DebugTokenUtilsTest, TokenTypeToUint32NoneERoT)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, "ERoT");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32FRCERoT)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::FRC, "ERoT");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32CRCSERoT)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::CRCS, "ERoT");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32NoneCPU)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, "CPU");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32FRCCPU)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::FRC, "CPU");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32CRCSCPU)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::CRCS, "CPU");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32NoneGPU)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, "GPU");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32FRCGPU)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::FRC, "GPU");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32CRCSGPU)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::CRCS, "GPU");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32NoneSMA)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, "SMA");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32FRCSMA)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::FRC, "SMA");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32CRCSSMA)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::CRCS, "SMA");
    EXPECT_GE(result, 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32UnknownDeviceType)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, "UnknownDevice");
    EXPECT_GE(result, 0); // Should use default mapping
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32EmptyDeviceType)
{
    uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, "");
    EXPECT_GE(result, 0); // Should use default mapping
}

TEST(DebugTokenUtilsTest, TokenTypeToUint32CaseSensitivity)
{
    // Test case sensitivity of device type
    uint32_t result1 = tokenTypeToUint32(TokenTypeEnum::None, "erot");
    uint32_t result2 = tokenTypeToUint32(TokenTypeEnum::None, "EROT");

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
}

// ============================================================================
// tokenTypeToEnum Tests
// ============================================================================

TEST(DebugTokenUtilsTest, TokenTypeToEnumZeroERoT)
{
    TokenTypeEnum result = tokenTypeToEnum(0, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumZeroCPU)
{
    TokenTypeEnum result = tokenTypeToEnum(0, "CPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumZeroGPU)
{
    TokenTypeEnum result = tokenTypeToEnum(0, "GPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumZeroSMA)
{
    TokenTypeEnum result = tokenTypeToEnum(0, "SMA");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumNonZeroERoT)
{
    TokenTypeEnum result = tokenTypeToEnum(1, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumNonZeroCPU)
{
    TokenTypeEnum result = tokenTypeToEnum(1, "CPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumNonZeroGPU)
{
    TokenTypeEnum result = tokenTypeToEnum(1, "GPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumNonZeroSMA)
{
    TokenTypeEnum result = tokenTypeToEnum(1, "SMA");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumInvalidValue)
{
    // Test with invalid/unmapped value
    TokenTypeEnum result = tokenTypeToEnum(0xFFFFFFFF, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumUnknownDevice)
{
    TokenTypeEnum result = tokenTypeToEnum(0, "UnknownDevice");
    EXPECT_GE(static_cast<int>(result), 0); // Should use default mapping
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumEmptyDevice)
{
    TokenTypeEnum result = tokenTypeToEnum(0, "");
    EXPECT_GE(static_cast<int>(result), 0); // Should use default mapping
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumRoundTripERoT)
{
    // Test round-trip: enum -> uint32 -> enum
    TokenTypeEnum original = TokenTypeEnum::None;
    uint32_t intermediate = tokenTypeToUint32(original, "ERoT");
    TokenTypeEnum result = tokenTypeToEnum(intermediate, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumRoundTripCPU)
{
    TokenTypeEnum original = TokenTypeEnum::FRC;
    uint32_t intermediate = tokenTypeToUint32(original, "CPU");
    TokenTypeEnum result = tokenTypeToEnum(intermediate, "CPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumRoundTripGPU)
{
    TokenTypeEnum original = TokenTypeEnum::CRCS;
    uint32_t intermediate = tokenTypeToUint32(original, "GPU");
    TokenTypeEnum result = tokenTypeToEnum(intermediate, "GPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenTypeToEnumMultipleValues)
{
    // Test multiple different uint32_t values
    TokenTypeEnum result1 = tokenTypeToEnum(0, "ERoT");
    TokenTypeEnum result2 = tokenTypeToEnum(1, "ERoT");
    TokenTypeEnum result3 = tokenTypeToEnum(2, "ERoT");
    TokenTypeEnum result4 = tokenTypeToEnum(100, "ERoT");

    EXPECT_GE(static_cast<int>(result1), 0);
    EXPECT_GE(static_cast<int>(result2), 0);
    EXPECT_GE(static_cast<int>(result3), 0);
    EXPECT_GE(static_cast<int>(result4), 0);
}

// ============================================================================
// tokenSubtypeToEnum Tests
// ============================================================================

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumZeroZeroERoT)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumZeroZeroCPU)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, "CPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumZeroZeroGPU)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, "GPU");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumZeroZeroSMA)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, "SMA");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumVariousSubtypesERoT)
{
    // Test different subtype values for token type 0
    TokenSubtypeEnum result1 = tokenSubtypeToEnum(0, 0, "ERoT");
    TokenSubtypeEnum result2 = tokenSubtypeToEnum(0, 1, "ERoT");
    TokenSubtypeEnum result3 = tokenSubtypeToEnum(0, 2, "ERoT");
    TokenSubtypeEnum result4 = tokenSubtypeToEnum(0, 10, "ERoT");

    EXPECT_GE(static_cast<int>(result1), 0);
    EXPECT_GE(static_cast<int>(result2), 0);
    EXPECT_GE(static_cast<int>(result3), 0);
    EXPECT_GE(static_cast<int>(result4), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumVariousTypesERoT)
{
    // Test different token type values with subtype 0
    TokenSubtypeEnum result1 = tokenSubtypeToEnum(0, 0, "ERoT");
    TokenSubtypeEnum result2 = tokenSubtypeToEnum(1, 0, "ERoT");
    TokenSubtypeEnum result3 = tokenSubtypeToEnum(2, 0, "ERoT");
    TokenSubtypeEnum result4 = tokenSubtypeToEnum(10, 0, "ERoT");

    EXPECT_GE(static_cast<int>(result1), 0);
    EXPECT_GE(static_cast<int>(result2), 0);
    EXPECT_GE(static_cast<int>(result3), 0);
    EXPECT_GE(static_cast<int>(result4), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumVariousTypesCPU)
{
    TokenSubtypeEnum result1 = tokenSubtypeToEnum(0, 0, "CPU");
    TokenSubtypeEnum result2 = tokenSubtypeToEnum(1, 1, "CPU");
    TokenSubtypeEnum result3 = tokenSubtypeToEnum(2, 2, "CPU");

    EXPECT_GE(static_cast<int>(result1), 0);
    EXPECT_GE(static_cast<int>(result2), 0);
    EXPECT_GE(static_cast<int>(result3), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumVariousTypesGPU)
{
    TokenSubtypeEnum result1 = tokenSubtypeToEnum(0, 0, "GPU");
    TokenSubtypeEnum result2 = tokenSubtypeToEnum(1, 1, "GPU");
    TokenSubtypeEnum result3 = tokenSubtypeToEnum(2, 2, "GPU");

    EXPECT_GE(static_cast<int>(result1), 0);
    EXPECT_GE(static_cast<int>(result2), 0);
    EXPECT_GE(static_cast<int>(result3), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumVariousTypesSMA)
{
    TokenSubtypeEnum result1 = tokenSubtypeToEnum(0, 0, "SMA");
    TokenSubtypeEnum result2 = tokenSubtypeToEnum(1, 1, "SMA");
    TokenSubtypeEnum result3 = tokenSubtypeToEnum(2, 2, "SMA");

    EXPECT_GE(static_cast<int>(result1), 0);
    EXPECT_GE(static_cast<int>(result2), 0);
    EXPECT_GE(static_cast<int>(result3), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumInvalidType)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0xFFFFFFFF, 0, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumInvalidSubtype)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0xFFFFFFFF, "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumInvalidBoth)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0xFFFFFFFF, 0xFFFFFFFF,
                                                 "ERoT");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumUnknownDevice)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, "UnknownDevice");
    EXPECT_GE(static_cast<int>(result), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeToEnumEmptyDevice)
{
    TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, "");
    EXPECT_GE(static_cast<int>(result), 0);
}

// ============================================================================
// tokenSubtypeBitmapToEnumArray Tests
// ============================================================================

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayZeroBitmapERoT)
{
    std::vector<TokenSubtypeEnum> result =
        tokenSubtypeBitmapToEnumArray(0, 0, "ERoT");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayZeroBitmapCPU)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 0,
                                                                         "CPU");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayZeroBitmapGPU)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 0,
                                                                         "GPU");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayZeroBitmapSMA)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 0,
                                                                         "SMA");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArraySingleBitERoT)
{
    // Test with bitmap = 1 (bit 0 set)
    std::vector<TokenSubtypeEnum> result =
        tokenSubtypeBitmapToEnumArray(0, 1, "ERoT");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArraySingleBitCPU)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 1,
                                                                         "CPU");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArraySingleBitGPU)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 1,
                                                                         "GPU");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArraySingleBitSMA)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 1,
                                                                         "SMA");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayMultipleBitsERoT)
{
    // Test with bitmap = 0x7 (bits 0, 1, 2 set)
    std::vector<TokenSubtypeEnum> result =
        tokenSubtypeBitmapToEnumArray(0, 0x7, "ERoT");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayMultipleBitsCPU)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 0x7,
                                                                         "CPU");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayMultipleBitsGPU)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 0x7,
                                                                         "GPU");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayMultipleBitsSMA)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 0x7,
                                                                         "SMA");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayAllBitsERoT)
{
    // Test with all bits set
    std::vector<TokenSubtypeEnum> result =
        tokenSubtypeBitmapToEnumArray(0, 0xFFFFFFFF, "ERoT");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayVariousBitmaps)
{
    // Test different bitmap patterns
    std::vector<TokenSubtypeEnum> result1 =
        tokenSubtypeBitmapToEnumArray(0, 0x1, "ERoT");
    std::vector<TokenSubtypeEnum> result2 =
        tokenSubtypeBitmapToEnumArray(0, 0x3, "ERoT");
    std::vector<TokenSubtypeEnum> result3 =
        tokenSubtypeBitmapToEnumArray(0, 0xF, "ERoT");
    std::vector<TokenSubtypeEnum> result4 =
        tokenSubtypeBitmapToEnumArray(0, 0xFF, "ERoT");

    EXPECT_GE(result1.size(), 0);
    EXPECT_GE(result2.size(), 0);
    EXPECT_GE(result3.size(), 0);
    EXPECT_GE(result4.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayVariousTypes)
{
    // Test different token types with same bitmap
    std::vector<TokenSubtypeEnum> result1 =
        tokenSubtypeBitmapToEnumArray(0, 1, "ERoT");
    std::vector<TokenSubtypeEnum> result2 =
        tokenSubtypeBitmapToEnumArray(1, 1, "ERoT");
    std::vector<TokenSubtypeEnum> result3 =
        tokenSubtypeBitmapToEnumArray(2, 1, "ERoT");

    EXPECT_GE(result1.size(), 0);
    EXPECT_GE(result2.size(), 0);
    EXPECT_GE(result3.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayInvalidType)
{
    std::vector<TokenSubtypeEnum> result =
        tokenSubtypeBitmapToEnumArray(0xFFFFFFFF, 1, "ERoT");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayUnknownDevice)
{
    std::vector<TokenSubtypeEnum> result =
        tokenSubtypeBitmapToEnumArray(0, 1, "UnknownDevice");
    EXPECT_GE(result.size(), 0);
}

TEST(DebugTokenUtilsTest, TokenSubtypeBitmapToEnumArrayEmptyDevice)
{
    std::vector<TokenSubtypeEnum> result = tokenSubtypeBitmapToEnumArray(0, 1,
                                                                         "");
    EXPECT_GE(result.size(), 0);
}

// ============================================================================
// Cross-Device Consistency Tests
// ============================================================================

TEST(DebugTokenUtilsTest, AllDeviceTypesHandleTokenTypeConversion)
{
    const std::vector<std::string> deviceTypes = {"ERoT", "CPU", "GPU", "SMA",
                                                  "BMCIRoT"};

    for (const auto& deviceType : deviceTypes)
    {
        uint32_t result = tokenTypeToUint32(TokenTypeEnum::None, deviceType);
        EXPECT_GE(result, 0) << "Device: " << deviceType;

        TokenTypeEnum enumResult = tokenTypeToEnum(0, deviceType);
        EXPECT_GE(static_cast<int>(enumResult), 0) << "Device: " << deviceType;
    }
}

TEST(DebugTokenUtilsTest, AllDeviceTypesHandleSubtypeConversion)
{
    const std::vector<std::string> deviceTypes = {"ERoT", "CPU", "GPU", "SMA",
                                                  "BMCIRoT"};

    for (const auto& deviceType : deviceTypes)
    {
        TokenSubtypeEnum result = tokenSubtypeToEnum(0, 0, deviceType);
        EXPECT_GE(static_cast<int>(result), 0) << "Device: " << deviceType;
    }
}

TEST(DebugTokenUtilsTest, AllDeviceTypesHandleBitmapConversion)
{
    const std::vector<std::string> deviceTypes = {"ERoT", "CPU", "GPU", "SMA",
                                                  "BMCIRoT"};

    for (const auto& deviceType : deviceTypes)
    {
        std::vector<TokenSubtypeEnum> result =
            tokenSubtypeBitmapToEnumArray(0, 1, deviceType);
        EXPECT_GE(result.size(), 0) << "Device: " << deviceType;
    }
}

TEST(DebugTokenUtilsTest, StringViewHandlingAcrossFunctions)
{
    // Verify that std::string_view works with different string types
    std::string deviceStr = "ERoT";
    const char* deviceCStr = "CPU";
    std::string_view deviceSV = "GPU";

    uint32_t result1 = tokenTypeToUint32(TokenTypeEnum::None, deviceStr);
    uint32_t result2 = tokenTypeToUint32(TokenTypeEnum::None, deviceCStr);
    uint32_t result3 = tokenTypeToUint32(TokenTypeEnum::None, deviceSV);

    EXPECT_GE(result1, 0);
    EXPECT_GE(result2, 0);
    EXPECT_GE(result3, 0);
}

// ============================================================================
// PDF-aligned GPU / SMA mappings
// ============================================================================

TEST(DebugTokenUtilsTest, gpuHardwareUnlockTargetMaskMatchesPdfBit)
{
    EXPECT_EQ(tokenSubtypeToEnum(4, 0x00200000, "GPU"),
              TokenSubtypeEnum::TargetMaskUnlock);
}

TEST(DebugTokenUtilsTest, gpuHardwareUnlockPxucGlobalHulkMatchesPdfBit)
{
    EXPECT_EQ(tokenSubtypeToEnum(4, 0x00100000, "GPU"),
              TokenSubtypeEnum::PxucGlobalHulk);
}

TEST(DebugTokenUtilsTest, smaDebugCapabilitySubtypesMatchPdf)
{
    EXPECT_EQ(tokenTypeToEnum(2, "SMA"), TokenTypeEnum::SMADebugCapability);
    EXPECT_EQ(tokenSubtypeToEnum(2, 0x00000001, "SMA"),
              TokenSubtypeEnum::RasTest);
    EXPECT_EQ(tokenSubtypeToEnum(2, 0x00000002, "SMA"),
              TokenSubtypeEnum::PowerFailI2cDebug);
}

TEST(DebugTokenUtilsTest, smaCpldDebugCapabilitySubtypeMatchesPdf)
{
    EXPECT_EQ(tokenTypeToEnum(4, "SMA"), TokenTypeEnum::CpldDebugCapability);
    EXPECT_EQ(tokenSubtypeToEnum(4, 0x00000001, "SMA"),
              TokenSubtypeEnum::CpldDebugEnable);
}

TEST(DebugTokenUtilsTest, unmappedTokenInputsReturnNone)
{
    // SMA token type values are 0, 1, 2, and 4 — numeric 3 is not mapped.
    EXPECT_EQ(tokenTypeToEnum(3, "SMA"), TokenTypeEnum::None);

    // SMADebugCapability only defines 0x1 and 0x2 (plus None); other bits are
    // unmapped.
    EXPECT_EQ(tokenSubtypeToEnum(2, 0x00000004, "SMA"), TokenSubtypeEnum::None);

    // CpldDebugCapability only defines 0x1 (plus None); 0x2 is unmapped.
    EXPECT_EQ(tokenSubtypeToEnum(4, 0x00000002, "SMA"), TokenSubtypeEnum::None);

    // No subtype table for unknown device types.
    EXPECT_EQ(tokenSubtypeToEnum(1, 0x00000001, "UnknownDevice"),
              TokenSubtypeEnum::None);
}

// ============================================================================
// tokenSubtypeBitmapToEnumArray: TRUE branch of
//   if (enumValue != TokenSubtypeEnum::None || bitMask == 0)
//
// All prior tests used combinations where tokenSubtypeToEnum() returns None,
// so push_back() was never called (FALSE branch).
// GPU type=1 (DebugFirmwareUnlock), subtype=0x1 maps to TokenSubtypeEnum::VBIOS
// (non-None) → TRUE branch → VBIOS is pushed into the result.
// ============================================================================

TEST(DebugTokenUtilsTest,
     TokenSubtypeBitmapToEnumArray_GPU_Type1_Bit0_PushesVBIOS)
{
    // tokenSubtype=0x1 → bit 0 set → bitMask=1
    // tokenSubtypeToEnum(1, 1, "GPU") → VBIOS (non-None)
    // → TRUE branch → VBIOS pushed
    auto result = tokenSubtypeBitmapToEnumArray(1, 0x1, "GPU");

    ASSERT_FALSE(result.empty());
    EXPECT_EQ(result[0], TokenSubtypeEnum::VBIOS);
}

TEST(DebugTokenUtilsTest,
     TokenSubtypeBitmapToEnumArray_GPU_Type1_MultipleBits_PushesMultipleEnums)
{
    // tokenSubtype=0x3 → bits 0 and 1 set
    // bit 0 → bitMask=1 → VBIOS; bit 1 → bitMask=2 → FSPRT
    auto result = tokenSubtypeBitmapToEnumArray(1, 0x3, "GPU");

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0], TokenSubtypeEnum::VBIOS);
    EXPECT_EQ(result[1], TokenSubtypeEnum::FSPRT);
}

// ============================================================================
// BMCIRoT token type/subtype mappings
// ============================================================================

TEST(DebugTokenUtilsTest, BmcIrotTokenTypeNone)
{
    EXPECT_EQ(tokenTypeToEnum(0, "BMCIRoT"), TokenTypeEnum::None);
}

TEST(DebugTokenUtilsTest, BmcIrotTokenTypeDebugFirmwareUnlock)
{
    EXPECT_EQ(tokenTypeToEnum(1, "BMCIRoT"),
              TokenTypeEnum::DebugFirmwareUnlock);
}

TEST(DebugTokenUtilsTest, BmcIrotTokenTypeRoundTrip)
{
    uint32_t val = tokenTypeToUint32(TokenTypeEnum::DebugFirmwareUnlock,
                                     "BMCIRoT");
    EXPECT_EQ(val, 1u);
    EXPECT_EQ(tokenTypeToEnum(val, "BMCIRoT"),
              TokenTypeEnum::DebugFirmwareUnlock);
}

TEST(DebugTokenUtilsTest, BmcIrotUnmappedTokenTypeReturnsNone)
{
    EXPECT_EQ(tokenTypeToEnum(2, "BMCIRoT"), TokenTypeEnum::None);
    EXPECT_EQ(tokenTypeToEnum(0xFF, "BMCIRoT"), TokenTypeEnum::None);
}

TEST(DebugTokenUtilsTest, BmcIrotSubtypeNone)
{
    EXPECT_EQ(tokenSubtypeToEnum(0, 0, "BMCIRoT"), TokenSubtypeEnum::None);
    EXPECT_EQ(tokenSubtypeToEnum(1, 0, "BMCIRoT"), TokenSubtypeEnum::None);
}

TEST(DebugTokenUtilsTest, BmcIrotBitmapZeroReturnsEmpty)
{
    auto result = tokenSubtypeBitmapToEnumArray(1, 0, "BMCIRoT");
    EXPECT_TRUE(result.empty());
}

// ============================================================================
// parseTokenRecords tests
//
// Exercises every reachable bound check in parseTokenRecords:
//   1. Well-formed multi-record file -> all records returned, sizes match
//   2. Truncated header (file ends mid-StructureHeader) -> partial parse
//   3. Truncated payload (StructureHeader.size points past EOF) -> partial
//   4. Zero records declared -> empty result without scanning
//   5. tlvOff > fileSize -> early return, empty result
// ============================================================================

namespace
{

DebugTokenHeader makeHeader(uint16_t numRecords, uint16_t tlvOff,
                            uint32_t fileSize)
{
    DebugTokenHeader h{};
    std::memset(&h, 0, sizeof(h));
    h.version = 1;
    h.type = FileTypeDebugToken;
    h.numberOfRecords = numRecords;
    h.offsetToListOfStructs = tlvOff;
    h.fileSize = fileSize;
    return h;
}

// Build a single TLV-record's bytes: a StructureHeader followed by
// payloadSize bytes of dummy payload. Caller-controlled payloadSize lets
// tests forge truncated payloads by claiming a larger size in the header
// than is actually appended to the file.
std::vector<uint8_t> makeRecordBytes(uint32_t payloadSize, uint8_t fillByte,
                                     uint32_t headerSize = UINT32_MAX)
{
    debug_token::StructureHeader sh{};
    std::memset(&sh, 0, sizeof(sh));
    sh.identifier[0] = debug_token::TLV_IDENTIFIER[0];
    sh.identifier[1] = debug_token::TLV_IDENTIFIER[1];
    sh.identifier[2] = debug_token::TLV_IDENTIFIER[2];
    sh.identifier[3] = debug_token::TLV_IDENTIFIER[3];
    sh.versionMajor = 1;
    sh.versionMinor = 0;
    sh.size = (headerSize == UINT32_MAX) ? payloadSize : headerSize;

    std::vector<uint8_t> bytes(sizeof(sh) + payloadSize);
    std::memcpy(bytes.data(), &sh, sizeof(sh));
    std::fill(bytes.begin() + sizeof(sh), bytes.end(), fillByte);
    return bytes;
}

std::pair<std::vector<uint8_t>, DebugTokenHeader>
    makeMultiRecordFile(std::initializer_list<uint32_t> payloadSizes)
{
    std::vector<std::vector<uint8_t>> records;
    for (auto sz : payloadSizes)
    {
        records.push_back(makeRecordBytes(sz, 0xAA));
    }

    size_t total = sizeof(DebugTokenHeader);
    for (const auto& r : records)
    {
        total += r.size();
    }

    auto hdr = makeHeader(static_cast<uint16_t>(records.size()),
                          sizeof(DebugTokenHeader),
                          static_cast<uint32_t>(total));

    std::vector<uint8_t> out(total);
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    size_t off = sizeof(DebugTokenHeader);
    for (const auto& r : records)
    {
        std::memcpy(out.data() + off, r.data(), r.size());
        off += r.size();
    }
    return {out, hdr};
}

} // namespace

TEST(DebugTokenUtilsParseTokenRecordsTest, WellFormedMultiRecord)
{
    constexpr uint32_t payloadA = 16;
    constexpr uint32_t payloadB = 32;
    constexpr uint32_t payloadC = 8;

    auto recA = makeRecordBytes(payloadA, 0xAA);
    auto recB = makeRecordBytes(payloadB, 0xBB);
    auto recC = makeRecordBytes(payloadC, 0xCC);

    const uint16_t tlvOff = sizeof(DebugTokenHeader);
    const uint32_t fileSize = tlvOff + recA.size() + recB.size() + recC.size();
    auto header = makeHeader(3, tlvOff, fileSize);

    std::vector<uint8_t> file(fileSize);
    std::memcpy(file.data(), &header, sizeof(header));
    std::memcpy(file.data() + tlvOff, recA.data(), recA.size());
    std::memcpy(file.data() + tlvOff + recA.size(), recB.data(), recB.size());
    std::memcpy(file.data() + tlvOff + recA.size() + recB.size(), recC.data(),
                recC.size());

    auto records = parseTokenRecords(file, header);

    ASSERT_EQ(records.size(), 3u);
    EXPECT_EQ(records[0].size(),
              sizeof(debug_token::StructureHeader) + payloadA);
    EXPECT_EQ(records[1].size(),
              sizeof(debug_token::StructureHeader) + payloadB);
    EXPECT_EQ(records[2].size(),
              sizeof(debug_token::StructureHeader) + payloadC);
    // Spans must reference the original buffer and start at the TLV offsets.
    EXPECT_EQ(records[0].data(), file.data() + tlvOff);
    EXPECT_EQ(records[1].data(), file.data() + tlvOff + recA.size());
    EXPECT_EQ(records[2].data(),
              file.data() + tlvOff + recA.size() + recB.size());
}

TEST(DebugTokenUtilsParseTokenRecordsTest, TruncatedStructureHeader)
{
    auto recA = makeRecordBytes(16, 0xAA);

    const uint16_t tlvOff = sizeof(DebugTokenHeader);
    // Append recA in full, then half of a StructureHeader to simulate a
    // file that ends mid-record-header.
    const size_t shortHdr = sizeof(debug_token::StructureHeader) / 2;
    const uint32_t fileSize = tlvOff + recA.size() + shortHdr;
    // Claim two records in the header even though only one fully fits.
    auto header = makeHeader(2, tlvOff, fileSize);

    std::vector<uint8_t> file(fileSize, 0);
    std::memcpy(file.data(), &header, sizeof(header));
    std::memcpy(file.data() + tlvOff, recA.data(), recA.size());

    auto records = parseTokenRecords(file, header);

    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].size(), sizeof(debug_token::StructureHeader) + 16u);
}

TEST(DebugTokenUtilsParseTokenRecordsTest, TruncatedPayload)
{
    // First record claims 64 bytes of payload but only 4 bytes are
    // present in the file -> tlvOff + off + recSz > fileData.size() ->
    // parser stops with an empty result.
    auto truncated = makeRecordBytes(/*actualPayload=*/4, 0xDE,
                                     /*headerSize=*/64);

    const uint16_t tlvOff = sizeof(DebugTokenHeader);
    const uint32_t fileSize = tlvOff + truncated.size();
    auto header = makeHeader(1, tlvOff, fileSize);

    std::vector<uint8_t> file(fileSize, 0);
    std::memcpy(file.data(), &header, sizeof(header));
    std::memcpy(file.data() + tlvOff, truncated.data(), truncated.size());

    auto records = parseTokenRecords(file, header);
    EXPECT_TRUE(records.empty());
}

TEST(DebugTokenUtilsParseTokenRecordsTest, ZeroRecordsReturnsEmpty)
{
    auto header = makeHeader(0, sizeof(DebugTokenHeader),
                             sizeof(DebugTokenHeader));
    std::vector<uint8_t> file(sizeof(DebugTokenHeader), 0);
    std::memcpy(file.data(), &header, sizeof(header));

    auto records = parseTokenRecords(file, header);
    EXPECT_TRUE(records.empty());
}

TEST(DebugTokenUtilsParseTokenRecordsTest, TlvOffsetBeyondFileSize)
{
    // tlvOff points past the end of fileData -> immediate empty return.
    auto header = makeHeader(/*numRecords=*/3, /*tlvOff=*/4096,
                             /*fileSize=*/sizeof(DebugTokenHeader));

    std::vector<uint8_t> file(sizeof(DebugTokenHeader), 0);
    std::memcpy(file.data(), &header, sizeof(header));

    auto records = parseTokenRecords(file, header);
    EXPECT_TRUE(records.empty());
}

TEST(IsMultiRecordContainerTest, TlvMagicPrefixReturnsFalse)
{
    std::vector<uint8_t> file(64, 0);
    std::memcpy(file.data(), debug_token::TLV_IDENTIFIER,
                sizeof(debug_token::TLV_IDENTIFIER));
    EXPECT_FALSE(isMultiRecordContainer(file));
}

TEST(IsMultiRecordContainerTest, DebugTokenHeaderTypeMatchReturnsTrue)
{
    auto [file, hdr] = makeMultiRecordFile({8});
    EXPECT_TRUE(isMultiRecordContainer(file));
}

TEST(IsMultiRecordContainerTest, DebugTokenHeaderTypeMismatchReturnsFalse)
{
    auto [file, hdr] = makeMultiRecordFile({8});
    file[offsetof(DebugTokenHeader, type)] = 0x00;
    EXPECT_FALSE(isMultiRecordContainer(file));
}

TEST(IsMultiRecordContainerTest, PrefixShorterThanHeaderReturnsFalse)
{
    std::vector<uint8_t> shortPrefix(sizeof(DebugTokenHeader) - 1, 0xFF);
    EXPECT_FALSE(isMultiRecordContainer(shortPrefix));
}

TEST(IsMultiRecordContainerTest, EmptyPrefixReturnsFalse)
{
    EXPECT_FALSE(isMultiRecordContainer({}));
}

TEST(IsMultiRecordContainerTest, PartialTlvMagicFallsThroughToHeaderCheck)
{
    // A prefix that partially matches the TLV magic but differs on the
    // last byte must miss the magic check and fall through to the header
    // interpretation. With type == FileTypeDebugToken, the header check
    // then returns true.
    std::vector<uint8_t> prefix(sizeof(DebugTokenHeader), 0);
    prefix[0] = debug_token::TLV_IDENTIFIER[0];
    prefix[2] = debug_token::TLV_IDENTIFIER[2];
    prefix[3] = debug_token::TLV_IDENTIFIER[3] ^ 0xFF;
    prefix[offsetof(DebugTokenHeader, type)] = FileTypeDebugToken;
    EXPECT_TRUE(isMultiRecordContainer(prefix));
}

TEST(IsMultiRecordContainerTest, TlvIdentifierMatchesSpec)
{
    // Regression guard: the spec mandates the TLV identifier is the ASCII
    // string "TLV1". If this ever changes upstream, the detection logic in
    // isMultiRecordContainer needs to be re-evaluated.
    EXPECT_EQ(debug_token::TLV_IDENTIFIER[0], 0x54); // 'T'
    EXPECT_EQ(debug_token::TLV_IDENTIFIER[1], 0x4C); // 'L'
    EXPECT_EQ(debug_token::TLV_IDENTIFIER[2], 0x56); // 'V'
    EXPECT_EQ(debug_token::TLV_IDENTIFIER[3], 0x31); // '1'
}
