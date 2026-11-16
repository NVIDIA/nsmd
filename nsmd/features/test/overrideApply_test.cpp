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

#include "features/overrides/overrideApply.hpp"

#include <gtest/gtest.h>

using namespace nsm::features;
using namespace std::string_literals;

namespace
{

Override makeOverride(Override::Action action, const std::string& feature,
                      const std::string& property = {}, dbus::Value value = {})
{
    Override o;
    o.action = action;
    o.feature = feature;
    o.property = property;
    o.value = std::move(value);
    return o;
}

Feature makeFeature(std::string_view id, bool enabled = true)
{
    Feature f;
    f.id = id;
    f.enabled = enabled;
    f.params["MaxValue"] = ParamValue{125.0};
    f.params["Name"] = ParamValue{"{device}"s};
    f.params["Count"] = var<uint64_t>("{PortCount}");
    f.optional["Extra"] = uint64_t{0};
    return f;
}

FeatureList makeList()
{
    FeatureList list;
    list.push_back(makeFeature("A"));
    list.push_back(makeFeature("B", false));
    return list;
}

} // namespace

// ---- normalize ------------------------------------------------------------

TEST(OverrideNormalizeTest, SameType_Returned)
{
    auto out = normalize(dbus::Value{1.5}, dbus::Value{2.5});
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*out), 2.5);
}

TEST(OverrideNormalizeTest, SameType_String)
{
    auto out = normalize(dbus::Value{"a"s}, dbus::Value{"b"s});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(std::get<std::string>(*out), "b");
}

TEST(OverrideNormalizeTest, IntegerToDouble)
{
    auto out = normalize(dbus::Value{1.5}, dbus::Value{uint64_t{3}});
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*out), 3.0);
    out = normalize(dbus::Value{1.5}, dbus::Value{int64_t{-3}});
    ASSERT_TRUE(out.has_value());
    EXPECT_DOUBLE_EQ(std::get<double>(*out), -3.0);
}

TEST(OverrideNormalizeTest, BoolToDouble_Rejected)
{
    EXPECT_FALSE(normalize(dbus::Value{1.5}, dbus::Value{true}).has_value());
}

TEST(OverrideNormalizeTest, DoubleToInteger_Rejected)
{
    EXPECT_FALSE(
        normalize(dbus::Value{uint64_t{1}}, dbus::Value{2.0}).has_value());
}

TEST(OverrideNormalizeTest, IntegerWidening_InRange)
{
    auto out = normalize(dbus::Value{uint64_t{0}}, dbus::Value{int64_t{7}});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(std::get<uint64_t>(*out), 7u);
}

TEST(OverrideNormalizeTest, NegativeToUnsigned_Rejected)
{
    EXPECT_FALSE(normalize(dbus::Value{uint64_t{0}}, dbus::Value{int64_t{-1}})
                     .has_value());
}

TEST(OverrideNormalizeTest, Narrowing_FitsAndOverflows)
{
    auto fits = normalize(dbus::Value{uint8_t{0}}, dbus::Value{uint64_t{255}});
    ASSERT_TRUE(fits.has_value());
    EXPECT_EQ(std::get<uint8_t>(*fits), 255);
    EXPECT_FALSE(normalize(dbus::Value{uint8_t{0}}, dbus::Value{uint64_t{256}})
                     .has_value());
}

TEST(OverrideNormalizeTest, BoolTarget_IntegerRejected)
{
    EXPECT_FALSE(
        normalize(dbus::Value{false}, dbus::Value{uint64_t{1}}).has_value());
}

TEST(OverrideNormalizeTest, IntegerTarget_BoolRejected)
{
    EXPECT_FALSE(
        normalize(dbus::Value{uint64_t{0}}, dbus::Value{true}).has_value());
}

TEST(OverrideNormalizeTest, StringVsNumber_Rejected)
{
    EXPECT_FALSE(
        normalize(dbus::Value{"a"s}, dbus::Value{uint64_t{1}}).has_value());
    EXPECT_FALSE(
        normalize(dbus::Value{uint64_t{1}}, dbus::Value{"a"s}).has_value());
}

