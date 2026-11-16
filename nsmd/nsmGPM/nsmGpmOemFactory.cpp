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

#include "coroutine.hpp"
#include "dBusAsyncUtils.hpp"
#include "globals.hpp"
#include "interfaceWrapper.hpp"
#include "nsmGPMFeatures.hpp"
#include "nsmGpmOem.hpp"
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
enum class GPMMetricId : uint8_t
{
    DRAMUsage = 4,
    NVLinkRawTxBandwidth = 10,
    NVLinkDataTxBandwidth = 11,
    NVLinkRawRxBandwidth = 12,
    NVLinkDataRxBandwidth = 13
};

std::vector<uint8_t> convertToBytes(const std::vector<uint64_t>& data)
{
    std::vector<uint8_t> result(data.size());

    for (size_t i{}; i < data.size(); ++i)
    {
        result[i] = data[i];
    }

    return result;
}

int addNsmPerInstanceGPMMetric(
    std::shared_ptr<NsmGPMInterfaceCreator> gpmInterfaceCreator,
    std::shared_ptr<NsmDevice> nsmDevice, const std::string& inventoryObjPath,
    const std::string& name, const std::string& type, uint8_t retrievalSource,
    uint8_t gpuInstance, uint8_t computeInstance, const std::string& metric,
    uint8_t metricId, std::optional<uint64_t> v1BitmaskOpt,
    const std::optional<std::vector<uint64_t>>& v2InstanceBitfield,
    const std::string& uuid, const std::string& interface)
{
    std::shared_ptr<MetricPerInstanceUpdator> metricUpdator{};
    GPMMetricsUnit metricUnit{};

    if (metric == "NVDEC")
    {
        std::string propertyName = "NVDecInstanceUtilizationPercent";
        gpmInterfaceCreator->addGpmIntfProperty(propertyName,
                                                DataType::VectorDouble);
        metricUpdator = makeGPMPerInstanceUpdator(
            propertyName, inventoryObjPath, gpmInterfaceCreator->getGPMIntf());
        metricUnit = GPMMetricsUnit::PERCENTAGE;
    }
    else if (metric == "NVJPG")
    {
        std::string propertyName = "NVJpgInstanceUtilizationPercent";
        gpmInterfaceCreator->addGpmIntfProperty(propertyName,
                                                DataType::VectorDouble);
        metricUpdator = makeGPMPerInstanceUpdator(
            propertyName, inventoryObjPath, gpmInterfaceCreator->getGPMIntf());
        metricUnit = GPMMetricsUnit::PERCENTAGE;
    }
    else if (metric == "NVENC")
    {
        std::string propertyName = "NVEncInstanceUtilizationPercent";
        gpmInterfaceCreator->addGpmIntfProperty(propertyName,
                                                DataType::VectorDouble);
        metricUpdator = makeGPMPerInstanceUpdator(
            propertyName, inventoryObjPath, gpmInterfaceCreator->getGPMIntf());
        metricUnit = GPMMetricsUnit::PERCENTAGE;
    }
    else
    {
        lg2::error(
            "Failed to create NSM GPM PerInstance Metrics. Unsupported GPM PerInstance Metic {METRIC}. Config={INTF}",
            "METRIC", metric, "INTF", interface);
        return NSM_ERROR;
    }

    if (v1BitmaskOpt.has_value())
    {
        const uint64_t rawBitmask = v1BitmaskOpt.value();
        if ((rawBitmask >> 32) != 0U)
        {
            lg2::warning(
                "InstanceBitfield has bits above 31 set; truncating for V1. "
                "Name={NAME}, Metric={METRIC}, raw={RAW}",
                "NAME", name, "METRIC", metric, "RAW", rawBitmask);
        }

        auto v1Sensor = std::make_shared<NsmGPMPerInstanceV1>(
            name, type, retrievalSource, gpuInstance, computeInstance, metricId,
            static_cast<uint32_t>(rawBitmask), metricUnit, metricUpdator);

        lg2::info("Created NSM GPM PerInstance V1 sensor: "
                  "UUID={UUID}, Name={NAME}, Type={TYPE}, Metric={METRIC}",
                  "UUID", uuid, "NAME", name, "TYPE", type, "METRIC", metric);

        nsmDevice->addSensor(v1Sensor, PollingType::GpuPerformanceMonitoring);
        return NSM_SUCCESS;
    }

    // V2 path: InstanceBitfield is required for discovery-based per-instance.
    const std::vector<uint64_t> instanceBitfield = v2InstanceBitfield.value();
    std::vector<bitfield8_t> instanceBitfieldBytes(instanceBitfield.size());
    for (size_t i = 0; i < instanceBitfield.size(); i++)
    {
        instanceBitfieldBytes[i].byte =
            static_cast<uint8_t>(instanceBitfield[i]);
    }

    auto staticSensor = std::make_shared<NsmGetSupportedPerInstanceGPMMetrics>(
        name, type, nsmDevice, retrievalSource, gpuInstance, computeInstance,
        metricId, instanceBitfieldBytes, metricUnit, metricUpdator);

    lg2::info("Created NSM GetSupportedPerInstanceGPMMetrics static sensor: "
              "UUID={UUID}, Name={NAME}, Type={TYPE}, Metric={METRIC}",
              "UUID", uuid, "NAME", name, "TYPE", type, "METRIC", metric);

    nsmDevice->addStaticSensor(staticSensor);
    return NSM_SUCCESS;
}

} // namespace nsm

