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
#ifdef NVIDIA_SHMEM
#include <telemetry_mrd_producer.hpp>
#endif

namespace nsm
{
using updateGPMMetricFunc = double (GPMMetricsIntf::*)(double);
using updateNVLinkMetricFunc = double (NVLinkMetricsIntf::*)(double);
using updatePerInstanceGPMMetricFunc =
    std::vector<double> (GPMMetricsIntf::*)(std::vector<double>);

class GPMMetricUpdator : public MetricUpdator
{
  public:
    GPMMetricUpdator(const std::string& propertyName,
                     const std::string& objPath,
                     std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf) :
        propertyName(propertyName), objPath(objPath), gpmIntf(gpmIntf) {};

    void updateMetric(const double val) override
    {
        if (previousValue != val && gpmIntf)
        {
            gpmIntf->set_property(propertyName, val);
            previousValue = val;
        }

#ifdef NVIDIA_SHMEM
        auto timestamp = utils::getCurrentSteadyClockTimestamp();

        DbusVariantType valueVariant{val};

        nv::shmem::AggregationService::updateTelemetry(
            objPath, GPMDBusInterfaceName, propertyName, valueVariant,
            timestamp, 0);
#endif
    }

  private:
    const std::string propertyName;
    const std::string objPath;
    static const std::string DBusIntf;
    const std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf;
};

const std::string GPMMetricUpdator::DBusIntf{"com.nvidia.GPMMetrics"};

class NVLinkMetricUpdator : public MetricUpdator
{
  public:
    NVLinkMetricUpdator(NVLinkMetricsIntf* intf,
                        updateNVLinkMetricFunc updateFunc,
                        const std::string& objPath, const std::string& name) :
        intf(intf), updateFunc(updateFunc), objPath(objPath), name(name) {};