TEST(OverrideNormalizeTest, IntegerVector_Converted)
{
    auto out = normalize(dbus::Value{std::vector<uint64_t>{}},
                         dbus::Value{std::vector<int64_t>{1, 2, 3}});
    ASSERT_TRUE(out.has_value());
    EXPECT_EQ(std::get<std::vector<uint64_t>>(*out),
              (std::vector<uint64_t>{1, 2, 3}));
}

TEST(OverrideNormalizeTest, IntegerVector_NegativeElement_Rejected)
{
    EXPECT_FALSE(normalize(dbus::Value{std::vector<uint64_t>{}},
                           dbus::Value{std::vector<int64_t>{1, -2}})
                     .has_value());
}

TEST(OverrideNormalizeTest, IntegerVector_ElementOverflow_Rejected)
{
    EXPECT_FALSE(normalize(dbus::Value{std::vector<uint8_t>{}},
                           dbus::Value{std::vector<uint64_t>{1, 256}})
                     .has_value());
    auto fits = normalize(dbus::Value{std::vector<uint8_t>{}},
                          dbus::Value{std::vector<uint64_t>{1, 255}});
    ASSERT_TRUE(fits.has_value());
    EXPECT_EQ(std::get<std::vector<uint8_t>>(*fits),
              (std::vector<uint8_t>{1, 255}));
}

TEST(OverrideNormalizeTest, EmptyIntegerVector_Converted)
{
    auto out = normalize(dbus::Value{std::vector<uint64_t>{9}},
                         dbus::Value{std::vector<int64_t>{}});
    ASSERT_TRUE(out.has_value());
    EXPECT_TRUE(std::get<std::vector<uint64_t>>(*out).empty());
}

TEST(OverrideNormalizeTest, StringVectorVsIntegerVector_Rejected)
{
    EXPECT_FALSE(normalize(dbus::Value{std::vector<std::string>{}},
                           dbus::Value{std::vector<uint64_t>{1}})
                     .has_value());
}

// ---- applyOverride --------------------------------------------------------

TEST(ApplyOverrideTest, DeclaredParam_Replaced)
{
    auto f = makeFeature("A");
    std::string error;
    EXPECT_TRUE(applyOverride(f, "MaxValue", dbus::Value{90.0}, error));
    EXPECT_DOUBLE_EQ(std::get<double>(f.params.at("MaxValue").value), 90.0);
    EXPECT_TRUE(error.empty());
}

TEST(ApplyOverrideTest, IntegerOverrideOfDoubleParam_Converted)
{
    auto f = makeFeature("A");
    std::string error;
    EXPECT_TRUE(
        applyOverride(f, "MaxValue", dbus::Value{uint64_t{100}}, error));
    EXPECT_DOUBLE_EQ(std::get<double>(f.params.at("MaxValue").value), 100.0);
}

TEST(ApplyOverrideTest, TypeMismatch_Rejected_ParamUnchanged)
{
    auto f = makeFeature("A");
    std::string error;
    EXPECT_FALSE(applyOverride(f, "MaxValue", dbus::Value{"x"s}, error));
    EXPECT_EQ(error, "type mismatch");
    EXPECT_DOUBLE_EQ(std::get<double>(f.params.at("MaxValue").value), 125.0);
}

TEST(ApplyOverrideTest, UnknownProperty_Rejected_NothingAdded)
{
    auto f = makeFeature("A");
    const auto before = f.params.size();
    std::string error;
    EXPECT_FALSE(applyOverride(f, "Nope", dbus::Value{uint64_t{1}}, error));
    EXPECT_EQ(error, "unknown property");
    EXPECT_EQ(f.params.size(), before);
    EXPECT_FALSE(f.params.contains("Nope"));
}

TEST(ApplyOverrideTest, OptionalProperty_AddedToParams)
{
    auto f = makeFeature("A");
    ASSERT_FALSE(f.params.contains("Extra"));
    std::string error;
    EXPECT_TRUE(applyOverride(f, "Extra", dbus::Value{uint64_t{7}}, error));
    ASSERT_TRUE(f.params.contains("Extra"));
    EXPECT_EQ(std::get<uint64_t>(f.params.at("Extra").value), 7u);
    // The declaration only gives the type
    EXPECT_EQ(std::get<uint64_t>(f.optional.at("Extra")), 0u);
}

