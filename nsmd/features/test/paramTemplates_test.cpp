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

#include "features/paramTemplates.hpp"

#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using namespace nsm::features;
using namespace std::string_literals;

namespace
{

DeviceContext makeDev()
{
    DeviceContext dev;
    dev.name = "HGX_GPU_0";
    dev.kind = "gpu";
    dev.instance = 2;
    dev.platformPrefix = "HGX_";
    dev.parentChassis = "board/HGX_ProcessorModule_0";
    dev.variables["PortCount"] = uint64_t{4};
    dev.variables["Label"] = "front"s;
    dev.variables["Flag"] = true;
    dev.variables["Pair"] = std::vector<std::string>{"a", "b"};
    return dev;
}

struct Resolved
{
    std::optional<dbus::PropertyMap> params;
    std::string error;
    std::vector<std::string> dropped;
};

Resolved resolve(const DeviceContext& dev, const ParamMap& params)
{
    Resolved r;
    r.params = resolveParams(dev, params, r.error, r.dropped);
    return r;
}

std::string text(const Resolved& r, const char* key)
{
    return std::get<std::string>(r.params->at(key));
}

} // namespace

TEST(ParamTemplatesTest, IsBuiltinVariable_AllBuiltinNames)
{
    for (const char* name :
         {"device", "deviceBase", "instance", "instance+1", "parentChassis",
          "parentChassisName", "platformPrefix"})
    {
        EXPECT_TRUE(isBuiltinVariable(name)) << name;
    }
}

TEST(ParamTemplatesTest, IsBuiltinVariable_OtherNames_False)
{
    EXPECT_FALSE(isBuiltinVariable("PortCount"));
    EXPECT_FALSE(isBuiltinVariable("Name"));
    EXPECT_FALSE(isBuiltinVariable(""));
    EXPECT_FALSE(isBuiltinVariable("{device}"));
}

