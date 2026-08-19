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

#include "nsmThresholdEvaluator.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cmath>

namespace nsm
{

namespace
{

// Compares `value` against a single tier/direction threshold, and on a
// state transition updates the cached alarm, sets the D-Bus alarm property,
// and fires the matching Asserted/Deasserted signal. Tier-specific behavior
// (property setter, signal calls) is supplied via callbacks since the
// sdbusplus-generated tier interfaces (Warning/Critical/HardShutdown/
// SoftShutdown/PerformanceLoss) do not share a common base for these
// differently-named accessors.
template <typename SetAlarmFn, typename OnAssertedFn, typename OnDeassertedFn>
void evaluateTransition(const char* tierLabel, const char* dirLabel,
                        double value, double threshold, bool assertHigh,
                        bool& cachedAlarm, SetAlarmFn&& setAlarm,
                        OnAssertedFn&& onAsserted,
                        OnDeassertedFn&& onDeasserted)
{
    if (std::isnan(threshold))
    {
        return;
    }

    const bool newAlarm = assertHigh ? (value >= threshold)
                                     : (value <= threshold);
    if (newAlarm == cachedAlarm)
    {
        return;
    }

    setAlarm(newAlarm);
    if (newAlarm)
    {
        lg2::info("{TIER}Alarm{DIR} asserted: value={VALUE} threshold={THRESH}",
                  "TIER", tierLabel, "DIR", dirLabel, "VALUE", value, "THRESH",
                  threshold);
        onAsserted(value);
    }
    else
    {
        lg2::info(
            "{TIER}Alarm{DIR} deasserted: value={VALUE} threshold={THRESH}",
            "TIER", tierLabel, "DIR", dirLabel, "VALUE", value, "THRESH",
            threshold);
        onDeasserted(value);
    }
    cachedAlarm = newAlarm;
}

} // namespace

NsmThresholdEvaluator::NsmThresholdEvaluator(
    std::shared_ptr<WarnIntf> warnIntf, std::shared_ptr<CritIntf> critIntf,
    std::shared_ptr<HardShutdownIntf> hardShutdownIntf,
    std::shared_ptr<SoftShutdownIntf> softShutdownIntf,
    std::shared_ptr<PerfLossIntf> perfLossIntf) :
    warnIntf(std::move(warnIntf)), critIntf(std::move(critIntf)),
    hardShutdownIntf(std::move(hardShutdownIntf)),
    softShutdownIntf(std::move(softShutdownIntf)),
    perfLossIntf(std::move(perfLossIntf))
{}

void NsmThresholdEvaluator::evaluate(double value)
{
    if (warnIntf)
    {
        // assert when value >= WarningHigh; deassert when value < WarningHigh
        evaluateTransition("Warning", "High", value, warnIntf->warningHigh(),
                           true, warningAlarmHigh, [this](bool a) {
            warnIntf->warningAlarmHigh(a);
        }, [this](double v) {
            warnIntf->warningHighAlarmAsserted(v);
        }, [this](double v) { warnIntf->warningHighAlarmDeasserted(v); });

        // assert when value <= WarningLow; deassert when value > WarningLow
        evaluateTransition("Warning", "Low", value, warnIntf->warningLow(),
                           false, warningAlarmLow, [this](bool a) {
            warnIntf->warningAlarmLow(a);
        }, [this](double v) {
            warnIntf->warningLowAlarmAsserted(v);
        }, [this](double v) { warnIntf->warningLowAlarmDeasserted(v); });
    }

    if (critIntf)
    {
        // assert when value >= CriticalHigh; deassert when value < CriticalHigh
        evaluateTransition("Critical", "High", value, critIntf->criticalHigh(),
                           true, criticalAlarmHigh, [this](bool a) {
            critIntf->criticalAlarmHigh(a);
        }, [this](double v) {
            critIntf->criticalHighAlarmAsserted(v);
        }, [this](double v) { critIntf->criticalHighAlarmDeasserted(v); });

        // assert when value <= CriticalLow; deassert when value > CriticalLow
        evaluateTransition("Critical", "Low", value, critIntf->criticalLow(),
                           false, criticalAlarmLow, [this](bool a) {
            critIntf->criticalAlarmLow(a);
        }, [this](double v) {
            critIntf->criticalLowAlarmAsserted(v);
        }, [this](double v) { critIntf->criticalLowAlarmDeasserted(v); });
    }

    if (hardShutdownIntf)
    {
        evaluateTransition("HardShutdown", "High", value,
                           hardShutdownIntf->hardShutdownHigh(), true,
                           hardShutdownAlarmHigh, [this](bool a) {
            hardShutdownIntf->hardShutdownAlarmHigh(a);
        }, [this](double v) {
            hardShutdownIntf->hardShutdownHighAlarmAsserted(v);
        }, [this](double v) {
            hardShutdownIntf->hardShutdownHighAlarmDeasserted(v);
        });

        evaluateTransition("HardShutdown", "Low", value,
                           hardShutdownIntf->hardShutdownLow(), false,
                           hardShutdownAlarmLow, [this](bool a) {
            hardShutdownIntf->hardShutdownAlarmLow(a);
        }, [this](double v) {
            hardShutdownIntf->hardShutdownLowAlarmAsserted(v);
        }, [this](double v) {
            hardShutdownIntf->hardShutdownLowAlarmDeasserted(v);
        });
    }

    if (softShutdownIntf)
    {
        evaluateTransition("SoftShutdown", "High", value,
                           softShutdownIntf->softShutdownHigh(), true,
                           softShutdownAlarmHigh, [this](bool a) {
            softShutdownIntf->softShutdownAlarmHigh(a);
        }, [this](double v) {
            softShutdownIntf->softShutdownHighAlarmAsserted(v);
        }, [this](double v) {
            softShutdownIntf->softShutdownHighAlarmDeasserted(v);
        });

        evaluateTransition("SoftShutdown", "Low", value,
                           softShutdownIntf->softShutdownLow(), false,
                           softShutdownAlarmLow, [this](bool a) {
            softShutdownIntf->softShutdownAlarmLow(a);
        }, [this](double v) {
            softShutdownIntf->softShutdownLowAlarmAsserted(v);
        }, [this](double v) {
            softShutdownIntf->softShutdownLowAlarmDeasserted(v);
        });
    }

    if (perfLossIntf)
    {
        evaluateTransition("PerformanceLoss", "High", value,
                           perfLossIntf->performanceLossHigh(), true,
                           performanceLossAlarmHigh, [this](bool a) {
            perfLossIntf->performanceLossAlarmHigh(a);
        }, [this](double v) {
            perfLossIntf->performanceLossHighAlarmAsserted(v);
        }, [this](double v) {
            perfLossIntf->performanceLossHighAlarmDeasserted(v);
        });

        evaluateTransition("PerformanceLoss", "Low", value,
                           perfLossIntf->performanceLossLow(), false,
                           performanceLossAlarmLow, [this](bool a) {
            perfLossIntf->performanceLossAlarmLow(a);
        }, [this](double v) {
            perfLossIntf->performanceLossLowAlarmAsserted(v);
        }, [this](double v) {
            perfLossIntf->performanceLossLowAlarmDeasserted(v);
        });
    }
}

} // namespace nsm
