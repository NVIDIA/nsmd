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

/*
 * Unit tests for NsmThresholdEvaluator
 *
 * Coverage:
 *   - Happy-path: all ten tier×direction combinations (assert and
 *     deassert) across Warning, Critical, HardShutdown, SoftShutdown,
 *     PerformanceLoss
 *   - Boundary: equality triggers assert (>= / <= semantics)
 *   - NaN threshold: slot silently skipped, alarm state unchanged
 *   - Null interface guard: any subset of tiers skipped, no crash
 *   - Idempotency: repeated evaluate() with no state change is a no-op
 *   - All ten alarms evaluated independently in one call
 *   - HardShutdown/SoftShutdown/PerformanceLoss default to nullptr —
 *     verified inert when omitted
 *   - Signal emission: all 20 tier×direction×transition Asserted/Deasserted
 *     signals, verified via a sdbusplus match subscribed on the fixture's
 *     own bus connection (see expectSignal() on the fixture)
 *
 * Internal-state access: #define private public exposes warningAlarmHigh,
 * warningAlarmLow, criticalAlarmHigh, criticalAlarmLow,
 * hardShutdownAlarmHigh, hardShutdownAlarmLow, softShutdownAlarmHigh,
 * softShutdownAlarmLow, performanceLossAlarmHigh, performanceLossAlarmLow
 * for direct read/write in tests.
 */

#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#define private public
#define protected public
#include "nsmThresholdEvaluator.hpp"
#undef private
#undef protected

using namespace nsm;

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

// Fixture provides a D-Bus connection and pre-built threshold interface
// objects that can be injected into NsmThresholdEvaluator.  Each test
// receives fresh interface objects (SetUp/TearDown per TEST_F).
class NsmThresholdEvaluatorTest : public ::testing::Test
{
  protected:
    sdbusplus::bus_t bus = sdbusplus::bus::new_default();

    std::shared_ptr<NsmThresholdEvaluator::WarnIntf> warnIntf;
    std::shared_ptr<NsmThresholdEvaluator::CritIntf> critIntf;
    std::shared_ptr<NsmThresholdEvaluator::HardShutdownIntf> hardShutdownIntf;
    std::shared_ptr<NsmThresholdEvaluator::SoftShutdownIntf> softShutdownIntf;
    std::shared_ptr<NsmThresholdEvaluator::PerfLossIntf> perfLossIntf;

    void SetUp() override
    {
        warnIntf = std::make_shared<NsmThresholdEvaluator::WarnIntf>(
            bus, "/xyz/openbmc_project/sensors/threshold/test_warn");
        critIntf = std::make_shared<NsmThresholdEvaluator::CritIntf>(
            bus, "/xyz/openbmc_project/sensors/threshold/test_crit");
        hardShutdownIntf =
            std::make_shared<NsmThresholdEvaluator::HardShutdownIntf>(
                bus,
                "/xyz/openbmc_project/sensors/threshold/test_hardshutdown");
        softShutdownIntf =
            std::make_shared<NsmThresholdEvaluator::SoftShutdownIntf>(
                bus,
                "/xyz/openbmc_project/sensors/threshold/test_softshutdown");
        perfLossIntf = std::make_shared<NsmThresholdEvaluator::PerfLossIntf>(
            bus, "/xyz/openbmc_project/sensors/threshold/test_perfloss");

        // Default: all thresholds NaN (not configured); no alarm will fire
        // unless a test explicitly sets a threshold value.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        warnIntf->warningHigh(nan);
        warnIntf->warningLow(nan);
        critIntf->criticalHigh(nan);
        critIntf->criticalLow(nan);
        hardShutdownIntf->hardShutdownHigh(nan);
        hardShutdownIntf->hardShutdownLow(nan);
        softShutdownIntf->softShutdownHigh(nan);
        softShutdownIntf->softShutdownLow(nan);
        perfLossIntf->performanceLossHigh(nan);
        perfLossIntf->performanceLossLow(nan);
    }

