/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/**
 * Additional branch coverage for nsmd/stateChangeLogger.hpp
 *
 * Targets remaining uncovered template instantiation branches:
 * - Line 46:  virtual destructor via polymorphic deletion
 * - Lines 83-84, 86, 88: argument size mismatch error path
 * - Lines 91, 98: shouldLogAndUpdate / return stateChanged for new combos
 * - Lines 94-95: logClearedCodes + erase for new type combos
 * - Lines 131, 133: createStateArgs for new type orderings
 * - Lines 178-179: shouldLogAndUpdate variadic for new combos
 * - Lines 210, 213-214, 221: logClearedCodes for new combos
 * - Lines 232, 236: appendClearedCodes Bitfield256/bool branches
 *
 * New type combinations exercised:
 * - reason + bool (2-type)
 * - completion + bool (2-type)
 * - bool + reason (reversed order)
 * - bool + completion (reversed order)
 * - bool + sw (reversed order)
 * - bool + reason + completion (3-type with bool first)
 * - completion + sw + bool (3-type)
 * - reason + sw + bool (3-type)
 * - reason + sw (2-type, not yet covered)
 * - completion + sw (2-type, not yet covered)
 * - Multiple rapid state changes and same-value no-log paths
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

struct StateChangeLoggerBranch3Test : public Test, public StateChangeLogger
{
    const utils::Bitfield256& bitfieldArg(const std::string& fName,
                                          size_t index = 0)
    {
        auto& logger = loggers[fName];
        return std::get<utils::Bitfield256>(logger[index]);
    }
    auto boolArg(const std::string& fName, size_t index)
    {
        auto& logger = loggers[fName];
        return std::get<bool>(logger[index]);
    }
};

// =============================================================================
// Virtual destructor coverage (line 46): polymorphic deletion through base ptr
// =============================================================================
TEST(StateChangeLoggerBranch3Destructor, PolymorphicDeletion)
{
    std::unique_ptr<StateChangeLogger> ptr =
        std::make_unique<StateChangeLogger>();
    // Triggers virtual ~StateChangeLogger() via base pointer
    ptr.reset();
    EXPECT_EQ(ptr, nullptr);
}

// =============================================================================
// Argument size mismatch path (lines 83-84, 86, 88)
// Manually inject a logger with wrong arg count then call shouldLog
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, ArgSizeMismatch_ReturnsTrue)
{
    const auto fName = "SizeMismatch";
    // Create a logger with 1 arg
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_EQ(loggers[fName].size(), 1u);
    // Now call with 2 args => size mismatch => returns true
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR),
                          nsm_completion_codes(NSM_ERROR)));
}

TEST_F(StateChangeLoggerBranch3Test, ArgSizeMismatch_MoreArgs)
{
    const auto fName = "SizeMismatch2";
    // Create a logger with 2 args
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_ERROR)));
    EXPECT_EQ(loggers[fName].size(), 2u);
    // Call with 1 arg => mismatch
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
}

TEST_F(StateChangeLoggerBranch3Test, ArgSizeMismatch_BoolVsEnum)
{
    const auto fName = "SizeMismatch3";
    // Create with bool only
    EXPECT_TRUE(shouldLog(fName, true));
    EXPECT_EQ(loggers[fName].size(), 1u);
    // Call with reason + bool => mismatch (2 vs 1)
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT), true));
}

// =============================================================================
// reason + bool: 2-type combo not previously tested
// Exercises createStateArgs, shouldLogAndUpdate, appendClearedCodes for this
// specific template instantiation
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, ReasonAndBool_ErrorThenClear)
{
    const auto fName = "ReasonBool";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT), true));
    EXPECT_FALSE(loggers.empty());
    // Same state
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT), true));
    // Partial: reason clears, bool stays
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL), true));
    EXPECT_FALSE(loggers.empty());
    // Full clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// completion + bool: 2-type combo not previously tested
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, CompletionAndBool_ErrorThenClear)
{
    const auto fName = "CcBool";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR), true));
    EXPECT_FALSE(loggers.empty());
    // Same state => no change
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ERROR), true));
    // Clear all
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// bool + reason: reversed order (bool first)
// Different template instantiation from reason + bool
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, BoolThenReason_ReversedOrder)
{
    const auto fName = "BoolReason";
    EXPECT_TRUE(shouldLog(fName, true, nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_FALSE(loggers.empty());
    // Same state
    EXPECT_FALSE(shouldLog(fName, true, nsm_reason_codes(ERR_TIMEOUT)));
    // Clear all
    EXPECT_FALSE(shouldLog(fName, false, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// bool + completion: reversed order (bool first)
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, BoolThenCompletion_ReversedOrder)
{
    const auto fName = "BoolCc";
    EXPECT_TRUE(shouldLog(fName, true, nsm_completion_codes(NSM_BUSY)));
    EXPECT_FALSE(loggers.empty());
    // Different completion error
    EXPECT_TRUE(shouldLog(fName, true, nsm_completion_codes(NSM_ERROR)));
    // Same state
    EXPECT_FALSE(shouldLog(fName, true, nsm_completion_codes(NSM_ERROR)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, false, nsm_completion_codes(NSM_ACCEPTED)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// bool + sw: reversed order (bool first)
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, BoolThenSw_ReversedOrder)
{
    const auto fName = "BoolSw";
    EXPECT_TRUE(shouldLog(fName, true, nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_FALSE(loggers.empty());
    // Clear
    EXPECT_FALSE(shouldLog(fName, false, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// reason + sw: 2-type combo not previously tested
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, ReasonAndSw_ErrorThenClear)
{
    const auto fName = "ReasonSw";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED),
                          nsm_sw_codes(NSM_SW_ERROR_DATA)));
    EXPECT_FALSE(loggers.empty());
    // Same state
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED),
                           nsm_sw_codes(NSM_SW_ERROR_DATA)));
    // Full clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// completion + sw: 2-type combo not previously tested
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, CompletionAndSw_ErrorThenClear)
{
    const auto fName = "CcSw";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERR_NOT_READY),
                          nsm_sw_codes(NSM_SW_ERROR_NULL)));
    EXPECT_FALSE(loggers.empty());
    // Partial: completion clears, sw stays
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_ERROR_NULL)));
    EXPECT_FALSE(loggers.empty());
    // Full clear
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// bool + reason + completion: 3-type with bool first
// New template instantiation ordering
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, BoolReasonCompletion_ThreeTypes)
{
    const auto fName = "BoolRcCc";
    EXPECT_TRUE(shouldLog(fName, true, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_ERROR)));
    EXPECT_FALSE(loggers.empty());
    // Same state
    EXPECT_FALSE(shouldLog(fName, true, nsm_reason_codes(ERR_TIMEOUT),
                           nsm_completion_codes(NSM_ERROR)));
    // New reason error
    EXPECT_TRUE(shouldLog(fName, true, nsm_reason_codes(ERR_NOT_SUPPORTED),
                          nsm_completion_codes(NSM_ERROR)));
    // Clear all
    EXPECT_FALSE(shouldLog(fName, false, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// completion + sw + bool: 3-type without reason
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, CompletionSwBool_ThreeTypes)
{
    const auto fName = "CcSwBool";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR),
                          nsm_sw_codes(NSM_SW_ERROR), true));
    EXPECT_FALSE(loggers.empty());
    // Partial clear: completion ok, sw still error, bool still error
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_ERROR), true));
    EXPECT_FALSE(loggers.empty());
    // Full clear
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// reason + sw + bool: 3-type without completion
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, ReasonSwBool_ThreeTypes)
{
    const auto fName = "RcSwBool";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_sw_codes(NSM_SW_ERROR_TIMEOUT), true));
    EXPECT_FALSE(loggers.empty());
    // Same state
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                           nsm_sw_codes(NSM_SW_ERROR_TIMEOUT), true));
    // Full clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// Multiple rapid state changes: error rotations with mixed types
// Exercises shouldLogAndUpdate with stateChanged accumulation across types
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, RapidStateChanges_MixedTypes)
{
    const auto fName = "Rapid";
    // Initial error
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_sw_codes(NSM_SW_ERROR)));
    // Both change to new errors
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED),
                          nsm_sw_codes(NSM_SW_ERROR_DATA)));
    // Only one changes
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_PROPERTY_NOT_SUPPORTED),
                          nsm_sw_codes(NSM_SW_ERROR_DATA)));
    // Other one changes
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_PROPERTY_NOT_SUPPORTED),
                          nsm_sw_codes(NSM_SW_ERROR_LENGTH)));
    // Neither changes
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_PROPERTY_NOT_SUPPORTED),
                           nsm_sw_codes(NSM_SW_ERROR_LENGTH)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// Edge case: all success from start with new type combos (no logger created)
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, AllSuccessFromStart_ReasonBool)
{
    EXPECT_FALSE(shouldLog("SuccRB", nsm_reason_codes(ERR_NULL), false));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerBranch3Test, AllSuccessFromStart_CompletionBool)
{
    EXPECT_FALSE(shouldLog("SuccCB", nsm_completion_codes(NSM_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerBranch3Test, AllSuccessFromStart_BoolReason)
{
    EXPECT_FALSE(shouldLog("SuccBR", false, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerBranch3Test, AllSuccessFromStart_ReasonSw)
{
    EXPECT_FALSE(shouldLog("SuccRS", nsm_reason_codes(ERR_NULL),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

TEST_F(StateChangeLoggerBranch3Test, AllSuccessFromStart_CompletionSw)
{
    EXPECT_FALSE(shouldLog("SuccCS", nsm_completion_codes(NSM_ACCEPTED),
                           nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// Bool edge: error false->true->false->true->false rapid flip
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, BoolRapidFlip)
{
    const auto fName = "BoolRapid";
    // false is success, no logger
    EXPECT_FALSE(shouldLog(fName, false));
    // true is error, creates logger
    EXPECT_TRUE(shouldLog(fName, true));
    // false clears
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
    // true again
    EXPECT_TRUE(shouldLog(fName, true));
    // true again no change
    EXPECT_FALSE(shouldLog(fName, true));
    // false clears
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// sw + reason (reversed from reason + sw): different template instantiation
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, SwThenReason_ReversedOrder)
{
    const auto fName = "SwReason";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR),
                          nsm_reason_codes(ERR_TIMEOUT)));
    // Same
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR),
                           nsm_reason_codes(ERR_TIMEOUT)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS),
                           nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// completion + reason (reversed from reason + completion): new instantiation
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, CompletionThenReason_ReversedOrder)
{
    const auto fName = "CcReason";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR),
                          nsm_reason_codes(ERR_NOT_SUPPORTED)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ACCEPTED),
                           nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// sw + completion (reversed from completion + sw): new instantiation
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, SwThenCompletion_ReversedOrder)
{
    const auto fName = "SwCc";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_COMMAND_FAIL),
                          nsm_completion_codes(NSM_ERR_INVALID_DATA)));
    // Different errors
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_TIMEOUT),
                          nsm_completion_codes(NSM_ERR_NOT_READY)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS),
                           nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// 4-type reversed: bool + sw + completion + reason
// Different ordering from the existing reason+completion+sw+bool
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, FourTypes_ReversedOrder)
{
    const auto fName = "FourReversed";
    EXPECT_TRUE(shouldLog(fName, true, nsm_sw_codes(NSM_SW_ERROR),
                          nsm_completion_codes(NSM_ERROR),
                          nsm_reason_codes(ERR_TIMEOUT)));
    // Same state
    EXPECT_FALSE(shouldLog(fName, true, nsm_sw_codes(NSM_SW_ERROR),
                           nsm_completion_codes(NSM_ERROR),
                           nsm_reason_codes(ERR_TIMEOUT)));
    // Full clear
    EXPECT_FALSE(shouldLog(fName, false, nsm_sw_codes(NSM_SW_SUCCESS),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// Single-type error with multiple different codes accumulated, then clear
// For each enum type, accumulate codes to exercise bitfield setBit paths
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, ReasonCodes_AccumulateAndClear)
{
    const auto fName = "RcAccum";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_DOWNSTREAM_TIMEOUT)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_I2C_NACK_FROM_DEV_ADDR)));
    // Duplicate
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// Mixed type: one type errors while other stays success the whole time
// Tests the partial-success path where stateChanged=false but not allSuccess
// =============================================================================
TEST_F(StateChangeLoggerBranch3Test, OneTypeAlwaysSuccess_OtherErrors)
{
    const auto fName = "PartialOnly";
    // reason=error, bool=success(false)
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT), false));
    // reason still error (same), bool still success => no change, not all succ
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT), false));
    EXPECT_FALSE(loggers.empty());
    // reason new error, bool still success
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED), false));
    // Full clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// extractOddArgs: new combinations for template instantiation coverage
// =============================================================================
TEST(StateChangeLoggerExtractOddArgsBranch3, SixPairs_AllTypes)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("a"), bool{true}, std::string("b"), uint8_t(1),
        std::string("c"), uint16_t(2), std::string("d"), int(3),
        std::string("e"), nsm_sw_codes(NSM_SW_ERROR), std::string("f"),
        nsm_reason_codes(ERR_TIMEOUT));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch3, TwoPairs_CompletionAndReason)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k1"), nsm_completion_codes(NSM_ERROR), std::string("k2"),
        nsm_reason_codes(ERR_TIMEOUT));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch3, ThreePairs_BoolUint8Uint16)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("a"), bool{false}, std::string("b"), uint8_t(255),
        std::string("c"), uint16_t(65535));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}
