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

#pragma once

#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/HardShutdown/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/PerformanceLoss/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/SoftShutdown/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>

#include <memory>

namespace nsm
{

/**
 * @class NsmThresholdEvaluator
 *
 * Evaluates sensor readings against Warning, Critical, HardShutdown,
 * SoftShutdown, and PerformanceLoss threshold tiers and updates D-Bus alarm
 * state on the corresponding sdbusplus interface objects.
 *
 * The evaluator uses a simple level comparison with no hysteresis deadband:
 *   Upper: assert when value >= threshold; deassert when value < threshold
 *   Lower: assert when value <= threshold; deassert when value > threshold
 *
 * Alarm state and signals are emitted only on transitions. NaN threshold
 * values are silently skipped (threshold not configured for that slot).
 *
 * This evaluator must only be called with a valid (non-NaN) reading and
 * only when the sensor is available and functional. Those pre-conditions
 * are enforced by NsmNumericSensorDbusValue::updateReading().
 *
 * HardShutdown, SoftShutdown, and PerformanceLoss are infra-only tiers
 * today: no sensor is currently configured with any of the three on any
 * platform, pending Platform Architecture's per-sensor PDI enumeration.
 * Whether HardShutdown is the spec-sanctioned mapping for the NSM
 * Fatal/Non-recoverable tier (vs. an nsmd implementation convention) is
 * tracked separately —
 * the tier itself is confirmed distinct from Critical. Passing nullptr for
 * hardShutdownIntf/
 * softShutdownIntf/perfLossIntf (the default) leaves those tiers inert —
 * evaluation for a given sensor activates automatically once
 * entity-manager configures a threshold value for that tier, with no
 * further code change.
 */
class NsmThresholdEvaluator
{
  public:
    using WarnIntf =
        sdbusplus::server::xyz::openbmc_project::sensor::threshold::Warning;
    using CritIntf =
        sdbusplus::server::xyz::openbmc_project::sensor::threshold::Critical;
    using HardShutdownIntf = sdbusplus::server::xyz::openbmc_project::sensor::
        threshold::HardShutdown;
    using SoftShutdownIntf = sdbusplus::server::xyz::openbmc_project::sensor::
        threshold::SoftShutdown;
    using PerfLossIntf = sdbusplus::server::xyz::openbmc_project::sensor::
        threshold::PerformanceLoss;

    NsmThresholdEvaluator(
        std::shared_ptr<WarnIntf> warnIntf, std::shared_ptr<CritIntf> critIntf,
        std::shared_ptr<HardShutdownIntf> hardShutdownIntf = nullptr,
        std::shared_ptr<SoftShutdownIntf> softShutdownIntf = nullptr,
        std::shared_ptr<PerfLossIntf> perfLossIntf = nullptr);

    /**
     * @brief Evaluate a valid sensor reading against all configured tiers.
     *
     * For each configured tier and direction: compare reading against the
     * threshold value, update the alarm boolean property on state change,
     * and emit the corresponding Asserted / Deasserted signal.
     *
     * @param value  Current sensor reading. Must not be NaN.
     */
    void evaluate(double value);

  private:
    std::shared_ptr<WarnIntf> warnIntf;
    std::shared_ptr<CritIntf> critIntf;
    std::shared_ptr<HardShutdownIntf> hardShutdownIntf;
    std::shared_ptr<SoftShutdownIntf> softShutdownIntf;
    std::shared_ptr<PerfLossIntf> perfLossIntf;

    bool warningAlarmHigh{false};
    bool warningAlarmLow{false};
    bool criticalAlarmHigh{false};
    bool criticalAlarmLow{false};
    bool hardShutdownAlarmHigh{false};
    bool hardShutdownAlarmLow{false};
    bool softShutdownAlarmHigh{false};
    bool softShutdownAlarmLow{false};
    bool performanceLossAlarmHigh{false};
    bool performanceLossAlarmLow{false};
};

} // namespace nsm
