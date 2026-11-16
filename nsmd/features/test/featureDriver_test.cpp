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
#include "features/featureDriver.hpp"
#include "test/mockDBusHandler.hpp"
#include "test/mockSensorManager.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>

using namespace nsm;
using namespace nsm::features;
using namespace std::string_literals;

namespace
{

// The creators are plain function pointers, so they record into globals
struct Recorder
{
    std::vector<std::string> calls;
    size_t lastParamCount = 0;
    std::vector<unsigned> indexes;
} rec;

std::string tagOf(const dbus::PropertyMap& params)
{
    auto it = params.find("Tag");
    if (it == params.end())
    {
        return "<none>";
    }
    return std::get<std::string>(it->second);
}

requester::Coroutine okCreator(SensorManager&, const DeviceContext&,
                               const dbus::PropertyMap& params)
{
    rec.calls.push_back(tagOf(params));
    rec.lastParamCount = params.size();
    co_return NSM_SW_SUCCESS;
}

requester::Coroutine failCreator(SensorManager&, const DeviceContext&,
                                 const dbus::PropertyMap& params)
{
    rec.calls.push_back(tagOf(params));
    co_return NSM_SW_ERROR;
}

#ifndef COVERAGE_DISABLE_COROUTINES
requester::Coroutine throwCreator(SensorManager&, const DeviceContext&,
                                  const dbus::PropertyMap& params)
{
    rec.calls.push_back(tagOf(params));
    throw std::runtime_error("creator failed");
    co_return NSM_SW_SUCCESS;
}
#endif // COVERAGE_DISABLE_COROUTINES

requester::Coroutine indexCreator(SensorManager&, const DeviceContext& dev,
                                  const dbus::PropertyMap&)
{
    rec.indexes.push_back(dev.nextIndex("Group"));
    co_return NSM_SW_SUCCESS;
}

Feature feature(std::string_view id, CreatorFn create, std::string tag,
                bool enabled = true)
{
    Feature f;
    f.id = id;
    f.enabled = enabled;
    f.create = create;
    f.params["Tag"] = ParamValue{std::move(tag)};
    return f;
}

constexpr uint64_t hugeBudgetUsec = 1'000'000'000'000ull;

} // namespace

class FeatureDriverTest :
    public ::testing::Test,
    public utils::DBusTest,
    public SensorManagerTest
{
  protected:
    NsmDeviceTable devices;
    common::Event event;

    FeatureDriverTest() : SensorManagerTest(devices)
    {
        rec = Recorder{};
    }

    ~FeatureDriverTest()
    {
        cleanupDeviceSensors(devices);
    }

    DeviceContext makeDev()
    {
        DeviceContext dev;
        dev.name = "HGX_GPU_0";
        dev.kind = "gpu";
        dev.platformPrefix = "HGX_";
        dev.instance = 1;
        return dev;
    }

    // Runs the event loop until the coroutine finishes; the end-of-device
    // yield needs loop iterations
    void drive(const requester::Coroutine& co)
    {
        for (int i = 0; i < 1000 && !co.done(); ++i)
        {
            event.run(std::chrono::microseconds(10000));
        }
        ASSERT_TRUE(co.done());
    }

    requester::Coroutine run(DeviceContext dev, FeatureList features,
                             YieldBudget& budget)
    {
        return createDeviceFeatures(mockManager, std::move(dev),
                                    std::move(features), budget);
    }
};

// ---- YieldBudget ----------------------------------------------------------

TEST_F(FeatureDriverTest, Budget_NotExceededWithinLargeBudget)
{
    YieldBudget budget(event, hugeBudgetUsec);
    EXPECT_FALSE(budget.exceeded());
    EXPECT_EQ(budget.yields(), 0u);
}

TEST_F(FeatureDriverTest, Budget_ZeroBudget_AlwaysExceeded)
{
    YieldBudget budget(event, 0);
    EXPECT_TRUE(budget.exceeded());
}

TEST_F(FeatureDriverTest, Budget_TakeLongestSlice_ResetsToZero)
{
    YieldBudget budget(event, hugeBudgetUsec);
    budget.exceeded();
    budget.takeLongestSliceUsec();
    EXPECT_EQ(budget.takeLongestSliceUsec(), 0u);
}

// The tests guarded by COVERAGE_DISABLE_COROUTINES need real suspension: in
// coverage builds co_await is a no-op and a creator's throw is not caught by
// the coroutine promise.
#ifndef COVERAGE_DISABLE_COROUTINES
TEST_F(FeatureDriverTest, Budget_Yield_CountsAndResumesOnLoop)
{
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = budget.yield();
    // Suspended until the loop runs
    EXPECT_FALSE(co.done());
    EXPECT_EQ(budget.yields(), 0u);
    drive(co);
    EXPECT_EQ(budget.yields(), 1u);
    EXPECT_EQ(co.data(), NSM_SW_SUCCESS);
}
#endif // COVERAGE_DISABLE_COROUTINES

