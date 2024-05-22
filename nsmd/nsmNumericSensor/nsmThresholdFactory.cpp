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

requester::Coroutine static updateSensorWithRetries(
    SensorManager& manager, std::shared_ptr<NsmSensor> sensor, const eid_t eid,
    const int retries)
{
    for (int retryCount{0}; retries == -1 || retryCount <= retries;
         ++retryCount)
    {
        auto rc = co_await sensor->update(manager, eid);
        if (rc == NSM_SW_SUCCESS || rc == NSM_SW_ERROR_COMMAND_FAIL)
        {
            co_return rc;
        }

        lg2::error(
            "updateSensorWithRetries: sensor update failed. rc={RC}, name={NAME}, "
            "eid={EID}, retryCount={RETRY}.",
            "RC", rc, "NAME", sensor->getName(), "EID", eid, "RETRY",
            retryCount);
    }

    co_return NSM_SW_ERROR;
}

NsmThresholdFactory::NsmThresholdFactory(
    SensorManager& manager, const std::string& interface,
    const std::string& objPath, std::shared_ptr<NsmNumericSensor> numericSensor,
    const NumericSensorInfo& info, const uuid_t& uuid) :
    manager(manager),
    interface(interface), objPath(objPath), numericSensor(numericSensor),
    info(info), uuid(uuid), nsmDevice(manager.getNsmDevice(uuid))
{}

void NsmThresholdFactory::make()
{
    std::unordered_map<std::string, std::string> thresholdInterfaces =
        getThresholdInterfaces();

    processThresholdsPair<ThresholdWarningIntf, NsmThresholdValueWarningLow,
                          NsmThresholdValueWarningHigh>(
        thresholdInterfaces,
        ThresholdsPairInfo{.lowerThreshold{"LowerCaution"},
                           .upperThreshold{"UpperCaution"}});

    processThresholdsPair<ThresholdCriticalIntf, NsmThresholdValueCriticalLow,
                          NsmThresholdValueCriticalHigh>(
        thresholdInterfaces,
        ThresholdsPairInfo{.lowerThreshold{"LowerCritical"},
                           .upperThreshold{"UpperCritical"}});

    processThresholdsPair<ThresholdHardShutdownIntf,
                          NsmThresholdValueHardShutdownLow,
                          NsmThresholdValueHardShutdownHigh>(
        thresholdInterfaces, ThresholdsPairInfo{.lowerThreshold{"LowerFatal"},
                                                .upperThreshold{"UpperFatal"}});
}

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
void NsmThresholdFactory::processThresholdsPair(
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
            createNsmThreshold(lowerThresholdIntf->second,
                               thresholdsPairInfo.lowerThreshold,
                               std::move(thresholdValue));
        }

        if (upperThresholdIntf != thresholdInterfaces.end())
        {
            auto thresholdValue = std::make_unique<ThresholdValueHigh>(
                info.name + '_' + thresholdsPairInfo.upperThreshold,
                "NSM_ThermalParameter", dbusInterface);
            createNsmThreshold(upperThresholdIntf->second,
                               thresholdsPairInfo.upperThreshold,
                               std::move(thresholdValue));
        }
    }
}

void NsmThresholdFactory::createNsmThreshold(
    const std::string& intfName, const std::string& thresholdType,
    std::unique_ptr<NsmThresholdValue> thresholdValue)
{
    NumericSensorInfo thresholdInfo;

    thresholdInfo.name = info.name + "_" + thresholdType;

    bool dynamic = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Dynamic", intfName.c_str());

    if (!dynamic)
    {
        double threshold = utils::DBusHandler().getDbusProperty<double>(
            objPath.c_str(), "Value", intfName.c_str());

        std::cout << "Value : " << threshold << '\n';

        thresholdValue->updateReading(threshold);

        nsmDevice->deviceSensors.push_back(std::move(thresholdValue));

        lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type=Static",
                  "UUID", uuid, "NAME", thresholdInfo.name);

        return;
    }

    thresholdInfo.type = utils::DBusHandler().getDbusProperty<std::string>(
        objPath.c_str(), "Type", intfName.c_str());

    if (thresholdInfo.type != "NSM_ThermalParameter")
    {
        lg2::error(
            "Unsupported Threshold Type {TYPE} : UUID={UUID}, Name={NAME}",
            "UUID", uuid, "NAME", thresholdInfo.name, "TYPE",
            thresholdInfo.type);

        return;
    }

    thresholdInfo.sensorId = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "ParameterId", intfName.c_str());

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
        nsmDevice->deviceSensors.emplace_back(sensor);
        nsmDevice->capabilityRefreshSensors.emplace_back(sensor);

        updateSensorWithRetries(manager, sensor, manager.getEid(nsmDevice), -1)
            .detach();

        return;
    }

    thresholdInfo.priority = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Priority", intfName.c_str());

    thresholdInfo.aggregated = utils::DBusHandler().getDbusProperty<bool>(
        objPath.c_str(), "Aggregated", intfName.c_str());

    NumericSensorFactory::makeAggregatorAndAddSensor(
        std::make_unique<NsmThresholdAggregatorBuilder>().get(), thresholdInfo,
        sensor, uuid, nsmDevice.get());
}

} // namespace nsm