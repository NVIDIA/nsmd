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

#include "common/event.hpp"
#include "features/overrides/overrideApply.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <functional>
#include <map>
#include <memory>
#include <set>

#include <gtest/gtest.h>

// The cache is filled from D-Bus; the tests fill it directly
#define private public
#include "features/policy/policyStore.hpp"
#undef private

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

} // namespace

class PolicyStoreTest : public ::testing::Test
{
  protected:
    PolicyStoreTest() :
        bus(std::make_shared<sdbusplus::asio::connection>(io)),
        store(*bus, event, [this]() { ++readyCalls; })
    {}

    PolicyStore::Entry entry(bool allDevices, std::set<std::string> devices,
                             std::vector<Override> overrides,
                             std::map<std::string, dbus::Value> values = {})
    {
        PolicyStore::Entry e;
        e.allDevices = allDevices;
        e.devices.insert(devices.begin(), devices.end());
        e.overrides = std::move(overrides);
        e.templateValues = std::move(values);
        return e;
    }

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> bus;
    common::Event event;
    int readyCalls = 0;
    PolicyStore store;
};

TEST_F(PolicyStoreTest, InitialState_NotReady_NoCallback)
{
    EXPECT_FALSE(store.ready());
    EXPECT_EQ(readyCalls, 0);
}

TEST_F(PolicyStoreTest, EmptyCache_NoOverridesNoValues)
{
    EXPECT_TRUE(store.overridesFor("gpu", "HGX_GPU_0").empty());
    EXPECT_TRUE(store.templateValuesFor("gpu", "HGX_GPU_0").empty());
}

TEST_F(PolicyStoreTest, UnknownKind_Empty)
{
    store.byKind["gpu"].push_back(
        entry(true, {}, {makeOverride(Override::Action::Disable, "Temp")}));
    EXPECT_TRUE(store.overridesFor("cpu", "HGX_GPU_0").empty());
    EXPECT_TRUE(store.templateValuesFor("cpu", "HGX_GPU_0").empty());
}

TEST_F(PolicyStoreTest, AllDevicesEntry_AppliesToEveryDeviceOfKind)
{
    store.byKind["gpu"].push_back(
        entry(true, {}, {makeOverride(Override::Action::Disable, "Temp")}));
    for (const char* name : {"HGX_GPU_0", "HGX_GPU_7", "anything"})
    {
        auto out = store.overridesFor("gpu", name);
        ASSERT_EQ(out.size(), 1u) << name;
        EXPECT_EQ(out[0].feature, "Temp");
        EXPECT_EQ(out[0].action, Override::Action::Disable);
    }
}

TEST_F(PolicyStoreTest, ListedDevices_OnlyThoseMatch)
{
    store.byKind["gpu"].push_back(
        entry(false, {"HGX_GPU_0", "HGX_GPU_1"},
              {makeOverride(Override::Action::Enable, "Power")}));
    EXPECT_EQ(store.overridesFor("gpu", "HGX_GPU_0").size(), 1u);
    EXPECT_EQ(store.overridesFor("gpu", "HGX_GPU_1").size(), 1u);
    EXPECT_TRUE(store.overridesFor("gpu", "HGX_GPU_2").empty());
}

TEST_F(PolicyStoreTest, DeviceMatch_IsExactName)
{
    store.byKind["gpu"].push_back(
        entry(false, {"HGX_GPU_1"},
              {makeOverride(Override::Action::Enable, "Power")}));
    EXPECT_TRUE(store.overridesFor("gpu", "HGX_GPU_10").empty());
    EXPECT_TRUE(store.overridesFor("gpu", "GPU_1").empty());
}

TEST_F(PolicyStoreTest, EntryWithNoDevicesAndNotAllDevices_MatchesNothing)
{
    store.byKind["gpu"].push_back(
        entry(false, {}, {makeOverride(Override::Action::Enable, "Power")}));
    EXPECT_TRUE(store.overridesFor("gpu", "HGX_GPU_0").empty());
}

TEST_F(PolicyStoreTest, Entries_AppliedInIndexOrder)
{
    store.byKind["gpu"].push_back(
        entry(true, {}, {makeOverride(Override::Action::Disable, "Temp")}));
    store.byKind["gpu"].push_back(
        entry(false, {"HGX_GPU_0"},
              {makeOverride(Override::Action::Enable, "Temp"),
               makeOverride(Override::Action::Set, "Temp", "MaxValue",
                            dbus::Value{80.0})}));

    auto out = store.overridesFor("gpu", "HGX_GPU_0");
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0].action, Override::Action::Disable);
    EXPECT_EQ(out[1].action, Override::Action::Enable);
    EXPECT_EQ(out[2].action, Override::Action::Set);
    EXPECT_EQ(out[2].property, "MaxValue");

    // A device the second entry does not list gets only the first
    auto other = store.overridesFor("gpu", "HGX_GPU_1");
    ASSERT_EQ(other.size(), 1u);
    EXPECT_EQ(other[0].action, Override::Action::Disable);
}

TEST_F(PolicyStoreTest, KindsAreIndependent)
{
    store.byKind["gpu"].push_back(
        entry(true, {}, {makeOverride(Override::Action::Disable, "A")}));
    store.byKind["cpu"].push_back(
        entry(true, {}, {makeOverride(Override::Action::Enable, "B")}));
    auto gpu = store.overridesFor("gpu", "d");
    auto cpu = store.overridesFor("cpu", "d");
    ASSERT_EQ(gpu.size(), 1u);
    ASSERT_EQ(cpu.size(), 1u);
    EXPECT_EQ(gpu[0].feature, "A");
    EXPECT_EQ(cpu[0].feature, "B");
}

TEST_F(PolicyStoreTest, TemplateValues_LaterEntryWins)
{
    store.byKind["gpu"].push_back(entry(
        true, {}, {},
        {{"Port", dbus::Value{uint64_t{1}}}, {"Label", dbus::Value{"a"s}}}));
    store.byKind["gpu"].push_back(
        entry(false, {"HGX_GPU_0"}, {}, {{"Port", dbus::Value{uint64_t{2}}}}));

    auto v0 = store.templateValuesFor("gpu", "HGX_GPU_0");
    ASSERT_EQ(v0.size(), 2u);
    EXPECT_EQ(std::get<uint64_t>(v0.at("Port")), 2u);
    EXPECT_EQ(std::get<std::string>(v0.at("Label")), "a");

    auto v1 = store.templateValuesFor("gpu", "HGX_GPU_1");
    ASSERT_EQ(v1.size(), 2u);
    EXPECT_EQ(std::get<uint64_t>(v1.at("Port")), 1u);
}

TEST_F(PolicyStoreTest, TemplateValues_ListedDeviceOnly)
{
    store.byKind["gpu"].push_back(
        entry(false, {"HGX_GPU_0"}, {}, {{"Port", dbus::Value{uint64_t{3}}}}));
    EXPECT_EQ(store.templateValuesFor("gpu", "HGX_GPU_0").size(), 1u);
    EXPECT_TRUE(store.templateValuesFor("gpu", "HGX_GPU_1").empty());
}

TEST_F(PolicyStoreTest, OverridesAndTemplateValues_DoNotMix)
{
    store.byKind["gpu"].push_back(
        entry(true, {}, {makeOverride(Override::Action::Disable, "A")}));
    EXPECT_TRUE(store.templateValuesFor("gpu", "d").empty());
    store.byKind["cpu"].push_back(
        entry(true, {}, {}, {{"X", dbus::Value{uint64_t{1}}}}));
    EXPECT_TRUE(store.overridesFor("cpu", "d").empty());
}