TEST_F(FeatureDriverTest, Budget_YieldIfExceeded_WithinBudget_DoesNotYield)
{
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = budget.yieldIfExceeded();
    EXPECT_TRUE(co.done());
    EXPECT_EQ(budget.yields(), 0u);
}

#ifndef COVERAGE_DISABLE_COROUTINES
TEST_F(FeatureDriverTest, Budget_YieldIfExceeded_OverBudget_Yields)
{
    YieldBudget budget(event, 0);
    auto co = budget.yieldIfExceeded();
    EXPECT_FALSE(co.done());
    drive(co);
    EXPECT_EQ(budget.yields(), 1u);
}
#endif // COVERAGE_DISABLE_COROUTINES

// ---- createDeviceFeatures -------------------------------------------------

TEST_F(FeatureDriverTest, CreatesEnabledFeaturesInListOrder)
{
    FeatureList list;
    list.push_back(feature("A", okCreator, "A"));
    list.push_back(feature("B", okCreator, "B", false));
    list.push_back(feature("C", okCreator, "C"));
    list.push_back(feature("D", okCreator, "D"));

    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);

    EXPECT_EQ(rec.calls, (std::vector<std::string>{"A", "C", "D"}));
    EXPECT_EQ(co.data(), NSM_SW_SUCCESS);
    EXPECT_FALSE(static_cast<bool>(co.exception()));
}

TEST_F(FeatureDriverTest, EmptyList_SucceedsWithFinalYield)
{
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), {}, budget);
    drive(co);
    EXPECT_TRUE(rec.calls.empty());
    EXPECT_EQ(co.data(), NSM_SW_SUCCESS);
    EXPECT_EQ(budget.yields(), 1u);
}

TEST_F(FeatureDriverTest, AllDisabled_NothingCreated)
{
    FeatureList list;
    list.push_back(feature("A", okCreator, "A", false));
    list.push_back(feature("B", okCreator, "B", false));
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_TRUE(rec.calls.empty());
}

TEST_F(FeatureDriverTest, CreatorReceivesResolvedParams)
{
    FeatureList list;
    list.push_back(feature("A", okCreator, "{device}/{instance}"));
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"HGX_GPU_0/1"}));
}

TEST_F(FeatureDriverTest, UnresolvableParams_FeatureSkipped_NextStillRuns)
{
    FeatureList list;
    Feature bad;
    bad.id = "Bad";
    bad.create = okCreator;
    bad.params["Tag"] = ParamValue{"{parentChassis}"s}; // device has none
    list.push_back(bad);
    list.push_back(feature("Good", okCreator, "Good"));

    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"Good"}));
    EXPECT_EQ(co.data(), NSM_SW_SUCCESS);
}

TEST_F(FeatureDriverTest, MissingRequiredVariable_FeatureSkipped)
{
    FeatureList list;
    Feature bad;
    bad.id = "Bad";
    bad.create = okCreator;
    bad.params["Count"] = var<uint64_t>("{Nope}");
    list.push_back(bad);
    list.push_back(feature("Good", okCreator, "Good"));

    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"Good"}));
}

TEST_F(FeatureDriverTest, MissingOptionalVariable_CreatorRunsWithoutParam)
{
    FeatureList list;
    Feature f = feature("A", okCreator, "A");
    f.params["Loc"] = optVar<std::string>("{Nope}");
    list.push_back(f);

    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"A"}));
    EXPECT_EQ(rec.lastParamCount, 1u); // only Tag
}

TEST_F(FeatureDriverTest, FeatureWithoutCreator_Skipped_NextStillRuns)
{
    FeatureList list;
    list.push_back(feature("NoCreator", nullptr, "X"));
    list.push_back(feature("Good", okCreator, "Good"));
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"Good"}));
}

TEST_F(FeatureDriverTest, FailingCreator_DoesNotStopLaterFeatures)
{
    FeatureList list;
    list.push_back(feature("A", okCreator, "A"));
    list.push_back(feature("B", failCreator, "B"));
    list.push_back(feature("C", okCreator, "C"));
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"A", "B", "C"}));
    EXPECT_EQ(co.data(), NSM_SW_SUCCESS);
}

