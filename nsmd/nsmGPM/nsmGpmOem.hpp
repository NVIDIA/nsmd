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
#include <nsmSensor.hpp>
#include <nsmSensorAggregator.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/server.hpp>

namespace nsm
{
enum class GPMMetricsUnit : uint8_t
{
    PERCENTAGE,
    BANDWIDTH
};

constexpr uint8_t GPM_METRIC_TYPE_AGGREGATE = 0;
constexpr uint8_t GPM_METRIC_TYPE_PER_INSTANCE = 1;

using DecodeMetricFunc = std::pair<uint8_t, double> (*)(const uint8_t*, size_t);
std::pair<uint8_t, double> decodePercentage(const uint8_t* sample, size_t size);
std::pair<uint8_t, double> decodeBandwidth(const uint8_t* sample, size_t size);
enum class DataType
{
    Double,
    VectorDouble
};
struct GpmIntfMetricInfo
{
    std::string name;
    DataType dataType;
    DecodeMetricFunc decodeFunc;
};

const static std::string GPMDBusInterfaceName = "com.nvidia.GPMMetrics";

extern const std::unordered_map<uint8_t, std::vector<GpmIntfMetricInfo>>
    GPMIntfMetricsTable;

using GPMMetricsIntf =
    sdbusplus::server::object_t<sdbusplus::com::nvidia::server::GPMMetrics>;
using NVLinkMetricsIntf = sdbusplus::server::object_t<
    sdbusplus::com::nvidia::NVLink::server::NVLinkMetrics>;
using DimmIntf = sdbusplus::server::object_t<
    sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm>;

class MetricUpdator
{
  protected:
    double previousValue = std::numeric_limits<double>::quiet_NaN();

  public:
    virtual ~MetricUpdator() = default;

    virtual void updateMetric(const double val) = 0;
};

class MetricPerInstanceUpdator
{
  protected:
    std::vector<double> previousMetrics;

  public:
    virtual ~MetricPerInstanceUpdator() = default;
    virtual void updateMetric(const std::vector<double>& metrics) = 0;
};

struct MetricInfo
{
    DecodeMetricFunc decodeFunc;
    std::unique_ptr<MetricUpdator> updater;
};

struct NVLinkMetricsUpdatorInfo
{
    std::string objPath;
    std::shared_ptr<NVLinkMetricsIntf> interface;
};

std::shared_ptr<MetricPerInstanceUpdator> makeGPMPerInstanceUpdator(
    const std::string& propertyName, const std::string& objPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf);

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

    void updateMetric(const double val) override;

  private:
    std::shared_ptr<DimmIntf> intf;
    const std::string objPath;
    static const std::string dBusIntf;
    static const std::string dBusProperty;
};

class NsmGPMInterfaceCreator
{
  public:
    NsmGPMInterfaceCreator(sdbusplus::asio::object_server& objServer,
                           const std::string& objpath);

    void createGPMIntf();
    void addGpmIntfProperty(const std::vector<uint8_t>& metricsBitfield);
    void addGpmIntfProperty(const std::string& name, const DataType& dataType);
    std::shared_ptr<sdbusplus::asio::dbus_interface> getGPMIntf()
    {
        return gpmIntf;
    }
    void initialize()
    {
        if (gpmIntf)
        {
            gpmIntf->initialize();
        }
        else
        {
            lg2::error("gpmIntf not created for {OBJ_PATH}", "OBJ_PATH",
                       objpath);
        }
    }

  private:
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf;
    sdbusplus::asio::object_server& objServer;
    const std::string objpath;
};

