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
 * Branch coverage batch 3 for nsmd/nsmChassis/nsmErot.cpp (extractNumber)
 *
 * Covers:
 * - extractNumber: string ending in digits -> returns the number
 * - extractNumber: string with no digits -> returns -1
 * - extractNumber: empty string -> returns -1
 * - extractNumber: all digits -> returns the number
 * - extractNumber: single digit at end -> returns that digit
 * - extractNumber: mixed text and trailing digits
 */

#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"
using namespace ::testing;

#define private public
#define protected public

#include "libnsm/firmware-utils.h"

#include "nsmChassis/nsmErot.hpp"

using namespace nsm;

namespace nsm
{
int extractNumber(const std::string& str);
} // namespace nsm

// ============================================================================
// extractNumber tests
// ============================================================================

struct ExtractNumberTest : public Test
{};

TEST_F(ExtractNumberTest, StringEndingInDigits)
{
    EXPECT_EQ(extractNumber("Slot42"), 42);
}

TEST_F(ExtractNumberTest, StringEndingInSingleDigit)
{
    EXPECT_EQ(extractNumber("Slot1"), 1);
}

TEST_F(ExtractNumberTest, AllDigits)
{
    EXPECT_EQ(extractNumber("12345"), 12345);
}

TEST_F(ExtractNumberTest, NoDigits)
{
    EXPECT_EQ(extractNumber("NoDigitsHere"), -1);
}

TEST_F(ExtractNumberTest, EmptyString)
{
    EXPECT_EQ(extractNumber(""), -1);
}

TEST_F(ExtractNumberTest, DigitsInMiddleOnly)
{
    // "abc123def" -> trailing "def" has no digits, so extractNumber sees no
    // trailing digits -> returns -1
    EXPECT_EQ(extractNumber("abc123def"), -1);
}

TEST_F(ExtractNumberTest, TrailingZero)
{
    EXPECT_EQ(extractNumber("Slot0"), 0);
}

TEST_F(ExtractNumberTest, MultipleTrailingDigits)
{
    EXPECT_EQ(extractNumber("Device007"), 7);
}

TEST_F(ExtractNumberTest, LargeNumber)
{
    EXPECT_EQ(extractNumber("Slot999999"), 999999);
}
