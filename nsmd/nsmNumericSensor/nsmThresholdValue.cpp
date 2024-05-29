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

#include "nsmThresholdValue.hpp"

namespace nsm
{

NsmThresholdValueWarningLow::NsmThresholdValueWarningLow(
    const std::string& name, const std::string& type,
    std::shared_ptr<ThresholdWarningIntf> intf) :
    NsmThresholdValue(name, type), intf(intf)
{}

void NsmThresholdValueWarningLow::updateReading(double value,
                                                uint64_t /*timestamp*/)
{
    intf->warningLow(value);
}

NsmThresholdValueWarningHigh::NsmThresholdValueWarningHigh(
    const std::string& name, const std::string& type,
    std::shared_ptr<ThresholdWarningIntf> intf) :
    NsmThresholdValue(name, type), intf(intf)
{}

void NsmThresholdValueWarningHigh::updateReading(double value,
                                                 uint64_t /*timestamp*/)
{
    intf->warningHigh(value);
}

NsmThresholdValueCriticalLow::NsmThresholdValueCriticalLow(
    const std::string& name, const std::string& type,
    std::shared_ptr<ThresholdCriticalIntf> intf) :
    NsmThresholdValue(name, type), intf(intf)
{}

void NsmThresholdValueCriticalLow::updateReading(double value,
                                                 uint64_t /*timestamp*/)
{
    intf->criticalLow(value);
}

NsmThresholdValueCriticalHigh::NsmThresholdValueCriticalHigh(
    const std::string& name, const std::string& type,
    std::shared_ptr<ThresholdCriticalIntf> intf) :
    NsmThresholdValue(name, type), intf(intf)
{}

void NsmThresholdValueCriticalHigh::updateReading(double value,
                                                  uint64_t /*timestamp*/)
{
    intf->criticalHigh(value);
}

NsmThresholdValueHardShutdownLow::NsmThresholdValueHardShutdownLow(
    const std::string& name, const std::string& type,
    std::shared_ptr<ThresholdHardShutdownIntf> intf) :
    NsmThresholdValue(name, type), intf(intf)
{}

void NsmThresholdValueHardShutdownLow::updateReading(double value,
                                                     uint64_t /*timestamp*/)
{
    intf->hardShutdownLow(value);
}

NsmThresholdValueHardShutdownHigh::NsmThresholdValueHardShutdownHigh(
    const std::string& name, const std::string& type,
    std::shared_ptr<ThresholdHardShutdownIntf> intf) :
    NsmThresholdValue(name, type), intf(intf)
{}

void NsmThresholdValueHardShutdownHigh::updateReading(double value,
                                                      uint64_t /*timestamp*/)
{
    intf->hardShutdownHigh(value);
}

} // namespace nsm