    // Result of expectSignal(): whether the matched signal was observed
    // before the pump budget ran out, and the SensorValue payload it
    // carried (0.0 if not received).
    struct SignalCapture
    {
        bool received{false};
        double value{0.0};
    };

    // Subscribes a match on `bus` for the given object path / interface /
    // signal member, runs `trigger` (expected to cause the evaluator to
    // emit that signal), then pumps `bus` until the signal round-trips
    // back from the daemon or the pump budget is exhausted.
    //
    // Same-connection self-reception: the interface objects (warnIntf,
    // critIntf, ...) and the match below both live on `bus`, the single
    // connection this fixture opens via sdbusplus::bus::new_default(). The
    // emitted signal still round-trips through the D-Bus daemon and is
    // delivered back to this connection because the match subscribes to
    // it; process_discard() dispatches it to the match callback.
    SignalCapture expectSignal(const std::string& path,
                               const std::string& interface,
                               const std::string& member,
                               const std::function<void()>& trigger)
    {
        SignalCapture capture;
        sdbusplus::bus::match_t match(
            bus,
            sdbusplus::bus::match::rules::type::signal() +
                sdbusplus::bus::match::rules::path(path) +
                sdbusplus::bus::match::rules::interface(interface) +
                sdbusplus::bus::match::rules::member(member),
            [&capture](sdbusplus::message_t& msg) {
            double sensorValue = 0.0;
            msg.read(sensorValue);
            capture.value = sensorValue;
            capture.received = true;
        });

        trigger();

        for (int i = 0; i < 200 && !capture.received; ++i)
        {
            bus.process_discard();
            if (!capture.received)
            {
                bus.wait(std::chrono::milliseconds(5));
            }
        }

        return capture;
    }
};

// ===========================================================================
// Happy-path tests — tier × direction combinations
// ===========================================================================

// Test 1: warningAlarmHigh is set to true when value >= WarningHigh
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateWarningHigh_Assert_WhenValueExceedsThreshold)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    evaluator.evaluate(85.0);

    EXPECT_TRUE(evaluator.warningAlarmHigh);
}

// Test 2: warningAlarmHigh is cleared when value drops below WarningHigh
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateWarningHigh_Deassert_WhenValueDropsBelowThreshold)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    // Pre-set alarm state to simulate a prior assertion
    evaluator.warningAlarmHigh = true;
    // Force the interface alarm property to match so evaluate() sees
    // the current state and only triggers a deassert transition
    warnIntf->warningAlarmHigh(true);

    evaluator.evaluate(75.0); // 75.0 < 80.0 → deassert

    EXPECT_FALSE(evaluator.warningAlarmHigh);
}

// Test 3: warningAlarmLow is set to true when value <= WarningLow
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateWarningLow_Assert_WhenValueDropsBelowThreshold)
{
    warnIntf->warningLow(15.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    evaluator.evaluate(10.0); // 10.0 <= 15.0 → assert

    EXPECT_TRUE(evaluator.warningAlarmLow);
}

// Test 4: warningAlarmLow is cleared when value rises above WarningLow
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateWarningLow_Deassert_WhenValueRisesAboveThreshold)
{
    warnIntf->warningLow(15.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    // Pre-set alarm state to simulate a prior assertion
    evaluator.warningAlarmLow = true;
    warnIntf->warningAlarmLow(true);

    evaluator.evaluate(20.0); // 20.0 > 15.0 → deassert

    EXPECT_FALSE(evaluator.warningAlarmLow);
}

// Test 5: criticalAlarmHigh is set to true when value >= CriticalHigh
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateCriticalHigh_Assert_WhenValueExceedsCriticalThreshold)
{
    critIntf->criticalHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    evaluator.evaluate(95.0); // 95.0 >= 90.0 → assert

    EXPECT_TRUE(evaluator.criticalAlarmHigh);
}

// Test 5b: criticalAlarmHigh is cleared when value drops below CriticalHigh
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateCriticalHigh_Deassert_WhenValueDropsBelowThreshold)
{
    critIntf->criticalHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    evaluator.criticalAlarmHigh = true;
    critIntf->criticalAlarmHigh(true);

    evaluator.evaluate(85.0); // 85.0 < 90.0 → deassert

    EXPECT_FALSE(evaluator.criticalAlarmHigh);
}

