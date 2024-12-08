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

#include "diagnostics.h"

#include <com/nvidia/ResetCounters/ResetCounterMetrics/server.hpp>
#include <nsmSensorAggregator.hpp>
#include <xyz/openbmc_project/Association/Definitions/server.hpp>

#include <memory>
#include <string>
#include <unordered_map>

namespace nsm
{

using ResetCountersIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::ResetCounters::server::ResetCounterMetrics>;
using AssociationDefinitionsIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Association::server::Definitions>;
/** @class ResetStatisticsAggregator
 *
 * Derived from NsmSensorAggregator to handle reset statistics data in aggregate
 * response format. Updates relevant D-Bus properties with reset counters data.
 */
class ResetStatisticsAggregator : public NsmSensorAggregator
{
  public:
    ResetStatisticsAggregator(
        const std::string& name, const std::string& type,
        const std::string& inventoryObjPath,
        std::shared_ptr<ResetCountersIntf> resetCountersIntf,
        std::unique_ptr<AssociationDefinitionsIntf> associationDef);

    ~ResetStatisticsAggregator() = default;

    /** @brief Generates a request message for reset statistics */
    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    void updateMetricOnSharedMemory() override;

  private:
    /** @brief Handles telemetry samples in the response message */
    int handleSamples(const std::vector<TelemetrySample>& samples) override;

    /** @brief Updates a D-Bus property with the provided value */
    void updateProperty(const std::string& property, uint64_t value);

    // Mapping of tags to property names
    static const std::unordered_map<uint8_t, std::string> tagToPropertyMap;

  private:
    const std::string inventoryObjPath;
    std::shared_ptr<ResetCountersIntf> resetCountersIntf{nullptr};
    std::unique_ptr<AssociationDefinitionsIntf> associationDef = nullptr;
};

} // namespace nsm
