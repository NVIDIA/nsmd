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

/**
 * Branch coverage for nsmd/stateChangeLogger.hpp
 *
 * Targets uncovered branches:
 * - shouldLog: various combinations of all-success / partial-success
 * - shouldLogAndUpdate: Bitfield256 setBit returning false (duplicate)
 * - appendClearedCodes: all four type-ternary branches with non-empty
 *   clearedCodes (comma separator) and empty clearedCodes
 * - extractOddArgs: template instantiation with different type combos
 * - isSuccess: all constexpr branches for each allowed type
 * - initializeArg: bool vs. enum branches
 * - flooding suppression: same error state repeated
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

struct StateChangeLoggerBranchTest : public Test, public StateChangeLogger
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

// =============================================================================
// shouldLog: single nsm_reason_codes error then immediate success in next call
// Exercises: !loggerExists && !allSuccess => create, then allSuccess => clear
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, SingleReasonCodeError_ThenClear)
{
    const auto fName = "SingleReasonCodeError_ThenClear";
    // Error: creates logger, returns true
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_FALSE(loggers.empty());
    // Different error: already stored, setBit returns true
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED)));
    // Same error again: setBit returns false (already set), stateChanged=false
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED)));
    // Success: clears logger
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: single nsm_completion_codes error with NSM_ACCEPTED success
// Exercises: isSuccess(NSM_ACCEPTED) => true (the || right side)
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, CompletionCode_AcceptedIsAlsoSuccess)
{
    const auto fName = "CompletionCode_Accepted";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR)));
    EXPECT_FALSE(loggers.empty());
    // NSM_ACCEPTED is also success
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ACCEPTED)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: bool type transitions: false -> true -> true -> false
// Exercises: shouldLogAndUpdate for bool: value != arg check,
//            value update, repeated same value (no change)
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, BoolType_MultipleTransitions)
{
    const auto fName = "BoolTransitions";
    // bool=true is error; first call creates logger, returns true
    EXPECT_TRUE(shouldLog(fName, true));
    EXPECT_FALSE(loggers.empty());
    // Same error (true) again: no state change
    EXPECT_FALSE(shouldLog(fName, true));
    // Success (false): clears logger
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog with mixed types: reason_codes + completion_codes + sw_codes + bool
// Tests all 4 initializeArg branches and all 4 isSuccess branches.
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, MixedFourTypes_AllBranches)
{
    const auto fName = "MixedFourTypes";
    // All errors
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_ERROR),
                          nsm_sw_codes(NSM_SW_ERROR), true));
    EXPECT_FALSE(loggers.empty());
    // Partial success (only reason cleared)
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_ERROR),
                           nsm_sw_codes(NSM_SW_ERROR), true));
    EXPECT_FALSE(loggers.empty());
    // All success => clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: all success from the start => no logger created
// Tests: !loggerExists && allSuccess => return false
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, AllSuccessFromStart_NoLoggerCreated)
{
    const auto fName = "AllSuccessFromStart";
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: flooding suppression test
// Same error repeated many times => only first returns true
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, FloodingSuppression_RepeatedErrors)
{
    const auto fName = "FloodingSuppression";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    for (int i = 0; i < 10; i++)
    {
        EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    }
    // New different error code
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: uint16_t + uint8_t overload with multiple error/success cycles
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, Uint16ReasonCodeOverload_MultipleCycles)
{
    const auto fName = "Uint16Multi";
    EXPECT_TRUE(shouldLog(fName, uint16_t(ERR_TIMEOUT), uint8_t(NSM_ERROR),
                          NSM_SW_ERROR));
    // Same state
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_TIMEOUT), uint8_t(NSM_ERROR),
                           NSM_SW_ERROR));
    // Clear
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: uint16_t + uint8_t + bool overload
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, Uint16ReasonCodeWithBool_MultipleCycles)
{
    const auto fName = "Uint16Bool";
    EXPECT_TRUE(shouldLog(fName, uint16_t(ERR_TIMEOUT), uint8_t(NSM_ERROR),
                          NSM_SW_ERROR, true));
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_TIMEOUT), uint8_t(NSM_ERROR),
                           NSM_SW_ERROR, true));
    // Partial success
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_ERROR),
                           NSM_SW_SUCCESS, false));
    // Full success
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// appendClearedCodes: all four type labels in a single clear
// reason_codes, completion_codes, sw_codes, bool all in error then all cleared.
// Exercises all four ternary branches with non-empty codesText and the
// comma separator path multiple times.
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, AllFourTypesCleared_AllLabels)
{
    const auto fName = "AllFourLabels";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_ERROR),
                          nsm_sw_codes(NSM_SW_ERROR_DATA), true));
    EXPECT_FALSE(loggers.empty());
    // Clear all at once
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: error in bool only, other types success => logger created,
// tests initializeArg(bool) and shouldLogAndUpdate for bool
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, BoolErrorOnly_OtherTypesSuccess)
{
    const auto fName = "BoolErrorOnly";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                          nsm_completion_codes(NSM_SUCCESS),
                          nsm_sw_codes(NSM_SW_SUCCESS), true));
    EXPECT_FALSE(loggers.empty());
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: stateChanged false but not allSuccess => logger persists
// Only one type in error, others success. That type stays the same => no change
// but not allSuccess => logger not erased
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, NoStateChange_NotAllSuccess_Persists)
{
    const auto fName = "Persists";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR),
                          nsm_completion_codes(NSM_SUCCESS)));
    // Same sw error, same completion success => no state change, not all
    // success
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR),
                           nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_FALSE(loggers.empty());
    // Now clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS),
                           nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// extractOddArgs: three pairs with mixed types
// =============================================================================
TEST(StateChangeLoggerExtractOddArgsBranch, ThreePairs_MixedTypes)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k1"), uint16_t(42), std::string("k2"),
        nsm_sw_codes(NSM_SW_SUCCESS), std::string("k3"), bool{false});
    // All values are filtered through IsAllowedTypeToExtract which checks
    // against the whole tuple type => always empty tuple
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

// =============================================================================
// extractOddArgs: single pair with int value
// =============================================================================
TEST(StateChangeLoggerExtractOddArgsBranch, OnePair_IntValue)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("key"),
                                                    int(99));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

// =============================================================================
// extractOddArgs: single pair with nsm_reason_codes value
// =============================================================================
TEST(StateChangeLoggerExtractOddArgsBranch, OnePair_ReasonCodeValue)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("key"), nsm_reason_codes(ERR_TIMEOUT));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

// =============================================================================
// extractOddArgs: single pair with nsm_completion_codes value
// =============================================================================
TEST(StateChangeLoggerExtractOddArgsBranch, OnePair_CompletionCodeValue)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("key"), nsm_completion_codes(NSM_ERROR));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

// =============================================================================
// shouldLog: multiple different error codes for same type accumulate in
// bitfield Tests setBit returning true for new codes and false for duplicates
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, MultipleDifferentCodes_Accumulate)
{
    const auto fName = "Accumulate";
    // First error
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_TRUE(bitfieldArg(fName, 0).isAnyBitSet());
    // Second different error
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA)));
    // Third different error
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_LENGTH)));
    // Repeat second error => no state change
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: bool transition from true back to true (no change) then false
// Covers the (value != arg) = false path when both are true
// =============================================================================
TEST_F(StateChangeLoggerBranchTest, BoolTrueToTrue_NoChange)
{
    const auto fName = "BoolNoChange";
    EXPECT_TRUE(shouldLog(fName, true));
    // true again => no change
    EXPECT_FALSE(shouldLog(fName, true));
    EXPECT_FALSE(shouldLog(fName, true));
    // clear
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}