// Test 6: criticalAlarmLow is set to true when value <= CriticalLow
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateCriticalLow_Assert_WhenValueDropsBelowCriticalThreshold)
{
    critIntf->criticalLow(10.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    evaluator.evaluate(5.0); // 5.0 <= 10.0 → assert

    EXPECT_TRUE(evaluator.criticalAlarmLow);
}

// Test 6e: criticalAlarmLow is cleared when value rises above CriticalLow
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateCriticalLow_Deassert_WhenValueRisesAboveThreshold)
{
    critIntf->criticalLow(10.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    evaluator.criticalAlarmLow = true;
    critIntf->criticalAlarmLow(true);

    evaluator.evaluate(15.0); // 15.0 > 10.0 → deassert

    EXPECT_FALSE(evaluator.criticalAlarmLow);
}

// Test 7: assert triggers at exactly the threshold value (>= semantics)
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateAtBoundary_Assert_WhenValueEqualsThreshold)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    evaluator.evaluate(80.0); // exactly at boundary: 80.0 >= 80.0 → assert

    EXPECT_TRUE(evaluator.warningAlarmHigh);
}

// ===========================================================================
// HardShutdown tier tests (confirmed distinct from Critical;
// spec-sanctioning of the mapping tracked separately
// ===========================================================================

// Test 6a: hardShutdownAlarmHigh is set to true when value >= HardShutdownHigh
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateHardShutdownHigh_Assert_WhenValueExceedsThreshold)
{
    hardShutdownIntf->hardShutdownHigh(105.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);

    evaluator.evaluate(110.0);

    EXPECT_TRUE(evaluator.hardShutdownAlarmHigh);
}

// Test 6b: hardShutdownAlarmHigh is cleared when value drops below threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateHardShutdownHigh_Deassert_WhenValueDropsBelowThreshold)
{
    hardShutdownIntf->hardShutdownHigh(105.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);

    evaluator.hardShutdownAlarmHigh = true;
    hardShutdownIntf->hardShutdownAlarmHigh(true);

    evaluator.evaluate(95.0); // 95.0 < 105.0 → deassert

    EXPECT_FALSE(evaluator.hardShutdownAlarmHigh);
}

// Test 6c: hardShutdownAlarmLow is set to true when value <= HardShutdownLow
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateHardShutdownLow_Assert_WhenValueDropsBelowThreshold)
{
    hardShutdownIntf->hardShutdownLow(60.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);

    evaluator.evaluate(55.0); // 55.0 <= 60.0 → assert

    EXPECT_TRUE(evaluator.hardShutdownAlarmLow);
}

// Test 6d: hardShutdownAlarmLow is cleared when value rises above threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateHardShutdownLow_Deassert_WhenValueRisesAboveThreshold)
{
    hardShutdownIntf->hardShutdownLow(60.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);

    evaluator.hardShutdownAlarmLow = true;
    hardShutdownIntf->hardShutdownAlarmLow(true);

    evaluator.evaluate(65.0); // 65.0 > 60.0 → deassert

    EXPECT_FALSE(evaluator.hardShutdownAlarmLow);
}

// ===========================================================================
// SoftShutdown / PerformanceLoss tier tests (STH-REQ-09, infra-only tiers)
// ===========================================================================

// Test 7a: softShutdownAlarmHigh is set to true when value >= SoftShutdownHigh
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateSoftShutdownHigh_Assert_WhenValueExceedsThreshold)
{
    softShutdownIntf->softShutdownHigh(100.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);

    evaluator.evaluate(105.0);

    EXPECT_TRUE(softShutdownIntf->softShutdownAlarmHigh());
}

// Test 7b: softShutdownAlarmHigh is cleared when value drops below threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateSoftShutdownHigh_Deassert_WhenValueDropsBelowThreshold)
{
    softShutdownIntf->softShutdownHigh(100.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);
    evaluator.evaluate(105.0); // establish asserted state via evaluate()
    ASSERT_TRUE(softShutdownIntf->softShutdownAlarmHigh());

    evaluator.evaluate(90.0); // 90.0 < 100.0 → deassert

    EXPECT_FALSE(softShutdownIntf->softShutdownAlarmHigh());
}

