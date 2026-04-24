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

#pragma once

#include <com/nvidia/DebugToken/Common/server.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace nsm::token_utils
{

using TokenTypeEnum =
    sdbusplus::common::com::nvidia::debug_token::Common::Types;
using TokenSubtypeEnum =
    sdbusplus::common::com::nvidia::debug_token::Common::SubTypes;

// File type constant for debug token files
constexpr uint8_t FileTypeDebugToken = 2;

// Multi-record debug-token file header.
struct DebugTokenHeader
{
    uint8_t version;
    uint8_t type;
    uint16_t numberOfRecords;
    uint16_t offsetToListOfStructs;
    uint32_t fileSize;
    uint8_t reserved[6];
} __attribute__((packed));

// Parse the multi-record file into byte-spans, one per TLV record (spans
// reference @p fileData). On a truncated/malformed record, parsing stops and
// the spans collected so far are returned; callers can detect truncation by
// comparing the result size against header.numberOfRecords.
std::vector<std::span<const uint8_t>>
    parseTokenRecords(std::span<const uint8_t> fileData,
                      const DebugTokenHeader& header);

// Determine whether @p prefix is the start of a multi-record
// DebugTokenHeader container (not a single-record TLV file).
//
// The debug-token TLV spec requires every TLV structure to begin with the
// 4-byte magic identifier "TLV1" at offset 0. A prefix that starts with
// that magic is a single-record TLV file and this function returns false.
// Otherwise the prefix is interpreted as a DebugTokenHeader and the
// function returns true iff its type field equals FileTypeDebugToken.
// A prefix shorter than sizeof(DebugTokenHeader) that does not match the
// TLV magic returns false.
bool isMultiRecordContainer(std::span<const uint8_t> prefix);

/**
 * @brief Converts an enum token type to its numeric representation
 *
 * @param tokenTypeEnum The token type enum value
 * @param deviceType The type of device the debug token is installed on
 * @return uint32_t The numeric representation of the token type, or 0 if not
 * found
 */
uint32_t tokenTypeToUint32(TokenTypeEnum tokenTypeEnum,
                           std::string_view deviceType);

/**
 * @brief Converts a numeric token type to its enum representation
 *
 * @param tokenType The numeric token type value
 * @param deviceType The type of device the debug token is installed on
 * @return TokenTypeEnum The enum representation
 */
TokenTypeEnum tokenTypeToEnum(uint32_t tokenType, std::string_view deviceType);

/**
 * @brief Converts a numeric token subtype to its enum representation
 *
 * @param tokenType The numeric token type value (context for subtype)
 * @param tokenSubtype The numeric token subtype value
 * @param deviceType The type of device the debug token is installed on
 * @return TokenSubtypeEnum The enum representation
 */
TokenSubtypeEnum tokenSubtypeToEnum(uint32_t tokenType, uint32_t tokenSubtype,
                                    std::string_view deviceType);

/**
 * @brief Converts a numeric token subtype bitmap to array of enum subtypes
 *
 * @param tokenType The numeric token type value (context for subtype)
 * @param tokenSubtype The numeric token subtype bitmap value
 * @param deviceType The type of device the debug token is installed on
 * @return std::vector of SubTypes enums
 */
std::vector<TokenSubtypeEnum>
    tokenSubtypeBitmapToEnumArray(uint32_t tokenType, uint32_t tokenSubtype,
                                  std::string_view deviceType);

} // namespace nsm::token_utils