#ifndef COVERAGE_DISABLE_COROUTINES
TEST_F(FeatureDriverTest, ThrowingCreator_DoesNotStopLaterFeatures)
{
    FeatureList list;
    list.push_back(feature("A", throwCreator, "A"));
    list.push_back(feature("B", okCreator, "B"));
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(co.data(), NSM_SW_SUCCESS);
    EXPECT_FALSE(static_cast<bool>(co.exception()));
}
#endif // COVERAGE_DISABLE_COROUTINES

TEST_F(FeatureDriverTest, LargeBudget_OnlyTheEndOfDeviceYield)
{
    FeatureList list;
    for (const char* tag : {"A", "B", "C"})
    {
        list.push_back(feature(tag, okCreator, tag));
    }
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(makeDev(), list, budget);
    drive(co);
    EXPECT_EQ(budget.yields(), 1u);
}

TEST_F(FeatureDriverTest, ZeroBudget_YieldsAfterEveryCreatedFeature)
{
    FeatureList list;
    for (const char* tag : {"A", "B", "C"})
    {
        list.push_back(feature(tag, okCreator, tag));
    }
    list.push_back(feature("Off", okCreator, "Off", false));
    YieldBudget budget(event, 0);
    auto co = run(makeDev(), list, budget);
    drive(co);
    // One per created feature plus the one at the end of the device
    EXPECT_EQ(budget.yields(), 4u);
    EXPECT_EQ(rec.calls.size(), 3u);
}

#ifndef COVERAGE_DISABLE_COROUTINES
TEST_F(FeatureDriverTest, DeviceSuspendsBetweenFeatures_WhenOverBudget)
{
    FeatureList list;
    list.push_back(feature("A", okCreator, "A"));
    list.push_back(feature("B", okCreator, "B"));
    YieldBudget budget(event, 0);
    auto co = run(makeDev(), list, budget);
    // Only the first feature ran before the yield to the loop
    EXPECT_FALSE(co.done());
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"A"}));
    drive(co);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"A", "B"}));
}
#endif // COVERAGE_DISABLE_COROUTINES

TEST_F(FeatureDriverTest, IndexCounters_ShareOneContextAcrossFeatures)
{
    FeatureList list;
    list.push_back(feature("A", indexCreator, "A"));
    list.push_back(feature("B", indexCreator, "B"));
    list.push_back(feature("C", indexCreator, "C"));
    auto dev = makeDev();
    YieldBudget budget(event, hugeBudgetUsec);
    auto co = run(dev, list, budget);
    drive(co);
    EXPECT_EQ(rec.indexes, (std::vector<unsigned>{0, 1, 2}));
    // The driver works on its own copy of the context
    EXPECT_EQ(dev.nextIndex("Group"), 0u);
}

TEST_F(FeatureDriverTest, SecondDevice_StartsItsIndexesAtZero)
{
    FeatureList list;
    list.push_back(feature("A", indexCreator, "A"));
    YieldBudget budget1(event, hugeBudgetUsec);
    auto co1 = run(makeDev(), list, budget1);
    drive(co1);
    YieldBudget budget2(event, hugeBudgetUsec);
    auto co2 = run(makeDev(), list, budget2);
    drive(co2);
    EXPECT_EQ(rec.indexes, (std::vector<unsigned>{0, 0}));
}

#ifndef COVERAGE_DISABLE_COROUTINES
TEST_F(FeatureDriverTest, TwoDevicesInterleave_WhenYielding)
{
    // Budget 0 forces a yield after every feature, so a second device's
    // creation can start while the first one is suspended
    YieldBudget budget1(event, 0);
    YieldBudget budget2(event, 0);
    auto dev1 = makeDev();
    dev1.name = "D1";
    auto dev2 = makeDev();
    dev2.name = "D2";
    FeatureList l1;
    l1.push_back(feature("A", okCreator, "{device}-A"));
    l1.push_back(feature("B", okCreator, "{device}-B"));
    auto co1 = run(dev1, l1, budget1);
    auto co2 = run(dev2, l1, budget2);
    EXPECT_EQ(rec.calls, (std::vector<std::string>{"D1-A", "D2-A"}));
    for (int i = 0; i < 1000 && !(co1.done() && co2.done()); ++i)
    {
        event.run(std::chrono::microseconds(10000));
    }
    ASSERT_TRUE(co1.done());
    ASSERT_TRUE(co2.done());
    EXPECT_EQ(rec.calls.size(), 4u);
    EXPECT_EQ(std::count(rec.calls.begin(), rec.calls.end(), "D1-B"), 1);
    EXPECT_EQ(std::count(rec.calls.begin(), rec.calls.end(), "D2-B"), 1);
}
#endif // COVERAGE_DISABLE_COROUTINES
