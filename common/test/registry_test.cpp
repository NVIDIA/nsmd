// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES
/*
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

#include "registry.hpp"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace
{

struct Widget
{
    std::string name;
    int value = 0;

    std::string id() const
    {
        return name;
    }
};

struct Anonymous
{
    int value = 0;
};

struct UnorderedId
{
    struct Key
    {};

    Key id() const
    {
        return {};
    }
};

struct ReferenceId
{
    std::string name;

    const std::string& id() const
    {
        return name;
    }
};

static_assert(common::RegistryRecord<Widget>);
static_assert(common::RegistryRecord<ReferenceId>);
static_assert(!common::RegistryRecord<Anonymous>);
static_assert(!common::RegistryRecord<UnorderedId>);

using WidgetRegistry = common::Registry<Widget>;
using common::RegistryEvent;

struct Heard
{
    std::string id;
    RegistryEvent event;

    bool operator==(const Heard&) const = default;
};

class RegistryTest : public ::testing::Test
{
  protected:
    WidgetRegistry::Interest watch(std::set<std::string> ids)
    {
        return registry.watch(
            std::move(ids), [this](const std::string& id, RegistryEvent event) {
            heard.push_back({id, event});
        });
    }

    WidgetRegistry registry;
    std::vector<Heard> heard;
};

TEST_F(RegistryTest, FindReturnsTheRecordForAnId)
{
    EXPECT_EQ(registry.find("a"), nullptr);

    EXPECT_TRUE(registry.add({"a", 1}));

    auto found = registry.find("a");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, 1);
    EXPECT_EQ(registry.find("b"), nullptr);
}

TEST_F(RegistryTest, FirstRecordForAnIdIsKept)
{
    auto interest = watch({"a"});
    const Widget first{"a", 1};
    EXPECT_TRUE(registry.add(first));

    EXPECT_FALSE(registry.add({"a", 2}));
    EXPECT_FALSE(registry.add(first));

    EXPECT_EQ(registry.find("a")->value, 1);
    EXPECT_EQ(heard, (std::vector<Heard>{{"a", RegistryEvent::Added}}));
}

TEST_F(RegistryTest, OnlyWatchedIdsAreHeard)
{
    auto interest = watch({"a", "c"});

    registry.add({"a", 1});
    registry.add({"b", 1});
    registry.add({"c", 1});
    registry.changed("a");
    registry.changed("b");

    EXPECT_EQ(heard, (std::vector<Heard>{{"a", RegistryEvent::Added},
                                         {"c", RegistryEvent::Added},
                                         {"a", RegistryEvent::Changed}}));
}

TEST_F(RegistryTest, ChangeOfAnUnrecordedIdIsIgnored)
{
    auto interest = watch({"a"});

    registry.changed("a");

    EXPECT_TRUE(heard.empty());
}

TEST_F(RegistryTest, RecordsAddedBeforeTheWatchAreNotReplayed)
{
    registry.add({"a", 1});

    auto interest = watch({"a"});
    EXPECT_TRUE(heard.empty());

    registry.changed("a");
    EXPECT_EQ(heard, (std::vector<Heard>{{"a", RegistryEvent::Changed}}));
}

TEST_F(RegistryTest, EveryInterestInAnIdIsHeard)
{
    auto first = watch({"a"});
    auto second = watch({"a"});

    registry.add({"a", 1});

    EXPECT_EQ(heard.size(), 2);
}

TEST_F(RegistryTest, DestroyedInterestIsNotHeard)
{
    std::optional<WidgetRegistry::Interest> interest = watch({"a"});
    registry.add({"a", 1});
    ASSERT_EQ(heard.size(), 1);

    interest.reset();
    registry.changed("a");

    EXPECT_EQ(heard.size(), 1);
}

TEST_F(RegistryTest, MovedInterestKeepsWatching)
{
    WidgetRegistry::Interest kept;
    {
        auto interest = watch({"a"});
        kept = std::move(interest);
    }

    registry.add({"a", 1});

    EXPECT_EQ(heard.size(), 1);
}

TEST_F(RegistryTest, InterestDestroyedByAnEarlierListenerIsSkipped)
{
    std::optional<WidgetRegistry::Interest> later;
    auto earlier = registry.watch({"a"},
                                  [&](const std::string&, RegistryEvent) {
        heard.push_back({"earlier", RegistryEvent::Added});
        later.reset();
    });
    later = registry.watch({"a"}, [&](const std::string&, RegistryEvent) {
        heard.push_back({"later", RegistryEvent::Added});
    });

    registry.add({"a", 1});

    EXPECT_EQ(heard, (std::vector<Heard>{{"earlier", RegistryEvent::Added}}));
}

TEST_F(RegistryTest, ListenerMayAddARecord)
{
    auto interest = registry.watch(
        {"a", "b"}, [&](const std::string& id, RegistryEvent event) {
        heard.push_back({id, event});
        if (id == "a")
        {
            registry.add({"b", 1});
        }
    });

    registry.add({"a", 1});

    EXPECT_EQ(heard, (std::vector<Heard>{{"a", RegistryEvent::Added},
                                         {"b", RegistryEvent::Added}}));
    EXPECT_NE(registry.find("b"), nullptr);
}

} // namespace