TEST(ApplyOverrideTest, OptionalProperty_TypeChecked)
{
    auto f = makeFeature("A");
    std::string error;
    EXPECT_FALSE(applyOverride(f, "Extra", dbus::Value{"x"s}, error));
    EXPECT_EQ(error, "type mismatch");
    EXPECT_FALSE(f.params.contains("Extra"));
}

TEST(ApplyOverrideTest, VarParam_ReplacedByPlainValue)
{
    auto f = makeFeature("A");
    ASSERT_EQ(f.params.at("Count").var, "{PortCount}");
    std::string error;
    EXPECT_TRUE(applyOverride(f, "Count", dbus::Value{uint64_t{3}}, error));
    EXPECT_TRUE(f.params.at("Count").var.empty());
    EXPECT_EQ(std::get<uint64_t>(f.params.at("Count").value), 3u);
}

TEST(ApplyOverrideTest, TemplateStringParam_ReplacedByLiteral)
{
    auto f = makeFeature("A");
    std::string error;
    EXPECT_TRUE(applyOverride(f, "Name", dbus::Value{"fixed"s}, error));
    EXPECT_EQ(std::get<std::string>(f.params.at("Name").value), "fixed");
}

// ---- applyOverrides -------------------------------------------------------

TEST(ApplyOverridesTest, EnableAndDisable_FlipFlags)
{
    auto list = makeList();
    auto stats = applyOverrides(list,
                                {makeOverride(Override::Action::Disable, "A"),
                                 makeOverride(Override::Action::Enable, "B")},
                                "dev");
    EXPECT_EQ(stats.applied, 2u);
    EXPECT_EQ(stats.rejected, 0u);
    EXPECT_FALSE(list[0].enabled);
    EXPECT_TRUE(list[1].enabled);
}

TEST(ApplyOverridesTest, NoEffectOverride_NotCounted)
{
    auto list = makeList();
    auto stats = applyOverrides(list,
                                {makeOverride(Override::Action::Enable, "A"),
                                 makeOverride(Override::Action::Disable, "B")},
                                "dev");
    EXPECT_EQ(stats.applied, 0u);
    EXPECT_EQ(stats.rejected, 0u);
    EXPECT_TRUE(list[0].enabled);
    EXPECT_FALSE(list[1].enabled);
}

TEST(ApplyOverridesTest, SetOverride_AppliedToParam)
{
    auto list = makeList();
    auto stats = applyOverrides(list,
                                {makeOverride(Override::Action::Set, "A",
                                              "MaxValue", dbus::Value{80.0})},
                                "dev");
    EXPECT_EQ(stats.applied, 1u);
    EXPECT_EQ(stats.rejected, 0u);
    EXPECT_DOUBLE_EQ(std::get<double>(list[0].params.at("MaxValue").value),
                     80.0);
    EXPECT_DOUBLE_EQ(std::get<double>(list[1].params.at("MaxValue").value),
                     125.0);
}

TEST(ApplyOverridesTest, SetOnDisabledFeature_StillApplied)
{
    auto list = makeList();
    auto stats = applyOverrides(list,
                                {makeOverride(Override::Action::Set, "B",
                                              "MaxValue", dbus::Value{70.0})},
                                "dev");
    EXPECT_EQ(stats.applied, 1u);
    EXPECT_DOUBLE_EQ(std::get<double>(list[1].params.at("MaxValue").value),
                     70.0);
    EXPECT_FALSE(list[1].enabled);
}

TEST(ApplyOverridesTest, EnableDisableAppliedBeforeSet_RecordOrderKept)
{
    auto list = makeList();
    // Set is listed first; the enable still takes effect, and the two Sets
    // of one property apply in record order (the later one wins)
    auto stats = applyOverrides(
        list,
        {makeOverride(Override::Action::Set, "B", "MaxValue", dbus::Value{1.0}),
         makeOverride(Override::Action::Enable, "B"),
         makeOverride(Override::Action::Set, "B", "MaxValue",
                      dbus::Value{2.0})},
        "dev");
    EXPECT_EQ(stats.applied, 3u);
    EXPECT_TRUE(list[1].enabled);
    EXPECT_DOUBLE_EQ(std::get<double>(list[1].params.at("MaxValue").value),
                     2.0);
}

