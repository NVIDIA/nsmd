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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
using namespace ::testing;

#include "base.h"

#include "utils.hpp"

#define private public
#define protected public

#include "stateChangeLogger.hpp"

using namespace nsm;

struct StateChangeLoggerTest : public Test, public StateChangeLogger
{
    const utils::Bitfield256& bitfieldArg(const std::string& fName,
                                          size_t index = 0)
    {
        auto& logger = loggers[fName];
        assert(logger.size() > index);
        return std::get<utils::Bitfield256>(logger[index]);
    }
    auto boolArg(const std::string& fName, size_t index)
    {
        auto& logger = loggers[fName];
        assert(logger.size() > index);
        return std::get<bool>(logger[index]);
    }
};

TEST_F(StateChangeLoggerTest, GoodTestSuccessNotStored)
{
    EXPECT_FALSE(shouldLog("GoodTestSuccessNotStored1", NSM_SW_SUCCESS,
                           nsm_completion_codes(0)));
    EXPECT_TRUE(loggers.empty());

    EXPECT_FALSE(shouldLog("GoodTestSuccessNotStored2", ERR_NULL, NSM_SUCCESS,
                           NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
    EXPECT_NO_THROW(shouldLog("GoodTestSuccessNotStored2", ERR_NULL,
                              NSM_SUCCESS, NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());

    EXPECT_FALSE(shouldLog("GoodTestSuccessNotStored3", ERR_NULL, NSM_SUCCESS,
                           NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestArgsMismatch)
{
    EXPECT_TRUE(shouldLog("GoodTestArgsMismatch", NSM_SW_ERROR,
                          nsm_completion_codes(0)));
    EXPECT_FALSE(loggers.empty());

    EXPECT_TRUE(shouldLog("GoodTestArgsMismatch", ERR_NULL, NSM_SUCCESS,
                          NSM_SW_SUCCESS));

    EXPECT_FALSE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestLogResultCode)
{
    const auto fName = "GoodTestLogResultCode";
    EXPECT_TRUE(shouldLog(fName, NSM_SW_ERROR_DATA));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName).isAnyBitSet());
    EXPECT_FALSE(shouldLog(fName, NSM_SW_ERROR_DATA));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName).isAnyBitSet());
    EXPECT_TRUE(shouldLog(fName, NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(shouldLog(fName, NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName).isAnyBitSet());
    EXPECT_FALSE(shouldLog(fName, NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestLogResponse)
{
    const auto fName = "GoodTestLogResponse";
    EXPECT_TRUE(shouldLog(fName, ERR_TIMEOUT, NSM_SUCCESS, NSM_SW_ERROR));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_FALSE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(shouldLog(fName, ERR_NOT_SUPPORTED, NSM_ERR_INVALID_DATA,
                          NSM_SW_ERROR));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                          NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                           NSM_SW_ERROR_LENGTH));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_FALSE(
        shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA, NSM_SW_SUCCESS));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_FALSE(shouldLog(fName, ERR_NULL, NSM_SUCCESS, NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestLogResponseWithSize)
{
    const auto fName = "GoodTestLogResponseWithSize";
    EXPECT_TRUE(
        shouldLog(fName, ERR_TIMEOUT, NSM_ERR_NOT_READY, NSM_SW_ERROR, false));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_FALSE(boolArg(fName, 3));
    EXPECT_TRUE(shouldLog(fName, ERR_NOT_SUPPORTED, NSM_ERR_NOT_READY,
                          NSM_SW_SUCCESS, true));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(boolArg(fName, 3));
    EXPECT_TRUE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                          NSM_SW_ERROR_LENGTH, false));
    EXPECT_FALSE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                           NSM_SW_ERROR_LENGTH, false));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(boolArg(fName, 3));
    EXPECT_FALSE(shouldLog(fName, ERR_TIMEOUT, NSM_ERR_INVALID_DATA,
                           NSM_SW_SUCCESS, false));
    EXPECT_FALSE(loggers.empty());
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 1).isAnyBitSet());
    EXPECT_TRUE(bitfieldArg(fName, 2).isAnyBitSet());
    EXPECT_TRUE(boolArg(fName, 3));
    EXPECT_FALSE(
        shouldLog(fName, ERR_NULL, NSM_SUCCESS, NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerTest, GoodTestBoolOnly)
{
    const auto clearLoggerMsg1 = "GoodTestBoolOnly 1";
    const auto clearLoggerMsg2 = "GoodTestBoolOnly 2";
    EXPECT_TRUE(shouldLog(clearLoggerMsg1, true));
    EXPECT_TRUE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, true));
    EXPECT_FALSE(shouldLog(clearLoggerMsg1, false));
    EXPECT_FALSE(shouldLog(clearLoggerMsg2, false));
}

// NSM_ACCEPTED (0x7d) is a second valid success completion code.
// isSuccess(nsm_completion_codes): left side (arg==NSM_SUCCESS) is false,
// right side (arg==NSM_ACCEPTED) is true → covers the || branch.
TEST_F(StateChangeLoggerTest, GoodTestNsmAcceptedIsSuccess)
{
    EXPECT_FALSE(
        shouldLog("GoodTestNsmAccepted", nsm_completion_codes(NSM_ACCEPTED)));
    EXPECT_TRUE(loggers.empty());
}