class NsmGPMAggregated : public NsmSensorAggregator
{
  public:
    NsmGPMAggregated(const std::string& name, const std::string& type,
                     const std::string& objpath, uint8_t retrievalSource,
                     uint8_t gpuInstance, uint8_t computeIndex,
                     const std::vector<uint8_t>& metricsBitfield,
                     std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf,
                     std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    std::vector<MetricInfo*> getMetricInfo(uint8_t metricId)
    {
        std::vector<MetricInfo*> metricInfos;
        for (auto& metric : metricsTable[metricId])
        {
            metricInfos.push_back(&metric);
        }
        return metricInfos;
    }

  private:
    int handleSample(const TelemetrySample& sample) override;

  private:
    const uint8_t retrievalSource;
    const uint8_t gpuInstance;
    const uint8_t computeInstance;
    const std::vector<uint8_t> metricsBitfield;
    const std::string objPath;

    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf{nullptr};
    std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf{nullptr};

    std::array<std::vector<MetricInfo>,
               NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE>
        metricsTable{};
};

class NsmGPMPerInstance : public NsmSensor
{
  public:
    NsmGPMPerInstance(const std::string& name, const std::string& type,
                      uint8_t retrievalSource, uint8_t gpuInstance,
                      uint8_t computeInstance, uint8_t metricId,
                      const std::vector<bitfield8_t> instanceBitfield,
                      GPMMetricsUnit unit,
                      std::shared_ptr<MetricPerInstanceUpdator> metricUpdator);

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;
    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

  private:
    std::vector<double> metrics;
    const uint8_t retrievalSource;
    const uint8_t gpuInstance;
    const uint8_t computeInstance;
    const uint8_t metricId;
    const std::vector<bitfield8_t> instanceBitfield;
    const std::string objPath;

    DecodeMetricFunc decodeFunc{};
    const std::shared_ptr<MetricPerInstanceUpdator> metricUpdator;
};

class NsmDevice;

/**
 * @brief Static sensor class to query supported GPM metrics and create
 *        aggregated sensors based on device capabilities.
 *
 * This sensor queries the device for supported GPM metrics and the maximum
 * number of metrics per command. Upon successful response, it splits the
 * configured metrics bitfield into chunks (respecting maxMetricsPerCommand)
 * and creates NsmGPMAggregated sensors for each chunk.
 *
 * The sensor is intended to be added as a static sensor, meaning it is
 * polled once during initialization. If the device is not responsive,
 * the static sensor polling mechanism will retry until success.
 */
class NsmGetSupportedGPMMetrics : public NsmSensor
{
  public:
    /**
     * @brief Constructor
     *
     * @param name Sensor name
     * @param type Sensor type
     * @param metricType Metric type: 0 = aggregate, 1 = individual
     * @param nsmDevice Device to add created sensors to
     * @param objPath D-Bus object path for GPM interface
     * @param retrievalSource Retrieval source for GPM queries
     * @param gpuInstance GPU instance ID
     * @param computeInstance Compute instance ID
     * @param configuredMetricsBitfield Bitfield of metrics to query (from
     * config)
     * @param gpmIntf GPM D-Bus interface for metric updates
     * @param nvlinkMetricsIntf NVLink metrics interface
     * @param dimmIntf Optional DIMM interface for DRAM usage metrics (nullptr
     * if not used)
     * @param memoryInventoryObjPath Object path for memory inventory (empty if
     * not used)
     */
    NsmGetSupportedGPMMetrics(
        const std::string& name, const std::string& type, uint8_t metricType,
        std::shared_ptr<NsmDevice> nsmDevice, const std::string& objPath,
        uint8_t retrievalSource, uint8_t gpuInstance, uint8_t computeInstance,
        const std::vector<uint8_t>& configuredMetricsBitfield,
        std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf,
        std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf,
        std::shared_ptr<DimmIntf> dimmIntf = nullptr,
        const std::string& memoryInventoryObjPath = "");

    NsmGetSupportedGPMMetrics() = delete;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    uint16_t getMaxMetricsPerCommand() const
    {
        return maxMetricsPerCommand;
    }

    const std::vector<uint8_t>& getSupportedMetricsBitmask() const
    {
        return supportedMetricsBitmask;
    }

    bool hasValidResponse() const
    {
        return responseReceived;
    }

  private:
    /**
     * @brief Split metrics bitfield into chunks based on maxMetricsPerCommand
     *
     * @param metricsBitfield The full metrics bitfield to split
     * @return Vector of bitfield chunks, each with <= maxMetricsPerCommand
     *         enabled bits
     */
    std::vector<std::vector<uint8_t>>
        splitMetricsBitfield(const std::vector<uint8_t>& metricsBitfield) const;

