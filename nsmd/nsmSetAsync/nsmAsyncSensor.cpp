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

#include "nsmAsyncSensor.hpp"

#include "sensorManager.hpp"

namespace nsm
{
requester::Coroutine
    NsmAsyncSensor::set(const AsyncSetOperationValueType& value,
                        AsyncOperationStatusType* status,
                        std::shared_ptr<NsmDevice> device)
{
    if (status == nullptr)
    {
        throw sdbusplus::error::xyz::openbmc_project::common::InvalidArgument{};
    }

    this->value = &value;
    this->status = status;
    SensorManager& manager = SensorManager::getInstance();
    auto eid = manager.getEid(device);
    auto rc = co_await update(manager, eid);

    // coverity[missing_return]
    co_return rc;
}

requester::Coroutine NsmAsyncSensor::update(SensorManager& manager, eid_t eid)
{
    auto requestMsg = genRequestMsg(eid, 0);
    if (!requestMsg.has_value())
    {
        lg2::error(
            "NsmAsyncSensor::update: genRequestMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
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
            "NsmAsyncSensor::update: SendRecvNsmMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
        *status = AsyncOperationStatusType::WriteFailure;
        // coverity[missing_return]
        co_return rc;
    }

    rc = handleResponseMsg(responseMsg.get(), responseLen);

    if (rc != NSM_SW_SUCCESS)
    {
        lg2::error(
            "NsmAsyncSensor::update: handleResponseMsg failed, name={NAME}, eid={EID}",
            "NAME", getName(), "EID", eid);
        *status = AsyncOperationStatusType::WriteFailure;
    }

    // coverity[missing_return]
    co_return rc;
}
} // namespace nsm