// Test 7c: softShutdownAlarmLow is set to true when value <= SoftShutdownLow
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateSoftShutdownLow_Assert_WhenValueDropsBelowThreshold)
{
    softShutdownIntf->softShutdownLow(65.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);

    evaluator.evaluate(60.0); // 60.0 <= 65.0 → assert

    EXPECT_TRUE(softShutdownIntf->softShutdownAlarmLow());
}

// Test 7d: softShutdownAlarmLow is cleared when value rises above threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateSoftShutdownLow_Deassert_WhenValueRisesAboveThreshold)
{
    softShutdownIntf->softShutdownLow(65.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);
    evaluator.evaluate(60.0); // establish asserted state via evaluate()
    ASSERT_TRUE(softShutdownIntf->softShutdownAlarmLow());

    evaluator.evaluate(70.0); // 70.0 > 65.0 → deassert

    EXPECT_FALSE(softShutdownIntf->softShutdownAlarmLow());
}

// Test 7e: performanceLossAlarmHigh is set to true when value >= threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossHigh_Assert_WhenValueExceedsThreshold)
{
    perfLossIntf->performanceLossHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);

    evaluator.evaluate(95.0);

    EXPECT_TRUE(perfLossIntf->performanceLossAlarmHigh());
}

// Test 7e2: performanceLossAlarmHigh is cleared when value drops below
// threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossHigh_Deassert_WhenValueDropsBelowThreshold)
{
    perfLossIntf->performanceLossHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);
    evaluator.evaluate(95.0); // establish asserted state via evaluate()
    ASSERT_TRUE(perfLossIntf->performanceLossAlarmHigh());

    evaluator.evaluate(85.0); // 85.0 < 90.0 → deassert

    EXPECT_FALSE(perfLossIntf->performanceLossAlarmHigh());
}

// Test 7f: performanceLossAlarmLow is set to true when value <= threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossLow_Assert_WhenValueDropsBelowThreshold)
{
    perfLossIntf->performanceLossLow(80.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);

    evaluator.evaluate(75.0);

    EXPECT_TRUE(perfLossIntf->performanceLossAlarmLow());
}

// Test 7f2: performanceLossAlarmLow is cleared when value rises above
// threshold
TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossLow_Deassert_WhenValueRisesAboveThreshold)
{
    perfLossIntf->performanceLossLow(80.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);
    evaluator.evaluate(75.0); // establish asserted state via evaluate()
    ASSERT_TRUE(perfLossIntf->performanceLossAlarmLow());

    evaluator.evaluate(85.0); // 85.0 > 80.0 → deassert

    EXPECT_FALSE(perfLossIntf->performanceLossAlarmLow());
}

// Test 7g: nullptr hardShutdownIntf, softShutdownIntf, perfLossIntf (the
// default) — tiers stay inert, matching STH-REQ-09's "infra-only, no
// consumer yet" state. Confirms omitting the new constructor params is safe
// and behaviorally identical to pre-STH-REQ-09 callers.
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateDefaultConstructorArgs_NewTiersInert_NoCrash)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr); // 2-arg call site

    evaluator.evaluate(85.0);                           // must not crash

    EXPECT_TRUE(evaluator.warningAlarmHigh);
    EXPECT_FALSE(evaluator.hardShutdownAlarmHigh);
    EXPECT_FALSE(evaluator.hardShutdownAlarmLow);
    EXPECT_FALSE(evaluator.softShutdownAlarmHigh);
    EXPECT_FALSE(evaluator.softShutdownAlarmLow);
    EXPECT_FALSE(evaluator.performanceLossAlarmHigh);
    EXPECT_FALSE(evaluator.performanceLossAlarmLow);
}

// ===========================================================================
// Error / edge-path tests
// ===========================================================================

