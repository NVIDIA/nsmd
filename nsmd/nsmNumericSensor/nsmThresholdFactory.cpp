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

#include "nsmThresholdFactory.hpp"

#include "dBusAsyncUtils.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmThreshold.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmThresholdValue.hpp"

using namespace std::string_literals;

namespace nsm
{

class NsmThresholdAggregatorBuilder : public NumericSensorAggregatorBuilder
{
  public:
    virtual ~NsmThresholdAggregatorBuilder() = default;

    std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmThresholdAggregator>(info.name, info.type,
                                                        info.priority);
    }
};

NsmThresholdFactory::NsmThresholdFactory(
    SensorManager& manager, const std::string& interface,
    const std::string& objPath, std::shared_ptr<NsmNumericSensor> numericSensor,
    const NumericSensorInfo& info, const uuid_t& uuid) :
    manager(manager), interface(interface), objPath(objPath),
    numericSensor(numericSensor), info(info), uuid(uuid),
    nsmDevice(manager.getNsmDeviceFromStaticUUID(uuid))
{}

requester::Coroutine NsmThresholdFactory::make()
{
    std::unordered_map<std::string, std::string> thresholdInterfaces;
    auto result = co_await getThresholdInterfacesAsync(thresholdInterfaces);

    if (result != NSM_SW_SUCCESS)
    {
        lg2::error("Failed to get threshold interfaces for {OBJPATH}",
                   "OBJPATH", objPath);
        co_return result;
    }

    co_await processThresholdsPair<ThresholdWarningIntf,
                                   NsmThresholdValueWarningLow,
                                   NsmThresholdValueWarningHigh>(
        thresholdInterfaces,
        ThresholdsPairInfo{.lowerThreshold{"LowerCaution"},
                           .upperThreshold{"UpperCaution"}});

    co_await processThresholdsPair<ThresholdCriticalIntf,
                                   NsmThresholdValueCriticalLow,
                                   NsmThresholdValueCriticalHigh>(
        thresholdInterfaces,
        ThresholdsPairInfo{.lowerThreshold{"LowerCritical"},
                           .upperThreshold{"UpperCritical"}});

    co_await processThresholdsPair<ThresholdHardShutdownIntf,
                                   NsmThresholdValueHardShutdownLow,
                                   NsmThresholdValueHardShutdownHigh>(
        thresholdInterfaces, ThresholdsPairInfo{.lowerThreshold{"LowerFatal"},
                                                .upperThreshold{"UpperFatal"}});
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine NsmThresholdFactory::getThresholdInterfacesAsync(
    std::unordered_map<std::string, std::string>& thresholdInterfaces)
{
    const std::string thresholdInterfaceName = interface + ".ThermalParameters";
    thresholdInterfaces.clear();

    // Use existing async utility instead of blocking call
    auto mapperResponse = co_await utils::coGetServiceMap(objPath,
                                                          dbus::Interfaces{});

    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& intf : interfaces)
        {
            if (intf.find(thresholdInterfaceName) != std::string::npos)
            {
                // Use async property read instead of blocking call
                auto name = co_await utils::coGetDbusProperty<std::string>(
                    objPath, "Name", intf, service);

                thresholdInterfaces[name] = intf;
            }
        }
    }

    co_return NSM_SW_SUCCESS;
}

// Keep original function for backward compatibility (deprecated)
std::unordered_map<std::string, std::string>
    NsmThresholdFactory::getThresholdInterfaces()
{
    const std::string thresholdInterfaceName = interface + ".ThermalParameters";
    std::unordered_map<std::string, std::string> thresholdInterfaces;
    std::map<std::string, std::vector<std::string>> mapperResponse;
    auto& bus = utils::DBusHandler::getBus();

    auto mapper = bus.new_method_call(utils::mapperService, utils::mapperPath,
                                      utils::mapperInterface, "GetObject");
    mapper.append(objPath, std::vector<std::string>{});

    auto mapperResponseMsg = bus.call(mapper);
    mapperResponseMsg.read(mapperResponse);

    for (const auto& [service, interfaces] : mapperResponse)
    {
        for (const auto& intf : interfaces)
        {
            if (intf.find(thresholdInterfaceName) != std::string::npos)
            {
                auto name = utils::DBusHandler().getDbusProperty<std::string>(
                    objPath.c_str(), "Name", intf.c_str());

                thresholdInterfaces[name] = intf;
            }
        }
    }

    return thresholdInterfaces;
}

template <typename DBusIntf,
          std::derived_from<NsmThresholdValue> ThresholdValueLow,
          std::derived_from<NsmThresholdValue> ThresholdValueHigh>
