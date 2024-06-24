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

#include "nsmGpmOem.hpp"

#include "platform-environmental.h"

#include <config.h>

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
using updateGPMMetricFunc = double (GPMMetricsIntf::*)(double);
using updateNVLinkMetricFunc = double (NVLinkMetricsIntf::*)(double);
using updatePerInstanceGPMMetricFunc =
    std::vector<double> (GPMMetricsIntf::*)(std::vector<double>);

class GPMMetricUpdator : public MetricUpdator
{
  public:
    GPMMetricUpdator(GPMMetricsIntf* intf, updateGPMMetricFunc updateFunc,
                     const std::string& objPath) :
        intf(intf),
        updateFunc(updateFunc), objPath(objPath){};

    void updateMetric([[maybe_unused]] const std::string& name,
                      const double val) override
    {
        (intf->*updateFunc)(val);

#ifdef NVIDIA_SHMEM
        auto timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        DbusVariantType valueVariant{val};

        nv::shmem::AggregationService::updateTelemetry(
            objPath, DBusIntf, name, valueVariant, timestamp, 0);
#endif
    }

  private:
    GPMMetricsIntf* const intf;
    const updateGPMMetricFunc updateFunc;
    const std::string& objPath;
    static const std::string DBusIntf;
};

const std::string GPMMetricUpdator::DBusIntf{"com.nvidia.GPMMetrics"};

class NVLinkMetricUpdator : public MetricUpdator
{
  public:
    NVLinkMetricUpdator(NVLinkMetricsIntf* intf,
                        updateNVLinkMetricFunc updateFunc,
                        const std::string& objPath) :
        intf(intf),
        updateFunc(updateFunc), objPath(objPath){};

    void updateMetric([[maybe_unused]] const std::string& name,
                      const double val) override
    {
        (intf->*updateFunc)(val);

#ifdef NVIDIA_SHMEM
        auto timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        DbusVariantType valueVariant{val};

        nv::shmem::AggregationService::updateTelemetry(
            objPath, DBusIntf, name, valueVariant, timestamp, 0);
#endif
    }

  private:
    NVLinkMetricsIntf* const intf;
    const updateNVLinkMetricFunc updateFunc;
    const std::string& objPath;
    static const std::string DBusIntf;
};

const std::string NVLinkMetricUpdator::DBusIntf{
    "com.nvidia.NVLink.NVLinkMetrics"};

std::pair<uint8_t, double> decodePercentage(const uint8_t* sample, size_t size)
{
    double percentage{};
    auto rc = decode_aggregate_gpm_metric_percentage_data(sample, size,
                                                          &percentage);

    return {rc, percentage};
}

std::pair<uint8_t, double> decodeBandwidth(const uint8_t* sample, size_t size)
{
    uint64_t bandwidth{};
    auto rc = decode_aggregate_gpm_metric_bandwidth_data(sample, size,
                                                         &bandwidth);

    // unit of bandwidth is Bytes per seconds in NSM Command Response and unit
    // for GPMMetrics PDI is Gbps. Hence it is converted to Gbps.
    static constexpr uint64_t conversionFactor = 1024 * 1024 * 128;
    double bandwidthGbps = bandwidth / static_cast<double>(conversionFactor);

    return {rc, bandwidthGbps};
}

class GPMMetricInstanceUpdator : public MetricPerInstanceUpdator
{
  public:
    GPMMetricInstanceUpdator(const std::string& name,
                             const std::string& objPath,
                             const std::shared_ptr<GPMMetricsIntf>& gpmIntf,
                             updatePerInstanceGPMMetricFunc updateFunc) :
        name(name),
        objPath(objPath), gpmIntf(gpmIntf), updateFunc(updateFunc)
    {}
    void updateMetric(const std::vector<double>& metrics) override
    {
        (*gpmIntf.*updateFunc)(metrics);

#ifdef NVIDIA_SHMEM
        auto timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        DbusVariantType valueVariant{metrics};

        nv::shmem::AggregationService::updateTelemetry(
            objPath, DBusIntf, name, valueVariant, timestamp, 0);
#endif
    }

  private:
    const std::string name;
    const std::string objPath;
    const std::shared_ptr<GPMMetricsIntf> gpmIntf;
    updatePerInstanceGPMMetricFunc updateFunc;
    const static std::string DBusIntf;
};

const std::string GPMMetricInstanceUpdator::DBusIntf{"com.nvidia.GPMMetrics"};

class PortMetricPerInstanceUpdator : public MetricPerInstanceUpdator
{
  public:
    PortMetricPerInstanceUpdator(
        const std::string& name,
        const std::vector<NVLinkMetricsUpdatorInfo>& updatorInfos,
        const updateNVLinkMetricFunc updateFunc) :
        name(name), updatorInfos(updatorInfos), updateFunc(updateFunc)
    {}

