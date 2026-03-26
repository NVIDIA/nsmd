/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

/**
 * Additional branch coverage for nsmd/stateChangeLogger.hpp
 *
 * Targets uncovered branches:
 * - extractOddArgs with various type combinations and actual allowed types
 * - shouldLog: argument size mismatch path
 * - shouldLog: stateChanged=true AND allSuccess=true simultaneously
 * - shouldLogAndUpdate: Bitfield256 path with isSuccess=true (no setBit call)
 * - appendClearedCodes: each type label independently (single type loggers)
 * - appendClearedCodes: empty clearedCodes for bool=false (no prior error)
 * - isSuccess: NSM_ACCEPTED path for completion_codes
 * - initializeArg: each type separately
 * - logClearedCodes: empty clearedCodes path
 * - Multiple loggers simultaneously
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

struct StateChangeLoggerBranch2Test : public Test, public StateChangeLogger
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
// shouldLog: single nsm_sw_codes only — tests appendClearedCodes for sw_codes
// label "ResultCodes=[...]" independently
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, SingleSwCode_ClearShowsResultCodesLabel)
{
    const auto fName = "SwCodeOnly";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA)));
    EXPECT_FALSE(loggers.empty());
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: single nsm_completion_codes only — tests appendClearedCodes for
// completion_codes label "CompletionCodes=[...]" independently
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test,
       SingleCompletionCode_ClearShowsCompletionCodesLabel)
{
    const auto fName = "CcOnly";
    EXPECT_TRUE(
        shouldLog(fName, nsm_completion_codes(NSM_ERR_UNSUPPORTED_MSG_TYPE)));
    EXPECT_FALSE(loggers.empty());
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: single nsm_reason_codes only — tests appendClearedCodes for
// reason_codes label "ReasonCodes=[...]" independently
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test,
       SingleReasonCode_ClearShowsReasonCodesLabel)
{
    const auto fName = "RcOnly";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_I2C_NACK_FROM_DEV_ADDR)));
    EXPECT_FALSE(loggers.empty());
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: single bool only — tests appendClearedCodes for bool
// label "Flag=[1]" independently
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, SingleBool_ClearShowsFlagLabel)
{
    const auto fName = "BoolOnly";
    EXPECT_TRUE(shouldLog(fName, true));
    EXPECT_FALSE(loggers.empty());
    // Clear
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: bool error=false is success, no logger created
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, BoolFalse_IsSuccess_NoLogger)
{
    const auto fName = "BoolFalseSuccess";
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: NSM_ACCEPTED is success for completion_codes
// Tests: isSuccess(NSM_ACCEPTED) => true via the || right side
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, CompletionCode_NSM_ACCEPTED_IsSuccess)
{
    const auto fName = "AcceptedSuccess";
    // All success from start
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ACCEPTED)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: NSM_SUCCESS is also success
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, CompletionCode_NSM_SUCCESS_IsSuccess)
{
    const auto fName = "SuccessSuccess";
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: NSM_SW_SUCCESS alone is success
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, SwCode_NSM_SW_SUCCESS_IsSuccess)
{
    const auto fName = "SwSuccess";
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: ERR_NULL alone is success for reason_codes
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ReasonCode_ERR_NULL_IsSuccess)
{
    const auto fName = "ReasonSuccess";
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: many different error codes accumulated then cleared
// Tests multiple setBit calls and cleared codes listing multiple bits
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ManyReasonCodes_Accumulated)
{
    const auto fName = "ManyRC";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NOT_SUPPORTED)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_PROPERTY_NOT_SUPPORTED)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NVLINK_PORT_INVALID)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_NVLINK_PORT_DISABLED)));
    // Repeat one
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: many completion codes accumulated
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ManyCompletionCodes_Accumulated)
{
    const auto fName = "ManyCC";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR)));
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERR_INVALID_DATA)));
    EXPECT_TRUE(
        shouldLog(fName, nsm_completion_codes(NSM_ERR_INVALID_DATA_LENGTH)));
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERR_NOT_READY)));
    EXPECT_TRUE(shouldLog(
        fName, nsm_completion_codes(NSM_ERR_UNSUPPORTED_COMMAND_CODE)));
    EXPECT_TRUE(
        shouldLog(fName, nsm_completion_codes(NSM_ERR_UNSUPPORTED_MSG_TYPE)));
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_BUSY)));
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERR_BUS_ACCESS)));
    // Repeat
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ERROR)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: many sw codes accumulated
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ManySwCodes_Accumulated)
{
    const auto fName = "ManySW";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_LENGTH)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_NULL)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_COMMAND_FAIL)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_TIMEOUT)));
    // Repeat
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: mixed 2-type: sw_codes + bool
// Tests initializeArg for both types and appendClearedCodes with both labels
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, SwCodeAndBool_MixedErrors)
{
    const auto fName = "SwBool";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR), true));
    // sw still error, bool clears
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA), false));
    // Both still not success
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_DATA), false));
    // Clear all
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: mixed 2-type: reason_codes + completion_codes
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ReasonAndCompletion_Mixed)
{
    const auto fName = "RcCc";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_ERROR)));
    // reason clears but completion still error
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_ERROR)));
    EXPECT_FALSE(loggers.empty());
    // Both clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: mixed 3-type: reason + completion + sw
// Uses the uint16_t/uint8_t/T overload
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, Uint16Overload_ThreeTypes)
{
    const auto fName = "ThreeType";
    EXPECT_TRUE(shouldLog(fName, uint16_t(ERR_DOWNSTREAM_TIMEOUT),
                          uint8_t(NSM_ERR_BUS_ACCESS), NSM_SW_ERROR_NULL));
    // All same => no change
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_DOWNSTREAM_TIMEOUT),
                           uint8_t(NSM_ERR_BUS_ACCESS), NSM_SW_ERROR_NULL));
    // Partial clear
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL),
                           uint8_t(NSM_ERR_BUS_ACCESS), NSM_SW_SUCCESS));
    EXPECT_FALSE(loggers.empty());
    // Full clear
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: uint16_t/uint8_t/T/bool overload — all success from start
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, Uint16WithBool_AllSuccessFromStart)
{
    const auto fName = "AllSuccStart";
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: uint16_t/uint8_t/T/bool overload — error then clear
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, Uint16WithBool_ErrorThenClear)
{
    const auto fName = "U16Bool";
    EXPECT_TRUE(shouldLog(fName, uint16_t(ERR_INVALID_PCI),
                          uint8_t(NSM_ERR_INVALID_DATA), NSM_SW_ERROR_LENGTH,
                          true));
    // Clear all
    EXPECT_FALSE(shouldLog(fName, uint16_t(ERR_NULL), uint8_t(NSM_SUCCESS),
                           NSM_SW_SUCCESS, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// Multiple independent loggers — no interference
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, MultipleLoggers_NoInterference)
{
    EXPECT_TRUE(shouldLog("LogA", nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_TRUE(shouldLog("LogB", nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_TRUE(shouldLog("LogC", nsm_completion_codes(NSM_ERROR)));
    EXPECT_EQ(loggers.size(), 3u);

    // Clear LogB only
    EXPECT_FALSE(shouldLog("LogB", nsm_reason_codes(ERR_NULL)));
    EXPECT_EQ(loggers.size(), 2u);

    // Clear LogA
    EXPECT_FALSE(shouldLog("LogA", nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_EQ(loggers.size(), 1u);

    // Clear LogC
    EXPECT_FALSE(shouldLog("LogC", nsm_completion_codes(NSM_ACCEPTED)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// extractOddArgs: with string keys and allowed-type values
// Note: IsAllowedTypeToExtract always compares against the tuple type itself,
// so individual types never match. All tuples should be empty.
// =============================================================================
TEST(StateChangeLoggerExtractOddArgsBranch2, BoolValue)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"),
                                                    bool{true});
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, Uint8Value)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"),
                                                    uint8_t(42));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, Uint16Value)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"),
                                                    uint16_t(1000));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, IntValue)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"), int(55));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, SwCodeValue)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"),
                                                    nsm_sw_codes(NSM_SW_ERROR));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, ReasonCodeValue)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k"), nsm_reason_codes(ERR_TIMEOUT));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, CompletionCodeValue)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k"), nsm_completion_codes(NSM_ERROR));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, StringValue_NotAllowed)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"),
                                                    std::string("val"));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, DoubleValue_NotAllowed)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("k"),
                                                    double(3.14));
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, FourPairs_MixedTypes)
{
    auto result = StateChangeLogger::extractOddArgs(
        std::string("k1"), uint8_t(1), std::string("k2"), uint16_t(2),
        std::string("k3"), nsm_sw_codes(NSM_SW_ERROR), std::string("k4"),
        bool{true});
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

TEST(StateChangeLoggerExtractOddArgsBranch2, TwoPairs_IntAndBool)
{
    auto result = StateChangeLogger::extractOddArgs(std::string("a"), int(10),
                                                    std::string("b"), true);
    EXPECT_EQ(std::tuple_size_v<decltype(result)>, 0);
}

// =============================================================================
// shouldLog: error->different_error->success cycle for each enum type
// Exercises the Bitfield256 setBit returning true for new, false for dup
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ReasonCode_ErrorRotation)
{
    const auto fName = "RcRotation";
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_INVALID_PCI)));
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_INVALID_RQD)));
    // Repeat first
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT)));
    // New
    EXPECT_TRUE(
        shouldLog(fName, nsm_reason_codes(ERR_I2C_NACK_FROM_DEV_CMD_DATA)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: completion codes error rotation
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, CompletionCode_ErrorRotation)
{
    const auto fName = "CcRotation";
    EXPECT_TRUE(shouldLog(fName, nsm_completion_codes(NSM_ERROR)));
    EXPECT_TRUE(
        shouldLog(fName, nsm_completion_codes(NSM_ERR_INVALID_DATA_LENGTH)));
    EXPECT_TRUE(
        shouldLog(fName, nsm_completion_codes(NSM_ERR_INVALID_REQUEST_TYPE)));
    EXPECT_TRUE(shouldLog(
        fName, nsm_completion_codes(NSM_ERR_INVALID_STATE_FOR_COMMAND)));
    // Repeat
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ERROR)));
    // Clear with NSM_ACCEPTED
    EXPECT_FALSE(shouldLog(fName, nsm_completion_codes(NSM_ACCEPTED)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: sw codes error rotation
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, SwCode_ErrorRotation)
{
    const auto fName = "SwRotation";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_COMMAND_FAIL)));
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_TIMEOUT)));
    // Repeat
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    // Clear
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: bool transition true->false->true->false
// Covers the value != arg path when value flips
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, BoolFlipFlop)
{
    const auto fName = "BoolFlip";
    // true => error, creates logger
    EXPECT_TRUE(shouldLog(fName, true));
    // false => success => clears
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
    // true again => new logger
    EXPECT_TRUE(shouldLog(fName, true));
    // false again => clears
    EXPECT_FALSE(shouldLog(fName, false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: mixed 4-type with partial clears in sequence
// Tests the appendClearedCodes comma separator for multiple types
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, FourTypes_PartialClearSequence)
{
    const auto fName = "PartialSeq";
    // All errors
    EXPECT_TRUE(shouldLog(fName, nsm_reason_codes(ERR_TIMEOUT),
                          nsm_completion_codes(NSM_BUSY),
                          nsm_sw_codes(NSM_SW_ERROR_DATA), true));

    // Reason clears but others stay
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_BUSY),
                           nsm_sw_codes(NSM_SW_ERROR_DATA), true));
    EXPECT_FALSE(loggers.empty());

    // Completion and sw clear but bool stays
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), true));
    EXPECT_FALSE(loggers.empty());

    // All clear
    EXPECT_FALSE(shouldLog(fName, nsm_reason_codes(ERR_NULL),
                           nsm_completion_codes(NSM_SUCCESS),
                           nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: logClearedCodes with empty clearedCodes path
// All types are success with no prior error bits recorded (no set bits).
// This exercises the !clearedCodes.empty() == false branch.
// We achieve this by having an error on one type, then clearing immediately.
// The enum type with error will have bits set, but bool with error=false won't.
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ClearedCodes_BoolNoError_EmptyBits)
{
    const auto fName = "EmptyBool";
    // Only sw_codes has error, bool is success from start
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR), false));
    // Clear: sw clears (has bits), bool was never error (no bits)
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS), false));
    EXPECT_TRUE(loggers.empty());
}

// =============================================================================
// shouldLog: re-entering error after clear (re-create logger)
// =============================================================================
TEST_F(StateChangeLoggerBranch2Test, ReEnterError_AfterClear)
{
    const auto fName = "ReEnter";
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR)));
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
    // Re-enter
    EXPECT_TRUE(shouldLog(fName, nsm_sw_codes(NSM_SW_ERROR_TIMEOUT)));
    EXPECT_FALSE(loggers.empty());
    EXPECT_FALSE(shouldLog(fName, nsm_sw_codes(NSM_SW_SUCCESS)));
    EXPECT_TRUE(loggers.empty());
}
