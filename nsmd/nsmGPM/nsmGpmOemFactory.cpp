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
#include "nsmObjectFactory.hpp"

#include <phosphor-logging/lg2.hpp>

namespace nsm
{
enum class GPMMetricId : uint8_t
{
    NVLinkRawTxBandwidth = 10,
    NVLinkDataTxBandwidth = 11,
    NVLinkRawRxBandwidth = 12,
    NVLinkDataRxBandwidth = 13
};

template <typename T>
std::optional<T>
    getPropertyFromCollection(const PropertyValuesCollection& collection,
                              const std::string& name)
{
    auto fit = std::lower_bound(collection.cbegin(), collection.cend(), name,
                                [&](const auto& elem, const std::string& name) {
        return elem.first < name;
    });

    if (fit == collection.cend() || fit->first != name)
    {
        lg2::error("getPropertyFromCollection : Property {PROP} not found.",
                   "PROP", name);

        return std::nullopt;
    }

    return std::get<T>(fit->second);
}

std::vector<uint8_t> convertToBytes(const std::vector<uint64_t>& data)
{
    std::vector<uint8_t> result(data.size());

    for (size_t i{}; i < data.size(); ++i)
    {
        result[i] = data[i];
    }

    return result;
}

static std::vector<std::string>
    getPerInstanceInterfaces(const std::string& interface,
                             const std::string& objPath)
{
    const std::string interfaceName = interface + ".PerInstanceMetrics";
    std::vector<std::string> interfaces;
    std::map<std::string, std::vector<std::string>> mapperResponse;
    auto& bus = utils::DBusHandler::getBus();

    auto mapper = bus.new_method_call(utils::mapperService, utils::mapperPath,
                                      utils::mapperInterface, "GetObject");
    mapper.append(objPath, std::vector<std::string>{});

    auto mapperResponseMsg = bus.call(mapper);
    mapperResponseMsg.read(mapperResponse);

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

    return interfaces;
}

void createNsmPerInstanceGPMMetric(std::shared_ptr<GPMMetricsIntf> gpmInf,
                                   std::shared_ptr<NsmDevice> nsmDevice,
                                   const std::string& inventoryObjPath,
                                   const std::string& interface,
                                   const std::string& objPath)
{
    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name =
        getPropertyFromCollection<std::string>(properties, "Name").value();
    name = utils::makeDBusNameValid(name);
    std::string type =
        getPropertyFromCollection<std::string>(properties, "Type").value();
    type = utils::makeDBusNameValid(type);
    const bool priority =
        getPropertyFromCollection<bool>(properties, "Priority").value();
    const uint8_t retrievalSource =
        getPropertyFromCollection<uint64_t>(properties, "RetrievalSource")
            .value();
    const uint8_t gpuInstance =
        getPropertyFromCollection<uint64_t>(properties, "GpuInstance").value();
    const uint8_t computeInstance =
        getPropertyFromCollection<uint64_t>(properties, "ComputeInstance")
            .value();
    const std::string metric =
        getPropertyFromCollection<std::string>(properties, "Metric").value();
    const uint8_t metricId =
        getPropertyFromCollection<uint64_t>(properties, "MetricId").value();
    const uint32_t instanceBitfield =
        getPropertyFromCollection<uint64_t>(properties, "InstanceBitfield")
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
        return;
    }

    auto gpmPerInstanceMetric = std::make_shared<NsmGPMPerInstance>(
        name, type, retrievalSource, gpuInstance, computeInstance, metricId,
        instanceBitfield, metricUnit, metricUpdator);

    lg2::info(
        "Created NSM GPM PerInstance Metrics : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", nsmDevice->uuid, "NAME", name, "TYPE", type);

    nsmDevice->deviceSensors.emplace_back(gpmPerInstanceMetric);

    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(gpmPerInstanceMetric);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(gpmPerInstanceMetric);
    }
}