// appendClearedCodes() for a bool stored-value of false:
// storedValue=false → returns "" → codesText is empty →
// if (!codesText.empty()) is false → nothing appended.
// Trigger path: reason-code error with bool=false, then all success.
TEST_F(StateChangeLoggerTest, GoodTestBoolFalseAppendClearedCodes)
{
    const auto fName = "GoodTestBoolFalseAppendClearedCodes";
    // Error in reason code, bool=false (success) — logger created,
    // bool slot initialised to false and never flipped to true.
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT), false));
    EXPECT_FALSE(loggers.empty());
    // All success → logClearedCodes triggered.
    // appendClearedCodes for bool{false}: "" → empty → not appended.
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL), false));
    EXPECT_TRUE(loggers.empty());
}

// Tests the shouldLog(name, uint16_t reasonCode, uint8_t cc, T rc) overload
// (lines 103–109 in stateChangeLogger.hpp): exercises the function body and
// the downstream variadic template instantiation for the same 3-arg types.
TEST_F(StateChangeLoggerTest,
       ShouldLog_Uint16ReasonCodeOverload_ErrorThenSuccess)
{
    const auto fName = "ShouldLog_Uint16ReasonCode";
    // Explicit casts produce uint16_t / uint8_t → selects the overload
    EXPECT_TRUE(shouldLog(fName, uint16_t(ERR_TIMEOUT), uint8_t(NSM_ERROR),
                          NSM_SW_ERROR));
    EXPECT_FALSE(loggers.empty());
    // All success via the same overload → clears the logger
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

// Tests the shouldLog(name, uint16_t reasonCode, uint8_t cc, T rc, bool)
// overload (lines 113–119 in stateChangeLogger.hpp).
TEST_F(StateChangeLoggerTest,
       ShouldLog_Uint16ReasonCodeWithBoolOverload_ErrorThenSuccess)
{
    const auto fName = "ShouldLog_Uint16WithBool";
    EXPECT_TRUE(shouldLog(fName, uint16_t(ERR_TIMEOUT), uint8_t(NSM_ERROR),
                          NSM_SW_ERROR, true));
    EXPECT_FALSE(loggers.empty());
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// appendClearedCodes type-discriminator coverage (lines 247-252)
// Each test clears a logger that has exactly ONE code type in error so that
// only that type's ternary branch is exercised with non-empty codesText.
// =============================================================================

// Only nsm_reason_codes in error → cleared → "ReasonCodes=[...]" branch.
TEST_F(StateChangeLoggerTest, ClearReasonCodeOnly_ReasonCodesLabel)
{
    const auto fName = "ClearReasonCodeOnly";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_FALSE(loggers.empty());
    // All success → logClearedCodes → appendClearedCodes for reason_codes
    // produces "ReasonCodes=[...]"
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// Only nsm_sw_codes in error → cleared → "ResultCodes=[...]" branch.
TEST_F(StateChangeLoggerTest, ClearSwCodeOnly_ResultCodesLabel)
{
    const auto fName = "ClearSwCodeOnly";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA)));
    EXPECT_FALSE(loggers.empty());
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// Only nsm_completion_codes in error → cleared → "CompletionCodes=[...]"
// branch.
TEST_F(StateChangeLoggerTest, ClearCompletionCodeOnly_CompletionCodesLabel)
{
    const auto fName = "ClearCompletionCodeOnly";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR)));
    EXPECT_FALSE(loggers.empty());
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// Only bool in error (bool=true means error) → cleared → "Flag=[1]" branch.
TEST_F(StateChangeLoggerTest, ClearBoolOnly_FlagLabel)
{
    const auto fName = "ClearBoolOnly";
    EXPECT_TRUE(shouldLog(fName, true));
    EXPECT_FALSE(loggers.empty());
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}

// Two code types cleared → comma separator path (line 256) exercised.
// reason_codes + sw_codes both in error, both cleared at once.
TEST_F(StateChangeLoggerTest, ClearTwoTypes_CommaSeparator)
{
    const auto fName = "ClearTwoTypes";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_FALSE(loggers.empty());
    // All success → clearedCodes = "ReasonCodes=[...], ResultCodes=[...]"
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// Three code types cleared → comma separator inserted twice.
TEST_F(StateChangeLoggerTest, ClearThreeTypes_TwoCommasSeparators)
{
    const auto fName = "ClearThreeTypes";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_ERROR),
                          nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_FALSE(loggers.empty());
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// extractOddArgs static function tests (lines 290, 299, 300)
//
// extractOddArgs(k0, v0, k1, v1, ...) extracts odd-indexed values (v0, v1, ...)
// that match AllowedTypesToExtract.  Because IsAllowedTypeToExtract checks
// T == std::tuple<...> (always false for individual types), extractAllowedTuple
// always returns std::tuple<> — covering the false branch at line 281.
// =============================================================================

TEST(StateChangeLoggerExtractOddArgs, OnePair_ReturnsEmptyTuple)
{
    // One (key, value) pair.  Value type bool is not AllowedTypesToExtract
    // (which compares against the whole tuple type) → false branch.
    auto result = StateChangeLogger::extractOddArgs(std::string("key"),
                                                    bool{true});
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgs, TwoPairs_ReturnsEmptyTuple)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k1"), uint8_t(1), std::string("k2"), uint16_t(2));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgs, FourPairs_ReturnsEmptyTuple)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k1"), int(1), std::string("k2"),
        nsm_sw_codes(NSM_SW_SUCCESS), std::string("k3"),
        nsm_reason_codes(ERR_NULL), std::string("k4"),
        nsm_completion_codes(NSM_SUCCESS));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}