TEST(ApplyOverridesTest, EmptyOverrides_NothingChanges)
{
    auto list = makeList();
    auto stats = applyOverrides(list, {}, "dev");
    EXPECT_EQ(stats.applied, 0u);
    EXPECT_EQ(stats.rejected, 0u);
    EXPECT_TRUE(list[0].enabled);
    EXPECT_FALSE(list[1].enabled);
}

#ifndef NSM_STRICT_OVERRIDES
// A rejected override aborts nsmd in strict builds, so these run only in
// the default build.

TEST(ApplyOverridesTest, UnknownFeature_RejectedForEveryAction)
{
    auto list = makeList();
    auto stats =
        applyOverrides(list,
                       {makeOverride(Override::Action::Enable, "Nope"),
                        makeOverride(Override::Action::Disable, "Nope"),
                        makeOverride(Override::Action::Set, "Nope", "MaxValue",
                                     dbus::Value{1.0})},
                       "dev");
    EXPECT_EQ(stats.applied, 0u);
    EXPECT_EQ(stats.rejected, 3u);
}

TEST(ApplyOverridesTest, UnknownPropertyAndTypeMismatch_Rejected)
{
    auto list = makeList();
    auto stats = applyOverrides(
        list,
        {makeOverride(Override::Action::Set, "A", "Nope", dbus::Value{1.0}),
         makeOverride(Override::Action::Set, "A", "MaxValue",
                      dbus::Value{"x"s})},
        "dev");
    EXPECT_EQ(stats.applied, 0u);
    EXPECT_EQ(stats.rejected, 2u);
    EXPECT_DOUBLE_EQ(std::get<double>(list[0].params.at("MaxValue").value),
                     125.0);
}

TEST(ApplyOverridesTest, RejectedOverride_DoesNotStopOthers)
{
    auto list = makeList();
    auto stats = applyOverrides(
        list,
        {makeOverride(Override::Action::Disable, "Nope"),
         makeOverride(Override::Action::Disable, "A"),
         makeOverride(Override::Action::Set, "A", "Nope", dbus::Value{1.0}),
         makeOverride(Override::Action::Set, "A", "MaxValue",
                      dbus::Value{60.0})},
        "dev");
    EXPECT_EQ(stats.applied, 2u);
    EXPECT_EQ(stats.rejected, 2u);
    EXPECT_FALSE(list[0].enabled);
    EXPECT_DOUBLE_EQ(std::get<double>(list[0].params.at("MaxValue").value),
                     60.0);
}

#endif // NSM_STRICT_OVERRIDES

// ---- valueToString --------------------------------------------------------

TEST(ValueToStringTest, Scalars)
{
    EXPECT_EQ(valueToString(dbus::Value{true}), "true");
    EXPECT_EQ(valueToString(dbus::Value{false}), "false");
    EXPECT_EQ(valueToString(dbus::Value{uint64_t{42}}), "42");
    EXPECT_EQ(valueToString(dbus::Value{int64_t{-7}}), "-7");
    EXPECT_EQ(valueToString(dbus::Value{uint8_t{7}}), "7");
    EXPECT_EQ(valueToString(dbus::Value{1.5}), "1.5");
    EXPECT_EQ(valueToString(dbus::Value{"abc"s}), "abc");
}

TEST(ValueToStringTest, Vectors)
{
    EXPECT_EQ(valueToString(dbus::Value{std::vector<std::string>{"a", "b"}}),
              "[a,b]");
    EXPECT_EQ(valueToString(dbus::Value{std::vector<uint64_t>{1, 2}}), "[1,2]");
    EXPECT_EQ(valueToString(dbus::Value{std::vector<int64_t>{-1}}), "[-1]");
    EXPECT_EQ(valueToString(dbus::Value{std::vector<std::string>{}}), "[]");
}

TEST(ValueToStringTest, UnsupportedType_Unprintable)
{
    using Triple = std::tuple<std::string, std::string, std::string>;
    EXPECT_EQ(valueToString(dbus::Value{std::vector<Triple>{}}),
              "<unprintable>");
}