static void createNsmGPMMetrics(SensorManager& manager,
                                const std::string& interface,
                                const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name =
        getPropertyFromCollection<std::string>(properties, "Name").value();
    name = utils::makeDBusNameValid(name);
    const std::string type = interface.substr(interface.find_last_of('.') + 1);
    const std::string uuid =
        getPropertyFromCollection<std::string>(properties, "UUID").value();
    const bool priority =
        getPropertyFromCollection<bool>(properties, "Priority").value();
    const uint8_t retrievalSource =
        getPropertyFromCollection<uint64_t>(properties, "RetrievalSource")
            .value();
    const uint8_t gpuInstance =
        getPropertyFromCollection<uint64_t>(properties, "GpuInstance").value();
    const uint8_t computeInstance =
        getPropertyFromCollection<uint64_t>(properties, "ComputeInstance")
            .value();
    const std::vector<uint8_t> metricsBitfield = convertToBytes(
        getPropertyFromCollection<std::vector<uint64_t>>(properties,
                                                         "MetricsBitfield")
            .value());
    std::string inventoryObjPath =
        getPropertyFromCollection<std::string>(properties, "InventoryObjPath")
            .value();
    inventoryObjPath = utils::makeDBusNameValid(inventoryObjPath);

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of GPM Metrics PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

    auto gpmIntf = std::make_shared<GPMMetricsIntf>(bus,
                                                    inventoryObjPath.c_str());
    auto nvlinkMetricsIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, inventoryObjPath.c_str());

    auto gpmAggregateMetrics = std::make_shared<NsmGPMAggregated>(
        name, type, inventoryObjPath, retrievalSource, gpuInstance,
        computeInstance, metricsBitfield, gpmIntf, nvlinkMetricsIntf);

    lg2::info(
        "Created NSM GPM Aggregted Metrics : UUID={UUID}, Name={NAME}, Type={TYPE}",
        "UUID", uuid, "NAME", name, "TYPE", type);

    nsmDevice->deviceSensors.emplace_back(gpmAggregateMetrics);

    if (priority)
    {
        nsmDevice->prioritySensors.emplace_back(gpmAggregateMetrics);
    }
    else
    {
        nsmDevice->roundRobinSensors.emplace_back(gpmAggregateMetrics);
    }

    auto perInstanceInterfaces = getPerInstanceInterfaces(interface, objPath);
    for (const auto& intf : perInstanceInterfaces)
    {
        createNsmPerInstanceGPMMetric(gpmIntf, nsmDevice, inventoryObjPath,
                                      intf, objPath);
    }
}

static void createNsmPerPortGPMMetrics(SensorManager& manager,
                                       const std::string& interface,
                                       const std::string& objPath)
{
    auto& bus = utils::DBusHandler::getBus();

    auto properties = utils::DBusHandler().getDbusProperties(objPath.c_str(),
                                                             interface.c_str());
    std::sort(properties.begin(), properties.end());

    std::string name =
        getPropertyFromCollection<std::string>(properties, "Name").value();
    name = utils::makeDBusNameValid(name);
    const std::string type = interface.substr(interface.find_last_of('.') + 1);
    const std::string uuid =
        getPropertyFromCollection<std::string>(properties, "UUID").value();
    const bool priority =
        getPropertyFromCollection<bool>(properties, "Priority").value();
    const uint8_t retrievalSource =
        getPropertyFromCollection<uint64_t>(properties, "RetrievalSource")
            .value();
    const uint8_t gpuInstance =
        getPropertyFromCollection<uint64_t>(properties, "GpuInstance").value();
    const uint8_t computeInstance =
        getPropertyFromCollection<uint64_t>(properties, "ComputeInstance")
            .value();
    const std::vector<std::string> metrics =
        getPropertyFromCollection<std::vector<std::string>>(properties,
                                                            "Metrics")
            .value();
    std::vector<uint64_t> ports =
        getPropertyFromCollection<std::vector<uint64_t>>(properties, "Ports")
            .value();
    const uint32_t instanceBitfield =
        getPropertyFromCollection<uint64_t>(properties, "InstanceBitfield")
            .value();
    std::string inventoryObjPath =
        getPropertyFromCollection<std::string>(properties, "InventoryObjPath")
            .value();
    inventoryObjPath = utils::makeDBusNameValid(inventoryObjPath);

    auto nsmDevice = manager.getNsmDevice(uuid);

    if (!nsmDevice)
    {
        // cannot found a nsmDevice for the sensor
        lg2::error(
            "The UUID of GPM Metrics PDI matches no NsmDevice : UUID={UUID}, Name={NAME}, Type={TYPE}",
            "UUID", uuid, "NAME", name, "TYPE", type);
        return;
    }

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

        nsmDevice->deviceSensors.emplace_back(gpmPerPortMetric);

        if (priority)
        {
            nsmDevice->prioritySensors.emplace_back(gpmPerPortMetric);
        }
        else
        {
            nsmDevice->roundRobinSensors.emplace_back(gpmPerPortMetric);
        }
    }
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmGPMMetrics, "xyz.openbmc_project.Configuration.NSM_GPMMetrics")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmPerPortGPMMetrics,
    "xyz.openbmc_project.Configuration.NSM_GPMPortMetrics")
} // namespace nsm
