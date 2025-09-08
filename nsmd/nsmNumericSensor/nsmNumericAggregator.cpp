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

#include "nsmNumericAggregator.hpp"

#include "nsmNumericSensor.hpp"

#include <phosphor-logging/lg2.hpp>

#include <limits>

namespace nsm
{
int NsmNumericAggregator::addSensor(
    uint8_t tag, std::shared_ptr<NsmNumericSensorValueAggregate> sensor)
{
    if (tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE)
    {
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag] = sensor;

    return NSM_SW_SUCCESS;
}

int NsmNumericAggregator::updateSensorReading(uint8_t tag, double reading,
                                              uint64_t timestamp)
{
    if (tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE || !sensors[tag])
    {
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag]->updateReading(reading, timestamp);

    return NSM_SW_SUCCESS;
}

void NsmNumericAggregator::logFalseValid(uint8_t tag, bool valid)
{
    if (valid)
    {
        if (tag_map.isAnyBitSet())
        {
            lg2::info(
                "handleResponseMsg: NsmNumericAggregator Aggregator {NAME}"
                " of type {TYPE} | Bits Invalid Code Cleared for"
                " Tag(s) : [{TAGCLEAREDBITS}]",
                "NAME", getName(), "TYPE", getType(), "TAGCLEAREDBITS",
                tag_map.getSetBits());
        }
        // Clear the bitMaps
        tag_map.clear();
    }
    else if (tag_map.setBit(tag))
    {
        lg2::debug(
            "NsmNumericAggregator: False Valid bit in Tag {TAG} for Aggregator {NAME} of type {TYPE}.",
            "TAG", tag, "NAME", getName(), "TYPE", getType());
    }
}

int NsmNumericAggregator::updateSensorNotWorking(uint8_t tag, bool valid)
{
    if (tag > NSM_AGGREGATE_MAX_UNRESERVED_SAMPLE_TAG_VALUE || !sensors[tag])
    {
        return NSM_SW_ERROR_DATA;
    }

    logFalseValid(tag, valid);

    sensors[tag]->updateReading(std::numeric_limits<double>::quiet_NaN());

    return NSM_SW_SUCCESS;
}

requester::Coroutine
    NsmNumericAggregator::update(std::shared_ptr<NsmDevice> nsmDevice)
{
    auto requestMsg = genRequestMsg(nsmDevice->getEid(), 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmNumericAggregator::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", nsmDevice->getEid());
        co_return NSM_SW_ERROR;
    }

#ifdef LTTNG_TRACING
    lttng_ust_tracepoint(nsmd, sensor_polling_request_generated,
                         nsmDevice->getEid(), requestMsg.value().data(),
                         this->getName().c_str());
#endif

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await nsmDevice->sensorIO(nsmDevice->getEid(), *requestMsg,
                                           responseMsg, responseLen, false);
    if (rc)
    {
        // we are relying on samples in NsmSensorAggregator to
        // be preserved across invocations of
        // NsmSensor::update for this to work.
        for (const auto& sample : samples)
        {
            updateSensorNotWorking(sample.tag, false);
        }

        co_return rc;
    }

    rc = handleResponseMsg(responseMsg.get(), responseLen);

#ifdef LTTNG_TRACING
    lttng_ust_tracepoint(nsmd, dbus_sensor_reading_updated, nsmDevice->getEid(),
                         responseMsg.get(), this->getName().c_str());
#endif

    co_return rc;
}
} // namespace nsm
