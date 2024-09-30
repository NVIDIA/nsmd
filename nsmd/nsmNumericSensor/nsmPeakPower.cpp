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

#include "nsmPeakPower.hpp"

#include "platform-environmental.h"

#include "nsmDevice.hpp"
#include "nsmNumericSensorFactory.hpp"
#include "nsmObjectFactory.hpp"
#include "nsmPeakPowerAggregator.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <telemetry_mrd_producer.hpp>

namespace nsm
{
using namespace std::string_literals;

NsmPeakPower::NsmPeakPower(sdbusplus::bus::bus& bus, const std::string& name,
                           const std::string& type, uint8_t sensorId,
                           uint8_t averagingInterval) :
    NsmNumericSensor(
        name, type, sensorId,
        std::make_shared<NsmNumericSensorValueAggregate>(
            std::make_unique<NsmNumericSensorDbusPeakValueTimestamp>(
                bus, ("/xyz/openbmc_project/sensors/power/"s + name).c_str()))),
    averagingInterval(averagingInterval)
{}

std::optional<std::vector<uint8_t>>
    NsmPeakPower::genRequestMsg(eid_t eid, uint8_t instanceId)
{
    std::vector<uint8_t> request(sizeof(nsm_msg_hdr) +
                                 sizeof(nsm_get_max_observed_power_req));
    auto requestPtr = reinterpret_cast<struct nsm_msg*>(request.data());
    auto rc = encode_get_max_observed_power_req(instanceId, sensorId,
                                                averagingInterval, requestPtr);
    if (rc)
    {
        lg2::debug("encode_get_max_observed_power_req failed. "
                   "eid={EID} rc={RC}",
                   "EID", eid, "RC", rc);
        return std::nullopt;
    }

    return request;
}

uint8_t NsmPeakPower::handleResponseMsg(const struct nsm_msg* responseMsg,
                                        size_t responseLen)
{
    uint8_t cc = NSM_SUCCESS;
    uint16_t reason_code = ERR_NULL;

    uint32_t reading = 0;

    auto rc = decode_get_max_observed_power_resp(responseMsg, responseLen, &cc,
                                                 &reason_code, &reading);

    if (cc == NSM_SUCCESS && rc == NSM_SW_SUCCESS)
    {
        // unit of power is milliwatt in NSM Command Response and selected unit
        // on DBus is Watts. Hence it is converted to Watts.
        sensorValue->updateReading(reading / 1000.0);
        clearErrorBitMap("decode_get_max_observed_power_resp");
    }
    else
    {
        sensorValue->updateReading(std::numeric_limits<double>::quiet_NaN());

        logHandleResponseMsg("decode_get_max_observed_power_resp", reason_code,
                             cc, rc);
        return NSM_SW_ERROR_COMMAND_FAIL;
    }

    return NSM_SW_SUCCESS;
}

std::shared_ptr<NsmNumericSensor> PeakPowerSensorBuilder::makeSensor(
    [[maybe_unused]] const std::string& interface,
    [[maybe_unused]] const std::string& objPath, sdbusplus::bus::bus& bus,
    const NumericSensorInfo& info)
{
    auto averagingInterval = utils::DBusHandler().getDbusProperty<uint64_t>(
        objPath.c_str(), "AveragingInterval", interface.c_str());

    auto sensor = std::make_shared<NsmPeakPower>(
        bus, info.name, info.type, info.sensorId, averagingInterval);

    return sensor;
}

std::shared_ptr<NsmNumericAggregator>
    PeakPowerSensorBuilder::makeAggregator(const NumericSensorInfo& info)
{
    return std::make_shared<NsmPeakPowerAggregator>(info.name, info.type,
                                                    info.priority, 0);
}

} // namespace nsm