    void updateMetric(const std::vector<double>& metrics) override
    {
        const size_t length = std::min(metrics.size(), updatorInfos.size());
        for (size_t i{}; i < length; ++i)
        {
            (updatorInfos[i].interface.get()->*updateFunc)(metrics[i]);

#ifdef NVIDIA_SHMEM
            auto timestamp = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());

            DbusVariantType valueVariant{metrics[i]};

            nv::shmem::AggregationService::updateTelemetry(
                updatorInfos[i].objPath, DBusIntf, name, valueVariant,
                timestamp, 0);
#endif
        }
    }

  private:
    const std::string name;
    const std::vector<NVLinkMetricsUpdatorInfo> updatorInfos;
    const updateNVLinkMetricFunc updateFunc;
    static const std::string DBusIntf;
};

const std::string PortMetricPerInstanceUpdator::DBusIntf{
    "com.nvidia.NVLink.NVLinkMetrics"};

std::shared_ptr<MetricPerInstanceUpdator>
    makeNVDecPerInstanceUpdator(const std::string& objPath,
                                const std::shared_ptr<GPMMetricsIntf> gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new GPMMetricInstanceUpdator{
            "NVDecInstanceUtilizationPercent", objPath, gpmIntf,
            &GPMMetricsIntf::nvDecInstanceUtilizationPercent}};
}

std::shared_ptr<MetricPerInstanceUpdator>
    makeNVJpgPerInstanceUpdator(const std::string& objPath,
                                const std::shared_ptr<GPMMetricsIntf> gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new GPMMetricInstanceUpdator{
            "NVJpgInstanceUtilizationPercent", objPath, gpmIntf,
            &GPMMetricsIntf::nvJpgInstanceUtilizationPercent}};
}

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkRawRxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new PortMetricPerInstanceUpdator{
            "NVLinkRawRxBandwidthGbps", gpmIntf,
            &NVLinkMetricsIntf::nvLinkRawRxBandwidthGbps}};
}

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkRawTxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new PortMetricPerInstanceUpdator{
            "NVLinkRawTxBandwidthGbps", gpmIntf,
            &NVLinkMetricsIntf::nvLinkRawTxBandwidthGbps}};
}

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkDataRxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new PortMetricPerInstanceUpdator{
            "NVLinkDataRxBandwidthGbps", gpmIntf,
            &NVLinkMetricsIntf::nvLinkDataRxBandwidthGbps}};
}

std::shared_ptr<MetricPerInstanceUpdator> makeNVLinkDataTxPerInstanceUpdator(
    const std::vector<NVLinkMetricsUpdatorInfo>& gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new PortMetricPerInstanceUpdator{
            "NVLinkDataTxBandwidthGbps", gpmIntf,
            &NVLinkMetricsIntf::nvLinkDataTxBandwidthGbps}};
}

