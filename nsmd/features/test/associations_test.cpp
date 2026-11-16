/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION &
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

#include "features/associations.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace nsm::features;

namespace
{

const std::string systemBase = "/xyz/openbmc_project/inventory/system";

} // namespace

TEST(AssociationsTest, Target_EmptyRelative_IsNullopt)
{
    EXPECT_FALSE(associationTarget("").has_value());
    EXPECT_FALSE(associationTarget("", "/custom/base").has_value());
}

TEST(AssociationsTest, Target_RelativeToSystemBaseByDefault)
{
    EXPECT_EQ(associationTarget("board/HGX_GPU_0"),
              systemBase + "/board/HGX_GPU_0");
}

TEST(AssociationsTest, Target_RelativeToGivenBase)
{
    EXPECT_EQ(associationTarget("GPU_0", "/custom/base"), "/custom/base/GPU_0");
}

TEST(AssociationsTest, Target_AbsoluteRelative_ReplacesBase)
{
    // std::filesystem semantics: an absolute right-hand side wins
    EXPECT_EQ(associationTarget("/abs/path", "/custom/base"), "/abs/path");
}

TEST(AssociationsTest, Target_InvalidCharacters_AreReplaced)
{
    EXPECT_EQ(associationTarget("bad name-1", "/b"), "/b/bad_name_1");
}

TEST(AssociationsTest, AddAssociations_OnePairPerNonEmptyEntry)
{
    dbus::PropertyMap params{
        {"Paths", std::vector<std::string>{"a", "", "b/c"}}};
    std::vector<utils::Association> out;

    addAssociations(out, params, "Paths", "fwd", "bwd");

    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].forward, "fwd");
    EXPECT_EQ(out[0].backward, "bwd");
    EXPECT_EQ(out[0].absolutePath, systemBase + "/a");
    EXPECT_EQ(out[1].absolutePath, systemBase + "/b/c");
}

TEST(AssociationsTest, AddAssociations_MissingKey_AddsNothing)
{
    dbus::PropertyMap params;
    std::vector<utils::Association> out;

    addAssociations(out, params, "Paths", "fwd", "bwd");

    EXPECT_TRUE(out.empty());
}

TEST(AssociationsTest, AddAssociations_AppendsToExisting)
{
    dbus::PropertyMap params{{"Paths", std::vector<std::string>{"x"}}};
    std::vector<utils::Association> out{{"f0", "b0", "/p0"}};

    addAssociations(out, params, "Paths", "f1", "b1", "/base");

    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].absolutePath, "/p0");
    EXPECT_EQ(out[1].absolutePath, "/base/x");
}

TEST(AssociationsTest, AddAssociation_Scalar_AddsOnePair)
{
    dbus::PropertyMap params{{"Path", std::string("board/X")}};
    std::vector<utils::Association> out;

    addAssociation(out, params, "Path", "fwd", "bwd");

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].forward, "fwd");
    EXPECT_EQ(out[0].backward, "bwd");
    EXPECT_EQ(out[0].absolutePath, systemBase + "/board/X");
}

TEST(AssociationsTest, AddAssociation_EmptyOrMissing_AddsNothing)
{
    std::vector<utils::Association> out;

    addAssociation(out, {{"Path", std::string()}}, "Path", "f", "b");
    addAssociation(out, {}, "Path", "f", "b");

    EXPECT_TRUE(out.empty());
}