    uint8_t metricType;
    uint16_t maxMetricsPerCommand = 0;
    uint16_t maskSize = 0;
    std::vector<uint8_t> supportedMetricsBitmask;
    bool responseReceived = false;

    // Factory context for creating aggregated sensors
    std::shared_ptr<NsmDevice> nsmDevice;
    std::string objPath;
    uint8_t retrievalSource;
    uint8_t gpuInstance;
    uint8_t computeInstance;
    std::vector<uint8_t> configuredMetricsBitfield;
    std::shared_ptr<sdbusplus::asio::dbus_interface> gpmIntf;
    std::shared_ptr<NVLinkMetricsIntf> nvlinkMetricsIntf;

    // DRAM usage configuration (optional)
    std::shared_ptr<DimmIntf> dimmIntf;
    std::string memoryInventoryObjPath;
};

/**
 * @brief Static sensor class to query supported per-instance GPM metrics and
 *        create per-instance sensors based on device capabilities.
 *
 * This sensor queries the device for supported per-instance GPM metrics and the
 * maximum number of metrics per command. Upon successful response, it splits
 * the configured instance bitfield into chunks (respecting
 * maxMetricsPerCommand) and creates NsmGPMPerInstance sensors for each chunk.
 *
 * This class mirrors NsmGetSupportedGPMMetrics but is specialized for
 * per-instance metrics (metric type 1) such as NVDEC, NVJPG, NVENC, and NVLink.
 */
class NsmGetSupportedPerInstanceGPMMetrics : public NsmSensor
{
  public:
    /**
     * @brief Constructor
     *
     * @param name Sensor name
     * @param type Sensor type
     * @param nsmDevice Device to add created sensors to
     * @param retrievalSource Retrieval source for GPM queries
     * @param gpuInstance GPU instance ID
     * @param computeInstance Compute instance ID
     * @param metricId The per-instance metric ID (e.g., NVDEC, NVJPG, etc.)
     * @param instanceBitfield Bitfield of instances to query (from config)
     * @param unit Unit type for the metric (PERCENTAGE or BANDWIDTH)
     * @param metricUpdator Updator for publishing metric values
     */
    NsmGetSupportedPerInstanceGPMMetrics(
        const std::string& name, const std::string& type,
        std::shared_ptr<NsmDevice> nsmDevice, uint8_t retrievalSource,
        uint8_t gpuInstance, uint8_t computeInstance, uint8_t metricId,
        const std::vector<bitfield8_t>& instanceBitfield, GPMMetricsUnit unit,
        std::shared_ptr<MetricPerInstanceUpdator> metricUpdator);

    NsmGetSupportedPerInstanceGPMMetrics() = delete;

    std::optional<std::vector<uint8_t>>
        genRequestMsg(eid_t eid, uint8_t instanceId) override;

    uint8_t handleResponseMsg(const struct nsm_msg* responseMsg,
                              size_t responseLen) override;

    uint16_t getMaxMetricsPerCommand() const
    {
        return maxMetricsPerCommand;
    }

    const std::vector<uint8_t>& getSupportedMetricsBitmask() const
    {
        return supportedMetricsBitmask;
    }

    bool hasValidResponse() const
    {
        return responseReceived;
    }

  private:
    /**
     * @brief Split instance bitfield into chunks based on maxMetricsPerCommand
     *
     * @param instBitfield The full instance bitfield to split
     * @return Vector of bitfield chunks, each with <= maxMetricsPerCommand
     *         enabled bits
     */
    std::vector<std::vector<bitfield8_t>> splitInstanceBitfield(
        const std::vector<bitfield8_t>& instBitfield) const;

    uint16_t maxMetricsPerCommand = 0;
    uint16_t maskSize = 0;
    std::vector<uint8_t> supportedMetricsBitmask;
    bool responseReceived = false;

    // Factory context for creating per-instance sensors
    std::shared_ptr<NsmDevice> nsmDevice;
    uint8_t retrievalSource;
    uint8_t gpuInstance;
    uint8_t computeInstance;
    uint8_t metricId;
    std::vector<bitfield8_t> instanceBitfield;
    GPMMetricsUnit unit;
    std::shared_ptr<MetricPerInstanceUpdator> metricUpdator;
};

} // namespace nsm
