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

#include "features/overrides/overrideDecode.hpp"

#include <gtest/gtest.h>

using namespace nsm::features;
using namespace std::string_literals;

TEST(OverrideDecodeTest, EnabledTrue_BecomesEnable)
{
    std::vector<Override> out;
    decodeFeatureOverrides("Temp", FeatureOverrides{{"Enabled", true}}, "src",
                           out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].action, Override::Action::Enable);
    EXPECT_EQ(out[0].feature, "Temp");
    EXPECT_TRUE(out[0].property.empty());
}

TEST(OverrideDecodeTest, EnabledFalse_BecomesDisable)
{
    std::vector<Override> out;
    decodeFeatureOverrides("Temp", FeatureOverrides{{"Enabled", false}}, "src",
                           out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].action, Override::Action::Disable);
    EXPECT_EQ(out[0].feature, "Temp");
}

TEST(OverrideDecodeTest, OtherKeys_BecomeSet)
{
    std::vector<Override> out;
    decodeFeatureOverrides("Temp",
                           FeatureOverrides{{"MaxValue", 90.0},
                                            {"Name", "x"s},
                                            {"SensorId", uint64_t{5}}},
                           "src", out);
    ASSERT_EQ(out.size(), 3u);
    // std::map order: MaxValue, Name, SensorId
    EXPECT_EQ(out[0].action, Override::Action::Set);
    EXPECT_EQ(out[0].property, "MaxValue");
    EXPECT_DOUBLE_EQ(std::get<double>(out[0].value), 90.0);
    EXPECT_EQ(out[1].property, "Name");
    EXPECT_EQ(std::get<std::string>(out[1].value), "x");
    EXPECT_EQ(out[2].property, "SensorId");
    EXPECT_EQ(std::get<uint64_t>(out[2].value), 5u);
    for (const auto& o : out)
    {
        EXPECT_EQ(o.feature, "Temp");
    }
}

TEST(OverrideDecodeTest, EnabledAndSet_BothDecoded)
{
    std::vector<Override> out;
    decodeFeatureOverrides(
        "Temp", FeatureOverrides{{"Enabled", true}, {"MaxValue", 1.0}}, "src",
        out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].action, Override::Action::Enable);
    EXPECT_EQ(out[1].action, Override::Action::Set);
    EXPECT_EQ(out[1].property, "MaxValue");
}

TEST(OverrideDecodeTest, AppendsToExistingOutput)
{
    std::vector<Override> out;
    decodeFeatureOverrides("A", FeatureOverrides{{"Enabled", true}}, "src",
                           out);
    decodeFeatureOverrides("B", FeatureOverrides{{"Enabled", false}}, "src",
                           out);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].feature, "A");
    EXPECT_EQ(out[1].feature, "B");
}

TEST(OverrideDecodeTest, EmptyValues_NothingDecoded)
{
    std::vector<Override> out;
    decodeFeatureOverrides("Temp", FeatureOverrides{}, "src", out);
    EXPECT_TRUE(out.empty());
}

#ifndef NSM_STRICT_OVERRIDES
// A non-bool Enabled aborts nsmd in strict builds.
TEST(OverrideDecodeTest, NonBoolEnabled_DroppedOthersKept)
{
    std::vector<Override> out;
    decodeFeatureOverrides(
        "Temp", FeatureOverrides{{"Enabled", uint64_t{1}}, {"MaxValue", 2.0}},
        "src", out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].action, Override::Action::Set);
    EXPECT_EQ(out[0].property, "MaxValue");
}
#endif // NSM_STRICT_OVERRIDES

TEST(OverrideDecodeTest, FeaturesMap_DecodesEveryFeature)
{
    FeaturesMap features;
    features["Temp.TEMP_0"] = FeatureOverrides{{"Enabled", false}};
    features["Power"] = FeatureOverrides{{"MaxValue", 5.0}};
    auto out = decodeFeatureOverrides(features, "src");
    ASSERT_EQ(out.size(), 2u);
    // std::map order: Power, Temp.TEMP_0
    EXPECT_EQ(out[0].feature, "Power");
    EXPECT_EQ(out[0].action, Override::Action::Set);
    EXPECT_EQ(out[1].feature, "Temp.TEMP_0");
    EXPECT_EQ(out[1].action, Override::Action::Disable);
}

TEST(OverrideDecodeTest, FeaturesMap_Empty_NoOverrides)
{
    EXPECT_TRUE(decodeFeatureOverrides(FeaturesMap{}, "src").empty());
}

TEST(OverrideDecodeTest, InterfaceNames_AreTheEmContract)
{
    EXPECT_STREQ(telemetryInterface,
                 "xyz.openbmc_project.Configuration.NSM_Telemetry");
    EXPECT_STREQ(telemetryOverridesInterface,
                 "xyz.openbmc_project.Configuration.NSM_Telemetry."
                 "FeatureOverrides");
    EXPECT_STREQ(policyInterface,
                 "xyz.openbmc_project.Configuration.NSM_Policy");
    EXPECT_STREQ(policyEntryPrefix,
                 "xyz.openbmc_project.Configuration.NSM_Policy.Policy");
    EXPECT_STREQ(enabledKey, "Enabled");
    EXPECT_STREQ(templateValuesKey, "TemplateValues");
}
