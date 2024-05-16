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

#include "nsmNumericSensor.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Critical/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/HardShutdown/server.hpp>
#include <xyz/openbmc_project/Sensor/Threshold/Warning/server.hpp>

using ThresholdWarningIntf =
    sdbusplus::server::xyz::openbmc_project::sensor::threshold::Warning;
using ThresholdCriticalIntf =
    sdbusplus::server::xyz::openbmc_project::sensor::threshold::Critical;
using ThresholdHardShutdownIntf =
    sdbusplus::server::xyz::openbmc_project::sensor::threshold::HardShutdown;

namespace nsm
{
class NsmThresholdValue : public NsmNumericSensorValue, public NsmObject
{
  public:
    using NsmObject::NsmObject;
};

class NsmThresholdValueWarningLow : public NsmThresholdValue
{
  public:
    NsmThresholdValueWarningLow(const std::string& name,
                                const std::string& type,
                                std::shared_ptr<ThresholdWarningIntf> intf);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    std::shared_ptr<ThresholdWarningIntf> intf;
};

class NsmThresholdValueWarningHigh : public NsmThresholdValue
{
  public:
    NsmThresholdValueWarningHigh(const std::string& name,
                                 const std::string& type,
                                 std::shared_ptr<ThresholdWarningIntf> intf);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    std::shared_ptr<ThresholdWarningIntf> intf;
};

class NsmThresholdValueCriticalLow : public NsmThresholdValue
{
  public:
    NsmThresholdValueCriticalLow(const std::string& name,
                                 const std::string& type,
                                 std::shared_ptr<ThresholdCriticalIntf> intf);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    std::shared_ptr<ThresholdCriticalIntf> intf;
};

class NsmThresholdValueCriticalHigh : public NsmThresholdValue
{
  public:
    NsmThresholdValueCriticalHigh(const std::string& name,
                                  const std::string& type,
                                  std::shared_ptr<ThresholdCriticalIntf> intf);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    std::shared_ptr<ThresholdCriticalIntf> intf;
};

class NsmThresholdValueHardShutdownLow : public NsmThresholdValue
{
  public:
    NsmThresholdValueHardShutdownLow(
        const std::string& name, const std::string& type,
        std::shared_ptr<ThresholdHardShutdownIntf> intf);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    std::shared_ptr<ThresholdHardShutdownIntf> intf;
};

class NsmThresholdValueHardShutdownHigh : public NsmThresholdValue
{
  public:
    NsmThresholdValueHardShutdownHigh(
        const std::string& name, const std::string& type,
        std::shared_ptr<ThresholdHardShutdownIntf> intf);
    void updateReading(double value, uint64_t timestamp = 0) override;

  private:
    std::shared_ptr<ThresholdHardShutdownIntf> intf;
};

} // namespace nsm
