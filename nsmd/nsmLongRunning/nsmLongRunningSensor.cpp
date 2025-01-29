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

#include "nsmLongRunningSensor.hpp"

#include "sensorManager.hpp"

namespace nsm
{

NsmLongRunningSensor::NsmLongRunningSensor(const std::string& name,
                                           const std::string& type,
                                           bool isLongRunning,
                                           std::shared_ptr<NsmDevice> device,
                                           uint8_t messageType,
                                           uint8_t commandCode) :
    NsmSensor(name, type),
    NsmLongRunningEvent(name, type + "_LongRunningEvent", isLongRunning),
    device(device), messageType(messageType), commandCode(commandCode)
{}

requester::Coroutine NsmLongRunningSensor::update(SensorManager& manager,
                                                  eid_t eid)
{
    uint8_t rc = NSM_SW_SUCCESS;

    // Acquire the semaphore before proceeding
    co_await device->getSemaphore().acquire(eid);
    // by default command will be treated as long running
    isLongRunning = true;
    // Register the active handler in the device with messageType and
    // commandCode
    device->registerLongRunningHandler(messageType, commandCode,
                                       shared_from_this());
    rc = co_await NsmLongRunningSensor::updateLongRunningSensor(manager, eid);
    if (rc == NSM_SW_SUCCESS && isLongRunning)
    {
        // if(isLongRunning) means event will be there as second response
        rc = co_await timer;
        if (rc != NSM_SW_SUCCESS)
        {
            lg2::error(
                "NsmLongRunningSensor::update: LongRunning timer start failed, name={NAME}, eid={EID}",
                "NAME", NsmSensor::getName(), "EID", eid);
        }
        else if (timer.expired())
        {
            lg2::error(
                "NsmLongRunningSensor::update: LongRunning sensor timeout, name={NAME}, eid={EID}",
                "NAME", NsmSensor::getName(), "EID", eid);
            rc = NSM_SW_ERROR;
        }
    }

    // Unregister the active handler in the device
    device->clearLongRunningHandler();
    // Release the semaphore after the update is complete
    device->getSemaphore().release();

    // coverity[missing_return]
    co_return rc;
}

// function for handling first response of long running request
requester::Coroutine
    NsmLongRunningSensor::updateLongRunningSensor(SensorManager& manager,
                                                  eid_t eid)
{
    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmLongRunningSensor::updateLongRunningSensor: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", NsmSensor::getName(), "EID", eid);
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await manager.SendRecvNsmMsg(eid, *requestMsg, responseMsg,
                                              responseLen);

    if (rc)
    {
        lg2::debug(
            "NsmLongRunningSensor::updateLongRunningSensor: SendRecvNsmMsg failed, name={NAME}, eid={EID}",
            "NAME", NsmSensor::getName(), "EID", eid);
        // coverity[missing_return]
        co_return rc;
    }
    uint8_t cc;
    uint16_t reasonCode = 0, dataSize = 0;
    rc = decode_common_resp(responseMsg.get(), responseLen, &cc, &dataSize,
                            &reasonCode);

    if (cc == NSM_SUCCESS)
    {
        // treat it as normal request and return here itself
        isLongRunning = false;
        rc = handleResponseMsg(responseMsg.get(), responseLen);
        // coverity[missing_return]
        co_return rc;
    }

    // if cc != NSM_SUCCESS proceed for event handling
    if (!initAcceptInstanceId(responseMsg->hdr.instance_id, cc, rc))
    {
        logHandleResponseMsg(
            "NsmLongRunningSensor::updateLongRunningSensor: Failed to accept LongRunning",
            reasonCode, cc, rc);
        rc = NSM_SW_ERROR_COMMAND_FAIL;
    }
    else
    {
        clearErrorBitMap(
            "NsmLongRunningSensor::updateLongRunningSensor: Failed to accept LongRunning");
    }

    // coverity[missing_return]
    co_return rc;
}

int NsmLongRunningSensor::handle(eid_t eid, NsmType, NsmEventId,
                                 const nsm_msg* event, size_t eventLen)
{
    auto rc = validateEvent(eid, event, eventLen);
    if (rc == NSM_SW_SUCCESS)
    {
        rc = handleResponseMsg(event, eventLen);
    }
    if (!timer.stop())
    {
        lg2::error(
            "NsmLongRunningSensor::handle: LongRunning timer not stopped, eid={EID}",
            "EID", eid);
    }
    return rc;
}

} // namespace nsm
