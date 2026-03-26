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

/*
 * Branch coverage for nsmd/nsmEvent/nsmEventInfo.hpp
 *
 * Covers:
 * - getEventErrorId L42 FALSE: odd-size errorId → error log, return ""
 * - getEventErrorId L46 FALSE (it==end): even-size, key not found
 * - getEventErrorId L46 FALSE (it+1==end): key at last position
 * - getEventErrorId L46 TRUE + L48: key found, value returned
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public

#include "nsmEvent/nsmEventInfo.hpp"

using namespace nsm;
using namespace ::testing;

// ============================================================================
// Helper
// ============================================================================

static NsmEventInfo makeInfo(std::vector<std::string> errorId)
{
    NsmEventInfo info{};
    info.uuid = "test-uuid";
    info.errorId = std::move(errorId);
    return info;
}

// ============================================================================
// L42 FALSE: odd-size errorId → goes to error-log branch at end of function
// ============================================================================

TEST(GetEventErrorId, OddSizeErrorId_ReturnsEmpty)
{
    // 3 elements → size % 2 != 0 → FALSE branch of L42
    auto info = makeInfo({"key1", "val1", "extra"});
    auto result = getEventErrorId(info, "key1");
    EXPECT_EQ(result, "");
}

TEST(GetEventErrorId, SingleElementErrorId_ReturnsEmpty)
{
    // 1 element → size % 2 != 0 → FALSE branch of L42
    auto info = makeInfo({"onlyone"});
    auto result = getEventErrorId(info, "onlyone");
    EXPECT_EQ(result, "");
}

// ============================================================================
// L42 TRUE but L46 FALSE via it == end: even-size, key not found
// ============================================================================

TEST(GetEventErrorId, EvenSizeKeyNotFound_ReturnsEmpty)
{
    // 2 elements, key "missing" not in errorId → it==end → FALSE branch of L46
    auto info = makeInfo({"key1", "val1"});
    auto result = getEventErrorId(info, "missing");
    EXPECT_EQ(result, "");
}

TEST(GetEventErrorId, EmptyErrorId_ReturnsEmpty)
{
    // 0 elements → size % 2 == 0 → TRUE branch of L42
    // find returns end immediately → FALSE branch of L46
    auto info = makeInfo({});
    auto result = getEventErrorId(info, "somekey");
    EXPECT_EQ(result, "");
}

// ============================================================================
// L42 TRUE but L46 FALSE via it+1 == end: key at last position
// Key is the last element and has no following value → it+1 == end
// ============================================================================

TEST(GetEventErrorId, KeyAtLastPosition_ReturnsEmpty)
{
    // errorId = {"key1", "val1", "dangling_key"} → odd size → L42 FALSE
    // Actually we need EVEN size with key at end:
    // errorId = {"other", "key1", "val1", "dangling"} size=4 (even)
    // look for "dangling" → it points to index 3 (last), it+1 == end
    auto info = makeInfo({"other", "key1", "val1", "dangling"});
    auto result = getEventErrorId(info, "dangling");
    EXPECT_EQ(result, "");
}

// ============================================================================
// L42 TRUE + L46 TRUE + L48: key found with a following value
// ============================================================================

TEST(GetEventErrorId, KeyFound_ReturnsValue)
{
    // errorId = {"k1", "v1"} → size=2 (even) → L42 TRUE
    // find "k1" → it points to 0, it+1 points to 1 → TRUE branch L46
    // returns "v1"
    auto info = makeInfo({"k1", "v1"});
    auto result = getEventErrorId(info, "k1");
    EXPECT_EQ(result, "v1");
}

TEST(GetEventErrorId, MultipleKeyValuePairs_CorrectKeyFound)
{
    // 4 elements: two key-value pairs
    auto info = makeInfo({"k1", "v1", "k2", "v2"});
    EXPECT_EQ(getEventErrorId(info, "k1"), "v1");
    EXPECT_EQ(getEventErrorId(info, "k2"), "v2");
    EXPECT_EQ(getEventErrorId(info, "missing"), "");
}

TEST(GetEventErrorId, KeyInMiddle_ReturnsValue)
{
    // 6 elements: key at index 2 (middle)
    auto info = makeInfo({"a", "1", "target", "found!", "c", "3"});
    auto result = getEventErrorId(info, "target");
    EXPECT_EQ(result, "found!");
}