NsmGPMAggregated::NsmGPMAggregated(
    const std::string& name, const std::string& type,
    const std::string& objpath, uint8_t retrievalSource, uint8_t gpuInstance,
    uint8_t computeInstance, const std::vector<uint8_t>& metricsBitfield,
    std::shared_ptr<GPMMetricsIntf> gpmIntf,
    std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf) :
    NsmSensorAggregator(name, type),
    retrievalSource(retrievalSource), gpuInstance(gpuInstance),
    computeInstance(computeInstance), metricsBitfield(metricsBitfield),
    objPath(objpath), gpmIntf(gpmIntf), nvlinkMetricsIntf(nvlinkMetricsIntf)
{
    metricsTable[0] = {
        "GraphicsEngineActivityPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::graphicsEngineActivityPercent,
            objPath}}};

    metricsTable[1] = {
        "SMActivityPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::smActivityPercent, objPath}}};

    metricsTable[2] = {
        "SMOccupancyPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::smOccupancyPercent, objPath}}};

    metricsTable[3] = {
        "TensorCoreActivityPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::tensorCoreActivityPercent,
            objPath}}};

    // Metric DRAM_Usage is not published

    metricsTable[5] = {
        "FP64ActivityPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::fP64ActivityPercent, objPath}}};

    metricsTable[6] = {
        "FP32ActivityPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::fP32ActivityPercent, objPath}}};

    metricsTable[7] = {
        "FP16ActivityPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::fP16ActivityPercent, objPath}}};

    metricsTable[8] = {
        "PCIeRawRxBandwidthGbps", decodeBandwidth,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::pcIeRawRxBandwidthGbps, objPath}}};

    metricsTable[9] = {
        "PCIeRawTxBandwidthGbps", decodeBandwidth,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::pcIeRawTxBandwidthGbps, objPath}}};

    metricsTable[10] = {
        "NVLinkRawTxBandwidthGbps", decodeBandwidth,
        std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
            nvlinkMetricsIntf.get(),
            &NVLinkMetricsIntf::nvLinkRawTxBandwidthGbps, objPath}}};

    metricsTable[11] = {
        "NVLinkDataTxBandwidthGbps", decodeBandwidth,
        std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
            nvlinkMetricsIntf.get(),
            &NVLinkMetricsIntf::nvLinkDataTxBandwidthGbps, objPath}}};

    metricsTable[12] = {
        "NVLinkRawRxBandwidthGbps", decodeBandwidth,
        std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
            nvlinkMetricsIntf.get(),
            &NVLinkMetricsIntf::nvLinkRawRxBandwidthGbps, objPath}}};

    metricsTable[13] = {
        "NVLinkDataRxBandwidthGbps", decodeBandwidth,
        std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
            nvlinkMetricsIntf.get(),
            &NVLinkMetricsIntf::nvLinkDataRxBandwidthGbps, objPath}}};

    metricsTable[14] = {
        "NVDecUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::nvDecUtilizationPercent, objPath}}};

    metricsTable[15] = {
        "NVJpgUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::nvJpgUtilizationPercent, objPath}}};

    metricsTable[16] = {
        "NVOfaUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::nvOfaUtilizationPercent, objPath}}};

    metricsTable[17] = {
        "IntegerActivityUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::integerActivityUtilizationPercent,
            objPath}}};

    metricsTable[18] = {
        "DMMAUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::dmmaUtilizationPercent, objPath}}};

    metricsTable[19] = {
        "HMMAUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::hmmaUtilizationPercent, objPath}}};

    metricsTable[20] = {
        "IMMAUtilizationPercent", decodePercentage,
        std::unique_ptr<MetricUpdator>{new GPMMetricUpdator{
            gpmIntf.get(), &GPMMetricsIntf::immaUtilizationPercent, objPath}}};
}

std::optional<std::vector<uint8_t>>
    NsmGPMAggregated::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_query_aggregate_gpm_metrics_req) - 1 +
                   metricsBitfield.size());
    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_query_aggregate_gpm_metrics_req(
        instanceId, retrievalSource, gpuInstance, computeInstance,
        metricsBitfield.data(), metricsBitfield.size(), requestPtr);

    if (rc)
    {
        lg2::error("encode_query_aggregate_gpm_metrics_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmGPMAggregated::handleSamples(const std::vector<TelemetrySample>& samples)
{
    uint8_t returnValue = NSM_SW_SUCCESS;

    for (const auto& sample : samples)
    {
        if (sample.tag > metricsTable.size())
        {
            // Currently there is not need to handle special tag like timestamp
            // and uuid
            continue;
        }

        const auto& metric = metricsTable[sample.tag];
        if (!metric.decodeFunc || !metric.updater)
        {
            continue;
        }

        auto [rc, val] = metric.decodeFunc(sample.data, sample.data_len);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode GPM Aggregate Metric {NAME}. Object Path = {OBJPATH}, rc = {RC}.",
                "NAME", metric.name, "OBJPATH", objPath, "RC", rc);
            returnValue = rc;
        }

        metric.updater->updateMetric(metric.name, val);
    }

    return returnValue;
}

NsmGPMPerInstance::NsmGPMPerInstance(
    const std::string& name, const std::string& type, uint8_t retrievalSource,
    uint8_t gpuInstance, uint8_t computeInstance, uint8_t metricId,
    const uint32_t instanceBitfield, GPMMetricsUnit unit,
    std::shared_ptr<MetricPerInstanceUpdator> metricUpdator) :
    NsmSensorAggregator(name, type),
    retrievalSource(retrievalSource), gpuInstance(gpuInstance),
    computeInstance(computeInstance), metricId(metricId),
    instanceBitfield(instanceBitfield), metricUpdator(metricUpdator)
{
    metrics.reserve(32);
    switch (unit)
    {
        case GPMMetricsUnit::PERCENTAGE:
            decodeFunc = decodePercentage;
            break;

        case GPMMetricsUnit::BANDWIDTH:
            decodeFunc = decodeBandwidth;
            break;
    };
}

std::optional<std::vector<uint8_t>>
    NsmGPMPerInstance::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request;

    request.resize(sizeof(nsm_msg_hdr) +
                   sizeof(nsm_query_per_instance_gpm_metrics_req));

    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_query_per_instance_gpm_metrics_req(
        instanceId, retrievalSource, gpuInstance, computeInstance, metricId,
        instanceBitfield, requestPtr);

    if (rc)
    {
        lg2::error("encode_query_aggregate_gpm_metrics_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmGPMPerInstance::handleSamples(
    const std::vector<TelemetrySample>& samples)
{
    uint8_t returnValue = NSM_SW_SUCCESS;

    metrics.clear();

    for (const auto& sample : samples)
    {
        metrics.resize(
            std::max(metrics.size(), static_cast<size_t>(sample.tag) + 1), 0);
        auto [rc, val] = decodeFunc(sample.data, sample.data_len);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "Failed to decode GPM Per-instance Metric for Instance ID {INSTANCE_ID}. Object Path = {OBJPATH}, rc = {RC}.",
                "INSTANCE_ID", sample.tag, "OBJPATH", objPath, "RC", rc);
            returnValue = rc;

            continue;
        }

        metrics[sample.tag] = val;
    }

    metricUpdator->updateMetric(metrics);

    return returnValue;
}
} // namespace nsm
