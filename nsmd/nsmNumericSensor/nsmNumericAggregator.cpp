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
    if (!sensors[tag])
    {
        return NSM_SW_ERROR_DATA;
    }

    sensors[tag]->updateReading(reading, timestamp);

    return NSM_SW_SUCCESS;
}

int NsmNumericAggregator::updateSensorNotWorking(uint8_t tag, bool valid)
{
    if (!sensors[tag])
    {
        return NSM_SW_ERROR_DATA;
    }

    if (!valid)
    {
        logFalseValid(tag);
    }
    else
    {
        clearTagBitMap("NsmNumericAggregator");
    }

    sensors[tag]->updateReading(std::numeric_limits<double>::quiet_NaN());

    return NSM_SW_SUCCESS;
}

requester::Coroutine NsmNumericAggregator::update(SensorManager& manager,
                                                  eid_t eid)
{
    if (isLongRunning)
    {
        // TODO: Temporarily disabling long running commands.
        // To be removed after backend starts support long running commands.

        // coverity[missing_return]
        co_return NSM_SW_SUCCESS;
    }

    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmNumericAggregator::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await manager.SendRecvNsmMsg(eid, *requestMsg, responseMsg,
                                              responseLen);
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
    co_return rc;
}
} // namespace nsm
