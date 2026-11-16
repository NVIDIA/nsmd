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
#include "nsmNumericSensor.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmThreshold.hpp"
#include "nsmThresholdAggregator.hpp"
#include "nsmThresholdEvaluator.hpp"
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
    SensorManager& manager, std::shared_ptr<NsmNumericSensor> numericSensor,
    const NumericSensorInfo& info, const uuid_t& uuid,
    std::shared_ptr<NsmDevice> nsmDevice,
    std::unordered_map<std::string, FeatureThreshold> featureThresholds) :
    manager(manager), numericSensor(numericSensor), info(info), uuid(uuid),
    nsmDevice(std::move(nsmDevice)),
    featureThresholds(std::move(featureThresholds))
{}

requester::Coroutine NsmThresholdFactory::make()
{
    std::unordered_map<std::string, std::string> thresholdInterfaces;
    for (const auto& [name, threshold] : featureThresholds)
    {
        thresholdInterfaces[name] = {};
    }

    // Capture interface objects for evaluator creation, one per tier.
    std::shared_ptr<ThresholdWarningIntf> warningIntf;
    std::shared_ptr<ThresholdCriticalIntf> criticalIntf;

    if (auto rc = co_await processThresholdsPair<ThresholdWarningIntf,
                                                 NsmThresholdValueWarningLow,
                                                 NsmThresholdValueWarningHigh>(
            thresholdInterfaces,
            ThresholdsPairInfo{.lowerThreshold{"LowerCaution"},
                               .upperThreshold{"UpperCaution"}},
            &warningIntf);
        rc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmThresholdFactory: {TIER} threshold setup failed for {SENSOR}, rc={RC}",
            "TIER", std::string("Warning"), "SENSOR", info.name, "RC", rc);
    }

    if (auto rc = co_await processThresholdsPair<ThresholdCriticalIntf,
                                                 NsmThresholdValueCriticalLow,
                                                 NsmThresholdValueCriticalHigh>(
            thresholdInterfaces,
            ThresholdsPairInfo{.lowerThreshold{"LowerCritical"},
                               .upperThreshold{"UpperCritical"}},
            &criticalIntf);
        rc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmThresholdFactory: {TIER} threshold setup failed for {SENSOR}, rc={RC}",
            "TIER", std::string("Critical"), "SENSOR", info.name, "RC", rc);
    }

    // HardShutdown (NSM Fatal/Non-recoverable tier, confirmed distinct from
    // Critical). Whether HardShutdown is the spec-sanctioned mapping is
    // tracked separately.
    std::shared_ptr<ThresholdHardShutdownIntf> hardShutdownIntf;

    if (auto rc =
            co_await processThresholdsPair<ThresholdHardShutdownIntf,
                                           NsmThresholdValueHardShutdownLow,
                                           NsmThresholdValueHardShutdownHigh>(
                thresholdInterfaces,
                ThresholdsPairInfo{.lowerThreshold{"LowerFatal"},
                                   .upperThreshold{"UpperFatal"}},
                &hardShutdownIntf);
        rc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmThresholdFactory: {TIER} threshold setup failed for {SENSOR}, rc={RC}",
            "TIER", std::string("HardShutdown"), "SENSOR", info.name, "RC", rc);
    }

    // SoftShutdown and PerformanceLoss (STH-REQ-09, threshold-tier
    // extensibility): infra-only today — no feature list assigns either tier
    // to any sensor. Wired unconditionally so a tier passed in
    // featureThresholds activates evaluation; processThresholdsPair() is a
    // no-op until then.
    std::shared_ptr<ThresholdSoftShutdownIntf> softShutdownIntf;
    std::shared_ptr<ThresholdPerformanceLossIntf> perfLossIntf;

    if (auto rc =
            co_await processThresholdsPair<ThresholdSoftShutdownIntf,
                                           NsmThresholdValueSoftShutdownLow,
                                           NsmThresholdValueSoftShutdownHigh>(
                thresholdInterfaces,
                ThresholdsPairInfo{.lowerThreshold{"LowerSoftShutdown"},
                                   .upperThreshold{"UpperSoftShutdown"}},
                &softShutdownIntf);
        rc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmThresholdFactory: {TIER} threshold setup failed for {SENSOR}, rc={RC}",
            "TIER", std::string("SoftShutdown"), "SENSOR", info.name, "RC", rc);
    }

    if (auto rc = co_await processThresholdsPair<
            ThresholdPerformanceLossIntf, NsmThresholdValuePerformanceLossLow,
            NsmThresholdValuePerformanceLossHigh>(
            thresholdInterfaces,
            ThresholdsPairInfo{.lowerThreshold{"LowerPerformanceLoss"},
                               .upperThreshold{"UpperPerformanceLoss"}},
            &perfLossIntf);
        rc != NSM_SUCCESS)
    {
        lg2::error(
            "NsmThresholdFactory: {TIER} threshold setup failed for {SENSOR}, rc={RC}",
            "TIER", std::string("PerformanceLoss"), "SENSOR", info.name, "RC",
            rc);
    }

    // Attach threshold evaluator to the sensor's primary D-Bus value object
    // if at least one tier interface was created.
    if (warningIntf || criticalIntf || hardShutdownIntf || softShutdownIntf ||
        perfLossIntf)
    {
        auto evaluator = std::make_unique<NsmThresholdEvaluator>(
            warningIntf, criticalIntf, hardShutdownIntf, softShutdownIntf,
            perfLossIntf);

        bool evaluatorAttached = false;
        auto sensorValueObject = numericSensor->getSensorValueObject();
        if (sensorValueObject)
        {
            const auto& objects = sensorValueObject->getObjects();
            for (const auto& obj : objects)
            {
                if (obj->setThresholdEvaluator(evaluator))
                {
                    evaluatorAttached = true;
                    break;
                }
            }
        }
        if (!evaluatorAttached)
        {
            lg2::error(
                "NsmThresholdFactory: no NsmNumericSensorDbusValue found for {SENSOR} — threshold evaluator not attached",
                "SENSOR", info.name);
        }
    }

    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

