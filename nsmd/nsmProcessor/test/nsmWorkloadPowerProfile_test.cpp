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

#include "nsmWorkloadPowerProfile.hpp"

#include <gtest/gtest.h>

using namespace nsm;

TEST(NsmWorkLoadProfileEnum, testConstructor)
{
    std::string name = "ProfileEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"Performance", "Balanced",
                                             "PowerSaver"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.getName(), name);
    EXPECT_EQ(profileEnum.getType(), type);
}

TEST(NsmWorkLoadProfileEnum, testToString)
{
    std::string name = "ProfileEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"Profile0", "Profile1",
                                             "Profile2"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toString(0), "Profile0");
    EXPECT_EQ(profileEnum.toString(1), "Profile1");
    EXPECT_EQ(profileEnum.toString(2), "Profile2");
    EXPECT_EQ(profileEnum.toString(99), "Unknown");
}

TEST(NsmWorkLoadProfileEnum, testToEnum)
{
    std::string name = "ProfileEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"High", "Medium", "Low"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toEnum("High"), 0);
    EXPECT_EQ(profileEnum.toEnum("Medium"), 1);
    EXPECT_EQ(profileEnum.toEnum("Low"), 2);
    EXPECT_EQ(profileEnum.toEnum("Invalid"), -1);
}

TEST(NsmWorkLoadProfileEnum, testEmptyList)
{
    std::string name = "EmptyEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames; // Empty

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toString(0), "Unknown");
    EXPECT_EQ(profileEnum.toEnum("Any"), -1);
}

TEST(NsmWorkLoadProfileEnum, testSingleProfile)
{
    std::string name = "SingleEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"OnlyOne"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    EXPECT_EQ(profileEnum.toString(0), "OnlyOne");
    EXPECT_EQ(profileEnum.toEnum("OnlyOne"), 0);
    EXPECT_EQ(profileEnum.toString(1), "Unknown");
}

TEST(NsmWorkLoadProfileEnum, testManyProfiles)
{
    std::string name = "ManyEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames;
    for (int i = 0; i < 10; i++)
    {
        profileNames.push_back("Profile" + std::to_string(i));
    }

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    for (int i = 0; i < 10; i++)
    {
        EXPECT_EQ(profileEnum.toString(i), "Profile" + std::to_string(i));
        EXPECT_EQ(profileEnum.toEnum("Profile" + std::to_string(i)), i);
    }
}

TEST(NsmWorkLoadProfileEnum, testDuplicateStrings)
{
    std::string name = "DuplicateEnum";
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames = {"Same", "Same", "Different"};

    NsmWorkLoadProfileEnum profileEnum(name, type, profileNames);

    // Last duplicate wins
    EXPECT_EQ(profileEnum.toEnum("Same"), 1);
    EXPECT_EQ(profileEnum.toEnum("Different"), 2);
}

TEST(NsmWorkLoadProfileEnum, testMultipleInstances)
{
    std::string type = "NSM_ProfileEnum";
    std::vector<std::string> profileNames1 = {"A", "B", "C"};
    std::vector<std::string> profileNames2 = {"X", "Y", "Z"};

    std::string name1 = "Enum1";
    std::string name2 = "Enum2";

    NsmWorkLoadProfileEnum enum1(name1, type, profileNames1);
    NsmWorkLoadProfileEnum enum2(name2, type, profileNames2);

    EXPECT_EQ(enum1.toString(0), "A");
    EXPECT_EQ(enum2.toString(0), "X");
    EXPECT_EQ(enum1.toEnum("A"), 0);
    EXPECT_EQ(enum2.toEnum("X"), 0);
}
