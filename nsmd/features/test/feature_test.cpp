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

#include "features/feature.hpp"

#include <gtest/gtest.h>

using namespace nsm::features;
using namespace std::string_literals;

TEST(FeatureHelpersTest, DeviceBaseName_StripsPrefix)
{
    DeviceContext dev;
    dev.name = "HGX_GPU_0";
    dev.platformPrefix = "HGX_";
    EXPECT_EQ(deviceBaseName(dev), "GPU_0");
}

TEST(FeatureHelpersTest, DeviceBaseName_NoPrefixConfigured_Unchanged)
{
    DeviceContext dev;
    dev.name = "HGX_GPU_0";
    EXPECT_EQ(deviceBaseName(dev), "HGX_GPU_0");
}

TEST(FeatureHelpersTest, DeviceBaseName_NameWithoutPrefix_Unchanged)
{
    DeviceContext dev;
    dev.name = "GPU_0";
    dev.platformPrefix = "HGX_";
    EXPECT_EQ(deviceBaseName(dev), "GPU_0");
}

TEST(FeatureHelpersTest, DeviceBaseName_PrefixOnlyInMiddle_Unchanged)
{
    DeviceContext dev;
    dev.name = "X_HGX_GPU_0";
    dev.platformPrefix = "HGX_";
    EXPECT_EQ(deviceBaseName(dev), "X_HGX_GPU_0");
}

TEST(FeatureHelpersTest, NextIndex_CountsPerGroupFromZero)
{
    DeviceContext dev;
    EXPECT_EQ(dev.nextIndex("Assembly"), 0u);
    EXPECT_EQ(dev.nextIndex("Assembly"), 1u);
    EXPECT_EQ(dev.nextIndex("Assembly"), 2u);
    EXPECT_EQ(dev.nextIndex("Other"), 0u);
    EXPECT_EQ(dev.nextIndex("Assembly"), 3u);
}

TEST(FeatureHelpersTest, NextIndex_WorksOnConstContext)
{
    const DeviceContext dev;
    EXPECT_EQ(dev.nextIndex("G"), 0u);
    EXPECT_EQ(dev.nextIndex("G"), 1u);
}

TEST(FeatureHelpersTest, LiteralHelpers_ProduceFixedWidthTypes)
{
    dbus::Value a = u64(5);
    dbus::Value b = i64(-5);
    EXPECT_EQ(std::get<uint64_t>(a), 5u);
    EXPECT_EQ(std::get<int64_t>(b), -5);
}

TEST(FeatureHelpersTest, Param_Present_ReturnsValue)
{
    dbus::PropertyMap params{{"Count", u64(3)}, {"Name", "x"s}};
    EXPECT_EQ(param<uint64_t>(params, "Count"), std::optional<uint64_t>(3));
    EXPECT_EQ(param<std::string>(params, "Name"),
              std::optional<std::string>("x"));
}

TEST(FeatureHelpersTest, Param_Missing_ReturnsNullopt)
{
    dbus::PropertyMap params;
    EXPECT_FALSE(param<uint64_t>(params, "Count").has_value());
}

TEST(FeatureHelpersTest, Param_WrongType_Throws)
{
    dbus::PropertyMap params{{"Count", u64(3)}};
    EXPECT_THROW(param<std::string>(params, "Count"), std::bad_variant_access);
}

TEST(FeatureHelpersTest, ArrayParam_Present_ReturnsElements)
{
    dbus::PropertyMap params{
        {"Ids", std::vector<uint64_t>{1, 2}},
        {"Names", std::vector<std::string>{"a", "b", "c"}}};
    EXPECT_EQ(arrayParam<uint64_t>(params, "Ids"),
              (std::vector<uint64_t>{1, 2}));
    EXPECT_EQ(arrayParam<std::string>(params, "Names"),
              (std::vector<std::string>{"a", "b", "c"}));
}

TEST(FeatureHelpersTest, ArrayParam_Missing_ReturnsEmpty)
{
    dbus::PropertyMap params;
    EXPECT_TRUE(arrayParam<uint64_t>(params, "Ids").empty());
}

TEST(FeatureHelpersTest, ArrayParam_WrongType_Throws)
{
    dbus::PropertyMap params{{"Ids", std::vector<uint64_t>{1}}};
    EXPECT_THROW(arrayParam<std::string>(params, "Ids"),
                 std::bad_variant_access);
}

TEST(FeatureHelpersTest, Var_IsRequiredBinding)
{
    auto p = var<uint64_t>("{PortCount}");
    EXPECT_EQ(p.var, "{PortCount}");
    EXPECT_FALSE(p.optional);
    EXPECT_TRUE(std::holds_alternative<uint64_t>(p.value));
}

TEST(FeatureHelpersTest, OptVar_IsOptionalBinding)
{
    auto p = optVar<std::string>("{LocationCode}");
    EXPECT_EQ(p.var, "{LocationCode}");
    EXPECT_TRUE(p.optional);
    EXPECT_TRUE(std::holds_alternative<std::string>(p.value));
}

TEST(FeatureHelpersTest, ParamValue_PlainValue_HasNoBinding)
{
    ParamValue p{"text"s};
    EXPECT_TRUE(p.var.empty());
    EXPECT_FALSE(p.optional);
    EXPECT_EQ(std::get<std::string>(p.value), "text");
}

TEST(FeatureHelpersTest, Feature_Defaults)
{
    Feature f;
    EXPECT_TRUE(f.enabled);
    EXPECT_EQ(f.create, nullptr);
    EXPECT_TRUE(f.params.empty());
    EXPECT_TRUE(f.optional.empty());
}