template <typename DBusIntf,
          std::derived_from<NsmThresholdValue> ThresholdValueLow,
          std::derived_from<NsmThresholdValue> ThresholdValueHigh>
requester::Coroutine NsmThresholdFactory::processThresholdsPair(
    const std::unordered_map<std::string, std::string>& thresholdInterfaces,
    const ThresholdsPairInfo& thresholdsPairInfo,
    std::shared_ptr<DBusIntf>* outIntf)
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
            auto rc = co_await createNsmThreshold(
                lowerThresholdIntf->second, thresholdsPairInfo.lowerThreshold,
                std::move(thresholdValue));
            // Export the created interface to the caller only on success.
            if (rc == NSM_SUCCESS && outIntf != nullptr)
            {
                *outIntf = dbusInterface;
            }
        }

        if (upperThresholdIntf != thresholdInterfaces.end())
        {
            auto thresholdValue = std::make_unique<ThresholdValueHigh>(
                info.name + '_' + thresholdsPairInfo.upperThreshold,
                "NSM_ThermalParameter", dbusInterface);
            auto rc = co_await createNsmThreshold(
                upperThresholdIntf->second, thresholdsPairInfo.upperThreshold,
                std::move(thresholdValue));
            // Export the created interface to the caller only on success.
            if (rc == NSM_SUCCESS && outIntf != nullptr)
            {
                *outIntf = dbusInterface;
            }
        }
    }
    // coverity[missing_return]
    co_return NSM_SUCCESS;
}

requester::Coroutine NsmThresholdFactory::createNsmThreshold(
    [[maybe_unused]] const std::string& intfName,
    const std::string& thresholdType,
    std::unique_ptr<NsmThresholdValue> thresholdValue)
{
    NumericSensorInfo thresholdInfo;

    thresholdInfo.name = info.name + "_" + thresholdType;

    const auto& threshold = featureThresholds.at(thresholdType);
    if (!threshold.parameterId)
    {
        thresholdValue->updateReading(threshold.value);

        nsmDevice->addDeviceSensors(std::move(thresholdValue));

        lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type=Static",
                  "UUID", uuid, "NAME", thresholdInfo.name);
        co_return NSM_SUCCESS;
    }

    thresholdInfo.type = "NSM_ThermalParameter";
    thresholdInfo.sensorId = *threshold.parameterId;

    auto sensor = std::make_shared<NsmThreshold>(
        thresholdInfo.name, thresholdInfo.type, thresholdInfo.sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::move(thresholdValue)));

    lg2::info("Created NSM Sensor : UUID={UUID}, Name={NAME}, Type={TYPE}",
              "UUID", uuid, "NAME", thresholdInfo.name, "TYPE",
              thresholdInfo.type);

    if (!threshold.periodicUpdate)
    {
        nsmDevice->addStaticSensor(sensor);
        nsmDevice->addCapabilityRefreshSensor(sensor);
        co_return NSM_SUCCESS;
    }

    thresholdInfo.priority = threshold.priority;
    thresholdInfo.aggregated = threshold.aggregated;

    NumericSensorFactory::makeAggregatorAndAddSensor(
        std::make_unique<NsmThresholdAggregatorBuilder>().get(), thresholdInfo,
        sensor, uuid, nsmDevice.get());
    co_return NSM_SUCCESS;
}

} // namespace nsm