namespace nsm::features
{

struct PerInstanceGPMMetric
{
    const char* metric;
    uint8_t metricId;
};

static const std::vector<PerInstanceGPMMetric> perInstanceGPMMetrics{
    {"NVDEC", 14},
    {"NVJPG", 15},
    {"NVENC", 21},
};

requester::Coroutine createGPMMetricsFeature(SensorManager& manager,
                                             const DeviceContext& dev,
                                             const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("GPMMetrics feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    auto& bus = utils::DBusHandler::getBus();

    std::string name = utils::makeDBusNameValid(
        param<std::string>(params, "Name").value_or(""));
    const std::string type = "NSM_GPMMetrics";
    const std::string& uuid = dev.staticUuid;
    const uint8_t retrievalSource =
        param<uint64_t>(params, "AggregatedRetrievalSource").value_or(0);
    const uint8_t gpuInstance =
        param<uint64_t>(params, "AggregatedGpuInstance").value_or(0);
    const uint8_t computeInstance =
        param<uint64_t>(params, "AggregatedComputeInstance").value_or(0);
    const std::vector<uint8_t> metricsBitfield = convertToBytes(
        param<std::vector<uint64_t>>(params, "AggregatedMetricsBitfield")
            .value_or(std::vector<uint64_t>{}));
    std::string inventoryObjPath = utils::makeDBusNameValid(
        (acceleratorInventoryBasePath / deviceBaseName(dev)).string());

    bool populateMemoryBandwidth =
        param<bool>(params, "AggregatedMemoryBandwidth").value_or(false);

    auto nsmDevice = dev.device;

    auto nvlinkMetricsIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, inventoryObjPath.c_str());

    auto gpmInterfaceCreator = std::make_shared<NsmGPMInterfaceCreator>(
        manager.getObjServer(), inventoryObjPath);
    gpmInterfaceCreator->addGpmIntfProperty(metricsBitfield);

    std::shared_ptr<DimmIntf> dimmIntf = nullptr;
    std::string memoryInventoryObjPath;
    try
    {
        if (populateMemoryBandwidth)
        {
            memoryInventoryObjPath = utils::makeDBusNameValid(
                (dimmInventoryBasePath / (deviceBaseName(dev) + "_DRAM_0"))
                    .string());

            auto sensorObjectPath = memoryInventoryObjPath +
                                    "/xyz.openbmc_project.Inventory.Item.Dimm";

            dimmIntf = getInterfaceOnObjectPath<DimmIntf>(
                sensorObjectPath, manager, bus, memoryInventoryObjPath.c_str());
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Caught exception while creating DimmIntf for populateMemoryBandwidth. "
            "Error: {ERROR}",
            "ERROR", e.what());
    }

    auto getSupportedMetricsSensor =
        std::make_shared<NsmGetSupportedGPMMetrics>(
            name, type, GPM_METRIC_TYPE_AGGREGATE, nsmDevice, inventoryObjPath,
            retrievalSource, gpuInstance, computeInstance, metricsBitfield,
            gpmInterfaceCreator->getGPMIntf(), nvlinkMetricsIntf, dimmIntf,
            memoryInventoryObjPath);

    lg2::info("Created NSM GetSupportedGPMMetrics static sensor: UUID={UUID}, "
              "Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", name, "TYPE", type);

    nsmDevice->addStaticSensor(getSupportedMetricsSensor);

    for (const auto& [metric, metricId] : perInstanceGPMMetrics)
    {
        const std::string prefix = std::string(metric) + "PerInstance";
        if (!param<bool>(params, (prefix + "Enabled").c_str()).value_or(false))
        {
            continue;
        }

        std::string instanceName = utils::makeDBusNameValid(
            param<std::string>(params, (prefix + "Name").c_str()).value_or(""));
        const std::string instanceType = "NSM_GPMPerInstanceMetrics";
        const uint8_t instanceRetrievalSource =
            param<uint64_t>(params, (prefix + "RetrievalSource").c_str())
                .value_or(0);
        const uint8_t instanceGpuInstance =
            param<uint64_t>(params, (prefix + "GpuInstance").c_str())
                .value_or(0);
        const uint8_t instanceComputeInstance =
            param<uint64_t>(params, (prefix + "ComputeInstance").c_str())
                .value_or(0);

        std::optional<uint64_t> v1Bitmask;
        std::optional<std::vector<uint64_t>> v2InstanceBitfield;
        auto bitfieldIt = params.find((prefix + "Bitfield"));
        if (bitfieldIt != params.end())
        {
            if (std::holds_alternative<uint64_t>(bitfieldIt->second))
            {
                v1Bitmask = std::get<uint64_t>(bitfieldIt->second);
            }
            else
            {
                v2InstanceBitfield =
                    std::get<std::vector<uint64_t>>(bitfieldIt->second);
            }
        }

        addNsmPerInstanceGPMMetric(gpmInterfaceCreator, nsmDevice,
                                   inventoryObjPath, instanceName, instanceType,
                                   instanceRetrievalSource, instanceGpuInstance,
                                   instanceComputeInstance, metric, metricId,
                                   v1Bitmask, v2InstanceBitfield, uuid, prefix);
    }

    gpmInterfaceCreator->initialize();
    co_return NSM_SUCCESS;
}

requester::Coroutine
    createGPMPortMetricsFeature([[maybe_unused]] SensorManager& manager,
                                const DeviceContext& dev,
                                const dbus::PropertyMap& params)
{
    if (!dev.device)
    {
        lg2::error("GPMPortMetrics feature of {NAME}: no NsmDevice", "NAME",
                   dev.name);
        co_return NSM_ERROR;
    }

    auto& bus = utils::DBusHandler::getBus();

    std::string name = utils::makeDBusNameValid(
        param<std::string>(params, "Name").value_or(""));
    const std::string type = "NSM_GPMPortMetrics";
    const std::string& uuid = dev.staticUuid;
    const uint8_t retrievalSource =
        param<uint64_t>(params, "RetrievalSource").value_or(0);
    const uint8_t gpuInstance =
        param<uint64_t>(params, "GpuInstance").value_or(0);
    const uint8_t computeInstance =
        param<uint64_t>(params, "ComputeInstance").value_or(0);
    const std::vector<std::string> metrics =
        param<std::vector<std::string>>(params, "Metrics")
            .value_or(std::vector<std::string>{});
    const uint64_t nvlinkCount =
        param<uint64_t>(params, "NVLinkCount").value_or(0);

    std::optional<uint64_t> v1BitmaskOpt;
    auto bitfieldIt = params.find("InstanceBitfield");
    if (bitfieldIt != params.end() &&
        std::holds_alternative<uint64_t>(bitfieldIt->second))
    {
        v1BitmaskOpt = std::get<uint64_t>(bitfieldIt->second);
    }
    const bool useV1 = v1BitmaskOpt.has_value();

    uint32_t instanceBitmaskV1 = 0;
    std::vector<bitfield8_t> instanceBitfieldBytes;
    if (useV1)
    {
        const uint64_t rawBitmask = v1BitmaskOpt.value();
        if ((rawBitmask >> 32) != 0U)
        {
            lg2::warning(
                "InstanceBitfield has bits above 31 set; truncating for V1. "
                "Name={NAME}, raw={RAW}",
                "NAME", name, "RAW", rawBitmask);
        }
        instanceBitmaskV1 = static_cast<uint32_t>(rawBitmask);
    }
    else
    {
        const std::vector<uint64_t> instanceBitfield =
            param<std::vector<uint64_t>>(params, "InstanceBitfield")
                .value_or(std::vector<uint64_t>{});
        instanceBitfieldBytes.resize(instanceBitfield.size());
        for (size_t i = 0; i < instanceBitfield.size(); i++)
        {
            instanceBitfieldBytes[i].byte =
                static_cast<uint8_t>(instanceBitfield[i]);
        }
    }

    std::string inventoryObjPath = utils::makeDBusNameValid(
        param<std::string>(params, "ParentObjPath").value_or(""));

    auto nsmDevice = dev.device;

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos;

    for (uint64_t port = 0; port < nvlinkCount; ++port)
    {
        const std::string objPath = inventoryObjPath + "/Ports/NVLink_" +
                                    std::to_string(port);
        updatorInfos.push_back(NVLinkMetricsUpdatorInfo{
            .objPath = utils::makeDBusNameValid(objPath),
            .interface = std::make_shared<NVLinkMetricsIntf>(bus,
                                                             objPath.c_str())});
    }

    for (const auto& metric : metrics)
    {
        GPMMetricsUnit unit = GPMMetricsUnit::BANDWIDTH;
        uint8_t metricId;
        std::shared_ptr<MetricPerInstanceUpdator> updator;

        if (metric == "NVLinkRawTxBandwidthGbps")
        {
            updator = makeNVLinkRawTxPerInstanceUpdator(updatorInfos);
            metricId = static_cast<uint8_t>(GPMMetricId::NVLinkRawTxBandwidth);
        }
        else if (metric == "NVLinkDataTxBandwidthGbps")
        {
            updator = makeNVLinkDataTxPerInstanceUpdator(updatorInfos);
            metricId = static_cast<uint8_t>(GPMMetricId::NVLinkDataTxBandwidth);
        }
        else if (metric == "NVLinkRawRxBandwidthGbps")
        {
            updator = makeNVLinkRawRxPerInstanceUpdator(updatorInfos);
            metricId = static_cast<uint8_t>(GPMMetricId::NVLinkRawRxBandwidth);
        }
        else if (metric == "NVLinkDataRxBandwidthGbps")
        {
            updator = makeNVLinkDataRxPerInstanceUpdator(updatorInfos);
            metricId = static_cast<uint8_t>(GPMMetricId::NVLinkDataRxBandwidth);
        }
        else
        {
            lg2::error(
                "Failed to create NSM GPM PerPort Metrics. Unsupported GPM Metric {METRIC}. Device={NAME}",
                "METRIC", metric, "NAME", dev.name);
            continue;
        }

        if (useV1)
        {
            auto v1Sensor = std::make_shared<NsmGPMPerInstanceV1>(
                name + "_" + metric, type, retrievalSource, gpuInstance,
                computeInstance, metricId, instanceBitmaskV1, unit,
                std::move(updator));

            lg2::info("Created NSM GPM PerInstance V1 sensor for PerPort: "
                      "Metric={METRIC}, UUID={UUID}, Name={NAME}, Type={TYPE}",
                      "METRIC", metric, "UUID", uuid, "NAME", name, "TYPE",
                      type);

            nsmDevice->addSensor(v1Sensor,
                                 PollingType::GpuPerformanceMonitoring);
            continue;
        }

        auto staticSensor =
            std::make_shared<NsmGetSupportedPerInstanceGPMMetrics>(
                name + "_" + metric, type, nsmDevice, retrievalSource,
                gpuInstance, computeInstance, metricId, instanceBitfieldBytes,
                unit, std::move(updator));

        lg2::info(
            "Created NSM GetSupportedPerInstanceGPMMetrics static sensor for PerPort: "
            "Metric={METRIC}, UUID={UUID}, Name={NAME}, Type={TYPE}",
            "METRIC", metric, "UUID", uuid, "NAME", name, "TYPE", type);

        nsmDevice->addStaticSensor(staticSensor);
    }

    co_return NSM_SUCCESS;
}

} // namespace nsm::features