// Test 8: NaN threshold → slot is silently skipped; alarm state remains false
TEST_F(NsmThresholdEvaluatorTest, EvaluateNanThreshold_NoAlarmChange)
{
    // warningHigh is NaN (set in SetUp) → threshold not configured → skip
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    evaluator.evaluate(85.0);

    EXPECT_FALSE(evaluator.warningAlarmHigh); // unchanged from initial false
}

// Test 9: nullptr warnIntf → warning tier skipped entirely; critical still
//         evaluated normally; no crash
TEST_F(NsmThresholdEvaluatorTest, EvaluateNullWarnIntf_NoCrash)
{
    critIntf->criticalHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    evaluator.evaluate(95.0); // must not crash

    // Critical tier fires; warning booleans are untouched
    EXPECT_TRUE(evaluator.criticalAlarmHigh);
    EXPECT_FALSE(evaluator.warningAlarmHigh);
    EXPECT_FALSE(evaluator.warningAlarmLow);
}

// Test 10: both interfaces nullptr → evaluate() is a no-op; no crash
TEST_F(NsmThresholdEvaluatorTest, EvaluateNullBothIntfs_NoCrash)
{
    NsmThresholdEvaluator evaluator(nullptr, nullptr);

    evaluator.evaluate(85.0); // must not crash

    EXPECT_FALSE(evaluator.warningAlarmHigh);
    EXPECT_FALSE(evaluator.warningAlarmLow);
    EXPECT_FALSE(evaluator.criticalAlarmHigh);
    EXPECT_FALSE(evaluator.criticalAlarmLow);
}

// Test 10a: all five interfaces nullptr → evaluate() is a no-op; no crash
TEST_F(NsmThresholdEvaluatorTest, EvaluateNullAllFiveIntfs_NoCrash)
{
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    nullptr);

    evaluator.evaluate(85.0); // must not crash

    EXPECT_FALSE(evaluator.warningAlarmHigh);
    EXPECT_FALSE(evaluator.warningAlarmLow);
    EXPECT_FALSE(evaluator.criticalAlarmHigh);
    EXPECT_FALSE(evaluator.criticalAlarmLow);
    EXPECT_FALSE(evaluator.hardShutdownAlarmHigh);
    EXPECT_FALSE(evaluator.hardShutdownAlarmLow);
    EXPECT_FALSE(evaluator.softShutdownAlarmHigh);
    EXPECT_FALSE(evaluator.softShutdownAlarmLow);
    EXPECT_FALSE(evaluator.performanceLossAlarmHigh);
    EXPECT_FALSE(evaluator.performanceLossAlarmLow);
}

// Test 11: evaluate() called twice above threshold → state stays asserted;
//          no redundant transition (idempotent)
TEST_F(NsmThresholdEvaluatorTest, EvaluateNoStateChange_NoRepeatTransition)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    evaluator.evaluate(85.0); // first call: false → true (assert)
    EXPECT_TRUE(evaluator.warningAlarmHigh);

    evaluator.evaluate(85.0); // second call: already true → no transition
    EXPECT_TRUE(evaluator.warningAlarmHigh);
}

// Test 12: evaluate() called below threshold when already deasserted →
//          state stays false; no redundant transition
TEST_F(NsmThresholdEvaluatorTest,
       EvaluateNoStateChange_BelowThresholdAfterDeassert)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);
    // Initial state is false; evaluate below threshold → stays false

    evaluator.evaluate(75.0); // 75.0 < 80.0; alarm already false → no change

    EXPECT_FALSE(evaluator.warningAlarmHigh);
}

// Test 13: all four tier×direction combinations evaluated independently
//          in a single evaluate() call
TEST_F(NsmThresholdEvaluatorTest, EvaluateAllFourDirections_IndependentState)
{
    warnIntf->warningHigh(80.0);
    warnIntf->warningLow(20.0);
    critIntf->criticalHigh(90.0);
    critIntf->criticalLow(10.0);

    NsmThresholdEvaluator evaluator(warnIntf, critIntf);

    // value=5.0 is:
    //   below WarningLow(20)   → warningAlarmLow asserted
    //   below CriticalLow(10)  → criticalAlarmLow asserted
    //   below WarningHigh(80)  → warningAlarmHigh NOT asserted
    //   below CriticalHigh(90) → criticalAlarmHigh NOT asserted
    evaluator.evaluate(5.0);

    EXPECT_TRUE(evaluator.warningAlarmLow);
    EXPECT_TRUE(evaluator.criticalAlarmLow);
    EXPECT_FALSE(evaluator.warningAlarmHigh);
    EXPECT_FALSE(evaluator.criticalAlarmHigh);
}

