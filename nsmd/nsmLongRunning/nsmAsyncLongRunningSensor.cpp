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

#include "nsmAsyncLongRunningSensor.hpp"

#include "sensorManager.hpp"

namespace nsm
{

NsmAsyncLongRunningSensor::NsmAsyncLongRunningSensor(
    const std::string& name, const std::string& type, bool isLongRunning,
    std::shared_ptr<NsmDevice> device, uint8_t messageType,
    uint8_t commandCode) :
    NsmAsyncSensor(name, type),
    NsmLongRunningEvent(name, type + "_LongRunningEvent", isLongRunning),
    device(device), messageType(messageType), commandCode(commandCode)
{}

requester::Coroutine NsmAsyncLongRunningSensor::update(SensorManager& manager,
                                                       eid_t eid)
{
    uint8_t rc = NSM_SW_SUCCESS;
    if (isLongRunning)
    {
        // Acquire the semaphore before proceeding
        co_await device->getSemaphore().acquire(eid);
        // Register the active handler in the device with messageType and
        // commandCode
        device->registerLongRunningHandler(messageType, commandCode, this);
        rc = co_await NsmAsyncLongRunningSensor::updateLongRunningSensor(
            manager, eid);
        if (rc == NSM_SW_SUCCESS)
        {
            rc = co_await timer;
            if (rc != NSM_SW_SUCCESS)
            {
                lg2::error(
                    "NsmAsyncLongRunningSensor::update: LongRunning timer start failed, name={NAME}, eid={EID}",
                    "NAME", NsmSensor::getName(), "EID", eid);
            }
            else if (timer.expired())
            {
                lg2::error(
                    "NsmAsyncLongRunningSensor::update: LongRunning sensor timeout expired, name={NAME}, eid={EID}",
                    "NAME", NsmSensor::getName(), "EID", eid);
                rc = NSM_SW_ERROR;
            }
        }
        // Unregister the active handler in the device
        device->clearLongRunningHandler();
        // Release the semaphore after the update is complete
        device->getSemaphore().release();
    }
    else
    {
        rc = co_await NsmAsyncSensor::update(manager, eid);
    }

    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine
    NsmAsyncLongRunningSensor::updateLongRunningSensor(SensorManager& manager,
                                                       eid_t eid)
{
    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmAsyncLongRunningSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", NsmSensor::getName(), "EID", eid);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return NSM_SW_ERROR;
    }

    std::shared_ptr<const nsm_msg> responseMsg;
    size_t responseLen = 0;
    auto rc = co_await manager.SendRecvNsmMsg(eid, *requestMsg, responseMsg,
                                              responseLen);
    if (rc)
    {
        lg2::error(
            "NsmAsyncLongRunningSensor::update: SendRecvNsmMsg failed, name={NAME}, eid={EID}",
            "NAME", NsmAsyncSensor::getName(), "EID", eid);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }
    uint8_t cc;
    uint16_t reasonCode = 0, dataSize = 0;
    rc = decode_common_resp(responseMsg.get(), responseLen, &cc, &dataSize,
                            &reasonCode);
    if (!initAcceptInstanceId(responseMsg->hdr.instance_id, cc, rc))
    {
        logHandleResponseMsg(
            "NsmAsyncLongRunningSensor::update: Failed to accept LongRunning",
            reasonCode, cc, rc);
        *status = AsyncOperationStatusType::InternalFailure;
        rc = NSM_SW_ERROR_COMMAND_FAIL;
    }
    else
    {
        clearErrorBitMap(
            "NsmAsyncLongRunningSensor::update: Failed to accept LongRunning");
    }

    // coverity[missing_return]
    co_return rc;
}

int NsmAsyncLongRunningSensor::handle(eid_t eid, NsmType, NsmEventId,
                                      const nsm_msg* event, size_t eventLen)
{
    if (!validateEvent(eid, event, eventLen))
    {
        // All false cases are logged in validateEvent
        return NSM_SW_ERROR_COMMAND_FAIL;
    }
    return handleResponseMsg(event, eventLen);
}

} // namespace nsm