requester::Coroutine NsmThresholdFactory::processThresholdsPair(
    const std::unordered_map<std::string, std::string>& thresholdInterfaces,
    const ThresholdsPairInfo& thresholdsPairInfo)
{
    auto lowerThresholdIntf =
        thresholdInterfaces.find(thresholdsPairInfo.lowerThreshold);
    auto upperThresholdIntf =
        thresholdInterfaces.find(thresholdsPairInfo.upperThreshold);

    if (lowerThresholdIntf != thresholdInterfaces.end() ||
        upperThresholdIntf != thresholdInterfaces.end())
    {
        auto dbusInterface = std::make_shared<DBusIntf>(
            utils::DBusHandler::getBus(),
            ("/xyz/openbmc_project/sensors/"s + numericSensor->getSensorType() +
             '/' + info.name)
                .c_str());

        if (lowerThresholdIntf != thresholdInterfaces.end())
        {
            auto thresholdValue = std::make_unique<ThresholdValueLow>(
                info.name + '_' + thresholdsPairInfo.lowerThreshold,
                "NSM_ThermalParameter", dbusInterface);
            co_await createNsmThreshold(lowerThresholdIntf->second,
                                        thresholdsPairInfo.lowerThreshold,
                                        std::move(thresholdValue));
        }

        if (upperThresholdIntf != thresholdInterfaces.end())
        {
            auto thresholdValue = std::make_unique<ThresholdValueHigh>(
                info.name + '_' + thresholdsPairInfo.upperThreshold,
                "NSM_ThermalParameter", dbusInterface);
            co_await createNsmThreshold(upperThresholdIntf->second,
                                        thresholdsPairInfo.upperThreshold,
                                        std::move(thresholdValue));
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine NsmThresholdFactory::createNsmThreshold(
    const std::string& intfName, const std::string& thresholdType,
    std::unique_ptr<NsmThresholdValue> thresholdValue)
{
    NumericSensorInfo thresholdInfo;

    thresholdInfo.name = info.name + "_" + thresholdType;

    auto allCurrentIfaceProperties = co_await utils::coGetAllDbusProperty(
        utils::entityManagerServiceStr, objPath.c_str(), intfName.c_str());

    bool dynamic{};
    if (allCurrentIfaceProperties.count("Dynamic"))
    {
        dynamic = std::get<bool>(allCurrentIfaceProperties.at("Dynamic"));
    }

    if (!dynamic)
    {
        double threshold{};
        if (allCurrentIfaceProperties.count("Value"))
        {
            threshold = std::get<double>(allCurrentIfaceProperties.at("Value"));
        }

        thresholdValue->updateReading(threshold);

        nsmDevice->deviceSensors.push_back(std::move(thresholdValue));

        lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type=Static",
                  "UUID", uuid, "NAME", thresholdInfo.name);
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }

    std::string type{};
    if (allCurrentIfaceProperties.count("Type"))
    {
        type = std::get<std::string>(allCurrentIfaceProperties.at("Type"));
    }
    thresholdInfo.type = type;

    if (thresholdInfo.type != "NSM_ThermalParameter")
    {
        lg2::error(
            "Unsupported Threshold Type {TYPE} : UUID={UUID}, Name={NAME}",
            "UUID", uuid, "NAME", thresholdInfo.name, "TYPE",
            thresholdInfo.type);
        // coverity[missing_return]
        co_return NSM_ERROR;
    }

    uint64_t sensorId{};
    if (allCurrentIfaceProperties.count("ParameterId"))
    {
        sensorId =
            std::get<uint64_t>(allCurrentIfaceProperties.at("ParameterId"));
    }
    thresholdInfo.sensorId = sensorId;

    bool periodicUpdate{false};

    try
    {
        periodicUpdate = utils::DBusHandler().getDbusProperty<bool>(
            objPath.c_str(), "PeriodicUpdate", intfName.c_str());
    }
    catch (const std::exception& e)
    {}

    auto sensor = std::make_shared<NsmThreshold>(
        thresholdInfo.name, thresholdInfo.type, thresholdInfo.sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::move(thresholdValue)));

    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", thresholdInfo.name, "TYPE",
              thresholdInfo.type);

    if (!periodicUpdate)
    {
        nsmDevice->addStaticSensor(sensor);
        nsmDevice->capabilityRefreshSensors.emplace_back(sensor);
        // coverity[missing_return]
        co_return NSM_SUCCESS;
    }

    bool priority{};
    if (allCurrentIfaceProperties.count("Priority"))
    {
        priority = std::get<bool>(allCurrentIfaceProperties.at("Priority"));
    }
    thresholdInfo.priority = priority;

    bool aggregated{};
    if (allCurrentIfaceProperties.count("Aggregated"))
    {
        aggregated = std::get<bool>(allCurrentIfaceProperties.at("Aggregated"));
    }
    thresholdInfo.aggregated = aggregated;

    NumericSensorFactory::makeAggregatorAndAddSensor(
        std::make_unique<NsmThresholdAggregatorBuilder>().get(), thresholdInfo,
        sensor, uuid, nsmDevice.get());
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

} // namespace nsm
