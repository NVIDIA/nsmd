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
#include "interfaceWrapper.hpp"
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

static requester::Coroutine
    getPerInstanceInterfacesAsync(const std::string& interface,
                                  const std::string& objPath,
                                  std::vector<std::string>& interfaces)
{
    const std::string interfaceName = interface + ".PerInstanceMetrics";
    interfaces.clear();

    // Use existing async utility instead of blocking call
    auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                          dbus::Interfaces{});

    for (const auto& [service, intfs] : mapperResponse)
    {
        for (const auto& intf : intfs)
        {
            if (intf.find(interfaceName) != std::string::npos)
            {
                interfaces.push_back(intf);
            }
        }
    }

    co_return NSM_SW_SUCCESS;
}

requester::Coroutine createNsmPerInstanceGPMMetric(
    std::shared_ptr<GPMMetricsIntf> gpmInf,
    std::shared_ptr<NsmDevice> nsmDevice, const std::string& inventoryObjPath,
    const std::string& interface, const std::string& objPath)
{
    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name = utils::getPropertyFromCollection<std::string>(properties,
                                                                     "Name")
                           .value();
    name = utils::makeDBusNameValid(name);
    std::string type = utils::getPropertyFromCollection<std::string>(properties,
                                                                     "Type")
                           .value();
    type = utils::makeDBusNameValid(type);
    const uint8_t retrievalSource = utils::getPropertyFromCollection<uint64_t>(
                                        properties, "RetrievalSource")
                                        .value();
    const uint8_t gpuInstance =
        utils::getPropertyFromCollection<uint64_t>(properties, "GpuInstance")
            .value();
    const uint8_t computeInstance = utils::getPropertyFromCollection<uint64_t>(
                                        properties, "ComputeInstance")
                                        .value();
    const std::string metric =
        utils::getPropertyFromCollection<std::string>(properties, "Metric")
            .value();
    const uint8_t metricId =
        utils::getPropertyFromCollection<uint64_t>(properties, "MetricId")
            .value();
    const uint32_t instanceBitfield =
        utils::getPropertyFromCollection<uint64_t>(properties,
                                                   "InstanceBitfield")
            .value();

    std::shared_ptr<MetricPerInstanceUpdator> metricUpdator{};
    GPMMetricsUnit metricUnit{};

    if (metric == "NVDEC")
    {
        metricUpdator = makeNVDecPerInstanceUpdator(inventoryObjPath, gpmInf);
        metricUnit = GPMMetricsUnit::PERCENTAGE;
    }
    else if (metric == "NVJPG")
    {
        metricUpdator = makeNVJpgPerInstanceUpdator(inventoryObjPath, gpmInf);
        metricUnit = GPMMetricsUnit::PERCENTAGE;
    }
    else
    {
        lg2::error(
            "Failed to create NSM GPM PerInstance Metrics. Unsupported GPM PerInstance Metic {METRIC}. Config={INTF}",
            "METRIC", metric, "INTF", interface);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    auto gpmPerInstanceMetric = std::make_shared<NsmGPMPerInstance>(
        name, type, retrievalSource, gpuInstance, computeInstance, metricId,
        instanceBitfield, metricUnit, metricUpdator);

    lg2::info(
        "Created NSM GPM PerInstance Metrics : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", nsmDevice->uuid, "NAME", name, "TYPE", type);

    nsmDevice->addSensor(gpmPerInstanceMetric,
                         PollingType::GpuPerformanceMonitoring);
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

static requester::Coroutine createNsmGPMMetrics(SensorManager& manager,
                                                const std::string& interface,
                                                const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name = utils::getPropertyFromCollection<std::string>(properties,
                                                                     "Name")
                           .value();
    name = utils::makeDBusNameValid(name);
    const std::string type = interface.substr(interface.find_last_of('.') + 1);
    const std::string uuid =
        utils::getPropertyFromCollection<std::string>(properties, "UUID")
            .value();
    const uint8_t retrievalSource = utils::getPropertyFromCollection<uint64_t>(
                                        properties, "RetrievalSource")
                                        .value();
    const uint8_t gpuInstance =
        utils::getPropertyFromCollection<uint64_t>(properties, "GpuInstance")
            .value();
    const uint8_t computeInstance = utils::getPropertyFromCollection<uint64_t>(
                                        properties, "ComputeInstance")
                                        .value();
    const std::vector<uint8_t> metricsBitfield =
        convertToBytes(utils::getPropertyFromCollection<std::vector<uint64_t>>(
                           properties, "MetricsBitfield")
                           .value());
    std::string inventoryObjPath =
        utils::getPropertyFromCollection<std::string>(properties,
                                                      "InventoryObjPath")
            .value();
    inventoryObjPath = utils::makeDBusNameValid(inventoryObjPath);

    bool populateMemoryBandwidth{false};

    try
    {
        populateMemoryBandwidth = utils::getPropertyFromCollection<bool>(
                                      properties, "MemoryBandwidth")
                                      .value();
    }
    catch (const std::exception& e)
    {}

    auto nsmDevice = manager.getNsmDevice(uuid);

    auto gpmIntf = std::make_shared<GPMMetricsIntf>(bus,
                                                    inventoryObjPath.c_str());
    auto nvlinkMetricsIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, inventoryObjPath.c_str());

    auto gpmAggregateMetrics = std::make_shared<NsmGPMAggregated>(
        name, type, inventoryObjPath, retrievalSource, gpuInstance,
        computeInstance, metricsBitfield, gpmIntf, nvlinkMetricsIntf);

    if (populateMemoryBandwidth)
    {
        std::string memoryInventoryObjPath =
            utils::getPropertyFromCollection<std::string>(
                properties, "MemoryInventoryObjPath")
                .value();
        memoryInventoryObjPath =
            utils::makeDBusNameValid(memoryInventoryObjPath);

        auto sensorObjectPath = memoryInventoryObjPath +
                                "/xyz.openbmc_project.Inventory.Item.Dimm";

        std::shared_ptr<DimmIntf> dimmIntf = getInterfaceOnObjectPath<DimmIntf>(
            sensorObjectPath, manager, bus, memoryInventoryObjPath.c_str());

        auto dramUsage = gpmAggregateMetrics->getMetricInfo(
            static_cast<uint8_t>(GPMMetricId::DRAMUsage));
        dramUsage->updater = std::make_unique<DRAMUsageMetricUpdator>(
            dimmIntf, memoryInventoryObjPath);
    }

    lg2::info(
        "Created NSM GPM Aggregted Metrics : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", uuid, "NAME", name, "TYPE", type);

    nsmDevice->addSensor(gpmAggregateMetrics,
                         PollingType::GpuPerformanceMonitoring);

    std::vector<std::string> perInstanceInterfaces;
    auto result = co_await getPerInstanceInterfacesAsync(interface, objPath,
                                                         perInstanceInterfaces);

    if (result == NSM_SW_SUCCESS)
    {
        for (const auto& intf : perInstanceInterfaces)
        {
            co_await createNsmPerInstanceGPMMetric(
                gpmIntf, nsmDevice, inventoryObjPath, intf, objPath);
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

static requester::Coroutine
    createNsmPerPortGPMMetrics(SensorManager& manager,
                               const std::string& interface,
                               const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name = utils::getPropertyFromCollection<std::string>(properties,
                                                                     "Name")
                           .value();
    name = utils::makeDBusNameValid(name);
    const std::string type = interface.substr(interface.find_last_of('.') + 1);
    const std::string uuid =
        utils::getPropertyFromCollection<std::string>(properties, "UUID")
            .value();
    const uint8_t retrievalSource = utils::getPropertyFromCollection<uint64_t>(
                                        properties, "RetrievalSource")
                                        .value();
    const uint8_t gpuInstance =
        utils::getPropertyFromCollection<uint64_t>(properties, "GpuInstance")
            .value();
    const uint8_t computeInstance = utils::getPropertyFromCollection<uint64_t>(
                                        properties, "ComputeInstance")
                                        .value();
    const std::vector<std::string> metrics =
        utils::getPropertyFromCollection<std::vector<std::string>>(properties,
                                                                   "Metrics")
            .value();
    std::vector<uint64_t> ports =
        utils::getPropertyFromCollection<std::vector<uint64_t>>(properties,
                                                                "Ports")
            .value();
    const uint32_t instanceBitfield =
        utils::getPropertyFromCollection<uint64_t>(properties,
                                                   "InstanceBitfield")
            .value();
    std::string inventoryObjPath =
        utils::getPropertyFromCollection<std::string>(properties,
                                                      "InventoryObjPath")
            .value();
    inventoryObjPath = utils::makeDBusNameValid(inventoryObjPath);

    auto nsmDevice = manager.getNsmDevice(uuid);

    std::vector<NVLinkMetricsUpdatorInfo> updatorInfos;

    // sort the vector and remove duplicates
    std::sort(ports.begin(), ports.end());
    auto lastElem = std::unique(ports.begin(), ports.end());
    ports.erase(lastElem, ports.end());

    for (const auto& port : ports)
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
                "Failed to create NSM GPM PerPort Metrics. Unsupported GPM Metric {METRIC}. Config={OBJ}",
                "METRIC", metric, "OBJ", objPath);
            continue;
        }

        auto gpmPerPortMetric = std::make_shared<NsmGPMPerInstance>(
            name + "_" + metric, type, retrievalSource, gpuInstance,
            computeInstance, metricId, instanceBitfield, unit,
            std::move(updator));

        lg2::info(
            "Created NSM GPM PerPort Metric {METRIC}: UUID={UUID}, Name={NAME}, Type={TYPE}",
            "METRIC", metric, "UUID", nsmDevice->uuid, "NAME", name, "TYPE",
            type);

        nsmDevice->addSensor(gpmPerPortMetric,
                             PollingType::GpuPerformanceMonitoring);
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmGPMMetrics, "xyz.openbmc_project.Configuration.NSM_GPMMetrics")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmPerPortGPMMetrics,
    "xyz.openbmc_project.Configuration.NSM_GPMPortMetrics")
} // namespace nsm
