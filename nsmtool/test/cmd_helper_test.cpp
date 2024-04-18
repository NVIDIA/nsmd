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

#include "base.h"

#include "cmd_helper.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::ElementsAre;
using ordered_json = nlohmann::ordered_json;

TEST(parseBitfieldVar, GoodTest)
{
    bitfield8_t supportedTypes[8] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

    ordered_json result;
    std::string key("Supported Nvidia Message Types");
    nsmtool::helper::parseBitfieldVar(result, key, &supportedTypes[0], 8);
    EXPECT_EQ(result[key].size(), 0);

    supportedTypes[0].byte = 0x1f;
    nsmtool::helper::parseBitfieldVar(result, key, &supportedTypes[0], 8);
    EXPECT_EQ(result[key].size(), 5);
    EXPECT_THAT(result[key], ElementsAre(0, 1, 2, 3, 4));

    supportedTypes[0].byte = 0;
    supportedTypes[7].byte = 0xf8;
    result.clear();
    nsmtool::helper::parseBitfieldVar(result, key, &supportedTypes[0], 8);
    EXPECT_EQ(result[key].size(), 5);
    EXPECT_THAT(result[key], ElementsAre(59, 60, 61, 62, 63));
}
