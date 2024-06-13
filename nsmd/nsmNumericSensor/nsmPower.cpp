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

#include "config.h"

#include "nsmPower.hpp"

#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPowerAggregator.hpp"
#include "sensorManager.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmPower::NsmPower(sdbusplus::bus::bus& bus, const std::string& name,
                   const std::string& type, uint8_t sensorId,
                   uint8_t averagingInterval,
                   const std::vector<utils::Association>& association,
                   [[maybe_unused]] const std::string& chassis_association,
                   const std::string& physicalContext,
                   const std::string* implementation,
                   const double maxAllowableValue) :
    NsmNumericSensor(
        name, type, sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusValueTimestamp>(
                bus, name, getSensorType(), SensorUnit::Watts, association,
                physicalContext, implementation, maxAllowableValue)
#ifdef NVIDIA_SHMEM
                ,
            std::make_unique<NsmNumericSensorShmem>(
                name, getSensorType(), chassis_association,
                std::make_unique<SMBPBIPowerSMBusSensorBytesConverter>())
#endif
                )),
    averagingInterval(averagingInterval)
{}

std::optional<std::vector<uint8_t>> NsmPower::genRequestMsg(eid_t eid,
                                                            uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_current_power_draw_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_current_power_draw_req(instanceId, sensorId,
                                                averagingInterval, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_current_power_draw_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPower::handleResponseMsg(const struct nsm_msg* responseMsg,
                                    size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint32_t reading = 0;

    auto rc = decode_get_current_power_draw_resp(responseMsg, responseLen, &cc,
                                                 &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // unit of power is milliwatt in NSM Command Response and selected unit
        // in SensorValue PDI is Watts. Hence it is converted to Watts.
        sensorValue->updateReading(reading / 1000.0);
    }
    else
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());

        lg2::error(
            "handleResponseMsg: decode_get_temperature_reading_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

class PowerSensorFactory : public NumericSensorBuilder
{
  public:
    std::shared_ptr<NsmNumericSensor>
        makeSensor([[maybe_unused]] const std::string& interface,
                   [[maybe_unused]] const std::string& objPath,
                   sdbusplus::bus::bus& bus,
                   const NumericSensorInfo& info) override
    {
        auto averagingInterval = utils::DBusHandler().getDbusProperty<uint64_t>(
            objPath.c_str(), "AveragingInterval", interface.c_str());

        std::vector<std::string> candidateForList;
        try
        {
            candidateForList =
                utils::DBusHandler().getDbusProperty<std::vector<std::string>>(
                    objPath.c_str(), "CompositeNumericSensors",
                    interface.c_str());
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {}

        auto sensor = std::make_shared<NsmPower>(
            bus, info.name, info.type, info.sensorId, averagingInterval,
            info.associations, info.chassis_association, info.physicalContext,
            info.implementation.get(), info.maxAllowableValue);

        if (!candidateForList.empty())
        {
            auto compositeChildValueSensor =
                std::make_unique<NsmNumericSensorCompositeChildValue>(
                    info.name, info.type, candidateForList);
            sensor->getSensorValueObject()->append(
                std::move(compositeChildValueSensor));
        }
        return sensor;
    };

    std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmPowerAggregator>(info.name, info.type,
                                                    info.priority, 0);
    };
};

static NumericSensorFactory numericSensorFactory{
    std::make_unique<PowerSensorFactory>()};

REGISTER_NSM_CREATION_FUNCTION(numericSensorFactory.getCreationFunction(),
                               "xyz.openbmc_project.Configuration.NSM_Power")

} // namespace nsm