// Test 13a: all ten tier×direction combinations evaluated independently
//           in a single evaluate() call, including HardShutdown,
//           SoftShutdown, and PerformanceLoss
TEST_F(NsmThresholdEvaluatorTest, EvaluateAllTenDirections_IndependentState)
{
    warnIntf->warningHigh(80.0);
    warnIntf->warningLow(20.0);
    critIntf->criticalHigh(90.0);
    critIntf->criticalLow(10.0);
    hardShutdownIntf->hardShutdownHigh(105.0);
    hardShutdownIntf->hardShutdownLow(2.0);
    softShutdownIntf->softShutdownHigh(100.0);
    softShutdownIntf->softShutdownLow(5.0);
    perfLossIntf->performanceLossHigh(70.0);
    perfLossIntf->performanceLossLow(15.0);

    NsmThresholdEvaluator evaluator(warnIntf, critIntf, hardShutdownIntf,
                                    softShutdownIntf, perfLossIntf);

    // value=12.0 is:
    //   below WarningLow(20)         → warningAlarmLow asserted
    //   above CriticalLow(10)        → criticalAlarmLow NOT asserted
    //   above HardShutdownLow(2)     → hardShutdownAlarmLow NOT asserted
    //   above SoftShutdownLow(5)     → softShutdownAlarmLow NOT asserted
    //   below PerformanceLossLow(15) → performanceLossAlarmLow asserted
    //   below all five *High thresholds → none of the High alarms assert
    evaluator.evaluate(12.0);

    EXPECT_TRUE(evaluator.warningAlarmLow);
    EXPECT_FALSE(evaluator.criticalAlarmLow);
    EXPECT_FALSE(evaluator.hardShutdownAlarmLow);
    EXPECT_FALSE(softShutdownIntf->softShutdownAlarmLow());
    EXPECT_TRUE(perfLossIntf->performanceLossAlarmLow());
    EXPECT_FALSE(evaluator.warningAlarmHigh);
    EXPECT_FALSE(evaluator.criticalAlarmHigh);
    EXPECT_FALSE(evaluator.hardShutdownAlarmHigh);
    EXPECT_FALSE(softShutdownIntf->softShutdownAlarmHigh());
    EXPECT_FALSE(perfLossIntf->performanceLossAlarmHigh());
}

// ===========================================================================
// Signal emission tests — all 20 tier × direction × transition slots
//
// Each test subscribes a sdbusplus match for the exact signal under test,
// triggers the evaluator, then pumps the fixture's bus (expectSignal(),
// declared in the fixture) until the signal round-trips back through the
// D-Bus daemon or the pump budget is exhausted. See expectSignal()'s
// comment for why self-reception on a single bus connection works.
// ===========================================================================

// --- Warning --------------------------------------------------------------

