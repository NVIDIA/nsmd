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

#include "nsmTemp.hpp"

#include "platform-environmental.h"

#include "nsmNumericSensorFactory.hpp"
#include "nsmTempAggregator.hpp"

#include <phosphor-logging/lg2.hpp>
#ifdef NVIDIA_SHMEM
#include <telemetry_mrd_producer.hpp>
#endif
namespace nsm
{
NsmTemp::NsmTemp(sdbusplus::bus::bus& bus, const std::string& name,
                 const std::string& type, uint8_t sensorId,
                 const std::vector<utils::Association>& association,
                 [[maybe_unused]] const std::string& chassis_association,
                 const std::string& physicalContext,
                 const std::string* implementation,
                 const double maxAllowableValue, const double maxValue,
                 const double minValue, const std::string* readingBasis,
                 const std::string* description) :
    NsmNumericSensor(
        name, type, sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
#ifdef NVIDIA_SHMEM
            std::make_unique<NsmNumericSensorShmem>(
                name, getSensorType(), chassis_association,
                std::make_unique<SMBPBITempSMBusSensorBytesConverter>()),
#endif
            std::make_unique<NsmNumericSensorDbusValue>(
                bus, name, getSensorType(), SensorUnit::DegreesC, association,
                physicalContext, implementation, maxAllowableValue, maxValue,
                minValue, readingBasis, description)))
{}

std::optional<std::vector<uint8_t>> NsmTemp::genRequestMsg(eid_t eid,
                                                           uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_temperature_reading_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_temperature_reading_req(instanceId, sensorId,
                                                 requestPtr);
    if (rc != NSM_SW_SUCCESS)
    {
        lg2::debug("encode_get_temperature_reading_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmTemp::handleResponseMsg(const struct nsm_msg* responseMsg,
                                   size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reasonCode = ERR_NULL;
    double reading = 0;

    auto rc = decode_get_temperature_reading_resp(responseMsg, responseLen, &cc,
                                                  &reasonCode, &reading);

    LG2_ERROR_FLT(
        "decode_get_temperature_reading_resp failure | reasonCode: {REASONCODE}, cc: {CC}, rc: {RC}",
        "REASONCODE", reasonCode, "CC", cc, "RC", rc);

    reading = rc == NSM_SW_SUCCESS && cc == NSM_SUCCESS
                  ? reading
                  : std::numeric_limits<double>::quiet_NaN();
    sensorValue->updateReading(reading,
                               utils::getCurrentSteadyClockTimestamp());

    return cc ? cc : rc;
}

class TempSensorFactory : public NumericSensorBuilder
{
  public:
    std::shared_ptr<NsmNumericSensor> makeSensor(
        [[maybe_unused]] const std::string& interface,
        [[maybe_unused]] const std::string& objPath, sdbusplus::bus::bus& bus,
        const NumericSensorInfo& info,
        [[maybe_unused]] const dbus::PropertyMap& properties) override
    {
        return std::make_shared<NsmTemp>(
            bus, info.name, info.type, info.sensorId, info.associations,
            info.chassis_association, info.physicalContext,
            info.implementation.get(), info.maxAllowableValue, info.maxValue,
            info.minValue, info.readingBasis.get(), info.description.get());
    };

    std::shared_ptr<NsmNumericAggregator>
        makeAggregator(const NumericSensorInfo& info) override
    {
        return std::make_shared<NsmTempAggregator>(info.name, info.type,
                                                   info.priority);
    };
};

static NumericSensorFactory numericSensorFactory{
    std::make_unique<TempSensorFactory>()};

REGISTER_NSM_CREATION_FUNCTION(numericSensorFactory.getCreationFunction(),
                               "xyz.openbmc_project.Configuration.NSM_Temp")

} // namespace nsm
