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

requester::Coroutine
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
    std::shared_ptr<NsmGPMInterfaceCreator> gpmInterfaceCreator,
    std::shared_ptr<NsmDevice> nsmDevice, const std::string& inventoryObjPath,
    const std::string& interface, const std::string& objPath,
    const std::string& uuid)
{
    lg2::info(
        "createNsmPerInstanceGPMMetric: interface={INTERFACE}, objPath={OBJPATH}",
        "INTERFACE", interface, "OBJPATH", objPath);

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
    // InstanceBitfield holds uint64_t for V1 and vector<uint64_t> for V2.
    // getPropertyFromCollection<uint64_t> throws bad_variant_access on V2
    // configs; swallow it so the nullopt signals "not V1" -> V2 dispatch.
    std::optional<uint64_t> v1BitmaskOpt;
    try
    {
        v1BitmaskOpt = utils::getPropertyFromCollection<uint64_t>(
            properties, "InstanceBitfield");
    }
    catch (const std::bad_variant_access&)
    {
        lg2::info("InstanceBitfield stored as vector<uint64_t>; routing to V2 "
                  "discovery flow. Name={NAME}, Metric={METRIC}, Config={INTF}",
                  "NAME", name, "METRIC", metric, "INTF", interface);
    }

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
        // coverity[missing_return]
        co_return NSM_ERROR;
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
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }

    // V2 path: InstanceBitfield is required for discovery-based per-instance.
    const std::vector<uint64_t> instanceBitfield =
        utils::getPropertyFromCollection<std::vector<uint64_t>>(
            properties, "InstanceBitfield")
            .value();
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
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine createNsmGPMMetrics(SensorManager& manager,
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

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

    auto nvlinkMetricsIntf =
        std::make_shared<NVLinkMetricsIntf>(bus, inventoryObjPath.c_str());

    auto gpmInterfaceCreator = std::make_shared<NsmGPMInterfaceCreator>(
        manager.getObjServer(), inventoryObjPath);
    gpmInterfaceCreator->addGpmIntfProperty(metricsBitfield);

    // Create DimmIntf upfront if populateMemoryBandwidth is configured
    std::shared_ptr<DimmIntf> dimmIntf = nullptr;
    std::string memoryInventoryObjPath;
    try
    {
        if (populateMemoryBandwidth)
        {
            memoryInventoryObjPath =
                utils::getPropertyFromCollection<std::string>(
                    properties, "MemoryInventoryObjPath")
                    .value();
            memoryInventoryObjPath =
                utils::makeDBusNameValid(memoryInventoryObjPath);

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

    // Create static sensor to query supported GPM metrics from device.
    // This sensor queries the device for maxMetricsPerCommand and then
    // creates the appropriate number of NsmGPMAggregated sensors, splitting
    // the configured metricsBitfield into chunks as needed.
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

    std::vector<std::string> perInstanceInterfaces;
    try
    {
        auto result = co_await getPerInstanceInterfacesAsync(
            interface, objPath, perInstanceInterfaces);

        if (result == NSM_SW_SUCCESS)
        {
            for (const auto& intf : perInstanceInterfaces)
            {
                co_await createNsmPerInstanceGPMMetric(
                    gpmInterfaceCreator, nsmDevice, inventoryObjPath, intf,
                    objPath, uuid);
            }
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "Caught execption while create getPerInstanceInterfacesAsync. Error: {ERROR}",
            "ERROR", e.what());
    }
    gpmInterfaceCreator->initialize();
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine createNsmPerPortGPMMetrics(SensorManager& manager,
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
    // InstanceBitfield holds uint64_t for V1 and vector<uint64_t> for V2.
    std::optional<uint64_t> v1BitmaskOpt;
    try
    {
        v1BitmaskOpt = utils::getPropertyFromCollection<uint64_t>(
            properties, "InstanceBitfield");
    }
    catch (const std::bad_variant_access&)
    {
        lg2::info("InstanceBitfield stored as vector<uint64_t>; routing to V2 "
                  "discovery flow. Name={NAME}, Config={INTF}",
                  "NAME", name, "INTF", interface);
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
            utils::getPropertyFromCollection<std::vector<uint64_t>>(
                properties, "InstanceBitfield")
                .value();
        instanceBitfieldBytes.resize(instanceBitfield.size());
        for (size_t i = 0; i < instanceBitfield.size(); i++)
        {
            instanceBitfieldBytes[i].byte =
                static_cast<uint8_t>(instanceBitfield[i]);
        }
    }

    std::string inventoryObjPath =
        utils::getPropertyFromCollection<std::string>(properties,
                                                      "InventoryObjPath")
            .value();
    inventoryObjPath = utils::makeDBusNameValid(inventoryObjPath);

    auto nsmDevice = manager.getNsmDeviceFromStaticUUID(uuid);

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
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

REGISTER_NSM_CREATION_FUNCTION(
    createNsmGPMMetrics, "xyz.openbmc_project.Configuration.NSM_GPMMetrics")
REGISTER_NSM_CREATION_FUNCTION(
    createNsmPerPortGPMMetrics,
    "xyz.openbmc_project.Configuration.NSM_GPMPortMetrics")
} // namespace nsm