TEST_F(NsmThresholdEvaluatorTest, EvaluateWarningHigh_EmitsAssertedSignal)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_warn",
        NsmThresholdEvaluator::WarnIntf::interface, "WarningHighAlarmAsserted",
        [&]() { evaluator.evaluate(85.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 85.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateWarningHigh_EmitsDeassertedSignal)
{
    warnIntf->warningHigh(80.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);
    evaluator.warningAlarmHigh = true;
    warnIntf->warningAlarmHigh(true);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_warn",
        NsmThresholdEvaluator::WarnIntf::interface,
        "WarningHighAlarmDeasserted", [&]() { evaluator.evaluate(75.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 75.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateWarningLow_EmitsAssertedSignal)
{
    warnIntf->warningLow(15.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_warn",
        NsmThresholdEvaluator::WarnIntf::interface, "WarningLowAlarmAsserted",
        [&]() { evaluator.evaluate(10.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 10.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateWarningLow_EmitsDeassertedSignal)
{
    warnIntf->warningLow(15.0);
    NsmThresholdEvaluator evaluator(warnIntf, nullptr);
    evaluator.warningAlarmLow = true;
    warnIntf->warningAlarmLow(true);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_warn",
        NsmThresholdEvaluator::WarnIntf::interface, "WarningLowAlarmDeasserted",
        [&]() { evaluator.evaluate(20.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 20.0);
}

// --- Critical ---------------------------------------------------------

TEST_F(NsmThresholdEvaluatorTest, EvaluateCriticalHigh_EmitsAssertedSignal)
{
    critIntf->criticalHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_crit",
        NsmThresholdEvaluator::CritIntf::interface, "CriticalHighAlarmAsserted",
        [&]() { evaluator.evaluate(95.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 95.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateCriticalHigh_EmitsDeassertedSignal)
{
    critIntf->criticalHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);
    evaluator.criticalAlarmHigh = true;
    critIntf->criticalAlarmHigh(true);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_crit",
        NsmThresholdEvaluator::CritIntf::interface,
        "CriticalHighAlarmDeasserted", [&]() { evaluator.evaluate(85.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 85.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateCriticalLow_EmitsAssertedSignal)
{
    critIntf->criticalLow(10.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_crit",
        NsmThresholdEvaluator::CritIntf::interface, "CriticalLowAlarmAsserted",
        [&]() { evaluator.evaluate(5.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 5.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateCriticalLow_EmitsDeassertedSignal)
{
    critIntf->criticalLow(10.0);
    NsmThresholdEvaluator evaluator(nullptr, critIntf);
    evaluator.criticalAlarmLow = true;
    critIntf->criticalAlarmLow(true);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_crit",
        NsmThresholdEvaluator::CritIntf::interface,
        "CriticalLowAlarmDeasserted", [&]() { evaluator.evaluate(15.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 15.0);
}

// --- HardShutdown -------------------------------------------------------

TEST_F(NsmThresholdEvaluatorTest, EvaluateHardShutdownHigh_EmitsAssertedSignal)
{
    hardShutdownIntf->hardShutdownHigh(105.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_hardshutdown",
        NsmThresholdEvaluator::HardShutdownIntf::interface,
        "HardShutdownHighAlarmAsserted", [&]() { evaluator.evaluate(110.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 110.0);
}

TEST_F(NsmThresholdEvaluatorTest,
       EvaluateHardShutdownHigh_EmitsDeassertedSignal)
{
    hardShutdownIntf->hardShutdownHigh(105.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);
    evaluator.hardShutdownAlarmHigh = true;
    hardShutdownIntf->hardShutdownAlarmHigh(true);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_hardshutdown",
        NsmThresholdEvaluator::HardShutdownIntf::interface,
        "HardShutdownHighAlarmDeasserted", [&]() { evaluator.evaluate(95.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 95.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateHardShutdownLow_EmitsAssertedSignal)
{
    hardShutdownIntf->hardShutdownLow(60.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_hardshutdown",
        NsmThresholdEvaluator::HardShutdownIntf::interface,
        "HardShutdownLowAlarmAsserted", [&]() { evaluator.evaluate(55.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 55.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateHardShutdownLow_EmitsDeassertedSignal)
{
    hardShutdownIntf->hardShutdownLow(60.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, hardShutdownIntf);
    evaluator.hardShutdownAlarmLow = true;
    hardShutdownIntf->hardShutdownAlarmLow(true);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_hardshutdown",
        NsmThresholdEvaluator::HardShutdownIntf::interface,
        "HardShutdownLowAlarmDeasserted", [&]() { evaluator.evaluate(65.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 65.0);
}

// --- SoftShutdown -------------------------------------------------------

TEST_F(NsmThresholdEvaluatorTest, EvaluateSoftShutdownHigh_EmitsAssertedSignal)
{
    softShutdownIntf->softShutdownHigh(100.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_softshutdown",
        NsmThresholdEvaluator::SoftShutdownIntf::interface,
        "SoftShutdownHighAlarmAsserted", [&]() { evaluator.evaluate(105.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 105.0);
}

TEST_F(NsmThresholdEvaluatorTest,
       EvaluateSoftShutdownHigh_EmitsDeassertedSignal)
{
    softShutdownIntf->softShutdownHigh(100.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);
    evaluator.evaluate(105.0); // establish asserted state via evaluate()
    ASSERT_TRUE(softShutdownIntf->softShutdownAlarmHigh());

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_softshutdown",
        NsmThresholdEvaluator::SoftShutdownIntf::interface,
        "SoftShutdownHighAlarmDeasserted", [&]() { evaluator.evaluate(90.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 90.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateSoftShutdownLow_EmitsAssertedSignal)
{
    softShutdownIntf->softShutdownLow(65.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_softshutdown",
        NsmThresholdEvaluator::SoftShutdownIntf::interface,
        "SoftShutdownLowAlarmAsserted", [&]() { evaluator.evaluate(60.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 60.0);
}

TEST_F(NsmThresholdEvaluatorTest, EvaluateSoftShutdownLow_EmitsDeassertedSignal)
{
    softShutdownIntf->softShutdownLow(65.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, softShutdownIntf,
                                    nullptr);
    evaluator.evaluate(60.0); // establish asserted state via evaluate()
    ASSERT_TRUE(softShutdownIntf->softShutdownAlarmLow());

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_softshutdown",
        NsmThresholdEvaluator::SoftShutdownIntf::interface,
        "SoftShutdownLowAlarmDeasserted", [&]() { evaluator.evaluate(70.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 70.0);
}

// --- PerformanceLoss ------------------------------------------------------

TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossHigh_EmitsAssertedSignal)
{
    perfLossIntf->performanceLossHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);

    auto capture =
        expectSignal("/xyz/openbmc_project/sensors/threshold/test_perfloss",
                     NsmThresholdEvaluator::PerfLossIntf::interface,
                     "PerformanceLossHighAlarmAsserted",
                     [&]() { evaluator.evaluate(95.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 95.0);
}

TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossHigh_EmitsDeassertedSignal)
{
    perfLossIntf->performanceLossHigh(90.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);
    evaluator.evaluate(95.0); // establish asserted state via evaluate()
    ASSERT_TRUE(perfLossIntf->performanceLossAlarmHigh());

    auto capture =
        expectSignal("/xyz/openbmc_project/sensors/threshold/test_perfloss",
                     NsmThresholdEvaluator::PerfLossIntf::interface,
                     "PerformanceLossHighAlarmDeasserted",
                     [&]() { evaluator.evaluate(85.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 85.0);
}

TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossLow_EmitsAssertedSignal)
{
    perfLossIntf->performanceLossLow(80.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);

    auto capture = expectSignal(
        "/xyz/openbmc_project/sensors/threshold/test_perfloss",
        NsmThresholdEvaluator::PerfLossIntf::interface,
        "PerformanceLossLowAlarmAsserted", [&]() { evaluator.evaluate(75.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 75.0);
}

TEST_F(NsmThresholdEvaluatorTest,
       EvaluatePerformanceLossLow_EmitsDeassertedSignal)
{
    perfLossIntf->performanceLossLow(80.0);
    NsmThresholdEvaluator evaluator(nullptr, nullptr, nullptr, nullptr,
                                    perfLossIntf);
    evaluator.evaluate(75.0); // establish asserted state via evaluate()
    ASSERT_TRUE(perfLossIntf->performanceLossAlarmLow());

    auto capture =
        expectSignal("/xyz/openbmc_project/sensors/threshold/test_perfloss",
                     NsmThresholdEvaluator::PerfLossIntf::interface,
                     "PerformanceLossLowAlarmDeasserted",
                     [&]() { evaluator.evaluate(85.0); });

    EXPECT_TRUE(capture.received);
    EXPECT_DOUBLE_EQ(capture.value, 85.0);
}