    void updateMetric(const double val) override
    {
        if (previousValue != val)
        {
            (intf->*updateFunc)(val);
            previousValue = val;
        }

#ifdef NVIDIA_SHMEM
        auto timestamp = utils::getCurrentSteadyClockTimestamp();

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
    const std::string name;
};

const std::string NVLinkMetricUpdator::DBusIntf{
    "com.nvidia.NVLink.NVLinkMetrics"};

DRAMUsageMetricUpdator::DRAMUsageMetricUpdator(std::shared_ptr<DimmIntf> intf,
                                               const std::string& objPath) :
    intf(intf), objPath(objPath)
{}

void DRAMUsageMetricUpdator::updateMetric(const double val)
{
    if (previousValue != val)
    {
        intf->utilization(val);
        previousValue = val;
    }

#ifdef NVIDIA_SHMEM
    auto timestamp = utils::getCurrentSteadyClockTimestamp();

    DbusVariantType valueVariant{val};

    nv::shmem::AggregationService::updateTelemetry(
        objPath, dBusIntf, dBusProperty, valueVariant, timestamp, 0);
#endif
}

const std::string DRAMUsageMetricUpdator::dBusIntf{
    "xyz.openbmc_project.Inventory.Item.Dimm"};

const std::string DRAMUsageMetricUpdator::dBusProperty{"Utilization"};

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

const std::unordered_map<uint8_t, std::vector<GpmIntfMetricInfo>>
    GPMIntfMetricsTable{
        {0,
         {GpmIntfMetricInfo{"GraphicsEngineActivityPercent", DataType::Double,
                            decodePercentage}}},
        {1,
         {GpmIntfMetricInfo{"SMActivityPercent", DataType::Double,
                            decodePercentage}}},
        {2,
         {GpmIntfMetricInfo{"SMOccupancyPercent", DataType::Double,
                            decodePercentage}}},
        {3,
         {GpmIntfMetricInfo{"TensorCoreActivityPercent", DataType::Double,
                            decodePercentage}}},
        {5,
         {GpmIntfMetricInfo{"FP64ActivityPercent", DataType::Double,
                            decodePercentage}}},
        {6,
         {GpmIntfMetricInfo{"FP32ActivityPercent", DataType::Double,
                            decodePercentage}}},
        {7,
         {GpmIntfMetricInfo{"FP16ActivityPercent", DataType::Double,
                            decodePercentage}}},
        {8,
         {GpmIntfMetricInfo{"PCIeRawTxBandwidthGbps", DataType::Double,
                            decodeBandwidth}}},
        {9,
         {GpmIntfMetricInfo{"PCIeRawRxBandwidthGbps", DataType::Double,
                            decodeBandwidth}}},
        {14,
         {GpmIntfMetricInfo{"NVDecUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {15,
         {GpmIntfMetricInfo{"NVJpgUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {16,
         {GpmIntfMetricInfo{"NVOfaUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {17,
         {GpmIntfMetricInfo{"IntegerActivityUtilizationPercent",
                            DataType::Double, decodePercentage}}},
        {18,
         {GpmIntfMetricInfo{"DMMAUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {19,
         {GpmIntfMetricInfo{"HMMAUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {20,
         {GpmIntfMetricInfo{"IMMAUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {21,
         {GpmIntfMetricInfo{"NVEncUtilizationPercent", DataType::Double,
                            decodePercentage}}},
        {22,
         {GpmIntfMetricInfo{"HostMemoryCacheHitPercent", DataType::Double,
                            decodePercentage}}},
        {23,
         {GpmIntfMetricInfo{"HostMemoryCacheMissPercent", DataType::Double,
                            decodePercentage}}},
        {24,
         {GpmIntfMetricInfo{"PeerMemoryCacheHitPercent", DataType::Double,
                            decodePercentage}}},
        {25,
         {GpmIntfMetricInfo{"PeerMemoryCacheMissPercent", DataType::Double,
                            decodePercentage}}},
        {26,
         {GpmIntfMetricInfo{"DRAMMemoryCacheHitPercent", DataType::Double,
                            decodePercentage}}},
        {27,
         {GpmIntfMetricInfo{"DRAMMemoryCacheMissPercent", DataType::Double,
                            decodePercentage}}},
        {28,
         {GpmIntfMetricInfo{"C2CRawTxBandwidthGbps", DataType::Double,
                            decodeBandwidth}}},
        {29,
         {GpmIntfMetricInfo{"C2CRawRxBandwidthGbps", DataType::Double,
                            decodeBandwidth}}},
        {30,
         {GpmIntfMetricInfo{"C2CDataTxBandwidthGbps", DataType::Double,
                            decodeBandwidth}}},
        {31,
         {GpmIntfMetricInfo{"C2CDataRxBandwidthGbps", DataType::Double,
                            decodeBandwidth}}},
    };

class GPMMetricInstanceUpdator : public MetricPerInstanceUpdator
{
  public:
    GPMMetricInstanceUpdator(
        const std::string& propertyName, const std::string& objPath,
        std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf) :
        propertyName(propertyName), objPath(objPath), gpmIntf(gpmIntf)
    {}
    void updateMetric(const std::vector<double>& metrics) override
    {
        if (previousMetrics != metrics && gpmIntf)
        {
            gpmIntf->set_property(propertyName, metrics);
            previousMetrics = metrics;
        }

#ifdef NVIDIA_SHMEM
        auto timestamp = utils::getCurrentSteadyClockTimestamp();

        DbusVariantType valueVariant{metrics};

        nv::shmem::AggregationService::updateTelemetry(
            objPath, GPMDBusInterfaceName, propertyName, valueVariant,
            timestamp, 0);
#endif
    }

  private:
    const std::string propertyName;
    const std::string objPath;
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf;
    static const std::string DBusIntf;
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
        if (previousMetrics.size() != length)
        {
            previousMetrics.resize(length,
                                   std::numeric_limits<double>::quiet_NaN());
        }

        for (size_t i{}; i < length; ++i)
        {
            if (previousMetrics[i] != metrics[i])
            {
                (updatorInfos[i].interface.get()->*updateFunc)(metrics[i]);
                previousMetrics[i] = metrics[i];
            }

#ifdef NVIDIA_SHMEM
            auto timestamp = utils::getCurrentSteadyClockTimestamp();

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

std::shared_ptr<MetricPerInstanceUpdator> makeGPMPerInstanceUpdator(
    const std::string& propertyName, const std::string& objPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf)
{
    return std::shared_ptr<MetricPerInstanceUpdator>{
        new GPMMetricInstanceUpdator{propertyName, objPath, gpmIntf}};
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

NsmGPMInterfaceCreator::NsmGPMInterfaceCreator(
    sdbusplus::asio::object_server& objServer, const std::string& objpath) :
    objServer(objServer), objpath(objpath)
{
    createGPMIntf();
}

void NsmGPMInterfaceCreator::createGPMIntf()
{
    gpmIntf = objServer.add_interface(objpath.c_str(), GPMDBusInterfaceName);
    if (!gpmIntf)
    {
        lg2::error("Failed to create GPM Interface on object path {OBJPATH}",
                   "OBJPATH", objpath);
        return;
    }
}

void NsmGPMInterfaceCreator::addGpmIntfProperty(
    const std::vector<uint8_t>& metricsBitfield)
{
    if (!gpmIntf)
    {
        lg2::error("GPM Interface not created");
        return;
    }
    std::vector<bool> supportedGPMMetrics;
    utils::convertBitfieldToVector(metricsBitfield, supportedGPMMetrics);
    for (size_t i = 0; i < supportedGPMMetrics.size(); i++)
    {
        if (supportedGPMMetrics[i] &&
            GPMIntfMetricsTable.find(i) != GPMIntfMetricsTable.end())
        {
            for (const auto& metric : GPMIntfMetricsTable.at(i))
            {
                switch (metric.dataType)
                {
                    case DataType::Double:
                        gpmIntf->register_property(
                            metric.name,
                            double{std::numeric_limits<double>::quiet_NaN()});
                        break;
                    case DataType::VectorDouble:
                        gpmIntf->register_property(metric.name,
                                                   std::vector<double>{});
                        break;
                }
            }
        }
    }
}

void NsmGPMInterfaceCreator::addGpmIntfProperty(const std::string& name,
                                                const DataType& dataType)
{
    if (!gpmIntf)
    {
        lg2::error("GPM Interface not created");
        return;
    }

    switch (dataType)
    {
        case DataType::Double:
            gpmIntf->register_property(
                name, double{std::numeric_limits<double>::quiet_NaN()});
            break;
        case DataType::VectorDouble:
            gpmIntf->register_property(name, std::vector<double>{});
            break;
    }
}

NsmGPMAggregated::NsmGPMAggregated(
    const std::string& name, const std::string& type,
    const std::string& objpath, uint8_t retrievalSource, uint8_t gpuInstance,
    uint8_t computeInstance, const std::vector<uint8_t>& metricsBitfield,
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf,
    std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf) :
    NsmSensorAggregator(name, type), retrievalSource(retrievalSource),
    gpuInstance(gpuInstance), computeInstance(computeInstance),
    metricsBitfield(metricsBitfield), objPath(objpath), gpmIntf(gpmIntf),
    nvlinkMetricsIntf(nvlinkMetricsIntf)
{
    std::vector<bool> supportedGPMMetrics;
    utils::convertBitfieldToVector(metricsBitfield, supportedGPMMetrics);
    for (size_t i = 0; i < supportedGPMMetrics.size(); i++)
    {
        if (supportedGPMMetrics[i] &&
            GPMIntfMetricsTable.find(i) != GPMIntfMetricsTable.end())
        {
            metricsTable[i].clear();
            for (const auto& metric : GPMIntfMetricsTable.at(i))
            {
                metricsTable[i].emplace_back(MetricInfo{
                    metric.decodeFunc,
                    std::unique_ptr<MetricUpdator>{
                        new GPMMetricUpdator{metric.name, objPath, gpmIntf}}});
            }
        }
    }

    metricsTable[4].clear();
    metricsTable[4].emplace_back(MetricInfo{decodePercentage, {}});

    metricsTable[10].clear();
    metricsTable[10].emplace_back(MetricInfo{
        decodeBandwidth, std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
                             nvlinkMetricsIntf.get(),
                             &NVLinkMetricsIntf::nvLinkRawTxBandwidthGbps,
                             objPath, "NVLinkRawTxBandwidthGbps"}}});
    metricsTable[11].clear();
    metricsTable[11].emplace_back(MetricInfo{
        decodeBandwidth, std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
                             nvlinkMetricsIntf.get(),
                             &NVLinkMetricsIntf::nvLinkDataTxBandwidthGbps,
                             objPath, "NVLinkDataTxBandwidthGbps"}}});
    metricsTable[12].clear();
    metricsTable[12].emplace_back(MetricInfo{
        decodeBandwidth, std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
                             nvlinkMetricsIntf.get(),
                             &NVLinkMetricsIntf::nvLinkRawRxBandwidthGbps,
                             objPath, "NVLinkRawRxBandwidthGbps"}}});
    metricsTable[13].clear();
    metricsTable[13].emplace_back(MetricInfo{
        decodeBandwidth, std::unique_ptr<MetricUpdator>{new NVLinkMetricUpdator{
                             nvlinkMetricsIntf.get(),
                             &NVLinkMetricsIntf::nvLinkDataRxBandwidthGbps,
                             objPath, "NVLinkDataRxBandwidthGbps"}}});
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
        lg2::debug("encode_query_aggregate_gpm_metrics_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

int NsmGPMAggregated::handleSample(const TelemetrySample& sample)
{
    uint8_t returnValue = NSM_SW_SUCCESS;

    if (sample.tag > metricsTable.size())
    {
        // Currently there is not need to handle special tag like timestamp
        // and uuid
        return returnValue;
    }

    for (auto& metric : metricsTable[sample.tag])
    {
        if (!metric.decodeFunc || !metric.updater)
        {
            return returnValue;
        }

        auto [rc, val] = metric.decodeFunc(sample.data, sample.data_len);

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "Failed to decode GPM Aggregate Metric {SAMPLE_TAG}. Object Path = {OBJPATH}, rc = {RC}.",
                "SAMPLE_TAG", sample.tag, "OBJPATH", objPath, "RC", rc);
            returnValue = rc;
        }

        metric.updater->updateMetric(val);
    }
    return returnValue;
}

NsmGPMPerInstance::NsmGPMPerInstance(
    const std::string& name, const std::string& type, uint8_t retrievalSource,
    uint8_t gpuInstance, uint8_t computeInstance, uint8_t metricId,
    const std::vector<bitfield8_t> instanceBitfield, GPMMetricsUnit unit,
    std::shared_ptr<MetricPerInstanceUpdator> metricUpdator) :
    NsmSensor(name, type), retrievalSource(retrievalSource),
    gpuInstance(gpuInstance), computeInstance(computeInstance),
    metricId(metricId), instanceBitfield(instanceBitfield),
    metricUpdator(metricUpdator)
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
                   sizeof(nsm_query_per_instance_gpm_metrics_v2_req) +
                   instanceBitfield.size() - 1);

    auto requestPtr = reinterpret_cast<nsm_msg*>(request.data());

    auto rc = encode_query_per_instance_gpm_metrics_v2_req(
        instanceId, retrievalSource, gpuInstance, computeInstance, metricId,
        instanceBitfield.data(), instanceBitfield.size(), requestPtr);

    if (rc)
    {
        lg2::debug("encode_query_aggregate_gpm_metrics_req failed. "
                   "eid={EID}, rc={RC}.",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmGPMPerInstance::handleResponseMsg(const nsm_msg* responseMsg,
                                             size_t responseLen)
{
    uint8_t returnValue = NSM_SW_SUCCESS;
    uint8_t cc = NSM_SUCCESS;
    size_t consumedLen{};
    auto responseData = reinterpret_cast<const uint8_t*>(responseMsg);

    uint16_t telemetryCount{};

    auto rc = decode_aggregate_resp(responseMsg, responseLen, &consumedLen, &cc,
                                    &telemetryCount);

    if (shouldLog("decode_aggregate_resp", uint16_t(0), cc, rc))
    {
        LG2_ERROR("decode_aggregate_resp | cc: {CC}, rc: {RC}", "CC", cc, "RC",
                  rc);
    }

    if (cc != NSM_SUCCESS || rc != NSM_SW_SUCCESS)
    {
        return cc ? cc : rc;
    }

    if (telemetryCount == 0)
    {
        metricUpdator->updateMetric(std::vector<double>());
        return returnValue;
    }

    metrics.clear();

    responseData += consumedLen;
    responseLen -= consumedLen;

    while (telemetryCount--)
    {
        uint8_t tag;
        bool valid;
        const uint8_t* data;
        size_t dataLen;

        auto sampleData =
            reinterpret_cast<const nsm_aggregate_resp_sample*>(responseData);

        rc = decode_aggregate_resp_sample(sampleData, responseLen, &consumedLen,
                                          &tag, &valid, &data, &dataLen);

        responseData += consumedLen;
        responseLen -= consumedLen;

        if (rc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "responseHandler: decode_aggregate_resp_sample failed. "
                "Type={TYPE}, Tag={TAG}, sensor={NAME}, rc={RC}, valid_bit={VALID}",
                "TYPE", getType(), "TAG", tag, "NAME", getName(), "RC", rc,
                "VALID", valid);
            continue;
        }

        if (tag >= metrics.size())
        {
            metrics.resize(tag + 1);
        }

        auto [decodeRc, val] = decodeFunc(data, dataLen);

        if (decodeRc != NSM_SW_SUCCESS)
        {
            lg2::debug(
                "Failed to decode GPM Per-instance Metric for Instance ID {INSTANCE_ID}. Object Path = {OBJPATH}, rc = {RC}.",
                "INSTANCE_ID", tag, "OBJPATH", objPath, "RC", decodeRc);
            returnValue = decodeRc;
        }
        else
        {
            metrics[tag] = val;
        }
    }

    metricUpdator->updateMetric(metrics);

    return returnValue;
}
} // namespace nsm