TEST(ParamTemplatesTest, Text_BuiltinVariables_Replaced)
{
    auto r = resolve(makeDev(), {{"A", ParamValue{"{device}_Temp_{instance}"s}},
                                 {"B", ParamValue{"{instance+1}"s}},
                                 {"C", ParamValue{"{deviceBase}"s}},
                                 {"D", ParamValue{"{platformPrefix}"s}},
                                 {"E", ParamValue{"{parentChassis}"s}},
                                 {"F", ParamValue{"{parentChassisName}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "HGX_GPU_0_Temp_2");
    EXPECT_EQ(text(r, "B"), "3");
    EXPECT_EQ(text(r, "C"), "GPU_0");
    EXPECT_EQ(text(r, "D"), "HGX_");
    EXPECT_EQ(text(r, "E"), "board/HGX_ProcessorModule_0");
    EXPECT_EQ(text(r, "F"), "HGX_ProcessorModule_0");
    EXPECT_TRUE(r.error.empty());
    EXPECT_TRUE(r.dropped.empty());
}

TEST(ParamTemplatesTest, Text_ParentChassisWithoutSlash_NameIsWholeValue)
{
    auto dev = makeDev();
    dev.parentChassis = "Chassis_0";
    auto r = resolve(dev, {{"A", ParamValue{"{parentChassisName}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "Chassis_0");
}

TEST(ParamTemplatesTest, Text_DeviceVariables_ConvertedToText)
{
    auto r = resolve(makeDev(), {{"A", ParamValue{"{Label}/{PortCount}"s}},
                                 {"B", ParamValue{"{Flag}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "front/4");
    EXPECT_EQ(text(r, "B"), "true");
}

TEST(ParamTemplatesTest, Text_FalseBool_ConvertedToText)
{
    auto dev = makeDev();
    dev.variables["Flag"] = false;
    auto r = resolve(dev, {{"A", ParamValue{"{Flag}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "false");
}

TEST(ParamTemplatesTest, Text_UnknownToken_Kept)
{
    auto r = resolve(
        makeDev(), {{"A", ParamValue{"event {0} of {Unknown} on {device}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "event {0} of {Unknown} on HGX_GPU_0");
}

TEST(ParamTemplatesTest, Text_UnterminatedBrace_Kept)
{
    auto r = resolve(makeDev(), {{"A", ParamValue{"{device} {open"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "HGX_GPU_0 {open");
}

TEST(ParamTemplatesTest, Text_NoTokens_Unchanged)
{
    auto r = resolve(makeDev(), {{"A", ParamValue{"plain"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "plain");
}

TEST(ParamTemplatesTest, Text_EmptyString_Unchanged)
{
    auto r = resolve(makeDev(), {{"A", ParamValue{""s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "");
}

TEST(ParamTemplatesTest, Text_ReplacedTextNotScannedAgain)
{
    auto dev = makeDev();
    dev.variables["Label"] = "{device}"s;
    auto r = resolve(dev, {{"A", ParamValue{"{Label}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "{device}");
}

TEST(ParamTemplatesTest, Text_BuiltinShadowsDeviceVariable)
{
    auto dev = makeDev();
    dev.variables["device"] = "other"s;
    auto r = resolve(dev, {{"A", ParamValue{"{device}"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "HGX_GPU_0");
}

TEST(ParamTemplatesTest, Text_ParentChassisAbsent_Fails)
{
    auto dev = makeDev();
    dev.parentChassis.clear();
    auto r = resolve(dev, {{"A", ParamValue{"{parentChassis}"s}}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_NE(r.error.find("{parentChassis} has no value"), std::string::npos)
        << r.error;
}

TEST(ParamTemplatesTest, Text_ParentChassisNameAbsent_Fails)
{
    auto dev = makeDev();
    dev.parentChassis.clear();
    auto r = resolve(dev, {{"A", ParamValue{"x/{parentChassisName}"s}}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_NE(r.error.find("{parentChassisName} has no value"),
              std::string::npos)
        << r.error;
}

TEST(ParamTemplatesTest, Text_ArrayVariable_CannotBeUsedInText)
{
    auto r = resolve(makeDev(), {{"A", ParamValue{"{Pair}"s}}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_NE(r.error.find("cannot be used in text"), std::string::npos)
        << r.error;
}

TEST(ParamTemplatesTest, TextArray_EveryElementResolved)
{
    auto r = resolve(makeDev(),
                     {{"Paths", ParamValue{std::vector<std::string>{
                                    "chassis/{device}",
                                    "board/{parentChassisName}", "fixed"}}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(
        std::get<std::vector<std::string>>(r.params->at("Paths")),
        (std::vector<std::string>{"chassis/HGX_GPU_0",
                                  "board/HGX_ProcessorModule_0", "fixed"}));
}

TEST(ParamTemplatesTest, TextArray_FailingElement_FailsAll)
{
    auto dev = makeDev();
    dev.parentChassis.clear();
    auto r = resolve(dev, {{"Paths", ParamValue{std::vector<std::string>{
                                         "{device}", "{parentChassis}"}}}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_FALSE(r.error.empty());
}

TEST(ParamTemplatesTest, NonTextValues_PassThrough)
{
    auto r = resolve(makeDev(),
                     {{"Count", ParamValue{uint64_t{5}}},
                      {"Flag", ParamValue{true}},
                      {"Max", ParamValue{125.5}},
                      {"Ids", ParamValue{std::vector<uint64_t>{1, 2}}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(std::get<uint64_t>(r.params->at("Count")), 5u);
    EXPECT_EQ(std::get<bool>(r.params->at("Flag")), true);
    EXPECT_DOUBLE_EQ(std::get<double>(r.params->at("Max")), 125.5);
    EXPECT_EQ(std::get<std::vector<uint64_t>>(r.params->at("Ids")),
              (std::vector<uint64_t>{1, 2}));
}

TEST(ParamTemplatesTest, EmptyParams_EmptyResult)
{
    auto r = resolve(makeDev(), {});
    ASSERT_TRUE(r.params.has_value());
    EXPECT_TRUE(r.params->empty());
}

TEST(ParamTemplatesTest, Var_TakesDeviceVariableInDeclaredType)
{
    auto r = resolve(makeDev(), {{"Count", var<uint64_t>("{PortCount}")},
                                 {"Label", var<std::string>("{Label}")}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(std::get<uint64_t>(r.params->at("Count")), 4u);
    EXPECT_EQ(text(r, "Label"), "front");
}

TEST(ParamTemplatesTest, Var_BuiltinConvertedToDeclaredType)
{
    auto r = resolve(makeDev(), {{"Inst", var<uint64_t>("{instance}")},
                                 {"Next", var<uint64_t>("{instance+1}")},
                                 {"Name", var<std::string>("{device}")}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(std::get<uint64_t>(r.params->at("Inst")), 2u);
    EXPECT_EQ(std::get<uint64_t>(r.params->at("Next")), 3u);
    EXPECT_EQ(text(r, "Name"), "HGX_GPU_0");
}

TEST(ParamTemplatesTest, Var_IntegerConvertedToDouble)
{
    auto r = resolve(makeDev(), {{"Max", var<double>("{PortCount}")}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_DOUBLE_EQ(std::get<double>(r.params->at("Max")), 4.0);
}

TEST(ParamTemplatesTest, Var_ArrayVariable_KeepsArrayType)
{
    auto r = resolve(makeDev(),
                     {{"Pair", var<std::vector<std::string>>("{Pair}")}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(std::get<std::vector<std::string>>(r.params->at("Pair")),
              (std::vector<std::string>{"a", "b"}));
}

TEST(ParamTemplatesTest, Var_Missing_Fails)
{
    auto r = resolve(makeDev(), {{"Count", var<uint64_t>("{Nope}")}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_EQ(r.error, "{Nope} has no value, Count not set");
}

TEST(ParamTemplatesTest, Var_ParentChassisAbsent_Fails)
{
    auto dev = makeDev();
    dev.parentChassis.clear();
    auto r = resolve(dev, {{"P", var<std::string>("{parentChassis}")}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_EQ(r.error, "{parentChassis} has no value, P not set");
}

TEST(ParamTemplatesTest, Var_WrongType_Fails)
{
    auto r = resolve(makeDev(), {{"Name", var<std::string>("{PortCount}")}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_NE(r.error.find("does not convert to the type of Name"),
              std::string::npos)
        << r.error;
}

TEST(ParamTemplatesTest, OptVar_Present_Set)
{
    auto r = resolve(makeDev(), {{"Loc", optVar<std::string>("{Label}")}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "Loc"), "front");
    EXPECT_TRUE(r.dropped.empty());
}

TEST(ParamTemplatesTest, OptVar_Missing_LeftOutAndReported)
{
    auto r = resolve(makeDev(), {{"Loc", optVar<std::string>("{Nope}")},
                                 {"Other", ParamValue{"x"s}}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_FALSE(r.params->contains("Loc"));
    EXPECT_TRUE(r.params->contains("Other"));
    ASSERT_EQ(r.dropped.size(), 1u);
    EXPECT_EQ(r.dropped[0], "{Nope} has no value, Loc not set");
}

TEST(ParamTemplatesTest, OptVar_WrongType_LeftOutAndReported)
{
    auto r = resolve(makeDev(), {{"Loc", optVar<std::string>("{PortCount}")}});
    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_FALSE(r.params->contains("Loc"));
    ASSERT_EQ(r.dropped.size(), 1u);
    EXPECT_NE(r.dropped[0].find("does not convert"), std::string::npos)
        << r.dropped[0];
}

TEST(ParamTemplatesTest, OptVar_MissingDoesNotMaskRequiredFailure)
{
    auto r = resolve(makeDev(), {{"Loc", optVar<std::string>("{Nope}")},
                                 {"Req", var<uint64_t>("{AlsoNope}")}});
    EXPECT_FALSE(r.params.has_value());
    EXPECT_NE(r.error.find("Req not set"), std::string::npos) << r.error;
}

TEST(ParamTemplatesTest, Text_EveryIntegerVariableType_ConvertedToText)
{
    auto dev = makeDev();
    dev.variables["U8"] = uint8_t{8};
    dev.variables["I16"] = int16_t{-16};
    dev.variables["U16"] = uint16_t{16};
    dev.variables["I32"] = int32_t{-32};
    dev.variables["U32"] = uint32_t{32};
    dev.variables["I64"] = int64_t{-64};
    dev.variables["U64"] = uint64_t{64};

    auto r = resolve(
        dev, {{"A", ParamValue{"{U8},{I16},{U16},{I32},{U32},{I64},{U64}"s}}});

    ASSERT_TRUE(r.params.has_value()) << r.error;
    EXPECT_EQ(text(r, "A"), "8,-16,16,-32,32,-64,64");
}

TEST(ParamTemplatesTest, Text_NonTextVariableTypes_CannotBeUsedInText)
{
    using AssocList =
        std::vector<std::tuple<std::string, std::string, std::string>>;

    const std::vector<std::pair<std::string, dbus::Value>> variables{
        {"Real", dbus::Value{2.5}},
        {"Bytes", dbus::Value{std::vector<uint8_t>{1, 2}}},
        {"Fd", dbus::Value{std::in_place_type<sdbusplus::message::unix_fd>}},
        {"Unsigneds", dbus::Value{std::vector<uint64_t>{1}}},
        {"Signeds", dbus::Value{std::vector<int64_t>{1}}},
        {"Assocs", dbus::Value{AssocList{}}},
    };

    for (const auto& [name, value] : variables)
    {
        auto dev = makeDev();
        dev.variables[name] = value;

        auto r = resolve(dev, {{"A", ParamValue{"x{" + name + "}"}}});

        EXPECT_FALSE(r.params.has_value()) << name;
        EXPECT_NE(r.error.find("cannot be used in text"), std::string::npos)
            << name << ": " << r.error;
    }
}
