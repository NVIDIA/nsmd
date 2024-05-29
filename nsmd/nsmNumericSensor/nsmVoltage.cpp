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

#include "nsmVoltage.hpp"

#include "platform-environmental.h"

#include "nsmNumericSensorFactory.hpp"
#include "nsmVoltageAggregator.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
NsmVoltage::NsmVoltage(sdbusplus::bus::bus& bus, const std::string& name,
                       const std::string& type, uint8_t sensorId,
                       const std::vector<utils::Association>& association) :
    NsmNumericSensor(
        name, type, sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusValue>(
                bus, name, getSensorType(), SensorUnit::Volts, association)))
{}

std::optional<std::vector<uint8_t>>
    NsmVoltage::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_voltage_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_voltage_req(instanceId, sensorId, requestPtr);
    if (rc)
    {
        lg2::error("encode_get_voltage_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmVoltage::handleResponseMsg(const struct nsm_msg* responseMsg,
                                      size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;
    uint32_t reading = 0;

    auto rc = decode_get_voltage_resp(responseMsg, responseLen, &cc,
                                      &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // unit of voltage is microvolts in NSM Command Response and selected
        // unit in SensorValue PDI is Volts. Hence it is converted to Volts.
        sensorValue->updateReading(reading / 1'000'000.0);
    }
    else
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());

        lg2::error(
            "handleResponseMsg: decode_get_voltage_resp "
            "sensor={NAME} with reasonCode={REASONCODE}, cc={CC} and rc={RC}",
            "NAME", getName(), "REASONCODE", reason_code, "CC", cc, "RC", rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

class VoltageSensorFactory : public NumericSensorBuilder
{
  public:
    std::shared_ptr<NsmNumericSensor>
        makeSensor([[maybe_unused]] const std::string& interface,
                   [[maybe_unused]] const std::string& objPath,
                   sdbusplus::bus::bus& bus,
                   const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmVoltage>(bus, info.name, info.type,
                                            info.sensorId, info.associations);
    };

    std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmVoltageAggregator>(info.name, info.type,
                                                      info.priority);
    };
};

static NumericSensorFactory numericSensorFactory{
    std::make_unique<VoltageSensorFactory>()};

REGISTER_NSM_CREATION_FUNCTION(numericSensorFactory.getCreationFunction(),
                               "xyz.openbmc_project.Configuration.NSM_Voltage")

} // namespace nsm
