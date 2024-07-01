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

#include <com/nvidia/GPMMetrics/server.hpp>
#include <com/nvidia/NVLink/NVLinkMetrics/server.hpp>
#include <nsmSensorAggregator.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/server.hpp>

namespace nsm
{
enum class GPMMetricsUnit : uint8_t
{
    PERCENTAGE,
    BANDWIDTH
};

using GPMMetricsIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::GPMMetrics>;
using NVLinkMetricsIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::NVLink::server::NVLinkMetrics>;
using DimmIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm>;

using DecodeMetricFunc = std::pair<uint8_t, double> (*)(const uint8_t*, size_t);

class MetricUpdator
{
  public:
    virtual ~MetricUpdator() = default;

    virtual void updateMetric(const std::string& name, const double val) = 0;
};

class MetricPerInstanceUpdator
{
  public:
    virtual ~MetricPerInstanceUpdator() = default;
    virtual void updateMetric(const std::vector<double>& metrics) = 0;
};

struct MetricInfo
{
    const char* name;
    DecodeMetricFunc decodeFunc;
    std::unique_ptr<MetricUpdator> updater;
};

struct NVLinkMetricsUpdatorInfo
{
    std::string objPath;
    std::shared_ptr<NVLinkMetricsIntf> interface;
};

std::pair<uint8_t, double> decodePercentage(const uint8_t* sample, size_t size);
std::pair<uint8_t, double> decodeBandwidth(const uint8_t* sample, size_t size);

std::shared_ptr<MetricPerInstanceUpdator>
    makeNVDecPerInstanceUpdator(const std::string& objPath,
                                const std::shared_ptr<GPMMetricsIntf> gpmIntf);

std::shared_ptr<MetricPerInstanceUpdator>
    makeNVJpgPerInstanceUpdator(const std::string& objPath,
                                const std::shared_ptr<GPMMetricsIntf> gpmIntf);

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkRawRxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf);

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkRawTxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf);

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkDataRxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf);

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkDataTxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf);

class DRAMUsageMetricUpdator : public MetricUpdator
{
  public:
    DRAMUsageMetricUpdator(std::shared_ptr<DimmIntf> intf,
                           const std::string& objPath);

    void updateMetric(const std::string& name, const double val) override;

  private:
    std::shared_ptr<DimmIntf> intf;
    const std::string objPath;
    static const std::string dBusIntf;
    static const std::string dBusProperty;
};

class NsmGPMAggregated : public NsmSensorAggregator
{
  public:
    NsmGPMAggregated(const std::string& name, const std::string& type,
                     const std::string& objpath, uint8_t retrievalSource,
                     uint8_t gpuInstance, uint8_t computeIndex,
                     const std::vector<uint8_t>& metricsBitfield,
                     std::shared_ptr<GPMMetricsIntf> gpmIntf,
                     std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    MetricInfo* getMetricInfo(uint8_t metricId)
    {
        return metricId < NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE
                   ? &metricsTable[metricId]
                   : nullptr;
    }

  private:
    int handleSamples(const std::vector<TelemetrySample>& samples) override;

  private:
    const uint8_t retrievalSource;
    const uint8_t gpuInstance;
    const uint8_t computeInstance;
    const std::vector<uint8_t> metricsBitfield;
    const std::string objPath;

    std::shared_ptr<GPMMetricsIntf> gpmIntf{nullptr};
    std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf{nullptr};

    std::array<MetricInfo, NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE>
        metricsTable{};
};

class NsmGPMPerInstance : public NsmSensorAggregator
{
  public:
    NsmGPMPerInstance(const std::string& name, const std::string& type,
                      uint8_t retrievalSource, uint8_t gpuInstance,
                      uint8_t computeInstance, uint8_t metricId,
                      const uint32_t instanceBitfield, GPMMetricsUnit unit,
                      std::shared_ptr<MetricPerInstanceUpdator> metricUpdator);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

  private:
    int handleSamples(const std::vector<TelemetrySample>& samples) override;

  private:
    std::vector<double> metrics;
    const uint8_t retrievalSource;
    const uint8_t gpuInstance;
    const uint8_t computeInstance;
    const uint8_t metricId;
    const uint32_t instanceBitfield;
    const std::string objPath;

    DecodeMetricFunc decodeFunc{};
    const std::shared_ptr<MetricPerInstanceUpdator> metricUpdator;
};
} // namespace nsm
